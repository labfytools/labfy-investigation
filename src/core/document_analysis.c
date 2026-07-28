/******************************************************************************
 * @file document_analysis.c
 * @brief Modèles communs pour les analyses documentaires en mémoire.
 ******************************************************************************/
#include "core/document_analysis.h"

static char *document_analysis_now_utc(void)
{
    GDateTime *now = g_date_time_new_now_utc();
    char *value = g_date_time_format_iso8601(now);
    g_date_time_unref(now);
    return value;
}

DocumentToolExecution *document_tool_execution_new(
    const char *tool_id,
    const char *source_path
)
{
    if (tool_id == NULL || tool_id[0] == '\0' ||
        source_path == NULL || source_path[0] == '\0')
        return NULL;
    DocumentToolExecution *execution = g_new0(DocumentToolExecution, 1);
    execution->tool_id = g_strdup(tool_id);
    execution->source_path = g_strdup(source_path);
    execution->started_at_utc = document_analysis_now_utc();
    execution->arguments = g_ptr_array_new_with_free_func(g_free);
    execution->warnings = g_ptr_array_new_with_free_func(g_free);
    execution->errors = g_ptr_array_new_with_free_func(g_free);
    execution->exit_status = -1;
    execution->state = DOCUMENT_ANALYSIS_STATE_FAILED;
    return execution;
}

void document_tool_execution_free(DocumentToolExecution *execution)
{
    if (execution == NULL)
        return;
    g_free(execution->tool_id);
    g_free(execution->version);
    g_ptr_array_unref(execution->arguments);
    g_free(execution->started_at_utc);
    g_free(execution->finished_at_utc);
    g_free(execution->source_path);
    g_free(execution->source_sha256);
    g_free(execution->raw_stdout);
    g_free(execution->raw_stdout_sha256);
    g_free(execution->raw_stderr);
    g_ptr_array_unref(execution->warnings);
    g_ptr_array_unref(execution->errors);
    g_free(execution);
}

void document_tool_execution_add_argument(
    DocumentToolExecution *execution,
    const char *argument
)
{
    if (execution != NULL && argument != NULL)
        g_ptr_array_add(execution->arguments, g_strdup(argument));
}

const char *document_analysis_state_code(DocumentAnalysisState state)
{
    switch (state)
    {
        case DOCUMENT_ANALYSIS_STATE_SUCCESS: return "success";
        case DOCUMENT_ANALYSIS_STATE_PARTIAL: return "partial";
        case DOCUMENT_ANALYSIS_STATE_UNAVAILABLE: return "unavailable";
        case DOCUMENT_ANALYSIS_STATE_CANCELLED: return "cancelled";
        case DOCUMENT_ANALYSIS_STATE_INCOMPATIBLE: return "incompatible";
        case DOCUMENT_ANALYSIS_STATE_FAILED: return "failed";
    }
    return "failed";
}
