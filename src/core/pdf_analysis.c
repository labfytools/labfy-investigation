/******************************************************************************
 * @file pdf_analysis.c
 * @brief Extraction PDF native puis OCR de secours.
 ******************************************************************************/
#include "core/pdf_analysis.h"
#include "core/document_tool_runner.h"

#include <glib/gstdio.h>

static void pdf_page_analysis_free(gpointer data)
{
    PdfPageAnalysis *page = data;
    if (page == NULL)
        return;
    g_free(page->text);
    document_tool_execution_free(page->render_execution);
    document_tool_execution_free(page->execution);
    g_ptr_array_unref(page->warnings);
    g_free(page);
}

void pdf_analysis_result_free(PdfAnalysisResult *result)
{
    if (result == NULL)
        return;
    g_free(result->source_path);
    g_free(result->native_text);
    document_tool_execution_free(result->pdfinfo_execution);
    document_tool_execution_free(result->native_execution);
    g_ptr_array_unref(result->pages);
    g_ptr_array_unref(result->warnings);
    g_free(result);
}

gboolean pdf_analysis_text_is_usable(const char *text)
{
    if (text == NULL)
        return FALSE;
    gsize total = 0;
    gsize non_space = 0;
    gsize printable = 0;
    for (const char *cursor = text; *cursor != '\0';
         cursor = g_utf8_next_char(cursor))
    {
        gunichar character = g_utf8_get_char(cursor);
        total++;
        if (!g_unichar_isspace(character))
            non_space++;
        if (g_unichar_isprint(character) || g_unichar_isspace(character))
            printable++;
    }
    return non_space >= 32 && total > 0 &&
        ((double) printable / (double) total) >= 0.70;
}

static guint pdf_analysis_parse_pages(const char *text)
{
    GRegex *regex = g_regex_new("(?im)^Pages:\\s*([0-9]+)", 0, 0, NULL);
    GMatchInfo *match = NULL;
    guint pages = 0;
    g_regex_match(regex, text != NULL ? text : "", 0, &match);
    if (g_match_info_matches(match))
    {
        char *value = g_match_info_fetch(match, 1);
        pages = (guint) g_ascii_strtoull(value, NULL, 10);
        g_free(value);
    }
    g_match_info_free(match);
    g_regex_unref(regex);
    return pages;
}

static gboolean pdf_analysis_parse_encrypted(const char *text)
{
    GRegex *regex = g_regex_new(
        "(?im)^Encrypted:\\s*(yes|oui|true)", 0, 0, NULL);
    gboolean encrypted = g_regex_match(
        regex, text != NULL ? text : "", 0, NULL);
    g_regex_unref(regex);
    return encrypted;
}

static PdfPageAnalysis *pdf_page_new(
    guint page_number,
    PdfPageMethod method,
    const char *text,
    DocumentAnalysisState state
)
{
    PdfPageAnalysis *page = g_new0(PdfPageAnalysis, 1);
    page->page_number = page_number;
    page->method = method;
    page->text = g_strdup(text);
    page->state = state;
    page->warnings = g_ptr_array_new_with_free_func(g_free);
    return page;
}

static void pdf_analysis_add_native_pages(PdfAnalysisResult *result)
{
    char **pages = g_strsplit(result->native_text, "\f", -1);
    guint added = 0;
    for (guint index = 0; pages[index] != NULL; index++)
    {
        if (pages[index][0] == '\0' && pages[index + 1] == NULL)
            break;
        g_ptr_array_add(result->pages, pdf_page_new(
            index + 1, PDF_PAGE_METHOD_NATIVE, pages[index],
            DOCUMENT_ANALYSIS_STATE_SUCCESS));
        added++;
    }
    if (result->page_count == 0)
        result->page_count = added;
    g_strfreev(pages);
}

static gboolean pdf_analysis_render_and_ocr(
    PdfAnalysisResult *result,
    const PdfAnalysisTools *tools,
    const char *languages,
    GCancellable *cancellable,
    GError **error
)
{
    GError *temporary_error = NULL;
    char *temporary_directory = g_dir_make_tmp(
        "labfy-pdf-analysis-XXXXXX", &temporary_error);
    if (temporary_directory == NULL)
    {
        g_propagate_error(error, temporary_error);
        return FALSE;
    }
    guint pages = MIN(result->page_count, DOCUMENT_ANALYSIS_MAX_PDF_PAGES);
    if (result->page_count > DOCUMENT_ANALYSIS_MAX_PDF_PAGES)
    {
        result->state = DOCUMENT_ANALYSIS_STATE_PARTIAL;
        g_ptr_array_add(result->warnings,
            g_strdup("Le nombre de pages PDF dépasse la limite."));
    }

    gboolean success = TRUE;
    for (guint page_number = 1; page_number <= pages; page_number++)
    {
        if (cancellable != NULL &&
            g_cancellable_set_error_if_cancelled(cancellable, error))
        {
            result->state = result->pages->len > 0
                ? DOCUMENT_ANALYSIS_STATE_PARTIAL
                : DOCUMENT_ANALYSIS_STATE_CANCELLED;
            success = FALSE;
            break;
        }
        char *prefix = g_strdup_printf("%s/page-%u",
            temporary_directory, page_number);
        char *page_text = g_strdup_printf("%u", page_number);
        const char *render_arguments[] = {
            "-f", page_text, "-singlefile", "-png",
            result->source_path, prefix, NULL
        };
        DocumentToolExecution *render_execution = NULL;
        if (!document_tool_runner_run("pdftoppm", tools->pdftoppm,
                render_arguments, result->source_path, cancellable,
                &render_execution, error))
        {
            document_tool_execution_free(render_execution);
            g_free(page_text);
            g_free(prefix);
            success = FALSE;
            break;
        }
        char *image_path = g_strconcat(prefix, ".png", NULL);
        if (render_execution->state != DOCUMENT_ANALYSIS_STATE_SUCCESS)
        {
            PdfPageAnalysis *page = pdf_page_new(page_number,
                PDF_PAGE_METHOD_OCR, NULL,
                DOCUMENT_ANALYSIS_STATE_FAILED);
            page->execution = render_execution;
            g_ptr_array_add(result->pages, page);
            result->state = DOCUMENT_ANALYSIS_STATE_PARTIAL;
        }
        else
        {
            OcrAnalysisResult *ocr = ocr_analysis_run(
                tools->tesseract, image_path, languages,
                cancellable, error);
            PdfPageAnalysis *page = pdf_page_new(page_number,
                PDF_PAGE_METHOD_OCR,
                ocr != NULL ? ocr->text : NULL,
                ocr != NULL ? ocr->execution->state :
                    DOCUMENT_ANALYSIS_STATE_FAILED);
            page->render_execution = render_execution;
            if (ocr != NULL)
            {
                page->execution = ocr->execution;
                ocr->execution = NULL;
                ocr_analysis_result_free(ocr);
            }
            g_ptr_array_add(result->pages, page);
            if (page->state != DOCUMENT_ANALYSIS_STATE_SUCCESS)
                result->state = DOCUMENT_ANALYSIS_STATE_PARTIAL;
        }
        g_remove(image_path);
        g_free(image_path);
        g_free(page_text);
        g_free(prefix);
    }
    g_rmdir(temporary_directory);
    g_free(temporary_directory);
    return success;
}

PdfAnalysisResult *pdf_analysis_run(
    const PdfAnalysisTools *tools,
    const char *pdf_path,
    const char *ocr_languages,
    GCancellable *cancellable,
    GError **error
)
{
    if (tools == NULL || pdf_path == NULL || ocr_languages == NULL)
    {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
            "Les paramètres d'analyse PDF sont invalides.");
        return NULL;
    }
    PdfAnalysisResult *result = g_new0(PdfAnalysisResult, 1);
    result->source_path = g_strdup(pdf_path);
    result->pages = g_ptr_array_new_with_free_func(pdf_page_analysis_free);
    result->warnings = g_ptr_array_new_with_free_func(g_free);
    result->state = DOCUMENT_ANALYSIS_STATE_SUCCESS;

    const char *info_arguments[] = { pdf_path, NULL };
    if (!document_tool_runner_run("pdfinfo", tools->pdfinfo,
            info_arguments, pdf_path, cancellable,
            &result->pdfinfo_execution, error))
        goto failure;
    if (result->pdfinfo_execution->state ==
        DOCUMENT_ANALYSIS_STATE_UNAVAILABLE)
    {
        result->state = DOCUMENT_ANALYSIS_STATE_UNAVAILABLE;
        return result;
    }
    const char *version_arguments[] = { "-v", NULL };
    result->pdfinfo_execution->version =
        document_tool_runner_read_version(
            tools->pdfinfo, version_arguments, cancellable);
    result->encrypted = pdf_analysis_parse_encrypted(
        result->pdfinfo_execution->raw_stdout);
    result->page_count = pdf_analysis_parse_pages(
        result->pdfinfo_execution->raw_stdout);
    if (result->encrypted)
    {
        result->state = DOCUMENT_ANALYSIS_STATE_PARTIAL;
        g_ptr_array_add(result->warnings,
            g_strdup("Le PDF est chiffré ; aucun contournement n'est tenté."));
        return result;
    }

    const char *text_arguments[] = {
        "-enc", "UTF-8", "-layout", pdf_path, "-", NULL
    };
    if (!document_tool_runner_run("pdftotext", tools->pdftotext,
            text_arguments, pdf_path, cancellable,
            &result->native_execution, error))
        goto failure;
    result->native_execution->version =
        document_tool_runner_read_version(
            tools->pdftotext, version_arguments, cancellable);
    if (result->native_execution->state ==
        DOCUMENT_ANALYSIS_STATE_SUCCESS)
        result->native_text = g_strdup(
            result->native_execution->raw_stdout);
    result->native_text_usable = pdf_analysis_text_is_usable(
        result->native_text);
    if (result->native_text_usable)
        pdf_analysis_add_native_pages(result);
    else if (!pdf_analysis_render_and_ocr(result, tools, ocr_languages,
            cancellable, error))
    {
        if (error != NULL && *error != NULL &&
            g_error_matches(*error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
            g_clear_error(error);
            return result;
        }
        result->state = DOCUMENT_ANALYSIS_STATE_PARTIAL;
        g_clear_error(error);
    }
    return result;

failure:
    pdf_analysis_result_free(result);
    return NULL;
}
