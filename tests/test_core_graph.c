/******************************************************************************
 * @file test_core_graph.c
 * @brief Contrats J3 lecture seule, fixture C, projection et sérialisation.
 ******************************************************************************/

#include "core/core_graph_projection_service.h"
#include "core/core_graph_snapshot_serializer.h"
#include "core/core_graph_specimen.h"
#include "dao/evidence_dao.h"
#include "database/database.h"
#include "database/statement.h"
#include "database/transaction.h"
#include "models/core_graph_snapshot.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <json-glib/json-glib.h>
#include <string.h>

static const CoreGraphLimits test_limits = {
    .max_nodes = 500U,
    .max_edges = 1000U,
    .max_serialized_bytes = 1024U * 1024U
};

static void test_remove_tree(const char *path)
{
    GDir *directory = g_dir_open(path, 0, NULL);
    const char *name = NULL;
    if (directory == NULL) { g_remove(path); return; }
    while ((name = g_dir_read_name(directory)) != NULL)
    {
        char *child = g_build_filename(path, name, NULL);
        if (g_file_test(child, G_FILE_TEST_IS_DIR)) test_remove_tree(child);
        else g_assert_cmpint(g_remove(child), ==, 0);
        g_free(child);
    }
    g_dir_close(directory);
    g_assert_cmpint(g_rmdir(path), ==, 0);
}

static gint64 test_query_count(Database *database, const char *table)
{
    char *sql = g_strdup_printf("SELECT COUNT(*) FROM %s;", table);
    DatabaseStatement *statement = database_statement_prepare(database, sql);
    gint64 count = -1;
    g_assert_nonnull(statement);
    g_assert_cmpint(database_statement_step(statement), ==,
        DATABASE_STATEMENT_STEP_ROW);
    g_assert_true(database_statement_column_int64(statement, 0, &count));
    g_assert_cmpint(database_statement_step(statement), ==,
        DATABASE_STATEMENT_STEP_DONE);
    database_statement_finalize(statement);
    g_free(sql);
    return count;
}

static char *test_query_metadata(Database *database, const char *key)
{
    DatabaseStatement *statement = database_statement_prepare(database,
        "SELECT value FROM metadata WHERE key=?;");
    char *value = NULL;
    g_assert_nonnull(statement);
    g_assert_true(database_statement_bind_text(statement, 1, key));
    g_assert_cmpint(database_statement_step(statement), ==,
        DATABASE_STATEMENT_STEP_ROW);
    g_assert_true(database_statement_column_text(statement, 0, &value));
    database_statement_finalize(statement);
    return value;
}

static void test_database_read_only_contract(void)
{
    GError *error = NULL;
    char *directory = g_dir_make_tmp("labfy-core-readonly-XXXXXX", &error);
    char *path = g_build_filename(directory, "Enquete.sqlite", NULL);
    char *missing = g_build_filename(directory, "absente.sqlite", NULL);
    Database *database = NULL;
    DatabaseStatement *statement = NULL;
    g_assert_no_error(error);
    g_assert_true(database_initialize(path, "SPECIMEN read-only", directory));
    database = database_open_read_only(path, &error);
    g_assert_no_error(error);
    g_assert_nonnull(database);
    g_assert_true(database_transaction_begin_read_only(database));
    g_assert_cmpint(test_query_count(database, "metadata"), >, 0);
    g_assert_true(database_transaction_commit(database));
    statement = database_statement_prepare(database,
        "INSERT INTO metadata(key,value) VALUES('forbidden','write');");
    g_assert_nonnull(statement);
    g_test_expect_message(NULL, G_LOG_LEVEL_WARNING,
        "*attempt to write a readonly database*");
    g_assert_cmpint(database_statement_step(statement), ==,
        DATABASE_STATEMENT_STEP_ERROR);
    g_test_assert_expected_messages();
    database_statement_finalize(statement);
    database_close(database);
    database = database_open_read_only(missing, &error);
    g_assert_null(database);
    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
    g_clear_error(&error);
    g_assert_false(g_file_test(missing, G_FILE_TEST_EXISTS));
    g_free(missing); g_free(path);
    test_remove_tree(directory); g_free(directory);
}

static void test_database_read_only_rejects_unsupported_schema(void)
{
    GError *error = NULL;
    char *directory = g_dir_make_tmp("labfy-core-schema-XXXXXX", &error);
    char *path = g_build_filename(directory, "Enquete.sqlite", NULL);
    Database *writer = NULL;
    Database *reader = NULL;
    DatabaseStatement *statement = NULL;
    g_assert_no_error(error);
    g_assert_true(database_initialize(path, "SPECIMEN schema", directory));
    writer = database_open(path);
    statement = database_statement_prepare(writer,
        "UPDATE metadata SET value='21' WHERE key='schema_version';");
    g_assert_nonnull(statement);
    g_assert_cmpint(database_statement_step(statement), ==,
        DATABASE_STATEMENT_STEP_DONE);
    database_statement_finalize(statement);
    database_close(writer);
    reader = database_open_read_only(path, &error);
    g_assert_null(reader);
    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
    g_clear_error(&error);
    g_free(path); test_remove_tree(directory); g_free(directory);
}

static JsonObject *test_parse_object(const char *data, JsonParser **out_parser)
{
    JsonParser *parser = json_parser_new();
    GError *error = NULL;
    g_assert_true(json_parser_load_from_data(parser, data, -1, &error));
    g_assert_no_error(error);
    *out_parser = parser;
    return json_node_get_object(json_parser_get_root(parser));
}

static guint test_count_nodes(JsonArray *nodes, const char *kind,
    const char *state)
{
    guint count = 0;
    for (guint index = 0; index < json_array_get_length(nodes); index++)
    {
        JsonObject *node = json_array_get_object_element(nodes, index);
        if (g_strcmp0(json_object_get_string_member(node, "object_kind"), kind) == 0 &&
            (state == NULL || g_strcmp0(json_object_get_string_member(node, "state"),
                state) == 0)) count++;
    }
    return count;
}

static void test_assert_snapshot_contract(const char *data)
{
    JsonParser *parser = NULL;
    JsonObject *root = test_parse_object(data, &parser);
    JsonArray *nodes = json_object_get_array_member(root, "nodes");
    JsonArray *edges = json_object_get_array_member(root, "edges");
    GHashTable *ids = g_hash_table_new(g_str_hash, g_str_equal);
    g_assert_cmpstr(json_object_get_string_member(root, "contract"), ==,
        "labfy.web_graph.snapshot.v2");
    g_assert_cmpstr(json_object_get_string_member(root, "origin"), ==, "core");
    g_assert_cmpint(json_object_get_int_member(root, "schema_version"), ==, 20);
    g_assert_true(json_object_get_boolean_member(root, "complete"));
    g_assert_cmpuint(json_array_get_length(nodes), ==, 9U);
    g_assert_cmpuint(json_array_get_length(edges), ==, 8U);
    g_assert_cmpuint(test_count_nodes(nodes, "entity", NULL), ==, 4U);
    g_assert_cmpuint(test_count_nodes(nodes, "evidence", NULL), ==, 2U);
    g_assert_cmpuint(test_count_nodes(nodes, "observation", "promoted"), ==, 1U);
    g_assert_cmpuint(test_count_nodes(nodes, "observation", "unpromoted"), ==, 1U);
    g_assert_cmpuint(test_count_nodes(nodes, "execution", NULL), ==, 1U);
    for (guint index = 0; index < json_array_get_length(nodes); index++)
    {
        JsonObject *node = json_array_get_object_element(nodes, index);
        const char *id = json_object_get_string_member(node, "id");
        JsonObject *object_ref = json_object_get_object_member(node, "object_ref");
        g_assert_false(g_hash_table_contains(ids, id));
        g_hash_table_add(ids, (gpointer) id);
        g_assert_cmpstr(json_object_get_string_member(object_ref, "object_kind"), ==,
            json_object_get_string_member(node, "object_kind"));
        g_assert_cmpstr(json_object_get_string_member(object_ref, "object_id"), ==,
            json_object_get_string_member(node, "object_id"));
    }
    for (guint index = 0; index < json_array_get_length(edges); index++)
    {
        JsonObject *edge = json_array_get_object_element(edges, index);
        g_assert_true(g_hash_table_contains(ids,
            json_object_get_string_member(edge, "source")));
        g_assert_true(g_hash_table_contains(ids,
            json_object_get_string_member(edge, "target")));
    }
    g_assert_nonnull(g_strstr_len(data, -1, "<script>window.__LABFY_XSS__"));
    g_assert_nonnull(g_strstr_len(data, -1,
        "Aucune transformation identifiée"));
    g_hash_table_unref(ids);
    g_object_unref(parser);
}

static void test_specimen_projection_is_stable_and_read_only(void)
{
    GError *error = NULL;
    char *directory = g_dir_make_tmp("labfy-core-specimen-XXXXXX", &error);
    CoreGraphSpecimenPaths paths = {0};
    Database *reader = NULL;
    CoreGraphSnapshot *snapshot = NULL;
    char *published = NULL;
    char *serialized = NULL;
    char *schema_before = NULL;
    char *created_before = NULL;
    gsize published_length = 0;
    gsize serialized_length = 0;
    gint64 entity_count = 0;
    gint64 evidence_count = 0;
    char *journal_path = NULL;
    char *wal_path = NULL;
    char *shm_path = NULL;
    g_assert_no_error(error);
    g_assert_true(core_graph_specimen_generate(directory, &paths, &error));
    g_assert_no_error(error);
    journal_path = g_strdup_printf("%s-journal", paths.database_path);
    wal_path = g_strdup_printf("%s-wal", paths.database_path);
    shm_path = g_strdup_printf("%s-shm", paths.database_path);
    g_assert_false(g_file_test(journal_path, G_FILE_TEST_EXISTS));
    g_assert_false(g_file_test(wal_path, G_FILE_TEST_EXISTS));
    g_assert_false(g_file_test(shm_path, G_FILE_TEST_EXISTS));
    g_assert_true(g_file_get_contents(paths.snapshot_path, &published,
        &published_length, &error));
    g_assert_no_error(error);
    test_assert_snapshot_contract(published);
    reader = database_open_read_only(paths.database_path, &error);
    g_assert_no_error(error);
    entity_count = test_query_count(reader, "entites");
    evidence_count = test_query_count(reader, "preuves");
    schema_before = test_query_metadata(reader, "schema_version");
    created_before = test_query_metadata(reader, "created_at");
    snapshot = core_graph_projection_service_collect(reader, test_limits, &error);
    g_assert_no_error(error);
    g_assert_nonnull(snapshot);
    serialized = core_graph_snapshot_serialize(snapshot, TRUE,
        &serialized_length, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(serialized_length, ==, published_length);
    g_assert_cmpmem(serialized, serialized_length, published, published_length);
    g_assert_cmpint(test_query_count(reader, "entites"), ==, entity_count);
    g_assert_cmpint(test_query_count(reader, "preuves"), ==, evidence_count);
    char *schema_after = test_query_metadata(reader, "schema_version");
    char *created_after = test_query_metadata(reader, "created_at");
    g_assert_cmpstr(schema_after, ==, schema_before);
    g_assert_cmpstr(created_after, ==, created_before);
    g_assert_false(g_file_test(journal_path, G_FILE_TEST_EXISTS));
    g_assert_false(g_file_test(wal_path, G_FILE_TEST_EXISTS));
    g_assert_false(g_file_test(shm_path, G_FILE_TEST_EXISTS));
    g_free(created_after); g_free(schema_after);
    core_graph_snapshot_free(snapshot);
    database_close(reader);
    g_free(created_before); g_free(schema_before);
    g_free(shm_path); g_free(wal_path); g_free(journal_path);
    g_free(serialized); g_free(published);
    core_graph_specimen_paths_clear(&paths);
    test_remove_tree(directory); g_free(directory);
}

static void test_production_update_changes_reexport(void)
{
    GError *error = NULL;
    char *directory = g_dir_make_tmp("labfy-core-update-XXXXXX", &error);
    CoreGraphSpecimenPaths paths = {0};
    Database *writer = NULL;
    Database *reader = NULL;
    EvidenceDao *dao = NULL;
    CoreGraphSnapshot *snapshot = NULL;
    char *before = NULL;
    char *after = NULL;
    gsize before_length = 0;
    gsize after_length = 0;
    g_assert_no_error(error);
    g_assert_true(core_graph_specimen_generate(directory, &paths, &error));
    g_assert_true(g_file_get_contents(paths.snapshot_path, &before,
        &before_length, &error));
    writer = database_open(paths.database_path);
    dao = evidence_dao_new(writer, &error);
    g_assert_nonnull(dao);
    g_assert_true(evidence_dao_update_metadata(dao,
        CORE_GRAPH_SPECIMEN_EVIDENCE_A, "document",
        "SPECIMEN/preuve-échange.txt", "Générateur C SPECIMEN J3",
        "Description modifiée via EvidenceDao", "2026-09-26T12:05:00Z",
        &error));
    g_assert_no_error(error);
    evidence_dao_free(dao);
    database_close(writer);
    reader = database_open_read_only(paths.database_path, &error);
    snapshot = core_graph_projection_service_collect(reader, test_limits, &error);
    after = core_graph_snapshot_serialize(snapshot, TRUE, &after_length, &error);
    g_assert_no_error(error);
    g_assert_true(before_length != after_length ||
        memcmp(before, after, before_length) != 0);
    g_assert_nonnull(g_strstr_len(after, after_length,
        "Description modifiée via EvidenceDao"));
    g_free(after); g_free(before);
    core_graph_snapshot_free(snapshot);
    database_close(reader);
    core_graph_specimen_paths_clear(&paths);
    test_remove_tree(directory); g_free(directory);
}

static void test_serialization_limits_utf8_and_atomic_failure(void)
{
    GError *error = NULL;
    CoreGraphLimits limits = { .max_nodes = 1U, .max_edges = 1U,
        .max_serialized_bytes = 64U };
    CoreGraphSnapshot *snapshot = core_graph_snapshot_new(
        "SPECIMEN-TEST", "SPECIMEN", 20U, limits, &error);
    const char invalid_label[] = {'x', (char) 0xff, '\0'};
    CoreGraphNodeView node = {
        .id = "entity:one", .object_kind = "entity", .object_id = "one",
        .type = "person", .label = invalid_label, .state = "active",
        .provenance_complete = FALSE
    };
    char *directory = g_dir_make_tmp("labfy-core-atomic-XXXXXX", &error);
    char *output = g_build_filename(directory, "snapshot.json", NULL);
    g_assert_no_error(error);
    g_assert_nonnull(snapshot);
    g_assert_true(core_graph_snapshot_add_node(snapshot, &node, &error));
    g_assert_null(core_graph_snapshot_serialize(snapshot, TRUE, NULL, &error));
    g_assert_error(error, CORE_GRAPH_SERIALIZER_ERROR,
        CORE_GRAPH_SERIALIZER_ERROR_INVALID_UTF8);
    g_clear_error(&error);
    core_graph_snapshot_free(snapshot);

    snapshot = core_graph_snapshot_new("SPECIMEN-TEST", "SPECIMEN", 20U,
        limits, &error);
    node.label = "Étiquette valide mais snapshot volontairement trop petit";
    g_assert_true(core_graph_snapshot_add_node(snapshot, &node, &error));
    g_assert_false(core_graph_snapshot_write_atomic(snapshot, TRUE, output,
        &error));
    g_assert_error(error, CORE_GRAPH_SERIALIZER_ERROR,
        CORE_GRAPH_SERIALIZER_ERROR_LIMIT);
    g_clear_error(&error);
    g_assert_false(g_file_test(output, G_FILE_TEST_EXISTS));
    core_graph_snapshot_free(snapshot);
    g_free(output); test_remove_tree(directory); g_free(directory);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/core-graph/database/read-only",
        test_database_read_only_contract);
    g_test_add_func("/core-graph/database/schema",
        test_database_read_only_rejects_unsupported_schema);
    g_test_add_func("/core-graph/specimen/stable",
        test_specimen_projection_is_stable_and_read_only);
    g_test_add_func("/core-graph/specimen/production-update",
        test_production_update_changes_reexport);
    g_test_add_func("/core-graph/serializer/failures",
        test_serialization_limits_utf8_and_atomic_failure);
    return g_test_run();
}
