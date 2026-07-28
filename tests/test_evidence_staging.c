#include "core/evidence_staging.h"
#include <glib.h>
#include <glib/gstdio.h>

static void test_prepare_cleanup(void)
{
    char *directory = g_dir_make_tmp("labfy-staging-test-XXXXXX", NULL);
    char *source = g_build_filename(directory, "SPECIMEN été.png", NULL);
    const char content[] = "\x89PNG\r\n\x1a\nSPECIMEN";
    EvidenceStaging *staging;
    EvidenceStagingResult *result;
    GError *error = NULL;
    g_assert_true(g_file_set_contents(source, content,
        sizeof content - 1, &error));
    staging = evidence_staging_new(&error);
    result = evidence_staging_prepare(staging, source, NULL, &error);
    g_assert_no_error(error);
    g_assert_nonnull(result);
    g_assert_cmpstr(result->mime_type, ==, "image/png");
    g_assert_cmpstr(result->suggested_type, ==, "screenshot");
    g_assert_true(g_file_test(result->staging_path, G_FILE_TEST_EXISTS));
    {
        char *after = NULL;
        gsize length = 0;
        g_assert_true(g_file_get_contents(source, &after, &length, NULL));
        g_assert_cmpmem(after, length, content, sizeof content - 1);
        g_free(after);
    }
    g_assert_true(evidence_staging_remove(
        staging, result->staging_path, &error));
    g_assert_false(g_file_test(result->staging_path, G_FILE_TEST_EXISTS));
    evidence_staging_result_free(result);
    evidence_staging_free(staging);
    g_unlink(source); g_rmdir(directory);
    g_free(source); g_free(directory);
}
static void test_invalid_sources(void)
{
    char *directory = g_dir_make_tmp("labfy-staging-invalid-XXXXXX", NULL);
    char *link_path = g_build_filename(directory, "SPECIMEN-link", NULL);
    EvidenceStaging *staging = evidence_staging_new(NULL);
    GError *error = NULL;
    g_assert_null(evidence_staging_prepare(
        staging, directory, NULL, &error));
    g_clear_error(&error);
    {
        GFile *link_file = g_file_new_for_path(link_path);
        g_assert_true(g_file_make_symbolic_link(
            link_file, directory, NULL, NULL));
        g_object_unref(link_file);
    }
    g_assert_null(evidence_staging_prepare(
        staging, link_path, NULL, &error));
    g_clear_error(&error);
    g_assert_null(evidence_staging_prepare(
        staging, "/tmp/SPECIMEN-absent", NULL, &error));
    g_clear_error(&error);
    evidence_staging_free(staging);
    g_unlink(link_path); g_rmdir(directory);
    g_free(link_path); g_free(directory);
}
int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/evidence-staging/prepare-cleanup",
        test_prepare_cleanup);
    g_test_add_func("/evidence-staging/invalid-sources",
        test_invalid_sources);
    return g_test_run();
}
