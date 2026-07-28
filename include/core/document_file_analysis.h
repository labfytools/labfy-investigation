/******************************************************************************
 * @file document_file_analysis.h
 * @brief Orchestration des analyses compatibles d'un fichier dérivé.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_DOCUMENT_FILE_ANALYSIS_H
#define LABFY_INVESTIGATION_DOCUMENT_FILE_ANALYSIS_H

#include "core/exiftool_analysis.h"
#include "core/ocr_analysis.h"
#include "core/pdf_analysis.h"

G_BEGIN_DECLS

typedef struct
{
    const char *exiftool;
    const char *tesseract;
    const char *pdfinfo;
    const char *pdftotext;
    const char *pdftoppm;
} DocumentAnalysisTools;

typedef struct
{
    char *source_path;
    char *declared_mime;
    char *detected_mime;
    ExiftoolAnalysisResult *metadata;
    OcrAnalysisResult *ocr;
    PdfAnalysisResult *pdf;
    DocumentAnalysisState state;
    GPtrArray *warnings;
} DocumentFileAnalysis;

DocumentFileAnalysis *document_file_analysis_run(
    const DocumentAnalysisTools *tools,
    const char *source_path,
    const char *declared_mime,
    const char *detected_mime,
    gboolean request_image_ocr,
    const char *ocr_languages,
    GCancellable *cancellable,
    GError **error
);
void document_file_analysis_free(DocumentFileAnalysis *analysis);

G_END_DECLS
#endif
