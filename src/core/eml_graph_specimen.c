/******************************************************************************
 * @file eml_graph_specimen.c
 * @brief Fixture qui exécute réellement l'analyseur EML natif.
 ******************************************************************************/

#include "core/eml_graph_specimen.h"

#include "core/core_graph_projection_service.h"
#include "core/core_graph_snapshot_serializer.h"
#include "core/eml_analysis_persistence_service.h"
#include "core/evidence_importer.h"
#include "core/local_capability_registry.h"
#include "dao/evidence_entity_dao.h"
#include "database/database.h"
#include "database/statement.h"
#include "models/evidence_record.h"

#include <glib/gstdio.h>
#include <json-glib/json-glib.h>

static const char *const eml_graph_timestamp = "2026-09-26T14:00:00Z";
static const char *const eml_graph_reanalysis_timestamp =
    "2026-09-26T14:01:00Z";

static const char *const eml_graph_default_content =
    "From: Élodie Exemple <Elodie@Atelier.test>\r\n"
    "To: Noé Témoin <noe@destination.test>\r\n"
    "Reply-To: support@atelier.test\r\n"
    "Received: from relais.atelier.test ([192.0.2.42]) "
        "by mx.destination.test with ESMTP;\r\n"
    "Received: from ipv6.atelier.test ([2001:db8::42]) "
        "by mx.destination.test with ESMTP;\r\n"
    "Message-ID: <specimen-émail@atelier.test>\r\n"
    "Date: Sat, 26 Sep 2026 16:00:00 +0200\r\n"
    "Subject: <script>window.__LABFY_EML_XSS__=true</script> — démonstration\r\n"
    "MIME-Version: 1.0\r\nContent-Type: text/plain; charset=UTF-8\r\n"
    "\r\nCorps SPECIMEN sans ressource distante.\r\n";

static const char *const eml_graph_alternate_content =
    "From: Maëlle Variante <maelle@variant.test>\r\n"
    "To: Archive SPECIMEN <archive@destination.test>\r\n"
    "Received: from relais.variant.test ([198.51.100.77]) "
        "by mx.destination.test with ESMTP;\r\n"
    "Message-ID: <variant@variant.test>\r\n"
    "Date: Sat, 26 Sep 2026 17:00:00 +0200\r\n"
    "Subject: <img src=x onerror=window.__LABFY_EML_XSS__=true>\r\n"
    "\r\nNouvelle fixture indépendante.\r\n";

void eml_graph_specimen_paths_clear(EmlGraphSpecimenPaths *paths)
{
    if (paths == NULL) return;
    g_free(paths->database_path);
    g_free(paths->snapshot_path);
    g_free(paths->manifest_path);
    *paths = (EmlGraphSpecimenPaths) {0};
}

static void eml_graph_set_error(
    GError **error,
    const char *message)
{
    if (error != NULL && *error == NULL)
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED, message);
}

static gboolean eml_graph_query_ok(Database *database, const char *sql)
{
    DatabaseStatement *statement = database_statement_prepare(database, sql);
    char *value = NULL;
    gboolean success = statement != NULL &&
        database_statement_step(statement) == DATABASE_STATEMENT_STEP_ROW &&
        database_statement_column_text(statement, 0, &value) &&
        g_strcmp0(value, "ok") == 0;
    g_free(value);
    database_statement_finalize(statement);
    return success;
}

static gboolean eml_graph_foreign_keys_ok(Database *database)
{
    DatabaseStatement *statement = database_statement_prepare(database,
        "PRAGMA foreign_key_check;");
    gboolean success = statement != NULL &&
        database_statement_step(statement) == DATABASE_STATEMENT_STEP_DONE;
    database_statement_finalize(statement);
    return success;
}

static gboolean eml_graph_no_observation(
    Database *database,
    const char *evidence_identifier,
    GError **error)
{
    EvidenceEntityDao *dao = evidence_entity_dao_new(database, error);
    GPtrArray *items = dao != NULL
        ? evidence_entity_dao_list_observations(dao, evidence_identifier, error)
        : NULL;
    gboolean empty = items != NULL && items->len == 0U;
    if (items != NULL) g_ptr_array_unref(items);
    evidence_entity_dao_free(dao);
    if (!empty)
        eml_graph_set_error(error,
            "La preuve importée contient déjà des observations inattendues.");
    return empty;
}

static EvidenceRecord *eml_graph_import(
    Database *database,
    const char *root,
    const char *source_path,
    GError **error)
{
    char *destination = g_build_filename(root,
        "01_Preuves_Originales", "Emails", NULL);
    EvidenceImporter *importer = NULL;
    EvidenceRecord *record = NULL;
    EvidenceImportRequest request = {
        .source_path = source_path,
        .destination_directory = destination,
        .relative_directory = "01_Preuves_Originales/Emails",
        .type_identifier = "email",
        .collected_at = eml_graph_timestamp,
        .source = "Générateur C SPECIMEN J3 EML",
        .description = "Message entièrement fictif pour analyse locale."
    };
    if (destination == NULL || g_mkdir_with_parents(destination, 0700) != 0)
        goto cleanup;
    importer = evidence_importer_new(database, error);
    if (importer != NULL)
        record = evidence_importer_import(importer, &request, NULL, error);
cleanup:
    evidence_importer_free(importer);
    g_free(destination);
    return record;
}

static EmlAnalysisPublicationResult *eml_graph_analyze(
    Database *database,
    const char *root,
    const EmlAnalysisPersistenceRequest *request,
    GError **error)
{
    EmlAnalysisPersistenceService *service =
        eml_analysis_persistence_service_new(database, root, error);
    EmlAnalysisPrepared *prepared = service != NULL
        ? eml_analysis_persistence_service_prepare(service, request, NULL, error)
        : NULL;
    EmlAnalysisPublicationResult *result = prepared != NULL
        ? eml_analysis_persistence_service_publish(service, prepared, NULL, error)
        : NULL;
    eml_analysis_prepared_free(prepared);
    eml_analysis_persistence_service_free(service);
    return result;
}

static gboolean eml_graph_write_manifest(
    const EmlGraphSpecimenPaths *paths,
    const char *source_identifier,
    const char *source_sha256,
    guint per_analysis,
    EmlGraphSpecimenVariant variant,
    GError **error)
{
    JsonBuilder *builder = json_builder_new();
    JsonGenerator *generator = json_generator_new();
    JsonNode *root = NULL;
    char *data = NULL;
    gboolean success = builder != NULL && generator != NULL;
    if (!success) goto cleanup;
    json_builder_begin_object(builder);
#define ADD_STRING(name, value) \
    json_builder_set_member_name(builder, name); \
    json_builder_add_string_value(builder, value)
    ADD_STRING("contract", "labfy.eml_graph.specimen_manifest.v1");
    json_builder_set_member_name(builder, "synthetic");
    json_builder_add_boolean_value(builder, TRUE);
    ADD_STRING("scenario", variant == EML_GRAPH_SPECIMEN_ALTERNATE
        ? "eml-analysis-alternate" : "eml-analysis-default");
    ADD_STRING("database_file", "Enquete.sqlite");
    ADD_STRING("snapshot_file", "core-snapshot.json");
    ADD_STRING("snapshot_contract", "labfy.web_graph.snapshot.v3");
    ADD_STRING("source_evidence_id", source_identifier);
    ADD_STRING("source_sha256", source_sha256);
    ADD_STRING("primary_request_id", EML_GRAPH_REQUEST_PRIMARY);
    ADD_STRING("primary_derivative_id", EML_GRAPH_DERIVATIVE_PRIMARY);
    ADD_STRING("reanalysis_request_id", EML_GRAPH_REQUEST_REANALYSIS);
    ADD_STRING("reanalysis_derivative_id", EML_GRAPH_DERIVATIVE_REANALYSIS);
#undef ADD_STRING
    json_builder_set_member_name(builder, "observations_per_analysis");
    json_builder_add_int_value(builder, per_analysis);
    json_builder_set_member_name(builder, "total_observations");
    json_builder_add_int_value(builder, per_analysis * 2U);
    json_builder_set_member_name(builder, "idempotent_replay_reused");
    json_builder_add_boolean_value(builder, TRUE);
    json_builder_set_member_name(builder, "note");
    json_builder_add_string_value(builder,
        "Manifeste technique SPECIMEN ; ne certifie aucune donnée.");
    json_builder_end_object(builder);
    root = json_builder_get_root(builder);
    json_generator_set_root(generator, root);
    data = json_generator_to_data(generator, NULL);
    success = data != NULL &&
        g_file_set_contents(paths->manifest_path, data, -1, error) &&
        g_chmod(paths->manifest_path, 0600) == 0;
cleanup:
    if (!success) eml_graph_set_error(error,
        "Impossible d'écrire le manifeste EML synthétique.");
    g_free(data);
    if (root != NULL) json_node_unref(root);
    g_clear_object(&generator);
    g_clear_object(&builder);
    return success;
}

gboolean eml_graph_specimen_generate(
    const char *output_directory,
    EmlGraphSpecimenVariant variant,
    EmlGraphSpecimenPaths *out_paths,
    GError **error)
{
    EmlGraphSpecimenPaths paths = {0};
    Database *database = NULL;
    EvidenceRecord *source_record = NULL;
    EmlAnalysisPublicationResult *primary = NULL;
    EmlAnalysisPublicationResult *replay = NULL;
    EmlAnalysisPublicationResult *reanalysis = NULL;
    LocalCapabilityRegistry *registry = NULL;
    CoreGraphSnapshot *snapshot = NULL;
    char *input_directory = NULL;
    char *input_path = NULL;
    char *source_identifier = NULL;
    char *source_sha256 = NULL;
    guint observations = 0U;
    gboolean success = FALSE;
    const char *content = variant == EML_GRAPH_SPECIMEN_ALTERNATE
        ? eml_graph_alternate_content : eml_graph_default_content;
    EmlAnalysisPersistenceRequest primary_request = {
        .request_identifier = EML_GRAPH_REQUEST_PRIMARY,
        .derivative_evidence_identifier = EML_GRAPH_DERIVATIVE_PRIMARY,
        .requested_at = eml_graph_timestamp
    };
    EmlAnalysisPersistenceRequest reanalysis_request = {
        .request_identifier = EML_GRAPH_REQUEST_REANALYSIS,
        .derivative_evidence_identifier = EML_GRAPH_DERIVATIVE_REANALYSIS,
        .requested_at = eml_graph_reanalysis_timestamp
    };
    CoreGraphLimits limits = {500U, 1000U, 1024U * 1024U};
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    if (out_paths != NULL) *out_paths = (EmlGraphSpecimenPaths) {0};
    if (output_directory == NULL || out_paths == NULL ||
        !g_file_test(output_directory, G_FILE_TEST_IS_DIR))
    {
        eml_graph_set_error(error,
            "Un répertoire privé existant est obligatoire.");
        return FALSE;
    }
    paths.database_path = g_build_filename(output_directory,
        "Enquete.sqlite", NULL);
    paths.snapshot_path = g_build_filename(output_directory,
        "core-snapshot.json", NULL);
    paths.manifest_path = g_build_filename(output_directory,
        "generation-manifest.json", NULL);
    input_directory = g_build_filename(output_directory, "SPECIMEN_INPUT", NULL);
    /* WHY: le libellé non fiable doit traverser C, JSON et DOM sans jamais
     * devenir du HTML exécutable dans la surface Web. */
    input_path = g_build_filename(input_directory,
        "message-<img onerror=window.__LABFY_EML_XSS__=true>.eml", NULL);
    if (paths.database_path == NULL || paths.snapshot_path == NULL ||
        paths.manifest_path == NULL || input_directory == NULL ||
        input_path == NULL || g_file_test(paths.database_path,
            G_FILE_TEST_EXISTS) ||
        g_mkdir_with_parents(input_directory, 0700) != 0 ||
        !g_file_set_contents(input_path, content, -1, error) ||
        !database_initialize(paths.database_path,
            "SPECIMEN J3 — analyse EML", output_directory)) goto cleanup;
    database = database_open(paths.database_path);
    if (database == NULL) goto cleanup;
    source_record = eml_graph_import(database, output_directory, input_path, error);
    if (source_record == NULL || !eml_graph_no_observation(database,
            evidence_record_get_identifier(source_record), error)) goto cleanup;
    source_identifier = g_strdup(evidence_record_get_identifier(source_record));
    source_sha256 = g_strdup(evidence_record_get_sha256(source_record));
    primary_request.source_evidence_identifier = source_identifier;
    reanalysis_request.source_evidence_identifier = source_identifier;
    evidence_record_free(source_record); source_record = NULL;
    database_close(database); database = NULL;

    database = database_open(paths.database_path);
    registry = local_capability_registry_new(error);
    const LocalCapabilityStatus *eml_capability = registry != NULL
        ? local_capability_registry_lookup(registry,
            LOCAL_CAPABILITY_EML_HEADERS) : NULL;
    char *applicability_reason = NULL;
    gboolean applicable = eml_capability != NULL &&
        eml_capability->availability == LOCAL_CAPABILITY_READY &&
        local_capability_status_applies(eml_capability, "message/rfc822",
            input_path,
            &applicability_reason);
    g_free(applicability_reason);
    if (!applicable) goto cleanup;
    primary = eml_graph_analyze(database, output_directory,
        &primary_request, error);
    if (primary == NULL || eml_analysis_publication_result_was_reused(primary))
        goto cleanup;
    observations = eml_analysis_publication_result_get_observation_count(primary);
    database_close(database); database = NULL;

    database = database_open(paths.database_path);
    replay = eml_graph_analyze(database, output_directory,
        &primary_request, error);
    if (replay == NULL ||
        !eml_analysis_publication_result_was_reused(replay) ||
        eml_analysis_publication_result_get_observation_count(replay) !=
            observations) goto cleanup;
    reanalysis = eml_graph_analyze(database, output_directory,
        &reanalysis_request, error);
    if (reanalysis == NULL ||
        eml_analysis_publication_result_was_reused(reanalysis) ||
        eml_analysis_publication_result_get_observation_count(reanalysis) !=
        observations ||
        !eml_graph_query_ok(database, "PRAGMA integrity_check;") ||
        !eml_graph_foreign_keys_ok(database))
        goto cleanup;
    database_close(database); database = NULL;

    database = database_open_read_only(paths.database_path, error);
    snapshot = database != NULL
        ? core_graph_projection_service_collect(database, limits, error) : NULL;
    database_close(database); database = NULL;
    if (snapshot == NULL || observations == 0U ||
        !core_graph_snapshot_write_atomic(snapshot, TRUE,
            paths.snapshot_path, error) ||
        !eml_graph_write_manifest(&paths, source_identifier, source_sha256,
            observations, variant, error)) goto cleanup;
    *out_paths = paths;
    paths = (EmlGraphSpecimenPaths) {0};
    success = TRUE;
cleanup:
    if (!success) eml_graph_set_error(error,
        "La génération du scénario EML a échoué.");
    core_graph_snapshot_free(snapshot);
    local_capability_registry_free(registry);
    eml_analysis_publication_result_free(reanalysis);
    eml_analysis_publication_result_free(replay);
    eml_analysis_publication_result_free(primary);
    evidence_record_free(source_record);
    database_close(database);
    g_free(source_sha256);
    g_free(source_identifier);
    g_free(input_path);
    g_free(input_directory);
    eml_graph_specimen_paths_clear(&paths);
    return success;
}
