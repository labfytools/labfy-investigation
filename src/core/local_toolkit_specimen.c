/******************************************************************************
 * @file local_toolkit_specimen.c
 * @brief Fixture SPECIMEN J4 traversant registre, adapters et V20.
 ******************************************************************************/
#include "core/local_toolkit_specimen.h"

#include "core/core_graph_projection_service.h"
#include "core/core_graph_snapshot_serializer.h"
#include "core/eml_graph_specimen.h"
#include "core/evidence_importer.h"
#include "core/exiftool_persistence_service.h"
#include "core/local_capability_registry.h"
#include "core/local_tool_runner.h"
#include "database/database.h"
#include "models/evidence_record.h"

#include <glib/gstdio.h>
#include <json-glib/json-glib.h>

#define J4_IMAGE_ID "71000000-0000-4000-8000-000000000041"
#define J4_REQUEST_ID "71000000-0000-4000-8000-000000000042"
#define J4_DERIVATIVE_ID "71000000-0000-4000-8000-000000000043"

static const char *const j4_timestamp = "2026-09-26T18:00:00Z";
static const char *const j4_png_base64 =
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=";

static gboolean local_toolkit_write_manifest(const char *directory,
    const LocalCapabilityStatus *status, guint metadata_count, GError **error)
{
    char *path = g_build_filename(directory, "generation-manifest.json", NULL);
    JsonBuilder *builder = json_builder_new();
    JsonGenerator *generator = json_generator_new();
    json_builder_begin_object(builder);
#define ADD_STRING(name, value) json_builder_set_member_name(builder, name); \
    json_builder_add_string_value(builder, value)
    ADD_STRING("contract", "labfy.local_toolkit.specimen_manifest.v1");
    json_builder_set_member_name(builder, "synthetic");
    json_builder_add_boolean_value(builder, TRUE);
    ADD_STRING("snapshot_file", "core-snapshot.json");
    ADD_STRING("snapshot_contract", "labfy.web_graph.snapshot.v3");
    ADD_STRING("eml_capability", LOCAL_CAPABILITY_EML_HEADERS);
    ADD_STRING("exiftool_capability", LOCAL_CAPABILITY_EXIF_METADATA);
    ADD_STRING("exiftool_version", status->tool_version);
    ADD_STRING("exiftool_request_id", J4_REQUEST_ID);
    ADD_STRING("exiftool_derivative_id", J4_DERIVATIVE_ID);
    json_builder_set_member_name(builder, "metadata_count");
    json_builder_add_int_value(builder, metadata_count);
    json_builder_set_member_name(builder, "client_execution");
    json_builder_add_boolean_value(builder, FALSE);
    ADD_STRING("execution_note", "Exécution par le lanceur local");
#undef ADD_STRING
    json_builder_end_object(builder);
    JsonNode *root = json_builder_get_root(builder);
    json_generator_set_root(generator, root);
    char *data = json_generator_to_data(generator, NULL);
    gboolean success = data != NULL && g_file_set_contents(path, data, -1, error) &&
        g_chmod(path, 0600) == 0;
    g_free(data); json_node_unref(root); g_object_unref(generator);
    g_object_unref(builder); g_free(path); return success;
}

static EvidenceRecord *local_toolkit_import_image(Database *database,
    const char *root, const char *path, GError **error)
{
    char *destination = g_build_filename(root,
        "01_Preuves_Originales", "Images", NULL);
    EvidenceImporter *importer = evidence_importer_new(database, error);
    EvidenceImportRequest request = {
        .source_path = path, .destination_directory = destination,
        .relative_directory = "01_Preuves_Originales/Images",
        .type_identifier = "photo", .collected_at = j4_timestamp,
        .source = "Générateur C SPECIMEN J4",
        .description = "Image fictive contenant des métadonnées fictives."
    };
    EvidenceRecord *record = destination != NULL &&
        g_mkdir_with_parents(destination, 0700) == 0 && importer != NULL
        ? evidence_importer_import(importer, &request, NULL, error) : NULL;
    evidence_importer_free(importer); g_free(destination); return record;
}

gboolean local_toolkit_specimen_generate(
    const char *output_directory, GError **error)
{
    EmlGraphSpecimenPaths eml_paths = {0};
    LocalCapabilityRegistry *registry = NULL;
    Database *database = NULL;
    EvidenceRecord *image = NULL;
    ExiftoolPersistenceService *service = NULL;
    ExiftoolPublicationResult *publication = NULL;
    CoreGraphSnapshot *snapshot = NULL;
    char *fixture_directory = NULL, *fixture_path = NULL;
    gboolean success = FALSE;
    if (!eml_graph_specimen_generate(output_directory,
            EML_GRAPH_SPECIMEN_DEFAULT, &eml_paths, error)) goto cleanup;
    registry = local_capability_registry_new(error);
    const LocalCapabilityStatus *status = registry != NULL
        ? local_capability_registry_lookup(registry,
            LOCAL_CAPABILITY_EXIF_METADATA) : NULL;
    if (status == NULL || status->availability != LOCAL_CAPABILITY_READY)
        goto cleanup;
    fixture_directory = g_build_filename(output_directory, "SPECIMEN_INPUT", NULL);
    fixture_path = g_build_filename(fixture_directory, "image-specimen.png", NULL);
    gsize png_size = 0U; guchar *png = g_base64_decode(j4_png_base64, &png_size);
    if (png == NULL || !g_file_set_contents(fixture_path, (char *) png,
            (gssize) png_size, error)) { g_free(png); goto cleanup; }
    g_free(png);
    /* WHY: la fixture reçoit ses métadonnées avant import. L'adapter de
     * production reste strictement en lecture seule et n'accepte aucune option. */
    const char *write_argv[] = { status->executable, "-config", "",
        "-overwrite_original", "-XMP-dc:Creator=Alice SPECIMEN",
        "-XMP-dc:Description=<script>inerte</script> — J4",
        "--", fixture_path, NULL };
    LocalToolRunnerResult *write_result = NULL;
    if (!local_tool_runner_run(status->executable, write_argv,
            fixture_directory, &status->descriptor->limits, NULL,
            &write_result, error) ||
        write_result->state != LOCAL_TOOL_RUNNER_EXITED) {
        local_tool_runner_result_free(write_result); goto cleanup;
    }
    local_tool_runner_result_free(write_result);
    database = database_open(eml_paths.database_path);
    image = database != NULL ? local_toolkit_import_image(database,
        output_directory, fixture_path, error) : NULL;
    if (image == NULL) goto cleanup;
    ExiftoolPersistenceRequest request = {
        .request_identifier = J4_REQUEST_ID,
        .source_evidence_identifier = evidence_record_get_identifier(image),
        .derivative_evidence_identifier = J4_DERIVATIVE_ID,
        .requested_at = j4_timestamp
    };
    service = exiftool_persistence_service_new(database, output_directory,
        registry, error);
    publication = service != NULL ? exiftool_persistence_service_execute(
        service, &request, NULL, error) : NULL;
    if (publication == NULL || publication->metadata_count == 0U ||
        publication->reused) goto cleanup;
    ExiftoolPublicationResult *replay = exiftool_persistence_service_execute(
        service, &request, NULL, error);
    gboolean replay_ok = replay != NULL && replay->reused;
    exiftool_publication_result_free(replay);
    if (!replay_ok) goto cleanup;
    database_close(database); database = NULL;
    database = database_open_read_only(eml_paths.database_path, error);
    CoreGraphLimits limits = {500U, 1000U, 1024U * 1024U};
    snapshot = database != NULL ? core_graph_projection_service_collect_with_registry(
        database, limits, registry, error) : NULL;
    database_close(database); database = NULL;
    if (snapshot == NULL || !core_graph_snapshot_write_atomic(snapshot, TRUE,
            eml_paths.snapshot_path, error) ||
        !local_toolkit_write_manifest(output_directory, status,
            publication->metadata_count, error)) goto cleanup;
    success = TRUE;
cleanup:
    core_graph_snapshot_free(snapshot);
    exiftool_publication_result_free(publication);
    exiftool_persistence_service_free(service);
    evidence_record_free(image); database_close(database);
    local_capability_registry_free(registry);
    g_free(fixture_path); g_free(fixture_directory);
    eml_graph_specimen_paths_clear(&eml_paths);
    return success;
}
