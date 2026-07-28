/******************************************************************************
 * @file eml_integration.c
 * @brief Intégration transactionnelle de propositions issues d'un EML.
 ******************************************************************************/
#include "core/eml_integration.h"
#include "core/controlled_vocab.h"
#include "dao/entity_dao.h"
#include "dao/evidence_entity_dao.h"
#include "database/transaction.h"
#include "database/statement.h"
#include "models/entity_record.h"

EmlEntityProposal *eml_entity_proposal_new(const char *type, const char *value)
{
    return eml_entity_proposal_new_with_metadata(type, value, "proposed",
        "header");
}
EmlEntityProposal *eml_entity_proposal_new_with_metadata(const char *type,
    const char *value, const char *verification_status,
    const char *provenance_kind)
{
    EmlEntityProposal *proposal = NULL;
    if (type == NULL || type[0] == '\0' || value == NULL || value[0] == '\0') return NULL;
    proposal = g_new0(EmlEntityProposal, 1);
    proposal->type_identifier = g_strdup(type); proposal->value = g_strdup(value);
    proposal->verification_status = g_strdup(verification_status != NULL
        ? verification_status : "proposed");
    proposal->provenance_kind = g_strdup(provenance_kind != NULL
        ? provenance_kind : "header");
    proposal->value_raw = g_strdup(value);
    proposal->role = g_strdup("other");
    proposal->source_header = g_strdup("manual");
    proposal->occurrence = 1;
    if (proposal->type_identifier == NULL || proposal->value == NULL ||
        proposal->verification_status == NULL || proposal->provenance_kind == NULL)
    { eml_entity_proposal_free(proposal); return NULL; }
    return proposal;
}
EmlEntityProposal *eml_entity_proposal_new_observation(const char *type,
    const char *raw, const char *normalized, const char *role,
    const char *source_header, guint occurrence,
    const char *verification_status, const char *provenance_kind)
{
    EmlEntityProposal *proposal = eml_entity_proposal_new_with_metadata(type,
        normalized, verification_status, provenance_kind);
    if (proposal == NULL) return NULL;
    g_free(proposal->value_raw); proposal->value_raw = g_strdup(raw);
    g_free(proposal->role); proposal->role = g_strdup(role);
    g_free(proposal->source_header);
    proposal->source_header = g_strdup(source_header);
    proposal->occurrence = occurrence;
    if (proposal->value_raw == NULL || proposal->role == NULL ||
        proposal->source_header == NULL || occurrence == 0)
    { eml_entity_proposal_free(proposal); return NULL; }
    return proposal;
}
void eml_entity_proposal_free(EmlEntityProposal *proposal)
{
    if (proposal == NULL) return;
    g_free(proposal->type_identifier); g_free(proposal->value);
    g_free(proposal->verification_status); g_free(proposal->provenance_kind);
    g_free(proposal->value_raw); g_free(proposal->role);
    g_free(proposal->source_header);
    g_free(proposal);
}
/** @brief Recherche une entité existante avec le même type et la même valeur. */
static const EntityRecord *eml_integration_find_existing(const GPtrArray *entities,
    const EmlEntityProposal *proposal)
{
    for (guint i = 0; entities != NULL && i < entities->len; i++)
    {
        const EntityRecord *record = g_ptr_array_index((GPtrArray *) entities, i);
        if (g_strcmp0(entity_record_get_type_identifier(record),
                proposal->type_identifier) == 0 &&
            g_ascii_strcasecmp(entity_record_get_value(record), proposal->value) == 0)
            return record;
    }
    return NULL;
}
gboolean eml_integration_apply(Database *database, const char *evidence_identifier,
    const GPtrArray *proposals, guint *out_observations,
    guint *out_created, guint *out_reused, GError **error)
{
    EntityDao *entity_dao = NULL;
    EvidenceEntityDao *link_dao = NULL;
    GPtrArray *entities = NULL;
    GDateTime *now = NULL;
    char *timestamp = NULL;
    guint created = 0;
    guint reused = 0;
    guint observations = 0;
    gboolean active = FALSE;
    gboolean success = FALSE;

    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (out_created != NULL)
        *out_created = 0;
    if (out_reused != NULL)
        *out_reused = 0;
    if (out_observations != NULL)
        *out_observations = 0;

    if (database == NULL || evidence_identifier == NULL || proposals == NULL || proposals->len == 0)
    {
        g_set_error_literal(error,
            g_quark_from_static_string("eml-integration-error"), 1,
            "Sélectionnez au moins une proposition EML.");
        return FALSE;
    }
    for (guint i = 0; i < proposals->len; i++)
    {
        const EmlEntityProposal *proposal = g_ptr_array_index(
            (GPtrArray *) proposals, i);
        if (proposal == NULL ||
            proposal->role == NULL || proposal->role[0] == '\0' ||
            proposal->source_header == NULL ||
            proposal->source_header[0] == '\0' ||
            proposal->occurrence == 0 ||
            !controlled_vocab_is_valid_verification_status(
                proposal->verification_status) ||
            !controlled_vocab_is_valid_provenance_kind(
                proposal->provenance_kind))
        {
            g_set_error_literal(error,
                g_quark_from_static_string("eml-integration-error"), 2,
                "Le statut ou la provenance d’une proposition est invalide.");
            return FALSE;
        }
        if (g_strcmp0(proposal->verification_status, "rejected") == 0 ||
            g_strcmp0(proposal->verification_status, "invalid") == 0)
        {
            g_set_error_literal(error,
                g_quark_from_static_string("eml-integration-error"), 3,
                "Une proposition rejetée ou invalide ne peut pas être intégrée.");
            return FALSE;
        }
    }
    if (!database_transaction_begin(database))
        return FALSE;
    active = TRUE;
    entity_dao = entity_dao_new(database, error);
    link_dao = evidence_entity_dao_new(database, error);
    if (entity_dao == NULL || link_dao == NULL) goto cleanup;
    entities = entity_dao_list_all(entity_dao, error); if (entities == NULL) goto cleanup;
    now = g_date_time_new_now_utc();
    timestamp = now != NULL ? g_date_time_format(now, "%Y-%m-%dT%H:%M:%SZ") : NULL;
    if (timestamp == NULL) goto cleanup;
    for (guint i = 0; i < proposals->len; i++)
    {
        EmlEntityProposal *proposal = g_ptr_array_index((GPtrArray *) proposals, i);
        char *observation_identifier = NULL;
        if (!evidence_entity_dao_add_observation(link_dao,
                evidence_identifier, proposal->type_identifier,
                proposal->value_raw, proposal->value, proposal->role,
                proposal->provenance_kind, proposal->source_header,
                proposal->occurrence, proposal->verification_status,
                timestamp, &observation_identifier, error))
            goto cleanup;
        observations++;
        if (!proposal->promote_to_entity)
        { g_free(observation_identifier); continue; }
        const EntityRecord *existing = eml_integration_find_existing(entities, proposal);
        const char *identifier = existing != NULL
            ? entity_record_get_identifier(existing) : NULL;
        char *new_identifier = NULL; EntityRecord *new_record = NULL;
        if (existing == NULL)
        {
            new_identifier = g_uuid_string_random(); identifier = new_identifier;
            new_record = entity_record_new(identifier, proposal->type_identifier,
                proposal->value, proposal->value,
                "Indicateur extrait des en-têtes de la preuve EML.", 50,
                timestamp, timestamp, ENTITY_STATUS_ACTIVE, error);
            if (new_record == NULL || !entity_dao_insert(entity_dao, new_record, error))
            { entity_record_free(new_record); g_free(observation_identifier);
              g_free(new_identifier); goto cleanup; }
            g_ptr_array_add(entities, new_record); new_record = NULL; created++;
        }
        else reused++;
        if (!evidence_entity_dao_add_source(link_dao, evidence_identifier,
                identifier, "eml_observation", observation_identifier,
                timestamp, error))
        { g_free(observation_identifier); g_free(new_identifier); goto cleanup; }
        if (!evidence_entity_dao_promote_observation(link_dao,
                observation_identifier, identifier, timestamp,
                existing != NULL ? "reused" : "created", error))
        { g_free(observation_identifier); g_free(new_identifier); goto cleanup; }
        g_free(observation_identifier);
        g_free(new_identifier);
    }
    if (!database_transaction_commit(database)) goto cleanup;
    active = FALSE; success = TRUE;
    if (out_created != NULL) *out_created = created;
    if (out_reused != NULL) *out_reused = reused;
    if (out_observations != NULL) *out_observations = observations;
cleanup:
    if (!success && active) database_transaction_rollback(database);
    g_free(timestamp); g_clear_pointer(&now, g_date_time_unref);
    g_clear_pointer(&entities, g_ptr_array_unref);
    evidence_entity_dao_free(link_dao); entity_dao_free(entity_dao);
    return success;
}

static gboolean eml_integration_read_count(Database *database,
    const char *sql, const char *identifier, gint64 *out_count)
{
    DatabaseStatement *statement = database_statement_prepare(database, sql);
    gboolean success = statement != NULL &&
        database_statement_bind_text(statement, 1, identifier) &&
        database_statement_step(statement) == DATABASE_STATEMENT_STEP_ROW &&
        database_statement_column_int64(statement, 0, out_count);
    database_statement_finalize(statement);
    return success;
}

gboolean eml_integration_remove_promotion(Database *database,
    const char *observation_identifier, gboolean *out_entity_deleted,
    gboolean *out_entity_shared, GError **error)
{
    static const char *read_sql =
        "SELECT evidence_id,entity_id FROM evidence_entity_observations "
        "WHERE id=?;";
    static const char *detach_sql =
        "UPDATE evidence_entity_observations SET entity_id=NULL,"
        "promoted_at=NULL,promotion_kind=NULL WHERE id=? AND entity_id=?;";
    static const char *dependency_sql =
        "SELECT "
        "(SELECT COUNT(*) FROM evidence_entity_observations WHERE entity_id=?1)+"
        "(SELECT COUNT(*) FROM preuve_entites WHERE entite_id=?1)+"
        "(SELECT COUNT(*) FROM preuve_entite_sources WHERE entite_id=?1)+"
        "(SELECT COUNT(*) FROM relations WHERE entite_source_id=?1 OR entite_cible_id=?1)+"
        "(SELECT COUNT(*) FROM tag_entites WHERE entite_id=?1)+"
        "(SELECT COUNT(*) FROM recherche_entites WHERE entite_id=?1)+"
        "(SELECT COUNT(*) FROM entite_chronologie WHERE entite_id=?1)+"
        "(SELECT COUNT(*) FROM hypothese_entites WHERE entite_id=?1)+"
        "(SELECT COUNT(*) FROM osint_execution_entities WHERE entity_id=?1)+"
        "(SELECT COUNT(*) FROM comptes_sociaux WHERE entite_id=?1)+"
        "(SELECT COUNT(*) FROM person_roles WHERE entity_id=?1);";
    static const char *delete_sql = "DELETE FROM entites WHERE id=?;";
    DatabaseStatement *statement = NULL;
    EvidenceEntityDao *link_dao = NULL;
    char *evidence_identifier = NULL, *entity_identifier = NULL;
    gint64 count = 0; gboolean active = FALSE, success = FALSE;
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    if (out_entity_deleted != NULL) *out_entity_deleted = FALSE;
    if (out_entity_shared != NULL) *out_entity_shared = FALSE;
    if (database == NULL || !g_uuid_string_is_valid(observation_identifier))
        return FALSE;
    if (!database_transaction_begin(database)) return FALSE;
    active = TRUE;
    link_dao = evidence_entity_dao_new(database, error);
    if (link_dao == NULL) goto cleanup;
    statement = database_statement_prepare(database, read_sql);
    if (statement == NULL ||
        !database_statement_bind_text(statement, 1, observation_identifier) ||
        database_statement_step(statement) != DATABASE_STATEMENT_STEP_ROW ||
        !database_statement_column_text(statement, 0, &evidence_identifier) ||
        !database_statement_column_text(statement, 1, &entity_identifier))
        goto cleanup;
    database_statement_finalize(statement); statement = NULL;
    if (entity_identifier == NULL)
    {
        g_set_error_literal(error,
            g_quark_from_static_string("eml-integration-error"), 4,
            "Cette observation n’est pas ajoutée au graphe.");
        goto cleanup;
    }
    statement = database_statement_prepare(database, detach_sql);
    if (statement == NULL ||
        !database_statement_bind_text(statement, 1, observation_identifier) ||
        !database_statement_bind_text(statement, 2, entity_identifier) ||
        database_statement_step(statement) != DATABASE_STATEMENT_STEP_DONE)
        goto cleanup;
    database_statement_finalize(statement); statement = NULL;
    if (!evidence_entity_dao_remove_source(link_dao, evidence_identifier,
            entity_identifier, "eml_observation", observation_identifier,
            NULL, error)) goto cleanup;
    if (!eml_integration_read_count(database, dependency_sql,
            entity_identifier, &count)) goto cleanup;
    if (count == 0)
    {
        statement = database_statement_prepare(database, delete_sql);
        if (statement == NULL ||
            !database_statement_bind_text(statement, 1, entity_identifier) ||
            database_statement_step(statement) != DATABASE_STATEMENT_STEP_DONE)
            goto cleanup;
        database_statement_finalize(statement); statement = NULL;
        if (out_entity_deleted != NULL) *out_entity_deleted = TRUE;
    }
    else if (out_entity_shared != NULL) *out_entity_shared = TRUE;
    if (!database_transaction_commit(database)) goto cleanup;
    active = FALSE; success = TRUE;
cleanup:
    database_statement_finalize(statement);
    if (!success && active) database_transaction_rollback(database);
    evidence_entity_dao_free(link_dao);
    g_free(evidence_identifier); g_free(entity_identifier);
    return success;
}
