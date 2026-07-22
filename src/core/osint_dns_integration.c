/******************************************************************************
 * @file osint_dns_integration.c
 * @brief Intégration transactionnelle de propositions DNS validées.
 ******************************************************************************/

#include "core/osint_dns_integration.h"

#include "dao/entity_dao.h"
#include "dao/relation_dao.h"
#include "dao/osint_execution_dao.h"
#include "database/error.h"
#include "database/transaction.h"
#include "models/entity_record.h"
#include "models/osint_dns_proposal.h"
#include "models/relation_record.h"

#include <gio/gio.h>
#include <string.h>

/** @brief Normalise une valeur déjà persistée pour la comparaison. */
static char *osint_dns_integration_normalize_existing_value(
    const char *entity_type,
    const char *value
)
{
    if (g_strcmp0(entity_type, "ip_address") == 0)
    {
        GInetAddress *address = g_inet_address_new_from_string(value);
        char *normalized_value = NULL;
        if (address == NULL) return g_strdup(value);
        normalized_value = g_inet_address_to_string(address);
        g_object_unref(address);
        return normalized_value;
    }
    if (g_strcmp0(entity_type, "domain_name") == 0)
    {
        char *normalized_value = g_ascii_strdown(value, -1);
        gsize value_length = normalized_value != NULL
            ? strlen(normalized_value) : 0U;
        if (value_length > 0U && normalized_value[value_length - 1U] == '.')
            normalized_value[value_length - 1U] = '\0';
        return normalized_value;
    }
    return g_strdup(value);
}

/** @brief Construit la clé anti-doublon d'une entité. */
static char *osint_dns_integration_build_key(
    const char *entity_type,
    const char *value
)
{
    return g_strdup_printf("%s\n%s", entity_type, value);
}

/** @brief Construit la clé anti-doublon d'une relation orientée. */
static char *osint_dns_integration_build_relation_key(
    const char *source_identifier,
    const char *target_identifier,
    const char *relation_type
)
{
    return g_strdup_printf(
        "%s\n%s\n%s", source_identifier, target_identifier, relation_type
    );
}

/** @brief Renseigne une erreur littérale si possible. */
static void osint_dns_integration_set_error(
    GError **error,
    OsintDnsIntegrationError code,
    const char *message
)
{
    if (error != NULL && *error == NULL)
        g_set_error_literal(error, OSINT_DNS_INTEGRATION_ERROR, code, message);
}

GQuark osint_dns_integration_error_quark(void)
{
    return g_quark_from_static_string("osint-dns-integration-error-quark");
}

gboolean osint_dns_integration_apply(
    Database *database,
    const char *source_entity_identifier,
    const char *execution_identifier,
    GPtrArray *selected_proposals,
    guint *out_inserted_entity_count,
    guint *out_skipped_entity_count,
    guint *out_inserted_relation_count,
    guint *out_skipped_relation_count,
    GError **error
)
{
    EntityDao *entity_dao = NULL;
    RelationDao *relation_dao = NULL;
    OsintExecutionDao *execution_dao = NULL;
    GPtrArray *existing_entities = NULL;
    GPtrArray *existing_relations = NULL;
    GHashTable *known_entities = NULL;
    GHashTable *known_relations = NULL;
    EntityRecord *source_entity = NULL;
    GDateTime *now = NULL;
    char *timestamp = NULL;
    guint inserted_entity_count = 0U;
    guint skipped_entity_count = 0U;
    guint inserted_relation_count = 0U;
    guint skipped_relation_count = 0U;
    gboolean success = FALSE;

    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    if (out_inserted_entity_count != NULL) *out_inserted_entity_count = 0U;
    if (out_skipped_entity_count != NULL) *out_skipped_entity_count = 0U;
    if (out_inserted_relation_count != NULL) *out_inserted_relation_count = 0U;
    if (out_skipped_relation_count != NULL) *out_skipped_relation_count = 0U;
    if (database == NULL || source_entity_identifier == NULL ||
        !g_uuid_string_is_valid(source_entity_identifier) ||
        execution_identifier == NULL ||
        !g_uuid_string_is_valid(execution_identifier) ||
        selected_proposals == NULL ||
        selected_proposals->len == 0U)
    {
        osint_dns_integration_set_error(
            error, OSINT_DNS_INTEGRATION_ERROR_INVALID_ARGUMENT,
            "La sélection DNS à intégrer est invalide."
        );
        return FALSE;
    }

    entity_dao = entity_dao_new(database, error);
    if (entity_dao == NULL) return FALSE;
    relation_dao = relation_dao_new(database, error);
    if (relation_dao == NULL) goto cleanup;
    execution_dao = osint_execution_dao_new(database, error);
    if (execution_dao == NULL) goto cleanup;
    source_entity = entity_dao_find_by_identifier(
        entity_dao, source_entity_identifier, error
    );
    if (source_entity == NULL)
    {
        if (error == NULL || *error == NULL)
            osint_dns_integration_set_error(
                error, OSINT_DNS_INTEGRATION_ERROR_INVALID_ARGUMENT,
                "L'entité source de la résolution DNS est introuvable."
            );
        goto cleanup;
    }
    existing_entities = entity_dao_list_all(entity_dao, error);
    if (existing_entities == NULL) goto cleanup;
    existing_relations = relation_dao_list_all(relation_dao, error);
    if (existing_relations == NULL) goto cleanup;
    known_entities = g_hash_table_new_full(
        g_str_hash, g_str_equal, g_free, g_free
    );
    for (guint index = 0; index < existing_entities->len; index++)
    {
        const EntityRecord *entity = g_ptr_array_index(existing_entities, index);
        const char *entity_type = entity_record_get_type_identifier(entity);
        char *normalized_value = osint_dns_integration_normalize_existing_value(
            entity_type, entity_record_get_value(entity)
        );
        g_hash_table_insert(
            known_entities,
            osint_dns_integration_build_key(
                entity_type,
                normalized_value
            ),
            g_strdup(entity_record_get_identifier(entity))
        );
        g_free(normalized_value);
    }
    known_relations = g_hash_table_new_full(
        g_str_hash, g_str_equal, g_free, g_free
    );
    for (guint index = 0; index < existing_relations->len; index++)
    {
        const RelationRecord *relation = g_ptr_array_index(
            existing_relations, index
        );
        g_hash_table_insert(
            known_relations,
            osint_dns_integration_build_relation_key(
                relation_record_get_source_entity_identifier(relation),
                relation_record_get_target_entity_identifier(relation),
                relation_record_get_relation_type(relation)
            ),
            g_strdup(relation_record_get_identifier(relation))
        );
    }

    now = g_date_time_new_now_utc();
    timestamp = now != NULL
        ? g_date_time_format(now, "%Y-%m-%dT%H:%M:%SZ") : NULL;
    if (timestamp == NULL)
    {
        osint_dns_integration_set_error(
            error, OSINT_DNS_INTEGRATION_ERROR_MODEL,
            "Impossible de dater les entités DNS."
        );
        goto cleanup;
    }
    if (!database_transaction_begin(database))
    {
        osint_dns_integration_set_error(
            error, OSINT_DNS_INTEGRATION_ERROR_DATABASE,
            database_error_get_message(database) != NULL
                ? database_error_get_message(database)
                : "Impossible de démarrer la transaction DNS."
        );
        goto cleanup;
    }

    for (guint index = 0; index < selected_proposals->len; index++)
    {
        const OsintDnsProposal *proposal = g_ptr_array_index(
            selected_proposals, index
        );
        const char *entity_type = osint_dns_proposal_get_entity_type(proposal);
        const char *relation_type =
            osint_dns_proposal_get_relation_type(proposal);
        char *normalized_value = osint_dns_proposal_dup_normalized_value(proposal);
        char *entity_key = NULL;
        char *target_identifier = NULL;
        char *description = NULL;
        EntityRecord *entity = NULL;
        RelationRecord *relation = NULL;
        char *relation_identifier = NULL;
        char *relation_key = NULL;
        char *relation_label = NULL;
        char *relation_justification = NULL;
        GError *model_error = NULL;
        gboolean entity_was_created = FALSE;
        gboolean relation_was_created = FALSE;

        if (entity_type == NULL || relation_type == NULL ||
            normalized_value == NULL)
        {
            skipped_entity_count++;
            skipped_relation_count++;
            g_free(normalized_value);
            continue;
        }
        entity_key = osint_dns_integration_build_key(entity_type, normalized_value);
        target_identifier = g_strdup(
            g_hash_table_lookup(known_entities, entity_key)
        );
        if (target_identifier != NULL)
        {
            skipped_entity_count++;
        }
        else
        {
            target_identifier = g_uuid_string_random();
            description = g_strdup_printf(
                "Proposition DNS %s obtenue avec dns.dig depuis %s. "
                "Résultat OSINT à vérifier.",
                osint_dns_proposal_get_record_type(proposal),
                osint_dns_proposal_get_target(proposal)
            );
            entity = entity_record_new(
                target_identifier, entity_type, normalized_value,
                normalized_value, description, 50, timestamp, timestamp,
                ENTITY_STATUS_ACTIVE, &model_error
            );
            if (entity == NULL || !entity_dao_insert(entity_dao, entity, error))
            {
                if (error != NULL && *error == NULL && model_error != NULL)
                    g_propagate_error(error, g_steal_pointer(&model_error));
                entity_record_free(entity);
                g_clear_error(&model_error);
                g_free(description);
                g_free(target_identifier);
                g_free(entity_key);
                g_free(normalized_value);
                database_transaction_rollback(database);
                goto cleanup;
            }
            g_hash_table_insert(
                known_entities, entity_key, g_strdup(target_identifier)
            );
            entity_key = NULL;
            inserted_entity_count++;
            entity_was_created = TRUE;
            entity_record_free(entity);
            g_clear_error(&model_error);
            g_free(description);
        }

        relation_key = osint_dns_integration_build_relation_key(
            source_entity_identifier, target_identifier, relation_type
        );
        relation_identifier = g_strdup(
            g_hash_table_lookup(known_relations, relation_key)
        );
        if (g_strcmp0(source_entity_identifier, target_identifier) == 0 ||
            relation_identifier != NULL)
        {
            skipped_relation_count++;
        }
        else
        {
            relation_identifier = g_uuid_string_random();
            relation_label = g_strdup_printf(
                "%s — %s → %s",
                osint_dns_proposal_get_target(proposal), relation_type,
                normalized_value
            );
            relation_justification = g_strdup_printf(
                "Relation DNS %s obtenue avec dns.dig depuis %s. "
                "Résultat OSINT à vérifier.",
                osint_dns_proposal_get_record_type(proposal),
                osint_dns_proposal_get_target(proposal)
            );
            relation = relation_record_new(
                relation_identifier, source_entity_identifier,
                target_identifier, relation_type, relation_label,
                relation_justification, 50, timestamp, timestamp,
                RELATION_STATUS_ACTIVE, &model_error
            );
            if (relation == NULL ||
                !relation_dao_insert(relation_dao, relation, error))
            {
                if (error != NULL && *error == NULL && model_error != NULL)
                    g_propagate_error(error, g_steal_pointer(&model_error));
                relation_record_free(relation);
                g_clear_error(&model_error);
                g_free(relation_identifier);
                g_free(relation_label);
                g_free(relation_justification);
                g_free(relation_key);
                g_free(target_identifier);
                g_free(entity_key);
                g_free(normalized_value);
                database_transaction_rollback(database);
                goto cleanup;
            }
            g_hash_table_insert(
                known_relations, relation_key, g_strdup(relation_identifier)
            );
            relation_key = NULL;
            inserted_relation_count++;
            relation_was_created = TRUE;
            relation_record_free(relation);
            g_clear_error(&model_error);
            g_free(relation_label);
            g_free(relation_justification);
        }

        if (!osint_execution_dao_link_entity(
                execution_dao, execution_identifier, target_identifier,
                entity_was_created ? "created" : "reused", error
            ) ||
            (relation_identifier != NULL &&
             !osint_execution_dao_link_relation(
                 execution_dao, execution_identifier, relation_identifier,
                 relation_was_created ? "created" : "reused", error
             )))
        {
            g_free(relation_identifier);
            g_free(relation_key); g_free(target_identifier);
            g_free(entity_key); g_free(normalized_value);
            database_transaction_rollback(database);
            goto cleanup;
        }

        g_free(relation_identifier);
        g_free(relation_key);
        g_free(target_identifier);
        g_free(entity_key);
        g_free(normalized_value);
    }

    if (!database_transaction_commit(database))
    {
        osint_dns_integration_set_error(
            error, OSINT_DNS_INTEGRATION_ERROR_DATABASE,
            database_error_get_message(database) != NULL
                ? database_error_get_message(database)
                : "Impossible de valider la transaction DNS."
        );
        database_transaction_rollback(database);
        goto cleanup;
    }
    success = TRUE;
    if (out_inserted_entity_count != NULL)
        *out_inserted_entity_count = inserted_entity_count;
    if (out_skipped_entity_count != NULL)
        *out_skipped_entity_count = skipped_entity_count;
    if (out_inserted_relation_count != NULL)
        *out_inserted_relation_count = inserted_relation_count;
    if (out_skipped_relation_count != NULL)
        *out_skipped_relation_count = skipped_relation_count;

cleanup:
    g_free(timestamp);
    g_clear_pointer(&now, g_date_time_unref);
    g_clear_pointer(&known_entities, g_hash_table_unref);
    g_clear_pointer(&known_relations, g_hash_table_unref);
    g_clear_pointer(&existing_entities, g_ptr_array_unref);
    g_clear_pointer(&existing_relations, g_ptr_array_unref);
    entity_record_free(source_entity);
    relation_dao_free(relation_dao);
    osint_execution_dao_free(execution_dao);
    entity_dao_free(entity_dao);
    return success;
}
