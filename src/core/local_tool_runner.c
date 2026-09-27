#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

/******************************************************************************
 * @file local_tool_runner.c
 * @brief Exécution POSIX bornée, sans shell, des adapters locaux J4.
 ******************************************************************************/
#include "core/local_tool_runner.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define LOCAL_TOOL_BLOCK 8192U

static volatile sig_atomic_t local_tool_process_cancel_requested = 0;

void local_tool_runner_request_process_cancel(void)
{
    local_tool_process_cancel_requested = 1;
}

void local_tool_runner_reset_process_cancel(void)
{
    local_tool_process_cancel_requested = 0;
}

typedef struct {
    int fd;
    GByteArray *prefix;
    guint64 observed;
    gsize limit;
    gboolean complete;
} LocalToolCapture;

static gint64 local_tool_now_us(void)
{
    struct timespec value = {0};
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) return 0;
    return (gint64) value.tv_sec * G_USEC_PER_SEC + value.tv_nsec / 1000;
}

static gboolean local_tool_valid_executable(const char *path)
{
    struct stat status = {0};
    return path != NULL && path[0] == '/' && stat(path, &status) == 0 &&
        S_ISREG(status.st_mode) && access(path, X_OK) == 0;
}

static gboolean local_tool_valid_profile(const LocalToolRunnerProfile *profile)
{
    return profile != NULL && profile->wall_timeout_ms > 0U &&
        profile->terminate_grace_ms > 0U && profile->cpu_seconds > 0U &&
        profile->address_space_bytes > 0U && profile->file_size_bytes > 0U &&
        profile->file_descriptors >= 3U && profile->stdout_bytes > 0U &&
        profile->stderr_bytes > 0U;
}

static gboolean local_tool_clear_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL);
    return flags >= 0 && fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) == 0;
}

static void local_tool_child_fail(int error_fd, int error_number)
{
    int saved = error_number;
    (void) write(error_fd, &saved, sizeof(saved));
    _exit(126);
}

/* CONTRACT: uniquement des appels async-signal-safe entre fork et exec. */
static void local_tool_child(
    const char *executable,
    const char *const argv[],
    const char *working_directory,
    const LocalToolRunnerProfile *profile,
    int stdout_fd,
    int stderr_fd,
    int error_fd,
    int max_inherited_fd)
{
    struct rlimit limit = {0};
    const char *const environment[] = {
        "PATH=/usr/bin:/bin", "LANG=C.UTF-8", "LC_ALL=C.UTF-8",
        "HOME=/nonexistent", NULL
    };
    if (setsid() < 0 || chdir(working_directory) < 0 ||
        !local_tool_clear_nonblocking(stdout_fd) ||
        !local_tool_clear_nonblocking(stderr_fd) ||
        dup2(stdout_fd, STDOUT_FILENO) < 0 ||
        dup2(stderr_fd, STDERR_FILENO) < 0)
        local_tool_child_fail(error_fd, errno);
    int null_fd = open("/dev/null", O_RDONLY | O_CLOEXEC);
    if (null_fd < 0 || dup2(null_fd, STDIN_FILENO) < 0)
        local_tool_child_fail(error_fd, errno);
    if (null_fd > STDERR_FILENO) close(null_fd);
    /* INVARIANT: la borne vient du RLIMIT du parent avant fork, donc couvre
     * aussi les fd situés au-dessus de la future limite plus basse. */
    for (int fd = STDERR_FILENO + 1; fd < max_inherited_fd; fd++)
        if (fd != error_fd) (void) close(fd);
    limit.rlim_cur = limit.rlim_max = profile->cpu_seconds;
    if (setrlimit(RLIMIT_CPU, &limit) != 0)
        local_tool_child_fail(error_fd, errno);
    limit.rlim_cur = limit.rlim_max = profile->address_space_bytes;
    if (setrlimit(RLIMIT_AS, &limit) != 0)
        local_tool_child_fail(error_fd, errno);
    limit.rlim_cur = limit.rlim_max = profile->file_size_bytes;
    if (setrlimit(RLIMIT_FSIZE, &limit) != 0)
        local_tool_child_fail(error_fd, errno);
    limit.rlim_cur = limit.rlim_max = profile->file_descriptors;
    if (setrlimit(RLIMIT_NOFILE, &limit) != 0)
        local_tool_child_fail(error_fd, errno);
    execve(executable, (char *const *) argv, (char *const *) environment);
    local_tool_child_fail(error_fd, errno);
}

static gboolean local_tool_capture_read(LocalToolCapture *capture)
{
    guint8 block[LOCAL_TOOL_BLOCK];
    while (TRUE) {
        ssize_t count = read(capture->fd, block, sizeof(block));
        if (count > 0) {
            guint64 amount = (guint64) count;
            capture->observed = G_MAXUINT64 - capture->observed < amount
                ? G_MAXUINT64 : capture->observed + amount;
            gsize remaining = capture->prefix->len < capture->limit
                ? capture->limit - capture->prefix->len : 0U;
            gsize retained = MIN(remaining, (gsize) count);
            if (retained > 0U)
                g_byte_array_append(capture->prefix, block, retained);
            if (retained < (gsize) count) return FALSE;
            continue;
        }
        if (count == 0) { capture->complete = TRUE; return TRUE; }
        if (errno == EAGAIN || errno == EWOULDBLOCK) return TRUE;
        if (errno == EINTR) continue;
        return FALSE;
    }
}

static void local_tool_terminate_group(pid_t pid, guint grace_ms)
{
    gint64 deadline = local_tool_now_us() + (gint64) grace_ms * 1000;
    if (kill(-pid, SIGTERM) != 0 && errno == ESRCH)
        (void) kill(pid, SIGTERM);
    while (local_tool_now_us() < deadline) {
        if (kill(-pid, 0) != 0 && errno == ESRCH) return;
        struct timespec delay = { .tv_nsec = 5000000L };
        (void) nanosleep(&delay, NULL);
    }
    (void) kill(-pid, SIGKILL);
    (void) kill(pid, SIGKILL);
}

static char *local_tool_hash(GBytes *bytes)
{
    gsize size = 0U;
    const guint8 *data = g_bytes_get_data(bytes, &size);
    return g_compute_checksum_for_data(G_CHECKSUM_SHA256, data, size);
}

void local_tool_runner_result_free(LocalToolRunnerResult *result)
{
    if (result == NULL) return;
    g_clear_pointer(&result->stdout_bytes, g_bytes_unref);
    g_clear_pointer(&result->stderr_bytes, g_bytes_unref);
    g_free(result->stdout_sha256);
    g_free(result->stderr_sha256);
    g_free(result);
}

const char *local_tool_runner_state_code(LocalToolRunnerState state)
{
    static const char *const codes[] = { "exited", "nonzero", "signaled",
        "timeout", "cancelled", "output_limit", "not_found",
        "preparation_error", "io_error" };
    return state <= LOCAL_TOOL_RUNNER_IO_ERROR ? codes[state] : "io_error";
}

gboolean local_tool_runner_run(
    const char *executable,
    const char *const argv[],
    const char *working_directory,
    const LocalToolRunnerProfile *profile,
    GCancellable *cancellable,
    LocalToolRunnerResult **out_result,
    GError **error)
{
    int stdout_pipe[2] = {-1, -1}, stderr_pipe[2] = {-1, -1};
    int exec_pipe[2] = {-1, -1};
    pid_t pid = -1;
    int wait_status = 0;
    gboolean reaped = FALSE, stopping = FALSE, io_failure = FALSE;
    LocalToolRunnerState stop_state = LOCAL_TOOL_RUNNER_IO_ERROR;
    gint64 started = local_tool_now_us();
    gint64 deadline = 0;
    LocalToolCapture output = {0}, diagnostic = {0};
    LocalToolRunnerResult *result = NULL;
    struct rlimit inherited_limit = {0};
    int max_inherited_fd = 0;
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    if (out_result != NULL) *out_result = NULL;
    if (argv == NULL || argv[0] == NULL ||
        g_strcmp0(argv[0], executable) != 0 || out_result == NULL ||
        working_directory == NULL || !g_file_test(working_directory,
            G_FILE_TEST_IS_DIR) || !local_tool_valid_profile(profile)) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
            "Le contrat du runner local est invalide.");
        return FALSE;
    }
    if (getrlimit(RLIMIT_NOFILE, &inherited_limit) != 0 ||
        inherited_limit.rlim_cur == RLIM_INFINITY ||
        inherited_limit.rlim_cur > (rlim_t) G_MAXINT) {
        max_inherited_fd = 1048576;
    } else max_inherited_fd = (int) inherited_limit.rlim_cur;
    deadline = started + (gint64) profile->wall_timeout_ms * 1000;
    if (!local_tool_valid_executable(executable)) {
        result = g_new0(LocalToolRunnerResult, 1);
        result->state = LOCAL_TOOL_RUNNER_NOT_FOUND;
        result->exit_code = -1;
        result->stdout_bytes = g_bytes_new_static("", 0U);
        result->stderr_bytes = g_bytes_new_static("", 0U);
        result->stdout_complete = result->stderr_complete = TRUE;
        result->stdout_sha256 = local_tool_hash(result->stdout_bytes);
        result->stderr_sha256 = local_tool_hash(result->stderr_bytes);
        *out_result = result;
        return TRUE;
    }
    if (cancellable != NULL && g_cancellable_is_cancelled(cancellable)) {
        result = g_new0(LocalToolRunnerResult, 1);
        result->state = LOCAL_TOOL_RUNNER_CANCELLED;
        result->exit_code = -1;
        result->stdout_bytes = g_bytes_new_static("", 0U);
        result->stderr_bytes = g_bytes_new_static("", 0U);
        result->stdout_complete = result->stderr_complete = TRUE;
        result->stdout_sha256 = local_tool_hash(result->stdout_bytes);
        result->stderr_sha256 = local_tool_hash(result->stderr_bytes);
        *out_result = result;
        return TRUE;
    }
    if (pipe2(stdout_pipe, O_CLOEXEC | O_NONBLOCK) != 0 ||
        pipe2(stderr_pipe, O_CLOEXEC | O_NONBLOCK) != 0 ||
        pipe2(exec_pipe, O_CLOEXEC) != 0) goto system_failure;
    pid = fork();
    if (pid < 0) goto system_failure;
    if (pid == 0) {
        close(stdout_pipe[0]); close(stderr_pipe[0]); close(exec_pipe[0]);
        local_tool_child(executable, argv, working_directory, profile,
            stdout_pipe[1], stderr_pipe[1], exec_pipe[1], max_inherited_fd);
    }
    close(stdout_pipe[1]); stdout_pipe[1] = -1;
    close(stderr_pipe[1]); stderr_pipe[1] = -1;
    close(exec_pipe[1]); exec_pipe[1] = -1;
    output = (LocalToolCapture) { stdout_pipe[0], g_byte_array_new(), 0U,
        profile->stdout_bytes, FALSE };
    diagnostic = (LocalToolCapture) { stderr_pipe[0], g_byte_array_new(), 0U,
        profile->stderr_bytes, FALSE };
    while (!(reaped && output.complete && diagnostic.complete)) {
        gint64 now = local_tool_now_us();
        if (!stopping && (local_tool_process_cancel_requested != 0 ||
            (cancellable != NULL && g_cancellable_is_cancelled(cancellable)))) {
            stopping = TRUE; stop_state = LOCAL_TOOL_RUNNER_CANCELLED;
        } else if (!stopping && now >= deadline) {
            stopping = TRUE; stop_state = LOCAL_TOOL_RUNNER_TIMEOUT;
        }
        if (stopping) {
            local_tool_terminate_group(pid, profile->terminate_grace_ms);
            deadline = local_tool_now_us() +
                (gint64) profile->terminate_grace_ms * 1000;
        }
        struct pollfd descriptors[2] = {
            { output.fd, POLLIN | POLLHUP, 0 },
            { diagnostic.fd, POLLIN | POLLHUP, 0 }
        };
        (void) poll(descriptors, 2U, 10);
        if (!output.complete && !local_tool_capture_read(&output)) {
            stopping = TRUE; stop_state = output.prefix->len >= output.limit
                ? LOCAL_TOOL_RUNNER_OUTPUT_LIMIT : LOCAL_TOOL_RUNNER_IO_ERROR;
            io_failure = stop_state == LOCAL_TOOL_RUNNER_IO_ERROR;
        }
        if (!diagnostic.complete && !local_tool_capture_read(&diagnostic)) {
            stopping = TRUE; stop_state = diagnostic.prefix->len >= diagnostic.limit
                ? LOCAL_TOOL_RUNNER_OUTPUT_LIMIT : LOCAL_TOOL_RUNNER_IO_ERROR;
            io_failure = stop_state == LOCAL_TOOL_RUNNER_IO_ERROR;
        }
        if (!reaped) {
            pid_t waited = waitpid(pid, &wait_status, WNOHANG);
            if (waited == pid) reaped = TRUE;
            else if (waited < 0 && errno != EINTR) {
                stopping = TRUE; stop_state = LOCAL_TOOL_RUNNER_IO_ERROR;
                io_failure = TRUE;
            }
        }
    }
    result = g_new0(LocalToolRunnerResult, 1);
    int preparation_errno = 0;
    ssize_t preparation_size;
    do preparation_size = read(exec_pipe[0], &preparation_errno,
        sizeof(preparation_errno)); while (preparation_size < 0 && errno == EINTR);
    result->exit_code = WIFEXITED(wait_status) ? WEXITSTATUS(wait_status) : -1;
    result->term_signal = WIFSIGNALED(wait_status) ? WTERMSIG(wait_status) : 0;
    result->preparation_errno = preparation_size == sizeof(preparation_errno)
        ? preparation_errno : 0;
    result->state = result->preparation_errno != 0
        ? LOCAL_TOOL_RUNNER_PREPARATION_ERROR : stopping ? stop_state : WIFSIGNALED(wait_status)
        ? LOCAL_TOOL_RUNNER_SIGNALED : result->exit_code == 0
            ? LOCAL_TOOL_RUNNER_EXITED : LOCAL_TOOL_RUNNER_NONZERO;
    result->stdout_observed = output.observed;
    result->stderr_observed = diagnostic.observed;
    result->stdout_complete = output.complete &&
        output.observed <= output.limit && !io_failure;
    result->stderr_complete = diagnostic.complete &&
        diagnostic.observed <= diagnostic.limit && !io_failure;
    result->stdout_bytes = g_byte_array_free_to_bytes(output.prefix);
    result->stderr_bytes = g_byte_array_free_to_bytes(diagnostic.prefix);
    result->stdout_sha256 = local_tool_hash(result->stdout_bytes);
    result->stderr_sha256 = local_tool_hash(result->stderr_bytes);
    result->elapsed_us = local_tool_now_us() - started;
    close(output.fd); close(diagnostic.fd); close(exec_pipe[0]);
    *out_result = result;
    return TRUE;
system_failure:
    if (stdout_pipe[0] >= 0) close(stdout_pipe[0]);
    if (stdout_pipe[1] >= 0) close(stdout_pipe[1]);
    if (stderr_pipe[0] >= 0) close(stderr_pipe[0]);
    if (stderr_pipe[1] >= 0) close(stderr_pipe[1]);
    if (exec_pipe[0] >= 0) close(exec_pipe[0]);
    if (exec_pipe[1] >= 0) close(exec_pipe[1]);
    g_set_error(error, G_IO_ERROR, g_io_error_from_errno(errno),
        "Le runner local n'a pas pu créer le processus : %s.",
        g_strerror(errno));
    return FALSE;
}
