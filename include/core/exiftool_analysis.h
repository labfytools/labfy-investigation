/******************************************************************************
 * @file exiftool_analysis.h
 * @brief Analyse ExifTool structurée et traçable.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_EXIFTOOL_ANALYSIS_H
#define LABFY_INVESTIGATION_EXIFTOOL_ANALYSIS_H

#include "core/document_analysis.h"

G_BEGIN_DECLS

typedef struct
{
    DocumentToolExecution *execution;
    GPtrArray *metadata;
} ExiftoolAnalysisResult;

ExiftoolAnalysisResult *exiftool_analysis_run(
    const char *executable,
    const char *file_path,
    GCancellable *cancellable,
    GError **error
);
ExiftoolAnalysisResult *exiftool_analysis_parse(
    const char *file_path,
    const char *json,
    const char *stderr_text,
    int exit_status,
    GError **error
);
void exiftool_analysis_result_free(ExiftoolAnalysisResult *result);

G_END_DECLS
#endif
