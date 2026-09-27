/******************************************************************************
 * @file local_tool_runner.h
 * @brief Runner POSIX borné des adapters locaux J4.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_LOCAL_TOOL_RUNNER_H
#define LABFY_INVESTIGATION_LOCAL_TOOL_RUNNER_H

#include <gio/gio.h>
#include <glib.h>

G_BEGIN_DECLS

typedef enum {
    LOCAL_TOOL_RUNNER_EXITED,
    LOCAL_TOOL_RUNNER_NONZERO,
    LOCAL_TOOL_RUNNER_SIGNALED,
    LOCAL_TOOL_RUNNER_TIMEOUT,
    LOCAL_TOOL_RUNNER_CANCELLED,
    LOCAL_TOOL_RUNNER_OUTPUT_LIMIT,
    LOCAL_TOOL_RUNNER_NOT_FOUND,
    LOCAL_TOOL_RUNNER_PREPARATION_ERROR,
    LOCAL_TOOL_RUNNER_IO_ERROR
} LocalToolRunnerState;

typedef struct {
    guint wall_timeout_ms;
    guint terminate_grace_ms;
    guint cpu_seconds;
    guint64 address_space_bytes;
    guint64 file_size_bytes;
    guint file_descriptors;
    gsize stdout_bytes;
    gsize stderr_bytes;
} LocalToolRunnerProfile;

typedef struct {
    LocalToolRunnerState state;
    int exit_code;
    int term_signal;
    GBytes *stdout_bytes;
    GBytes *stderr_bytes;
    guint64 stdout_observed;
    guint64 stderr_observed;
    gboolean stdout_complete;
    gboolean stderr_complete;
    char *stdout_sha256;
    char *stderr_sha256;
    gint64 elapsed_us;
    int preparation_errno;
} LocalToolRunnerResult;

/**
 * CONTRACT: `executable` est un chemin absolu vers un fichier régulier
 * exécutable et `argv[0]` doit le désigner. Le runner copie seulement les
 * octets autorisés et possède entièrement le résultat retourné.
 *
 * INVARIANT: tout enfant créé reste dans la session dont le PID de tête est
 * celui du processus directement possédé. Timeout, annulation ou plafond de
 * sortie entraînent SIGTERM puis SIGKILL sur ce groupe seulement.
 */
gboolean local_tool_runner_run(
    const char *executable,
    const char *const argv[],
    const char *working_directory,
    const LocalToolRunnerProfile *profile,
    GCancellable *cancellable,
    LocalToolRunnerResult **out_result,
    GError **error);

/**
 * Demande l'annulation de l'exécution active du processus courant.
 *
 * CONTRACT: cette fonction est async-signal-safe et sert uniquement au
 * superviseur mono-worker qui possède au plus un runner actif. La demande est
 * consommée par `local_tool_runner_run`, qui garde la propriété du groupe
 * enfant jusqu'à sa terminaison et sa récolte.
 */
void local_tool_runner_request_process_cancel(void);

/** Réinitialise la demande avant de réclamer une nouvelle exécution. */
void local_tool_runner_reset_process_cancel(void);

void local_tool_runner_result_free(LocalToolRunnerResult *result);
const char *local_tool_runner_state_code(LocalToolRunnerState state);

G_END_DECLS
#endif
