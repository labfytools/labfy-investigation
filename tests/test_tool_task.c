/******************************************************************************
 * @file test_tool_task.c
 * @brief Tests unitaires de l'exécution d'outils dans BackgroundTask.
 ******************************************************************************/

#include "core/tool_task.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <string.h>
#include <sys/stat.h>

/**
 * @struct ToolTaskFixture
 * @brief Environnement temporaire contenant un faux outil.
 */
typedef struct
{
    char *temporary_directory;
    char *fake_tool_path;
    char *original_path;
} ToolTaskFixture;

/**
 * @struct ToolTaskWaitContext
 * @brief Synchronisation entre une BackgroundTask et le test principal.
 */
typedef struct
{
    GMainLoop *main_loop;

    BackgroundTask *completed_task;

    guint timeout_source_id;

    gboolean completion_called;
    gboolean timed_out;
} ToolTaskWaitContext;

/**
 * @struct ToolTaskCancellationContext
 * @brief Contexte utilisé pendant un test d'annulation.
 */
typedef struct
{
    GMainLoop *main_loop;
    BackgroundTask *background_task;

    guint cancel_source_id;
    guint timeout_source_id;

    gboolean completion_called;
    gboolean timed_out;
} ToolTaskCancellationContext;

/**
 * @struct ToolTaskConcurrentContext
 * @brief Contexte d'attente de deux tâches simultanées.
 */
typedef struct
{
    GMainLoop *main_loop;

    BackgroundTask *first_task;
    BackgroundTask *second_task;

    guint completed_count;
    guint timeout_source_id;

    gboolean timed_out;
} ToolTaskConcurrentContext;

static char *test_tool_task_shared_directory = NULL;

static void test_tool_task_write_script(
    ToolTaskFixture *fixture,
    const char *script_content
);

static void test_tool_task_fixture_setup(
    ToolTaskFixture *fixture,
    gconstpointer user_data
);

static ToolRegistry *test_tool_task_create_registry(
    ToolTaskFixture *fixture,
    gboolean create_executable,
    gboolean refresh_registry
);

static gboolean test_tool_task_on_timeout(
    gpointer user_data
);

static void test_tool_task_on_completed(
    BackgroundTask *background_task,
    gpointer user_data
);

static gboolean test_tool_task_start_and_wait(
    ToolTask *tool_task,
    ToolTaskWaitContext *wait_context,
    GError **error
);

static void test_tool_task_assert_bytes_equal_text(
    GBytes *bytes,
    const char *expected_text
);

static ToolRegistry *test_tool_task_create_registry(
    ToolTaskFixture *fixture,
    gboolean create_executable,
    gboolean refresh_registry
);

static gboolean test_tool_task_cancel_task(
    gpointer user_data
);

static gboolean test_tool_task_cancellation_timeout(
    gpointer user_data
);

static void test_tool_task_on_cancellation_completed(
    BackgroundTask *background_task,
    gpointer user_data
);

static gboolean test_tool_task_concurrent_timeout(
    gpointer user_data
);

static void test_tool_task_on_concurrent_completed(
    BackgroundTask *background_task,
    gpointer user_data
);

static void test_tool_task_cancellation(
    ToolTaskFixture *fixture,
    gconstpointer user_data
);

static void test_tool_task_concurrent_tasks(
    ToolTaskFixture *fixture,
    gconstpointer user_data
);

/**
 * @brief Demande l'annulation de la tâche testée.
 */
static gboolean test_tool_task_cancel_task(
    gpointer user_data
)
{
    ToolTaskCancellationContext *cancellation_context =
        user_data;

    if (cancellation_context == NULL)
    {
        return G_SOURCE_REMOVE;
    }

    cancellation_context->cancel_source_id = 0;

    background_task_cancel(
        cancellation_context->background_task
    );

    return G_SOURCE_REMOVE;
}

/**
 * @brief Interrompt le test d'annulation en cas de blocage.
 */
static gboolean test_tool_task_cancellation_timeout(
    gpointer user_data
)
{
    ToolTaskCancellationContext *cancellation_context =
        user_data;

    if (cancellation_context == NULL)
    {
        return G_SOURCE_REMOVE;
    }

    cancellation_context->timeout_source_id = 0;
    cancellation_context->timed_out = TRUE;

    if (cancellation_context->main_loop != NULL)
    {
        g_main_loop_quit(
            cancellation_context->main_loop
        );
    }

    return G_SOURCE_REMOVE;
}

/**
 * @brief Reçoit la fin de la tâche annulée.
 */
static void test_tool_task_on_cancellation_completed(
    BackgroundTask *background_task,
    gpointer user_data
)
{
    ToolTaskCancellationContext *cancellation_context =
        user_data;

    if (cancellation_context == NULL)
    {
        return;
    }

    g_assert_true(
        background_task ==
        cancellation_context->background_task
    );

    cancellation_context->completion_called = TRUE;

    if (cancellation_context->cancel_source_id != 0)
    {
        g_source_remove(
            cancellation_context->cancel_source_id
        );

        cancellation_context->cancel_source_id = 0;
    }

    if (cancellation_context->timeout_source_id != 0)
    {
        g_source_remove(
            cancellation_context->timeout_source_id
        );

        cancellation_context->timeout_source_id = 0;
    }

    if (cancellation_context->main_loop != NULL)
    {
        g_main_loop_quit(
            cancellation_context->main_loop
        );
    }
}

/**
 * @brief Interrompt un test resté bloqué trop longtemps.
 *
 * @param user_data Pointeur vers ToolTaskWaitContext.
 *
 * @return G_SOURCE_REMOVE.
 */
static gboolean test_tool_task_on_timeout(
    gpointer user_data
)
{
    ToolTaskWaitContext *wait_context =
        user_data;

    if (wait_context == NULL)
    {
        return G_SOURCE_REMOVE;
    }

    wait_context->timeout_source_id = 0;
    wait_context->timed_out = TRUE;

    if (wait_context->main_loop != NULL)
    {
        g_main_loop_quit(
            wait_context->main_loop
        );
    }

    return G_SOURCE_REMOVE;
}

/**
 * @brief Reçoit la notification finale d'une BackgroundTask.
 *
 * @param background_task Tâche terminée.
 * @param user_data Pointeur vers ToolTaskWaitContext.
 */
static void test_tool_task_on_completed(
    BackgroundTask *background_task,
    gpointer user_data
)
{
    ToolTaskWaitContext *wait_context =
        user_data;

    if (wait_context == NULL)
    {
        return;
    }

    wait_context->completion_called = TRUE;
    wait_context->completed_task =
        background_task;

    if (wait_context->timeout_source_id != 0)
    {
        g_source_remove(
            wait_context->timeout_source_id
        );

        wait_context->timeout_source_id = 0;
    }

    if (wait_context->main_loop != NULL)
    {
        g_main_loop_quit(
            wait_context->main_loop
        );
    }
}

/**
 * @brief Démarre une ToolTask et attend sa terminaison.
 *
 * @param tool_task Tâche à démarrer.
 * @param wait_context Contexte d'attente à initialiser.
 * @param error Emplacement facultatif pour l'erreur de démarrage.
 *
 * @return TRUE si la tâche a été démarrée et terminée avant le timeout.
 */
static gboolean test_tool_task_start_and_wait(
    ToolTask *tool_task,
    ToolTaskWaitContext *wait_context,
    GError **error
)
{
    gboolean start_success = FALSE;

    if (tool_task == NULL ||
        wait_context == NULL)
    {
        return FALSE;
    }

    wait_context->main_loop =
        g_main_loop_new(
            NULL,
            FALSE
        );

    wait_context->completed_task = NULL;
    wait_context->completion_called = FALSE;
    wait_context->timed_out = FALSE;

    wait_context->timeout_source_id =
        g_timeout_add_seconds(
            5,
            test_tool_task_on_timeout,
            wait_context
        );

    start_success = tool_task_start(
        tool_task,
        test_tool_task_on_completed,
        wait_context,
        NULL,
        error
    );

    if (!start_success)
    {
        if (wait_context->timeout_source_id != 0)
        {
            g_source_remove(
                wait_context->timeout_source_id
            );

            wait_context->timeout_source_id = 0;
        }

        g_main_loop_unref(
            wait_context->main_loop
        );

        wait_context->main_loop = NULL;

        return FALSE;
    }

    g_main_loop_run(
        wait_context->main_loop
    );

    if (wait_context->timeout_source_id != 0)
    {
        g_source_remove(
            wait_context->timeout_source_id
        );

        wait_context->timeout_source_id = 0;
    }

    g_main_loop_unref(
        wait_context->main_loop
    );

    wait_context->main_loop = NULL;

    return wait_context->completion_called &&
           !wait_context->timed_out;
}

/**
 * @brief Vérifie qu'un GBytes contient exactement le texte attendu.
 *
 * @param bytes Données à vérifier.
 * @param expected_text Texte attendu.
 */
static void test_tool_task_assert_bytes_equal_text(
    GBytes *bytes,
    const char *expected_text
)
{
    gconstpointer bytes_data = NULL;

    gsize bytes_size = 0;
    gsize expected_size = 0;

    g_assert_nonnull(
        bytes
    );

    g_assert_nonnull(
        expected_text
    );

    bytes_data = g_bytes_get_data(
        bytes,
        &bytes_size
    );

    expected_size = strlen(
        expected_text
    );

    g_assert_cmpuint(
        bytes_size,
        ==,
        expected_size
    );

    g_assert_cmpmem(
        bytes_data,
        bytes_size,
        expected_text,
        expected_size
    );
}

/**
 * @brief Vérifie que ToolTask copie toutes les données nécessaires.
 */
static void test_tool_task_argument_ownership(
    ToolTaskFixture *fixture,
    gconstpointer user_data
)
{
    ToolRegistry *tool_registry = NULL;
    ToolTask *tool_task = NULL;

    BackgroundTask *background_task = NULL;

    const ToolTaskResult *task_result = NULL;
    const ToolProcessResult *process_result = NULL;

    ToolTaskWaitContext wait_context = {0};

    GBytes *stdout_bytes = NULL;

    char *first_argument = NULL;
    char *second_argument = NULL;
    char *working_directory = NULL;

    const char *arguments[3];

    GError *error = NULL;

    (void) user_data;

    test_tool_task_write_script(
        fixture,
        "#!/bin/sh\n"
        "for argument in \"$@\"\n"
        "do\n"
        "    printf '%s\\n' \"$argument\"\n"
        "done\n"
    );

    tool_registry = test_tool_task_create_registry(
        fixture,
        FALSE,
        TRUE
    );

    first_argument = g_strdup(
        "premier argument"
    );

    second_argument = g_strdup(
        "second;non-interprete"
    );

    working_directory = g_strdup(
        fixture->temporary_directory
    );

    arguments[0] = first_argument;
    arguments[1] = second_argument;
    arguments[2] = NULL;

    tool_task = tool_task_new(
        tool_registry,
        "test.fake",
        "Test de propriété",
        arguments,
        working_directory,
        &error
    );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        tool_task
    );

    /*
     * Toutes ces données doivent maintenant être indépendantes.
     */
    g_free(
        first_argument
    );

    g_free(
        second_argument
    );

    g_free(
        working_directory
    );

    tool_registry_free(
        tool_registry
    );

    tool_registry = NULL;

    background_task =
        tool_task_get_background_task(
            tool_task
        );

    g_assert_true(
        test_tool_task_start_and_wait(
            tool_task,
            &wait_context,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_cmpint(
        background_task_get_state(
            background_task
        ),
        ==,
        BACKGROUND_TASK_STATE_COMPLETED
    );

    task_result =
        tool_task_result_from_background_task(
            background_task
        );

    g_assert_nonnull(
        task_result
    );

    g_assert_cmpuint(
        tool_task_result_get_argument_count(
            task_result
        ),
        ==,
        2
    );

    g_assert_cmpstr(
        tool_task_result_get_argument(
            task_result,
            0
        ),
        ==,
        "premier argument"
    );

    g_assert_cmpstr(
        tool_task_result_get_argument(
            task_result,
            1
        ),
        ==,
        "second;non-interprete"
    );

    g_assert_cmpstr(
        tool_task_result_get_working_directory(
            task_result
        ),
        ==,
        fixture->temporary_directory
    );

    process_result =
        tool_task_result_get_process_result(
            task_result
        );

    g_assert_nonnull(
        process_result
    );

    stdout_bytes =
        tool_process_result_ref_stdout(
            process_result
        );

    test_tool_task_assert_bytes_equal_text(
        stdout_bytes,
        "premier argument\n"
        "second;non-interprete\n"
    );

    g_bytes_unref(
        stdout_bytes
    );

    tool_task_free(
        tool_task
    );
}

/**
 * @brief Vérifie une exécution complète réussie.
 */
static void test_tool_task_success(
    ToolTaskFixture *fixture,
    gconstpointer user_data
)
{
    ToolRegistry *tool_registry = NULL;
    ToolTask *tool_task = NULL;

    BackgroundTask *background_task = NULL;

    const ToolTaskResult *task_result = NULL;
    const ToolProcessResult *process_result = NULL;

    ToolTaskWaitContext wait_context = {0};

    GBytes *stdout_bytes = NULL;
    GBytes *stderr_bytes = NULL;

    const char *arguments[] =
    {
        "example.org",
        "nom avec espaces",
        NULL
    };

    GError *error = NULL;

    (void) user_data;

    test_tool_task_write_script(
        fixture,
        "#!/bin/sh\n"
        "printf 'stdout:%s|%s' \"$1\" \"$2\"\n"
        "printf 'stderr-test' >&2\n"
        "exit 0\n"
    );

    tool_registry = test_tool_task_create_registry(
        fixture,
        FALSE,
        TRUE
    );

    tool_task = tool_task_new(
        tool_registry,
        "test.fake",
        "Exécution réussie",
        arguments,
        NULL,
        &error
    );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        tool_task
    );

    background_task =
        tool_task_get_background_task(
            tool_task
        );

    g_assert_true(
        test_tool_task_start_and_wait(
            tool_task,
            &wait_context,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        wait_context.completion_called
    );

    g_assert_true(
        wait_context.completed_task ==
        background_task
    );

    g_assert_cmpint(
        background_task_get_state(
            background_task
        ),
        ==,
        BACKGROUND_TASK_STATE_COMPLETED
    );

    g_assert_cmpfloat(
        background_task_get_progress(
            background_task
        ),
        ==,
        1.0
    );

    task_result =
        tool_task_result_from_background_task(
            background_task
        );

    g_assert_nonnull(
        task_result
    );

    g_assert_cmpstr(
        tool_task_result_get_tool_identifier(
            task_result
        ),
        ==,
        "test.fake"
    );

    g_assert_cmpstr(
        tool_task_result_get_executable_path(
            task_result
        ),
        ==,
        fixture->fake_tool_path
    );

    g_assert_cmpuint(
        tool_task_result_get_argument_count(
            task_result
        ),
        ==,
        2
    );

    process_result =
        tool_task_result_get_process_result(
            task_result
        );

    g_assert_nonnull(
        process_result
    );

    g_assert_true(
        tool_process_result_is_success(
            process_result
        )
    );

    g_assert_cmpint(
        tool_process_result_get_exit_status(
            process_result
        ),
        ==,
        0
    );

    stdout_bytes =
        tool_process_result_ref_stdout(
            process_result
        );

    stderr_bytes =
        tool_process_result_ref_stderr(
            process_result
        );

    test_tool_task_assert_bytes_equal_text(
        stdout_bytes,
        "stdout:example.org|nom avec espaces"
    );

    test_tool_task_assert_bytes_equal_text(
        stderr_bytes,
        "stderr-test"
    );

    g_bytes_unref(
        stdout_bytes
    );

    g_bytes_unref(
        stderr_bytes
    );

    tool_task_free(
        tool_task
    );

    tool_registry_free(
        tool_registry
    );
}

/**
 * @brief Vérifie qu'un code non nul produit une tâche terminée.
 */
static void test_tool_task_nonzero_exit(
    ToolTaskFixture *fixture,
    gconstpointer user_data
)
{
    ToolRegistry *tool_registry = NULL;
    ToolTask *tool_task = NULL;

    BackgroundTask *background_task = NULL;

    const ToolTaskResult *task_result = NULL;
    const ToolProcessResult *process_result = NULL;

    ToolTaskWaitContext wait_context = {0};

    GError *error = NULL;

    (void) user_data;

    test_tool_task_write_script(
        fixture,
        "#!/bin/sh\n"
        "printf 'erreur fonctionnelle' >&2\n"
        "exit 7\n"
    );

    tool_registry = test_tool_task_create_registry(
        fixture,
        FALSE,
        TRUE
    );

    tool_task = tool_task_new(
        tool_registry,
        "test.fake",
        "Code non nul",
        NULL,
        NULL,
        &error
    );

    g_assert_no_error(
        error
    );

    background_task =
        tool_task_get_background_task(
            tool_task
        );

    g_assert_true(
        test_tool_task_start_and_wait(
            tool_task,
            &wait_context,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_cmpint(
        background_task_get_state(
            background_task
        ),
        ==,
        BACKGROUND_TASK_STATE_COMPLETED
    );

    task_result =
        tool_task_result_from_background_task(
            background_task
        );

    g_assert_nonnull(
        task_result
    );

    process_result =
        tool_task_result_get_process_result(
            task_result
        );

    g_assert_nonnull(
        process_result
    );

    g_assert_false(
        tool_process_result_is_success(
            process_result
        )
    );

    g_assert_cmpint(
        tool_process_result_get_exit_status(
            process_result
        ),
        ==,
        7
    );

    tool_task_free(
        tool_task
    );

    tool_registry_free(
        tool_registry
    );
}

/**
 * @brief Vérifie l'échec lorsque l'exécutable disparaît avant le lancement.
 */
static void test_tool_task_spawn_failure(
    ToolTaskFixture *fixture,
    gconstpointer user_data
)
{
    ToolRegistry *tool_registry = NULL;
    ToolTask *tool_task = NULL;

    BackgroundTask *background_task = NULL;

    ToolTaskWaitContext wait_context = {0};

    GError *task_error = NULL;
    GError *stored_error = NULL;

    (void) user_data;

    tool_registry = test_tool_task_create_registry(
        fixture,
        TRUE,
        TRUE
    );

    tool_task = tool_task_new(
        tool_registry,
        "test.fake",
        "Échec de lancement",
        NULL,
        NULL,
        &task_error
    );

    g_assert_no_error(
        task_error
    );

    g_assert_nonnull(
        tool_task
    );

    background_task =
        tool_task_get_background_task(
            tool_task
        );

    g_assert_cmpint(
        g_remove(
            fixture->fake_tool_path
        ),
        ==,
        0
    );

    /*
     * Le démarrage asynchrone réussit. C'est le worker qui découvrira
     * ensuite que l'exécutable a disparu.
     */
    g_assert_true(
        test_tool_task_start_and_wait(
            tool_task,
            &wait_context,
            &task_error
        )
    );

    g_assert_no_error(
        task_error
    );

    g_assert_cmpint(
        background_task_get_state(
            background_task
        ),
        ==,
        BACKGROUND_TASK_STATE_FAILED
    );

    g_assert_null(
        tool_task_result_from_background_task(
            background_task
        )
    );

    stored_error =
        background_task_dup_error(
            background_task
        );

    g_assert_nonnull(
        stored_error
    );

    g_assert_error(
        stored_error,
        TOOL_TASK_ERROR,
        TOOL_TASK_ERROR_PROCESS
    );

    g_clear_error(
        &stored_error
    );

    tool_task_free(
        tool_task
    );

    tool_registry_free(
        tool_registry
    );
}

/**
 * @brief Vérifie la transmission du dossier de travail.
 */
static void test_tool_task_working_directory(
    ToolTaskFixture *fixture,
    gconstpointer user_data
)
{
    ToolRegistry *tool_registry = NULL;
    ToolTask *tool_task = NULL;

    BackgroundTask *background_task = NULL;

    const ToolTaskResult *task_result = NULL;
    const ToolProcessResult *process_result = NULL;

    ToolTaskWaitContext wait_context = {0};

    GBytes *stdout_bytes = NULL;

    char *canonical_directory = NULL;

    GError *error = NULL;

    (void) user_data;

    test_tool_task_write_script(
        fixture,
        "#!/bin/sh\n"
        "printf '%s' \"$PWD\"\n"
    );

    tool_registry = test_tool_task_create_registry(
        fixture,
        FALSE,
        TRUE
    );

    tool_task = tool_task_new(
        tool_registry,
        "test.fake",
        "Dossier de travail",
        NULL,
        fixture->temporary_directory,
        &error
    );

    g_assert_no_error(
        error
    );

    background_task =
        tool_task_get_background_task(
            tool_task
        );

    g_assert_true(
        test_tool_task_start_and_wait(
            tool_task,
            &wait_context,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    task_result =
        tool_task_result_from_background_task(
            background_task
        );

    g_assert_nonnull(
        task_result
    );

    process_result =
        tool_task_result_get_process_result(
            task_result
        );

    stdout_bytes =
        tool_process_result_ref_stdout(
            process_result
        );

    canonical_directory =
        g_canonicalize_filename(
            fixture->temporary_directory,
            NULL
        );

    test_tool_task_assert_bytes_equal_text(
        stdout_bytes,
        canonical_directory
    );

    g_assert_cmpstr(
        tool_task_result_get_working_directory(
            task_result
        ),
        ==,
        fixture->temporary_directory
    );

    g_bytes_unref(
        stdout_bytes
    );

    g_free(
        canonical_directory
    );

    tool_task_free(
        tool_task
    );

    tool_registry_free(
        tool_registry
    );
}

/**
 * @brief Crée un faux exécutable dans le dossier temporaire.
 *
 * @param fixture Fixture du test.
 * @param script_content Contenu complet du script.
 */
static void test_tool_task_write_script(
    ToolTaskFixture *fixture,
    const char *script_content
)
{
    GError *error = NULL;
    gboolean write_success = FALSE;
    int chmod_result = 0;

    g_assert_nonnull(
        fixture
    );

    g_assert_nonnull(
        fixture->fake_tool_path
    );

    g_assert_nonnull(
        script_content
    );

    write_success = g_file_set_contents(
        fixture->fake_tool_path,
        script_content,
        -1,
        &error
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        write_success
    );

    chmod_result = g_chmod(
        fixture->fake_tool_path,
        S_IRUSR |
        S_IWUSR |
        S_IXUSR
    );

    g_assert_cmpint(
        chmod_result,
        ==,
        0
    );
}

static void test_tool_task_fixture_setup(
    ToolTaskFixture *fixture,
    gconstpointer user_data
)
{
    (void) user_data;

    g_assert_nonnull(
        fixture
    );

    g_assert_nonnull(
        test_tool_task_shared_directory
    );

    fixture->temporary_directory =
        g_strdup(
            test_tool_task_shared_directory
        );

    fixture->fake_tool_path =
        g_build_filename(
            fixture->temporary_directory,
            "fake_tool",
            NULL
        );

    fixture->original_path = NULL;
}

/**
 * @brief Supprime les fichiers propres au test.
 *
 * Le dossier temporaire et le PATH sont partagés par toute la suite de
 * tests. Ils sont donc nettoyés uniquement à la fin de main().
 */
static void test_tool_task_fixture_teardown(
    ToolTaskFixture *fixture,
    gconstpointer user_data
)
{
    (void) user_data;

    if (fixture->fake_tool_path != NULL)
    {
        g_remove(
            fixture->fake_tool_path
        );
    }

    g_clear_pointer(
        &fixture->fake_tool_path,
        g_free
    );

    g_clear_pointer(
        &fixture->temporary_directory,
        g_free
    );

    g_clear_pointer(
        &fixture->original_path,
        g_free
    );
}

/**
 * @brief Crée un registre contenant le faux outil.
 *
 * @param fixture Fixture du test.
 * @param create_executable TRUE pour créer le faux exécutable.
 * @param refresh_registry TRUE pour détecter sa disponibilité.
 *
 * @return Nouveau registre.
 */
static ToolRegistry *test_tool_task_create_registry(
    ToolTaskFixture *fixture,
    gboolean create_executable,
    gboolean refresh_registry
)
{
    ToolRegistry *tool_registry = NULL;
    GError *error = NULL;

    if (create_executable)
    {
        test_tool_task_write_script(
            fixture,
            "#!/bin/sh\n"
            "exit 0\n"
        );
    }

    tool_registry = tool_registry_new();

    g_assert_nonnull(
        tool_registry
    );

    g_assert_true(
        tool_registry_register(
            tool_registry,
            "test.fake",
            "Fake Tool",
            "fake_tool",
            TOOL_REQUIREMENT_OPTIONAL,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    if (refresh_registry)
    {
        g_assert_true(
            tool_registry_refresh(
                tool_registry,
                &error
            )
        );

        g_assert_no_error(
            error
        );
    }

    return tool_registry;
}

/**
 * @brief Vérifie le refus des arguments invalides.
 */
static void test_tool_task_invalid_arguments(
    ToolTaskFixture *fixture,
    gconstpointer user_data
)
{
    ToolRegistry *tool_registry = NULL;
    ToolTask *tool_task = NULL;
    GError *error = NULL;

    (void) user_data;

    tool_registry = test_tool_task_create_registry(
        fixture,
        TRUE,
        TRUE
    );

    tool_task = tool_task_new(
        NULL,
        "test.fake",
        "Tâche de test",
        NULL,
        NULL,
        &error
    );

    g_assert_null(
        tool_task
    );

    g_assert_error(
        error,
        TOOL_TASK_ERROR,
        TOOL_TASK_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    tool_task = tool_task_new(
        tool_registry,
        NULL,
        "Tâche de test",
        NULL,
        NULL,
        &error
    );

    g_assert_null(
        tool_task
    );

    g_assert_error(
        error,
        TOOL_TASK_ERROR,
        TOOL_TASK_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    tool_task = tool_task_new(
        tool_registry,
        "",
        "Tâche de test",
        NULL,
        NULL,
        &error
    );

    g_assert_null(
        tool_task
    );

    g_assert_error(
        error,
        TOOL_TASK_ERROR,
        TOOL_TASK_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    tool_task = tool_task_new(
        tool_registry,
        "test.fake",
        NULL,
        NULL,
        NULL,
        &error
    );

    g_assert_null(
        tool_task
    );

    g_assert_error(
        error,
        TOOL_TASK_ERROR,
        TOOL_TASK_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    tool_task = tool_task_new(
        tool_registry,
        "test.fake",
        "",
        NULL,
        NULL,
        &error
    );

    g_assert_null(
        tool_task
    );

    g_assert_error(
        error,
        TOOL_TASK_ERROR,
        TOOL_TASK_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    tool_task = tool_task_new(
        tool_registry,
        "test.fake",
        "Tâche de test",
        NULL,
        "",
        &error
    );

    g_assert_null(
        tool_task
    );

    g_assert_error(
        error,
        TOOL_TASK_ERROR,
        TOOL_TASK_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    tool_registry_free(
        tool_registry
    );
}

/**
 * @brief Vérifie le refus d'un identifiant inconnu.
 */
static void test_tool_task_tool_not_found(
    ToolTaskFixture *fixture,
    gconstpointer user_data
)
{
    ToolRegistry *tool_registry = NULL;
    ToolTask *tool_task = NULL;
    GError *error = NULL;

    (void) fixture;
    (void) user_data;

    tool_registry = tool_registry_new();

    g_assert_nonnull(
        tool_registry
    );

    tool_task = tool_task_new(
        tool_registry,
        "unknown.tool",
        "Tâche inconnue",
        NULL,
        NULL,
        &error
    );

    g_assert_null(
        tool_task
    );

    g_assert_error(
        error,
        TOOL_TASK_ERROR,
        TOOL_TASK_ERROR_TOOL_NOT_FOUND
    );

    g_clear_error(
        &error
    );

    tool_registry_free(
        tool_registry
    );
}

/**
 * @brief Vérifie le refus d'un outil non encore contrôlé.
 */
static void test_tool_task_tool_not_checked(
    ToolTaskFixture *fixture,
    gconstpointer user_data
)
{
    ToolRegistry *tool_registry = NULL;
    ToolTask *tool_task = NULL;
    GError *error = NULL;

    (void) user_data;

    tool_registry = test_tool_task_create_registry(
        fixture,
        TRUE,
        FALSE
    );

    tool_task = tool_task_new(
        tool_registry,
        "test.fake",
        "Tâche non vérifiée",
        NULL,
        NULL,
        &error
    );

    g_assert_null(
        tool_task
    );

    g_assert_error(
        error,
        TOOL_TASK_ERROR,
        TOOL_TASK_ERROR_TOOL_NOT_CHECKED
    );

    g_clear_error(
        &error
    );

    tool_registry_free(
        tool_registry
    );
}

/**
 * @brief Vérifie le refus d'un outil explicitement absent.
 */
static void test_tool_task_tool_missing(
    ToolTaskFixture *fixture,
    gconstpointer user_data
)
{
    ToolRegistry *tool_registry = NULL;
    ToolTask *tool_task = NULL;
    GError *error = NULL;

    (void) user_data;

    /*
     * Aucun fichier fake_tool n'est créé avant le rafraîchissement.
     */
    tool_registry = test_tool_task_create_registry(
        fixture,
        FALSE,
        TRUE
    );

    tool_task = tool_task_new(
        tool_registry,
        "test.fake",
        "Tâche impossible",
        NULL,
        NULL,
        &error
    );

    g_assert_null(
        tool_task
    );

    g_assert_error(
        error,
        TOOL_TASK_ERROR,
        TOOL_TASK_ERROR_TOOL_MISSING
    );

    g_clear_error(
        &error
    );

    tool_registry_free(
        tool_registry
    );
}

/**
 * @brief Vérifie la création d'une ToolTask valide.
 */
static void test_tool_task_create(
    ToolTaskFixture *fixture,
    gconstpointer user_data
)
{
    ToolRegistry *tool_registry = NULL;
    ToolTask *tool_task = NULL;
    BackgroundTask *background_task = NULL;

    const char *arguments[] =
    {
        "premier argument",
        "second",
        NULL
    };

    GError *error = NULL;

    (void) user_data;

    tool_registry = test_tool_task_create_registry(
        fixture,
        TRUE,
        TRUE
    );

    tool_task = tool_task_new(
        tool_registry,
        "test.fake",
        "Exécution du faux outil",
        arguments,
        fixture->temporary_directory,
        &error
    );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        tool_task
    );

    background_task =
        tool_task_get_background_task(
            tool_task
        );

    g_assert_nonnull(
        background_task
    );

    g_assert_cmpstr(
        background_task_get_title(
            background_task
        ),
        ==,
        "Exécution du faux outil"
    );

    g_assert_cmpint(
        background_task_get_state(
            background_task
        ),
        ==,
        BACKGROUND_TASK_STATE_PENDING
    );

    g_assert_cmpfloat(
        background_task_get_progress(
            background_task
        ),
        ==,
        0.0
    );

    /*
     * Vérifie aussi la destruction d'une tâche jamais démarrée.
     */
    tool_task_free(
        tool_task
    );

    tool_registry_free(
        tool_registry
    );
}

/**
 * @brief Vérifie l'annulation d'un outil externe en cours.
 */
static void test_tool_task_cancellation(
    ToolTaskFixture *fixture,
    gconstpointer user_data
)
{
    ToolRegistry *tool_registry = NULL;
    ToolTask *tool_task = NULL;
    BackgroundTask *background_task = NULL;

    ToolTaskCancellationContext cancellation_context = {0};

    GError *error = NULL;

    gboolean start_success = FALSE;

    (void) user_data;

    /*
     * Boucle entièrement interne au shell.
     * Aucun programme externe comme sleep n'est lancé.
     */
    test_tool_task_write_script(
        fixture,
        "#!/bin/sh\n"
        "while :\n"
        "do\n"
        "    :\n"
        "done\n"
    );

    tool_registry = test_tool_task_create_registry(
        fixture,
        FALSE,
        TRUE
    );

    tool_task = tool_task_new(
        tool_registry,
        "test.fake",
        "Annulation du faux outil",
        NULL,
        NULL,
        &error
    );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        tool_task
    );

    background_task =
        tool_task_get_background_task(
            tool_task
        );

    cancellation_context.main_loop =
        g_main_loop_new(
            NULL,
            FALSE
        );

    cancellation_context.background_task =
        background_task;

    cancellation_context.cancel_source_id =
        g_timeout_add(
            200,
            test_tool_task_cancel_task,
            &cancellation_context
        );

    cancellation_context.timeout_source_id =
        g_timeout_add_seconds(
            5,
            test_tool_task_cancellation_timeout,
            &cancellation_context
        );

    start_success = tool_task_start(
        tool_task,
        test_tool_task_on_cancellation_completed,
        &cancellation_context,
        NULL,
        &error
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        start_success
    );

    g_main_loop_run(
        cancellation_context.main_loop
    );

    if (cancellation_context.cancel_source_id != 0)
    {
        g_source_remove(
            cancellation_context.cancel_source_id
        );
    }

    if (cancellation_context.timeout_source_id != 0)
    {
        g_source_remove(
            cancellation_context.timeout_source_id
        );
    }

    g_main_loop_unref(
        cancellation_context.main_loop
    );

    cancellation_context.main_loop = NULL;

    g_assert_false(
        cancellation_context.timed_out
    );

    g_assert_true(
        cancellation_context.completion_called
    );

    g_assert_cmpint(
        background_task_get_state(
            background_task
        ),
        ==,
        BACKGROUND_TASK_STATE_CANCELLED
    );

    g_assert_null(
        tool_task_result_from_background_task(
            background_task
        )
    );

    tool_task_free(
        tool_task
    );

    tool_registry_free(
        tool_registry
    );
}

/**
 * @brief Interrompt le test concurrent en cas de blocage.
 */
static gboolean test_tool_task_concurrent_timeout(
    gpointer user_data
)
{
    ToolTaskConcurrentContext *concurrent_context =
        user_data;

    if (concurrent_context == NULL)
    {
        return G_SOURCE_REMOVE;
    }

    concurrent_context->timeout_source_id = 0;
    concurrent_context->timed_out = TRUE;

    if (concurrent_context->main_loop != NULL)
    {
        g_main_loop_quit(
            concurrent_context->main_loop
        );
    }

    return G_SOURCE_REMOVE;
}

/**
 * @brief Compte les tâches concurrentes terminées.
 */
static void test_tool_task_on_concurrent_completed(
    BackgroundTask *background_task,
    gpointer user_data
)
{
    ToolTaskConcurrentContext *concurrent_context =
        user_data;

    if (concurrent_context == NULL)
    {
        return;
    }

    g_assert_true(
        background_task ==
            concurrent_context->first_task ||
        background_task ==
            concurrent_context->second_task
    );

    concurrent_context->completed_count++;

    if (concurrent_context->completed_count < 2)
    {
        return;
    }

    if (concurrent_context->timeout_source_id != 0)
    {
        g_source_remove(
            concurrent_context->timeout_source_id
        );

        concurrent_context->timeout_source_id = 0;
    }

    if (concurrent_context->main_loop != NULL)
    {
        g_main_loop_quit(
            concurrent_context->main_loop
        );
    }
}

/**
 * @brief Vérifie l'indépendance de deux ToolTask simultanées.
 */
static void test_tool_task_concurrent_tasks(
    ToolTaskFixture *fixture,
    gconstpointer user_data
)
{
    ToolRegistry *tool_registry = NULL;

    ToolTask *first_tool_task = NULL;
    ToolTask *second_tool_task = NULL;

    BackgroundTask *first_background_task = NULL;
    BackgroundTask *second_background_task = NULL;

    const ToolTaskResult *first_task_result = NULL;
    const ToolTaskResult *second_task_result = NULL;

    const ToolProcessResult *first_process_result = NULL;
    const ToolProcessResult *second_process_result = NULL;

    ToolTaskConcurrentContext concurrent_context = {0};

    GBytes *first_stdout = NULL;
    GBytes *second_stdout = NULL;

    const char *first_arguments[] =
    {
        "premiere-tache",
        NULL
    };

    const char *second_arguments[] =
    {
        "seconde-tache",
        NULL
    };

    GError *error = NULL;

    (void) user_data;

    test_tool_task_write_script(
        fixture,
        "#!/bin/sh\n"
        "counter=0\n"
        "while [ \"$counter\" -lt 10000 ]\n"
        "do\n"
        "    counter=$((counter + 1))\n"
        "done\n"
        "printf '%s' \"$1\"\n"
    );

    tool_registry = test_tool_task_create_registry(
        fixture,
        FALSE,
        TRUE
    );

    first_tool_task = tool_task_new(
        tool_registry,
        "test.fake",
        "Première tâche",
        first_arguments,
        NULL,
        &error
    );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        first_tool_task
    );

    second_tool_task = tool_task_new(
        tool_registry,
        "test.fake",
        "Seconde tâche",
        second_arguments,
        NULL,
        &error
    );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        second_tool_task
    );

    first_background_task =
        tool_task_get_background_task(
            first_tool_task
        );

    second_background_task =
        tool_task_get_background_task(
            second_tool_task
        );

    concurrent_context.main_loop =
        g_main_loop_new(
            NULL,
            FALSE
        );

    concurrent_context.first_task =
        first_background_task;

    concurrent_context.second_task =
        second_background_task;

    concurrent_context.timeout_source_id =
        g_timeout_add_seconds(
            5,
            test_tool_task_concurrent_timeout,
            &concurrent_context
        );

    g_assert_true(
        tool_task_start(
            first_tool_task,
            test_tool_task_on_concurrent_completed,
            &concurrent_context,
            NULL,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        tool_task_start(
            second_tool_task,
            test_tool_task_on_concurrent_completed,
            &concurrent_context,
            NULL,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_main_loop_run(
        concurrent_context.main_loop
    );

    if (concurrent_context.timeout_source_id != 0)
    {
        g_source_remove(
            concurrent_context.timeout_source_id
        );
    }

    g_main_loop_unref(
        concurrent_context.main_loop
    );

    concurrent_context.main_loop = NULL;

    g_assert_false(
        concurrent_context.timed_out
    );

    g_assert_cmpuint(
        concurrent_context.completed_count,
        ==,
        2
    );

    g_assert_cmpint(
        background_task_get_state(
            first_background_task
        ),
        ==,
        BACKGROUND_TASK_STATE_COMPLETED
    );

    g_assert_cmpint(
        background_task_get_state(
            second_background_task
        ),
        ==,
        BACKGROUND_TASK_STATE_COMPLETED
    );

    first_task_result =
        tool_task_result_from_background_task(
            first_background_task
        );

    second_task_result =
        tool_task_result_from_background_task(
            second_background_task
        );

    g_assert_nonnull(
        first_task_result
    );

    g_assert_nonnull(
        second_task_result
    );

    g_assert_cmpstr(
        tool_task_result_get_argument(
            first_task_result,
            0
        ),
        ==,
        "premiere-tache"
    );

    g_assert_cmpstr(
        tool_task_result_get_argument(
            second_task_result,
            0
        ),
        ==,
        "seconde-tache"
    );

    first_process_result =
        tool_task_result_get_process_result(
            first_task_result
        );

    second_process_result =
        tool_task_result_get_process_result(
            second_task_result
        );

    first_stdout =
        tool_process_result_ref_stdout(
            first_process_result
        );

    second_stdout =
        tool_process_result_ref_stdout(
            second_process_result
        );

    test_tool_task_assert_bytes_equal_text(
        first_stdout,
        "premiere-tache"
    );

    test_tool_task_assert_bytes_equal_text(
        second_stdout,
        "seconde-tache"
    );

    g_bytes_unref(
        first_stdout
    );

    g_bytes_unref(
        second_stdout
    );

    tool_task_free(
        first_tool_task
    );

    tool_task_free(
        second_tool_task
    );

    tool_registry_free(
        tool_registry
    );
}

int main(
    int argc,
    char **argv
)
{
    GError *error = NULL;
    int test_result = 0;
    
    g_test_init(
        &argc,
        &argv,
        NULL
    );
    test_tool_task_shared_directory =
    g_dir_make_tmp(
        "labfy-tool-task-XXXXXX",
        &error
    );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        test_tool_task_shared_directory
    );

    g_assert_true(
        g_setenv(
            "PATH",
            test_tool_task_shared_directory,
            TRUE
        )
    );

    g_test_add(
        "/tool_task/invalid_arguments",
        ToolTaskFixture,
        NULL,
        test_tool_task_fixture_setup,
        test_tool_task_invalid_arguments,
        test_tool_task_fixture_teardown
    );

    g_test_add(
        "/tool_task/tool_not_found",
        ToolTaskFixture,
        NULL,
        test_tool_task_fixture_setup,
        test_tool_task_tool_not_found,
        test_tool_task_fixture_teardown
    );

    g_test_add(
        "/tool_task/tool_not_checked",
        ToolTaskFixture,
        NULL,
        test_tool_task_fixture_setup,
        test_tool_task_tool_not_checked,
        test_tool_task_fixture_teardown
    );

    g_test_add(
        "/tool_task/tool_missing",
        ToolTaskFixture,
        NULL,
        test_tool_task_fixture_setup,
        test_tool_task_tool_missing,
        test_tool_task_fixture_teardown
    );

    g_test_add(
        "/tool_task/create",
        ToolTaskFixture,
        NULL,
        test_tool_task_fixture_setup,
        test_tool_task_create,
        test_tool_task_fixture_teardown
    );

    g_test_add(
        "/tool_task/argument_ownership",
        ToolTaskFixture,
        NULL,
        test_tool_task_fixture_setup,
        test_tool_task_argument_ownership,
        test_tool_task_fixture_teardown
    );

    g_test_add(
        "/tool_task/success",
        ToolTaskFixture,
        NULL,
        test_tool_task_fixture_setup,
        test_tool_task_success,
        test_tool_task_fixture_teardown
    );

    g_test_add(
        "/tool_task/nonzero_exit",
        ToolTaskFixture,
        NULL,
        test_tool_task_fixture_setup,
        test_tool_task_nonzero_exit,
        test_tool_task_fixture_teardown
    );

    g_test_add(
        "/tool_task/spawn_failure",
        ToolTaskFixture,
        NULL,
        test_tool_task_fixture_setup,
        test_tool_task_spawn_failure,
        test_tool_task_fixture_teardown
    );

    g_test_add(
        "/tool_task/working_directory",
        ToolTaskFixture,
        NULL,
        test_tool_task_fixture_setup,
        test_tool_task_working_directory,
        test_tool_task_fixture_teardown
    );

    g_test_add(
        "/tool_task/cancellation",
        ToolTaskFixture,
        NULL,
        test_tool_task_fixture_setup,
        test_tool_task_cancellation,
        test_tool_task_fixture_teardown
    );

    g_test_add(
        "/tool_task/concurrent_tasks",
        ToolTaskFixture,
        NULL,
        test_tool_task_fixture_setup,
        test_tool_task_concurrent_tasks,
        test_tool_task_fixture_teardown
    );

    test_result = g_test_run();

    g_rmdir(
        test_tool_task_shared_directory
    );

    g_clear_pointer(
        &test_tool_task_shared_directory,
        g_free
    );

    return test_result;
}
