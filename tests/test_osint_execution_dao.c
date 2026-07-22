/******************************************************************************
 * @file test_osint_execution_dao.c
 * @brief Tests du modèle et du DAO de provenance OSINT.
 ******************************************************************************/

#include "dao/osint_execution_dao.h"
#include "database/database.h"
#include "database/statement.h"

#include <glib.h>
#include <glib/gstdio.h>

static void test_insert_and_read_raw_execution(void)
{
    const guint8 raw_data[] = {0x41U, 0x00U, 0xFFU};
    GError *error = NULL;
    char *directory = g_dir_make_tmp("labfy-osint-execution-XXXXXX", &error);
    char *path = g_build_filename(directory, "Enquete.sqlite", NULL);
    Database *database = NULL;
    OsintExecutionDao *dao = NULL;
    OsintExecutionRecord *record = NULL;
    OsintExecutionRecord *loaded = NULL;
    GBytes *stdout_raw = g_bytes_new_static(raw_data, sizeof(raw_data));
    GBytes *stderr_raw = g_bytes_new_static("", 0U);
    GBytes *loaded_stdout = NULL;
    GPtrArray *history = NULL;
    GPtrArray *linked_objects = NULL;
    DatabaseStatement *statement = NULL;

    g_assert_true(database_initialize(path, "OSINT", directory));
    database = database_open(path);
    dao = osint_execution_dao_new(database, &error);
    record = osint_execution_record_new(
        "eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee", "dns.dig", "DiG 9",
        "dns-preview", "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa", "entity",
        "example.org", "[\"example.org\"]", "2026-01-01T00:00:00Z",
        "2026-01-01T00:00:01Z", TRUE, 0, "completed", stdout_raw,
        stderr_raw,
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        &error
    );
    g_assert_nonnull(record);
    g_assert_true(osint_execution_dao_insert(dao, record, &error));
    statement = database_statement_prepare(database,
        "INSERT INTO entites(id,type_id,valeur,confiance,created_at,updated_at,status) "
        "VALUES('bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb',"
        "(SELECT id FROM types_entite LIMIT 1),'linked.example',50,"
        "'2026-01-01T00:00:00Z','2026-01-01T00:00:00Z','active');");
    g_assert_nonnull(statement);
    g_assert_cmpint(database_statement_step(statement), ==,
        DATABASE_STATEMENT_STEP_DONE);
    database_statement_finalize(statement);
    g_assert_true(osint_execution_dao_link_entity(
        dao, "eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee",
        "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb", "created", &error));
    loaded = osint_execution_dao_find_by_identifier(
        dao, "eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee", &error
    );
    g_assert_nonnull(loaded);
    g_assert_cmpstr(
        osint_execution_record_get_target_value(loaded), ==, "example.org"
    );
    g_assert_true(osint_execution_record_has_exit_code(loaded));
    loaded_stdout = osint_execution_record_ref_stdout(loaded);
    g_assert_true(g_bytes_equal(stdout_raw, loaded_stdout));
    history = osint_execution_dao_list_by_selection(
        dao, "entity", "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa", &error);
    g_assert_nonnull(history);
    g_assert_cmpuint(history->len, ==, 1U);
    g_ptr_array_unref(history);
    history = osint_execution_dao_list_by_selection(
        dao, "entity", "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb", &error);
    g_assert_nonnull(history);
    g_assert_cmpuint(history->len, ==, 1U);
    linked_objects = osint_execution_dao_list_linked_objects(
        dao, "eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee", &error);
    g_assert_nonnull(linked_objects);
    g_assert_cmpuint(linked_objects->len, ==, 1U);
    g_assert_nonnull(g_strstr_len(
        g_ptr_array_index(linked_objects, 0U), -1, "créée"));

    g_ptr_array_unref(linked_objects);
    g_ptr_array_unref(history);
    g_bytes_unref(loaded_stdout);
    osint_execution_record_free(loaded);
    osint_execution_record_free(record);
    osint_execution_dao_free(dao);
    database_close(database);
    g_bytes_unref(stdout_raw);
    g_bytes_unref(stderr_raw);
    g_assert_cmpint(g_remove(path), ==, 0);
    g_assert_cmpint(g_rmdir(directory), ==, 0);
    g_free(path);
    g_free(directory);
}

static void test_invalid_model(void)
{
    GError *error = NULL;
    GBytes *empty = g_bytes_new_static("", 0U);
    g_assert_null(osint_execution_record_new(
        "invalid", "dns.dig", NULL, "dns-preview", "invalid", "entity",
        "example.org", "[]", "bad", "bad", FALSE, -1, "completed",
        empty, empty, "bad", &error
    ));
    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
    g_clear_error(&error);
    g_bytes_unref(empty);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/osint-execution-dao/raw", test_insert_and_read_raw_execution);
    g_test_add_func("/osint-execution-record/invalid", test_invalid_model);
    return g_test_run();
}
