/******************************************************************************
 * @file test_local_tool_runner.c
 * @brief Régressions du runner POSIX borné J4.
 ******************************************************************************/
#include "core/local_tool_runner.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

static LocalToolRunnerProfile profile(void)
{
    return (LocalToolRunnerProfile) { 500U, 50U, 2U,
        128U * 1024U * 1024U, 1024U * 1024U, 32U, 4096U, 4096U };
}

static char *fake_path(void)
{
    char *relative = g_canonicalize_filename("tests/fake_document_tool", NULL);
    g_assert_true(g_path_is_absolute(relative));
    return relative;
}

static void test_binary_and_nonzero(void)
{
    char *fake = fake_path(); char *work = g_dir_make_tmp("labfy-local-run-XXXXXX", NULL);
    const char *binary[] = {fake, "--binary", NULL};
    LocalToolRunnerResult *result = NULL; LocalToolRunnerProfile limits = profile();
    g_assert_true(local_tool_runner_run(fake, binary, work, &limits,
        NULL, &result, NULL));
    g_assert_cmpint(result->state, ==, LOCAL_TOOL_RUNNER_EXITED);
    g_assert_cmpuint(g_bytes_get_size(result->stdout_bytes), ==, 4U);
    g_assert_true(result->stdout_complete); local_tool_runner_result_free(result);
    const char *nonzero[] = {fake, "--emit", "--exit-status", "7", NULL};
    result = NULL; g_assert_true(local_tool_runner_run(fake, nonzero, work,
        &limits, NULL, &result, NULL));
    g_assert_cmpint(result->state, ==, LOCAL_TOOL_RUNNER_NONZERO);
    g_assert_cmpint(result->exit_code, ==, 7);
    local_tool_runner_result_free(result); g_rmdir(work); g_free(work); g_free(fake);
}

static void test_output_limit_and_concurrent(void)
{
    char *fake = fake_path(); char *work = g_dir_make_tmp("labfy-local-run-XXXXXX", NULL);
    const char *argv[] = {fake, "--emit", "--stdout-size", "12000",
        "--stderr-size", "12000", "--chunks", "30", "--slow", NULL};
    LocalToolRunnerProfile limits = profile(); limits.stdout_bytes = 1000U;
    limits.stderr_bytes = 1200U; LocalToolRunnerResult *result = NULL;
    g_assert_true(local_tool_runner_run(fake, argv, work, &limits,
        NULL, &result, NULL));
    g_assert_cmpint(result->state, ==, LOCAL_TOOL_RUNNER_OUTPUT_LIMIT);
    g_assert_cmpuint(g_bytes_get_size(result->stdout_bytes), <=, 1000U);
    g_assert_cmpuint(g_bytes_get_size(result->stderr_bytes), <=, 1200U);
    g_assert_false(result->stdout_complete && result->stderr_complete);
    local_tool_runner_result_free(result); g_rmdir(work); g_free(work); g_free(fake);
}

static gpointer cancel_later(gpointer data)
{ g_usleep(50000U); g_cancellable_cancel(data); return NULL; }

static gpointer cancel_process_after_start(gpointer data)
{
    const char *marker = data;
    for (guint attempt = 0; attempt < 200U; attempt++) {
        if (g_file_test(marker, G_FILE_TEST_IS_REGULAR)) break;
        g_usleep(5000U);
    }
    g_assert_true(g_file_test(marker, G_FILE_TEST_IS_REGULAR));
    local_tool_runner_request_process_cancel();
    return NULL;
}

static void test_process_signal_cancellation_after_start(void)
{
    char *fake = fake_path();
    char *work = g_dir_make_tmp("labfy-local-cancel-XXXXXX", NULL);
    char *marker = g_build_filename(work, "started", NULL);
    const char *argv[] = {fake, "--sleep", "--ignore-term", "--started-file",
                          marker, NULL};
    LocalToolRunnerProfile limits = profile(); limits.wall_timeout_ms = 3000U;
    LocalToolRunnerResult *result = NULL;
    local_tool_runner_reset_process_cancel();
    GThread *thread = g_thread_new("cancel-process", cancel_process_after_start,
                                    marker);
    g_assert_true(local_tool_runner_run(fake, argv, work, &limits, NULL,
                                        &result, NULL));
    g_thread_join(thread);
    g_assert_cmpint(result->state, ==, LOCAL_TOOL_RUNNER_CANCELLED);
    g_assert_cmpint(result->elapsed_us, <, 1500000);
    local_tool_runner_reset_process_cancel();
    local_tool_runner_result_free(result); g_remove(marker); g_rmdir(work);
    g_free(marker); g_free(work); g_free(fake);
}

static void test_timeout_cancel_signal_and_absent(void)
{
    char *fake = fake_path(); char *work = g_dir_make_tmp("labfy-local-run-XXXXXX", NULL);
    LocalToolRunnerProfile limits = profile(); LocalToolRunnerResult *result = NULL;
    const char *sleeping[] = {fake, "--sleep", "--ignore-term", NULL};
    limits.wall_timeout_ms = 100U;
    g_assert_true(local_tool_runner_run(fake, sleeping, work, &limits,
        NULL, &result, NULL));
    g_assert_cmpint(result->state, ==, LOCAL_TOOL_RUNNER_TIMEOUT);
    g_assert_cmpint(result->elapsed_us, <, 1000000); local_tool_runner_result_free(result);
    GCancellable *cancel = g_cancellable_new(); limits.wall_timeout_ms = 1000U;
    GThread *thread = g_thread_new("cancel-local", cancel_later, cancel);
    result = NULL; g_assert_true(local_tool_runner_run(fake, sleeping, work,
        &limits, cancel, &result, NULL)); g_thread_join(thread);
    g_assert_cmpint(result->state, ==, LOCAL_TOOL_RUNNER_CANCELLED);
    local_tool_runner_result_free(result); g_object_unref(cancel);
    const char *signaled[] = {fake, "--signal", NULL}; result = NULL;
    g_assert_true(local_tool_runner_run(fake, signaled, work, &limits,
        NULL, &result, NULL));
    g_assert_cmpint(result->state, ==, LOCAL_TOOL_RUNNER_SIGNALED);
    local_tool_runner_result_free(result);
    const char *missing[] = {"/nonexistent/labfy-tool", NULL}; result = NULL;
    g_assert_true(local_tool_runner_run(missing[0], missing, work, &limits,
        NULL, &result, NULL));
    g_assert_cmpint(result->state, ==, LOCAL_TOOL_RUNNER_NOT_FOUND);
    local_tool_runner_result_free(result); g_rmdir(work); g_free(work); g_free(fake);
}

static void test_descendant_pipe_and_independent_process(void)
{
    char *fake = fake_path(); char *work = g_dir_make_tmp("labfy-local-run-XXXXXX", NULL);
    pid_t witness = fork();
    g_assert_cmpint(witness, >=, 0);
    if (witness == 0) { sleep(2); _exit(0); }
    const char *argv[] = {fake, "--descendant-pipe", NULL};
    LocalToolRunnerProfile limits = profile(); limits.wall_timeout_ms = 150U;
    LocalToolRunnerResult *result = NULL;
    g_assert_true(local_tool_runner_run(fake, argv, work, &limits,
        NULL, &result, NULL));
    g_assert_cmpint(result->state, ==, LOCAL_TOOL_RUNNER_TIMEOUT);
    g_assert_cmpint(kill(witness, 0), ==, 0);
    kill(witness, SIGTERM); waitpid(witness, NULL, 0);
    local_tool_runner_result_free(result); g_rmdir(work); g_free(work); g_free(fake);
}

static void test_inherited_fd_and_child_pipe_flags(void)
{
    char *fake = fake_path();
    char *work = g_dir_make_tmp("labfy-local-fd-XXXXXX", NULL);
    char *marker = g_build_filename(work, "SPECIMEN-marker.txt", NULL);
    g_assert_true(g_file_set_contents(marker, "M", 1, NULL));
    int inherited = open(marker, O_RDONLY);
    g_assert_cmpint(inherited, >=, 0);
    int flags = fcntl(inherited, F_GETFD);
    g_assert_cmpint(fcntl(inherited, F_SETFD, flags & ~FD_CLOEXEC), ==, 0);
    char fd_text[32]; g_snprintf(fd_text, sizeof(fd_text), "%d", inherited);
    const char *argv[] = {fake, "--inspect-runner", "--fd", fd_text, NULL};
    LocalToolRunnerProfile limits = profile();
    LocalToolRunnerResult *result = NULL;
    g_assert_true(local_tool_runner_run(fake, argv, work, &limits,
        NULL, &result, NULL));
    gsize size = 0U; const char *data = g_bytes_get_data(result->stdout_bytes, &size);
    char *text = g_strndup(data, size);
    g_assert_cmpstr(text, ==,
        "inherited=0 stdout_nonblocking=0 stderr_nonblocking=0\n");
    g_free(text); local_tool_runner_result_free(result); close(inherited);
    g_remove(marker); g_rmdir(work); g_free(marker); g_free(work); g_free(fake);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/local-runner/binary-nonzero", test_binary_and_nonzero);
    g_test_add_func("/local-runner/output-limit", test_output_limit_and_concurrent);
    g_test_add_func("/local-runner/states", test_timeout_cancel_signal_and_absent);
    g_test_add_func("/local-runner/descendant-scope", test_descendant_pipe_and_independent_process);
    g_test_add_func("/local-runner/fd-and-pipe-flags",
        test_inherited_fd_and_child_pipe_flags);
    g_test_add_func("/local-runner/process-signal-cancel",
        test_process_signal_cancellation_after_start);
    return g_test_run();
}
