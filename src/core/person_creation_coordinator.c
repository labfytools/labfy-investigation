#include "core/person_creation_coordinator.h"
#include "core/file_hash.h"
#include "dao/entity_dao.h"
#include "dao/evidence_dao.h"
#include "dao/evidence_entity_dao.h"
#include "dao/person_role_assignment_dao.h"
#include "dao/identity_ocr_dao.h"
#include "dao/identity_traceability_dao.h"
#include "database/transaction.h"
#include <errno.h>
#include <glib/gstdio.h>
#include <string.h>


static GQuark coordinator_error(void)
{
    return g_quark_from_static_string("person-creation-coordinator-error");
}
typedef struct {
    const PersonCreationCoordinatorOptions *options;
    guint occurrences[PERSON_CREATION_FAILURE_COMPENSATION + 1];
} FailureControl;
static gboolean fail_at(FailureControl *control,
    PersonCreationFailurePoint point, GError **error)
{
    guint occurrence;
    if (control == NULL || control->options == NULL ||
        control->options->failure_point != point) return FALSE;
    occurrence = control->occurrences[point]++;
    if (occurrence != control->options->failure_occurrence) return FALSE;
    if (error != NULL && *error == NULL)
        g_set_error(error, coordinator_error(), 100 + point,
            "Échec injecté au point %u, occurrence %u.",
            (guint) point, occurrence);
    return TRUE;
}
static gboolean session_valid(const PersonCreationCoordinatorOptions *options)
{
    return options == NULL || options->session_check == NULL ||
        options->session_check(options->session_check_data);
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

static gboolean persist_ocr_runs(IdentityOcrDao *dao, const char *root,
    const char *person_identifier, const PersonEvidenceSelection *selection,
    const GPtrArray *evidence_identifiers, const GPtrArray *runs,
    const char *timestamp, GPtrArray *created_paths,
    GPtrArray *created_directories,
    FailureControl *failure,
    GCancellable *cancellable, GError **error)
{
    for (guint i = 0; runs != NULL && i < runs->len; i++) {
        IdentityOcrRun *run = g_ptr_array_index((GPtrArray *) runs, i);
        const char *evidence_identifier = NULL;
        for (guint j = 0; j < person_evidence_selection_get_count(selection);
             j++) {
            const PersonEvidenceSelectionItem *item =
                person_evidence_selection_get(selection, j);
            if (g_strcmp0(person_evidence_selection_item_get_identifier(item),
                    identity_ocr_run_get_evidence_id(run)) == 0) {
                evidence_identifier = g_ptr_array_index(
                    (GPtrArray *) evidence_identifiers, j);
                break;
            }
        }
        if (evidence_identifier == NULL) {
            g_set_error_literal(error, coordinator_error(), 4,
                "La preuve source de l’OCR n’est plus retenue.");
            return FALSE;
        }
        char *parent = g_build_filename(root, "02_Preuves_Traitees",
            "OCR", NULL);
        char *directory = g_build_filename(parent,
            identity_ocr_run_get_identifier(run), NULL);
        gboolean parent_existed = g_file_test(parent, G_FILE_TEST_IS_DIR);
        gboolean directory_existed =
            g_file_test(directory, G_FILE_TEST_IS_DIR);
        char *text_absolute = g_build_filename(directory, "ocr.txt", NULL);
        char *tsv_absolute = g_build_filename(directory, "ocr.tsv", NULL);
        char *text_relative = g_build_filename("02_Preuves_Traitees", "OCR",
            identity_ocr_run_get_identifier(run), "ocr.txt", NULL);
        char *tsv_relative = g_build_filename("02_Preuves_Traitees", "OCR",
            identity_ocr_run_get_identifier(run), "ocr.tsv", NULL);
        char *text_sha = NULL, *tsv_sha = NULL;
        guint64 artifact_size = 0;
        gboolean ok =
            !fail_at(failure,
                PERSON_CREATION_FAILURE_CREATE_OCR_DIRECTORY, error) &&
            g_mkdir_with_parents(directory, 0700) == 0 &&
            !fail_at(failure, PERSON_CREATION_FAILURE_COPY_OCR_TEXT, error) &&
            g_file_set_contents(text_absolute,
                identity_ocr_run_get_raw_text(run), -1, error) &&
            !fail_at(failure, PERSON_CREATION_FAILURE_COPY_OCR_TSV, error) &&
            g_file_set_contents(tsv_absolute,
                identity_ocr_run_get_tsv(run), -1, error) &&
            !fail_at(failure, PERSON_CREATION_FAILURE_HASH_OCR_TEXT, error) &&
            file_hash_compute_sha256(text_absolute, cancellable,
                &text_sha, &artifact_size, error) &&
            !fail_at(failure, PERSON_CREATION_FAILURE_HASH_OCR_TSV, error) &&
            file_hash_compute_sha256(tsv_absolute, cancellable,
                &tsv_sha, &artifact_size, error);
        const GPtrArray *fields = identity_ocr_run_get_fields(run);
        ok = ok && !fail_at(failure,
            PERSON_CREATION_FAILURE_INSERT_OCR_RUN, error) &&
            !fail_at(failure,
                PERSON_CREATION_FAILURE_INSERT_DOCUMENT_OBSERVATION, error);
        for (guint field_index = 0; ok && fields != NULL &&
             field_index < fields->len; field_index++) {
            IdentityFieldObservation *field =
                g_ptr_array_index((GPtrArray *) fields, field_index);
            IdentityReviewStatus status =
                identity_field_observation_get_status(field);
            if (status != IDENTITY_REVIEW_ACCEPTED &&
                status != IDENTITY_REVIEW_MODIFIED) continue;
            ok = !fail_at(failure,
                PERSON_CREATION_FAILURE_INSERT_FIELD, error) &&
                !fail_at(failure,
                    PERSON_CREATION_FAILURE_CREATE_SOURCE, error);
        }
        ok = ok &&
            identity_ocr_dao_insert(dao, person_identifier,
                evidence_identifier, run, text_relative, text_sha,
                tsv_relative, tsv_sha, timestamp, error);
        if (g_file_test(text_absolute, G_FILE_TEST_IS_REGULAR))
            g_ptr_array_add(created_paths, g_strdup(text_absolute));
        if (g_file_test(tsv_absolute, G_FILE_TEST_IS_REGULAR))
            g_ptr_array_add(created_paths, g_strdup(tsv_absolute));
        if (!parent_existed && g_file_test(parent, G_FILE_TEST_IS_DIR))
            g_ptr_array_add(created_directories, g_strdup(parent));
        if (!directory_existed && g_file_test(directory, G_FILE_TEST_IS_DIR))
            g_ptr_array_add(created_directories, g_strdup(directory));
        g_free(parent); g_free(directory); g_free(text_absolute); g_free(tsv_absolute);
        g_free(text_relative); g_free(tsv_relative);
        g_free(text_sha); g_free(tsv_sha);
        if (!ok) return FALSE;
    }
    return TRUE;
}

static gboolean persist_factual_relations(IdentityTraceabilityDao *dao,
    const char *person_identifier,
    const PersonEvidenceSelection *selection,
    const GPtrArray *evidence_identifiers,
    const GPtrArray *inputs, const char *timestamp,
    FailureControl *failure, GError **error)
{
    for (guint i = 0; inputs != NULL && i < inputs->len; i++) {
        const PersonCreationFactualRelationInput *input =
            g_ptr_array_index((GPtrArray *) inputs, i);
        const char *evidence_identifier = NULL;
        for (guint j = 0; input != NULL &&
             j < person_evidence_selection_get_count(selection); j++) {
            const PersonEvidenceSelectionItem *item =
                person_evidence_selection_get(selection, j);
            if (g_strcmp0(
                    person_evidence_selection_item_get_identifier(item),
                    input->evidence_selection_identifier) == 0) {
                evidence_identifier = g_ptr_array_index(
                    (GPtrArray *) evidence_identifiers, j);
                break;
            }
        }
        char *identifier = g_uuid_string_random();
        PersonEvidenceFactualRelation *relation =
            input != NULL && evidence_identifier != NULL
            ? person_evidence_factual_relation_new(identifier,
                person_identifier, evidence_identifier,
                input->ocr_run_identifier, input->relation_type,
                input->factual_note, timestamp, TRUE) : NULL;
        g_free(identifier);
        if (relation == NULL ||
            fail_at(failure,
                PERSON_CREATION_FAILURE_INSERT_FACTUAL_RELATION, error) ||
            !identity_traceability_dao_insert_factual_relation(
                dao, relation, error)) {
            person_evidence_factual_relation_free(relation);
            if (error != NULL && *error == NULL)
                g_set_error_literal(error, coordinator_error(), 10,
                    "La relation factuelle explicite est invalide.");
            return FALSE;
        }
        person_evidence_factual_relation_free(relation);
    }
    return TRUE;
}

static PersonCreationCoordinatorResult *person_creation_coordinator_execute_internal(
    Database *database, const char *root, const PersonEntityInput *person,
    const char *existing_person_identifier,
    const PersonEvidenceSelection *selection, const GPtrArray *ocr_runs,
    const PersonCreationCoordinatorEvidenceMetadata *metadata,
    const PersonCreationCoordinatorOptions *options,
    GCancellable *cancellable,
    GError **error)
{
    PersonCreationCoordinatorResult *result = NULL;
    EntityDao *entity_dao = NULL;
    EvidenceDao *evidence_dao = NULL;
    EvidenceEntityDao *link_dao = NULL;
    PersonRoleAssignmentDao *role_dao = NULL;
    IdentityOcrDao *ocr_dao = NULL;
    IdentityTraceabilityDao *traceability_dao = NULL;
    EntityRecord *person_record = NULL;
    GPtrArray *created_paths = g_ptr_array_new_with_free_func(g_free);
    GPtrArray *created_directories = g_ptr_array_new_with_free_func(g_free);
    char *timestamp = NULL;
    gboolean transaction_active = FALSE, success = FALSE;
    gboolean create_person = existing_person_identifier == NULL;
    FailureControl failure = {.options = options};
    if (fail_at(&failure, PERSON_CREATION_FAILURE_VALIDATE, error))
        goto cleanup;
    if (database == NULL || root == NULL ||
        (create_person && (person == NULL ||
         person->designation == NULL || person->designation[0] == '\0')) ||
        (!create_person && (existing_person_identifier[0] == '\0' ||
         !g_uuid_string_is_valid(existing_person_identifier))) ||
        !person_evidence_selection_is_confirmable(selection)) {
        g_set_error_literal(error, coordinator_error(), 1,
            "La création de personne ou la sélection est invalide.");
        goto cleanup;
    }
    if (!session_valid(options) ||
        fail_at(&failure, PERSON_CREATION_FAILURE_SESSION_BEFORE_START,
            error)) {
        if (error != NULL && *error == NULL)
            g_set_error_literal(error, coordinator_error(), 6,
                "La session d’enquête a changé avant la création.");
        goto cleanup;
    }
    if (cancelled(cancellable, error)) goto cleanup;
    if (fail_at(&failure, PERSON_CREATION_FAILURE_SOURCE_HASH, error))
        goto cleanup;
    if (!validate_selection_files(root, selection, cancellable, error))
        goto cleanup;
    timestamp = timestamp_now();
    result = g_new0(PersonCreationCoordinatorResult, 1);
    result->person_identifier = create_person
        ? g_uuid_string_random() : g_strdup(existing_person_identifier);
    result->evidence_identifiers =
        g_ptr_array_new_with_free_func(g_free);
    entity_dao = entity_dao_new(database, error);
    evidence_dao = evidence_dao_new(database, error);
    link_dao = evidence_entity_dao_new(database, error);
    role_dao = person_role_assignment_dao_new(database, error);
    ocr_dao = identity_ocr_dao_new(database);
    traceability_dao = identity_traceability_dao_new(database);
    person_record = entity_dao == NULL ? NULL : create_person
        ? build_person(person, result->person_identifier, timestamp, error)
        : entity_dao_find_by_identifier(
            entity_dao, result->person_identifier, error);
    if (timestamp == NULL || entity_dao == NULL || evidence_dao == NULL ||
        link_dao == NULL || role_dao == NULL || ocr_dao == NULL ||
        traceability_dao == NULL ||
        person_record == NULL ||
        g_strcmp0(entity_record_get_type_identifier(person_record),
            "person") != 0 ||
        !database_transaction_begin(database)) goto cleanup;
    transaction_active = TRUE;
    if (create_person &&
        (fail_at(&failure, PERSON_CREATION_FAILURE_CREATE_PERSON, error) ||
         !entity_dao_insert(entity_dao, person_record, error)))
        goto cleanup;
    for (guint i = 0; create_person && person->role_assignments != NULL &&
         i < person->role_assignments->len; i++)
        if (fail_at(&failure, PERSON_CREATION_FAILURE_CREATE_ROLE, error) ||
            !person_role_assignment_dao_insert(role_dao,
                result->person_identifier,
                g_ptr_array_index(person->role_assignments, i), error))
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
        if (fail_at(&failure, PERSON_CREATION_FAILURE_IMPORT_EVIDENCE,
                error)) goto cleanup;
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
                metadata != NULL && metadata->type_identifier != NULL
                    ? metadata->type_identifier
                    : person_evidence_selection_item_get_type_identifier(item),
                person_evidence_selection_item_get_size_bytes(item),
                person_evidence_selection_item_get_sha256(item),
                metadata != NULL && metadata->collected_at != NULL
                    ? metadata->collected_at : timestamp,
                metadata != NULL ? metadata->source : NULL, create_person
                    ? "Import depuis l’assistant de création d’une personne"
                    : "Import OCR rattaché à une personne existante",
                metadata != NULL && metadata->description != NULL
                    ? metadata->description
                    : person_evidence_selection_item_get_description(item),
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
        if (fail_at(&failure, PERSON_CREATION_FAILURE_LINK_EVIDENCE,
                error) ||
            !evidence_entity_dao_link(link_dao, evidence_identifier,
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
    if (!persist_ocr_runs(ocr_dao, root, result->person_identifier,
            selection, result->evidence_identifiers, ocr_runs, timestamp,
            created_paths, created_directories, &failure,
            cancellable, error))
        goto cleanup;
    if (!persist_factual_relations(traceability_dao,
            result->person_identifier, selection,
            result->evidence_identifiers,
            options != NULL ? options->factual_relations : NULL,
            timestamp, &failure, error))
        goto cleanup;
    for (guint i = 0; ocr_runs != NULL && i < ocr_runs->len; i++) {
        IdentityOcrRun *run = g_ptr_array_index((GPtrArray *) ocr_runs, i);
        char *directory = g_build_filename(root, "02_Preuves_Traitees",
            "OCR", identity_ocr_run_get_identifier(run), NULL);
        char *text_path = g_build_filename(directory, "ocr.txt", NULL);
        char *tsv_path = g_build_filename(directory, "ocr.tsv", NULL);
        gboolean alter_text = fail_at(&failure,
            PERSON_CREATION_FAILURE_ARTIFACT_TEXT_CHANGED, error);
        gboolean alter_tsv = fail_at(&failure,
            PERSON_CREATION_FAILURE_ARTIFACT_TSV_CHANGED, error);
        if (alter_text || alter_tsv) {
            g_clear_error(error);
            if (!g_file_set_contents(alter_text ? text_path : tsv_path,
                    "SPECIMEN MODIFIÉ APRÈS COPIE", -1, error)) {
                g_free(directory); g_free(text_path); g_free(tsv_path);
                goto cleanup;
            }
        }
        char *text_sha = NULL, *tsv_sha = NULL;
        guint64 size = 0;
        char *expected_text = g_compute_checksum_for_string(
            G_CHECKSUM_SHA256, identity_ocr_run_get_raw_text(run), -1);
        char *expected_tsv = g_compute_checksum_for_string(
            G_CHECKSUM_SHA256, identity_ocr_run_get_tsv(run), -1);
        gboolean verified = file_hash_compute_sha256(text_path, cancellable,
                &text_sha, &size, error) &&
            file_hash_compute_sha256(tsv_path, cancellable,
                &tsv_sha, &size, error) &&
            g_strcmp0(text_sha, expected_text) == 0 &&
            g_strcmp0(tsv_sha, expected_tsv) == 0;
        g_free(directory); g_free(text_path); g_free(tsv_path);
        g_free(text_sha); g_free(tsv_sha);
        g_free(expected_text); g_free(expected_tsv);
        if (!verified) {
            if (error != NULL && *error == NULL)
                g_set_error_literal(error, coordinator_error(), 7,
                    "Un artefact OCR a changé avant la validation.");
            goto cleanup;
        }
    }
    if (!session_valid(options) ||
        fail_at(&failure, PERSON_CREATION_FAILURE_SESSION_BEFORE_COMMIT,
            error)) {
        if (error != NULL && *error == NULL)
            g_set_error_literal(error, coordinator_error(), 8,
                "La session d’enquête a changé avant la validation.");
        goto cleanup;
    }
    if (fail_at(&failure, PERSON_CREATION_FAILURE_COMMIT, error) ||
        !database_transaction_commit(database)) goto cleanup;
    transaction_active = FALSE;
    success = TRUE;
cleanup:
    if (!success && transaction_active)
        database_transaction_rollback(database);
    if (!success)
        for (guint i = 0; i < created_paths->len; i++)
            g_unlink(g_ptr_array_index(created_paths, i));
    if (!success)
        for (guint i = created_directories->len; i > 0; i--) {
            const char *directory =
                g_ptr_array_index(created_directories, i - 1);
            gboolean compensation_failure =
                failure.options != NULL &&
                failure.options->inject_compensation_failure &&
                failure.occurrences[PERSON_CREATION_FAILURE_COMPENSATION]++ == 0;
            if (compensation_failure ||
                fail_at(&failure, PERSON_CREATION_FAILURE_COMPENSATION,
                    error)) {
                if (error != NULL && *error != NULL) {
                    char *message = g_strdup_printf(
                        "%s Compensation incomplète : suppression injectée "
                        "du dossier « %s ».", (*error)->message, directory);
                    g_clear_error(error);
                    g_set_error_literal(error, coordinator_error(), 9,
                        message);
                    g_free(message);
                }
                continue;
            }
            if (g_rmdir(directory) != 0 && errno != ENOENT &&
                errno != ENOTEMPTY && error != NULL && *error == NULL)
                g_set_error(error, coordinator_error(), 5,
                    "Impossible de nettoyer le dossier OCR : %s.",
                    g_strerror(errno));
        }
    g_ptr_array_unref(created_paths);
    g_ptr_array_unref(created_directories);
    entity_record_free(person_record);
    entity_dao_free(entity_dao);
    evidence_dao_free(evidence_dao);
    evidence_entity_dao_free(link_dao);
    person_role_assignment_dao_free(role_dao);
    identity_ocr_dao_free(ocr_dao);
    identity_traceability_dao_free(traceability_dao);
    g_free(timestamp);
    if (!success) {
        person_creation_coordinator_result_free(result);
        result = NULL;
    }
    return result;
}
PersonCreationCoordinatorResult *person_creation_coordinator_execute_with_options(
    Database *database, const char *root, const PersonEntityInput *person,
    const PersonEvidenceSelection *selection, const GPtrArray *ocr_runs,
    const PersonCreationCoordinatorOptions *options,
    GCancellable *cancellable, GError **error)
{
    return person_creation_coordinator_execute_internal(database, root,
        person, NULL, selection, ocr_runs, NULL, options,
        cancellable, error);
}
PersonCreationCoordinatorResult *
person_creation_coordinator_attach_to_existing_person(
    Database *database, const char *root, const char *person_identifier,
    const PersonEvidenceSelection *selection, const GPtrArray *ocr_runs,
    const PersonCreationCoordinatorEvidenceMetadata *metadata,
    const PersonCreationCoordinatorOptions *options,
    GCancellable *cancellable, GError **error)
{
    return person_creation_coordinator_execute_internal(database, root,
        NULL, person_identifier, selection, ocr_runs, metadata, options,
        cancellable, error);
}
PersonCreationCoordinatorResult *person_creation_coordinator_execute(
    Database *database, const char *root, const PersonEntityInput *person,
    const PersonEvidenceSelection *selection, const GPtrArray *ocr_runs,
    GCancellable *cancellable, GError **error)
{
    return person_creation_coordinator_execute_with_options(database, root,
        person, selection, ocr_runs, NULL, cancellable, error);
}
void person_creation_coordinator_result_free(
    PersonCreationCoordinatorResult *result)
{
    if (result == NULL) return;
    g_free(result->person_identifier);
    g_clear_pointer(&result->evidence_identifiers, g_ptr_array_unref);
    g_free(result);
}
