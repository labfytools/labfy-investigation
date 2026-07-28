/******************************************************************************
 * @file document_file_analysis.c
 * @brief Orchestration des analyses compatibles d'un fichier dérivé.
 ******************************************************************************/
#include "core/document_file_analysis.h"

void document_file_analysis_free(DocumentFileAnalysis *analysis)
{
    if (analysis == NULL)
        return;
    g_free(analysis->source_path);
    g_free(analysis->declared_mime);
    g_free(analysis->detected_mime);
    exiftool_analysis_result_free(analysis->metadata);
    ocr_analysis_result_free(analysis->ocr);
    pdf_analysis_result_free(analysis->pdf);
    g_ptr_array_unref(analysis->warnings);
    g_free(analysis);
}

static const char *document_file_analysis_effective_mime(
    const char *declared_mime,
    const char *detected_mime
)
{
    return detected_mime != NULL && detected_mime[0] != '\0'
        ? detected_mime
        : declared_mime;
}

DocumentFileAnalysis *document_file_analysis_run(
    const DocumentAnalysisTools *tools,
    const char *source_path,
    const char *declared_mime,
    const char *detected_mime,
    gboolean request_image_ocr,
    const char *ocr_languages,
    GCancellable *cancellable,
    GError **error
)
{
    if (tools == NULL || source_path == NULL)
    {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
            "Les paramètres d'analyse du fichier sont invalides.");
        return NULL;
    }
    DocumentFileAnalysis *analysis = g_new0(DocumentFileAnalysis, 1);
    analysis->source_path = g_strdup(source_path);
    analysis->declared_mime = g_strdup(declared_mime);
    analysis->detected_mime = g_strdup(detected_mime);
    analysis->warnings = g_ptr_array_new_with_free_func(g_free);
    analysis->state = DOCUMENT_ANALYSIS_STATE_SUCCESS;
    const char *mime = document_file_analysis_effective_mime(
        declared_mime, detected_mime);

    analysis->metadata = exiftool_analysis_run(
        tools->exiftool, source_path, cancellable, error);
    if (analysis->metadata == NULL)
    {
        if (error != NULL && *error != NULL &&
            g_error_matches(*error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            goto failure;
        g_clear_error(error);
        analysis->state = DOCUMENT_ANALYSIS_STATE_PARTIAL;
    }
    else if (analysis->metadata->execution->state !=
             DOCUMENT_ANALYSIS_STATE_SUCCESS)
        analysis->state = DOCUMENT_ANALYSIS_STATE_PARTIAL;

    if (g_strcmp0(mime, "application/pdf") == 0)
    {
        PdfAnalysisTools pdf_tools = {
            .pdfinfo = tools->pdfinfo,
            .pdftotext = tools->pdftotext,
            .pdftoppm = tools->pdftoppm,
            .tesseract = tools->tesseract
        };
        analysis->pdf = pdf_analysis_run(&pdf_tools, source_path,
            ocr_languages, cancellable, error);
        if (analysis->pdf == NULL)
            goto failure;
        if (analysis->pdf->state != DOCUMENT_ANALYSIS_STATE_SUCCESS)
            analysis->state = DOCUMENT_ANALYSIS_STATE_PARTIAL;
    }
    else if (request_image_ocr && ocr_analysis_mime_is_compatible(mime))
    {
        analysis->ocr = ocr_analysis_run(tools->tesseract,
            source_path, ocr_languages, cancellable, error);
        if (analysis->ocr == NULL)
            goto failure;
        if (analysis->ocr->execution->state !=
            DOCUMENT_ANALYSIS_STATE_SUCCESS)
            analysis->state = DOCUMENT_ANALYSIS_STATE_PARTIAL;
    }
    return analysis;

failure:
    document_file_analysis_free(analysis);
    return NULL;
}
