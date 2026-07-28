#include "core/person_creation_coordinator.h"
#include "core/file_hash.h"
#include "dao/entity_dao.h"
#include "dao/evidence_dao.h"
#include "dao/evidence_entity_dao.h"
#include "dao/person_role_assignment_dao.h"
#include "database/transaction.h"
#include <errno.h>
#include <glib/gstdio.h>
#include <string.h>

static GQuark coordinator_error(void)
{
    return g_quark_from_static_string("person-creation-coordinator-error");
}
static gboolean cancelled(GCancellable *cancellable, GError **error)
{
    return cancellable != NULL &&
        g_cancellable_set_error_if_cancelled(cancellable, error);
}
static char *timestamp_now(void)
{
    GDateTime *now = g_date_time_new_now_utc();
    char *timestamp = now != NULL
        ? g_date_time_format(now, "%Y-%m-%dT%H:%M:%SZ") : NULL;
    g_clear_pointer(&now, g_date_time_unref);
    return timestamp;
}
static char *safe_extension(const char *name)
{
    const char *dot = name != NULL ? strrchr(name, '.') : NULL;
    if (dot == NULL || strlen(dot) > 12) return g_strdup("");
    for (const char *cursor = dot + 1; *cursor != '\0'; cursor++)
        if (!g_ascii_isalnum(*cursor)) return g_strdup("");
    return g_ascii_strdown(dot, -1);
}
static gboolean copy_staged(const char *root,
    const PersonEvidenceSelectionItem *item, char **out_relative,
    char **out_internal, char **out_absolute, GCancellable *cancellable,
    GError **error)
{
    const char *type =
        person_evidence_selection_item_get_type_identifier(item);
    char *extension = safe_extension(
        person_evidence_selection_item_get_original_name(item));
    char *uuid = g_uuid_string_random();
    char *directory = g_build_filename(
        root, "01_Preuves_Originales", type, NULL);
    GFile *source = NULL, *destination = NULL;
    char *computed = NULL;
    guint64 computed_size = 0;
    gboolean success = FALSE;
    *out_internal = g_strconcat(uuid, extension, NULL);
    *out_relative = g_build_filename("01_Preuves_Originales", type,
        *out_internal, NULL);
    *out_absolute = g_build_filename(root, *out_relative, NULL);
    if (g_mkdir_with_parents(directory, 0700) != 0) {
        g_set_error(error, coordinator_error(), 1,
            "Impossible de préparer le dossier définitif : %s.",
            g_strerror(errno));
        goto cleanup;
    }
    source = g_file_new_for_path(
        person_evidence_selection_item_get_staging_path(item));
    destination = g_file_new_for_path(*out_absolute);
    if (!g_file_copy(source, destination, G_FILE_COPY_NONE, cancellable,
            NULL, NULL, error)) goto cleanup;
    if (!file_hash_compute_sha256(*out_absolute, cancellable,
            &computed, &computed_size, error) ||
        g_strcmp0(computed,
            person_evidence_selection_item_get_sha256(item)) != 0) {
        if (error != NULL && *error == NULL)
            g_set_error_literal(error, coordinator_error(), 2,
                "La copie définitive ne correspond pas au staging.");
        goto cleanup;
    }
    success = TRUE;
cleanup:
    if (!success && *out_absolute != NULL) g_unlink(*out_absolute);
    g_clear_object(&source);
    g_clear_object(&destination);
    g_free(computed);
    g_free(extension);
    g_free(uuid);
    g_free(directory);
    return success;
}
static EntityRecord *build_person(const PersonEntityInput *input,
    const char *identifier, const char *timestamp, GError **error)
{
    const char *value = input->declared_name != NULL &&
        input->declared_name[0] != '\0' ? input->declared_name :
        input->pseudonym != NULL && input->pseudonym[0] != '\0'
            ? input->pseudonym : input->designation;
    char *description = g_strdup_printf(
        "Statut d'identification : %s.%s%s%s%s",
        input->identification_status,
        input->pseudonym != NULL ? "\nPseudonyme déclaré : " : "",
        input->pseudonym != NULL ? input->pseudonym : "",
        input->notes != NULL ? "\nNotes factuelles : " : "",
        input->notes != NULL ? input->notes : "");
    EntityRecord *record = entity_record_new(identifier, "person", value,
        input->designation, description, input->confidence,
        timestamp, timestamp, ENTITY_STATUS_ACTIVE, error);
    g_free(description);
    return record;
}
static gboolean validate_selection_files(const char *root,
    const PersonEvidenceSelection *selection, GCancellable *cancellable,
    GError **error)
{
    for (guint i = 0; i < person_evidence_selection_get_count(selection);
         i++) {
        const PersonEvidenceSelectionItem *item =
            person_evidence_selection_get(selection, i);
        const char *expected =
            person_evidence_selection_item_get_sha256(item);
        char *path = NULL, *computed = NULL;
        guint64 size = 0;
        if (person_evidence_selection_item_get_origin(item) ==
            PERSON_EVIDENCE_ORIGIN_EXISTING) {
            const EvidenceRecord *record =
                person_evidence_selection_item_get_record(item);
            path = g_build_filename(root,
                evidence_record_get_relative_path(record), NULL);
        } else
            path = g_strdup(
                person_evidence_selection_item_get_staging_path(item));
        if (!file_hash_compute_sha256(path, cancellable,
                &computed, &size, error) ||
            g_strcmp0(computed, expected) != 0 ||
            size != person_evidence_selection_item_get_size_bytes(item)) {
            if (error != NULL && *error == NULL)
                g_set_error_literal(error, coordinator_error(), 3,
                    "Une preuve a changé depuis sa sélection.");
            g_free(path); g_free(computed);
            return FALSE;
        }
        g_free(path); g_free(computed);
    }
    return TRUE;
}
PersonCreationCoordinatorResult *person_creation_coordinator_execute(
    Database *database, const char *root, const PersonEntityInput *person,
    const PersonEvidenceSelection *selection, GCancellable *cancellable,
    GError **error)
{
    PersonCreationCoordinatorResult *result = NULL;
    EntityDao *entity_dao = NULL;
    EvidenceDao *evidence_dao = NULL;
    EvidenceEntityDao *link_dao = NULL;
    PersonRoleAssignmentDao *role_dao = NULL;
    EntityRecord *person_record = NULL;
    GPtrArray *created_paths = g_ptr_array_new_with_free_func(g_free);
    char *timestamp = NULL;
    gboolean transaction_active = FALSE, success = FALSE;
    if (database == NULL || root == NULL || person == NULL ||
        person->designation == NULL || person->designation[0] == '\0' ||
        !person_evidence_selection_is_confirmable(selection)) {
        g_set_error_literal(error, coordinator_error(), 1,
            "La création de personne ou la sélection est invalide.");
        goto cleanup;
    }
    if (cancelled(cancellable, error)) goto cleanup;
    if (!validate_selection_files(root, selection, cancellable, error))
        goto cleanup;
    timestamp = timestamp_now();
    result = g_new0(PersonCreationCoordinatorResult, 1);
    result->person_identifier = g_uuid_string_random();
    result->evidence_identifiers =
        g_ptr_array_new_with_free_func(g_free);
    entity_dao = entity_dao_new(database, error);
    evidence_dao = evidence_dao_new(database, error);
    link_dao = evidence_entity_dao_new(database, error);
    role_dao = person_role_assignment_dao_new(database, error);
    person_record = build_person(person, result->person_identifier,
        timestamp, error);
    if (timestamp == NULL || entity_dao == NULL || evidence_dao == NULL ||
        link_dao == NULL || role_dao == NULL || person_record == NULL ||
        !database_transaction_begin(database)) goto cleanup;
    transaction_active = TRUE;
    if (!entity_dao_insert(entity_dao, person_record, error) ||
        !person_role_assignment_dao_insert_all(role_dao,
            result->person_identifier, person->role_assignments, error))
        goto cleanup;
    for (guint i = 0; i < person_evidence_selection_get_count(selection);
         i++) {
        const PersonEvidenceSelectionItem *item =
            person_evidence_selection_get(selection, i);
        const char *evidence_identifier =
            person_evidence_selection_item_get_evidence_identifier(item);
        char *identifier = NULL, *relative = NULL, *internal = NULL;
        char *absolute = NULL, *duplicate_identifier = NULL;
        EvidenceRecord *record = NULL;
        if (cancelled(cancellable, error)) goto cleanup;
        if (person_evidence_selection_item_get_origin(item) ==
            PERSON_EVIDENCE_ORIGIN_STAGED) {
            duplicate_identifier = evidence_dao_find_identifier_by_sha256(
                evidence_dao,
                person_evidence_selection_item_get_sha256(item), error);
            if (error != NULL && *error != NULL) {
                g_free(duplicate_identifier);
                goto cleanup;
            }
            if (duplicate_identifier != NULL)
                evidence_identifier = duplicate_identifier;
            else {
            identifier = g_uuid_string_random();
            if (!copy_staged(root, item, &relative, &internal, &absolute,
                    cancellable, error)) {
                g_free(identifier); g_free(relative); g_free(internal);
                g_free(absolute); goto cleanup;
            }
            g_ptr_array_add(created_paths, g_strdup(absolute));
            record = evidence_record_new(identifier,
                person_evidence_selection_item_get_original_name(item),
                internal, relative,
                person_evidence_selection_item_get_type_identifier(item),
                person_evidence_selection_item_get_size_bytes(item),
                person_evidence_selection_item_get_sha256(item), timestamp,
                NULL, "Import depuis l’assistant de création d’une personne",
                person_evidence_selection_item_get_description(item),
                EVIDENCE_INTEGRITY_STATUS_VALID, error);
            if (record == NULL ||
                !evidence_dao_insert(evidence_dao, record, error)) {
                evidence_record_free(record); g_free(identifier);
                g_free(relative); g_free(internal); g_free(absolute);
                goto cleanup;
            }
            evidence_identifier = identifier;
            }
        }
        if (!evidence_entity_dao_link(link_dao, evidence_identifier,
                result->person_identifier, error)) {
            evidence_record_free(record); g_free(identifier);
            g_free(duplicate_identifier);
            g_free(relative); g_free(internal); g_free(absolute);
            goto cleanup;
        }
        g_ptr_array_add(result->evidence_identifiers,
            g_strdup(evidence_identifier));
        evidence_record_free(record);
        g_free(duplicate_identifier);
        g_free(identifier); g_free(relative); g_free(internal);
        g_free(absolute);
    }
    if (!database_transaction_commit(database)) goto cleanup;
    transaction_active = FALSE;
    success = TRUE;
cleanup:
    if (!success && transaction_active)
        database_transaction_rollback(database);
    if (!success)
        for (guint i = 0; i < created_paths->len; i++)
            g_unlink(g_ptr_array_index(created_paths, i));
    g_ptr_array_unref(created_paths);
    entity_record_free(person_record);
    entity_dao_free(entity_dao);
    evidence_dao_free(evidence_dao);
    evidence_entity_dao_free(link_dao);
    person_role_assignment_dao_free(role_dao);
    g_free(timestamp);
    if (!success) {
        person_creation_coordinator_result_free(result);
        result = NULL;
    }
    return result;
}
void person_creation_coordinator_result_free(
    PersonCreationCoordinatorResult *result)
{
    if (result == NULL) return;
    g_free(result->person_identifier);
    g_clear_pointer(&result->evidence_identifiers, g_ptr_array_unref);
    g_free(result);
}
