/******************************************************************************
 * @file ocr_analysis.h
 * @brief OCR Tesseract traçable et annulable.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_OCR_ANALYSIS_H
#define LABFY_INVESTIGATION_OCR_ANALYSIS_H

#include "core/document_analysis.h"
#include "core/document_tool_runner.h"

G_BEGIN_DECLS

typedef struct
{
    DocumentToolExecution *execution;
    char *requested_languages;
    char *text;
} OcrAnalysisResult;

OcrAnalysisResult *ocr_analysis_run(
    const char *executable,
    const char *image_path,
    const char *languages,
    GCancellable *cancellable,
    GError **error
);
OcrAnalysisResult *ocr_analysis_run_with_limits(
    const char *executable,
    const char *image_path,
    const char *languages,
    const DocumentToolRunnerLimits *limits,
    GCancellable *cancellable,
    GError **error
);
void ocr_analysis_result_free(OcrAnalysisResult *result);
gboolean ocr_analysis_mime_is_compatible(const char *mime_type);

G_END_DECLS
#endif
