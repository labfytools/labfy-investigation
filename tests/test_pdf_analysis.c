/******************************************************************************
 * @file test_pdf_analysis.c
 * @brief Tests synthétiques de l'analyse PDF.
 ******************************************************************************/
#include "core/pdf_analysis.h"
#include <glib.h>
#include <glib/gstdio.h>

static PdfAnalysisTools fake_tools(void)
{
    PdfAnalysisTools tools = {
        .pdfinfo = "tests/fake_document_tool",
        .pdftotext = "tests/fake_document_tool",
        .pdftoppm = "tests/fake_document_tool",
        .tesseract = "tests/fake_document_tool"
    };
    return tools;
}

static char *create_pdf(const char *directory, const char *name)
{
    char *path = g_build_filename(directory, name, NULL);
    g_assert_true(g_file_set_contents(path, "%PDF-synthetic", -1, NULL));
    return path;
}

static void test_encrypted_and_native(void)
{
    GError *error = NULL;
    char *directory = g_dir_make_tmp("labfy-pdf-XXXXXX", &error);
    PdfAnalysisTools tools = fake_tools();
    char *encrypted = create_pdf(directory, "encrypted.pdf");
    PdfAnalysisResult *result = pdf_analysis_run(
        &tools, encrypted, "fra", NULL, &error);
    g_assert_no_error(error);
    g_assert_true(result->encrypted);
    g_assert_cmpuint(result->pages->len, ==, 0);
    pdf_analysis_result_free(result);
    char *native = create_pdf(directory, "native.pdf");
    result = pdf_analysis_run(&tools, native, "fra", NULL, &error);
    g_assert_no_error(error);
    g_assert_true(result->native_text_usable);
    g_assert_cmpuint(result->pages->len, ==, 2);
    PdfPageAnalysis *page = g_ptr_array_index(result->pages, 0);
    g_assert_cmpint(page->method, ==, PDF_PAGE_METHOD_NATIVE);
    pdf_analysis_result_free(result);
    g_remove(encrypted);
    g_remove(native);
    g_rmdir(directory);
    g_free(encrypted);
    g_free(native);
    g_free(directory);
}

static void test_ocr_fallback_order_and_cleanup(void)
{
    GError *error = NULL;
    char *directory = g_dir_make_tmp("labfy-pdf-XXXXXX", &error);
    char *scan = create_pdf(directory, "scan.pdf");
    PdfAnalysisTools tools = fake_tools();
    PdfAnalysisResult *result = pdf_analysis_run(
        &tools, scan, "fra+eng", NULL, &error);
    g_assert_no_error(error);
    g_assert_false(result->native_text_usable);
    g_assert_cmpuint(result->pages->len, ==, 2);
    PdfPageAnalysis *first = g_ptr_array_index(result->pages, 0);
    PdfPageAnalysis *second = g_ptr_array_index(result->pages, 1);
    g_assert_cmpuint(first->page_number, ==, 1);
    g_assert_cmpuint(second->page_number, ==, 2);
    g_assert_cmpint(first->method, ==, PDF_PAGE_METHOD_OCR);
    g_assert_nonnull(strstr(second->text, "page deux"));
    pdf_analysis_result_free(result);
    g_remove(scan);
    g_rmdir(directory);
    g_free(scan);
    g_free(directory);
}

static void test_heuristic(void)
{
    g_assert_false(pdf_analysis_text_is_usable(""));
    g_assert_false(pdf_analysis_text_is_usable("court"));
    g_assert_true(pdf_analysis_text_is_usable(
        "Texte synthétique imprimable et suffisamment long pour le test."));
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/pdf-analysis/encrypted-native",
        test_encrypted_and_native);
    g_test_add_func("/pdf-analysis/ocr-fallback-cleanup",
        test_ocr_fallback_order_and_cleanup);
    g_test_add_func("/pdf-analysis/heuristic", test_heuristic);
    return g_test_run();
}
