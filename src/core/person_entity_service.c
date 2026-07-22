/******************************************************************************
 * @file person_entity_service.c
 * @brief Création transactionnelle d'une personne observée.
 ******************************************************************************/
#include "core/person_entity_service.h"
#include "dao/entity_dao.h"
#include "dao/evidence_entity_dao.h"
#include "database/transaction.h"
#include "models/entity_record.h"

/** @brief Indique si le niveau d'identification est reconnu. */
static gboolean person_entity_service_status_valid(const char *status)
{
    return g_strcmp0(status, "unknown") == 0 ||
        g_strcmp0(status, "suspected") == 0 ||
        g_strcmp0(status, "confirmed") == 0;
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
