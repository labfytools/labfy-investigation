#include "core/evidence_staging.h"
#include "core/person_creation_coordinator.h"
#include "database/database.h"
#include "database/statement.h"
#include <glib.h>
#include <glib/gstdio.h>

static guint64 count(Database *database, const char *table)
{
    char *sql = g_strdup_printf("SELECT COUNT(*) FROM %s;", table);
    DatabaseStatement *statement = database_statement_prepare(database, sql);
    int64_t value = 0;
    g_assert_nonnull(statement);
    g_assert_cmpint(database_statement_step(statement), ==,
        DATABASE_STATEMENT_STEP_ROW);
    g_assert_true(database_statement_column_int64(statement, 0, &value));
    database_statement_finalize(statement);
    g_free(sql);
    return (guint64) value;
}
static void remove_tree(const char *root)
{
    GDir *directory = g_dir_open(root, 0, NULL);
    const char *name;
    if (directory == NULL) return;
    while ((name = g_dir_read_name(directory)) != NULL) {
        char *path = g_build_filename(root, name, NULL);
        if (g_file_test(path, G_FILE_TEST_IS_DIR)) remove_tree(path);
        else g_unlink(path);
        g_free(path);
    }
    g_dir_close(directory);
    g_rmdir(root);
}
static void test_success_and_rollback(void)
{
    char *root = g_dir_make_tmp("labfy-coordinator-XXXXXX", NULL);
    char *database_directory =
        g_build_filename(root, "00_BaseDeDonnees", NULL);
    char *database_path =
        g_build_filename(database_directory, "Enquete.sqlite", NULL);
    char *source_one = g_build_filename(root, "SPECIMEN-one.txt", NULL);
    char *source_two = g_build_filename(root, "SPECIMEN-two.txt", NULL);
    EvidenceStaging *staging = NULL;
    EvidenceStagingResult *prepared_one = NULL, *prepared_two = NULL;
    PersonEvidenceSelection *selection = NULL;
    PersonCreationCoordinatorResult *result = NULL;
    Database *database;
    PersonEntityInput person = {
        .designation = "Personne SPECIMEN",
        .identification_status = "suspected",
        .confidence = 20
    };
    GError *error = NULL;
    g_assert_cmpint(g_mkdir_with_parents(database_directory, 0700), ==, 0);
    g_assert_true(database_initialize(
        database_path, "Enquête SPECIMEN", root));
    g_assert_true(g_file_set_contents(
        source_one, "SPECIMEN-ONE", -1, NULL));
    g_assert_true(g_file_set_contents(
        source_two, "SPECIMEN-TWO", -1, NULL));
    staging = evidence_staging_new(&error);
    prepared_one = evidence_staging_prepare(
        staging, source_one, NULL, &error);
    prepared_two = evidence_staging_prepare(
        staging, source_two, NULL, &error);
    g_assert_no_error(error);
    selection = person_evidence_selection_new();
    g_assert_true(person_evidence_selection_add_staged(selection,
        prepared_one->source_path, prepared_one->staging_path,
        prepared_one->original_name, prepared_one->mime_type, "text",
        prepared_one->size_bytes, prepared_one->sha256, NULL,
        prepared_one->prepared_at, &error));
    database = database_open(database_path);
    result = person_creation_coordinator_execute(database, root, &person,
        selection, NULL, &error);
    g_assert_no_error(error);
    g_assert_nonnull(result);
    g_assert_cmpuint(result->evidence_identifiers->len, ==, 1);
    g_assert_cmpuint(count(database, "entites"), ==, 1);
    g_assert_cmpuint(count(database, "preuves"), ==, 1);
    g_assert_cmpuint(count(database, "preuve_entites"), ==, 1);
    g_assert_cmpuint(count(database, "preuve_entite_sources"), ==, 1);
    person_creation_coordinator_result_free(result);
    person_evidence_selection_free(selection);
    selection = person_evidence_selection_new();
    g_assert_true(person_evidence_selection_add_staged(selection,
        prepared_two->source_path, prepared_two->staging_path,
        prepared_two->original_name, prepared_two->mime_type, "missing_type",
        prepared_two->size_bytes, prepared_two->sha256, NULL,
        prepared_two->prepared_at, &error));
    result = person_creation_coordinator_execute(database, root, &person,
        selection, NULL, &error);
    g_assert_null(result);
    g_assert_nonnull(error);
    g_clear_error(&error);
    g_assert_cmpuint(count(database, "entites"), ==, 1);
    g_assert_cmpuint(count(database, "preuves"), ==, 1);
    database_close(database);
    person_evidence_selection_free(selection);
    evidence_staging_result_free(prepared_one);
    evidence_staging_result_free(prepared_two);
    evidence_staging_free(staging);
    g_free(database_directory); g_free(database_path);
    g_free(source_one); g_free(source_two);
    remove_tree(root); g_free(root);
}
int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/person-coordinator/success-rollback",
        test_success_and_rollback);
    return g_test_run();
}
