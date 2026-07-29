#include "core/evidence_identity_import_task.h"

#include "core/evidence_staging.h"
#include "core/person_creation_coordinator.h"
#include "database/database.h"
#include "dao/evidence_dao.h"
#include "dao/identity_ocr_dao.h"
#include "models/person_evidence_selection.h"

struct EvidenceIdentityImportTaskRequest {
    char *database_path;
    char *root;
    char *source_path;
    char *person_identifier;
    char *type_identifier;
    char *collected_at;
    char *source;
    char *description;
    IdentityOcrRun *run;
    char *evidence_identifier;
    gboolean review;
    PersonCreationSessionCheck session_check;
    gpointer session_check_data;
};

EvidenceIdentityImportTaskRequest *
evidence_identity_import_task_request_new(
    const char *database_path, const char *root,
    const char *source_path, const char *person_identifier,
    const char *type_identifier, const char *collected_at,
    const char *source, const char *description,
    const IdentityOcrRun *run)
{
    if (database_path == NULL || root == NULL || source_path == NULL ||
        person_identifier == NULL || type_identifier == NULL ||
        collected_at == NULL || run == NULL) return NULL;
    EvidenceIdentityImportTaskRequest *request =
        g_new0(EvidenceIdentityImportTaskRequest, 1);
    request->database_path = g_strdup(database_path);
    request->root = g_strdup(root);
    request->source_path = g_strdup(source_path);
    request->person_identifier = g_strdup(person_identifier);
    request->type_identifier = g_strdup(type_identifier);
    request->collected_at = g_strdup(collected_at);
    request->source = g_strdup(source);
    request->description = g_strdup(description);
    request->run = identity_ocr_run_copy(run);
    return request;
}

EvidenceIdentityImportTaskRequest *
evidence_identity_import_task_request_new_review(
    const char *database_path, const char *root,
    const char *evidence_identifier, const IdentityOcrRun *run)
{
    if (database_path == NULL || root == NULL ||
        evidence_identifier == NULL || run == NULL) return NULL;
    EvidenceIdentityImportTaskRequest *request =
        g_new0(EvidenceIdentityImportTaskRequest, 1);
    request->database_path = g_strdup(database_path);
    request->root = g_strdup(root);
    request->evidence_identifier = g_strdup(evidence_identifier);
    request->run = identity_ocr_run_copy(run);
    request->review = TRUE;
    return request;
}

EvidenceIdentityImportTaskRequest *
evidence_identity_import_task_request_new_existing(
    const char *database_path, const char *root,
    const char *evidence_identifier, const char *person_identifier,
    const IdentityOcrRun *run)
{
    if (database_path == NULL || root == NULL ||
        evidence_identifier == NULL || person_identifier == NULL ||
        run == NULL) return NULL;
    EvidenceIdentityImportTaskRequest *request =
        g_new0(EvidenceIdentityImportTaskRequest, 1);
    request->database_path = g_strdup(database_path);
    request->root = g_strdup(root);
    request->evidence_identifier = g_strdup(evidence_identifier);
    request->person_identifier = g_strdup(person_identifier);
    request->run = identity_ocr_run_copy(run);
    return request;
}

void evidence_identity_import_task_request_set_session_check(
    EvidenceIdentityImportTaskRequest *request,
    PersonCreationSessionCheck session_check,
    gpointer session_check_data)
{
    if (request == NULL) return;
    request->session_check = session_check;
    request->session_check_data = session_check_data;
}

void evidence_identity_import_task_request_free(
    EvidenceIdentityImportTaskRequest *request)
{
    if (request == NULL) return;
    g_free(request->database_path); g_free(request->root);
    g_free(request->source_path); g_free(request->person_identifier);
    g_free(request->type_identifier); g_free(request->collected_at);
    g_free(request->source); g_free(request->description);
    g_free(request->evidence_identifier);
    identity_ocr_run_free(request->run);
    g_free(request);
}

static gboolean evidence_identity_import_worker(
    BackgroundTask *task, GCancellable *cancellable,
    gpointer data, gpointer *result, GError **error)
{
    EvidenceIdentityImportTaskRequest *request = data;
    EvidenceStaging *staging = NULL;
    EvidenceStagingResult *prepared = NULL;
    PersonEvidenceSelection *selection = NULL;
    GPtrArray *runs = NULL;
    Database *database = NULL;
    PersonCreationCoordinatorResult *coordinator_result = NULL;
    PersonCreationCoordinatorEvidenceMetadata metadata = {
        .collected_at = request->collected_at,
        .source = request->source,
        .description = request->description,
        .type_identifier = request->type_identifier
    };
    PersonCreationCoordinatorOptions options = {
        .session_check = request->session_check,
        .session_check_data = request->session_check_data
    };
    (void) task;
    *result = NULL;
    if (request->review) {
        IdentityOcrDao *dao = NULL;
        IdentityOcrRun *verified = NULL;
        GDateTime *now = NULL;
        char *timestamp = NULL;
        if (g_cancellable_set_error_if_cancelled(cancellable, error))
            goto cleanup;
        if (request->session_check != NULL &&
            !request->session_check(request->session_check_data)) {
            g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_CANCELLED,
                "La session d’enquête a changé avant la révision.");
            goto cleanup;
        }
        database = database_open(request->database_path);
        if (database == NULL) {
            g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                "Impossible d’ouvrir la base de l’enquête.");
            goto cleanup;
        }
        dao = identity_ocr_dao_new(database);
        now = g_date_time_new_now_utc();
        timestamp = g_date_time_format(now, "%Y-%m-%dT%H:%M:%SZ");
        if (dao == NULL || timestamp == NULL ||
            !identity_ocr_dao_update_review(
                dao, request->run, timestamp, error))
            goto review_cleanup;
        database_close(database);
        database = database_open(request->database_path);
        identity_ocr_dao_free(dao);
        dao = identity_ocr_dao_new(database);
        if (database == NULL || dao == NULL ||
            (verified = identity_ocr_dao_load_run(
                dao, request->root,
                identity_ocr_run_get_identifier(request->run),
                NULL, error)) == NULL ||
            g_strcmp0(identity_ocr_run_get_identifier(verified),
                identity_ocr_run_get_identifier(request->run)) != 0 ||
            g_strcmp0(identity_ocr_run_get_raw_text(verified),
                identity_ocr_run_get_raw_text(request->run)) != 0) {
            if (error != NULL && *error == NULL)
                g_set_error_literal(error, G_IO_ERROR,
                    G_IO_ERROR_INVALID_DATA,
                    "La révision OCR n’est pas relisible après validation.");
            identity_ocr_run_free(verified);
            verified = NULL;
        }
review_cleanup:
        g_free(timestamp);
        if (now != NULL) g_date_time_unref(now);
        identity_ocr_dao_free(dao);
        if (verified != NULL) {
            *result = verified;
            database_close(database);
            database = NULL;
            return TRUE;
        }
        goto cleanup;
    }
    if (request->evidence_identifier != NULL) {
        EvidenceDao *evidence_dao = NULL;
        EvidenceRecord *record = NULL;
        database = database_open(request->database_path);
        if (database == NULL) {
            g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                "Impossible d’ouvrir la base temporaire de l’enquête.");
            goto cleanup;
        }
        evidence_dao = evidence_dao_new(database, error);
        if (evidence_dao != NULL)
            record = evidence_dao_find_by_identifier(evidence_dao,
                request->evidence_identifier, error);
        if (record == NULL ||
            !person_evidence_selection_add_existing(selection =
                person_evidence_selection_new(), record, error) ||
            !identity_ocr_run_replace_evidence_id(request->run,
                request->evidence_identifier))
        {
            if (error != NULL && *error == NULL)
                g_set_error_literal(error, G_IO_ERROR,
                    G_IO_ERROR_INVALID_DATA,
                    "La preuve existante ne peut pas recevoir l’analyse OCR.");
            evidence_record_free(record);
            evidence_dao_free(evidence_dao);
            goto cleanup;
        }
        runs = g_ptr_array_new_with_free_func(
            (GDestroyNotify) identity_ocr_run_free);
        g_ptr_array_add(runs, identity_ocr_run_copy(request->run));
        coordinator_result =
            person_creation_coordinator_attach_to_existing_person(
                database, request->root, request->person_identifier,
                selection, runs, NULL, &options, cancellable, error);
        evidence_record_free(record);
        evidence_dao_free(evidence_dao);
        if (coordinator_result == NULL) goto cleanup;
        database_close(database);
        database = database_open(request->database_path);
        IdentityOcrDao *verification_dao =
            identity_ocr_dao_new(database);
        IdentityOcrRunRecord *persisted =
            verification_dao != NULL
            ? identity_ocr_dao_find_run(verification_dao,
                identity_ocr_run_get_identifier(request->run), error)
            : NULL;
        if (persisted == NULL ||
            g_strcmp0(persisted->evidence_id,
                request->evidence_identifier) != 0 ||
            g_strcmp0(persisted->expected_sha256,
                identity_ocr_run_get_expected_sha256(
                    request->run)) != 0) {
            if (error != NULL && *error == NULL)
                g_set_error_literal(error, G_IO_ERROR,
                    G_IO_ERROR_INVALID_DATA,
                    "L’analyse OCR n’est pas relisible après réouverture.");
            identity_ocr_run_record_free(persisted);
            identity_ocr_dao_free(verification_dao);
            goto cleanup;
        }
        identity_ocr_run_record_free(persisted);
        identity_ocr_dao_free(verification_dao);
        *result = coordinator_result;
        coordinator_result = NULL;
        goto cleanup;
    }
    staging = evidence_staging_new(error);
    if (staging == NULL) goto cleanup;
    prepared = evidence_staging_prepare(
        staging, request->source_path, cancellable, error);
    if (prepared == NULL) goto cleanup;
    selection = person_evidence_selection_new();
    if (!person_evidence_selection_add_staged(selection,
            prepared->source_path, prepared->staging_path,
            prepared->original_name, prepared->mime_type,
            request->type_identifier, prepared->size_bytes,
            prepared->sha256, request->description,
            prepared->prepared_at, error))
        goto cleanup;
    const PersonEvidenceSelectionItem *item =
        person_evidence_selection_get(selection, 0);
    if (!identity_ocr_run_replace_evidence_id(request->run,
            person_evidence_selection_item_get_identifier(item))) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
            "L’identifiant temporaire OCR ne peut pas être remplacé.");
        goto cleanup;
    }
    runs = g_ptr_array_new_with_free_func(
        (GDestroyNotify) identity_ocr_run_free);
    g_ptr_array_add(runs, identity_ocr_run_copy(request->run));
    database = database_open(request->database_path);
    if (database == NULL) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED,
            "Impossible d’ouvrir la base temporaire de l’enquête.");
        goto cleanup;
    }
    coordinator_result =
        person_creation_coordinator_attach_to_existing_person(
            database, request->root, request->person_identifier,
            selection, runs, &metadata, &options, cancellable, error);
    if (coordinator_result == NULL) goto cleanup;
    *result = coordinator_result;
    coordinator_result = NULL;

cleanup:
    person_creation_coordinator_result_free(coordinator_result);
    database_close(database);
    g_clear_pointer(&runs, g_ptr_array_unref);
    person_evidence_selection_free(selection);
    evidence_staging_result_free(prepared);
    if (*result == NULL) evidence_staging_cleanup(staging, NULL);
    evidence_staging_free(staging);
    return *result != NULL;
}

BackgroundTask *evidence_identity_import_task_start(
    TaskManager *manager,
    const EvidenceIdentityImportTaskRequest *request,
    BackgroundTaskCompletionCallback callback,
    gpointer callback_data, GDestroyNotify callback_data_destroy,
    GError **error)
{
    if (manager == NULL || request == NULL) return NULL;
    EvidenceIdentityImportTaskRequest *copy =
        request->review
        ? evidence_identity_import_task_request_new_review(
            request->database_path, request->root,
            request->evidence_identifier, request->run)
        : request->evidence_identifier != NULL
        ? evidence_identity_import_task_request_new_existing(
            request->database_path, request->root,
            request->evidence_identifier, request->person_identifier,
            request->run)
        : evidence_identity_import_task_request_new(
            request->database_path, request->root, request->source_path,
            request->person_identifier, request->type_identifier,
            request->collected_at, request->source,
            request->description, request->run);
    if (copy != NULL)
        evidence_identity_import_task_request_set_session_check(
            copy, request->session_check, request->session_check_data);
    BackgroundTask *task = background_task_new(request->review
        ? "Enregistrement de la révision OCR"
        : "Import de la preuve et conservation OCR");
    if (copy == NULL || task == NULL ||
        !task_manager_add(manager, task, error) ||
        !background_task_start(task, evidence_identity_import_worker,
            copy,
            (GDestroyNotify) evidence_identity_import_task_request_free,
            request->review
            ? (GDestroyNotify) identity_ocr_run_free
            : (GDestroyNotify) person_creation_coordinator_result_free,
            callback, callback_data, callback_data_destroy, error)) {
        evidence_identity_import_task_request_free(copy);
        background_task_unref(task);
        return NULL;
    }
    return task;
}
