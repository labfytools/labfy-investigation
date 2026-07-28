/******************************************************************************
 * @file test_document_tool_runner.c
 * @brief Tests synthétiques de la capture documentaire bornée.
 ******************************************************************************/
#include "core/document_tool_runner.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <sys/resource.h>

static char *create_source(char **out_directory)
{
    GError *error = NULL;
    *out_directory = g_dir_make_tmp("labfy-runner-XXXXXX", &error);
    g_assert_no_error(error);
    char *path = g_build_filename(*out_directory, "source.bin", NULL);
    g_assert_true(g_file_set_contents(path, "synthetic", -1, &error));
    g_assert_no_error(error);
    return path;
}

static void remove_source(char *directory, char *path)
{
    g_remove(path);
    g_rmdir(directory);
    g_free(path);
    g_free(directory);
}

static void test_limits_and_concurrent_drain(void)
{
    char *directory = NULL;
    char *path = create_source(&directory);
    const char *arguments[] = {
        "--emit", "--stdout-size", "5000", "--stderr-size", "7000",
        "--chunks", "20", "--exit-status", "7", NULL
    };
    DocumentToolRunnerLimits limits = { 1000, 1500 };
    DocumentToolExecution *execution = NULL;
    GError *error = NULL;
    g_assert_true(document_tool_runner_run_with_limits(
        "synthetic", "tests/fake_document_tool", arguments, path,
        &limits, NULL, &execution, &error));
    g_assert_no_error(error);
    g_assert_cmpuint(strlen(execution->raw_stdout), ==, 1000);
    g_assert_cmpuint(strlen(execution->raw_stderr), ==, 1500);
    g_assert_cmpuint(execution->stdout_bytes_observed, ==, 5000);
    g_assert_cmpuint(execution->stderr_bytes_observed, ==, 7000);
    g_assert_true(execution->stdout_truncated);
    g_assert_true(execution->stderr_truncated);
    g_assert_cmpint(execution->exit_status, ==, 7);
    g_assert_cmpint(execution->state, ==,
        DOCUMENT_ANALYSIS_STATE_PARTIAL);
    g_assert_cmpint(execution->raw_stdout[0], ==, 'O');
    g_assert_cmpint(execution->raw_stderr[0], ==, 'E');
    document_tool_execution_free(execution);
    remove_source(directory, path);
}

static void test_exact_and_below_limits(void)
{
    char *directory = NULL;
    char *path = create_source(&directory);
    const char *arguments[] = {
        "--emit", "--stdout-size", "1000", "--stderr-size", "12", NULL
    };
    DocumentToolRunnerLimits limits = { 1000, 20 };
    DocumentToolExecution *execution = NULL;
    g_assert_true(document_tool_runner_run_with_limits(
        "synthetic", "tests/fake_document_tool", arguments, path,
        &limits, NULL, &execution, NULL));
    g_assert_false(execution->stdout_truncated);
    g_assert_false(execution->stderr_truncated);
    g_assert_cmpint(execution->state, ==,
        DOCUMENT_ANALYSIS_STATE_SUCCESS);
    document_tool_execution_free(execution);
    remove_source(directory, path);
}

static gpointer cancel_later(gpointer user_data)
{
    g_usleep(60000);
    g_cancellable_cancel(user_data);
    return NULL;
}

static void test_cancellation_during_output(void)
{
    char *directory = NULL;
    char *path = create_source(&directory);
    const char *arguments[] = {
        "--emit", "--stdout-size", "100000", "--stderr-size", "100000",
        "--chunks", "100", "--slow", NULL
    };
    DocumentToolRunnerLimits limits = { 128, 128 };
    DocumentToolExecution *execution = NULL;
    GCancellable *cancellable = g_cancellable_new();
    GThread *thread = g_thread_new("cancel-runner", cancel_later, cancellable);
    GError *error = NULL;
    g_assert_false(document_tool_runner_run_with_limits(
        "synthetic", "tests/fake_document_tool", arguments, path,
        &limits, cancellable, &execution, &error));
    g_thread_join(thread);
    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
    g_assert_nonnull(execution);
    g_assert_cmpint(execution->state, ==,
        DOCUMENT_ANALYSIS_STATE_CANCELLED);
    g_clear_error(&error);
    document_tool_execution_free(execution);
    g_object_unref(cancellable);

    const char *success_arguments[] = {
        "--emit", "--stdout-size", "1", NULL
    };
    execution = NULL;
    g_assert_true(document_tool_runner_run_with_limits(
        "synthetic", "tests/fake_document_tool", success_arguments, path,
        &limits, NULL, &execution, NULL));
    document_tool_execution_free(execution);
    remove_source(directory, path);
}

static void test_repeated_runs_do_not_exhaust_descriptors(void)
{
    char *directory = NULL;
    char *path = create_source(&directory);
    const char *arguments[] = {
        "--emit", "--stdout-size", "8", "--stderr-size", "8", NULL
    };
    DocumentToolRunnerLimits limits = { 16, 16 };
    struct rlimit original_limit;
    g_assert_cmpint(getrlimit(RLIMIT_NOFILE, &original_limit), ==, 0);
    struct rlimit test_limit = original_limit;
    test_limit.rlim_cur = MIN(original_limit.rlim_cur, (rlim_t) 64);
    g_assert_cmpint(setrlimit(RLIMIT_NOFILE, &test_limit), ==, 0);
    for (guint index = 0; index < 96; index++)
    {
        DocumentToolExecution *execution = NULL;
        g_assert_true(document_tool_runner_run_with_limits(
            "synthetic", "tests/fake_document_tool", arguments, path,
            &limits, NULL, &execution, NULL));
        document_tool_execution_free(execution);
    }
    g_assert_cmpint(setrlimit(RLIMIT_NOFILE, &original_limit), ==, 0);
    remove_source(directory, path);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/document-tool-runner/limits-concurrent",
        test_limits_and_concurrent_drain);
    g_test_add_func("/document-tool-runner/exact-below",
        test_exact_and_below_limits);
    g_test_add_func("/document-tool-runner/cancellation",
        test_cancellation_during_output);
    g_test_add_func("/document-tool-runner/no-descriptor-exhaustion",
        test_repeated_runs_do_not_exhaust_descriptors);
    return g_test_run();
}
