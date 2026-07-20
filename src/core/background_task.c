/******************************************************************************
 * @file background_task.c
 * @brief Implémentation d'une tâche générique exécutée en arrière-plan.
 ******************************************************************************/

#include "core/background_task.h"

#include <glib.h>

/**
 * @struct BackgroundTask
 * @brief État interne d'une tâche asynchrone.
 *
 * Les champs mutables sont protégés par mutex.
 *
 * Le titre est immuable après la création de la tâche.
 */
struct BackgroundTask
{
    gatomicrefcount reference_count;
    GMutex mutex;

    char *title;
    char *status_message;

    BackgroundTaskState state;
    double progress;

    gint64 started_at_us;
    gint64 finished_at_us;

    GCancellable *cancellable;

    gpointer result;
    GDestroyNotify result_destroy;

    GError *error;

    BackgroundTaskCompletionCallback
        completion_callback;

    gpointer completion_data;
    GDestroyNotify completion_data_destroy;
};

/**
 * @struct BackgroundTaskRunContext
 * @brief Données privées associées à l'exécution GTask.
 */
typedef struct
{
    BackgroundTask *task;

    BackgroundTaskWorker worker;

    gpointer worker_data;
    GDestroyNotify worker_data_destroy;

    GDestroyNotify result_destroy;
} BackgroundTaskRunContext;

GQuark background_task_error_quark(void)
{
    return g_quark_from_static_string(
        "labfy-investigation-background-task-error"
    );
}

/**
 * @brief Exécute le worker dans un thread secondaire.
 *
 * @param async_task Tâche GLib.
 * @param source_object Objet source inutilisé.
 * @param task_data Contexte BackgroundTaskRunContext.
 * @param cancellable Objet d'annulation.
 */
static void background_task_run_in_thread(
    GTask *async_task,
    gpointer source_object,
    gpointer task_data,
    GCancellable *cancellable
)
{
    BackgroundTaskRunContext *run_context =
        task_data;

    gpointer worker_result = NULL;
    GError *worker_error = NULL;

    gboolean worker_succeeded = FALSE;

    (void) source_object;

    if (run_context == NULL ||
        run_context->task == NULL ||
        run_context->worker == NULL)
    {
        g_task_return_new_error(
            async_task,
            BACKGROUND_TASK_ERROR,
            BACKGROUND_TASK_ERROR_INVALID_ARGUMENT,
            "Le contexte d'exécution de la tâche est invalide."
        );

        return;
    }

    worker_succeeded = run_context->worker(
        run_context->task,
        cancellable,
        run_context->worker_data,
        &worker_result,
        &worker_error
    );

    if (worker_succeeded)
    {
        if (worker_error != NULL)
        {
            if (worker_result != NULL &&
                run_context->result_destroy != NULL)
            {
                run_context->result_destroy(
                    worker_result
                );

                worker_result = NULL;
            }

            g_task_return_new_error(
                async_task,
                BACKGROUND_TASK_ERROR,
                BACKGROUND_TASK_ERROR_WORKER_PROTOCOL,
                "Le worker a signalé un succès tout en retournant "
                "une erreur : %s",
                worker_error->message
            );

            g_clear_error(
                &worker_error
            );

            return;
        }

        g_task_return_pointer(
            async_task,
            worker_result,
            run_context->result_destroy
        );

        return;
    }

    /*
     * Un résultat ne doit pas être conservé lorsque le worker échoue.
     */
    if (worker_result != NULL &&
        run_context->result_destroy != NULL)
    {
        run_context->result_destroy(
            worker_result
        );

        worker_result = NULL;
    }

    if (worker_error == NULL)
    {
        g_task_return_new_error(
            async_task,
            BACKGROUND_TASK_ERROR,
            BACKGROUND_TASK_ERROR_WORKER_PROTOCOL,
            "Le worker a signalé un échec sans fournir de GError."
        );

        return;
    }

    /*
     * GTask devient propriétaire de worker_error.
     */
    g_task_return_error(
        async_task,
        worker_error
    );
}

/**
 * @brief Finalise une tâche après l'exécution du worker.
 *
 * Ce callback est rappelé sur le contexte ayant démarré GTask.
 *
 * @param source_object Objet source inutilisé.
 * @param async_result Résultat asynchrone.
 * @param user_data Pointeur vers BackgroundTask.
 */
static void background_task_on_completed(
    GObject *source_object,
    GAsyncResult *async_result,
    gpointer user_data
)
{
    BackgroundTask *task = user_data;

    gpointer worker_result = NULL;
    GError *worker_error = NULL;

    BackgroundTaskState final_state;

    BackgroundTaskCompletionCallback
        completion_callback = NULL;

    gpointer completion_data = NULL;

    GDestroyNotify completion_data_destroy =
        NULL;

    BackgroundTaskRunContext *run_context =
        NULL;

    BackgroundTask *internal_task_reference =
        NULL;
    
    (void) source_object;

    if (task == NULL ||
        async_result == NULL)
    {
        return;
    }

    run_context =
        g_task_get_task_data(
            G_TASK(async_result)
        );

    worker_result = g_task_propagate_pointer(
        G_TASK(async_result),
        &worker_error
    );

    if (worker_error == NULL)
    {
        final_state =
            BACKGROUND_TASK_STATE_COMPLETED;
    }
    else if (g_error_matches(
                 worker_error,
                 G_IO_ERROR,
                 G_IO_ERROR_CANCELLED
             ))
    {
        final_state =
            BACKGROUND_TASK_STATE_CANCELLED;
    }
    else
    {
        final_state =
            BACKGROUND_TASK_STATE_FAILED;
    }

    g_mutex_lock(
        &task->mutex
    );

    task->state = final_state;

    task->finished_at_us =
        g_get_monotonic_time();

    if (final_state ==
        BACKGROUND_TASK_STATE_COMPLETED)
    {
        task->result = worker_result;
        task->progress = 1.0;

        worker_result = NULL;
    }
    else
    {
        task->error = worker_error;
        worker_error = NULL;
    }

    completion_callback =
        task->completion_callback;

    completion_data =
        task->completion_data;

    completion_data_destroy =
        task->completion_data_destroy;

    /*
     * Ces données sont extraites afin de garantir leur destruction
     * exactement une fois après le callback utilisateur.
     */
    task->completion_callback = NULL;
    task->completion_data = NULL;
    task->completion_data_destroy = NULL;

    g_mutex_unlock(
        &task->mutex
    );

    /*
     * Aucun callback utilisateur ne doit être exécuté sous mutex.
     */
    if (completion_callback != NULL)
    {
        completion_callback(
            task,
            completion_data
        );
    }

    if (completion_data != NULL &&
        completion_data_destroy != NULL)
    {
        completion_data_destroy(
            completion_data
        );
    }

    /*
     * Ces variables sont normalement NULL après le transfert,
     * mais ce nettoyage sécurise les futurs changements.
     */
    if (worker_result != NULL &&
        task->result_destroy != NULL)
    {
        task->result_destroy(
            worker_result
        );
    }

    g_clear_error(
        &worker_error
    );

    /*
     * L'exécution est maintenant totalement finalisée.
     *
     * La référence interne doit être libérée ici, et non attendre la
     * destruction ultérieure du GTask. Le pointeur est placé à NULL pour
     * empêcher background_task_run_context_free() de refaire un unref.
     */
    if (run_context != NULL)
    {
        internal_task_reference =
            run_context->task;

        run_context->task =
            NULL;
    }

    background_task_unref(
        internal_task_reference
    );
}

/**
 * @brief Libère le contexte privé d'une exécution.
 *
 * @param user_data Pointeur vers BackgroundTaskRunContext.
 */
static void background_task_run_context_free(
    gpointer user_data
)
{
    BackgroundTaskRunContext *run_context =
        user_data;

    if (run_context == NULL)
    {
        return;
    }

    if (run_context->worker_data != NULL &&
        run_context->worker_data_destroy != NULL)
    {
        run_context->worker_data_destroy(
            run_context->worker_data
        );
    }

    background_task_unref(
        run_context->task
    );

    g_free(
        run_context
    );
}

BackgroundTask *background_task_new(
    const char *title
)
{
    BackgroundTask *task = NULL;

    if (title == NULL ||
        title[0] == '\0')
    {
        return NULL;
    }

    task = g_new0(
        BackgroundTask,
        1
    );

    g_atomic_ref_count_init(
        &task->reference_count
    );

    g_mutex_init(
        &task->mutex
    );

    task->title = g_strdup(
        title
    );

    task->state =
        BACKGROUND_TASK_STATE_PENDING;

    task->progress = 0.0;
    task->started_at_us = 0;
    task->finished_at_us = 0;

    return task;
}

BackgroundTask *background_task_ref(
    BackgroundTask *task
)
{
    if (task == NULL)
    {
        return NULL;
    }

    g_atomic_ref_count_inc(
        &task->reference_count
    );

    return task;
}

void background_task_unref(
    BackgroundTask *task
)
{
    gpointer result = NULL;
    GDestroyNotify result_destroy = NULL;

    gpointer completion_data = NULL;
    GDestroyNotify completion_data_destroy = NULL;

    GCancellable *cancellable = NULL;
    GError *error = NULL;

    char *title = NULL;
    char *status_message = NULL;

    if (task == NULL)
    {
        return;
    }

    if (!g_atomic_ref_count_dec(
            &task->reference_count
        ))
    {
        return;
    }

    /*
     * À ce stade, aucune autre référence ne doit accéder à la tâche.
     * Les ressources sont néanmoins extraites proprement avant
     * la destruction du mutex.
     */
    g_mutex_lock(
        &task->mutex
    );

    result = task->result;
    result_destroy = task->result_destroy;

    completion_data = task->completion_data;
    completion_data_destroy =
        task->completion_data_destroy;

    cancellable = task->cancellable;
    error = task->error;

    title = task->title;
    status_message = task->status_message;

    task->result = NULL;
    task->result_destroy = NULL;

    task->completion_data = NULL;
    task->completion_data_destroy = NULL;
    task->completion_callback = NULL;

    task->cancellable = NULL;
    task->error = NULL;

    task->title = NULL;
    task->status_message = NULL;

    g_mutex_unlock(
        &task->mutex
    );

    if (result != NULL &&
        result_destroy != NULL)
    {
        result_destroy(
            result
        );
    }

    if (completion_data != NULL &&
        completion_data_destroy != NULL)
    {
        completion_data_destroy(
            completion_data
        );
    }

    if (cancellable != NULL)
    {
        g_object_unref(
            cancellable
        );
    }

    g_clear_error(
        &error
    );

    g_free(
        status_message
    );

    g_free(
        title
    );

    g_mutex_clear(
        &task->mutex
    );

    g_free(
        task
    );
}

gboolean background_task_start(
    BackgroundTask *task,
    BackgroundTaskWorker worker,
    gpointer worker_data,
    GDestroyNotify worker_data_destroy,
    GDestroyNotify result_destroy,
    BackgroundTaskCompletionCallback completion_callback,
    gpointer completion_data,
    GDestroyNotify completion_data_destroy,
    GError **error
)
{
    BackgroundTaskRunContext *run_context =
        NULL;

    GCancellable *cancellable = NULL;
    GTask *async_task = NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (task == NULL ||
        worker == NULL)
    {
        g_set_error_literal(
            error,
            BACKGROUND_TASK_ERROR,
            BACKGROUND_TASK_ERROR_INVALID_ARGUMENT,
            "La tâche ou son worker est invalide."
        );

        return FALSE;
    }

    /*
     * Le GCancellable est préparé avant le verrouillage.
     */
    cancellable = g_cancellable_new();

    g_mutex_lock(
        &task->mutex
    );

    if (task->state !=
        BACKGROUND_TASK_STATE_PENDING)
    {
        g_mutex_unlock(
            &task->mutex
        );

        g_object_unref(
            cancellable
        );

        g_set_error_literal(
            error,
            BACKGROUND_TASK_ERROR,
            BACKGROUND_TASK_ERROR_ALREADY_STARTED,
            "Cette tâche a déjà été démarrée."
        );

        return FALSE;
    }

    task->state =
        BACKGROUND_TASK_STATE_RUNNING;

    task->progress = 0.0;

    task->started_at_us =
        g_get_monotonic_time();

    task->finished_at_us = 0;

    task->cancellable = g_object_ref(
        cancellable
    );

    task->result_destroy =
        result_destroy;

    task->completion_callback =
        completion_callback;

    task->completion_data =
        completion_data;

    task->completion_data_destroy =
        completion_data_destroy;

    g_mutex_unlock(
        &task->mutex
    );

    run_context = g_new0(
        BackgroundTaskRunContext,
        1
    );

    /*
     * Cette référence interne protège la tâche même si l'appelant
     * libère immédiatement sa propre référence.
     */
    run_context->task =
        background_task_ref(task);

    run_context->worker = worker;

    run_context->worker_data =
        worker_data;

    run_context->worker_data_destroy =
        worker_data_destroy;

    run_context->result_destroy =
        result_destroy;

    async_task = g_task_new(
        NULL,
        cancellable,
        background_task_on_completed,
        task
    );

    g_task_set_task_data(
        async_task,
        run_context,
        background_task_run_context_free
    );

    g_task_set_check_cancellable(
        async_task,
        TRUE
    );

    g_task_run_in_thread(
        async_task,
        background_task_run_in_thread
    );

    /*
     * GTask conserve sa propre référence pendant l'exécution.
     */
    g_object_unref(
        async_task
    );

    g_object_unref(
        cancellable
    );

    return TRUE;
}

void background_task_cancel(
    BackgroundTask *task
)
{
    GCancellable *cancellable = NULL;

    if (task == NULL)
    {
        return;
    }

    g_mutex_lock(
        &task->mutex
    );

    if (task->state ==
            BACKGROUND_TASK_STATE_RUNNING &&
        task->cancellable != NULL)
    {
        cancellable = g_object_ref(
            task->cancellable
        );
    }

    g_mutex_unlock(
        &task->mutex
    );

    /*
     * L'annulation est effectuée hors du mutex.
     *
     * GCancellable peut réveiller des callbacks ou des opérations
     * bloquantes. Il ne faut donc pas garder le verrou.
     */
    if (cancellable != NULL)
    {
        g_cancellable_cancel(
            cancellable
        );

        g_object_unref(
            cancellable
        );
    }
}

gboolean background_task_is_cancelled(
    const BackgroundTask *task
)
{
    BackgroundTask *mutable_task = NULL;
    GCancellable *cancellable = NULL;

    BackgroundTaskState state;
    gboolean is_cancelled = FALSE;

    if (task == NULL)
    {
        return FALSE;
    }

    mutable_task = (BackgroundTask *) task;

    g_mutex_lock(
        &mutable_task->mutex
    );

    state = mutable_task->state;

    if (state ==
        BACKGROUND_TASK_STATE_CANCELLED)
    {
        is_cancelled = TRUE;
    }
    else if (state ==
                 BACKGROUND_TASK_STATE_RUNNING &&
             mutable_task->cancellable != NULL)
    {
        cancellable = g_object_ref(
            mutable_task->cancellable
        );
    }

    g_mutex_unlock(
        &mutable_task->mutex
    );

    if (cancellable != NULL)
    {
        is_cancelled = g_cancellable_is_cancelled(
            cancellable
        );

        g_object_unref(
            cancellable
        );
    }

    return is_cancelled;
}

void background_task_report_progress(
    BackgroundTask *task,
    double progress,
    const char *status_message
)
{
    char *new_status_message = NULL;

    if (task == NULL)
    {
        return;
    }

    /*
     * Une valeur NaN ne doit jamais être conservée.
     */
    if (progress != progress)
    {
        progress = 0.0;
    }
    else if (progress < 0.0)
    {
        progress = 0.0;
    }
    else if (progress > 1.0)
    {
        progress = 1.0;
    }

    if (status_message != NULL)
    {
        new_status_message = g_strdup(
            status_message
        );
    }

    g_mutex_lock(
        &task->mutex
    );

    if (task->state !=
        BACKGROUND_TASK_STATE_RUNNING)
    {
        g_mutex_unlock(
            &task->mutex
        );

        g_free(
            new_status_message
        );

        return;
    }

    task->progress = progress;

    g_free(
        task->status_message
    );

    task->status_message =
        new_status_message;

    g_mutex_unlock(
        &task->mutex
    );
}

const char *background_task_get_title(
    const BackgroundTask *task
)
{
    if (task == NULL)
    {
        return NULL;
    }

    /*
     * Le titre est immuable après la construction.
     */
    return task->title;
}

BackgroundTaskState background_task_get_state(
    const BackgroundTask *task
)
{
    BackgroundTask *mutable_task = NULL;
    BackgroundTaskState state;

    if (task == NULL)
    {
        return BACKGROUND_TASK_STATE_PENDING;
    }

    mutable_task = (BackgroundTask *) task;

    g_mutex_lock(
        &mutable_task->mutex
    );

    state = mutable_task->state;

    g_mutex_unlock(
        &mutable_task->mutex
    );

    return state;
}

double background_task_get_progress(
    const BackgroundTask *task
)
{
    BackgroundTask *mutable_task = NULL;
    double progress = 0.0;

    if (task == NULL)
    {
        return 0.0;
    }

    mutable_task = (BackgroundTask *) task;

    g_mutex_lock(
        &mutable_task->mutex
    );

    progress = mutable_task->progress;

    g_mutex_unlock(
        &mutable_task->mutex
    );

    return progress;
}

char *background_task_dup_status_message(
    const BackgroundTask *task
)
{
    BackgroundTask *mutable_task = NULL;
    char *status_message = NULL;

    if (task == NULL)
    {
        return NULL;
    }

    mutable_task = (BackgroundTask *) task;

    g_mutex_lock(
        &mutable_task->mutex
    );

    status_message = g_strdup(
        mutable_task->status_message
    );

    g_mutex_unlock(
        &mutable_task->mutex
    );

    return status_message;
}

gint64 background_task_get_started_at_us(
    const BackgroundTask *task
)
{
    BackgroundTask *mutable_task = NULL;
    gint64 started_at_us = 0;

    if (task == NULL)
    {
        return 0;
    }

    mutable_task = (BackgroundTask *) task;

    g_mutex_lock(
        &mutable_task->mutex
    );

    started_at_us =
        mutable_task->started_at_us;

    g_mutex_unlock(
        &mutable_task->mutex
    );

    return started_at_us;
}

gint64 background_task_get_finished_at_us(
    const BackgroundTask *task
)
{
    BackgroundTask *mutable_task = NULL;
    gint64 finished_at_us = 0;

    if (task == NULL)
    {
        return 0;
    }

    mutable_task = (BackgroundTask *) task;

    g_mutex_lock(
        &mutable_task->mutex
    );

    finished_at_us =
        mutable_task->finished_at_us;

    g_mutex_unlock(
        &mutable_task->mutex
    );

    return finished_at_us;
}

GError *background_task_dup_error(
    const BackgroundTask *task
)
{
    BackgroundTask *mutable_task = NULL;
    GError *error = NULL;

    if (task == NULL)
    {
        return NULL;
    }

    mutable_task = (BackgroundTask *) task;

    g_mutex_lock(
        &mutable_task->mutex
    );

    if (mutable_task->error != NULL)
    {
        error = g_error_copy(
            mutable_task->error
        );
    }

    g_mutex_unlock(
        &mutable_task->mutex
    );

    return error;
}

gpointer background_task_get_result(
    const BackgroundTask *task
)
{
    BackgroundTask *mutable_task = NULL;
    gpointer result = NULL;

    if (task == NULL)
    {
        return NULL;
    }

    mutable_task = (BackgroundTask *) task;

    g_mutex_lock(
        &mutable_task->mutex
    );

    if (mutable_task->state ==
        BACKGROUND_TASK_STATE_COMPLETED)
    {
        result = mutable_task->result;
    }

    g_mutex_unlock(
        &mutable_task->mutex
    );

    return result;
}
