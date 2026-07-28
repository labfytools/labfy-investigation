/******************************************************************************
 * @file test_ocr_analysis.c
 * @brief Tests synthétiques de l'OCR.
 ******************************************************************************/
#include "core/ocr_analysis.h"
#include <glib.h>
#include <glib/gstdio.h>

static void test_languages_and_raw_text(void)
{
    GError *error = NULL;
    char *directory = g_dir_make_tmp("labfy-ocr-XXXXXX", &error);
    char *path = g_build_filename(directory, "synthetic.png", NULL);
    g_assert_true(g_file_set_contents(path, "PNG", 3, &error));
    const char *languages[] = { "fra", "eng", "fra+eng" };
    for (guint index = 0; index < G_N_ELEMENTS(languages); index++)
    {
        OcrAnalysisResult *result = ocr_analysis_run(
            "tests/fake_document_tool", path, languages[index],
            NULL, &error);
        g_assert_no_error(error);
        g_assert_cmpstr(result->requested_languages, ==, languages[index]);
        g_assert_cmpstr(result->text, ==,
            "Texte OCR synthétique page une.\n");
        g_assert_cmpstr(result->execution->raw_stdout, ==, result->text);
        g_assert_nonnull(result->execution->version);
        ocr_analysis_result_free(result);
    }
    g_remove(path);
    g_rmdir(directory);
    g_free(path);
    g_free(directory);
}

static void test_unavailable_and_compatibility(void)
{
    GError *error = NULL;
    OcrAnalysisResult *result = ocr_analysis_run(
        "tests/missing_document_tool", "synthetic.png", "fra",
        NULL, &error);
    g_assert_no_error(error);
    g_assert_cmpint(result->execution->state, ==,
        DOCUMENT_ANALYSIS_STATE_UNAVAILABLE);
    ocr_analysis_result_free(result);
    g_assert_true(ocr_analysis_mime_is_compatible("image/png"));
    g_assert_true(ocr_analysis_mime_is_compatible("image/tiff"));
    g_assert_false(ocr_analysis_mime_is_compatible("text/plain"));
}

static gpointer cancel_ocr(gpointer user_data)
{
    g_usleep(50000);
    g_cancellable_cancel(user_data);
    return NULL;
}

static void test_cancellation(void)
{
    GError *error = NULL;
    char *directory = g_dir_make_tmp("labfy-ocr-XXXXXX", &error);
    char *path = g_build_filename(directory, "sleep.png", NULL);
    g_assert_true(g_file_set_contents(path, "PNG", 3, &error));
    GCancellable *cancellable = g_cancellable_new();
    GThread *thread = g_thread_new("ocr-cancel", cancel_ocr, cancellable);
    OcrAnalysisResult *result = ocr_analysis_run(
        "tests/fake_document_tool", path, "fra", cancellable, &error);
    g_thread_join(thread);
    g_assert_null(result);
    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
    g_clear_error(&error);
    g_object_unref(cancellable);
    g_remove(path);
    g_rmdir(directory);
    g_free(path);
    g_free(directory);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/ocr-analysis/languages-raw",
        test_languages_and_raw_text);
    g_test_add_func("/ocr-analysis/unavailable-compatible",
        test_unavailable_and_compatibility);
    g_test_add_func("/ocr-analysis/cancellation", test_cancellation);
    return g_test_run();
}
