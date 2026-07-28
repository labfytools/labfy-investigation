/******************************************************************************
 * @file test_exiftool_analysis.c
 * @brief Tests synthétiques de l'analyse ExifTool.
 ******************************************************************************/
#include "core/exiftool_analysis.h"
#include <glib.h>
#include <glib/gstdio.h>

static void test_parse_and_sensitive_gps(void)
{
    const char *json =
        "[{\"File:MIMEType\":\"image/png\",\"File:FileSize\":42,"
        "\"EXIF:ImageWidth\":10,\"EXIF:GPSLatitude\":48.5,"
        "\"EXIF:GPSLongitude\":2.2,\"EXIF:Unknown\":true}]";
    GError *error = NULL;
    ExiftoolAnalysisResult *result = exiftool_analysis_parse(
        "synthetic.png", json, "synthetic warning", 1, &error);
    g_assert_no_error(error);
    g_assert_nonnull(result);
    g_assert_cmpint(result->execution->state, ==,
        DOCUMENT_ANALYSIS_STATE_PARTIAL);
    g_assert_cmpstr(result->execution->raw_stdout, ==, json);
    g_assert_nonnull(result->execution->raw_stdout_sha256);
    g_assert_cmpuint(result->metadata->len, ==, 6);
    DocumentMetadataEntry *latitude =
        g_ptr_array_index(result->metadata, 3);
    g_assert_cmpstr(latitude->code, ==, "image.gps_latitude");
    g_assert_true(latitude->sensitive);
    g_assert_true(latitude->requires_confirmation);
    DocumentMetadataEntry *unknown =
        g_ptr_array_index(result->metadata, 5);
    g_assert_cmpstr(unknown->code, ==, "metadata.unknown");
    g_assert_cmpstr(unknown->original_tag, ==, "Unknown");
    exiftool_analysis_result_free(result);
}

static void test_invalid_json(void)
{
    GError *error = NULL;
    g_assert_null(exiftool_analysis_parse(
        "synthetic.png", "[{\"broken\":", NULL, 0, &error));
    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
    g_clear_error(&error);
}

static void test_run_and_unavailable(void)
{
    GError *error = NULL;
    char *directory = g_dir_make_tmp("labfy-exif-XXXXXX", &error);
    char *path = g_build_filename(directory, "synthetic.png", NULL);
    g_assert_true(g_file_set_contents(path, "PNG", 3, &error));
    ExiftoolAnalysisResult *result = exiftool_analysis_run(
        "tests/fake_document_tool", path, NULL, &error);
    g_assert_no_error(error);
    g_assert_cmpint(result->execution->state, ==,
        DOCUMENT_ANALYSIS_STATE_SUCCESS);
    g_assert_cmpstr(result->execution->version, ==, "13.00");
    exiftool_analysis_result_free(result);
    result = exiftool_analysis_run(
        "tests/missing_document_tool", path, NULL, &error);
    g_assert_no_error(error);
    g_assert_cmpint(result->execution->state, ==,
        DOCUMENT_ANALYSIS_STATE_UNAVAILABLE);
    exiftool_analysis_result_free(result);
    g_remove(path);
    g_rmdir(directory);
    g_free(path);
    g_free(directory);
}

static gpointer cancel_exiftool(gpointer user_data)
{
    g_usleep(50000);
    g_cancellable_cancel(user_data);
    return NULL;
}

static void test_cancellation_and_truncated_json(void)
{
    GError *error = NULL;
    char *directory = g_dir_make_tmp("labfy-exif-hardening-XXXXXX", &error);
    char *slow_path = g_build_filename(directory, "slow.png", NULL);
    char *large_path = g_build_filename(directory, "large.png", NULL);
    g_assert_true(g_file_set_contents(slow_path, "PNG", 3, &error));
    g_assert_true(g_file_set_contents(large_path, "PNG", 3, &error));

    GCancellable *cancellable = g_cancellable_new();
    GThread *thread = g_thread_new(
        "exif-cancel", cancel_exiftool, cancellable);
    ExiftoolAnalysisResult *result = exiftool_analysis_run(
        "tests/fake_document_tool", slow_path, cancellable, &error);
    g_thread_join(thread);
    g_assert_null(result);
    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
    g_clear_error(&error);
    g_object_unref(cancellable);

    DocumentToolRunnerLimits limits = { 128, 128 };
    result = exiftool_analysis_run_with_limits(
        "tests/fake_document_tool", large_path, &limits, NULL, &error);
    g_assert_no_error(error);
    g_assert_nonnull(result);
    g_assert_true(result->execution->stdout_truncated);
    g_assert_cmpuint(strlen(result->execution->raw_stdout), ==, 128);
    g_assert_cmpuint(result->metadata->len, ==, 0);
    g_assert_cmpint(result->execution->state, ==,
        DOCUMENT_ANALYSIS_STATE_FAILED);
    g_assert_cmpuint(result->execution->warnings->len, >, 0);
    exiftool_analysis_result_free(result);

    g_remove(slow_path);
    g_remove(large_path);
    g_rmdir(directory);
    g_free(slow_path);
    g_free(large_path);
    g_free(directory);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/exiftool-analysis/parse-gps",
        test_parse_and_sensitive_gps);
    g_test_add_func("/exiftool-analysis/invalid-json", test_invalid_json);
    g_test_add_func("/exiftool-analysis/run-unavailable",
        test_run_and_unavailable);
    g_test_add_func("/exiftool-analysis/cancellation-truncated",
        test_cancellation_and_truncated_json);
    return g_test_run();
}
