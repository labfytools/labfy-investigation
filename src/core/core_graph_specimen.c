/******************************************************************************
 * @file core_graph_specimen.c
 * @brief Fixture C reproductible utilisant exclusivement les API de production.
 ******************************************************************************/

#include "core/core_graph_specimen.h"

#include "core/core_graph_projection_service.h"
#include "core/core_graph_snapshot_serializer.h"
#include "dao/entity_dao.h"
#include "dao/evidence_dao.h"
#include "dao/evidence_entity_dao.h"
#include "dao/osint_execution_dao.h"
#include "dao/relation_dao.h"
#include "database/database.h"
#include "database/error.h"
#include "database/transaction.h"
#include "models/entity_record.h"
#include "models/evidence_record.h"
#include "models/osint_execution_record.h"
#include "models/relation_record.h"

#include <glib/gstdio.h>
#include <json-glib/json-glib.h>
#include <sys/stat.h>

static const char *const specimen_timestamp = "2026-09-26T12:00:00Z";

void core_graph_specimen_paths_clear(CoreGraphSpecimenPaths *paths)
{
    if (paths == NULL) return;
    g_free(paths->database_path);
    g_free(paths->snapshot_path);
    g_free(paths->manifest_path);
    *paths = (CoreGraphSpecimenPaths) {0};
}

static void core_graph_specimen_set_error(
    GError **error,
    const char *context,
    const GError *nested)
{
    if (error == NULL || *error != NULL) return;
    g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "%s%s%s", context,
        nested != NULL ? " : " : "", nested != NULL ? nested->message : "");
}

static EntityRecord *core_graph_specimen_entity(
    const char *id,
    const char *type,
    const char *value,
    const char *label,
    GError **error)
{
    return entity_record_new(id, type, value, label, NULL, 50,
        specimen_timestamp, specimen_timestamp, ENTITY_STATUS_ACTIVE, error);
}

static EvidenceRecord *core_graph_specimen_evidence(
    const char *id,
    const char *original_name,
    const char *relative_path,
    const guint8 *data,
    gsize length,
    const char *description,
    GError **error)
{
    char *sha256 = g_compute_checksum_for_data(G_CHECKSUM_SHA256, data, length);
    char *internal_name = g_path_get_basename(relative_path);
    EvidenceRecord *record = NULL;
    if (sha256 != NULL && internal_name != NULL)
        record = evidence_record_new(id, original_name, internal_name,
            relative_path, "document", length, sha256, specimen_timestamp,
            specimen_timestamp, "Générateur C SPECIMEN J3", description,
            EVIDENCE_INTEGRITY_STATUS_VALID, error);
    g_free(internal_name);
    g_free(sha256);
    return record;
}

static gboolean core_graph_specimen_insert_entities(
    EntityDao *dao,
    GError **error)
{
    static const struct
    {
        const char *id;
        const char *type;
        const char *value;
        const char *label;
    } definitions[] = {
        {CORE_GRAPH_SPECIMEN_PERSON_A, "person", "Élodie Exemple",
            "Élodie <script>window.__LABFY_XSS__=true</script>"},
        {CORE_GRAPH_SPECIMEN_PERSON_B, "person", "Noé Témoin",
            "Noé Témoin"},
        {CORE_GRAPH_SPECIMEN_EMAIL, "email_address", "elodie@atelier.test",
            "elodie@atelier.test"},
        {CORE_GRAPH_SPECIMEN_DOMAIN, "domain_name", "atelier.test",
            "atelier.test"}
    };
    for (guint index = 0; index < G_N_ELEMENTS(definitions); index++)
    {
        EntityRecord *record = core_graph_specimen_entity(definitions[index].id,
            definitions[index].type, definitions[index].value,
            definitions[index].label, error);
        gboolean success = record != NULL && entity_dao_insert(dao, record, error);
        entity_record_free(record);
        if (!success) return FALSE;
    }
    return TRUE;
}

static gboolean core_graph_specimen_insert_relation(
    RelationDao *dao,
    GError **error)
{
    RelationRecord *record = relation_record_new(CORE_GRAPH_SPECIMEN_RELATION,
        CORE_GRAPH_SPECIMEN_EMAIL, CORE_GRAPH_SPECIMEN_DOMAIN, "uses_domain",
        "Adresse rattachée au domaine", "Relation de fixture explicite.", 50,
        specimen_timestamp, specimen_timestamp, RELATION_STATUS_ACTIVE, error);
    gboolean success = record != NULL && relation_dao_insert(dao, record, error);
    relation_record_free(record);
    return success;
}

static gboolean core_graph_specimen_insert_evidence(
    const char *output_directory,
    EvidenceDao *evidence_dao,
    EvidenceEntityDao *association_dao,
    char **out_promoted_observation,
    char **out_unpromoted_observation,
    GError **error)
{
    static const guint8 content_a[] =
        "SPECIMEN A\nAdresse observée: elodie@atelier.test\n";
    static const guint8 content_b[] =
        "SPECIMEN B\nTexte hostile: <img src=x onerror=alert(1)>\n";
    const char *relative_a = "SPECIMEN/preuve-échange.txt";
    const char *relative_b = "SPECIMEN/preuve-html.txt";
    char *directory = g_build_filename(output_directory, "SPECIMEN", NULL);
    char *path_a = g_build_filename(output_directory, relative_a, NULL);
    char *path_b = g_build_filename(output_directory, relative_b, NULL);
    EvidenceRecord *record_a = NULL;
    EvidenceRecord *record_b = NULL;
    gboolean success = directory != NULL && path_a != NULL && path_b != NULL &&
        g_mkdir_with_parents(directory, 0700) == 0 &&
        g_file_set_contents(path_a, (const char *) content_a,
            sizeof(content_a) - 1U, error) &&
        g_file_set_contents(path_b, (const char *) content_b,
            sizeof(content_b) - 1U, error);
    if (!success) goto cleanup;
    record_a = core_graph_specimen_evidence(CORE_GRAPH_SPECIMEN_EVIDENCE_A,
        "Échange SPECIMEN.txt", relative_a, content_a, sizeof(content_a) - 1U,
        "Extrait Unicode contrôlé", error);
    record_b = core_graph_specimen_evidence(CORE_GRAPH_SPECIMEN_EVIDENCE_B,
        "HTML inerte SPECIMEN.txt", relative_b, content_b,
        sizeof(content_b) - 1U, "<b>Texte affiché, jamais interprété</b>", error);
    success = record_a != NULL && record_b != NULL &&
        evidence_dao_insert(evidence_dao, record_a, error) &&
        evidence_dao_insert(evidence_dao, record_b, error) &&
        evidence_entity_dao_add_source(association_dao,
            CORE_GRAPH_SPECIMEN_EVIDENCE_A, CORE_GRAPH_SPECIMEN_PERSON_A,
            "manual", NULL, specimen_timestamp, error) &&
        evidence_entity_dao_add_source(association_dao,
            CORE_GRAPH_SPECIMEN_EVIDENCE_B, CORE_GRAPH_SPECIMEN_PERSON_A,
            "manual", NULL, specimen_timestamp, error) &&
        evidence_entity_dao_add_observation(association_dao,
            CORE_GRAPH_SPECIMEN_EVIDENCE_A, "email_address",
            "Elodie@Atelier.test", "elodie@atelier.test", "from", "header",
            "from", 1U, "confirmed", specimen_timestamp,
            out_promoted_observation, error) &&
        evidence_entity_dao_promote_observation(association_dao,
            *out_promoted_observation, CORE_GRAPH_SPECIMEN_EMAIL,
            specimen_timestamp, "reused", error) &&
        evidence_entity_dao_add_observation(association_dao,
            CORE_GRAPH_SPECIMEN_EVIDENCE_B, "pseudonym",
            "<svg onload=window.__LABFY_XSS__=true>", "alias_specimen", "body",
            "text", "body", 1U, "proposed", specimen_timestamp,
            out_unpromoted_observation, error);

cleanup:
    evidence_record_free(record_b);
    evidence_record_free(record_a);
    g_free(path_b); g_free(path_a); g_free(directory);
    return success;
}

static gboolean core_graph_specimen_insert_execution(
    OsintExecutionDao *dao,
    GError **error)
{
    static const guint8 stdout_data[] =
        "SPECIMEN raw: atelier.test -> elodie@atelier.test\n";
    GBytes *stdout_raw = g_bytes_new_static(stdout_data, sizeof(stdout_data) - 1U);
    GBytes *stderr_raw = g_bytes_new_static("", 0U);
    char *sha256 = g_compute_checksum_for_data(G_CHECKSUM_SHA256, stdout_data,
        sizeof(stdout_data) - 1U);
    OsintExecutionRecord *record = NULL;
    gboolean success = FALSE;
    if (stdout_raw != NULL && stderr_raw != NULL && sha256 != NULL)
        record = osint_execution_record_new(CORE_GRAPH_SPECIMEN_EXECUTION,
            "specimen.local", "SPECIMEN-1", "historical-demo",
            CORE_GRAPH_SPECIMEN_DOMAIN, "entity", "atelier.test",
            "[\"--synthetic\",\"atelier.test\"]", specimen_timestamp,
            "2026-09-26T12:00:01Z", TRUE, 0, "completed", stdout_raw,
            stderr_raw, sha256, error);
    success = record != NULL && osint_execution_dao_insert(dao, record, error) &&
        osint_execution_dao_link_entity(dao, CORE_GRAPH_SPECIMEN_EXECUTION,
            CORE_GRAPH_SPECIMEN_EMAIL, "reused", error);
    osint_execution_record_free(record);
    g_free(sha256);
    g_clear_pointer(&stderr_raw, g_bytes_unref);
    g_clear_pointer(&stdout_raw, g_bytes_unref);
    return success;
}

static gboolean core_graph_specimen_populate(
    const char *output_directory,
    const char *database_path,
    char **out_promoted_observation,
    char **out_unpromoted_observation,
    GError **error)
{
    Database *database = database_open(database_path);
    EntityDao *entity_dao = NULL;
    RelationDao *relation_dao = NULL;
    EvidenceDao *evidence_dao = NULL;
    EvidenceEntityDao *association_dao = NULL;
    OsintExecutionDao *execution_dao = NULL;
    gboolean transaction_active = FALSE;
    gboolean success = FALSE;
    if (database == NULL)
    {
        core_graph_specimen_set_error(error,
            "Impossible de rouvrir la fixture pour écriture", NULL);
        return FALSE;
    }
    if (!database_transaction_begin(database)) goto cleanup;
    transaction_active = TRUE;
    entity_dao = entity_dao_new(database, error);
    relation_dao = relation_dao_new(database, error);
    evidence_dao = evidence_dao_new(database, error);
    association_dao = evidence_entity_dao_new(database, error);
    execution_dao = osint_execution_dao_new(database, error);
    success = entity_dao != NULL && relation_dao != NULL &&
        evidence_dao != NULL && association_dao != NULL &&
        execution_dao != NULL &&
        core_graph_specimen_insert_entities(entity_dao, error) &&
        core_graph_specimen_insert_relation(relation_dao, error) &&
        core_graph_specimen_insert_evidence(output_directory, evidence_dao,
            association_dao, out_promoted_observation,
            out_unpromoted_observation, error) &&
        core_graph_specimen_insert_execution(execution_dao, error) &&
        database_transaction_commit(database);
    if (success) transaction_active = FALSE;

cleanup:
    if (!success && error != NULL && *error == NULL)
        core_graph_specimen_set_error(error,
            database_error_get_message(database) != NULL
                ? database_error_get_message(database)
                : "Impossible de peupler la fixture C", NULL);
    if (transaction_active) database_transaction_rollback(database);
    osint_execution_dao_free(execution_dao);
    evidence_entity_dao_free(association_dao);
    evidence_dao_free(evidence_dao);
    relation_dao_free(relation_dao);
    entity_dao_free(entity_dao);
    database_close(database);
    return success;
}

static gboolean core_graph_specimen_write_manifest(
    const CoreGraphSpecimenPaths *paths,
    const char *promoted_observation,
    const char *unpromoted_observation,
    const CoreGraphSnapshot *snapshot,
    GError **error)
{
    JsonBuilder *builder = json_builder_new();
    JsonGenerator *generator = json_generator_new();
    JsonNode *root = NULL;
    char *data = NULL;
    gboolean success = FALSE;
    if (builder == NULL || generator == NULL) goto cleanup;
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "contract");
    json_builder_add_string_value(builder, "labfy.core_graph.specimen_manifest.v1");
    json_builder_set_member_name(builder, "synthetic");
    json_builder_add_boolean_value(builder, TRUE);
    json_builder_set_member_name(builder, "database_file");
    json_builder_add_string_value(builder, "Enquete.sqlite");
    json_builder_set_member_name(builder, "snapshot_file");
    json_builder_add_string_value(builder, "core-snapshot.json");
    json_builder_set_member_name(builder, "schema_version");
    json_builder_add_int_value(builder, 20);
    json_builder_set_member_name(builder, "node_count");
    json_builder_add_int_value(builder,
        core_graph_snapshot_get_nodes(snapshot)->len);
    json_builder_set_member_name(builder, "edge_count");
    json_builder_add_int_value(builder,
        core_graph_snapshot_get_edges(snapshot)->len);
    json_builder_set_member_name(builder, "promoted_observation_id");
    json_builder_add_string_value(builder, promoted_observation);
    json_builder_set_member_name(builder, "unpromoted_observation_id");
    json_builder_add_string_value(builder, unpromoted_observation);
    json_builder_set_member_name(builder, "note");
    json_builder_add_string_value(builder,
        "Manifeste technique de génération ; ne certifie aucune donnée.");
    json_builder_end_object(builder);
    root = json_builder_get_root(builder);
    json_generator_set_root(generator, root);
    data = json_generator_to_data(generator, NULL);
    success = data != NULL &&
        g_file_set_contents(paths->manifest_path, data, -1, error) &&
        g_chmod(paths->manifest_path, 0600) == 0;

cleanup:
    if (!success && error != NULL && *error == NULL)
        core_graph_specimen_set_error(error,
            "Impossible d'écrire le manifeste technique", NULL);
    g_free(data);
    if (root != NULL) json_node_unref(root);
    g_clear_object(&generator);
    g_clear_object(&builder);
    return success;
}

gboolean core_graph_specimen_generate(
    const char *output_directory,
    CoreGraphSpecimenPaths *out_paths,
    GError **error)
{
    CoreGraphSpecimenPaths paths = {0};
    Database *reader = NULL;
    CoreGraphSnapshot *snapshot = NULL;
    char *promoted_observation = NULL;
    char *unpromoted_observation = NULL;
    CoreGraphLimits limits = {
        .max_nodes = 500U,
        .max_edges = 1000U,
        .max_serialized_bytes = 1024U * 1024U
    };
    gboolean success = FALSE;
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    if (out_paths != NULL) *out_paths = (CoreGraphSpecimenPaths) {0};
    if (output_directory == NULL || output_directory[0] == '\0' ||
        out_paths == NULL || !g_file_test(output_directory, G_FILE_TEST_IS_DIR))
    {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
            "Un répertoire privé existant est obligatoire pour la fixture.");
        return FALSE;
    }
    paths.database_path = g_build_filename(output_directory,
        "Enquete.sqlite", NULL);
    paths.snapshot_path = g_build_filename(output_directory,
        "core-snapshot.json", NULL);
    paths.manifest_path = g_build_filename(output_directory,
        "generation-manifest.json", NULL);
    if (paths.database_path == NULL || paths.snapshot_path == NULL ||
        paths.manifest_path == NULL || g_file_test(paths.database_path,
            G_FILE_TEST_EXISTS))
    {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_EXISTS,
            "La destination de fixture n'est pas neuve.");
        goto cleanup;
    }
    if (!database_initialize(paths.database_path,
            "SPECIMEN J3 — graphe cœur", output_directory) ||
        !core_graph_specimen_populate(output_directory, paths.database_path,
            &promoted_observation, &unpromoted_observation, error))
        goto cleanup;
    /* CONTRACT: tous les writers sont fermés avant l'ouverture read-only. */
    reader = database_open_read_only(paths.database_path, error);
    if (reader == NULL) goto cleanup;
    snapshot = core_graph_projection_service_collect(reader, limits, error);
    database_close(reader); reader = NULL;
    if (snapshot == NULL || !core_graph_snapshot_write_atomic(snapshot, TRUE,
            paths.snapshot_path, error) ||
        !core_graph_specimen_write_manifest(&paths, promoted_observation,
            unpromoted_observation, snapshot, error)) goto cleanup;
    *out_paths = paths;
    paths = (CoreGraphSpecimenPaths) {0};
    success = TRUE;

cleanup:
    database_close(reader);
    core_graph_snapshot_free(snapshot);
    g_free(unpromoted_observation);
    g_free(promoted_observation);
    core_graph_specimen_paths_clear(&paths);
    return success;
}
