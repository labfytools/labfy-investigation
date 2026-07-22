/******************************************************************************
 * @file test_osint_dns_integration.c
 * @brief Tests de l'intégration transactionnelle des propositions DNS.
 ******************************************************************************/

#include "core/osint_dns_integration.h"

#include "dao/entity_dao.h"
#include "database/database.h"
#include "models/osint_dns_proposal.h"

#include <glib.h>
#include <glib/gstdio.h>

static void test_integration_and_duplicates(void)
{
    GError *error = NULL;
    char *directory = g_dir_make_tmp("labfy-dns-integration-XXXXXX", &error);
    char *database_path = g_build_filename(directory, "Enquete.sqlite", NULL);
    Database *database = NULL;
    EntityDao *entity_dao = NULL;
    GPtrArray *proposals = NULL;
    GPtrArray *entities = NULL;
    guint inserted = 0U;
    guint skipped = 0U;

    g_assert_no_error(error);
    g_assert_true(database_initialize(database_path, "DNS", directory));
    database = database_open(database_path);
    proposals = osint_dns_proposal_parse(
        "example.org",
        "example.org. 60 IN A 192.0.2.10\n"
        "example.org. 60 IN CNAME WWW.Example.ORG.\n"
        "example.org. 60 IN MX 10 mail.example.org.\n"
    );
    g_assert_true(osint_dns_integration_apply(
        database, proposals, &inserted, &skipped, &error
    ));
    g_assert_no_error(error);
    g_assert_cmpuint(inserted, ==, 2);
    g_assert_cmpuint(skipped, ==, 1);

    g_assert_true(osint_dns_integration_apply(
        database, proposals, &inserted, &skipped, &error
    ));
    g_assert_cmpuint(inserted, ==, 0);
    g_assert_cmpuint(skipped, ==, 3);
    entity_dao = entity_dao_new(database, &error);
    entities = entity_dao_list_all(entity_dao, &error);
    g_assert_cmpuint(entities->len, ==, 2);

    g_ptr_array_unref(entities);
    entity_dao_free(entity_dao);
    g_ptr_array_unref(proposals);
    database_close(database);
    g_assert_cmpint(g_remove(database_path), ==, 0);
    g_assert_cmpint(g_rmdir(directory), ==, 0);
    g_free(database_path);
    g_free(directory);
}

static void test_invalid_arguments(void)
{
    GError *error = NULL;
    g_assert_false(osint_dns_integration_apply(
        NULL, NULL, NULL, NULL, &error
    ));
    g_assert_error(
        error, OSINT_DNS_INTEGRATION_ERROR,
        OSINT_DNS_INTEGRATION_ERROR_INVALID_ARGUMENT
    );
    g_clear_error(&error);
}

static void test_normalized_existing_domain_is_skipped(void)
{
    GError *error = NULL;
    char *directory = g_dir_make_tmp("labfy-dns-existing-XXXXXX", &error);
    char *database_path = g_build_filename(directory, "Enquete.sqlite", NULL);
    Database *database = NULL;
    EntityDao *entity_dao = NULL;
    EntityRecord *existing = NULL;
    GPtrArray *proposals = NULL;
    guint inserted = 0U;
    guint skipped = 0U;

    g_assert_true(database_initialize(database_path, "DNS", directory));
    database = database_open(database_path);
    entity_dao = entity_dao_new(database, &error);
    existing = entity_record_new(
        "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa", "domain_name",
        "WWW.Example.ORG.", NULL, NULL, 50, "2026-01-01T00:00:00Z",
        "2026-01-01T00:00:00Z", ENTITY_STATUS_ACTIVE, &error
    );
    g_assert_true(entity_dao_insert(entity_dao, existing, &error));
    proposals = osint_dns_proposal_parse(
        "example.org", "example.org. 60 IN CNAME www.example.org.\n"
    );
    g_assert_true(osint_dns_integration_apply(
        database, proposals, &inserted, &skipped, &error
    ));
    g_assert_cmpuint(inserted, ==, 0);
    g_assert_cmpuint(skipped, ==, 1);

    g_ptr_array_unref(proposals);
    entity_record_free(existing);
    entity_dao_free(entity_dao);
    database_close(database);
    g_assert_cmpint(g_remove(database_path), ==, 0);
    g_assert_cmpint(g_rmdir(directory), ==, 0);
    g_free(database_path);
    g_free(directory);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/osint-dns-integration/apply", test_integration_and_duplicates);
    g_test_add_func("/osint-dns-integration/invalid", test_invalid_arguments);
    g_test_add_func(
        "/osint-dns-integration/normalized-duplicate",
        test_normalized_existing_domain_is_skipped
    );
    return g_test_run();
}
