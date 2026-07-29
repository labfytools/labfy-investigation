/******************************************************************************
 * @file ocr_analysis.c
 * @brief OCR Tesseract traçable et annulable.
 ******************************************************************************/
#include "core/ocr_analysis.h"
#include "core/document_tool_runner.h"

static gint compare_language_codes(gconstpointer left, gconstpointer right)
{
    return g_strcmp0(*(const char *const *) left,
        *(const char *const *) right);
}

gboolean ocr_analysis_mime_is_compatible(const char *mime_type)
{
    return g_strcmp0(mime_type, "image/png") == 0 ||
        g_strcmp0(mime_type, "image/jpeg") == 0 ||
        g_strcmp0(mime_type, "image/tiff") == 0;
}

void ocr_analysis_result_free(OcrAnalysisResult *result)
{
    if (result == NULL)
        return;
    document_tool_execution_free(result->execution);
    g_free(result->requested_languages);
    g_free(result->available_languages);
    g_free(result->text);
    g_free(result->tsv);
    g_free(result);
}

OcrAnalysisResult *ocr_analysis_run(
    const char *executable,
    const char *image_path,
    const char *languages,
    GCancellable *cancellable,
    GError **error
)
{
    const DocumentToolRunnerLimits limits = {
        DOCUMENT_ANALYSIS_MAX_TEXT,
        DOCUMENT_ANALYSIS_MAX_STDERR
    };
    return ocr_analysis_run_with_limits(executable, image_path, languages,
        &limits, cancellable, error);
}

OcrAnalysisResult *ocr_analysis_run_with_limits(
    const char *executable,
    const char *image_path,
    const char *languages,
    const DocumentToolRunnerLimits *limits,
    GCancellable *cancellable,
    GError **error
)
{
    if (executable == NULL || image_path == NULL || languages == NULL ||
        languages[0] == '\0' ||
        strpbrk(languages, " \t\r\n/\\") != NULL)
    {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
            "Les paramètres OCR sont invalides.");
        return NULL;
    }
    const char *arguments[] = {
        image_path, "stdout", "-l", languages, NULL
    };
    const char *tsv_arguments[] = {
        image_path, "stdout", "-l", languages, "tsv", NULL
    };
    const char *version_arguments[] = { "--version", NULL };
    DocumentToolExecution *execution = NULL;
    if (!document_tool_runner_run_with_limits("tesseract", executable,
            arguments, image_path, limits, cancellable, &execution, error))
    {
        document_tool_execution_free(execution);
        return NULL;
    }
    OcrAnalysisResult *result = g_new0(OcrAnalysisResult, 1);
    result->execution = execution;
    execution->version = document_tool_runner_read_version(
        executable, version_arguments, cancellable);
    result->requested_languages = g_strdup(languages);
    if (execution->raw_stdout != NULL)
    {
        gsize length = strlen(execution->raw_stdout);
        if (length > DOCUMENT_ANALYSIS_MAX_TEXT)
        {
            result->text = g_strndup(execution->raw_stdout,
                DOCUMENT_ANALYSIS_MAX_TEXT);
            execution->state = DOCUMENT_ANALYSIS_STATE_PARTIAL;
            g_ptr_array_add(execution->warnings,
                g_strdup("Le texte OCR dépasse la limite autorisée."));
        }
        else
            result->text = g_strdup(execution->raw_stdout);
    }
    if (execution->state == DOCUMENT_ANALYSIS_STATE_SUCCESS &&
        (result->text == NULL || result->text[0] == '\0'))
    {
        execution->state = DOCUMENT_ANALYSIS_STATE_PARTIAL;
        g_ptr_array_add(execution->warnings,
            g_strdup("Tesseract n'a produit aucun texte."));
    }
    DocumentToolExecution *tsv_execution = NULL;
    if (!document_tool_runner_run_with_limits("tesseract", executable,
            tsv_arguments, image_path, limits, cancellable,
            &tsv_execution, error)) {
        ocr_analysis_result_free(result);
        return NULL;
    }
    result->tsv = g_strdup(tsv_execution->raw_stdout);
    document_tool_execution_free(tsv_execution);
    return result;
}

GPtrArray *ocr_analysis_parse_languages(const char *output)
{
    GPtrArray *items = g_ptr_array_new_with_free_func(g_free);
    if (output == NULL) return items;
    char **lines = g_strsplit(output, "\n", -1);
    for (guint i = 0; lines[i] != NULL; i++) {
        char *code = g_strstrip(lines[i]);
        if (*code == '\0' || g_str_has_prefix(code,
                "List of available languages")) continue;
        gboolean valid = TRUE;
        for (const char *p = code; *p != '\0'; p++)
            if (!g_ascii_isalnum(*p) && *p != '_' && *p != '-') valid = FALSE;
        gboolean duplicate = FALSE;
        for (guint j = 0; valid && j < items->len; j++)
            if (g_str_equal(code, g_ptr_array_index(items, j)))
                duplicate = TRUE;
        if (valid && !duplicate) g_ptr_array_add(items, g_strdup(code));
    }
    g_strfreev(lines);
    g_ptr_array_sort(items, compare_language_codes);
    return items;
}

GPtrArray *ocr_analysis_build_language_choices(const GPtrArray *languages)
{
    GPtrArray *choices = g_ptr_array_new_with_free_func(g_free);
    gboolean fra = FALSE, eng = FALSE;
    for (guint i = 0; languages != NULL && i < languages->len; i++) {
        const char *code = g_ptr_array_index((GPtrArray *) languages, i);
        g_ptr_array_add(choices, g_strdup(code));
        fra |= g_str_equal(code, "fra");
        eng |= g_str_equal(code, "eng");
    }
    if (fra && eng) g_ptr_array_add(choices, g_strdup("fra+eng"));
    return choices;
}

char *ocr_analysis_list_languages(const char *executable,
    GCancellable *cancellable, GError **error)
{
    const char *arguments[] = {"--list-langs", NULL};
    const DocumentToolRunnerLimits limits = {65536U, 16384U};
    DocumentToolExecution *execution = NULL;
    char *languages = NULL;
    if (executable == NULL ||
        !document_tool_runner_run_with_limits("tesseract", executable,
            arguments, executable, &limits, cancellable, &execution, error))
        return NULL;
    languages = g_strdup(execution->raw_stdout);
    document_tool_execution_free(execution);
    return languages;
}
