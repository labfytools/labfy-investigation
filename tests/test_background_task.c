/******************************************************************************
 * @file test_background_task.c
 * @brief Tests du module BackgroundTask.
 ******************************************************************************/

#include "core/background_task.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>

/**
 * @brief Délai maximal d'attente d'un test asynchrone.
 */
#define TEST_BACKGROUND_TASK_TIMEOUT_SECONDS 5

/**
 * @struct TestBackgroundTaskResult
 * @brief Résultat produit par le worker de test.
 */
typedef struct
{
    char *text;
    gboolean *destroyed;
} TestBackgroundTaskResult;

/**
 * @struct TestBackgroundTaskWorkerData
 * @brief Données transmises au worker de succès.
 */
typedef struct
{
    gboolean *result_destroyed;
} TestBackgroundTaskWorkerData;

/**
 * @struct TestBackgroundTaskCompletionData
 * @brief Contexte utilisé par le callback final.
 */
typedef struct
{
    GMainLoop *main_loop;
    guint completion_count;
} TestBackgroundTaskCompletionData;

typedef struct
{
    guint *worker_data_destroy_count;
    guint *result_destroy_count;
} TestBackgroundTaskLifetimeWorkerData;

typedef struct
{
    guint *destroy_count;
} TestBackgroundTaskLifetimeResult;

typedef struct
{
    GMainLoop *main_loop;
    guint completion_count;
    guint completion_data_destroy_count;
} TestBackgroundTaskLifetimeCompletionData;

/**
 * @brief Détruit un résultat produit par le worker.
 *
 * @param user_data Pointeur vers TestBackgroundTaskResult.
 */
static void test_background_task_result_free(
    gpointer user_data
)
{
    TestBackgroundTaskResult *result =
        user_data;

    if (result == NULL)
    {
        return;
    }

    if (result->destroyed != NULL)
    {
        *result->destroyed = TRUE;
    }

    g_free(
        result->text
    );

    g_free(
        result
    );
}

/**
 * @brief Termine un test qui a dépassé son délai maximal.
 *
 * @param user_data Boucle principale du test.
 *
 * @return G_SOURCE_REMOVE.
 */
static gboolean test_background_task_timeout(
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
 * @brief Worker réalisant une tâche avec succès.
 *
 * @param task Tâche en cours.
 * @param cancellable Objet d'annulation.
 * @param worker_data Données TestBackgroundTaskWorkerData.
 * @param result Emplacement recevant le résultat.
 * @param error Emplacement recevant une erreur.
 *
 * @return TRUE.
 */
static gboolean test_background_task_success_worker(
    BackgroundTask *task,
    GCancellable *cancellable,
    gpointer worker_data,
    gpointer *result,
    GError **error
)
{
    TestBackgroundTaskWorkerData *data =
        worker_data;

    TestBackgroundTaskResult *worker_result =
        NULL;

    assert(task != NULL);
    assert(cancellable != NULL);
    assert(data != NULL);
    assert(result != NULL);
    assert(error != NULL);
    assert(*error == NULL);

    background_task_report_progress(
        task,
        0.25,
        "Préparation"
    );

    background_task_report_progress(
        task,
        0.75,
        "Finalisation"
    );

    worker_result = g_new0(
        TestBackgroundTaskResult,
        1
    );

    worker_result->text = g_strdup(
        "Résultat valide"
    );

    worker_result->destroyed =
        data->result_destroyed;

    *result = worker_result;

    return TRUE;
}

/**
 * @brief Vérifie le résultat d'une tâche réussie.
 *
 * @param task Tâche terminée.
 * @param user_data Données TestBackgroundTaskCompletionData.
 */
static void test_background_task_success_completed(
    BackgroundTask *task,
    gpointer user_data
)
{
    TestBackgroundTaskCompletionData *completion_data =
        user_data;

    const TestBackgroundTaskResult *result =
        NULL;

    char *status_message = NULL;
    GError *error = NULL;

    assert(task != NULL);
    assert(completion_data != NULL);
    assert(completion_data->main_loop != NULL);

    completion_data->completion_count++;

    assert(
        completion_data->completion_count == 1
    );

    assert(
        background_task_get_state(task) ==
        BACKGROUND_TASK_STATE_COMPLETED
    );

    assert(
        background_task_get_progress(task) == 1.0
    );

    assert(
        background_task_get_started_at_us(task) > 0
    );

    assert(
        background_task_get_finished_at_us(task) >=
        background_task_get_started_at_us(task)
    );

    error = background_task_dup_error(
        task
    );

    assert(error == NULL);

    result = background_task_get_result(
        task
    );

    assert(result != NULL);
    assert(result->text != NULL);

    assert(
        strcmp(
            result->text,
            "Résultat valide"
        ) == 0
    );

    status_message =
        background_task_dup_status_message(
            task
        );

    assert(status_message != NULL);

    assert(
        strcmp(
            status_message,
            "Finalisation"
        ) == 0
    );

    g_free(
        status_message
    );

    g_main_loop_quit(
        completion_data->main_loop
    );
}

/**
 * @brief Vérifie la construction et l'état initial d'une tâche.
 */
static void test_background_task_creation(void)
{
    BackgroundTask *task = NULL;

    char *status_message = NULL;
    GError *error = NULL;

    assert(
        background_task_new(NULL) == NULL
    );

    assert(
        background_task_new("") == NULL
    );

    task = background_task_new(
        "Test de construction"
    );

    assert(task != NULL);

    assert(
        strcmp(
            background_task_get_title(task),
            "Test de construction"
        ) == 0
    );

    assert(
        background_task_get_state(task) ==
        BACKGROUND_TASK_STATE_PENDING
    );

    assert(
        background_task_get_progress(task) == 0.0
    );

    assert(
        background_task_get_started_at_us(task) == 0
    );

    assert(
        background_task_get_finished_at_us(task) == 0
    );

    assert(
        background_task_get_result(task) == NULL
    );

    status_message =
        background_task_dup_status_message(
            task
        );

    assert(status_message == NULL);

    error = background_task_dup_error(
        task
    );

    assert(error == NULL);

    /*
     * Une demande d'annulation avant le démarrage est ignorée.
     */
    background_task_cancel(
        task
    );

    assert(
        !background_task_is_cancelled(task)
    );

    /*
     * Une progression signalée avant le démarrage est ignorée.
     */
    background_task_report_progress(
        task,
        0.50,
        "Message ignoré"
    );

    assert(
        background_task_get_progress(task) == 0.0
    );

    status_message =
        background_task_dup_status_message(
            task
        );

    assert(status_message == NULL);

    background_task_unref(
        task
    );

    background_task_unref(
        NULL
    );
}

/**
 * @brief Vérifie une exécution asynchrone réussie.
 */
static void test_background_task_success(void)
{
    BackgroundTask *task = NULL;

    TestBackgroundTaskWorkerData *worker_data =
        NULL;

    TestBackgroundTaskCompletionData
        completion_data = {0};

    GMainLoop *main_loop = NULL;
    GError *error = NULL;

    guint timeout_source_id = 0;

    gboolean result_destroyed = FALSE;

    main_loop = g_main_loop_new(
        NULL,
        FALSE
    );

    assert(main_loop != NULL);

    completion_data.main_loop =
        main_loop;

    completion_data.completion_count = 0;

    worker_data = g_new0(
        TestBackgroundTaskWorkerData,
        1
    );

    worker_data->result_destroyed =
        &result_destroyed;

    task = background_task_new(
        "Test de succès"
    );

    assert(task != NULL);

    assert(
        background_task_start(
            task,
            test_background_task_success_worker,
            worker_data,
            g_free,
            test_background_task_result_free,
            test_background_task_success_completed,
            &completion_data,
            NULL,
            &error
        )
    );

    assert(error == NULL);

    assert(
        background_task_get_state(task) ==
        BACKGROUND_TASK_STATE_RUNNING
    );

    assert(
        background_task_get_started_at_us(task) > 0
    );

    timeout_source_id = g_timeout_add_seconds(
        TEST_BACKGROUND_TASK_TIMEOUT_SECONDS,
        test_background_task_timeout,
        main_loop
    );

    assert(timeout_source_id != 0);

    g_main_loop_run(
        main_loop
    );

    assert(
        completion_data.completion_count == 1
    );

    /*
     * Le délai n'a pas été déclenché puisque la tâche s'est terminée.
     */
    assert(
        g_source_remove(
            timeout_source_id
        )
    );

    assert(!result_destroyed);

    /*
     * Le résultat reste la propriété de BackgroundTask jusqu'au dernier
     * unref.
     */
    background_task_unref(
        task
    );

    assert(result_destroyed);

    g_main_loop_unref(
        main_loop
    );
}

/**
 * @brief Worker retournant volontairement une erreur normale.
 */
static gboolean test_background_task_failure_worker(
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
        "Échec volontaire du worker."
    );

    return FALSE;
}

/**
 * @brief Vérifie la finalisation d'une tâche en échec.
 */
static void test_background_task_failure_completed(
    BackgroundTask *task,
    gpointer user_data
)
{
    TestBackgroundTaskCompletionData *completion_data =
        user_data;

    GError *error = NULL;

    assert(task != NULL);
    assert(completion_data != NULL);

    completion_data->completion_count++;

    assert(
        completion_data->completion_count == 1
    );

    assert(
        background_task_get_state(task) ==
        BACKGROUND_TASK_STATE_FAILED
    );

    assert(
        background_task_get_result(task) == NULL
    );

    assert(
        background_task_get_started_at_us(task) > 0
    );

    assert(
        background_task_get_finished_at_us(task) >=
        background_task_get_started_at_us(task)
    );

    error = background_task_dup_error(
        task
    );

    assert(error != NULL);

    assert(
        g_error_matches(
            error,
            G_IO_ERROR,
            G_IO_ERROR_FAILED
        )
    );

    assert(
        strcmp(
            error->message,
            "Échec volontaire du worker."
        ) == 0
    );

    g_error_free(
        error
    );

    g_main_loop_quit(
        completion_data->main_loop
    );
}

/**
 * @brief Vérifie la conservation d'une erreur normale de worker.
 */
static void test_background_task_failure(void)
{
    BackgroundTask *task = NULL;

    TestBackgroundTaskCompletionData
        completion_data = {0};

    GMainLoop *main_loop = NULL;
    GError *error = NULL;

    guint timeout_source_id = 0;

    main_loop = g_main_loop_new(
        NULL,
        FALSE
    );

    assert(main_loop != NULL);

    completion_data.main_loop =
        main_loop;

    task = background_task_new(
        "Test d'échec"
    );

    assert(task != NULL);

    assert(
        background_task_start(
            task,
            test_background_task_failure_worker,
            NULL,
            NULL,
            NULL,
            test_background_task_failure_completed,
            &completion_data,
            NULL,
            &error
        )
    );

    assert(error == NULL);

    timeout_source_id = g_timeout_add_seconds(
        TEST_BACKGROUND_TASK_TIMEOUT_SECONDS,
        test_background_task_timeout,
        main_loop
    );

    assert(timeout_source_id != 0);

    g_main_loop_run(
        main_loop
    );

    assert(
        completion_data.completion_count == 1
    );

    assert(
        g_source_remove(
            timeout_source_id
        )
    );

    background_task_unref(
        task
    );

    g_main_loop_unref(
        main_loop
    );
}

/**
 * @brief Worker violant le contrat en échouant sans GError.
 */
static gboolean test_background_task_invalid_protocol_worker(
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

    return FALSE;
}

/**
 * @brief Vérifie la détection d'une violation du contrat du worker.
 */
static void test_background_task_invalid_protocol_completed(
    BackgroundTask *task,
    gpointer user_data
)
{
    TestBackgroundTaskCompletionData *completion_data =
        user_data;

    GError *error = NULL;

    assert(task != NULL);
    assert(completion_data != NULL);

    completion_data->completion_count++;

    assert(
        completion_data->completion_count == 1
    );

    assert(
        background_task_get_state(task) ==
        BACKGROUND_TASK_STATE_FAILED
    );

    assert(
        background_task_get_result(task) == NULL
    );

    error = background_task_dup_error(
        task
    );

    assert(error != NULL);

    assert(
        error->domain ==
        BACKGROUND_TASK_ERROR
    );

    assert(
        error->code ==
        BACKGROUND_TASK_ERROR_WORKER_PROTOCOL
    );

    g_error_free(
        error
    );

    g_main_loop_quit(
        completion_data->main_loop
    );
}

/**
 * @brief Vérifie qu'un worker incorrect est transformé en échec contrôlé.
 */
static void test_background_task_invalid_protocol(void)
{
    BackgroundTask *task = NULL;

    TestBackgroundTaskCompletionData
        completion_data = {0};

    GMainLoop *main_loop = NULL;
    GError *error = NULL;

    guint timeout_source_id = 0;

    main_loop = g_main_loop_new(
        NULL,
        FALSE
    );

    assert(main_loop != NULL);

    completion_data.main_loop =
        main_loop;

    task = background_task_new(
        "Test de protocole invalide"
    );

    assert(task != NULL);

    assert(
        background_task_start(
            task,
            test_background_task_invalid_protocol_worker,
            NULL,
            NULL,
            NULL,
            test_background_task_invalid_protocol_completed,
            &completion_data,
            NULL,
            &error
        )
    );

    assert(error == NULL);

    timeout_source_id = g_timeout_add_seconds(
        TEST_BACKGROUND_TASK_TIMEOUT_SECONDS,
        test_background_task_timeout,
        main_loop
    );

    assert(timeout_source_id != 0);

    g_main_loop_run(
        main_loop
    );

    assert(
        completion_data.completion_count == 1
    );

    assert(
        g_source_remove(
            timeout_source_id
        )
    );

    background_task_unref(
        task
    );

    g_main_loop_unref(
        main_loop
    );
}

/**
 * @brief Worker assez long pour tester l'annulation et le double démarrage.
 */
static gboolean test_background_task_wait_worker(
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
 * @brief Termine une tâche simple réussie.
 */
static void test_background_task_simple_completed(
    BackgroundTask *task,
    gpointer user_data
)
{
    TestBackgroundTaskCompletionData *completion_data =
        user_data;

    assert(task != NULL);
    assert(completion_data != NULL);

    completion_data->completion_count++;

    assert(
        completion_data->completion_count == 1
    );

    assert(
        background_task_get_state(task) ==
        BACKGROUND_TASK_STATE_COMPLETED
    );

    g_main_loop_quit(
        completion_data->main_loop
    );
}

/**
 * @brief Demande l'annulation depuis le contexte principal.
 *
 * @return G_SOURCE_REMOVE.
 */
static gboolean test_background_task_cancel_timeout(
    gpointer user_data
)
{
    BackgroundTask *task = user_data;

    assert(task != NULL);

    background_task_cancel(
        task
    );

    return G_SOURCE_REMOVE;
}

/**
 * @brief Libère une référence utilisée comme donnée GLib.
 */
static void test_background_task_unref_notify(
    gpointer user_data
)
{
    background_task_unref(
        user_data
    );
}

/**
 * @brief Vérifie la finalisation d'une tâche annulée.
 */
static void test_background_task_cancelled_completed(
    BackgroundTask *task,
    gpointer user_data
)
{
    TestBackgroundTaskCompletionData *completion_data =
        user_data;

    GError *error = NULL;

    assert(task != NULL);
    assert(completion_data != NULL);

    completion_data->completion_count++;

    assert(
        completion_data->completion_count == 1
    );

    assert(
        background_task_get_state(task) ==
        BACKGROUND_TASK_STATE_CANCELLED
    );

    assert(
        background_task_is_cancelled(task)
    );

    assert(
        background_task_get_result(task) == NULL
    );

    error = background_task_dup_error(
        task
    );

    assert(error != NULL);

    assert(
        g_error_matches(
            error,
            G_IO_ERROR,
            G_IO_ERROR_CANCELLED
        )
    );

    g_error_free(
        error
    );

    g_main_loop_quit(
        completion_data->main_loop
    );
}

/**
 * @brief Vérifie l'annulation coopérative d'une tâche.
 */
static void test_background_task_cancellation(void)
{
    BackgroundTask *task = NULL;

    TestBackgroundTaskCompletionData
        completion_data = {0};

    GMainLoop *main_loop = NULL;
    GError *error = NULL;

    guint cancel_source_id = 0;
    guint timeout_source_id = 0;

    main_loop = g_main_loop_new(
        NULL,
        FALSE
    );

    assert(main_loop != NULL);

    completion_data.main_loop =
        main_loop;

    task = background_task_new(
        "Test d'annulation"
    );

    assert(task != NULL);

    assert(
        background_task_start(
            task,
            test_background_task_wait_worker,
            NULL,
            NULL,
            NULL,
            test_background_task_cancelled_completed,
            &completion_data,
            NULL,
            &error
        )
    );

    assert(error == NULL);

    /*
     * La source conserve sa propre référence à la tâche jusqu'à son
     * déclenchement ou sa destruction.
     */
    cancel_source_id = g_timeout_add_full(
        G_PRIORITY_DEFAULT,
        50,
        test_background_task_cancel_timeout,
        background_task_ref(task),
        test_background_task_unref_notify
    );

    assert(cancel_source_id != 0);

    timeout_source_id = g_timeout_add_seconds(
        TEST_BACKGROUND_TASK_TIMEOUT_SECONDS,
        test_background_task_timeout,
        main_loop
    );

    assert(timeout_source_id != 0);

    g_main_loop_run(
        main_loop
    );

    assert(
        completion_data.completion_count == 1
    );

    assert(
        g_source_remove(
            timeout_source_id
        )
    );

    background_task_unref(
        task
    );

    g_main_loop_unref(
        main_loop
    );
}

/**
 * @brief Marque une donnée comme détruite.
 */
static void test_background_task_mark_destroyed(
    gpointer user_data
)
{
    gboolean *destroyed = user_data;

    assert(destroyed != NULL);

    *destroyed = TRUE;
}

/**
 * @brief Vérifie qu'une tâche ne peut être démarrée qu'une fois.
 */
static void test_background_task_double_start(void)
{
    BackgroundTask *task = NULL;

    TestBackgroundTaskCompletionData
        completion_data = {0};

    GMainLoop *main_loop = NULL;

    GError *first_error = NULL;
    GError *second_error = NULL;

    guint timeout_source_id = 0;

    gboolean second_worker_data_destroyed = FALSE;
    gboolean second_completion_data_destroyed = FALSE;

    main_loop = g_main_loop_new(
        NULL,
        FALSE
    );

    assert(main_loop != NULL);

    completion_data.main_loop =
        main_loop;

    task = background_task_new(
        "Test de double démarrage"
    );

    assert(task != NULL);

    assert(
        background_task_start(
            task,
            test_background_task_wait_worker,
            NULL,
            NULL,
            NULL,
            test_background_task_simple_completed,
            &completion_data,
            NULL,
            &first_error
        )
    );

    assert(first_error == NULL);

    assert(
        !background_task_start(
            task,
            test_background_task_wait_worker,
            &second_worker_data_destroyed,
            test_background_task_mark_destroyed,
            NULL,
            NULL,
            &second_completion_data_destroyed,
            test_background_task_mark_destroyed,
            &second_error
        )
    );

    assert(second_error != NULL);

    assert(
        second_error->domain ==
        BACKGROUND_TASK_ERROR
    );

    assert(
        second_error->code ==
        BACKGROUND_TASK_ERROR_ALREADY_STARTED
    );

    /*
     * Le démarrage ayant échoué, la propriété n'a pas été transférée.
     */
    assert(!second_worker_data_destroyed);
    assert(!second_completion_data_destroyed);

    g_error_free(
        second_error
    );

    timeout_source_id = g_timeout_add_seconds(
        TEST_BACKGROUND_TASK_TIMEOUT_SECONDS,
        test_background_task_timeout,
        main_loop
    );

    assert(timeout_source_id != 0);

    g_main_loop_run(
        main_loop
    );

    assert(
        g_source_remove(
            timeout_source_id
        )
    );

    background_task_unref(
        task
    );

    /*
     * La première tâche ne doit jamais s'approprier les données fournies
     * lors de la seconde tentative refusée.
     */
    assert(!second_worker_data_destroyed);
    assert(!second_completion_data_destroyed);

    g_main_loop_unref(
        main_loop
    );
}

static void test_background_task_lifetime_worker_data_free(
    gpointer user_data
)
{
    TestBackgroundTaskLifetimeWorkerData *worker_data =
        user_data;

    assert(worker_data != NULL);
    assert(worker_data->worker_data_destroy_count != NULL);

    (*worker_data->worker_data_destroy_count)++;

    g_free(
        worker_data
    );
}

static void test_background_task_lifetime_result_free(
    gpointer user_data
)
{
    TestBackgroundTaskLifetimeResult *result =
        user_data;

    assert(result != NULL);
    assert(result->destroy_count != NULL);

    (*result->destroy_count)++;

    g_free(
        result
    );
}

static void test_background_task_lifetime_completion_data_free(
    gpointer user_data
)
{
    TestBackgroundTaskLifetimeCompletionData *completion_data =
        user_data;

    assert(completion_data != NULL);

    completion_data->completion_data_destroy_count++;
}

static gboolean test_background_task_lifetime_worker(
    BackgroundTask *task,
    GCancellable *cancellable,
    gpointer worker_data,
    gpointer *result,
    GError **error
)
{
    TestBackgroundTaskLifetimeWorkerData *data =
        worker_data;

    TestBackgroundTaskLifetimeResult *worker_result =
        NULL;

    assert(task != NULL);
    assert(cancellable != NULL);
    assert(data != NULL);
    assert(result != NULL);
    assert(error != NULL);
    assert(*error == NULL);

    worker_result = g_new0(
        TestBackgroundTaskLifetimeResult,
        1
    );

    worker_result->destroy_count =
        data->result_destroy_count;

    *result = worker_result;

    return TRUE;
}

static void test_background_task_lifetime_completed(
    BackgroundTask *task,
    gpointer user_data
)
{
    TestBackgroundTaskLifetimeCompletionData *completion_data =
        user_data;

    assert(task != NULL);
    assert(completion_data != NULL);

    completion_data->completion_count++;

    assert(
        completion_data->completion_count == 1
    );

    assert(
        background_task_get_state(task) ==
        BACKGROUND_TASK_STATE_COMPLETED
    );

    assert(
        background_task_get_result(task) != NULL
    );

    g_main_loop_quit(
        completion_data->main_loop
    );
}

/**
 * @brief Vérifie que la tâche survit à la libération immédiate
 *        de la référence de l'appelant.
 */
static void test_background_task_internal_reference(void)
{
    BackgroundTask *task = NULL;

    TestBackgroundTaskLifetimeWorkerData *worker_data =
        NULL;

    TestBackgroundTaskLifetimeCompletionData
        completion_data = {0};

    GMainLoop *main_loop = NULL;
    GError *error = NULL;

    guint timeout_source_id = 0;

    guint worker_data_destroy_count = 0;
    guint result_destroy_count = 0;

    main_loop = g_main_loop_new(
        NULL,
        FALSE
    );

    assert(main_loop != NULL);

    completion_data.main_loop =
        main_loop;

    worker_data = g_new0(
        TestBackgroundTaskLifetimeWorkerData,
        1
    );

    worker_data->worker_data_destroy_count =
        &worker_data_destroy_count;

    worker_data->result_destroy_count =
        &result_destroy_count;

    task = background_task_new(
        "Test de référence interne"
    );

    assert(task != NULL);

    assert(
        background_task_start(
            task,
            test_background_task_lifetime_worker,
            worker_data,
            test_background_task_lifetime_worker_data_free,
            test_background_task_lifetime_result_free,
            test_background_task_lifetime_completed,
            &completion_data,
            test_background_task_lifetime_completion_data_free,
            &error
        )
    );

    assert(error == NULL);

    /*
     * La référence de l'appelant disparaît immédiatement.
     * La référence interne de l'exécution doit maintenir task en vie.
     */
    background_task_unref(
        task
    );

    task = NULL;

    timeout_source_id = g_timeout_add_seconds(
        TEST_BACKGROUND_TASK_TIMEOUT_SECONDS,
        test_background_task_timeout,
        main_loop
    );

    assert(timeout_source_id != 0);

    g_main_loop_run(
        main_loop
    );

    assert(
        g_source_remove(
            timeout_source_id
        )
    );

    /*
     * Termine les éventuelles destructions attachées à la source GTask.
     */
    while (g_main_context_iteration(
               NULL,
               FALSE
           ))
    {
    }

    assert(
        completion_data.completion_count == 1
    );

    assert(
        completion_data.completion_data_destroy_count == 1
    );

    assert(
        worker_data_destroy_count == 1
    );

    assert(
        result_destroy_count == 1
    );

    g_main_loop_unref(
        main_loop
    );
}

int main(void)
{
    test_background_task_creation();
    test_background_task_success();
    test_background_task_failure();
    test_background_task_invalid_protocol();
    test_background_task_cancellation();
    test_background_task_double_start();
    test_background_task_internal_reference();

    printf(
        "BackgroundTask : tests de construction et de succès valides.\n"
    );

    return 0;
}
