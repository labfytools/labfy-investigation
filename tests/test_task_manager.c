/******************************************************************************
 * @file test_task_manager.c
 * @brief Tests du gestionnaire central des tâches.
 ******************************************************************************/

#include "core/task_manager.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define TEST_TASK_MANAGER_TIMEOUT_SECONDS 5

/**
 * @struct TestTaskManagerCallbackData
 * @brief Données utilisées pour contrôler le callback du gestionnaire.
 */
typedef struct
{
    guint *changed_count;
    guint *destroy_count;
} TestTaskManagerCallbackData;

/**
 * @struct TestTaskManagerAsyncGroup
 * @brief Suit la fin de plusieurs tâches asynchrones.
 */
typedef struct
{
    GMainLoop *main_loop;
    guint remaining_tasks;
} TestTaskManagerAsyncGroup;

/**
 * @brief Interrompt un test asynchrone trop long.
 *
 * @param user_data Boucle principale du test.
 *
 * @return G_SOURCE_REMOVE.
 */
static gboolean test_task_manager_timeout(
    gpointer user_data
)
{
    GMainLoop *main_loop = user_data;

    if (main_loop != NULL)
    {
        g_main_loop_quit(
            main_loop
        );
    }

    return G_SOURCE_REMOVE;
}

/**
 * @brief Compte la fin d'une tâche d'un groupe.
 *
 * @param task Tâche terminée.
 * @param user_data Groupe asynchrone.
 */
static void test_task_manager_group_completed(
    BackgroundTask *task,
    gpointer user_data
)
{
    TestTaskManagerAsyncGroup *group =
        user_data;

    assert(task != NULL);
    assert(group != NULL);
    assert(group->remaining_tasks > 0);

    group->remaining_tasks--;

    if (group->remaining_tasks == 0)
    {
        g_main_loop_quit(
            group->main_loop
        );
    }
}

/**
 * @brief Compte les notifications émises par le gestionnaire.
 *
 * @param task_manager Gestionnaire ayant changé.
 * @param user_data Données TestTaskManagerCallbackData.
 */
static void test_task_manager_changed(
    TaskManager *task_manager,
    gpointer user_data
)
{
    TestTaskManagerCallbackData *callback_data =
        user_data;

    assert(task_manager != NULL);
    assert(callback_data != NULL);
    assert(callback_data->changed_count != NULL);

    (*callback_data->changed_count)++;
}

/**
 * @brief Détruit les données du callback et compte leur destruction.
 *
 * @param user_data Données TestTaskManagerCallbackData.
 */
static void test_task_manager_callback_data_free(
    gpointer user_data
)
{
    TestTaskManagerCallbackData *callback_data =
        user_data;

    assert(callback_data != NULL);
    assert(callback_data->destroy_count != NULL);

    (*callback_data->destroy_count)++;

    g_free(
        callback_data
    );
}

/**
 * @brief Installe un callback de test sur un gestionnaire.
 *
 * @param task_manager Gestionnaire concerné.
 * @param changed_count Compteur de notifications.
 * @param destroy_count Compteur de destructions.
 */
static void test_task_manager_install_callback(
    TaskManager *task_manager,
    guint *changed_count,
    guint *destroy_count
)
{
    TestTaskManagerCallbackData *callback_data =
        NULL;

    assert(task_manager != NULL);
    assert(changed_count != NULL);
    assert(destroy_count != NULL);

    callback_data = g_new0(
        TestTaskManagerCallbackData,
        1
    );

    callback_data->changed_count =
        changed_count;

    callback_data->destroy_count =
        destroy_count;

    task_manager_set_changed_callback(
        task_manager,
        test_task_manager_changed,
        callback_data,
        test_task_manager_callback_data_free
    );
}

/**
 * @brief Vérifie la construction d'un gestionnaire vide.
 */
static void test_task_manager_creation(void)
{
    TaskManager *task_manager = NULL;

    task_manager = task_manager_new();

    assert(task_manager != NULL);

    assert(
        task_manager_get_count(
            task_manager
        ) == 0
    );

    assert(
        task_manager_get_task(
            task_manager,
            0
        ) == NULL
    );

    assert(
        task_manager_get_count(NULL) == 0
    );

    assert(
        task_manager_get_task(
            NULL,
            0
        ) == NULL
    );

    task_manager_cancel_all(
        NULL
    );

    assert(
        task_manager_remove_finished(NULL) == 0
    );

    task_manager_free(
        task_manager
    );

    task_manager_free(
        NULL
    );
}

/**
 * @brief Vérifie l'ajout et la récupération d'une tâche.
 */
static void test_task_manager_add(void)
{
    TaskManager *task_manager = NULL;

    BackgroundTask *task = NULL;
    BackgroundTask *retrieved_task = NULL;

    GError *error = NULL;

    guint changed_count = 0;
    guint destroy_count = 0;

    task_manager = task_manager_new();

    assert(task_manager != NULL);

    test_task_manager_install_callback(
        task_manager,
        &changed_count,
        &destroy_count
    );

    task = background_task_new(
        "Tâche ajoutée"
    );

    assert(task != NULL);

    assert(
        task_manager_add(
            task_manager,
            task,
            &error
        )
    );

    assert(error == NULL);

    assert(
        task_manager_get_count(
            task_manager
        ) == 1
    );

    assert(changed_count == 1);

    retrieved_task = task_manager_get_task(
        task_manager,
        0
    );

    assert(retrieved_task != NULL);

    /*
     * Le manager retourne une nouvelle référence vers la même instance.
     */
    assert(retrieved_task == task);

    assert(
        strcmp(
            background_task_get_title(
                retrieved_task
            ),
            "Tâche ajoutée"
        ) == 0
    );

    background_task_unref(
        retrieved_task
    );

    /*
     * Le gestionnaire possède encore sa référence.
     * L'appelant possède toujours la référence créée par new().
     */
    task_manager_free(
        task_manager
    );

    assert(destroy_count == 1);

    assert(
        strcmp(
            background_task_get_title(task),
            "Tâche ajoutée"
        ) == 0
    );

    background_task_unref(
        task
    );
}

/**
 * @brief Vérifie qu'une même tâche ne peut pas être ajoutée deux fois.
 */
static void test_task_manager_duplicate(void)
{
    TaskManager *task_manager = NULL;
    BackgroundTask *task = NULL;

    GError *error = NULL;

    guint changed_count = 0;
    guint destroy_count = 0;

    task_manager = task_manager_new();
    task = background_task_new(
        "Tâche unique"
    );

    assert(task_manager != NULL);
    assert(task != NULL);

    test_task_manager_install_callback(
        task_manager,
        &changed_count,
        &destroy_count
    );

    assert(
        task_manager_add(
            task_manager,
            task,
            &error
        )
    );

    assert(error == NULL);

    assert(
        !task_manager_add(
            task_manager,
            task,
            &error
        )
    );

    assert(error != NULL);

    assert(
        error->domain ==
        TASK_MANAGER_ERROR
    );

    assert(
        error->code ==
        TASK_MANAGER_ERROR_ALREADY_ADDED
    );

    g_clear_error(
        &error
    );

    assert(
        task_manager_get_count(
            task_manager
        ) == 1
    );

    /*
     * Seul le premier ajout constitue un changement.
     */
    assert(changed_count == 1);

    task_manager_free(
        task_manager
    );

    assert(destroy_count == 1);

    background_task_unref(
        task
    );
}

/**
 * @brief Vérifie le retrait d'une tâche.
 */
static void test_task_manager_remove_task(void)
{
    TaskManager *task_manager = NULL;
    BackgroundTask *task = NULL;

    GError *error = NULL;

    guint changed_count = 0;
    guint destroy_count = 0;

    task_manager = task_manager_new();

    task = background_task_new(
        "Tâche retirée"
    );

    assert(task_manager != NULL);
    assert(task != NULL);

    test_task_manager_install_callback(
        task_manager,
        &changed_count,
        &destroy_count
    );

    assert(
        task_manager_add(
            task_manager,
            task,
            &error
        )
    );

    assert(error == NULL);
    assert(changed_count == 1);

    assert(
        task_manager_remove(
            task_manager,
            task
        )
    );

    assert(
        task_manager_get_count(
            task_manager
        ) == 0
    );

    assert(changed_count == 2);

    /*
     * Une seconde tentative ne modifie rien.
     */
    assert(
        !task_manager_remove(
            task_manager,
            task
        )
    );

    assert(changed_count == 2);

    /*
     * La référence de l'appelant reste valide après le retrait.
     */
    assert(
        strcmp(
            background_task_get_title(task),
            "Tâche retirée"
        ) == 0
    );

    task_manager_free(
        task_manager
    );

    assert(destroy_count == 1);

    background_task_unref(
        task
    );
}

/**
 * @brief Worker terminant normalement.
 */
static gboolean test_task_manager_success_worker(
    BackgroundTask *task,
    GCancellable *cancellable,
    gpointer worker_data,
    gpointer *result,
    GError **error
)
{
    (void) task;
    (void) worker_data;

    assert(cancellable != NULL);
    assert(result != NULL);
    assert(error != NULL);
    assert(*error == NULL);

    *result = NULL;

    return TRUE;
}

/**
 * @brief Worker terminant avec une erreur.
 */
static gboolean test_task_manager_failure_worker(
    BackgroundTask *task,
    GCancellable *cancellable,
    gpointer worker_data,
    gpointer *result,
    GError **error
)
{
    (void) task;
    (void) cancellable;
    (void) worker_data;
    (void) result;

    assert(error != NULL);
    assert(*error == NULL);

    g_set_error_literal(
        error,
        G_IO_ERROR,
        G_IO_ERROR_FAILED,
        "Échec volontaire."
    );

    return FALSE;
}

/**
 * @brief Worker suffisamment lent pour tester l'annulation.
 */
static gboolean test_task_manager_wait_worker(
    BackgroundTask *task,
    GCancellable *cancellable,
    gpointer worker_data,
    gpointer *result,
    GError **error
)
{
    guint step = 0;

    (void) worker_data;

    assert(task != NULL);
    assert(cancellable != NULL);
    assert(result != NULL);
    assert(error != NULL);
    assert(*error == NULL);

    for (step = 0; step < 100; step++)
    {
        if (g_cancellable_set_error_if_cancelled(
                cancellable,
                error
            ))
        {
            return FALSE;
        }

        background_task_report_progress(
            task,
            (double) step / 100.0,
            "Traitement en cours"
        );

        g_usleep(
            10000
        );
    }

    *result = NULL;

    return TRUE;
}

/**
 * @brief Vérifie que seules les tâches terminées sont retirées.
 */
static void test_task_manager_remove_finished_tasks(void)
{
    TaskManager *task_manager = NULL;

    BackgroundTask *pending_task = NULL;
    BackgroundTask *completed_task = NULL;
    BackgroundTask *failed_task = NULL;
    BackgroundTask *cancelled_task = NULL;
    BackgroundTask *remaining_task = NULL;

    TestTaskManagerAsyncGroup group = {0};

    GMainLoop *main_loop = NULL;
    GError *error = NULL;

    guint timeout_source_id = 0;
    guint changed_count = 0;
    guint destroy_count = 0;

    main_loop = g_main_loop_new(
        NULL,
        FALSE
    );

    assert(main_loop != NULL);

    group.main_loop = main_loop;
    group.remaining_tasks = 3;

    task_manager = task_manager_new();

    pending_task = background_task_new(
        "En attente"
    );

    completed_task = background_task_new(
        "Terminée"
    );

    failed_task = background_task_new(
        "Échouée"
    );

    cancelled_task = background_task_new(
        "Annulée"
    );

    assert(task_manager != NULL);
    assert(pending_task != NULL);
    assert(completed_task != NULL);
    assert(failed_task != NULL);
    assert(cancelled_task != NULL);

    test_task_manager_install_callback(
        task_manager,
        &changed_count,
        &destroy_count
    );

    assert(
        task_manager_add(
            task_manager,
            pending_task,
            &error
        )
    );

    assert(
        task_manager_add(
            task_manager,
            completed_task,
            &error
        )
    );

    assert(
        task_manager_add(
            task_manager,
            failed_task,
            &error
        )
    );

    assert(
        task_manager_add(
            task_manager,
            cancelled_task,
            &error
        )
    );

    assert(error == NULL);
    assert(changed_count == 4);

    assert(
        background_task_start(
            completed_task,
            test_task_manager_success_worker,
            NULL,
            NULL,
            NULL,
            test_task_manager_group_completed,
            &group,
            NULL,
            &error
        )
    );

    assert(
        background_task_start(
            failed_task,
            test_task_manager_failure_worker,
            NULL,
            NULL,
            NULL,
            test_task_manager_group_completed,
            &group,
            NULL,
            &error
        )
    );

    assert(
        background_task_start(
            cancelled_task,
            test_task_manager_wait_worker,
            NULL,
            NULL,
            NULL,
            test_task_manager_group_completed,
            &group,
            NULL,
            &error
        )
    );

    assert(error == NULL);

    background_task_cancel(
        cancelled_task
    );

    timeout_source_id = g_timeout_add_seconds(
        TEST_TASK_MANAGER_TIMEOUT_SECONDS,
        test_task_manager_timeout,
        main_loop
    );

    assert(timeout_source_id != 0);

    g_main_loop_run(
        main_loop
    );

    assert(group.remaining_tasks == 0);

    assert(
        g_source_remove(
            timeout_source_id
        )
    );

    assert(
        background_task_get_state(completed_task) ==
        BACKGROUND_TASK_STATE_COMPLETED
    );

    assert(
        background_task_get_state(failed_task) ==
        BACKGROUND_TASK_STATE_FAILED
    );

    assert(
        background_task_get_state(cancelled_task) ==
        BACKGROUND_TASK_STATE_CANCELLED
    );

    assert(
        background_task_get_state(pending_task) ==
        BACKGROUND_TASK_STATE_PENDING
    );

    assert(
        task_manager_remove_finished(
            task_manager
        ) == 3
    );

    assert(
        task_manager_get_count(
            task_manager
        ) == 1
    );

    /*
     * Une seule notification est produite pour le nettoyage groupé.
     */
    assert(changed_count == 5);

    remaining_task = task_manager_get_task(
        task_manager,
        0
    );

    assert(remaining_task == pending_task);

    background_task_unref(
        remaining_task
    );

    task_manager_free(
        task_manager
    );

    assert(destroy_count == 1);

    background_task_unref(
        pending_task
    );

    background_task_unref(
        completed_task
    );

    background_task_unref(
        failed_task
    );

    background_task_unref(
        cancelled_task
    );

    g_main_loop_unref(
        main_loop
    );
}

/**
 * @brief Vérifie l'annulation de toutes les tâches actives.
 */
static void test_task_manager_cancel_all_tasks(void)
{
    TaskManager *task_manager = NULL;

    BackgroundTask *first_task = NULL;
    BackgroundTask *second_task = NULL;
    BackgroundTask *pending_task = NULL;

    TestTaskManagerAsyncGroup group = {0};

    GMainLoop *main_loop = NULL;
    GError *error = NULL;

    guint timeout_source_id = 0;
    guint changed_count = 0;
    guint destroy_count = 0;

    main_loop = g_main_loop_new(
        NULL,
        FALSE
    );

    assert(main_loop != NULL);

    group.main_loop = main_loop;
    group.remaining_tasks = 2;

    task_manager = task_manager_new();

    first_task = background_task_new(
        "Première tâche active"
    );

    second_task = background_task_new(
        "Seconde tâche active"
    );

    pending_task = background_task_new(
        "Tâche en attente"
    );

    assert(task_manager != NULL);
    assert(first_task != NULL);
    assert(second_task != NULL);
    assert(pending_task != NULL);

    test_task_manager_install_callback(
        task_manager,
        &changed_count,
        &destroy_count
    );

    assert(
        task_manager_add(
            task_manager,
            first_task,
            &error
        )
    );

    assert(
        task_manager_add(
            task_manager,
            second_task,
            &error
        )
    );

    assert(
        task_manager_add(
            task_manager,
            pending_task,
            &error
        )
    );

    assert(error == NULL);
    assert(changed_count == 3);

    assert(
        background_task_start(
            first_task,
            test_task_manager_wait_worker,
            NULL,
            NULL,
            NULL,
            test_task_manager_group_completed,
            &group,
            NULL,
            &error
        )
    );

    assert(
        background_task_start(
            second_task,
            test_task_manager_wait_worker,
            NULL,
            NULL,
            NULL,
            test_task_manager_group_completed,
            &group,
            NULL,
            &error
        )
    );

    assert(error == NULL);

    task_manager_cancel_all(
        task_manager
    );

    /*
     * Une notification globale supplémentaire signale la demande
     * d'annulation.
     */
    assert(changed_count == 4);

    timeout_source_id = g_timeout_add_seconds(
        TEST_TASK_MANAGER_TIMEOUT_SECONDS,
        test_task_manager_timeout,
        main_loop
    );

    assert(timeout_source_id != 0);

    g_main_loop_run(
        main_loop
    );

    assert(group.remaining_tasks == 0);

    assert(
        g_source_remove(
            timeout_source_id
        )
    );

    assert(
        background_task_get_state(first_task) ==
        BACKGROUND_TASK_STATE_CANCELLED
    );

    assert(
        background_task_get_state(second_task) ==
        BACKGROUND_TASK_STATE_CANCELLED
    );

    assert(
        background_task_get_state(pending_task) ==
        BACKGROUND_TASK_STATE_PENDING
    );

    assert(
        !background_task_is_cancelled(
            pending_task
        )
    );

    task_manager_free(
        task_manager
    );

    assert(destroy_count == 1);

    background_task_unref(
        first_task
    );

    background_task_unref(
        second_task
    );

    background_task_unref(
        pending_task
    );

    g_main_loop_unref(
        main_loop
    );
}

/**
 * @brief Vérifie que le remplacement du callback détruit les anciennes
 *        données exactement une fois.
 */
static void test_task_manager_replace_callback(void)
{
    TaskManager *task_manager = NULL;
    BackgroundTask *task = NULL;

    GError *error = NULL;

    guint first_changed_count = 0;
    guint first_destroy_count = 0;

    guint second_changed_count = 0;
    guint second_destroy_count = 0;

    task_manager = task_manager_new();

    assert(task_manager != NULL);

    test_task_manager_install_callback(
        task_manager,
        &first_changed_count,
        &first_destroy_count
    );

    assert(first_destroy_count == 0);

    test_task_manager_install_callback(
        task_manager,
        &second_changed_count,
        &second_destroy_count
    );

    assert(first_destroy_count == 1);
    assert(second_destroy_count == 0);

    task = background_task_new(
        "Notification du second callback"
    );

    assert(task != NULL);

    assert(
        task_manager_add(
            task_manager,
            task,
            &error
        )
    );

    assert(error == NULL);

    assert(first_changed_count == 0);
    assert(second_changed_count == 1);

    task_manager_set_changed_callback(
        task_manager,
        NULL,
        NULL,
        NULL
    );

    assert(second_destroy_count == 1);

    assert(
        task_manager_remove(
            task_manager,
            task
        )
    );

    /*
     * Le callback étant désactivé, le compteur reste inchangé.
     */
    assert(second_changed_count == 1);

    task_manager_free(
        task_manager
    );

    assert(first_destroy_count == 1);
    assert(second_destroy_count == 1);

    background_task_unref(
        task
    );
}

int main(void)
{
    test_task_manager_creation();
    test_task_manager_add();
    test_task_manager_duplicate();
    test_task_manager_remove_task();
    test_task_manager_remove_finished_tasks();
    test_task_manager_cancel_all_tasks();
    test_task_manager_replace_callback();

    printf(
        "TaskManager : tous les tests sont valides.\n"
    );

    return 0;
}
