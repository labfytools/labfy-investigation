/******************************************************************************
 * @file document_tool_runner.c
 * @brief Exécution bornée et annulable des outils documentaires.
 ******************************************************************************/
#include "core/document_tool_runner.h"
#include "core/file_hash.h"
#include "core/tool_process.h"
#include <glib/gstdio.h>

char *document_tool_runner_read_version(
    const char *executable,
    const char *const arguments[],
    GCancellable *cancellable
)
{
    ToolProcessResult *result = NULL;
    GError *error = NULL;
    char *version = NULL;
    if (!tool_process_run(executable, arguments, NULL, cancellable,
            &result, &error))
    {
        g_clear_error(&error);
        return NULL;
    }
    GBytes *stdout_bytes = tool_process_result_ref_stdout(result);
    GBytes *stderr_bytes = tool_process_result_ref_stderr(result);
    gsize stdout_length = 0;
    gsize stderr_length = 0;
    const char *stdout_data = g_bytes_get_data(
        stdout_bytes, &stdout_length);
    const char *stderr_data = g_bytes_get_data(
        stderr_bytes, &stderr_length);
    if (stdout_data == NULL)
        stdout_data = "";
    if (stderr_data == NULL)
        stderr_data = "";
    if (stdout_length > 0)
        version = g_utf8_make_valid(stdout_data, (gssize) stdout_length);
    else if (stderr_length > 0)
        version = g_utf8_make_valid(stderr_data, (gssize) stderr_length);
    if (version != NULL)
        g_strstrip(version);
    g_bytes_unref(stdout_bytes);
    g_bytes_unref(stderr_bytes);
    tool_process_result_free(result);
    return version;
}

static char *document_tool_runner_bytes_to_text(
    GBytes *bytes,
    gsize limit,
    gboolean *truncated
)
{
    gsize length = 0;
    const char *data = bytes != NULL
        ? g_bytes_get_data(bytes, &length)
        : "";
    if (data == NULL)
        data = "";
    if (length > limit)
    {
        length = limit;
        *truncated = TRUE;
    }
    return g_utf8_make_valid(data, (gssize) length);
}

gboolean document_tool_runner_run(
    const char *tool_id,
    const char *executable,
    const char *const arguments[],
    const char *source_path,
    GCancellable *cancellable,
    DocumentToolExecution **out_execution,
    GError **error
)
{
    ToolProcessResult *process_result = NULL;
    DocumentToolExecution *execution = NULL;
    GError *process_error = NULL;
    gboolean stdout_truncated = FALSE;
    gboolean stderr_truncated = FALSE;

    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    if (tool_id == NULL || executable == NULL || source_path == NULL ||
        out_execution == NULL || *out_execution != NULL)
    {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
            "Les paramètres de l'outil documentaire sont invalides.");
        return FALSE;
    }
    GStatBuf source_stat;
    if (g_stat(source_path, &source_stat) == 0 &&
        source_stat.st_size > DOCUMENT_ANALYSIS_MAX_FILE_SIZE)
    {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NO_SPACE,
            "Le fichier dépasse la taille maximale d'analyse.");
        return FALSE;
    }
    execution = document_tool_execution_new(tool_id, source_path);
    for (gsize index = 0; arguments != NULL &&
         arguments[index] != NULL; index++)
        document_tool_execution_add_argument(execution, arguments[index]);
    (void) file_hash_compute_sha256(source_path, cancellable,
        &execution->source_sha256, NULL, NULL);

    if (!tool_process_run(executable, arguments, NULL, cancellable,
            &process_result, &process_error))
    {
        if (g_error_matches(process_error, TOOL_PROCESS_ERROR,
                TOOL_PROCESS_ERROR_CANCELLED))
        {
            execution->state = DOCUMENT_ANALYSIS_STATE_CANCELLED;
            g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_CANCELLED,
                "L'analyse documentaire a été annulée.");
        }
        else
        {
            execution->state = DOCUMENT_ANALYSIS_STATE_UNAVAILABLE;
            g_ptr_array_add(execution->errors,
                g_strdup(process_error != NULL ? process_error->message :
                    "Outil indisponible."));
        }
        g_clear_error(&process_error);
        GDateTime *now = g_date_time_new_now_utc();
        execution->finished_at_utc = g_date_time_format_iso8601(now);
        g_date_time_unref(now);
        *out_execution = execution;
        return execution->state != DOCUMENT_ANALYSIS_STATE_CANCELLED;
    }

    GBytes *stdout_bytes = tool_process_result_ref_stdout(process_result);
    GBytes *stderr_bytes = tool_process_result_ref_stderr(process_result);
    execution->raw_stdout = document_tool_runner_bytes_to_text(stdout_bytes,
        DOCUMENT_ANALYSIS_MAX_STDOUT, &stdout_truncated);
    execution->raw_stderr = document_tool_runner_bytes_to_text(stderr_bytes,
        DOCUMENT_ANALYSIS_MAX_STDERR, &stderr_truncated);
    execution->exit_status =
        tool_process_result_get_exit_status(process_result);
    if (execution->raw_stdout != NULL)
        execution->raw_stdout_sha256 = g_compute_checksum_for_string(
            G_CHECKSUM_SHA256, execution->raw_stdout, -1);
    if (stdout_truncated || stderr_truncated)
    {
        execution->state = DOCUMENT_ANALYSIS_STATE_PARTIAL;
        g_ptr_array_add(execution->warnings,
            g_strdup("La sortie de l'outil a été tronquée à la limite."));
    }
    else
        execution->state = tool_process_result_is_success(process_result)
            ? DOCUMENT_ANALYSIS_STATE_SUCCESS
            : DOCUMENT_ANALYSIS_STATE_FAILED;
    GDateTime *now = g_date_time_new_now_utc();
    execution->finished_at_utc = g_date_time_format_iso8601(now);
    g_date_time_unref(now);
    g_clear_pointer(&stdout_bytes, g_bytes_unref);
    g_clear_pointer(&stderr_bytes, g_bytes_unref);
    tool_process_result_free(process_result);
    *out_execution = execution;
    return TRUE;
}
