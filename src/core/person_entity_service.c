/******************************************************************************
 * @file person_entity_service.c
 * @brief Création transactionnelle d'une personne observée.
 ******************************************************************************/
#include "core/person_entity_service.h"
#include "dao/entity_dao.h"
#include "dao/evidence_entity_dao.h"
#include "database/transaction.h"
#include "database/statement.h"
#include "models/entity_record.h"

/** @brief Indique si le niveau d'identification est reconnu. */
static gboolean person_entity_service_status_valid(const char *status)
{
    return g_strcmp0(status, "unknown") == 0 ||
        g_strcmp0(status, "suspected") == 0 ||
        g_strcmp0(status, "confirmed") == 0;
}

gboolean person_entity_service_update_confidence(Database *database,
    const char *entity_identifier, gint confidence, GError **error)
{
    static const char validate_sql[] =
        "SELECT 1 FROM entites e JOIN types_entite t ON t.id=e.type_id "
        "WHERE e.id=? AND t.code='person';";
    static const char sql[] =
        "UPDATE entites SET confiance=?, updated_at=? WHERE id=? AND "
        "type_id=(SELECT id FROM types_entite WHERE code='person');";
    DatabaseStatement *statement = NULL;
    GDateTime *now = NULL;
    char *timestamp = NULL;
    gboolean active = FALSE, success = FALSE;
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    if (database == NULL || entity_identifier == NULL ||
        !g_uuid_string_is_valid(entity_identifier) || confidence < 0 ||
        confidence > 100)
    {
        g_set_error_literal(error, g_quark_from_static_string(
            "person-entity-service-error"), 4,
            "La personne ou le niveau de confiance est invalide.");
        return FALSE;
    }
    statement = database_statement_prepare(database, validate_sql);
    if (statement == NULL || !database_statement_bind_text(statement, 1,
            entity_identifier) || database_statement_step(statement) !=
            DATABASE_STATEMENT_STEP_ROW)
    {
        database_statement_finalize(statement);
        g_set_error_literal(error, g_quark_from_static_string(
            "person-entity-service-error"), 4,
            "La personne sélectionnée n'existe pas.");
        return FALSE;
    }
    database_statement_finalize(statement); statement = NULL;
    now = g_date_time_new_now_utc();
    timestamp = now != NULL ? g_date_time_format(now,
        "%Y-%m-%dT%H:%M:%SZ") : NULL;
    if (timestamp == NULL || !database_transaction_begin(database))
        goto cleanup;
    active = TRUE;
    statement = database_statement_prepare(database, sql);
    if (statement == NULL ||
        !database_statement_bind_int64(statement, 1, confidence) ||
        !database_statement_bind_text(statement, 2, timestamp) ||
        !database_statement_bind_text(statement, 3, entity_identifier) ||
        database_statement_step(statement) != DATABASE_STATEMENT_STEP_DONE)
        goto cleanup;
    database_statement_finalize(statement); statement = NULL;
    if (!database_transaction_commit(database)) goto cleanup;
    active = FALSE; success = TRUE;
cleanup:
    database_statement_finalize(statement);
    if (!success && active) database_transaction_rollback(database);
    if (!success && error != NULL && *error == NULL)
        g_set_error_literal(error, g_quark_from_static_string(
            "person-entity-service-error"), 5,
            "Le niveau de confiance n'a pas pu être enregistré.");
    g_free(timestamp); g_clear_pointer(&now, g_date_time_unref);
    return success;
}

gboolean person_entity_service_update_role(Database *database,
    const char *entity_identifier, PersonRole role, GError **error)
{
    static const char validate_sql[] =
        "SELECT 1 FROM entites e JOIN types_entite t ON t.id=e.type_id "
        "WHERE e.id=? AND t.code='person';";
    static const char upsert_sql[] =
        "INSERT INTO person_roles(entity_id, role, updated_at) VALUES(?,?,?) "
        "ON CONFLICT(entity_id) DO UPDATE SET role=excluded.role, "
        "updated_at=excluded.updated_at;";
    DatabaseStatement *statement = NULL;
    GDateTime *now = NULL;
    char *timestamp = NULL;
    const char *role_code = person_role_to_code(role);
    gboolean active = FALSE, success = FALSE;
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    if (database == NULL || entity_identifier == NULL ||
        !g_uuid_string_is_valid(entity_identifier) || role_code == NULL)
        goto invalid;
    statement = database_statement_prepare(database, validate_sql);
    if (statement == NULL || !database_statement_bind_text(statement, 1,
            entity_identifier) || database_statement_step(statement) !=
            DATABASE_STATEMENT_STEP_ROW)
        goto invalid;
    database_statement_finalize(statement); statement = NULL;
    now = g_date_time_new_now_utc();
    timestamp = now != NULL ? g_date_time_format(now,
        "%Y-%m-%dT%H:%M:%SZ") : NULL;
    if (timestamp == NULL || !database_transaction_begin(database))
        goto cleanup;
    active = TRUE;
    statement = database_statement_prepare(database, upsert_sql);
    if (statement == NULL ||
        !database_statement_bind_text(statement, 1, entity_identifier) ||
        !database_statement_bind_text(statement, 2, role_code) ||
        !database_statement_bind_text(statement, 3, timestamp) ||
        database_statement_step(statement) != DATABASE_STATEMENT_STEP_DONE)
        goto cleanup;
    database_statement_finalize(statement); statement = NULL;
    if (!database_transaction_commit(database)) goto cleanup;
    active = FALSE; success = TRUE; goto cleanup;
invalid:
    g_set_error_literal(error, g_quark_from_static_string(
        "person-entity-service-error"), 3,
        "La personne ou la catégorie sélectionnée est invalide.");
cleanup:
    database_statement_finalize(statement);
    if (!success && active) database_transaction_rollback(database);
    g_free(timestamp); g_clear_pointer(&now, g_date_time_unref);
    return success;
}

gboolean person_entity_service_create(Database *database,
    const PersonEntityInput *input, char **out_identifier, GError **error)
{
    EntityDao *entity_dao = NULL;
    EvidenceEntityDao *link_dao = NULL;
    EntityRecord *record = NULL;
    GDateTime *now = NULL;
    char *timestamp = NULL;
    char *identifier = NULL;
    char *description = NULL;
    const char *value = NULL;
    gboolean active = FALSE;
    gboolean success = FALSE;
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    if (out_identifier != NULL) *out_identifier = NULL;
    if (database == NULL || input == NULL || input->designation == NULL ||
        input->designation[0] == '\0' ||
        !person_entity_service_status_valid(input->identification_status) ||
        input->confidence < 0 || input->confidence > 100)
    {
        g_set_error_literal(error, g_quark_from_static_string(
            "person-entity-service-error"), 1,
            "Les informations de la personne sont invalides.");
        return FALSE;
    }
    value = input->declared_name != NULL && input->declared_name[0] != '\0'
        ? input->declared_name
        : input->pseudonym != NULL && input->pseudonym[0] != '\0'
            ? input->pseudonym : input->designation;
    description = g_strdup_printf(
        "Statut d'identification : %s.%s%s%s%s",
        input->identification_status,
        input->pseudonym != NULL ? "\nPseudonyme déclaré : " : "",
        input->pseudonym != NULL ? input->pseudonym : "",
        input->notes != NULL ? "\nNotes factuelles : " : "",
        input->notes != NULL ? input->notes : "");
    identifier = g_uuid_string_random();
    now = g_date_time_new_now_utc();
    timestamp = now != NULL ? g_date_time_format(now,
        "%Y-%m-%dT%H:%M:%SZ") : NULL;
    if (identifier == NULL || timestamp == NULL || description == NULL ||
        !database_transaction_begin(database)) goto cleanup;
    active = TRUE;
    entity_dao = entity_dao_new(database, error);
    record = entity_record_new(identifier, "person", value,
        input->designation, description, input->confidence,
        timestamp, timestamp, ENTITY_STATUS_ACTIVE, error);
    if (entity_dao == NULL || record == NULL ||
        !entity_dao_insert(entity_dao, record, error)) goto cleanup;
    if (input->evidence_identifier != NULL && input->evidence_identifier[0] != '\0')
    {
        link_dao = evidence_entity_dao_new(database, error);
        if (link_dao == NULL || !evidence_entity_dao_link(link_dao,
                input->evidence_identifier, identifier, error)) goto cleanup;
    }
    if (!database_transaction_commit(database)) goto cleanup;
    active = FALSE;
    success = TRUE;
    if (out_identifier != NULL) *out_identifier = g_strdup(identifier);
cleanup:
    if (!success && active) database_transaction_rollback(database);
    evidence_entity_dao_free(link_dao);
    entity_record_free(record);
    entity_dao_free(entity_dao);
    g_clear_pointer(&now, g_date_time_unref);
    g_free(description); g_free(timestamp); g_free(identifier);
    return success;
}
