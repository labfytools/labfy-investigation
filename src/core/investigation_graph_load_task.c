/******************************************************************************
 * @file investigation_graph_load_task.c
 * @brief Chargement asynchrone du graphe métier d'une enquête.
 ******************************************************************************/

#include "core/investigation_graph_load_task.h"

#include "core/background_task.h"
#include "core/investigation_graph_loader.h"

#include "database/database.h"

#include <gio/gio.h>
#include <glib.h>

#ifdef INVESTIGATION_GRAPH_LOAD_TASK_ENABLE_TEST_HOOKS
#include "core/investigation_graph_load_task_test.h"
#endif

/**
 * @brief Données privées copiées pour une exécution du worker.
 */
typedef struct
{
    char *database_path;
} InvestigationGraphLoadWorkerData;

/**
 * @brief Résultat possédé par BackgroundTask jusqu'au callback final.
 *
 * Le callback retire graph_model de ce conteneur avant de le transmettre à
 * l'appelant. Le destructeur du conteneur ne détruit donc que les graphes qui
 * n'ont pas été remis.
 */
typedef struct
{
    InvestigationGraphModel *graph_model;
} InvestigationGraphLoadResult;

/**
 * @brief Contexte maintenant la tâche publique en vie jusqu'à la fin.
 */
typedef struct
{
    InvestigationGraphLoadTask *load_task;
} InvestigationGraphLoadCompletionContext;

/**
 * @brief État privé de la tâche de chargement.
 */
struct InvestigationGraphLoadTask
{
    gatomicrefcount reference_count;
    GMutex mutex;

    char *database_path;

    BackgroundTask *background_task;

    InvestigationGraphLoadTaskCallback callback;
    gpointer user_data;
    GDestroyNotify user_data_destroy;

    gboolean running;
    gboolean disposed;
};


#ifdef INVESTIGATION_GRAPH_LOAD_TASK_ENABLE_TEST_HOOKS

/**
 * @brief État global des synchronisations déterministes réservées aux tests.
 */
typedef struct
{
    GMutex mutex;
    GCond condition;

    gboolean pause_before_open;
    gboolean reached_before_open;
    gboolean release_before_open;

    gboolean pause_after_open;
    gboolean reached_after_open;
    gboolean release_after_open;

    guint completion_count;
} InvestigationGraphLoadTaskTestHooks;

static InvestigationGraphLoadTaskTestHooks
investigation_graph_load_task_test_hooks;

static gsize investigation_graph_load_task_test_hooks_initialized =
    0;

/**
 * @brief Initialise une seule fois les primitives des hooks de test.
 */
static void investigation_graph_load_task_test_ensure_initialized(void)
{
    if (g_once_init_enter(
            &investigation_graph_load_task_test_hooks_initialized
        ))
    {
        g_mutex_init(
            &investigation_graph_load_task_test_hooks.mutex
        );

        g_cond_init(
            &investigation_graph_load_task_test_hooks.condition
        );

        g_once_init_leave(
            &investigation_graph_load_task_test_hooks_initialized,
            1
        );
    }
}

/**
 * @brief Suspend éventuellement le worker avant l'ouverture SQLite.
 */
static void investigation_graph_load_task_test_wait_before_open_hook(void)
{
    investigation_graph_load_task_test_ensure_initialized();

    g_mutex_lock(
        &investigation_graph_load_task_test_hooks.mutex
    );

    investigation_graph_load_task_test_hooks.reached_before_open =
        TRUE;

    g_cond_broadcast(
        &investigation_graph_load_task_test_hooks.condition
    );

    while (investigation_graph_load_task_test_hooks.pause_before_open &&
           !investigation_graph_load_task_test_hooks.release_before_open)
    {
        g_cond_wait(
            &investigation_graph_load_task_test_hooks.condition,
            &investigation_graph_load_task_test_hooks.mutex
        );
    }

    g_mutex_unlock(
        &investigation_graph_load_task_test_hooks.mutex
    );
}

/**
 * @brief Suspend éventuellement le worker après l'ouverture SQLite.
 */
static void investigation_graph_load_task_test_wait_after_open_hook(void)
{
    investigation_graph_load_task_test_ensure_initialized();

    g_mutex_lock(
        &investigation_graph_load_task_test_hooks.mutex
    );

    investigation_graph_load_task_test_hooks.reached_after_open =
        TRUE;

    g_cond_broadcast(
        &investigation_graph_load_task_test_hooks.condition
    );

    while (investigation_graph_load_task_test_hooks.pause_after_open &&
           !investigation_graph_load_task_test_hooks.release_after_open)
    {
        g_cond_wait(
            &investigation_graph_load_task_test_hooks.condition,
            &investigation_graph_load_task_test_hooks.mutex
        );
    }

    g_mutex_unlock(
        &investigation_graph_load_task_test_hooks.mutex
    );
}

/**
 * @brief Signale qu'une finalisation BackgroundTask a été exécutée.
 */
static void investigation_graph_load_task_test_mark_completion(void)
{
    investigation_graph_load_task_test_ensure_initialized();

    g_mutex_lock(
        &investigation_graph_load_task_test_hooks.mutex
    );

    investigation_graph_load_task_test_hooks.completion_count++;

    g_cond_broadcast(
        &investigation_graph_load_task_test_hooks.condition
    );

    g_mutex_unlock(
        &investigation_graph_load_task_test_hooks.mutex
    );
}

void investigation_graph_load_task_test_reset(void)
{
    investigation_graph_load_task_test_ensure_initialized();

    g_mutex_lock(
        &investigation_graph_load_task_test_hooks.mutex
    );

    investigation_graph_load_task_test_hooks.pause_before_open =
        FALSE;

    investigation_graph_load_task_test_hooks.reached_before_open =
        FALSE;

    investigation_graph_load_task_test_hooks.release_before_open =
        FALSE;

    investigation_graph_load_task_test_hooks.pause_after_open =
        FALSE;

    investigation_graph_load_task_test_hooks.reached_after_open =
        FALSE;

    investigation_graph_load_task_test_hooks.release_after_open =
        FALSE;

    investigation_graph_load_task_test_hooks.completion_count =
        0;

    g_cond_broadcast(
        &investigation_graph_load_task_test_hooks.condition
    );

    g_mutex_unlock(
        &investigation_graph_load_task_test_hooks.mutex
    );
}

void investigation_graph_load_task_test_pause_before_open(void)
{
    investigation_graph_load_task_test_ensure_initialized();

    g_mutex_lock(
        &investigation_graph_load_task_test_hooks.mutex
    );

    investigation_graph_load_task_test_hooks.pause_before_open =
        TRUE;

    investigation_graph_load_task_test_hooks.reached_before_open =
        FALSE;

    investigation_graph_load_task_test_hooks.release_before_open =
        FALSE;

    g_mutex_unlock(
        &investigation_graph_load_task_test_hooks.mutex
    );
}

void investigation_graph_load_task_test_wait_until_before_open(void)
{
    investigation_graph_load_task_test_ensure_initialized();

    g_mutex_lock(
        &investigation_graph_load_task_test_hooks.mutex
    );

    while (!investigation_graph_load_task_test_hooks.reached_before_open)
    {
        g_cond_wait(
            &investigation_graph_load_task_test_hooks.condition,
            &investigation_graph_load_task_test_hooks.mutex
        );
    }

    g_mutex_unlock(
        &investigation_graph_load_task_test_hooks.mutex
    );
}

void investigation_graph_load_task_test_release_before_open(void)
{
    investigation_graph_load_task_test_ensure_initialized();

    g_mutex_lock(
        &investigation_graph_load_task_test_hooks.mutex
    );

    investigation_graph_load_task_test_hooks.release_before_open =
        TRUE;

    g_cond_broadcast(
        &investigation_graph_load_task_test_hooks.condition
    );

    g_mutex_unlock(
        &investigation_graph_load_task_test_hooks.mutex
    );
}

void investigation_graph_load_task_test_pause_after_open(void)
{
    investigation_graph_load_task_test_ensure_initialized();

    g_mutex_lock(
        &investigation_graph_load_task_test_hooks.mutex
    );

    investigation_graph_load_task_test_hooks.pause_after_open =
        TRUE;

    investigation_graph_load_task_test_hooks.reached_after_open =
        FALSE;

    investigation_graph_load_task_test_hooks.release_after_open =
        FALSE;

    g_mutex_unlock(
        &investigation_graph_load_task_test_hooks.mutex
    );
}

void investigation_graph_load_task_test_wait_until_after_open(void)
{
    investigation_graph_load_task_test_ensure_initialized();

    g_mutex_lock(
        &investigation_graph_load_task_test_hooks.mutex
    );

    while (!investigation_graph_load_task_test_hooks.reached_after_open)
    {
        g_cond_wait(
            &investigation_graph_load_task_test_hooks.condition,
            &investigation_graph_load_task_test_hooks.mutex
        );
    }

    g_mutex_unlock(
        &investigation_graph_load_task_test_hooks.mutex
    );
}

void investigation_graph_load_task_test_release_after_open(void)
{
    investigation_graph_load_task_test_ensure_initialized();

    g_mutex_lock(
        &investigation_graph_load_task_test_hooks.mutex
    );

    investigation_graph_load_task_test_hooks.release_after_open =
        TRUE;

    g_cond_broadcast(
        &investigation_graph_load_task_test_hooks.condition
    );

    g_mutex_unlock(
        &investigation_graph_load_task_test_hooks.mutex
    );
}

guint investigation_graph_load_task_test_get_completion_count(void)
{
    guint completion_count =
        0;

    investigation_graph_load_task_test_ensure_initialized();

    g_mutex_lock(
        &investigation_graph_load_task_test_hooks.mutex
    );

    completion_count =
        investigation_graph_load_task_test_hooks.completion_count;

    g_mutex_unlock(
        &investigation_graph_load_task_test_hooks.mutex
    );

    return completion_count;
}

#endif

/**
 * @brief Enregistre une erreur littérale.
 */
static void investigation_graph_load_task_set_error_literal(
    GError **error,
    InvestigationGraphLoadTaskError error_code,
    const char *message
)
{
    if (error == NULL ||
        *error != NULL)
    {
        return;
    }

    g_set_error_literal(
        error,
        INVESTIGATION_GRAPH_LOAD_TASK_ERROR,
        error_code,
        message
    );
}

/**
 * @brief Enregistre une erreur en conservant le message d'origine.
 */
static void investigation_graph_load_task_set_nested_error(
    GError **error,
    InvestigationGraphLoadTaskError error_code,
    const char *context,
    const GError *nested_error
)
{
    if (error == NULL ||
        *error != NULL)
    {
        return;
    }

    g_set_error(
        error,
        INVESTIGATION_GRAPH_LOAD_TASK_ERROR,
        error_code,
        "%s : %s",
        context,
        nested_error != NULL
            ? nested_error->message
            : "erreur inconnue"
    );
}

/**
 * @brief Libère les données privées d'un worker.
 */
static void investigation_graph_load_worker_data_free(
    gpointer user_data
)
{
    InvestigationGraphLoadWorkerData *worker_data =
        user_data;

    if (worker_data == NULL)
    {
        return;
    }

    g_free(
        worker_data->database_path
    );

    g_free(
        worker_data
    );
}

/**
 * @brief Libère un résultat qui n'a pas été transféré au callback.
 */
static void investigation_graph_load_result_free(
    gpointer user_data
)
{
    InvestigationGraphLoadResult *load_result =
        user_data;

    if (load_result == NULL)
    {
        return;
    }

    investigation_graph_model_free(
        load_result->graph_model
    );

    load_result->graph_model =
        NULL;

    g_free(
        load_result
    );
}

/**
 * @brief Ajoute une référence interne à la tâche publique.
 */
static InvestigationGraphLoadTask *
investigation_graph_load_task_ref_internal(
    InvestigationGraphLoadTask *load_task
)
{
    if (load_task == NULL)
    {
        return NULL;
    }

    g_atomic_ref_count_inc(
        &load_task->reference_count
    );

    return load_task;
}

/**
 * @brief Détruit définitivement une tâche dont la dernière référence disparaît.
 */
static void investigation_graph_load_task_destroy(
    InvestigationGraphLoadTask *load_task
)
{
    BackgroundTask *background_task =
        NULL;

    gpointer user_data =
        NULL;

    GDestroyNotify user_data_destroy =
        NULL;

    if (load_task == NULL)
    {
        return;
    }

    background_task =
        load_task->background_task;

    user_data =
        load_task->user_data;

    user_data_destroy =
        load_task->user_data_destroy;

    load_task->background_task =
        NULL;

    load_task->callback =
        NULL;

    load_task->user_data =
        NULL;

    load_task->user_data_destroy =
        NULL;

    if (background_task != NULL)
    {
        background_task_cancel(
            background_task
        );

        background_task_unref(
            background_task
        );
    }

    if (user_data != NULL &&
        user_data_destroy != NULL)
    {
        user_data_destroy(
            user_data
        );
    }

    g_free(
        load_task->database_path
    );

    load_task->database_path =
        NULL;

    g_mutex_clear(
        &load_task->mutex
    );

    g_free(
        load_task
    );
}

/**
 * @brief Libère une référence interne ou publique.
 */
static void investigation_graph_load_task_unref_internal(
    InvestigationGraphLoadTask *load_task
)
{
    if (load_task == NULL)
    {
        return;
    }

    if (!g_atomic_ref_count_dec(
            &load_task->reference_count
        ))
    {
        return;
    }

    investigation_graph_load_task_destroy(
        load_task
    );
}

/**
 * @brief Libère le contexte de finalisation.
 */
static void investigation_graph_load_completion_context_free(
    gpointer user_data
)
{
    InvestigationGraphLoadCompletionContext *completion_context =
        user_data;

    if (completion_context == NULL)
    {
        return;
    }

    investigation_graph_load_task_unref_internal(
        completion_context->load_task
    );

    completion_context->load_task =
        NULL;

    g_free(
        completion_context
    );
}

/**
 * @brief Vérifie l'annulation du worker.
 */
static gboolean investigation_graph_load_task_check_cancelled(
    GCancellable *cancellable,
    GError **error
)
{
    if (cancellable == NULL)
    {
        investigation_graph_load_task_set_error_literal(
            error,
            INVESTIGATION_GRAPH_LOAD_TASK_ERROR_INTERNAL,
            "L'objet d'annulation du chargement est absent."
        );

        return TRUE;
    }

    return g_cancellable_set_error_if_cancelled(
        cancellable,
        error
    );
}

/**
 * @brief Exécute le chargement dans un thread secondaire.
 */
static gboolean investigation_graph_load_task_worker(
    BackgroundTask *background_task,
    GCancellable *cancellable,
    gpointer worker_data_pointer,
    gpointer *result,
    GError **error
)
{
    InvestigationGraphLoadWorkerData *worker_data =
        worker_data_pointer;

    Database *database =
        NULL;

    InvestigationGraphLoader *graph_loader =
        NULL;

    InvestigationGraphModel *graph_model =
        NULL;

    InvestigationGraphLoadResult *load_result =
        NULL;

    GError *component_error =
        NULL;

    if (background_task == NULL ||
        cancellable == NULL ||
        worker_data == NULL ||
        worker_data->database_path == NULL ||
        result == NULL ||
        error == NULL)
    {
        investigation_graph_load_task_set_error_literal(
            error,
            INVESTIGATION_GRAPH_LOAD_TASK_ERROR_INTERNAL,
            "Le contexte du worker de chargement est invalide."
        );

        return FALSE;
    }

    *result =
        NULL;

    if (investigation_graph_load_task_check_cancelled(
            cancellable,
            error
        ))
    {
        return FALSE;
    }

#ifdef INVESTIGATION_GRAPH_LOAD_TASK_ENABLE_TEST_HOOKS
    investigation_graph_load_task_test_wait_before_open_hook();
#endif

    if (investigation_graph_load_task_check_cancelled(
            cancellable,
            error
        ))
    {
        return FALSE;
    }

    background_task_report_progress(
        background_task,
        0.05,
        "Ouverture de la base de données"
    );

    database =
        database_open(
            worker_data->database_path
        );

    if (database == NULL)
    {
        g_set_error(
            error,
            INVESTIGATION_GRAPH_LOAD_TASK_ERROR,
            INVESTIGATION_GRAPH_LOAD_TASK_ERROR_DATABASE_OPEN,
            "Impossible d'ouvrir la base SQLite : %s",
            worker_data->database_path
        );

        return FALSE;
    }

#ifdef INVESTIGATION_GRAPH_LOAD_TASK_ENABLE_TEST_HOOKS
    investigation_graph_load_task_test_wait_after_open_hook();
#endif

    if (investigation_graph_load_task_check_cancelled(
            cancellable,
            error
        ))
    {
        goto failure;
    }

    background_task_report_progress(
        background_task,
        0.20,
        "Préparation du chargeur de graphe"
    );

    graph_loader =
        investigation_graph_loader_new(
            database,
            &component_error
        );

    if (graph_loader == NULL)
    {
        investigation_graph_load_task_set_nested_error(
            error,
            INVESTIGATION_GRAPH_LOAD_TASK_ERROR_LOAD,
            "Impossible de préparer le chargement du graphe",
            component_error
        );

        goto failure;
    }

    g_clear_error(
        &component_error
    );

    background_task_report_progress(
        background_task,
        0.35,
        "Chargement des entités et des relations"
    );

    graph_model =
        investigation_graph_loader_load(
            graph_loader,
            &component_error
        );

    if (graph_model == NULL)
    {
        investigation_graph_load_task_set_nested_error(
            error,
            INVESTIGATION_GRAPH_LOAD_TASK_ERROR_LOAD,
            "Impossible de charger le graphe d'enquête",
            component_error
        );

        goto failure;
    }

    g_clear_error(
        &component_error
    );

    if (investigation_graph_load_task_check_cancelled(
            cancellable,
            error
        ))
    {
        goto failure;
    }

    investigation_graph_loader_free(
        graph_loader
    );

    graph_loader =
        NULL;

    database_close(
        database
    );

    database =
        NULL;

    if (investigation_graph_load_task_check_cancelled(
            cancellable,
            error
        ))
    {
        goto failure;
    }

    load_result =
        g_try_new0(
            InvestigationGraphLoadResult,
            1
        );

    if (load_result == NULL)
    {
        investigation_graph_load_task_set_error_literal(
            error,
            INVESTIGATION_GRAPH_LOAD_TASK_ERROR_MEMORY,
            "Impossible d'allouer le résultat du chargement."
        );

        goto failure;
    }

    load_result->graph_model =
        graph_model;

    graph_model =
        NULL;

    if (investigation_graph_load_task_check_cancelled(
            cancellable,
            error
        ))
    {
        investigation_graph_load_result_free(
            load_result
        );

        return FALSE;
    }

    background_task_report_progress(
        background_task,
        1.0,
        "Graphe d'enquête chargé"
    );

    *result =
        load_result;

    return TRUE;

failure:

    g_clear_error(
        &component_error
    );

    investigation_graph_model_free(
        graph_model
    );

    investigation_graph_loader_free(
        graph_loader
    );

    if (database != NULL)
    {
        database_close(
            database
        );
    }

    return FALSE;
}

/**
 * @brief Produit l'erreur publique correspondant à l'état final.
 */
static GError *investigation_graph_load_task_create_callback_error(
    BackgroundTask *background_task
)
{
    BackgroundTaskState background_state;

    GError *background_error =
        NULL;

    background_state =
        background_task_get_state(
            background_task
        );

    if (background_state ==
        BACKGROUND_TASK_STATE_CANCELLED)
    {
        return g_error_new_literal(
            INVESTIGATION_GRAPH_LOAD_TASK_ERROR,
            INVESTIGATION_GRAPH_LOAD_TASK_ERROR_CANCELLED,
            "Le chargement du graphe d'enquête a été annulé."
        );
    }

    background_error =
        background_task_dup_error(
            background_task
        );

    if (background_error == NULL)
    {
        return g_error_new_literal(
            INVESTIGATION_GRAPH_LOAD_TASK_ERROR,
            INVESTIGATION_GRAPH_LOAD_TASK_ERROR_INTERNAL,
            "La tâche a échoué sans fournir d'erreur."
        );
    }

    if (background_error->domain ==
        INVESTIGATION_GRAPH_LOAD_TASK_ERROR)
    {
        return background_error;
    }

    {
        GError *callback_error =
            g_error_new(
                INVESTIGATION_GRAPH_LOAD_TASK_ERROR,
                INVESTIGATION_GRAPH_LOAD_TASK_ERROR_INTERNAL,
                "Échec interne de la tâche de fond : %s",
                background_error->message
            );

        g_error_free(
            background_error
        );

        return callback_error;
    }
}

/**
 * @brief Finalise une exécution sur le contexte ayant lancé BackgroundTask.
 */
static void investigation_graph_load_task_on_completed(
    BackgroundTask *background_task,
    gpointer user_data
)
{
    InvestigationGraphLoadCompletionContext *completion_context =
        user_data;

    InvestigationGraphLoadTask *load_task =
        NULL;

    InvestigationGraphLoadTaskCallback callback =
        NULL;

    gpointer callback_data =
        NULL;

    GDestroyNotify callback_data_destroy =
        NULL;

    InvestigationGraphModel *graph_model =
        NULL;

    GError *callback_error =
        NULL;

    gboolean disposed =
        FALSE;

    if (background_task == NULL ||
        completion_context == NULL ||
        completion_context->load_task == NULL)
    {
        return;
    }

    load_task =
        completion_context->load_task;

    g_mutex_lock(
        &load_task->mutex
    );

    disposed =
        load_task->disposed;

    if (load_task->background_task ==
        background_task)
    {
        load_task->background_task =
            NULL;

        load_task->running =
            FALSE;
    }

    if (!disposed)
    {
        callback =
            load_task->callback;

        callback_data =
            load_task->user_data;

        callback_data_destroy =
            load_task->user_data_destroy;
    }

    load_task->callback =
        NULL;

    load_task->user_data =
        NULL;

    load_task->user_data_destroy =
        NULL;

    g_mutex_unlock(
        &load_task->mutex
    );

    if (callback != NULL)
    {
        if (background_task_get_state(
                background_task
            ) == BACKGROUND_TASK_STATE_COMPLETED)
        {
            InvestigationGraphLoadResult *load_result =
                background_task_get_result(
                    background_task
                );

            if (load_result != NULL &&
                load_result->graph_model != NULL)
            {
                graph_model =
                    load_result->graph_model;

                load_result->graph_model =
                    NULL;
            }
            else
            {
                callback_error =
                    g_error_new_literal(
                        INVESTIGATION_GRAPH_LOAD_TASK_ERROR,
                        INVESTIGATION_GRAPH_LOAD_TASK_ERROR_INTERNAL,
                        "La tâche terminée ne contient aucun graphe."
                    );
            }
        }
        else
        {
            callback_error =
                investigation_graph_load_task_create_callback_error(
                    background_task
                );
        }

        callback(
            load_task,
            graph_model,
            callback_error,
            callback_data
        );
    }

    if (callback_data != NULL &&
        callback_data_destroy != NULL)
    {
        callback_data_destroy(
            callback_data
        );
    }

    g_clear_error(
        &callback_error
    );

    /*
     * Libère la référence à BackgroundTask possédée par le wrapper.
     * La référence interne de BackgroundTask protège encore l'appel courant.
     */
    background_task_unref(
        background_task
    );

#ifdef INVESTIGATION_GRAPH_LOAD_TASK_ENABLE_TEST_HOOKS
    investigation_graph_load_task_test_mark_completion();
#endif
}

GQuark investigation_graph_load_task_error_quark(void)
{
    return g_quark_from_static_string(
        "investigation-graph-load-task-error-quark"
    );
}

InvestigationGraphLoadTask *investigation_graph_load_task_new(
    const char *database_path,
    GError **error
)
{
    InvestigationGraphLoadTask *load_task =
        NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (database_path == NULL ||
        database_path[0] == '\0')
    {
        investigation_graph_load_task_set_error_literal(
            error,
            INVESTIGATION_GRAPH_LOAD_TASK_ERROR_INVALID_ARGUMENT,
            "Le chemin de la base SQLite est obligatoire."
        );

        return NULL;
    }

    load_task =
        g_try_new0(
            InvestigationGraphLoadTask,
            1
        );

    if (load_task == NULL)
    {
        investigation_graph_load_task_set_error_literal(
            error,
            INVESTIGATION_GRAPH_LOAD_TASK_ERROR_MEMORY,
            "Impossible d'allouer la tâche de chargement."
        );

        return NULL;
    }

    load_task->database_path =
        g_strdup(
            database_path
        );

    if (load_task->database_path == NULL)
    {
        g_free(
            load_task
        );

        investigation_graph_load_task_set_error_literal(
            error,
            INVESTIGATION_GRAPH_LOAD_TASK_ERROR_MEMORY,
            "Impossible de copier le chemin de la base SQLite."
        );

        return NULL;
    }

    g_atomic_ref_count_init(
        &load_task->reference_count
    );

    g_mutex_init(
        &load_task->mutex
    );

    load_task->running =
        FALSE;

    load_task->disposed =
        FALSE;

    return load_task;
}

void investigation_graph_load_task_free(
    InvestigationGraphLoadTask *load_task
)
{
    BackgroundTask *background_task =
        NULL;

    gpointer user_data =
        NULL;

    GDestroyNotify user_data_destroy =
        NULL;

    if (load_task == NULL)
    {
        return;
    }

    g_mutex_lock(
        &load_task->mutex
    );

    load_task->disposed =
        TRUE;

    user_data =
        load_task->user_data;

    user_data_destroy =
        load_task->user_data_destroy;

    load_task->callback =
        NULL;

    load_task->user_data =
        NULL;

    load_task->user_data_destroy =
        NULL;

    if (load_task->running &&
        load_task->background_task != NULL)
    {
        background_task =
            background_task_ref(
                load_task->background_task
            );
    }

    g_mutex_unlock(
        &load_task->mutex
    );

    if (user_data != NULL &&
        user_data_destroy != NULL)
    {
        user_data_destroy(
            user_data
        );
    }

    if (background_task != NULL)
    {
        background_task_cancel(
            background_task
        );

        background_task_unref(
            background_task
        );
    }

    investigation_graph_load_task_unref_internal(
        load_task
    );
}

gboolean investigation_graph_load_task_start(
    InvestigationGraphLoadTask *load_task,
    InvestigationGraphLoadTaskCallback callback,
    gpointer user_data,
    GDestroyNotify user_data_destroy,
    GError **error
)
{
    InvestigationGraphLoadWorkerData *worker_data =
        NULL;

    InvestigationGraphLoadCompletionContext *completion_context =
        NULL;

    BackgroundTask *background_task =
        NULL;

    GError *background_error =
        NULL;

    gboolean started =
        FALSE;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (load_task == NULL ||
        callback == NULL)
    {
        investigation_graph_load_task_set_error_literal(
            error,
            INVESTIGATION_GRAPH_LOAD_TASK_ERROR_INVALID_ARGUMENT,
            "La tâche ou son callback final est invalide."
        );

        return FALSE;
    }

    worker_data =
        g_try_new0(
            InvestigationGraphLoadWorkerData,
            1
        );

    completion_context =
        g_try_new0(
            InvestigationGraphLoadCompletionContext,
            1
        );

    background_task =
        background_task_new(
            "Charger le graphe d'enquête"
        );

    if (worker_data == NULL ||
        completion_context == NULL ||
        background_task == NULL)
    {
        investigation_graph_load_worker_data_free(
            worker_data
        );

        g_free(
            completion_context
        );

        background_task_unref(
            background_task
        );

        investigation_graph_load_task_set_error_literal(
            error,
            INVESTIGATION_GRAPH_LOAD_TASK_ERROR_MEMORY,
            "Impossible de préparer l'exécution asynchrone."
        );

        return FALSE;
    }

    worker_data->database_path =
        g_strdup(
            load_task->database_path
        );

    if (worker_data->database_path == NULL)
    {
        investigation_graph_load_worker_data_free(
            worker_data
        );

        g_free(
            completion_context
        );

        background_task_unref(
            background_task
        );

        investigation_graph_load_task_set_error_literal(
            error,
            INVESTIGATION_GRAPH_LOAD_TASK_ERROR_MEMORY,
            "Impossible de copier le chemin SQLite pour le worker."
        );

        return FALSE;
    }

    completion_context->load_task =
        investigation_graph_load_task_ref_internal(
            load_task
        );

    g_mutex_lock(
        &load_task->mutex
    );

    if (load_task->disposed)
    {
        investigation_graph_load_task_set_error_literal(
            error,
            INVESTIGATION_GRAPH_LOAD_TASK_ERROR_INVALID_ARGUMENT,
            "La tâche de chargement a déjà été libérée."
        );

        goto start_failure_locked;
    }

    if (load_task->running)
    {
        investigation_graph_load_task_set_error_literal(
            error,
            INVESTIGATION_GRAPH_LOAD_TASK_ERROR_ALREADY_RUNNING,
            "Un chargement du graphe est déjà actif."
        );

        goto start_failure_locked;
    }

    load_task->background_task =
        background_task;

    load_task->callback =
        callback;

    load_task->user_data =
        user_data;

    load_task->user_data_destroy =
        user_data_destroy;

    load_task->running =
        TRUE;

    started =
        background_task_start(
            background_task,
            investigation_graph_load_task_worker,
            worker_data,
            investigation_graph_load_worker_data_free,
            investigation_graph_load_result_free,
            investigation_graph_load_task_on_completed,
            completion_context,
            investigation_graph_load_completion_context_free,
            &background_error
        );

    if (!started)
    {
        load_task->background_task =
            NULL;

        load_task->callback =
            NULL;

        load_task->user_data =
            NULL;

        load_task->user_data_destroy =
            NULL;

        load_task->running =
            FALSE;

        investigation_graph_load_task_set_nested_error(
            error,
            INVESTIGATION_GRAPH_LOAD_TASK_ERROR_INTERNAL,
            "Impossible de démarrer BackgroundTask",
            background_error
        );

        goto start_failure_locked;
    }

    g_mutex_unlock(
        &load_task->mutex
    );

    g_clear_error(
        &background_error
    );

    return TRUE;

start_failure_locked:

    g_mutex_unlock(
        &load_task->mutex
    );

    g_clear_error(
        &background_error
    );

    investigation_graph_load_worker_data_free(
        worker_data
    );

    investigation_graph_load_completion_context_free(
        completion_context
    );

    background_task_unref(
        background_task
    );

    return FALSE;
}

void investigation_graph_load_task_cancel(
    InvestigationGraphLoadTask *load_task
)
{
    BackgroundTask *background_task =
        NULL;

    if (load_task == NULL)
    {
        return;
    }

    g_mutex_lock(
        &load_task->mutex
    );

    if (load_task->running &&
        load_task->background_task != NULL)
    {
        background_task =
            background_task_ref(
                load_task->background_task
            );
    }

    g_mutex_unlock(
        &load_task->mutex
    );

    if (background_task != NULL)
    {
        background_task_cancel(
            background_task
        );

        background_task_unref(
            background_task
        );
    }
}

gboolean investigation_graph_load_task_is_running(
    const InvestigationGraphLoadTask *load_task
)
{
    InvestigationGraphLoadTask *mutable_load_task =
        NULL;

    gboolean running =
        FALSE;

    if (load_task == NULL)
    {
        return FALSE;
    }

    mutable_load_task =
        (InvestigationGraphLoadTask *) load_task;

    g_mutex_lock(
        &mutable_load_task->mutex
    );

    running =
        mutable_load_task->running;

    g_mutex_unlock(
        &mutable_load_task->mutex
    );

    return running;
}
