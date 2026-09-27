#include "core/local_correlation_service.h"
#include "dao/evidence_dao.h"
#include "dao/evidence_entity_dao.h"
#include "database/transaction.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <json-glib/json-glib.h>

#define INVESTIGATION "88000000-0000-4000-8000-000000000001"

typedef struct { char *directory; char *path; Database *database; } Fixture;

static void add_evidence(Fixture *fixture, guint rank)
{
    char *identifier = g_strdup_printf("88000000-0000-4000-8000-%012u", rank + 10U);
    char *name = g_strdup_printf("SPECIMEN-%u.eml", rank);
    char *sha = g_strdup_printf("%064u", rank);
    GError *error = NULL;
    EvidenceRecord *record = evidence_record_new(identifier, name, name, name,
        "email", 100U + rank, sha, "2026-09-26T00:00:00Z", NULL,
        "SPECIMEN", NULL, EVIDENCE_INTEGRITY_STATUS_VALID, &error);
    g_assert_no_error(error); g_assert_nonnull(record);
    EvidenceDao *dao = evidence_dao_new(fixture->database, &error);
    g_assert_true(evidence_dao_insert(dao, record, &error));
    EvidenceEntityDao *observations = evidence_entity_dao_new(fixture->database, &error);
    char *observation_identifier = NULL;
    const char *email = rank < 2U ? "same@example.test" : "other@example.test";
    g_assert_true(evidence_entity_dao_add_observation(observations, identifier,
        "email_address", email, email, "from", "header",
        "from", 1U, "proposed", "2026-09-26T00:00:01Z",
        &observation_identifier, &error));
    g_clear_pointer(&observation_identifier, g_free);
    g_assert_true(evidence_entity_dao_add_observation(observations, identifier,
        "domain_name", "relay.example.test", "relay.example.test", "relay",
        "header", "received", 1U, "proposed",
        "2026-09-26T00:00:01Z", &observation_identifier, &error));
    g_clear_pointer(&observation_identifier, g_free);
    g_assert_no_error(error); evidence_entity_dao_free(observations);
    evidence_dao_free(dao); evidence_record_free(record);
    g_free(sha); g_free(name); g_free(identifier);
}

static void setup_service(Fixture *fixture, gconstpointer unused)
{
    (void)unused; GError *error = NULL;
    fixture->directory = g_dir_make_tmp("labfy-correlation-XXXXXX", &error);
    g_assert_no_error(error);
    fixture->path = g_build_filename(fixture->directory, "Enquete.sqlite", NULL);
    g_assert_true(database_initialize(fixture->path, "SPECIMEN", fixture->directory));
    fixture->database = database_open(fixture->path); g_assert_nonnull(fixture->database);
    for (guint i = 0; i < 4U; i++) add_evidence(fixture, i);
}

static void teardown_service(Fixture *fixture, gconstpointer unused)
{
    (void)unused; database_close(fixture->database); g_remove(fixture->path);
    g_rmdir(fixture->directory); g_free(fixture->path); g_free(fixture->directory);
}

static JsonParser *build_json(Database *database, LocalCorrelationLimits limits,
                              GBytes **out_bytes)
{
    GError *error = NULL; *out_bytes = local_correlation_service_build(
        database, INVESTIGATION, limits, &error);
    g_assert_no_error(error); g_assert_nonnull(*out_bytes);
    gsize size = 0U; const char *data = g_bytes_get_data(*out_bytes, &size);
    JsonParser *parser = json_parser_new();
    g_assert_true(json_parser_load_from_data(parser, data, (gssize)size, &error));
    g_assert_no_error(error); return parser;
}

static void test_service_limit_boundaries(Fixture *fixture, gconstpointer unused)
{
    (void)unused; LocalCorrelationLimits exact = {8U, 3U, 2U, 1024U * 1024U};
    GBytes *bytes = NULL; JsonParser *parser = build_json(fixture->database, exact, &bytes);
    JsonObject *root = json_node_get_object(json_parser_get_root(parser));
    g_assert_true(json_object_get_boolean_member(root, "complete"));
    g_assert_cmpuint(json_array_get_length(json_object_get_array_member(root, "observations")), ==, 8U);
    g_assert_cmpuint(json_array_get_length(json_object_get_array_member(root, "groups")), ==, 3U);
    g_assert_cmpuint(json_array_get_length(json_object_get_array_member(root, "connections")), ==, 2U);
    const char *revision = json_object_get_string_member(root, "revision");
    char *exact_revision = g_strdup(revision); gsize exact_size = g_bytes_get_size(bytes);
    g_object_unref(parser); g_bytes_unref(bytes);

    const LocalCorrelationLimits limited[] = {
        {7U, 3U, 2U, 1024U * 1024U}, {8U, 2U, 2U, 1024U * 1024U},
        {8U, 3U, 1U, 1024U * 1024U}};
    for (guint i = 0; i < G_N_ELEMENTS(limited); i++) {
        parser = build_json(fixture->database, limited[i], &bytes);
        root = json_node_get_object(json_parser_get_root(parser));
        g_assert_false(json_object_get_boolean_member(root, "complete"));
        g_assert_cmpstr(json_object_get_string_member(root, "revision"), !=, exact_revision);
        JsonArray *groups = json_object_get_array_member(root, "groups");
        JsonArray *connections = json_object_get_array_member(root, "connections");
        if (json_array_get_length(groups) < 2U)
            g_assert_cmpuint(json_array_get_length(connections), ==, 0U);
        g_object_unref(parser); g_bytes_unref(bytes);
    }
    GError *error = NULL;
    g_assert_null(local_correlation_service_build(fixture->database, INVESTIGATION,
        (LocalCorrelationLimits){8U, 3U, 2U, exact_size - 1U}, &error));
    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_NO_SPACE); g_clear_error(&error);
    g_free(exact_revision);
}

static void test_service_transactions_and_atomic_export(Fixture *fixture,
                                                         gconstpointer unused)
{
    (void)unused; LocalCorrelationLimits limits = {8U, 3U, 2U, 1024U * 1024U};
    GBytes *bytes = NULL; JsonParser *parser = build_json(fixture->database, limits, &bytes);
    g_assert_false(database_transaction_is_active(fixture->database));
    g_assert_true(database_transaction_begin_read_only(fixture->database));
    GBytes *borrowed = local_correlation_service_build(fixture->database,
        INVESTIGATION, limits, NULL);
    g_assert_nonnull(borrowed); g_assert_true(database_transaction_is_active(fixture->database));
    g_assert_true(database_transaction_rollback(fixture->database));
    char *export = g_build_filename(fixture->directory, "correlations.json", NULL);
    GError *error = NULL; g_assert_true(local_correlation_snapshot_write_atomic(bytes, export, &error));
    char *before = NULL; gsize before_size = 0U;
    g_assert_true(g_file_get_contents(export, &before, &before_size, &error));
    g_assert_false(local_correlation_snapshot_write_atomic(NULL, export, &error));
    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT); g_clear_error(&error);
    char *after = NULL; gsize after_size = 0U;
    g_assert_true(g_file_get_contents(export, &after, &after_size, &error));
    g_assert_cmpuint(after_size, ==, before_size); g_assert_cmpmem(after, after_size, before, before_size);
    g_free(after); g_free(before); g_remove(export); g_free(export);
    g_bytes_unref(borrowed); g_bytes_unref(bytes); g_object_unref(parser);
    g_assert_null(local_correlation_service_build(NULL, INVESTIGATION, limits, NULL));
}

static void test_typed_normalization(void)
{
    char *reason = NULL;
    char *value = local_correlation_normalize("email_address",
        "User.Name+case@EXAMPLE.TEST", &reason);
    g_assert_cmpstr(value, ==, "User.Name+case@example.test");
    g_assert_null(reason); g_free(value);
    value = local_correlation_normalize("domain_name", "Relay.Example.TEST.",
        &reason);
    g_assert_cmpstr(value, ==, "relay.example.test");
    g_assert_null(reason); g_free(value);
    value = local_correlation_normalize("ip_address", "2001:0db8::1", &reason);
    g_assert_cmpstr(value, ==, "2001:db8::1");
    g_assert_null(reason); g_free(value);
}

static void test_prudent_rejections(void)
{
    char *reason = NULL;
    char *value = local_correlation_normalize("email_address",
        "display <user@example.test>", &reason);
    g_assert_null(value); g_assert_nonnull(reason); g_clear_pointer(&reason, g_free);
    value = local_correlation_normalize("domain_name", "localhost", &reason);
    g_assert_null(value); g_assert_nonnull(reason); g_clear_pointer(&reason, g_free);
    value = local_correlation_normalize("ip_address", "999.1.1.1", &reason);
    g_assert_null(value); g_assert_nonnull(reason); g_clear_pointer(&reason, g_free);
    value = local_correlation_normalize("person_name", "Alice", &reason);
    g_assert_null(value); g_assert_nonnull(reason); g_free(reason);
    const char *invalid[] = {"a@b@example.test", "a..b.test", "-a.test",
        "a-.test", "a .test", "a\001.test", NULL};
    for (guint i = 0; invalid[i] != NULL; i++) {
        reason = NULL;
        value = local_correlation_normalize(
            i == 0 ? "email_address" : "domain_name", invalid[i], &reason);
        g_assert_null(value); g_assert_nonnull(reason); g_free(reason);
    }
    char *long_label = g_strdup_printf("%064d.test", 0);
    reason = NULL; value = local_correlation_normalize("domain_name",
        long_label, &reason);
    g_assert_null(value); g_assert_nonnull(reason); g_free(reason);
    g_free(long_label);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/local-correlation/normalization/typed",
        test_typed_normalization);
    g_test_add_func("/local-correlation/normalization/rejections",
        test_prudent_rejections);
    g_test_add("/local-correlation/service/limit-boundaries", Fixture, NULL,
        setup_service, test_service_limit_boundaries, teardown_service);
    g_test_add("/local-correlation/service/transactions-atomic-export", Fixture,
        NULL, setup_service, test_service_transactions_and_atomic_export,
        teardown_service);
    return g_test_run();
}
