#define _POSIX_C_SOURCE 200809L

/******************************************************************************
 * @file exiftool_persistence_service.c
 * @brief Adapter ExifTool réel et publication transactionnelle V20.
 ******************************************************************************/
#include "core/exiftool_persistence_service.h"

#include "core/exiftool_analysis.h"
#include "core/file_hash.h"
#include "dao/evidence_dao.h"
#include "dao/extraction_dao.h"
#include "database/error.h"
#include "database/transaction.h"
#include "models/evidence_record.h"

#include <errno.h>
#include <glib/gstdio.h>
#include <json-glib/json-glib.h>
#include <string.h>
#include <sys/stat.h>

#define EXIFTOOL_RELATIVE_DIRECTORY "02_Preuves_Traitees/Extractions/ExifTool"

struct ExiftoolPersistenceService {
    Database *database;
    char *root;
    const LocalCapabilityRegistry *registry;
};

static void exiftool_set_error(GError **error, GIOErrorEnum code,
    const char *message)
{
    if (error != NULL && *error == NULL)
        g_set_error_literal(error, G_IO_ERROR, code, message);
}

static gboolean exiftool_uuid_valid(const char *value)
{
    return value != NULL && g_uuid_string_is_valid(value);
}

static gboolean exiftool_relative_safe(const char *relative)
{
    if (relative == NULL || relative[0] == '\0' || g_path_is_absolute(relative))
        return FALSE;
    char **parts = g_strsplit(relative, G_DIR_SEPARATOR_S, -1);
    gboolean safe = parts != NULL;
    for (guint index = 0U; safe && parts[index] != NULL; index++)
        safe = parts[index][0] != '\0' && g_strcmp0(parts[index], ".") != 0 &&
            g_strcmp0(parts[index], "..") != 0;
    g_strfreev(parts);
    return safe;
}

static char *exiftool_controlled_path(const char *root,
    const EvidenceRecord *record, GError **error)
{
    const char *relative = evidence_record_get_relative_path(record);
    if (!exiftool_relative_safe(relative)) {
        exiftool_set_error(error, G_IO_ERROR_INVALID_ARGUMENT,
            "Le chemin relatif de la preuve ExifTool est invalide.");
        return NULL;
    }
    char **parts = g_strsplit(relative, G_DIR_SEPARATOR_S, -1);
    char *current = g_strdup(root);
    for (guint index = 0U; parts != NULL && parts[index] != NULL; index++) {
        char *next = g_build_filename(current, parts[index], NULL);
        GStatBuf status = {0};
        if (next == NULL || g_lstat(next, &status) != 0 ||
            S_ISLNK(status.st_mode)) {
            g_free(next); g_free(current); g_strfreev(parts);
            exiftool_set_error(error, G_IO_ERROR_INVALID_ARGUMENT,
                "La preuve ExifTool est absente ou traverse un lien symbolique.");
            return NULL;
        }
        g_free(current); current = next;
    }
    g_strfreev(parts);
    GStatBuf status = {0};
    if (g_stat(current, &status) != 0 || !S_ISREG(status.st_mode) ||
        (guint64) status.st_size != evidence_record_get_size_bytes(record) ||
        (guint64) status.st_size > EXIFTOOL_PERSISTENCE_MAX_SOURCE) {
        g_free(current);
        exiftool_set_error(error, G_IO_ERROR_INVALID_DATA,
            "La taille ou la nature de la preuve ExifTool est invalide.");
        return NULL;
    }
    char *hash = NULL;
    guint64 hashed_size = 0U;
    if (!file_hash_compute_sha256(current, NULL, &hash, &hashed_size, error) ||
        g_strcmp0(hash, evidence_record_get_sha256(record)) != 0) {
        g_free(hash); g_free(current);
        exiftool_set_error(error, G_IO_ERROR_INVALID_DATA,
            "L'empreinte de la preuve source a divergé.");
        return NULL;
    }
    g_free(hash);
    return current;
}

static char *exiftool_json_artifact(const ExiftoolPersistenceRequest *request,
    const EvidenceRecord *source, const LocalCapabilityStatus *status,
    const LocalToolRunnerResult *run, const ExiftoolAnalysisResult *analysis,
    gsize *out_size)
{
    JsonBuilder *builder = json_builder_new();
    JsonGenerator *generator = json_generator_new();
    json_builder_begin_object(builder);
#define ADD_STRING(name, value) json_builder_set_member_name(builder, name); \
    json_builder_add_string_value(builder, value)
    ADD_STRING("contract", EXIFTOOL_ARTIFACT_CONTRACT);
    ADD_STRING("request_id", request->request_identifier);
    ADD_STRING("created_at", request->requested_at);
    json_builder_set_member_name(builder, "source"); json_builder_begin_object(builder);
    ADD_STRING("evidence_id", request->source_evidence_identifier);
    ADD_STRING("sha256", evidence_record_get_sha256(source));
    json_builder_set_member_name(builder, "size_bytes");
    json_builder_add_int_value(builder, (gint64) evidence_record_get_size_bytes(source));
    json_builder_end_object(builder);
    json_builder_set_member_name(builder, "adapter"); json_builder_begin_object(builder);
    ADD_STRING("id", status->descriptor->adapter_id);
    ADD_STRING("version", status->descriptor->adapter_version);
    ADD_STRING("tool_id", status->descriptor->tool_id);
    ADD_STRING("tool_version", status->tool_version);
    ADD_STRING("capability_id", status->descriptor->capability_id);
    ADD_STRING("output_contract", status->descriptor->output_contract);
    json_builder_end_object(builder);
    json_builder_set_member_name(builder, "parameters"); json_builder_begin_object(builder);
    ADD_STRING("mode", "read_only_json");
    ADD_STRING("groups", "G1");
    json_builder_set_member_name(builder, "numeric_values");
    json_builder_add_boolean_value(builder, TRUE);
    json_builder_end_object(builder);
    json_builder_set_member_name(builder, "execution"); json_builder_begin_object(builder);
    ADD_STRING("state", local_tool_runner_state_code(run->state));
    json_builder_set_member_name(builder, "stdout"); json_builder_begin_object(builder);
    ADD_STRING("encoding", "base64"); ADD_STRING("sha256", run->stdout_sha256);
    gsize stdout_size = 0U; const guint8 *stdout_data =
        g_bytes_get_data(run->stdout_bytes, &stdout_size);
    char *base64 = g_base64_encode(stdout_data, stdout_size);
    ADD_STRING("data", base64); g_free(base64);
    json_builder_set_member_name(builder, "retained_bytes");
    json_builder_add_int_value(builder, (gint64) stdout_size);
    json_builder_set_member_name(builder, "observed_bytes");
    json_builder_add_int_value(builder, (gint64) run->stdout_observed);
    json_builder_set_member_name(builder, "complete");
    json_builder_add_boolean_value(builder, run->stdout_complete);
    json_builder_end_object(builder); json_builder_end_object(builder);
    json_builder_set_member_name(builder, "metadata"); json_builder_begin_array(builder);
    for (guint index = 0U; index < analysis->metadata->len; index++) {
        const DocumentMetadataEntry *entry = g_ptr_array_index(analysis->metadata, index);
        json_builder_begin_object(builder);
        ADD_STRING("code", entry->code); ADD_STRING("group", entry->original_group);
        ADD_STRING("tag", entry->original_tag); ADD_STRING("value", entry->raw_value);
        json_builder_set_member_name(builder, "sensitive");
        json_builder_add_boolean_value(builder, entry->sensitive);
        json_builder_end_object(builder);
    }
    json_builder_end_array(builder); json_builder_end_object(builder);
#undef ADD_STRING
    JsonNode *root = json_builder_get_root(builder);
    json_generator_set_root(generator, root);
    char *data = json_generator_to_data(generator, out_size);
    json_node_unref(root); g_object_unref(generator); g_object_unref(builder);
    return data;
}

static ExiftoolPublicationResult *exiftool_result_new(
    const ExiftoolPersistenceRequest *request, const char *version,
    guint count, gboolean reused)
{
    ExiftoolPublicationResult *result = g_new0(ExiftoolPublicationResult, 1);
    result->request_identifier = g_strdup(request->request_identifier);
    result->derivative_evidence_identifier =
        g_strdup(request->derivative_evidence_identifier);
    result->tool_version = g_strdup(version);
    result->metadata_count = count; result->reused = reused;
    result->status = g_strdup(reused ? "reused" : "completed");
    return result;
}

void exiftool_publication_result_free(ExiftoolPublicationResult *result)
{
    if (result == NULL) return;
    g_free(result->request_identifier); g_free(result->derivative_evidence_identifier);
    g_free(result->tool_version); g_free(result->status); g_free(result);
}

ExiftoolPersistenceService *exiftool_persistence_service_new(
    Database *database, const char *investigation_root,
    const LocalCapabilityRegistry *registry, GError **error)
{
    if (database == NULL || investigation_root == NULL || registry == NULL ||
        !g_file_test(investigation_root, G_FILE_TEST_IS_DIR)) {
        exiftool_set_error(error, G_IO_ERROR_INVALID_ARGUMENT,
            "Le service de publication ExifTool est invalide.");
        return NULL;
    }
    ExiftoolPersistenceService *service = g_new0(ExiftoolPersistenceService, 1);
    service->database = database; service->registry = registry;
    service->root = g_canonicalize_filename(investigation_root, NULL);
    return service;
}

void exiftool_persistence_service_free(ExiftoolPersistenceService *service)
{ if (service != NULL) { g_free(service->root); g_free(service); } }

static ExiftoolPublicationResult *exiftool_try_replay(
    ExiftoolPersistenceService *service, const ExiftoolPersistenceRequest *request,
    const EvidenceRecord *source, GError **error)
{
    ExtractionDao *dao = extraction_dao_new(service->database, error);
    ExtractionRecord *record = dao != NULL
        ? extraction_dao_find_by_identifier(dao, request->request_identifier, error) : NULL;
    extraction_dao_free(dao);
    if (record == NULL) return NULL;
    gboolean compatible = g_strcmp0(record->evidence_identifier,
            request->derivative_evidence_identifier) == 0 &&
        g_strcmp0(record->source_kind, "evidence") == 0 &&
        g_strcmp0(record->source_identifier,
            request->source_evidence_identifier) == 0 &&
        g_str_has_prefix(record->tool_identifier, "exiftool@") &&
        g_strcmp0(record->created_at, request->requested_at) == 0;
    char *historical_version = compatible
        ? g_strdup(record->tool_identifier + strlen("exiftool@")) : NULL;
    extraction_record_free(record);
    if (!compatible) {
        exiftool_set_error(error, G_IO_ERROR_EXISTS,
            "Le request_id ExifTool existe avec un contrat différent.");
        g_free(historical_version); return NULL;
    }
    EvidenceDao *evidence_dao = evidence_dao_new(service->database, error);
    EvidenceRecord *derivative = evidence_dao != NULL
        ? evidence_dao_find_by_identifier(evidence_dao,
            request->derivative_evidence_identifier, error) : NULL;
    evidence_dao_free(evidence_dao);
    if (derivative == NULL) {
        exiftool_set_error(error, G_IO_ERROR_FAILED,
            "RECOVERY_REQUIRED: extraction sans dérivé ExifTool cohérent.");
        g_free(historical_version); return NULL;
    }
    char *derivative_path = exiftool_controlled_path(service->root,
        derivative, error);
    if (derivative_path == NULL) {
        if (error != NULL && *error != NULL) g_clear_error(error);
        exiftool_set_error(error, G_IO_ERROR_FAILED,
            "RECOVERY_REQUIRED: l'artefact ExifTool publié est absent ou altéré.");
        evidence_record_free(derivative); g_free(historical_version); return NULL;
    }
    GStatBuf artifact_stat = {0};
    char *artifact = NULL; gsize artifact_size = 0U;
    JsonParser *parser = json_parser_new();
    gboolean typed = g_stat(derivative_path, &artifact_stat) == 0 &&
        artifact_stat.st_size >= 0 && artifact_stat.st_size <= 1024 * 1024 &&
        g_file_get_contents(derivative_path, &artifact, &artifact_size, NULL) &&
        json_parser_load_from_data(parser, artifact, (gssize) artifact_size, NULL);
    JsonNode *root = typed ? json_parser_get_root(parser) : NULL;
    JsonObject *object = root != NULL && JSON_NODE_HOLDS_OBJECT(root)
        ? json_node_get_object(root) : NULL;
    JsonObject *source_object = object != NULL
        ? json_object_get_object_member(object, "source") : NULL;
    JsonObject *adapter = object != NULL
        ? json_object_get_object_member(object, "adapter") : NULL;
    JsonObject *parameters = object != NULL
        ? json_object_get_object_member(object, "parameters") : NULL;
    JsonObject *execution = object != NULL
        ? json_object_get_object_member(object, "execution") : NULL;
    JsonObject *stdout_object = execution != NULL
        ? json_object_get_object_member(execution, "stdout") : NULL;
    typed = object != NULL && source_object != NULL && adapter != NULL &&
        parameters != NULL && execution != NULL && stdout_object != NULL &&
        g_strcmp0(json_object_get_string_member(object, "contract"),
            EXIFTOOL_ARTIFACT_CONTRACT) == 0 &&
        g_strcmp0(json_object_get_string_member(object, "request_id"),
            request->request_identifier) == 0 &&
        g_strcmp0(json_object_get_string_member(source_object, "evidence_id"),
            request->source_evidence_identifier) == 0 &&
        g_strcmp0(json_object_get_string_member(source_object, "sha256"),
            evidence_record_get_sha256(source)) == 0 &&
        json_object_get_int_member(source_object, "size_bytes") ==
            (gint64) evidence_record_get_size_bytes(source) &&
        g_strcmp0(json_object_get_string_member(adapter, "id"),
            "labfy.adapter.exiftool") == 0 &&
        g_strcmp0(json_object_get_string_member(adapter, "version"), "1") == 0 &&
        g_strcmp0(json_object_get_string_member(adapter, "tool_id"), "exiftool") == 0 &&
        g_strcmp0(json_object_get_string_member(adapter, "tool_version"),
            historical_version) == 0 &&
        g_strcmp0(json_object_get_string_member(parameters, "mode"),
            "read_only_json") == 0 &&
        g_strcmp0(json_object_get_string_member(parameters, "groups"), "G1") == 0 &&
        json_object_get_boolean_member(parameters, "numeric_values") &&
        g_strcmp0(json_object_get_string_member(execution, "state"), "exited") == 0 &&
        json_object_get_boolean_member(stdout_object, "complete");
    g_object_unref(parser); g_free(artifact); g_free(derivative_path);
    if (!typed) {
        evidence_record_free(derivative); g_free(historical_version);
        exiftool_set_error(error, G_IO_ERROR_INVALID_DATA,
            "RECOVERY_REQUIRED: le document dérivé ExifTool ne correspond pas à la demande.");
        return NULL;
    }
    evidence_record_free(derivative);
    ExiftoolPublicationResult *result = exiftool_result_new(request,
        historical_version, 0U, TRUE);
    g_free(historical_version);
    return result;
}

ExiftoolPublicationResult *exiftool_persistence_service_execute(
    ExiftoolPersistenceService *service, const ExiftoolPersistenceRequest *request,
    GCancellable *cancellable, GError **error)
{
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);
    if (service == NULL || request == NULL ||
        !exiftool_uuid_valid(request->request_identifier) ||
        !exiftool_uuid_valid(request->source_evidence_identifier) ||
        !exiftool_uuid_valid(request->derivative_evidence_identifier) ||
        request->requested_at == NULL) {
        exiftool_set_error(error, G_IO_ERROR_INVALID_ARGUMENT,
            "La demande ExifTool est invalide."); return NULL;
    }
    const LocalCapabilityStatus *status = local_capability_registry_lookup(
        service->registry, LOCAL_CAPABILITY_EXIF_METADATA);
    EvidenceDao *evidence_dao = evidence_dao_new(service->database, error);
    EvidenceRecord *source = evidence_dao != NULL
        ? evidence_dao_find_by_identifier(evidence_dao,
            request->source_evidence_identifier, error) : NULL;
    evidence_dao_free(evidence_dao);
    if (source == NULL) { exiftool_set_error(error, G_IO_ERROR_NOT_FOUND,
        "La preuve source ExifTool est absente."); return NULL; }
    char *reason = NULL;
    gboolean applies = local_capability_status_applies(status,
        evidence_record_get_mime_type(source) != NULL
            ? evidence_record_get_mime_type(source) : "image/unknown",
        evidence_record_get_relative_path(source), &reason);
    g_free(reason);
    char *source_path = applies ? exiftool_controlled_path(service->root,
        source, error) : NULL;
    if (!applies || source_path == NULL) { evidence_record_free(source); return NULL; }
    GError *replay_error = NULL;
    ExiftoolPublicationResult *replay = exiftool_try_replay(service, request,
        source, &replay_error);
    if (replay != NULL) {
        g_free(source_path); evidence_record_free(source); return replay;
    }
    if (replay_error != NULL) {
        g_free(source_path); evidence_record_free(source);
        g_propagate_error(error, replay_error); return NULL;
    }
    evidence_dao = evidence_dao_new(service->database, error);
    EvidenceRecord *orphan = evidence_dao != NULL
        ? evidence_dao_find_by_identifier(evidence_dao,
            request->derivative_evidence_identifier, error) : NULL;
    evidence_dao_free(evidence_dao);
    if (orphan != NULL) {
        evidence_record_free(orphan); g_free(source_path);
        evidence_record_free(source);
        exiftool_set_error(error, G_IO_ERROR_FAILED,
            "RECOVERY_REQUIRED: dérivé ExifTool sans extraction idempotente.");
        return NULL;
    }
    if (error != NULL && *error != NULL) {
        g_free(source_path); evidence_record_free(source); return NULL;
    }
    if (status == NULL || status->availability != LOCAL_CAPABILITY_READY ||
        status->executable == NULL) {
        g_free(source_path); evidence_record_free(source);
        exiftool_set_error(error, G_IO_ERROR_NOT_FOUND,
            status != NULL ? status->reason : "La capacité ExifTool est absente.");
        return NULL;
    }
    char *work = g_dir_make_tmp("labfy-exiftool-XXXXXX", error);
    char *copy_path = work != NULL ? g_build_filename(work, "input.bin", NULL) : NULL;
    char *content = NULL; gsize content_size = 0U;
    if (copy_path == NULL || !g_file_get_contents(source_path, &content,
            &content_size, error) || !g_file_set_contents(copy_path, content,
            (gssize) content_size, error) || g_chmod(copy_path, 0600) != 0)
        goto failure;
    const char *argv[] = { status->executable, "-config", "", "-j", "-G1",
        "-n", "--", copy_path, NULL };
    LocalToolRunnerResult *run = NULL;
    if (!local_tool_runner_run(status->executable, argv, work,
            &status->descriptor->limits, cancellable, &run, error)) goto failure;
    if (run->state != LOCAL_TOOL_RUNNER_EXITED || !run->stdout_complete) {
        exiftool_set_error(error, run->state == LOCAL_TOOL_RUNNER_CANCELLED
            ? G_IO_ERROR_CANCELLED : run->state == LOCAL_TOOL_RUNNER_TIMEOUT
                ? G_IO_ERROR_TIMED_OUT : G_IO_ERROR_INVALID_DATA,
            "ExifTool n'a pas produit un résultat JSON complet.");
        local_tool_runner_result_free(run); goto failure;
    }
    gsize stdout_size = 0U; const char *stdout_data =
        g_bytes_get_data(run->stdout_bytes, &stdout_size);
    char *stdout_text = g_strndup(stdout_data, stdout_size);
    ExiftoolAnalysisResult *analysis = exiftool_analysis_parse(copy_path,
        stdout_text, "", run->exit_code, error);
    g_free(stdout_text);
    if (analysis == NULL) { local_tool_runner_result_free(run); goto failure; }
    char *after_hash = NULL;
    guint64 after_size = 0U;
    if (!file_hash_compute_sha256(source_path, cancellable, &after_hash,
            &after_size, error) || after_size != evidence_record_get_size_bytes(source) ||
        g_strcmp0(after_hash,
            evidence_record_get_sha256(source)) != 0) {
        g_free(after_hash); exiftool_analysis_result_free(analysis);
        local_tool_runner_result_free(run); goto failure;
    }
    g_free(after_hash);
    gsize artifact_size = 0U;
    char *artifact = exiftool_json_artifact(request, source, status, run,
        analysis, &artifact_size);
    char *artifact_hash = artifact != NULL ? g_compute_checksum_for_data(
        G_CHECKSUM_SHA256, (const guint8 *) artifact, artifact_size) : NULL;
    char *directory = g_build_filename(service->root,
        EXIFTOOL_RELATIVE_DIRECTORY, NULL);
    char *name = g_strconcat(request->derivative_evidence_identifier, ".json", NULL);
    char *relative = g_build_filename(EXIFTOOL_RELATIVE_DIRECTORY, name, NULL);
    char *final_path = g_build_filename(directory, name, NULL);
    char *stage = g_strconcat(final_path, ".stage", NULL);
    gboolean final_created = FALSE, transaction = FALSE, published = FALSE;
    if (artifact == NULL || artifact_size > status->descriptor->limits.stdout_bytes ||
        g_mkdir_with_parents(directory, 0700) != 0 ||
        g_file_test(final_path, G_FILE_TEST_EXISTS) ||
        !g_file_set_contents(stage, artifact, (gssize) artifact_size, error) ||
        g_chmod(stage, 0600) != 0 || g_rename(stage, final_path) != 0)
        goto publish_cleanup;
    final_created = TRUE;
    GString *summary = g_string_new("Métadonnées dérivées (artefact privé) : ");
    guint summarized = 0U;
    for (guint pass = 0U; pass < 2U && summarized < 6U; pass++) {
        for (guint index = 0U; index < analysis->metadata->len && summarized < 6U; index++) {
            const DocumentMetadataEntry *entry = g_ptr_array_index(analysis->metadata, index);
            /* CONTRACT: aucun chemin local (SourceFile/System/File) ne franchit
             * la projection Web ; le brut complet reste dans l'artefact privé. */
            gboolean xmp = g_str_has_prefix(entry->original_group, "XMP");
            gboolean safe = xmp || g_strcmp0(entry->original_group, "EXIF") == 0 ||
                g_strcmp0(entry->original_group, "PNG") == 0;
            if (!safe || (pass == 0U) != xmp) continue;
            if (summarized++ > 0U) g_string_append(summary, " ; ");
            g_string_append_printf(summary, "%s:%s=%s", entry->original_group,
                entry->original_tag, entry->raw_value);
            if (summary->len > 480U) { g_string_truncate(summary, 480U); break; }
        }
    }
    EvidenceRecord *derivative = evidence_record_new(
        request->derivative_evidence_identifier,
        "Métadonnées ExifTool persistées.json", name, relative, "text",
        artifact_size, artifact_hash, request->requested_at,
        request->requested_at, "Adapter ExifTool local J4",
        summary->str,
        EVIDENCE_INTEGRITY_STATUS_VALID, error);
    g_string_free(summary, TRUE);
    if (derivative == NULL || !database_transaction_begin(service->database)) {
        evidence_record_free(derivative); goto publish_cleanup;
    }
    transaction = TRUE;
    EvidenceDao *output_dao = evidence_dao_new(service->database, error);
    ExtractionDao *extraction_dao = extraction_dao_new(service->database, error);
    char *persisted_tool = g_strconcat("exiftool@", status->tool_version, NULL);
    if (output_dao != NULL && extraction_dao != NULL &&
        evidence_dao_insert(output_dao, derivative, error) &&
        extraction_dao_insert(extraction_dao, request->request_identifier,
            request->derivative_evidence_identifier, "evidence",
            request->source_evidence_identifier, persisted_tool,
            request->requested_at, error) &&
        database_transaction_commit(service->database)) {
        transaction = FALSE; published = TRUE;
    }
    evidence_dao_free(output_dao); extraction_dao_free(extraction_dao);
    g_free(persisted_tool);
    evidence_record_free(derivative);
publish_cleanup:
    if (!published && transaction) database_transaction_rollback(service->database);
    if (!published && final_created) (void) g_remove(final_path);
    (void) g_remove(stage);
    ExiftoolPublicationResult *result = published
        ? exiftool_result_new(request, status->tool_version,
            analysis->metadata->len, FALSE) : NULL;
    if (!published) exiftool_set_error(error, G_IO_ERROR_FAILED,
        database_error_get_message(service->database) != NULL
            ? database_error_get_message(service->database)
            : "La publication ExifTool a échoué.");
    g_free(stage); g_free(final_path); g_free(relative); g_free(name);
    g_free(directory); g_free(artifact_hash); g_free(artifact);
    exiftool_analysis_result_free(analysis); local_tool_runner_result_free(run);
    g_free(content); (void) g_remove(copy_path); (void) g_rmdir(work);
    g_free(copy_path); g_free(work); g_free(source_path);
    evidence_record_free(source); return result;
failure:
    g_free(content); if (copy_path != NULL) (void) g_remove(copy_path);
    if (work != NULL) (void) g_rmdir(work);
    g_free(copy_path); g_free(work); g_free(source_path);
    evidence_record_free(source); return NULL;
}
