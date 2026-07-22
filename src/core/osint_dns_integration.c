/******************************************************************************
 * @file osint_dns_integration.c
 * @brief Intégration transactionnelle de propositions DNS validées.
 ******************************************************************************/

#include "core/osint_dns_integration.h"

#include "dao/entity_dao.h"
#include "database/error.h"
#include "database/transaction.h"
#include "models/entity_record.h"
#include "models/osint_dns_proposal.h"

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
    GPtrArray *selected_proposals,
    guint *out_inserted_count,
    guint *out_skipped_count,
    GError **error
)
{
    EntityDao *entity_dao = NULL;
    GPtrArray *existing_entities = NULL;
    GHashTable *known_entities = NULL;
    GDateTime *now = NULL;
    char *timestamp = NULL;
    guint inserted_count = 0U;
    guint skipped_count = 0U;
    gboolean success = FALSE;

    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    if (out_inserted_count != NULL) *out_inserted_count = 0U;
    if (out_skipped_count != NULL) *out_skipped_count = 0U;
    if (database == NULL || selected_proposals == NULL ||
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
    existing_entities = entity_dao_list_all(entity_dao, error);
    if (existing_entities == NULL) goto cleanup;
    known_entities = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    for (guint index = 0; index < existing_entities->len; index++)
    {
        const EntityRecord *entity = g_ptr_array_index(existing_entities, index);
        const char *entity_type = entity_record_get_type_identifier(entity);
        char *normalized_value = osint_dns_integration_normalize_existing_value(
            entity_type, entity_record_get_value(entity)
        );
        g_hash_table_add(
            known_entities,
            osint_dns_integration_build_key(
                entity_type,
                normalized_value
            )
        );
        g_free(normalized_value);
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
        char *normalized_value = osint_dns_proposal_dup_normalized_value(proposal);
        char *entity_key = NULL;
        char *identifier = NULL;
        char *description = NULL;
        EntityRecord *entity = NULL;
        GError *model_error = NULL;

        if (entity_type == NULL || normalized_value == NULL)
        {
            skipped_count++;
            g_free(normalized_value);
            continue;
        }
        entity_key = osint_dns_integration_build_key(entity_type, normalized_value);
        if (g_hash_table_contains(known_entities, entity_key))
        {
            skipped_count++;
            g_free(entity_key);
            g_free(normalized_value);
            continue;
        }

        identifier = g_uuid_string_random();
        description = g_strdup_printf(
            "Proposition DNS %s obtenue avec dns.dig depuis %s. "
            "Résultat OSINT à vérifier.",
            osint_dns_proposal_get_record_type(proposal),
            osint_dns_proposal_get_target(proposal)
        );
        entity = entity_record_new(
            identifier, entity_type, normalized_value, normalized_value,
            description, 50, timestamp, timestamp, ENTITY_STATUS_ACTIVE,
            &model_error
        );
        if (entity == NULL || !entity_dao_insert(entity_dao, entity, error))
        {
            if (error != NULL && *error == NULL && model_error != NULL)
                g_propagate_error(error, g_steal_pointer(&model_error));
            entity_record_free(entity);
            g_clear_error(&model_error);
            g_free(description);
            g_free(identifier);
            g_free(entity_key);
            g_free(normalized_value);
            database_transaction_rollback(database);
            goto cleanup;
        }

        g_hash_table_add(known_entities, entity_key);
        inserted_count++;
        entity_record_free(entity);
        g_clear_error(&model_error);
        g_free(description);
        g_free(identifier);
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
    if (out_inserted_count != NULL) *out_inserted_count = inserted_count;
    if (out_skipped_count != NULL) *out_skipped_count = skipped_count;

cleanup:
    g_free(timestamp);
    g_clear_pointer(&now, g_date_time_unref);
    g_clear_pointer(&known_entities, g_hash_table_unref);
    g_clear_pointer(&existing_entities, g_ptr_array_unref);
    entity_dao_free(entity_dao);
    return success;
}
