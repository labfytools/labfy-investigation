/******************************************************************************
 * @file task_manager.c
 * @brief Implémentation du gestionnaire central des tâches.
 ******************************************************************************/

#include "core/task_manager.h"

/**
 * @struct TaskManagerCallbackSlot
 * @brief Stockage référencé du callback de changement.
 *
 * Le comptage de références permet d'appeler le callback hors du mutex
 * sans risquer que ses données soient détruites pendant l'appel.
 */
typedef struct
{
    gatomicrefcount reference_count;

    TaskManagerChangedCallback callback;
    gpointer user_data;
    GDestroyNotify user_data_destroy;
} TaskManagerCallbackSlot;

/**
 * @struct TaskManager
 * @brief État interne du gestionnaire de tâches.
 */
struct TaskManager
{
    GMutex mutex;
    GPtrArray *tasks;

    TaskManagerCallbackSlot *changed_slot;
};

/**
 * @brief Adapte background_task_unref() au type GDestroyNotify.
 *
 * @param user_data Pointeur vers BackgroundTask.
 */
static void task_manager_task_unref(
    gpointer user_data
)
{
    background_task_unref(
        user_data
    );
}

/**
 * @brief Crée un emplacement référencé pour un callback.
 *
 * @param callback Callback à conserver.
 * @param user_data Données du callback.
 * @param user_data_destroy Destructeur des données.
 *
 * @return Nouvel emplacement, ou NULL si aucune donnée n'est fournie.
 */
static TaskManagerCallbackSlot *
task_manager_callback_slot_new(
    TaskManagerChangedCallback callback,
    gpointer user_data,
    GDestroyNotify user_data_destroy
)
{
    TaskManagerCallbackSlot *slot = NULL;

    if (callback == NULL &&
        user_data == NULL)
    {
        return NULL;
    }

    slot = g_new0(
        TaskManagerCallbackSlot,
        1
    );

    g_atomic_ref_count_init(
        &slot->reference_count
    );

    slot->callback = callback;
    slot->user_data = user_data;
    slot->user_data_destroy =
        user_data_destroy;

    return slot;
}

/**
 * @brief Ajoute une référence à un emplacement de callback.
 *
 * @param slot Emplacement concerné.
 *
 * @return L'emplacement fourni, ou NULL.
 */
static TaskManagerCallbackSlot *
task_manager_callback_slot_ref(
    TaskManagerCallbackSlot *slot
)
{
    if (slot == NULL)
    {
        return NULL;
    }

    g_atomic_ref_count_inc(
        &slot->reference_count
    );

    return slot;
}

/**
 * @brief Libère une référence à un emplacement de callback.
 *
 * @param slot Emplacement concerné.
 */
static void task_manager_callback_slot_unref(
    TaskManagerCallbackSlot *slot
)
{
    gpointer user_data = NULL;
    GDestroyNotify user_data_destroy = NULL;

    if (slot == NULL)
    {
        return;
    }

    if (!g_atomic_ref_count_dec(
            &slot->reference_count
        ))
    {
        return;
    }

    user_data = slot->user_data;
    user_data_destroy =
        slot->user_data_destroy;

    slot->callback = NULL;
    slot->user_data = NULL;
    slot->user_data_destroy = NULL;

    if (user_data != NULL &&
        user_data_destroy != NULL)
    {
        user_data_destroy(
            user_data
        );
    }

    g_free(
        slot
    );
}

/**
 * @brief Signale un changement du gestionnaire.
 *
 * Le callback est appelé sans verrouiller le mutex du gestionnaire.
 *
 * @param task_manager Gestionnaire concerné.
 */
static void task_manager_notify_changed(
    TaskManager *task_manager
)
{
    TaskManagerCallbackSlot *slot = NULL;

    if (task_manager == NULL)
    {
        return;
    }

    g_mutex_lock(
        &task_manager->mutex
    );

    slot = task_manager_callback_slot_ref(
        task_manager->changed_slot
    );

    g_mutex_unlock(
        &task_manager->mutex
    );

    if (slot != NULL &&
        slot->callback != NULL)
    {
        slot->callback(
            task_manager,
            slot->user_data
        );
    }

    task_manager_callback_slot_unref(
        slot
    );
}

/**
 * @brief Indique si un état représente une tâche terminée.
 *
 * @param state État à examiner.
 *
 * @return TRUE si la tâche est terminée, sinon FALSE.
 */
static gboolean task_manager_state_is_finished(
    BackgroundTaskState state
)
{
    return
        state == BACKGROUND_TASK_STATE_COMPLETED ||
        state == BACKGROUND_TASK_STATE_FAILED ||
        state == BACKGROUND_TASK_STATE_CANCELLED;
}

GQuark task_manager_error_quark(void)
{
    return g_quark_from_static_string(
        "labfy-investigation-task-manager-error"
    );
}

TaskManager *task_manager_new(void)
{
    TaskManager *task_manager = NULL;

    task_manager = g_new0(
        TaskManager,
        1
    );

    g_mutex_init(
        &task_manager->mutex
    );

    task_manager->tasks =
        g_ptr_array_new_with_free_func(
            task_manager_task_unref
        );

    if (task_manager->tasks == NULL)
    {
        g_mutex_clear(
            &task_manager->mutex
        );

        g_free(
            task_manager
        );

        return NULL;
    }

    return task_manager;
}

void task_manager_free(
    TaskManager *task_manager
)
{
    GPtrArray *tasks = NULL;

    TaskManagerCallbackSlot *changed_slot =
        NULL;

    if (task_manager == NULL)
    {
        return;
    }

    g_mutex_lock(
        &task_manager->mutex
    );

    tasks = task_manager->tasks;
    task_manager->tasks = NULL;

    changed_slot =
        task_manager->changed_slot;

    task_manager->changed_slot = NULL;

    g_mutex_unlock(
        &task_manager->mutex
    );

    /*
     * La libération des tâches et des données utilisateur se fait
     * hors du mutex.
     */
    if (tasks != NULL)
    {
        g_ptr_array_unref(
            tasks
        );
    }

    task_manager_callback_slot_unref(
        changed_slot
    );

    g_mutex_clear(
        &task_manager->mutex
    );

    g_free(
        task_manager
    );
}

gboolean task_manager_add(
    TaskManager *task_manager,
    BackgroundTask *task,
    GError **error
)
{
    gsize index = 0;
    gboolean already_added = FALSE;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (task_manager == NULL ||
        task == NULL)
    {
        g_set_error_literal(
            error,
            TASK_MANAGER_ERROR,
            TASK_MANAGER_ERROR_INVALID_ARGUMENT,
            "Le gestionnaire ou la tâche est invalide."
        );

        return FALSE;
    }

    g_mutex_lock(
        &task_manager->mutex
    );

    for (index = 0;
         index < task_manager->tasks->len;
         index++)
    {
        if (g_ptr_array_index(
                task_manager->tasks,
                index
            ) == task)
        {
            already_added = TRUE;
            break;
        }
    }

    if (!already_added)
    {
        g_ptr_array_add(
            task_manager->tasks,
            background_task_ref(task)
        );
    }

    g_mutex_unlock(
        &task_manager->mutex
    );

    if (already_added)
    {
        g_set_error_literal(
            error,
            TASK_MANAGER_ERROR,
            TASK_MANAGER_ERROR_ALREADY_ADDED,
            "Cette tâche est déjà suivie par le gestionnaire."
        );

        return FALSE;
    }

    task_manager_notify_changed(
        task_manager
    );

    return TRUE;
}

gsize task_manager_get_count(
    const TaskManager *task_manager
)
{
    TaskManager *mutable_task_manager = NULL;
    gsize count = 0;

    if (task_manager == NULL)
    {
        return 0;
    }

    mutable_task_manager =
        (TaskManager *) task_manager;

    g_mutex_lock(
        &mutable_task_manager->mutex
    );

    if (mutable_task_manager->tasks != NULL)
    {
        count =
            mutable_task_manager->tasks->len;
    }

    g_mutex_unlock(
        &mutable_task_manager->mutex
    );

    return count;
}

BackgroundTask *task_manager_get_task(
    const TaskManager *task_manager,
    gsize index
)
{
    TaskManager *mutable_task_manager = NULL;
    BackgroundTask *task = NULL;

    if (task_manager == NULL)
    {
        return NULL;
    }

    mutable_task_manager =
        (TaskManager *) task_manager;

    g_mutex_lock(
        &mutable_task_manager->mutex
    );

    if (mutable_task_manager->tasks != NULL &&
        index < mutable_task_manager->tasks->len)
    {
        task = background_task_ref(
            g_ptr_array_index(
                mutable_task_manager->tasks,
                index
            )
        );
    }

    g_mutex_unlock(
        &mutable_task_manager->mutex
    );

    return task;
}

gboolean task_manager_remove(
    TaskManager *task_manager,
    BackgroundTask *task
)
{
    BackgroundTask *removed_task = NULL;

    gsize index = 0;

    if (task_manager == NULL ||
        task == NULL)
    {
        return FALSE;
    }

    g_mutex_lock(
        &task_manager->mutex
    );

    for (index = 0;
         index < task_manager->tasks->len;
         index++)
    {
        if (g_ptr_array_index(
                task_manager->tasks,
                index
            ) == task)
        {
            removed_task = g_ptr_array_steal_index(
                task_manager->tasks,
                index
            );

            break;
        }
    }

    g_mutex_unlock(
        &task_manager->mutex
    );

    if (removed_task == NULL)
    {
        return FALSE;
    }

    /*
     * La référence du gestionnaire est libérée hors du mutex.
     */
    background_task_unref(
        removed_task
    );

    task_manager_notify_changed(
        task_manager
    );

    return TRUE;
}

gsize task_manager_remove_finished(
    TaskManager *task_manager
)
{
    GPtrArray *removed_tasks = NULL;

    gsize index = 0;
    gsize removed_count = 0;

    if (task_manager == NULL)
    {
        return 0;
    }

    removed_tasks =
        g_ptr_array_new_with_free_func(
            task_manager_task_unref
        );

    g_mutex_lock(
        &task_manager->mutex
    );

    /*
     * Le parcours se fait à l'envers afin que la suppression d'un
     * élément ne décale pas les index encore à examiner.
     */
    index = task_manager->tasks->len;

    while (index > 0)
    {
        BackgroundTask *task = NULL;
        BackgroundTaskState state;

        index--;

        task = g_ptr_array_index(
            task_manager->tasks,
            index
        );

        state = background_task_get_state(
            task
        );

        if (task_manager_state_is_finished(
                state
            ))
        {
            BackgroundTask *removed_task = NULL;

            removed_task = g_ptr_array_steal_index(
                task_manager->tasks,
                index
            );

            g_ptr_array_add(
                removed_tasks,
                removed_task
            );
        }
    }

    removed_count =
        removed_tasks->len;

    g_mutex_unlock(
        &task_manager->mutex
    );

    /*
     * Les références retirées sont libérées hors du mutex.
     */
    g_ptr_array_unref(
        removed_tasks
    );

    if (removed_count > 0)
    {
        task_manager_notify_changed(
            task_manager
        );
    }

    return removed_count;
}

void task_manager_cancel_all(
    TaskManager *task_manager
)
{
    GPtrArray *task_snapshot = NULL;

    gsize index = 0;
    gboolean cancellation_requested = FALSE;

    if (task_manager == NULL)
    {
        return;
    }

    task_snapshot =
        g_ptr_array_new_with_free_func(
            task_manager_task_unref
        );

    g_mutex_lock(
        &task_manager->mutex
    );

    for (index = 0;
         index < task_manager->tasks->len;
         index++)
    {
        BackgroundTask *task = NULL;

        task = g_ptr_array_index(
            task_manager->tasks,
            index
        );

        g_ptr_array_add(
            task_snapshot,
            background_task_ref(task)
        );
    }

    g_mutex_unlock(
        &task_manager->mutex
    );

    /*
     * L'annulation et la lecture des états sont effectuées hors
     * du mutex du gestionnaire.
     */
    for (index = 0;
         index < task_snapshot->len;
         index++)
    {
        BackgroundTask *task = NULL;

        task = g_ptr_array_index(
            task_snapshot,
            index
        );

        if (background_task_get_state(task) ==
            BACKGROUND_TASK_STATE_RUNNING)
        {
            background_task_cancel(
                task
            );

            cancellation_requested = TRUE;
        }
    }

    g_ptr_array_unref(
        task_snapshot
    );

    if (cancellation_requested)
    {
        task_manager_notify_changed(
            task_manager
        );
    }
}

void task_manager_set_changed_callback(
    TaskManager *task_manager,
    TaskManagerChangedCallback callback,
    gpointer user_data,
    GDestroyNotify user_data_destroy
)
{
    TaskManagerCallbackSlot *new_slot = NULL;
    TaskManagerCallbackSlot *old_slot = NULL;

    if (task_manager == NULL)
    {
        /*
         * Aucun transfert de propriété n'a lieu lorsque le
         * gestionnaire est invalide.
         */
        return;
    }

    new_slot = task_manager_callback_slot_new(
        callback,
        user_data,
        user_data_destroy
    );

    g_mutex_lock(
        &task_manager->mutex
    );

    old_slot =
        task_manager->changed_slot;

    task_manager->changed_slot =
        new_slot;

    g_mutex_unlock(
        &task_manager->mutex
    );

    /*
     * L'ancien destructeur utilisateur est appelé hors du mutex.
     */
    task_manager_callback_slot_unref(
        old_slot
    );
}
