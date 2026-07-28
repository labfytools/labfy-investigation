/******************************************************************************
 * @file document_analysis.h
 * @brief Modèles communs pour les analyses documentaires en mémoire.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_DOCUMENT_ANALYSIS_H
#define LABFY_INVESTIGATION_DOCUMENT_ANALYSIS_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define DOCUMENT_ANALYSIS_MAX_FILE_SIZE (50U * 1024U * 1024U)
#define DOCUMENT_ANALYSIS_MAX_STDOUT (8U * 1024U * 1024U)
#define DOCUMENT_ANALYSIS_MAX_STDERR (256U * 1024U)
#define DOCUMENT_ANALYSIS_MAX_TEXT (8U * 1024U * 1024U)
#define DOCUMENT_ANALYSIS_MAX_PDF_PAGES 100U
#define DOCUMENT_ANALYSIS_MAX_PIPELINE_ITEMS 128U

typedef enum
{
    DOCUMENT_ANALYSIS_STATE_SUCCESS,
    DOCUMENT_ANALYSIS_STATE_PARTIAL,
    DOCUMENT_ANALYSIS_STATE_UNAVAILABLE,
    DOCUMENT_ANALYSIS_STATE_CANCELLED,
    DOCUMENT_ANALYSIS_STATE_FAILED,
    DOCUMENT_ANALYSIS_STATE_INCOMPATIBLE
} DocumentAnalysisState;

typedef struct
{
    char *tool_id;
    char *version;
    GPtrArray *arguments;
    char *started_at_utc;
    char *finished_at_utc;
    char *source_path;
    char *source_sha256;
    char *raw_stdout;
    char *raw_stdout_sha256;
    char *raw_stderr;
    int exit_status;
    DocumentAnalysisState state;
    GPtrArray *warnings;
    GPtrArray *errors;
} DocumentToolExecution;

typedef struct
{
    char *code;
    char *original_group;
    char *original_tag;
    char *raw_value;
    gboolean sensitive;
    gboolean requires_confirmation;
} DocumentMetadataEntry;

DocumentToolExecution *document_tool_execution_new(
    const char *tool_id,
    const char *source_path
);
void document_tool_execution_free(DocumentToolExecution *execution);
void document_tool_execution_add_argument(
    DocumentToolExecution *execution,
    const char *argument
);
const char *document_analysis_state_code(DocumentAnalysisState state);

G_END_DECLS
#endif
