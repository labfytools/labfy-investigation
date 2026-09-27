#define _POSIX_C_SOURCE 200809L

/******************************************************************************
 * @file eml_analysis_persistence_service.c
 * @brief Frontière durable de l'analyse locale des en-têtes EML.
 ******************************************************************************/

#include "core/eml_analysis_persistence_service.h"

#include "core/eml_analyzer.h"
#include "core/file_hash.h"
#include "core/observation_review_service.h"
#include "dao/evidence_dao.h"
#include "dao/evidence_entity_dao.h"
#include "dao/extraction_dao.h"
#include "database/error.h"
#include "database/transaction.h"
#include "models/evidence_observation.h"
#include "models/evidence_record.h"

#include <glib/gstdio.h>
#include <json-glib/json-glib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#define EML_ANALYSIS_ARTIFACT_CONTRACT "labfy.eml_analysis.derivative.v1"
#define EML_ANALYSIS_RELATIVE_DIRECTORY \
    "02_Preuves_Traitees/Extractions/EML"

struct EmlAnalysisPersistenceService
{
    Database *database;
    char *root;
};

struct EmlAnalysisPublicationResult
{
    char *request_identifier;
    char *derivative_evidence_identifier;
    GPtrArray *observation_identifiers;
    GPtrArray *warnings;
    char *status;
    gboolean reused;
};

struct EmlAnalysisPrepared
{
    EmlAnalysisPersistenceRequest request;
    char *request_identifier;
    char *source_evidence_identifier;
    char *derivative_evidence_identifier;
    char *requested_at;
    char *source_sha256;
    guint64 source_size;
    char *artifact_data;
    gsize artifact_size;
    char *artifact_sha256;
    EmlAnalysis *analysis;
    EmlAnalysisPublicationResult *existing_result;
};

#ifdef EML_ANALYSIS_PERSISTENCE_ENABLE_TEST_HOOKS
static gboolean eml_analysis_fail_after_derivative_insert = FALSE;

void eml_analysis_persistence_test_fail_after_derivative_insert(
    gboolean enabled)
{
    eml_analysis_fail_after_derivative_insert = enabled;
}
#endif

GQuark eml_analysis_persistence_error_quark(void)
{
    return g_quark_from_static_string(
        "eml-analysis-persistence-error-quark");
}

static void eml_analysis_set_error(
    GError **error,
    EmlAnalysisPersistenceError code,
    const char *message)
{
    if (error != NULL && *error == NULL)
        g_set_error_literal(error, EML_ANALYSIS_PERSISTENCE_ERROR, code, message);
}

static gboolean eml_analysis_check_cancelled(
    GCancellable *cancellable,
    GError **error)
{
    if (cancellable == NULL || !g_cancellable_is_cancelled(cancellable))
        return FALSE;
    eml_analysis_set_error(error, EML_ANALYSIS_PERSISTENCE_ERROR_CANCELLED,
        "L'analyse EML a été annulée.");
    return TRUE;
}

static gboolean eml_analysis_timestamp_is_valid(const char *value)
{
    return value != NULL && strlen(value) == 20U && value[4] == '-' &&
        value[7] == '-' && value[10] == 'T' && value[13] == ':' &&
        value[16] == ':' && value[19] == 'Z';
}

static gboolean eml_analysis_path_is_within(
    const char *root,
    const char *path)
{
    gsize length = root != NULL ? strlen(root) : 0U;
    return length > 0U && path != NULL && g_str_has_prefix(path, root) &&
        path[length] == G_DIR_SEPARATOR;
}

static gboolean eml_analysis_relative_path_is_safe(const char *relative)
{
    const char *cursor = relative;
    if (relative == NULL || relative[0] == '\0' || g_path_is_absolute(relative))
        return FALSE;
    while (*cursor != '\0')
    {
        const char *separator = strchr(cursor, G_DIR_SEPARATOR);
        gsize length = separator != NULL ? (gsize) (separator - cursor)
            : strlen(cursor);
        if (length == 0U || (length == 1U && cursor[0] == '.') ||
            (length == 2U && cursor[0] == '.' && cursor[1] == '.'))
            return FALSE;
        cursor = separator != NULL ? separator + 1 : cursor + length;
    }
    return TRUE;
}

static gboolean eml_analysis_reject_symlink_components(
    const char *root,
    const char *relative,
    GError **error)
{
    char **parts = g_strsplit(relative, G_DIR_SEPARATOR_S, -1);
    char *current = g_strdup(root);
    gboolean success = current != NULL && parts != NULL;
    for (guint index = 0U; success && parts[index] != NULL; index++)
    {
        char *next = g_build_filename(current, parts[index], NULL);
        GStatBuf status = {0};
        if (next == NULL || g_lstat(next, &status) != 0 || S_ISLNK(status.st_mode))
            success = FALSE;
        g_free(current);
        current = next;
    }
    if (!success)
        eml_analysis_set_error(error, EML_ANALYSIS_PERSISTENCE_ERROR_SOURCE,
            "Le chemin de preuve est absent ou traverse un lien symbolique.");
    g_free(current);
    g_strfreev(parts);
    return success;
}

static gboolean eml_analysis_create_private_directory(
    const char *root,
    const char *relative,
    GError **error)
{
    char **parts = g_strsplit(relative, G_DIR_SEPARATOR_S, -1);
    char *current = g_strdup(root);
    gboolean success = parts != NULL && current != NULL;
    for (guint index = 0U; success && parts[index] != NULL; index++)
    {
        char *next = g_build_filename(current, parts[index], NULL);
        GStatBuf status = {0};
        if (next == NULL)
            success = FALSE;
        else if (g_lstat(next, &status) != 0)
        {
            success = errno == ENOENT && g_mkdir(next, 0700) == 0 &&
                g_lstat(next, &status) == 0;
        }
        if (success && (S_ISLNK(status.st_mode) || !S_ISDIR(status.st_mode)))
            success = FALSE;
        g_free(current);
        current = next;
    }
    if (!success)
        eml_analysis_set_error(error,
            EML_ANALYSIS_PERSISTENCE_ERROR_ARTIFACT,
            "Le répertoire privé des dérivés est invalide ou symbolique.");
    g_free(current);
    g_strfreev(parts);
    return success;
}

static gboolean eml_analysis_path_is_absent(const char *path)
{
    GStatBuf status = {0};
    errno = 0;
    return path != NULL && g_lstat(path, &status) != 0 && errno == ENOENT;
}

static char *eml_analysis_source_path(
    EmlAnalysisPersistenceService *service,
    const EvidenceRecord *record,
    GError **error)
{
    const char *relative = evidence_record_get_relative_path(record);
    char *path = NULL;
    char *canonical = NULL;
    GStatBuf status = {0};
    if (!eml_analysis_relative_path_is_safe(relative) ||
        !eml_analysis_reject_symlink_components(service->root, relative, error))
        return NULL;
    path = g_build_filename(service->root, relative, NULL);
    canonical = g_canonicalize_filename(path, NULL);
    if (path == NULL || canonical == NULL ||
        !eml_analysis_path_is_within(service->root, canonical) ||
        g_stat(canonical, &status) != 0 || !S_ISREG(status.st_mode))
    {
        eml_analysis_set_error(error, EML_ANALYSIS_PERSISTENCE_ERROR_SOURCE,
            "La preuve EML n'est pas un fichier régulier dans l'enquête.");
        g_clear_pointer(&canonical, g_free);
    }
    g_free(path);
    return canonical;
}

static EmlAnalysisPublicationResult *eml_analysis_result_new(
    const char *request_identifier,
    const char *derivative_identifier,
    gboolean reused)
{
    EmlAnalysisPublicationResult *result =
        g_new0(EmlAnalysisPublicationResult, 1);
    if (result == NULL) return NULL;
    result->request_identifier = g_strdup(request_identifier);
    result->derivative_evidence_identifier = g_strdup(derivative_identifier);
    result->observation_identifiers = g_ptr_array_new_with_free_func(g_free);
    result->warnings = g_ptr_array_new_with_free_func(g_free);
    result->status = g_strdup("completed");
    result->reused = reused;
    if (result->request_identifier == NULL ||
        result->derivative_evidence_identifier == NULL ||
        result->observation_identifiers == NULL || result->warnings == NULL ||
        result->status == NULL)
    {
        eml_analysis_publication_result_free(result);
        return NULL;
    }
    return result;
}

void eml_analysis_publication_result_free(
    EmlAnalysisPublicationResult *result)
{
    if (result == NULL) return;
    g_free(result->request_identifier);
    g_free(result->derivative_evidence_identifier);
    g_clear_pointer(&result->observation_identifiers, g_ptr_array_unref);
    g_clear_pointer(&result->warnings, g_ptr_array_unref);
    g_free(result->status);
    g_free(result);
}

static EmlAnalysisPublicationResult *eml_analysis_result_copy(
    const EmlAnalysisPublicationResult *source)
{
    EmlAnalysisPublicationResult *copy = source != NULL
        ? eml_analysis_result_new(source->request_identifier,
            source->derivative_evidence_identifier, source->reused) : NULL;
    if (copy == NULL) return NULL;
    for (guint index = 0U; index < source->observation_identifiers->len; index++)
        g_ptr_array_add(copy->observation_identifiers, g_strdup(
            g_ptr_array_index(source->observation_identifiers, index)));
    for (guint index = 0U; index < source->warnings->len; index++)
        g_ptr_array_add(copy->warnings, g_strdup(
            g_ptr_array_index(source->warnings, index)));
    return copy;
}

static void eml_analysis_json_string(
    JsonBuilder *builder,
    const char *name,
    const char *value)
{
    json_builder_set_member_name(builder, name);
    json_builder_add_string_value(builder, value);
}

static char *eml_analysis_build_artifact(
    const EmlAnalysisPrepared *prepared,
    gsize *out_size,
    GError **error)
{
    JsonBuilder *builder = json_builder_new();
    JsonGenerator *generator = json_generator_new();
    JsonNode *root = NULL;
    char *data = NULL;
    const GPtrArray *observations = eml_analysis_get_observations(
        prepared->analysis);
    if (builder == NULL || generator == NULL || observations == NULL)
        goto failure;
    json_builder_begin_object(builder);
    eml_analysis_json_string(builder, "contract",
        EML_ANALYSIS_ARTIFACT_CONTRACT);
    eml_analysis_json_string(builder, "request_id",
        prepared->request_identifier);
    eml_analysis_json_string(builder, "created_at", prepared->requested_at);
    json_builder_set_member_name(builder, "source");
    json_builder_begin_object(builder);
    eml_analysis_json_string(builder, "evidence_id",
        prepared->source_evidence_identifier);
    eml_analysis_json_string(builder, "sha256", prepared->source_sha256);
    json_builder_set_member_name(builder, "size_bytes");
    json_builder_add_int_value(builder, (gint64) prepared->source_size);
    json_builder_end_object(builder);
    json_builder_set_member_name(builder, "analyzer");
    json_builder_begin_object(builder);
    eml_analysis_json_string(builder, "id", EML_ANALYSIS_TOOL_ID);
    eml_analysis_json_string(builder, "version", EML_ANALYSIS_TOOL_VERSION);
    json_builder_end_object(builder);
    json_builder_set_member_name(builder, "parameters");
    json_builder_begin_object(builder);
    eml_analysis_json_string(builder, "scope", "headers-only");
    json_builder_set_member_name(builder, "max_source_bytes");
    json_builder_add_int_value(builder, EML_ANALYSIS_MAX_SOURCE_BYTES);
    json_builder_set_member_name(builder, "max_observations");
    json_builder_add_int_value(builder, EML_ANALYSIS_MAX_OBSERVATIONS);
    json_builder_end_object(builder);
    json_builder_set_member_name(builder, "observations");
    json_builder_begin_array(builder);
    for (guint index = 0U; index < observations->len; index++)
    {
        const EmlObservation *item = g_ptr_array_index(
            (GPtrArray *) observations, index);
        json_builder_begin_object(builder);
        eml_analysis_json_string(builder, "type", item->type_identifier);
        eml_analysis_json_string(builder, "value_raw", item->value_raw);
        eml_analysis_json_string(builder, "value_normalized",
            item->value_normalized);
        eml_analysis_json_string(builder, "role", item->role);
        eml_analysis_json_string(builder, "source_header",
            item->source_header);
        json_builder_set_member_name(builder, "occurrence");
        json_builder_add_int_value(builder, item->occurrence);
        eml_analysis_json_string(builder, "provenance_kind",
            item->provenance_kind);
        json_builder_end_object(builder);
    }
    json_builder_end_array(builder);
    json_builder_end_object(builder);
    root = json_builder_get_root(builder);
    if (root == NULL) goto failure;
    json_generator_set_root(generator, root);
    data = json_generator_to_data(generator, out_size);
    if (data == NULL || *out_size > EML_ANALYSIS_MAX_ARTIFACT_BYTES)
    {
        g_clear_pointer(&data, g_free);
        eml_analysis_set_error(error, EML_ANALYSIS_PERSISTENCE_ERROR_LIMIT,
            "Le dérivé d'analyse dépasse la limite de 1 Mio.");
    }
    json_node_unref(root);
    g_object_unref(generator);
    g_object_unref(builder);
    return data;
failure:
    if (root != NULL) json_node_unref(root);
    g_clear_object(&generator);
    g_clear_object(&builder);
    eml_analysis_set_error(error, EML_ANALYSIS_PERSISTENCE_ERROR_ARTIFACT,
        "Impossible de construire le dérivé structuré EML.");
    return NULL;
}

static gboolean eml_analysis_json_string_equals(
    JsonObject *object,
    const char *member,
    const char *expected)
{
    JsonNode *node = object != NULL
        ? json_object_get_member(object, member) : NULL;
    return node != NULL && JSON_NODE_HOLDS_VALUE(node) &&
        json_node_get_value_type(node) == G_TYPE_STRING &&
        g_strcmp0(json_node_get_string(node), expected) == 0;
}

static JsonObject *eml_analysis_json_object_member(
    JsonObject *object,
    const char *member)
{
    JsonNode *node = object != NULL
        ? json_object_get_member(object, member) : NULL;
    return node != NULL && JSON_NODE_HOLDS_OBJECT(node)
        ? json_node_get_object(node) : NULL;
}

static JsonArray *eml_analysis_json_array_member(
    JsonObject *object,
    const char *member)
{
    JsonNode *node = object != NULL
        ? json_object_get_member(object, member) : NULL;
    return node != NULL && JSON_NODE_HOLDS_ARRAY(node)
        ? json_node_get_array(node) : NULL;
}

static gboolean eml_analysis_json_integer_equals(
    JsonObject *object,
    const char *member,
    gint64 expected)
{
    JsonNode *node = object != NULL
        ? json_object_get_member(object, member) : NULL;
    return node != NULL && JSON_NODE_HOLDS_VALUE(node) &&
        json_node_get_value_type(node) == G_TYPE_INT64 &&
        json_node_get_int(node) == expected;
}

static gboolean eml_analysis_verify_artifact(
    const char *data,
    gsize length,
    const EmlAnalysisPersistenceRequest *request,
    const char *source_sha256,
    guint64 source_size,
    guint *out_observation_count)
{
    JsonParser *parser = json_parser_new();
    JsonNode *root = NULL;
    JsonObject *object = NULL;
    JsonObject *source = NULL;
    JsonObject *analyzer = NULL;
    JsonObject *parameters = NULL;
    JsonArray *observations = NULL;
    gboolean valid = parser != NULL &&
        json_parser_load_from_data(parser, data, (gssize) length, NULL);
    if (!valid) goto cleanup;
    root = json_parser_get_root(parser);
    if (root == NULL || !JSON_NODE_HOLDS_OBJECT(root))
    { valid = FALSE; goto cleanup; }
    object = json_node_get_object(root);
    source = eml_analysis_json_object_member(object, "source");
    analyzer = eml_analysis_json_object_member(object, "analyzer");
    parameters = eml_analysis_json_object_member(object, "parameters");
    observations = eml_analysis_json_array_member(object, "observations");
    valid = eml_analysis_json_string_equals(object, "contract",
            EML_ANALYSIS_ARTIFACT_CONTRACT) &&
        eml_analysis_json_string_equals(object, "request_id",
            request->request_identifier) &&
        eml_analysis_json_string_equals(object, "created_at",
            request->requested_at) &&
        eml_analysis_json_string_equals(source, "evidence_id",
            request->source_evidence_identifier) &&
        eml_analysis_json_string_equals(source, "sha256", source_sha256) &&
        source_size <= G_MAXINT64 &&
        eml_analysis_json_integer_equals(source, "size_bytes",
            (gint64) source_size) &&
        eml_analysis_json_string_equals(analyzer, "id", EML_ANALYSIS_TOOL_ID) &&
        eml_analysis_json_string_equals(analyzer, "version",
            EML_ANALYSIS_TOOL_VERSION) &&
        eml_analysis_json_string_equals(parameters, "scope", "headers-only") &&
        eml_analysis_json_integer_equals(parameters, "max_source_bytes",
            EML_ANALYSIS_MAX_SOURCE_BYTES) &&
        eml_analysis_json_integer_equals(parameters, "max_observations",
            EML_ANALYSIS_MAX_OBSERVATIONS) &&
        observations != NULL;
    if (valid && out_observation_count != NULL)
        *out_observation_count = json_array_get_length(observations);
cleanup:
    g_clear_object(&parser);
    return valid;
}

static gboolean eml_analysis_artifact_matches_observations(
    ObservationReviewService *review_service,
    const char *data,
    gsize length,
    const EmlAnalysisPersistenceRequest *request,
    const GPtrArray *persisted,
    GError **error)
{
    JsonParser *parser = json_parser_new();
    JsonNode *root = NULL;
    JsonArray *artifact = NULL;
    gboolean *matched = NULL;
    guint persisted_count = 0U;
    gboolean valid = parser != NULL && persisted != NULL &&
        json_parser_load_from_data(parser, data, (gssize) length, NULL);
    if (!valid) goto cleanup;
    root = json_parser_get_root(parser);
    if (root == NULL || !JSON_NODE_HOLDS_OBJECT(root))
    { valid = FALSE; goto cleanup; }
    artifact = eml_analysis_json_array_member(json_node_get_object(root),
        "observations");
    if (artifact == NULL) { valid = FALSE; goto cleanup; }
    matched = g_new0(gboolean, json_array_get_length(artifact));
    if (matched == NULL && json_array_get_length(artifact) > 0U)
    { valid = FALSE; goto cleanup; }
    for (guint index = 0U; valid && index < persisted->len; index++)
    {
        const EvidenceObservation *observation = g_ptr_array_index(
            (GPtrArray *) persisted, index);
        if (g_strcmp0(observation->extraction_identifier,
                request->request_identifier) != 0) continue;
        persisted_count++;
        gboolean found = FALSE;
        for (guint item = 0U; !found && item < json_array_get_length(artifact);
             item++)
        {
            JsonNode *node = json_array_get_element(artifact, item);
            JsonObject *object = node != NULL && JSON_NODE_HOLDS_OBJECT(node)
                ? json_node_get_object(node) : NULL;
            if (matched[item] || object == NULL) continue;
            found = eml_analysis_json_string_equals(object, "type",
                    observation->type_identifier) &&
                eml_analysis_json_string_equals(object, "value_raw",
                    observation->value_raw) &&
                eml_analysis_json_string_equals(object, "value_normalized",
                    observation->value_normalized) &&
                eml_analysis_json_string_equals(object, "role",
                    observation->role) &&
                eml_analysis_json_string_equals(object, "source_header",
                    observation->source_header) &&
                eml_analysis_json_integer_equals(object, "occurrence",
                    observation->occurrence) &&
                eml_analysis_json_string_equals(object, "provenance_kind",
                    observation->provenance_kind);
            if (found) matched[item] = TRUE;
        }
        gboolean replay_accepted = FALSE;
        valid = found &&
            g_strcmp0(observation->observed_at, request->requested_at) == 0 &&
            g_strcmp0(observation->integrated_at, request->requested_at) == 0;
        /* CONTRACT: seules les projections mutables prouvées par le journal de
         * revue peuvent différer de l'extraction initiale immuable. */
        if (valid)
            valid = observation_review_service_validate_replay(review_service,
                request->source_evidence_identifier, observation->identifier,
                request->request_identifier, observation->type_identifier,
                observation->value_raw, observation->value_normalized,
                observation->role, observation->provenance_kind,
                observation->source_header, observation->occurrence,
                &replay_accepted, error) && replay_accepted;
    }
    valid = valid && persisted_count == json_array_get_length(artifact);
cleanup:
    g_free(matched);
    g_clear_object(&parser);
    return valid;
}

static EmlAnalysisPublicationResult *eml_analysis_load_existing(
    EmlAnalysisPersistenceService *service,
    const EmlAnalysisPersistenceRequest *request,
    const EvidenceRecord *source_record,
    gboolean *out_found,
    GError **error)
{
    ExtractionDao *extraction_dao = NULL;
    EvidenceDao *evidence_dao = NULL;
    EvidenceEntityDao *observation_dao = NULL;
    ObservationReviewService *review_service = NULL;
    ExtractionRecord *extraction = NULL;
    EvidenceRecord *derivative = NULL;
    GPtrArray *observations = NULL;
    EmlAnalysisPublicationResult *result = NULL;
    char *artifact_path = NULL;
    char *artifact_data = NULL;
    char *artifact_sha256 = NULL;
    gsize artifact_length = 0U;
    guint64 artifact_size = 0U;
    guint expected_count = 0U;
    guint persisted_count = 0U;
    *out_found = FALSE;
    extraction_dao = extraction_dao_new(service->database, error);
    if (extraction_dao == NULL) goto cleanup;
    extraction = extraction_dao_find_by_identifier(extraction_dao,
        request->request_identifier, error);
    if (extraction == NULL)
    {
        if (error == NULL || *error == NULL) goto cleanup;
        goto cleanup;
    }
    *out_found = TRUE;
    if (g_strcmp0(extraction->source_kind, "evidence") != 0 ||
        g_strcmp0(extraction->source_identifier,
            request->source_evidence_identifier) != 0 ||
        g_strcmp0(extraction->evidence_identifier,
            request->derivative_evidence_identifier) != 0 ||
        g_strcmp0(extraction->tool_identifier, EML_ANALYSIS_TOOL_ID) != 0 ||
        g_strcmp0(extraction->created_at, request->requested_at) != 0)
    {
        eml_analysis_set_error(error,
            EML_ANALYSIS_PERSISTENCE_ERROR_CONFLICT,
            "L'identité de demande existe avec une entrée incompatible.");
        goto cleanup;
    }
    evidence_dao = evidence_dao_new(service->database, error);
    derivative = evidence_dao != NULL
        ? evidence_dao_find_by_identifier(evidence_dao,
            extraction->evidence_identifier, error) : NULL;
    if (derivative == NULL ||
        !eml_analysis_relative_path_is_safe(
            evidence_record_get_relative_path(derivative)))
    {
        eml_analysis_set_error(error,
            EML_ANALYSIS_PERSISTENCE_ERROR_RECOVERY_REQUIRED,
            "La demande existe sans preuve dérivée complète.");
        goto cleanup;
    }
    artifact_path = eml_analysis_source_path(service, derivative, NULL);
    if (artifact_path == NULL ||
        !g_file_get_contents(artifact_path, &artifact_data, &artifact_length,
            NULL) ||
        !file_hash_compute_sha256(artifact_path, NULL, &artifact_sha256,
            &artifact_size, NULL) ||
        artifact_size != evidence_record_get_size_bytes(derivative) ||
        g_strcmp0(artifact_sha256,
            evidence_record_get_sha256(derivative)) != 0 ||
        !eml_analysis_verify_artifact(artifact_data, artifact_length, request,
            evidence_record_get_sha256(source_record),
            evidence_record_get_size_bytes(source_record), &expected_count))
    {
        eml_analysis_set_error(error,
            EML_ANALYSIS_PERSISTENCE_ERROR_RECOVERY_REQUIRED,
            "Le dérivé existant est absent, altéré ou incompatible.");
        goto cleanup;
    }
    observation_dao = evidence_entity_dao_new(service->database, error);
    observations = observation_dao != NULL
        ? evidence_entity_dao_list_observations(observation_dao,
            request->source_evidence_identifier, error) : NULL;
    if (observations == NULL) goto cleanup;
    review_service = observation_review_service_new(service->database, error);
    if (review_service == NULL) goto cleanup;
    if (!eml_analysis_artifact_matches_observations(review_service,
            artifact_data, artifact_length, request, observations, error))
    {
        eml_analysis_set_error(error,
            EML_ANALYSIS_PERSISTENCE_ERROR_CONFLICT,
            "Le lot d'observations de la demande a été modifié.");
        goto cleanup;
    }
    result = eml_analysis_result_new(request->request_identifier,
        request->derivative_evidence_identifier, TRUE);
    if (result == NULL) goto cleanup;
    for (guint index = 0U; index < observations->len; index++)
    {
        const EvidenceObservation *item = g_ptr_array_index(observations, index);
        if (g_strcmp0(item->extraction_identifier,
                request->request_identifier) != 0) continue;
        g_ptr_array_add(result->observation_identifiers,
            g_strdup(item->identifier));
        persisted_count++;
    }
    if (persisted_count != expected_count)
    {
        eml_analysis_set_error(error,
            EML_ANALYSIS_PERSISTENCE_ERROR_RECOVERY_REQUIRED,
            "Le lot d'observations existant est incomplet.");
        g_clear_pointer(&result, eml_analysis_publication_result_free);
    }
cleanup:
    observation_review_service_free(review_service);
    g_free(artifact_sha256);
    g_free(artifact_data);
    g_free(artifact_path);
    g_clear_pointer(&observations, g_ptr_array_unref);
    evidence_entity_dao_free(observation_dao);
    evidence_record_free(derivative);
    evidence_dao_free(evidence_dao);
    extraction_record_free(extraction);
    extraction_dao_free(extraction_dao);
    return result;
}

EmlAnalysisPersistenceService *eml_analysis_persistence_service_new(
    Database *database,
    const char *investigation_root,
    GError **error)
{
    EmlAnalysisPersistenceService *service = NULL;
    char *canonical = NULL;
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);
    if (database == NULL || investigation_root == NULL ||
        investigation_root[0] == '\0')
    {
        eml_analysis_set_error(error,
            EML_ANALYSIS_PERSISTENCE_ERROR_INVALID_ARGUMENT,
            "La connexion et la racine d'enquête sont obligatoires.");
        return NULL;
    }
    canonical = g_canonicalize_filename(investigation_root, NULL);
    if (canonical == NULL ||
        !g_file_test(canonical, G_FILE_TEST_IS_DIR) ||
        g_file_test(canonical, G_FILE_TEST_IS_SYMLINK))
    {
        g_free(canonical);
        eml_analysis_set_error(error, EML_ANALYSIS_PERSISTENCE_ERROR_SOURCE,
            "La racine contrôlée n'est pas un répertoire réel.");
        return NULL;
    }
    service = g_new0(EmlAnalysisPersistenceService, 1);
    if (service == NULL)
    {
        g_free(canonical);
        eml_analysis_set_error(error,
            EML_ANALYSIS_PERSISTENCE_ERROR_INVALID_ARGUMENT,
            "Impossible d'allouer le service d'analyse EML.");
        return NULL;
    }
    service->database = database;
    service->root = canonical;
    return service;
}

void eml_analysis_persistence_service_free(
    EmlAnalysisPersistenceService *service)
{
    if (service == NULL) return;
    g_free(service->root);
    g_free(service);
}

void eml_analysis_prepared_free(EmlAnalysisPrepared *prepared)
{
    if (prepared == NULL) return;
    g_free(prepared->request_identifier);
    g_free(prepared->source_evidence_identifier);
    g_free(prepared->derivative_evidence_identifier);
    g_free(prepared->requested_at);
    g_free(prepared->source_sha256);
    g_free(prepared->artifact_data);
    g_free(prepared->artifact_sha256);
    eml_analysis_free(prepared->analysis);
    eml_analysis_publication_result_free(prepared->existing_result);
    g_free(prepared);
}

EmlAnalysisPrepared *eml_analysis_persistence_service_prepare(
    EmlAnalysisPersistenceService *service,
    const EmlAnalysisPersistenceRequest *request,
    GCancellable *cancellable,
    GError **error)
{
    EvidenceDao *evidence_dao = NULL;
    EvidenceRecord *source_record = NULL;
    EvidenceRecord *derivative_record = NULL;
    EmlAnalysisPrepared *prepared = NULL;
    char *source_path = NULL;
    char *source_hash = NULL;
    char *derivative_path = NULL;
    guint64 source_size = 0U;
    gboolean existing_found = FALSE;
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);
    if (service == NULL || request == NULL ||
        !g_uuid_string_is_valid(request->request_identifier) ||
        !g_uuid_string_is_valid(request->source_evidence_identifier) ||
        !g_uuid_string_is_valid(request->derivative_evidence_identifier) ||
        g_strcmp0(request->request_identifier,
            request->derivative_evidence_identifier) == 0 ||
        !eml_analysis_timestamp_is_valid(request->requested_at))
    {
        eml_analysis_set_error(error,
            EML_ANALYSIS_PERSISTENCE_ERROR_INVALID_ARGUMENT,
            "La demande d'analyse EML est invalide.");
        return NULL;
    }
    if (eml_analysis_check_cancelled(cancellable, error)) return NULL;
    evidence_dao = evidence_dao_new(service->database, error);
    source_record = evidence_dao != NULL
        ? evidence_dao_find_by_identifier(evidence_dao,
            request->source_evidence_identifier, error) : NULL;
    if (source_record == NULL)
    {
        eml_analysis_set_error(error, EML_ANALYSIS_PERSISTENCE_ERROR_SOURCE,
            "La preuve source persistée est absente.");
        goto cleanup;
    }
    prepared = g_new0(EmlAnalysisPrepared, 1);
    if (prepared == NULL) goto allocation_failure;
    prepared->request_identifier = g_strdup(request->request_identifier);
    prepared->source_evidence_identifier = g_strdup(
        request->source_evidence_identifier);
    prepared->derivative_evidence_identifier = g_strdup(
        request->derivative_evidence_identifier);
    prepared->requested_at = g_strdup(request->requested_at);
    prepared->request = (EmlAnalysisPersistenceRequest) {
        .request_identifier = prepared->request_identifier,
        .source_evidence_identifier = prepared->source_evidence_identifier,
        .derivative_evidence_identifier =
            prepared->derivative_evidence_identifier,
        .requested_at = prepared->requested_at
    };
    if (prepared->request_identifier == NULL ||
        prepared->source_evidence_identifier == NULL ||
        prepared->derivative_evidence_identifier == NULL ||
        prepared->requested_at == NULL) goto allocation_failure;

    /* CONTRACT: un replay ne se fie jamais au seul hash mémorisé dans SQLite.
     * La copie contrôlée est relue avant toute décision idempotente. */
    source_path = eml_analysis_source_path(service, source_record, error);
    if (source_path == NULL || !file_hash_compute_sha256(source_path,
            cancellable, &source_hash, &source_size, error) ||
        source_size != evidence_record_get_size_bytes(source_record) ||
        g_strcmp0(source_hash, evidence_record_get_sha256(source_record)) != 0)
    {
        eml_analysis_set_error(error, EML_ANALYSIS_PERSISTENCE_ERROR_SOURCE,
            "La copie contrôlée ne correspond pas à la preuve persistée.");
        g_clear_pointer(&prepared, eml_analysis_prepared_free);
        goto cleanup;
    }
    if (source_size > EML_ANALYSIS_MAX_SOURCE_BYTES)
    {
        eml_analysis_set_error(error, EML_ANALYSIS_PERSISTENCE_ERROR_LIMIT,
            "La preuve EML dépasse la limite locale de 4 Mio.");
        g_clear_pointer(&prepared, eml_analysis_prepared_free);
        goto cleanup;
    }
    prepared->source_sha256 = g_steal_pointer(&source_hash);
    prepared->source_size = source_size;
    prepared->existing_result = eml_analysis_load_existing(service, request,
        source_record, &existing_found, error);
    if (existing_found)
    {
        if (prepared->existing_result == NULL)
            g_clear_pointer(&prepared, eml_analysis_prepared_free);
        goto cleanup;
    }
    derivative_record = evidence_dao_find_by_identifier(evidence_dao,
        request->derivative_evidence_identifier, error);
    if (derivative_record != NULL)
    {
        eml_analysis_set_error(error,
            EML_ANALYSIS_PERSISTENCE_ERROR_CONFLICT,
            "L'identité de preuve dérivée est déjà utilisée.");
        g_clear_pointer(&prepared, eml_analysis_prepared_free);
        goto cleanup;
    }
    if (error != NULL && *error != NULL)
    {
        g_clear_pointer(&prepared, eml_analysis_prepared_free);
        goto cleanup;
    }
    derivative_path = g_build_filename(service->root,
        EML_ANALYSIS_RELATIVE_DIRECTORY,
        request->derivative_evidence_identifier, NULL);
    if (derivative_path != NULL)
    {
        char *with_suffix = g_strconcat(derivative_path, ".json", NULL);
        g_free(derivative_path);
        derivative_path = with_suffix;
    }
    if (derivative_path == NULL ||
        g_file_test(derivative_path, G_FILE_TEST_EXISTS))
    {
        eml_analysis_set_error(error,
            EML_ANALYSIS_PERSISTENCE_ERROR_RECOVERY_REQUIRED,
            "Un dérivé orphelin existe pour cette demande ; récupération requise.");
        g_clear_pointer(&prepared, eml_analysis_prepared_free);
        goto cleanup;
    }
    if (eml_analysis_check_cancelled(cancellable, error))
    {
        g_clear_pointer(&prepared, eml_analysis_prepared_free);
        goto cleanup;
    }
    prepared->analysis = eml_analyzer_analyze_file(source_path, error);
    if (prepared->analysis == NULL)
    {
        if (error != NULL && *error != NULL)
        {
            (*error)->domain = EML_ANALYSIS_PERSISTENCE_ERROR;
            (*error)->code = EML_ANALYSIS_PERSISTENCE_ERROR_ANALYSIS;
        }
        g_clear_pointer(&prepared, eml_analysis_prepared_free);
        goto cleanup;
    }
    if (eml_analysis_check_cancelled(cancellable, error))
    {
        g_clear_pointer(&prepared, eml_analysis_prepared_free);
        goto cleanup;
    }
    const GPtrArray *observations = eml_analysis_get_observations(
        prepared->analysis);
    if (observations == NULL || observations->len > EML_ANALYSIS_MAX_OBSERVATIONS)
    {
        eml_analysis_set_error(error, EML_ANALYSIS_PERSISTENCE_ERROR_LIMIT,
            "L'analyse EML dépasse la limite de 256 observations.");
        g_clear_pointer(&prepared, eml_analysis_prepared_free);
        goto cleanup;
    }
    prepared->artifact_data = eml_analysis_build_artifact(prepared,
        &prepared->artifact_size, error);
    if (prepared->artifact_data == NULL)
    {
        g_clear_pointer(&prepared, eml_analysis_prepared_free);
        goto cleanup;
    }
    prepared->artifact_sha256 = g_compute_checksum_for_data(G_CHECKSUM_SHA256,
        (const guint8 *) prepared->artifact_data, prepared->artifact_size);
    if (prepared->artifact_sha256 == NULL) goto allocation_failure;
    goto cleanup;
allocation_failure:
    eml_analysis_set_error(error,
        EML_ANALYSIS_PERSISTENCE_ERROR_INVALID_ARGUMENT,
        "Impossible d'allouer la préparation de l'analyse EML.");
    g_clear_pointer(&prepared, eml_analysis_prepared_free);
cleanup:
    g_free(derivative_path);
    g_free(source_hash);
    g_free(source_path);
    evidence_record_free(derivative_record);
    evidence_record_free(source_record);
    evidence_dao_free(evidence_dao);
    return prepared;
}

static gboolean eml_analysis_remove_created_file(
    const char *path,
    GError **error)
{
    if (path == NULL || !g_file_test(path, G_FILE_TEST_EXISTS)) return TRUE;
    if (g_remove(path) == 0) return TRUE;
    if (error != NULL) g_clear_error(error);
    if (error != NULL)
        g_set_error(error, EML_ANALYSIS_PERSISTENCE_ERROR,
            EML_ANALYSIS_PERSISTENCE_ERROR_ROLLBACK,
            "Le rollback SQLite a réussi mais le dérivé créé n'a pas pu être "
            "supprimé : %s", g_strerror(errno));
    return FALSE;
}

EmlAnalysisPublicationResult *eml_analysis_persistence_service_publish(
    EmlAnalysisPersistenceService *service,
    EmlAnalysisPrepared *prepared,
    GCancellable *cancellable,
    GError **error)
{
    EvidenceDao *evidence_dao = NULL;
    EvidenceEntityDao *observation_dao = NULL;
    ExtractionDao *extraction_dao = NULL;
    EvidenceRecord *derivative_record = NULL;
    EmlAnalysisPublicationResult *result = NULL;
    char *directory = NULL;
    char *internal_name = NULL;
    char *relative_path = NULL;
    char *final_path = NULL;
    char *staging_path = NULL;
    const GPtrArray *observations = NULL;
    gboolean transaction_active = FALSE;
    gboolean final_created = FALSE;
    gboolean success = FALSE;
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);
    if (service == NULL || prepared == NULL)
    {
        eml_analysis_set_error(error,
            EML_ANALYSIS_PERSISTENCE_ERROR_INVALID_ARGUMENT,
            "La publication EML est invalide.");
        return NULL;
    }
    if (prepared->existing_result != NULL)
        return eml_analysis_result_copy(prepared->existing_result);
    if (eml_analysis_check_cancelled(cancellable, error)) return NULL;
    directory = g_build_filename(service->root,
        EML_ANALYSIS_RELATIVE_DIRECTORY, NULL);
    internal_name = g_strconcat(prepared->derivative_evidence_identifier,
        ".json", NULL);
    relative_path = g_build_filename(EML_ANALYSIS_RELATIVE_DIRECTORY,
        internal_name, NULL);
    final_path = g_build_filename(directory, internal_name, NULL);
    staging_path = g_strdup_printf("%s.stage-%s", final_path,
        prepared->request_identifier);
    if (directory == NULL || internal_name == NULL || relative_path == NULL ||
        final_path == NULL || staging_path == NULL ||
        !eml_analysis_create_private_directory(service->root,
            EML_ANALYSIS_RELATIVE_DIRECTORY, error) ||
        !eml_analysis_path_is_absent(final_path) ||
        !eml_analysis_path_is_absent(staging_path) ||
        !g_file_set_contents(staging_path, prepared->artifact_data,
            (gssize) prepared->artifact_size, error) ||
        g_chmod(staging_path, 0600) != 0 ||
        g_rename(staging_path, final_path) != 0)
    {
        eml_analysis_set_error(error,
            EML_ANALYSIS_PERSISTENCE_ERROR_ARTIFACT,
            "Impossible de publier le dérivé EML privé.");
        goto cleanup;
    }
    final_created = TRUE;
    if (eml_analysis_check_cancelled(cancellable, error)) goto cleanup;
    derivative_record = evidence_record_new(
        prepared->derivative_evidence_identifier,
        "Analyse EML locale persistée.json", internal_name, relative_path,
        "text", prepared->artifact_size, prepared->artifact_sha256,
        prepared->requested_at, prepared->requested_at,
        "Analyseur EML natif Labfy",
        "Dérivé structuré des en-têtes ; ne certifie pas l'authenticité.",
        EVIDENCE_INTEGRITY_STATUS_VALID, error);
    if (derivative_record == NULL || !database_transaction_begin(service->database))
    {
        eml_analysis_set_error(error,
            EML_ANALYSIS_PERSISTENCE_ERROR_DATABASE,
            "Impossible de démarrer la publication SQLite de l'analyse.");
        goto cleanup;
    }
    transaction_active = TRUE;
    evidence_dao = evidence_dao_new(service->database, error);
    observation_dao = evidence_entity_dao_new(service->database, error);
    extraction_dao = extraction_dao_new(service->database, error);
    if (evidence_dao == NULL || observation_dao == NULL ||
        extraction_dao == NULL ||
        !evidence_dao_insert(evidence_dao, derivative_record, error))
        goto database_failure;
#ifdef EML_ANALYSIS_PERSISTENCE_ENABLE_TEST_HOOKS
    if (eml_analysis_fail_after_derivative_insert)
    {
        eml_analysis_set_error(error,
            EML_ANALYSIS_PERSISTENCE_ERROR_DATABASE,
            "Échec de publication intermédiaire injecté par le test.");
        goto database_failure;
    }
#endif
    if (!extraction_dao_insert(extraction_dao,
            prepared->request_identifier,
            prepared->derivative_evidence_identifier, "evidence",
            prepared->source_evidence_identifier, EML_ANALYSIS_TOOL_ID,
            prepared->requested_at, error)) goto database_failure;
    observations = eml_analysis_get_observations(prepared->analysis);
    result = eml_analysis_result_new(prepared->request_identifier,
        prepared->derivative_evidence_identifier, FALSE);
    if (result == NULL) goto database_failure;
    for (guint index = 0U; observations != NULL && index < observations->len;
         index++)
    {
        const EmlObservation *item = g_ptr_array_index(
            (GPtrArray *) observations, index);
        char *observation_identifier = NULL;
        if (!evidence_entity_dao_add_extracted_observation(observation_dao,
                prepared->source_evidence_identifier,
                prepared->request_identifier, item->type_identifier,
                item->value_raw, item->value_normalized, item->role,
                item->provenance_kind, item->source_header, item->occurrence,
                prepared->requested_at, &observation_identifier, error))
        {
            g_free(observation_identifier);
            goto database_failure;
        }
        g_ptr_array_add(result->observation_identifiers,
            observation_identifier);
    }
    if (!database_transaction_commit(service->database))
    {
        eml_analysis_set_error(error,
            EML_ANALYSIS_PERSISTENCE_ERROR_DATABASE,
            "Impossible de valider la publication SQLite de l'analyse.");
        goto database_failure;
    }
    transaction_active = FALSE;
    success = TRUE;
    goto cleanup;
database_failure:
    eml_analysis_set_error(error,
        EML_ANALYSIS_PERSISTENCE_ERROR_DATABASE,
        database_error_get_message(service->database) != NULL
            ? database_error_get_message(service->database)
            : "Impossible de publier l'analyse EML.");
cleanup:
    if (!success && transaction_active)
        database_transaction_rollback(service->database);
    if (!success)
    {
        g_clear_pointer(&result, eml_analysis_publication_result_free);
        if (final_created) eml_analysis_remove_created_file(final_path, error);
        if (staging_path != NULL) g_remove(staging_path);
    }
    extraction_dao_free(extraction_dao);
    evidence_entity_dao_free(observation_dao);
    evidence_dao_free(evidence_dao);
    evidence_record_free(derivative_record);
    g_free(staging_path);
    g_free(final_path);
    g_free(relative_path);
    g_free(internal_name);
    g_free(directory);
    return result;
}

const char *eml_analysis_publication_result_get_request_identifier(
    const EmlAnalysisPublicationResult *result)
{ return result != NULL ? result->request_identifier : NULL; }
const char *eml_analysis_publication_result_get_derivative_evidence_identifier(
    const EmlAnalysisPublicationResult *result)
{ return result != NULL ? result->derivative_evidence_identifier : NULL; }
const GPtrArray *eml_analysis_publication_result_get_observation_identifiers(
    const EmlAnalysisPublicationResult *result)
{ return result != NULL ? result->observation_identifiers : NULL; }
guint eml_analysis_publication_result_get_observation_count(
    const EmlAnalysisPublicationResult *result)
{ return result != NULL ? result->observation_identifiers->len : 0U; }
gboolean eml_analysis_publication_result_was_reused(
    const EmlAnalysisPublicationResult *result)
{ return result != NULL && result->reused; }
const char *eml_analysis_publication_result_get_status(
    const EmlAnalysisPublicationResult *result)
{ return result != NULL ? result->status : NULL; }
const GPtrArray *eml_analysis_publication_result_get_warnings(
    const EmlAnalysisPublicationResult *result)
{ return result != NULL ? result->warnings : NULL; }
