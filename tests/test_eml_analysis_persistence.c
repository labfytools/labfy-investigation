#define _POSIX_C_SOURCE 200809L

/******************************************************************************
 * @file test_eml_analysis_persistence.c
 * @brief Analyse EML native, publication V20 et projection de provenance.
 ******************************************************************************/

#include "core/core_graph_projection_service.h"
#include "core/eml_analysis_persistence_service.h"
#include "core/eml_analysis_persistence_service_test.h"
#include "core/eml_graph_specimen.h"
#include "core/evidence_importer.h"
#include "core/observation_review_service.h"
#include "dao/evidence_entity_dao.h"
#include "database/database.h"
#include "database/statement.h"
#include "models/evidence_observation.h"
#include "models/evidence_record.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <json-glib/json-glib.h>
#include <unistd.h>

typedef struct
{
    char *root;
    char *database_path;
    char *controlled_path;
    char *evidence_identifier;
} TestFixture;

static const char *const specimen_eml =
    "From: Élodie Exemple <Elodie@Atelier.test>\r\n"
    "To: Noé <noe@destination.test>\r\n"
    "Received: from relais.atelier.test ([192.0.2.42]) by mx.test;\r\n"
    "Message-ID: <specimen@atelier.test>\r\n"
    "Subject: SPECIMEN\r\n\r\nCorps local.\r\n";

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

static gint64 test_count(Database *database, const char *table)
{
    char *sql = g_strdup_printf("SELECT COUNT(*) FROM %s;", table);
    DatabaseStatement *statement = database_statement_prepare(database, sql);
    gint64 value = -1;
    g_assert_nonnull(statement);
    g_assert_cmpint(database_statement_step(statement), ==,
        DATABASE_STATEMENT_STEP_ROW);
    g_assert_true(database_statement_column_int64(statement, 0, &value));
    database_statement_finalize(statement);
    g_free(sql);
    return value;
}

static void test_assert_database_ok(Database *database)
{
    DatabaseStatement *statement = database_statement_prepare(database,
        "PRAGMA integrity_check;");
    char *value = NULL;
    g_assert_nonnull(statement);
    g_assert_cmpint(database_statement_step(statement), ==,
        DATABASE_STATEMENT_STEP_ROW);
    g_assert_true(database_statement_column_text(statement, 0, &value));
    g_assert_cmpstr(value, ==, "ok");
    g_free(value);
    database_statement_finalize(statement);
    statement = database_statement_prepare(database, "PRAGMA foreign_key_check;");
    g_assert_nonnull(statement);
    g_assert_cmpint(database_statement_step(statement), ==,
        DATABASE_STATEMENT_STEP_DONE);
    database_statement_finalize(statement);
}

static TestFixture *test_fixture_new(const char *contents, gssize length)
{
    GError *error = NULL;
    TestFixture *fixture = g_new0(TestFixture, 1);
    char *source_path = NULL;
    char *destination = NULL;
    Database *database = NULL;
    EvidenceImporter *importer = NULL;
    EvidenceRecord *record = NULL;
    fixture->root = g_dir_make_tmp("labfy-eml-persistence-XXXXXX", &error);
    g_assert_no_error(error);
    fixture->database_path = g_build_filename(fixture->root,
        "Enquete.sqlite", NULL);
    source_path = g_build_filename(fixture->root, "SPECIMEN-input.eml", NULL);
    destination = g_build_filename(fixture->root,
        "01_Preuves_Originales", "Emails", NULL);
    g_assert_cmpint(g_mkdir_with_parents(destination, 0700), ==, 0);
    g_assert_true(g_file_set_contents(source_path, contents, length, &error));
    g_assert_no_error(error);
    g_assert_true(database_initialize(fixture->database_path,
        "SPECIMEN EML service", fixture->root));
    database = database_open(fixture->database_path);
    g_assert_nonnull(database);
    importer = evidence_importer_new(database, &error);
    g_assert_no_error(error);
    EvidenceImportRequest request = {
        .source_path = source_path,
        .destination_directory = destination,
        .relative_directory = "01_Preuves_Originales/Emails",
        .type_identifier = "email",
        .collected_at = "2026-09-26T14:00:00Z",
        .source = "Fixture SPECIMEN",
        .description = "Entrée synthétique"
    };
    record = evidence_importer_import(importer, &request, NULL, &error);
    g_assert_no_error(error);
    g_assert_nonnull(record);
    fixture->evidence_identifier = g_strdup(
        evidence_record_get_identifier(record));
    fixture->controlled_path = g_build_filename(fixture->root,
        evidence_record_get_relative_path(record), NULL);
    evidence_record_free(record);
    evidence_importer_free(importer);
    database_close(database);
    g_free(destination);
    g_free(source_path);
    return fixture;
}

static void test_fixture_free(TestFixture *fixture)
{
    if (fixture == NULL) return;
    test_remove_tree(fixture->root);
    g_free(fixture->evidence_identifier);
    g_free(fixture->controlled_path);
    g_free(fixture->database_path);
    g_free(fixture->root);
    g_free(fixture);
}

static EmlAnalysisPublicationResult *test_analyze(
    Database *database,
    TestFixture *fixture,
    const char *request_identifier,
    const char *derivative_identifier,
    GError **error)
{
    EmlAnalysisPersistenceRequest request = {
        .request_identifier = request_identifier,
        .source_evidence_identifier = fixture->evidence_identifier,
        .derivative_evidence_identifier = derivative_identifier,
        .requested_at = "2026-09-26T14:00:00Z"
    };
    EmlAnalysisPersistenceService *service =
        eml_analysis_persistence_service_new(database, fixture->root, error);
    EmlAnalysisPrepared *prepared = service != NULL
        ? eml_analysis_persistence_service_prepare(service, &request, NULL, error)
        : NULL;
    EmlAnalysisPublicationResult *result = prepared != NULL
        ? eml_analysis_persistence_service_publish(service, prepared, NULL, error)
        : NULL;
    eml_analysis_prepared_free(prepared);
    eml_analysis_persistence_service_free(service);
    return result;
}

static void test_native_analysis_persists_and_replays(void)
{
    GError *error = NULL;
    TestFixture *fixture = test_fixture_new(specimen_eml, -1);
    Database *database = database_open(fixture->database_path);
    EmlAnalysisPublicationResult *first = test_analyze(database, fixture,
        EML_GRAPH_REQUEST_PRIMARY, EML_GRAPH_DERIVATIVE_PRIMARY, &error);
    g_assert_no_error(error);
    g_assert_nonnull(first);
    g_assert_false(eml_analysis_publication_result_was_reused(first));
    g_assert_cmpuint(eml_analysis_publication_result_get_observation_count(first),
        >, 0U);
    g_assert_cmpint(test_count(database, "entites"), ==, 0);
    g_assert_cmpint(test_count(database, "relations"), ==, 0);
    g_assert_cmpint(test_count(database, "extractions"), ==, 1);
    g_assert_cmpint(test_count(database, "preuves"), ==, 2);
    EvidenceEntityDao *dao = evidence_entity_dao_new(database, &error);
    GPtrArray *observations = evidence_entity_dao_list_observations(dao,
        fixture->evidence_identifier, &error);
    g_assert_no_error(error);
    g_assert_nonnull(observations);
    gboolean found_normalization = FALSE;
    for (guint index = 0; index < observations->len; index++)
    {
        EvidenceObservation *item = g_ptr_array_index(observations, index);
        g_assert_cmpstr(item->verification_status, ==, "proposed");
        g_assert_null(item->entity_identifier);
        g_assert_cmpstr(item->extraction_identifier, ==,
            EML_GRAPH_REQUEST_PRIMARY);
        if (g_strcmp0(item->value_raw, "Elodie@Atelier.test") == 0 &&
            g_strcmp0(item->value_normalized, "elodie@atelier.test") == 0)
            found_normalization = TRUE;
    }
    g_assert_true(found_normalization);
    test_assert_database_ok(database);
    g_ptr_array_unref(observations);
    evidence_entity_dao_free(dao);
    database_close(database);

    database = database_open(fixture->database_path);
    EmlAnalysisPublicationResult *replay = test_analyze(database, fixture,
        EML_GRAPH_REQUEST_PRIMARY, EML_GRAPH_DERIVATIVE_PRIMARY, &error);
    g_assert_no_error(error);
    g_assert_nonnull(replay);
    g_assert_true(eml_analysis_publication_result_was_reused(replay));
    g_assert_cmpuint(eml_analysis_publication_result_get_observation_count(replay),
        ==, eml_analysis_publication_result_get_observation_count(first));
    g_assert_cmpint(test_count(database, "extractions"), ==, 1);
    g_assert_cmpint(test_count(database, "evidence_entity_observations"), ==,
        (gint64) eml_analysis_publication_result_get_observation_count(first));

    EmlAnalysisPersistenceRequest conflict = {
        .request_identifier = EML_GRAPH_REQUEST_PRIMARY,
        .source_evidence_identifier = fixture->evidence_identifier,
        .derivative_evidence_identifier = EML_GRAPH_DERIVATIVE_REANALYSIS,
        .requested_at = "2026-09-26T14:00:00Z"
    };
    EmlAnalysisPersistenceService *service =
        eml_analysis_persistence_service_new(database, fixture->root, &error);
    g_assert_nonnull(service);
    g_assert_null(eml_analysis_persistence_service_prepare(service, &conflict,
        NULL, &error));
    g_assert_error(error, EML_ANALYSIS_PERSISTENCE_ERROR,
        EML_ANALYSIS_PERSISTENCE_ERROR_CONFLICT);
    g_clear_error(&error);
    eml_analysis_persistence_service_free(service);

    DatabaseStatement *mutation = database_statement_prepare(database,
        "UPDATE evidence_entity_observations SET value_normalized='tampered' "
        "WHERE extraction_id='61000000-0000-4000-8000-000000000031' "
        "AND id=(SELECT MIN(id) FROM evidence_entity_observations WHERE "
        "extraction_id='61000000-0000-4000-8000-000000000031');");
    g_assert_nonnull(mutation);
    g_assert_cmpint(database_statement_step(mutation), ==,
        DATABASE_STATEMENT_STEP_DONE);
    database_statement_finalize(mutation);
    g_assert_null(test_analyze(database, fixture, EML_GRAPH_REQUEST_PRIMARY,
        EML_GRAPH_DERIVATIVE_PRIMARY, &error));
    g_assert_error(error, EML_ANALYSIS_PERSISTENCE_ERROR,
        EML_ANALYSIS_PERSISTENCE_ERROR_CONFLICT);
    g_clear_error(&error);

    g_assert_true(g_file_set_contents(fixture->controlled_path,
        "From: altered-replay@specimen.test\r\n\r\n", -1, &error));
    g_assert_no_error(error);
    g_assert_null(test_analyze(database, fixture, EML_GRAPH_REQUEST_PRIMARY,
        EML_GRAPH_DERIVATIVE_PRIMARY, &error));
    g_assert_error(error, EML_ANALYSIS_PERSISTENCE_ERROR,
        EML_ANALYSIS_PERSISTENCE_ERROR_SOURCE);
    g_clear_error(&error);
    g_assert_cmpint(test_count(database, "extractions"), ==, 1);
    eml_analysis_publication_result_free(replay);
    eml_analysis_publication_result_free(first);
    database_close(database);
    test_fixture_free(fixture);
}

static void test_replay_accepts_journalled_review_and_promotion(void)
{
    GError *error = NULL;
    TestFixture *fixture = test_fixture_new(specimen_eml, -1);
    Database *database = database_open(fixture->database_path);
    EmlAnalysisPublicationResult *first = test_analyze(database, fixture,
        EML_GRAPH_REQUEST_PRIMARY, EML_GRAPH_DERIVATIVE_PRIMARY, &error);
    EvidenceEntityDao *dao = evidence_entity_dao_new(database, &error);
    GPtrArray *observations = evidence_entity_dao_list_observations(dao,
        fixture->evidence_identifier, &error);
    const char *observation_identifier = NULL;
    ObservationReviewService *review_service = NULL;
    ObservationReviewResult review_result = {0};
    g_assert_no_error(error);
    g_assert_nonnull(first);
    g_assert_nonnull(observations);
    for (guint index = 0U; index < observations->len; index++)
    {
        EvidenceObservation *item = g_ptr_array_index(observations, index);
        if (g_strcmp0(item->value_raw, "Elodie@Atelier.test") == 0)
        {
            observation_identifier = item->identifier;
            break;
        }
    }
    g_assert_nonnull(observation_identifier);
    review_service = observation_review_service_new(database, &error);
    g_assert_no_error(error);
    g_assert_nonnull(review_service);

    ObservationReviewRequest decide = {
        .operation_identifier = "63000000-0000-4000-8000-000000000001",
        .evidence_identifier = fixture->evidence_identifier,
        .observation_identifier = observation_identifier,
        .expected_revision = 0U,
        .action = OBSERVATION_REVIEW_ACTION_DECIDE,
        .verification_status = "confirmed",
        .author = "analyste-specimen",
        .reason = "Confirmation humaine synthétique",
        .occurred_at = "2026-09-26T14:05:00Z"
    };
    g_assert_true(observation_review_service_apply(review_service, &decide,
        &review_result, &error));
    g_assert_no_error(error);
    observation_review_result_clear(&review_result);
    ObservationReviewRequest correct = decide;
    correct.operation_identifier =
        "63000000-0000-4000-8000-000000000002";
    correct.expected_revision = 1U;
    correct.action = OBSERVATION_REVIEW_ACTION_CORRECT;
    correct.verification_status = NULL;
    correct.corrected_value = "elodie.revue@atelier.test";
    g_assert_true(observation_review_service_apply(review_service, &correct,
        &review_result, &error));
    g_assert_no_error(error);
    observation_review_result_clear(&review_result);
    ObservationReviewRequest promote = decide;
    promote.operation_identifier =
        "63000000-0000-4000-8000-000000000003";
    promote.expected_revision = 2U;
    promote.action = OBSERVATION_REVIEW_ACTION_PROMOTE_CREATE;
    promote.verification_status = NULL;
    g_assert_true(observation_review_service_apply(review_service, &promote,
        &review_result, &error));
    g_assert_no_error(error);
    g_assert_true(review_result.entity_created);
    observation_review_result_clear(&review_result);

    EmlAnalysisPublicationResult *promoted_replay = test_analyze(database,
        fixture, EML_GRAPH_REQUEST_PRIMARY, EML_GRAPH_DERIVATIVE_PRIMARY,
        &error);
    g_assert_no_error(error);
    g_assert_nonnull(promoted_replay);
    g_assert_true(eml_analysis_publication_result_was_reused(promoted_replay));

    ObservationReviewRequest withdraw = decide;
    withdraw.operation_identifier =
        "63000000-0000-4000-8000-000000000004";
    withdraw.expected_revision = 3U;
    withdraw.action = OBSERVATION_REVIEW_ACTION_WITHDRAW_PROMOTION;
    withdraw.verification_status = NULL;
    g_assert_true(observation_review_service_apply(review_service, &withdraw,
        &review_result, &error));
    g_assert_no_error(error);
    observation_review_result_clear(&review_result);
    EmlAnalysisPublicationResult *withdrawn_replay = test_analyze(database,
        fixture, EML_GRAPH_REQUEST_PRIMARY, EML_GRAPH_DERIVATIVE_PRIMARY,
        &error);
    g_assert_no_error(error);
    g_assert_nonnull(withdrawn_replay);
    g_assert_true(eml_analysis_publication_result_was_reused(withdrawn_replay));
    test_assert_database_ok(database);

    eml_analysis_publication_result_free(withdrawn_replay);
    eml_analysis_publication_result_free(promoted_replay);
    observation_review_service_free(review_service);
    g_ptr_array_unref(observations);
    evidence_entity_dao_free(dao);
    eml_analysis_publication_result_free(first);
    database_close(database);
    test_fixture_free(fixture);
}

static void test_publication_failure_rolls_back_and_compensates(void)
{
    GError *error = NULL;
    TestFixture *fixture = test_fixture_new(specimen_eml, -1);
    Database *database = database_open(fixture->database_path);
    EmlAnalysisPersistenceRequest request = {
        .request_identifier = EML_GRAPH_REQUEST_PRIMARY,
        .source_evidence_identifier = fixture->evidence_identifier,
        .derivative_evidence_identifier = EML_GRAPH_DERIVATIVE_PRIMARY,
        .requested_at = "2026-09-26T14:00:00Z"
    };
    EmlAnalysisPersistenceService *service =
        eml_analysis_persistence_service_new(database, fixture->root, &error);
    EmlAnalysisPrepared *prepared =
        eml_analysis_persistence_service_prepare(service, &request, NULL, &error);
    char *artifact = g_build_filename(fixture->root,
        "02_Preuves_Traitees", "Extractions", "EML",
        "62000000-0000-4000-8000-000000000031.json", NULL);
    g_assert_no_error(error);
    g_assert_nonnull(prepared);
    eml_analysis_persistence_test_fail_after_derivative_insert(TRUE);
    g_assert_null(eml_analysis_persistence_service_publish(service, prepared,
        NULL, &error));
    eml_analysis_persistence_test_fail_after_derivative_insert(FALSE);
    g_assert_error(error, EML_ANALYSIS_PERSISTENCE_ERROR,
        EML_ANALYSIS_PERSISTENCE_ERROR_DATABASE);
    g_clear_error(&error);
    g_assert_cmpint(test_count(database, "preuves"), ==, 1);
    g_assert_cmpint(test_count(database, "extractions"), ==, 0);
    g_assert_cmpint(test_count(database, "evidence_entity_observations"), ==, 0);
    g_assert_false(g_file_test(artifact, G_FILE_TEST_EXISTS));
    test_assert_database_ok(database);
    g_free(artifact);
    eml_analysis_prepared_free(prepared);
    eml_analysis_persistence_service_free(service);
    database_close(database);
    test_fixture_free(fixture);
}

static void test_missing_modified_and_oversize_sources(void)
{
    GError *error = NULL;
    TestFixture *missing = test_fixture_new(specimen_eml, -1);
    Database *database = database_open(missing->database_path);
    g_assert_cmpint(g_remove(missing->controlled_path), ==, 0);
    g_assert_null(test_analyze(database, missing, EML_GRAPH_REQUEST_PRIMARY,
        EML_GRAPH_DERIVATIVE_PRIMARY, &error));
    g_assert_error(error, EML_ANALYSIS_PERSISTENCE_ERROR,
        EML_ANALYSIS_PERSISTENCE_ERROR_SOURCE);
    g_clear_error(&error);
    database_close(database);
    test_fixture_free(missing);

    TestFixture *modified = test_fixture_new(specimen_eml, -1);
    database = database_open(modified->database_path);
    g_assert_true(g_file_set_contents(modified->controlled_path,
        "From: changed@specimen.test\r\n\r\n", -1, &error));
    g_assert_no_error(error);
    g_assert_null(test_analyze(database, modified, EML_GRAPH_REQUEST_PRIMARY,
        EML_GRAPH_DERIVATIVE_PRIMARY, &error));
    g_assert_error(error, EML_ANALYSIS_PERSISTENCE_ERROR,
        EML_ANALYSIS_PERSISTENCE_ERROR_SOURCE);
    g_clear_error(&error);
    database_close(database);
    test_fixture_free(modified);

    GString *large = g_string_sized_new(EML_ANALYSIS_MAX_SOURCE_BYTES + 1U);
    g_string_append(large, "From: large@specimen.test\r\n\r\n");
    while (large->len <= EML_ANALYSIS_MAX_SOURCE_BYTES)
        g_string_append_c(large, 'x');
    TestFixture *oversize = test_fixture_new(large->str, large->len);
    g_string_free(large, TRUE);
    database = database_open(oversize->database_path);
    g_assert_null(test_analyze(database, oversize, EML_GRAPH_REQUEST_PRIMARY,
        EML_GRAPH_DERIVATIVE_PRIMARY, &error));
    g_assert_error(error, EML_ANALYSIS_PERSISTENCE_ERROR,
        EML_ANALYSIS_PERSISTENCE_ERROR_LIMIT);
    g_clear_error(&error);
    database_close(database);
    test_fixture_free(oversize);
}

static void test_specimen_reanalysis_and_projection_failures(void)
{
    GError *error = NULL;
    char *directory = g_dir_make_tmp("labfy-eml-graph-test-XXXXXX", &error);
    EmlGraphSpecimenPaths paths = {0};
    g_assert_no_error(error);
    g_assert_true(eml_graph_specimen_generate(directory,
        EML_GRAPH_SPECIMEN_DEFAULT, &paths, &error));
    g_assert_no_error(error);
    Database *reader = database_open_read_only(paths.database_path, &error);
    g_assert_no_error(error);
    g_assert_nonnull(reader);
    g_assert_cmpint(test_count(reader, "extractions"), ==, 2);
    CoreGraphLimits too_small = {1U, 1U, 1024U * 1024U};
    g_assert_null(core_graph_projection_service_collect(reader, too_small,
        &error));
    g_assert_nonnull(error);
    g_clear_error(&error);
    g_assert_null(core_graph_projection_service_collect(reader, too_small,
        NULL));
    database_close(reader);
    eml_graph_specimen_paths_clear(&paths);
    test_remove_tree(directory);
    g_free(directory);
}

static void test_empty_analysis_and_cancellation(void)
{
    GError *error = NULL;
    TestFixture *fixture = test_fixture_new(
        "Subject: aucun indicateur\r\n\r\nCorps SPECIMEN.\r\n", -1);
    Database *database = database_open(fixture->database_path);
    EmlAnalysisPublicationResult *empty = test_analyze(database, fixture,
        EML_GRAPH_REQUEST_PRIMARY, EML_GRAPH_DERIVATIVE_PRIMARY, &error);
    g_assert_no_error(error);
    g_assert_nonnull(empty);
    g_assert_cmpuint(eml_analysis_publication_result_get_observation_count(empty),
        ==, 0U);
    g_assert_cmpint(test_count(database, "extractions"), ==, 1);
    g_assert_cmpint(test_count(database, "evidence_entity_observations"), ==, 0);

    EmlAnalysisPersistenceRequest request = {
        .request_identifier = EML_GRAPH_REQUEST_REANALYSIS,
        .source_evidence_identifier = fixture->evidence_identifier,
        .derivative_evidence_identifier = EML_GRAPH_DERIVATIVE_REANALYSIS,
        .requested_at = "2026-09-26T14:01:00Z"
    };
    EmlAnalysisPersistenceService *service =
        eml_analysis_persistence_service_new(database, fixture->root, &error);
    GCancellable *cancellable = g_cancellable_new();
    g_cancellable_cancel(cancellable);
    g_assert_null(eml_analysis_persistence_service_prepare(service, &request,
        cancellable, &error));
    g_assert_error(error, EML_ANALYSIS_PERSISTENCE_ERROR,
        EML_ANALYSIS_PERSISTENCE_ERROR_CANCELLED);
    g_clear_error(&error);
    g_assert_cmpint(test_count(database, "extractions"), ==, 1);
    g_object_unref(cancellable);
    eml_analysis_persistence_service_free(service);
    eml_analysis_publication_result_free(empty);
    database_close(database);
    test_fixture_free(fixture);
}

static void test_derivative_directory_rejects_symlink(void)
{
    GError *error = NULL;
    TestFixture *fixture = test_fixture_new(specimen_eml, -1);
    Database *database = database_open(fixture->database_path);
    char *link_path = g_build_filename(fixture->root,
        "02_Preuves_Traitees", NULL);
    g_assert_cmpint(symlink("01_Preuves_Originales", link_path), ==, 0);
    g_assert_null(test_analyze(database, fixture, EML_GRAPH_REQUEST_PRIMARY,
        EML_GRAPH_DERIVATIVE_PRIMARY, &error));
    g_assert_error(error, EML_ANALYSIS_PERSISTENCE_ERROR,
        EML_ANALYSIS_PERSISTENCE_ERROR_ARTIFACT);
    g_clear_error(&error);
    g_assert_cmpint(test_count(database, "preuves"), ==, 1);
    g_assert_cmpint(test_count(database, "extractions"), ==, 0);
    g_assert_cmpint(g_remove(link_path), ==, 0);
    g_free(link_path);
    database_close(database);
    test_fixture_free(fixture);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/eml-analysis/persistence/replay",
        test_native_analysis_persists_and_replays);
    g_test_add_func("/eml-analysis/persistence/replay-reviewed",
        test_replay_accepts_journalled_review_and_promotion);
    g_test_add_func("/eml-analysis/persistence/rollback",
        test_publication_failure_rolls_back_and_compensates);
    g_test_add_func("/eml-analysis/source/validation",
        test_missing_modified_and_oversize_sources);
    g_test_add_func("/eml-analysis/graph/reanalysis",
        test_specimen_reanalysis_and_projection_failures);
    g_test_add_func("/eml-analysis/analysis/empty-and-cancelled",
        test_empty_analysis_and_cancellation);
    g_test_add_func("/eml-analysis/artifact/symlink",
        test_derivative_directory_rejects_symlink);
    return g_test_run();
}
