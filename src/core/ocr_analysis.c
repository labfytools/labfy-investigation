/******************************************************************************
 * @file ocr_analysis.c
 * @brief OCR Tesseract traçable et annulable.
 ******************************************************************************/
#include "core/ocr_analysis.h"
#include "core/document_tool_runner.h"

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
    g_free(result->text);
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
        (!g_str_equal(languages, "fra") &&
         !g_str_equal(languages, "eng") &&
         !g_str_equal(languages, "fra+eng")))
    {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
            "Les paramètres OCR sont invalides.");
        return NULL;
    }
    const char *arguments[] = {
        image_path, "stdout", "-l", languages, NULL
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
    return result;
}
