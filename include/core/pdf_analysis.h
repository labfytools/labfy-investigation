/******************************************************************************
 * @file pdf_analysis.h
 * @brief Extraction PDF native puis OCR de secours.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_PDF_ANALYSIS_H
#define LABFY_INVESTIGATION_PDF_ANALYSIS_H

#include "core/document_analysis.h"
#include "core/ocr_analysis.h"

G_BEGIN_DECLS

typedef enum
{
    PDF_PAGE_METHOD_NATIVE,
    PDF_PAGE_METHOD_OCR
} PdfPageMethod;

typedef struct
{
    guint page_number;
    PdfPageMethod method;
    char *text;
    DocumentAnalysisState state;
    DocumentToolExecution *render_execution;
    DocumentToolExecution *execution;
    GPtrArray *warnings;
} PdfPageAnalysis;

typedef struct
{
    const char *pdfinfo;
    const char *pdftotext;
    const char *pdftoppm;
    const char *tesseract;
} PdfAnalysisTools;

typedef struct
{
    char *source_path;
    gboolean encrypted;
    guint page_count;
    char *native_text;
    gboolean native_text_usable;
    DocumentAnalysisState state;
    DocumentToolExecution *pdfinfo_execution;
    DocumentToolExecution *native_execution;
    GPtrArray *pages;
    GPtrArray *warnings;
} PdfAnalysisResult;

gboolean pdf_analysis_text_is_usable(const char *text);
PdfAnalysisResult *pdf_analysis_run(
    const PdfAnalysisTools *tools,
    const char *pdf_path,
    const char *ocr_languages,
    GCancellable *cancellable,
    GError **error
);
void pdf_analysis_result_free(PdfAnalysisResult *result);

G_END_DECLS
#endif
