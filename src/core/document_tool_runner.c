/******************************************************************************
 * @file document_tool_runner.c
 * @brief Exécution réellement bornée et annulable des outils documentaires.
 ******************************************************************************/
#include "core/document_tool_runner.h"
#include "core/file_hash.h"

#include <glib/gstdio.h>

#define DOCUMENT_TOOL_RUNNER_READ_BLOCK 4096U
#define DOCUMENT_TOOL_RUNNER_VERSION_LIMIT 65536U

typedef struct
{
    GInputStream *stream;
    GCancellable *cancellable;
    GByteArray *prefix;
    gsize limit;
    gsize bytes_observed;
    gboolean truncated;
    GError *error;
} DocumentToolStreamCapture;

typedef struct
{
    GBytes *stdout_bytes;
    GBytes *stderr_bytes;
    gsize stdout_bytes_observed;
    gsize stderr_bytes_observed;
    gboolean stdout_truncated;
    gboolean stderr_truncated;
    gboolean exited_normally;
    int exit_status;
} DocumentToolCaptureResult;

static GPtrArray *document_tool_runner_build_argv(
    const char *executable,
    const char *const arguments[]
)
{
    GPtrArray *argv = g_ptr_array_new();
    g_ptr_array_add(argv, (gpointer) executable);
    for (gsize index = 0; arguments != NULL &&
         arguments[index] != NULL; index++)
        g_ptr_array_add(argv, (gpointer) arguments[index]);
    g_ptr_array_add(argv, NULL);
    return argv;
}

static gpointer document_tool_runner_drain_stream(gpointer user_data)
{
    DocumentToolStreamCapture *capture = user_data;
    guint8 block[DOCUMENT_TOOL_RUNNER_READ_BLOCK];

    while (TRUE)
    {
        gssize bytes_read = g_input_stream_read(
            capture->stream, block, sizeof(block),
            capture->cancellable, &capture->error);
        if (bytes_read <= 0)
            break;

        gsize observed = (gsize) bytes_read;
        if (G_MAXSIZE - capture->bytes_observed < observed)
            capture->bytes_observed = G_MAXSIZE;
        else
            capture->bytes_observed += observed;

        gsize remaining = capture->prefix->len < capture->limit
            ? capture->limit - capture->prefix->len : 0;
        gsize retained = MIN(remaining, observed);
        if (retained > 0)
            g_byte_array_append(capture->prefix, block, retained);
        if (retained < observed)
            capture->truncated = TRUE;
    }
    return NULL;
}

static void document_tool_capture_result_clear(
    DocumentToolCaptureResult *result
)
{
    g_clear_pointer(&result->stdout_bytes, g_bytes_unref);
    g_clear_pointer(&result->stderr_bytes, g_bytes_unref);
}

static gboolean document_tool_runner_capture(
    const char *executable,
    const char *const arguments[],
    const DocumentToolRunnerLimits *limits,
    GCancellable *cancellable,
    DocumentToolCaptureResult *out_result,
    GError **error
)
{
    GPtrArray *argv = document_tool_runner_build_argv(executable, arguments);
    GSubprocessLauncher *launcher = g_subprocess_launcher_new(
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE);
    GError *local_error = NULL;
    GSubprocess *process = g_subprocess_launcher_spawnv(
        launcher, (const char *const *) argv->pdata, &local_error);
    g_object_unref(launcher);
    g_ptr_array_unref(argv);
    if (process == NULL)
    {
        g_propagate_error(error, local_error);
        return FALSE;
    }

    DocumentToolStreamCapture stdout_capture = {
        .stream = g_subprocess_get_stdout_pipe(process),
        .cancellable = cancellable,
        .prefix = g_byte_array_sized_new(MIN(limits->stdout_limit, 4096U)),
        .limit = limits->stdout_limit
    };
    DocumentToolStreamCapture stderr_capture = {
        .stream = g_subprocess_get_stderr_pipe(process),
        .cancellable = cancellable,
        .prefix = g_byte_array_sized_new(MIN(limits->stderr_limit, 4096U)),
        .limit = limits->stderr_limit
    };
    GThread *stdout_thread = g_thread_new(
        "document-stdout", document_tool_runner_drain_stream, &stdout_capture);
    GThread *stderr_thread = g_thread_new(
        "document-stderr", document_tool_runner_drain_stream, &stderr_capture);

    gboolean waited = g_subprocess_wait(process, cancellable, &local_error);
    if (!waited)
    {
        g_subprocess_force_exit(process);
        GError *final_wait_error = NULL;
        (void) g_subprocess_wait(process, NULL, &final_wait_error);
        g_clear_error(&final_wait_error);
    }
    g_thread_join(stdout_thread);
    g_thread_join(stderr_thread);

    gboolean cancelled =
        (cancellable != NULL && g_cancellable_is_cancelled(cancellable)) ||
        g_error_matches(local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED) ||
        g_error_matches(stdout_capture.error, G_IO_ERROR, G_IO_ERROR_CANCELLED) ||
        g_error_matches(stderr_capture.error, G_IO_ERROR, G_IO_ERROR_CANCELLED);

    if (!waited || stdout_capture.error != NULL ||
        stderr_capture.error != NULL)
    {
        if (cancelled)
            g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_CANCELLED,
                "L'exécution de l'outil documentaire a été annulée.");
        else if (local_error != NULL)
            g_propagate_error(error, g_steal_pointer(&local_error));
        else
            g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                "La lecture des sorties de l'outil documentaire a échoué.");
        g_clear_error(&local_error);
        g_clear_error(&stdout_capture.error);
        g_clear_error(&stderr_capture.error);
        g_byte_array_unref(stdout_capture.prefix);
        g_byte_array_unref(stderr_capture.prefix);
        g_object_unref(process);
        return FALSE;
    }

    out_result->stdout_bytes = g_byte_array_free_to_bytes(
        stdout_capture.prefix);
    out_result->stderr_bytes = g_byte_array_free_to_bytes(
        stderr_capture.prefix);
    out_result->stdout_bytes_observed = stdout_capture.bytes_observed;
    out_result->stderr_bytes_observed = stderr_capture.bytes_observed;
    out_result->stdout_truncated = stdout_capture.truncated;
    out_result->stderr_truncated = stderr_capture.truncated;
    out_result->exited_normally = g_subprocess_get_if_exited(process);
    out_result->exit_status = out_result->exited_normally
        ? g_subprocess_get_exit_status(process) : -1;
    g_clear_error(&local_error);
    g_object_unref(process);
    return TRUE;
}

static char *document_tool_runner_bytes_to_text(GBytes *bytes)
{
    gsize length = 0;
    const char *data = g_bytes_get_data(bytes, &length);
    return g_utf8_make_valid(data != NULL ? data : "", (gssize) length);
}

char *document_tool_runner_read_version(
    const char *executable,
    const char *const arguments[],
    GCancellable *cancellable
)
{
    DocumentToolRunnerLimits limits = {
        DOCUMENT_TOOL_RUNNER_VERSION_LIMIT,
        DOCUMENT_TOOL_RUNNER_VERSION_LIMIT
    };
    DocumentToolCaptureResult capture = { 0 };
    GError *error = NULL;
    if (!document_tool_runner_capture(executable, arguments, &limits,
            cancellable, &capture, &error))
    {
        g_clear_error(&error);
        return NULL;
    }
    char *version = document_tool_runner_bytes_to_text(
        g_bytes_get_size(capture.stdout_bytes) > 0
            ? capture.stdout_bytes : capture.stderr_bytes);
    g_strstrip(version);
    document_tool_capture_result_clear(&capture);
    return version;
}

gboolean document_tool_runner_run_with_limits(
    const char *tool_id,
    const char *executable,
    const char *const arguments[],
    const char *source_path,
    const DocumentToolRunnerLimits *limits,
    GCancellable *cancellable,
    DocumentToolExecution **out_execution,
    GError **error
)
{
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    if (tool_id == NULL || executable == NULL || source_path == NULL ||
        limits == NULL || limits->stdout_limit == 0 ||
        limits->stderr_limit == 0 || out_execution == NULL ||
        *out_execution != NULL)
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

    DocumentToolExecution *execution =
        document_tool_execution_new(tool_id, source_path);
    for (gsize index = 0; arguments != NULL &&
         arguments[index] != NULL; index++)
        document_tool_execution_add_argument(execution, arguments[index]);
    (void) file_hash_compute_sha256(source_path, cancellable,
        &execution->source_sha256, NULL, NULL);

    DocumentToolCaptureResult capture = { 0 };
    GError *capture_error = NULL;
    if (!document_tool_runner_capture(executable, arguments, limits,
            cancellable, &capture, &capture_error))
    {
        if (g_error_matches(capture_error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
            execution->state = DOCUMENT_ANALYSIS_STATE_CANCELLED;
            g_propagate_error(error, capture_error);
            capture_error = NULL;
        }
        else
        {
            execution->state = DOCUMENT_ANALYSIS_STATE_UNAVAILABLE;
            g_ptr_array_add(execution->errors, g_strdup(
                capture_error != NULL ? capture_error->message :
                "Outil indisponible."));
        }
        g_clear_error(&capture_error);
        GDateTime *now = g_date_time_new_now_utc();
        execution->finished_at_utc = g_date_time_format_iso8601(now);
        g_date_time_unref(now);
        *out_execution = execution;
        return execution->state != DOCUMENT_ANALYSIS_STATE_CANCELLED;
    }

    execution->raw_stdout =
        document_tool_runner_bytes_to_text(capture.stdout_bytes);
    execution->raw_stderr =
        document_tool_runner_bytes_to_text(capture.stderr_bytes);
    execution->stdout_bytes_observed = capture.stdout_bytes_observed;
    execution->stderr_bytes_observed = capture.stderr_bytes_observed;
    execution->stdout_truncated = capture.stdout_truncated;
    execution->stderr_truncated = capture.stderr_truncated;
    execution->exit_status = capture.exit_status;
    execution->raw_stdout_sha256 = g_compute_checksum_for_string(
        G_CHECKSUM_SHA256, execution->raw_stdout, -1);

    if (execution->stdout_truncated)
        g_ptr_array_add(execution->warnings,
            g_strdup("La sortie standard de l'outil a été tronquée."));
    if (execution->stderr_truncated)
        g_ptr_array_add(execution->warnings,
            g_strdup("La sortie d'erreur de l'outil a été tronquée."));
    execution->state =
        execution->stdout_truncated || execution->stderr_truncated
            ? DOCUMENT_ANALYSIS_STATE_PARTIAL
            : capture.exited_normally && capture.exit_status == 0
                ? DOCUMENT_ANALYSIS_STATE_SUCCESS
                : DOCUMENT_ANALYSIS_STATE_FAILED;

    GDateTime *now = g_date_time_new_now_utc();
    execution->finished_at_utc = g_date_time_format_iso8601(now);
    g_date_time_unref(now);
    document_tool_capture_result_clear(&capture);
    *out_execution = execution;
    return TRUE;
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
    const DocumentToolRunnerLimits limits = {
        DOCUMENT_ANALYSIS_MAX_STDOUT,
        DOCUMENT_ANALYSIS_MAX_STDERR
    };
    return document_tool_runner_run_with_limits(tool_id, executable,
        arguments, source_path, &limits, cancellable, out_execution, error);
}
