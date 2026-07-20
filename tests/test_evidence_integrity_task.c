/******************************************************************************
 * @file test_evidence_integrity_task.c
 * @brief Tests de la tâche asynchrone de vérification d'intégrité.
 ******************************************************************************/

#ifndef FILE_HASH_ENABLE_TEST_HOOKS
#define FILE_HASH_ENABLE_TEST_HOOKS
#endif

#include "core/evidence_integrity_task.h"

#include "core/background_task.h"
#include "core/evidence_integrity_verifier.h"
#include "core/file_hash.h"
#include "core/task_manager.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

#define TEST_EVIDENCE_INTEGRITY_TASK_TIMEOUT_SECONDS 5

#define TEST_SHA256_ABC \
    "ba7816bf8f01cfea414140de5dae2223" \
    "b00361a396177a9cb410ff61f20015ad"

#define TEST_SHA256_EMPTY \
    "e3b0c44298fc1c149afbf4c8996fb924" \
    "27ae41e4649b934ca495991b7852b855"

/**
 * @brief Contexte permettant d'attendre la fin d'une tâche.
 */
typedef struct
{
    GMainLoop *main_loop;

    GThread *main_thread;
    GThread *callback_thread;

    guint completion_count;

    gboolean completed;
    gboolean timed_out;
} TestEvidenceIntegrityTaskCompletionContext;

/**
 * @brief Enquête temporaire utilisée par les tests.
 */
typedef struct
{
    char *root_directory;
    char *evidence_directory;
    char *documents_directory;

    TaskManager *task_manager;
} TestEvidenceIntegrityTaskFixture;

/**
 * @brief Contexte du crochet d'annulation après le premier bloc lu.
 */
typedef struct
{
    GMutex mutex;
    GCond condition;

    BackgroundTask *task;

    guint block_count;
    gboolean task_is_ready;
} TestEvidenceIntegrityTaskCancellationContext;

/**
 * @brief Callback final commun aux tests.
 */
static void test_evidence_integrity_task_completed(
    BackgroundTask *task,
    gpointer user_data
)
{
    TestEvidenceIntegrityTaskCompletionContext *context =
        user_data;

    assert(task != NULL);
    assert(context != NULL);
    assert(context->main_loop != NULL);

    context->callback_thread =
        g_thread_self();

    context->completion_count++;
    context->completed = TRUE;

    g_main_loop_quit(
        context->main_loop
    );
}

/**
 * @brief Interrompt un test asynchrone trop long.
 */
static gboolean test_evidence_integrity_task_timeout(
    gpointer user_data
)
{
    TestEvidenceIntegrityTaskCompletionContext *context =
        user_data;

    assert(context != NULL);

    context->timed_out = TRUE;

    g_main_loop_quit(
        context->main_loop
    );

    return G_SOURCE_REMOVE;
}

/**
 * @brief Initialise un contexte d'attente.
 */
static TestEvidenceIntegrityTaskCompletionContext
test_evidence_integrity_task_completion_context_create(void)
{
    TestEvidenceIntegrityTaskCompletionContext context =
        {0};

    context.main_loop =
        g_main_loop_new(
            NULL,
            FALSE
        );

    assert(context.main_loop != NULL);

    context.main_thread =
        g_thread_self();

    return context;
}

/**
 * @brief Libère un contexte d'attente.
 */
static void test_evidence_integrity_task_completion_context_clear(
    TestEvidenceIntegrityTaskCompletionContext *context
)
{
    assert(context != NULL);
    assert(context->main_loop != NULL);

    g_main_loop_unref(
        context->main_loop
    );

    context->main_loop = NULL;
    context->main_thread = NULL;
    context->callback_thread = NULL;
}

/**
 * @brief Attend la fin d'une tâche avec un délai maximal.
 */
static void test_evidence_integrity_task_wait(
    TestEvidenceIntegrityTaskCompletionContext *context
)
{
    guint timeout_source_id = 0;

    assert(context != NULL);
    assert(context->main_loop != NULL);

    timeout_source_id =
        g_timeout_add_seconds(
            TEST_EVIDENCE_INTEGRITY_TASK_TIMEOUT_SECONDS,
            test_evidence_integrity_task_timeout,
            context
        );

    assert(timeout_source_id != 0);

    g_main_loop_run(
        context->main_loop
    );

    if (!context->timed_out)
    {
        assert(
            g_source_remove(
                timeout_source_id
            )
        );
    }

    assert(!context->timed_out);
    assert(context->completed);
    assert(context->completion_count == 1);

    /*
     * GTask rappelle le callback sur le contexte principal ayant
     * démarré la tâche.
     */
    assert(
        context->callback_thread ==
        context->main_thread
    );
}

/**
 * @brief Crée une enquête temporaire minimale.
 */
static TestEvidenceIntegrityTaskFixture
test_evidence_integrity_task_fixture_create(void)
{
    TestEvidenceIntegrityTaskFixture fixture =
        {0};

    GError *error =
        NULL;

    fixture.root_directory =
        g_dir_make_tmp(
            "labfy-evidence-integrity-task-test-XXXXXX",
            &error
        );

    assert(fixture.root_directory != NULL);
    assert(error == NULL);

    fixture.evidence_directory =
        g_build_filename(
            fixture.root_directory,
            "01_Preuves_Originales",
            NULL
        );

    fixture.documents_directory =
        g_build_filename(
            fixture.evidence_directory,
            "Documents",
            NULL
        );

    assert(fixture.evidence_directory != NULL);
    assert(fixture.documents_directory != NULL);

    assert(
        g_mkdir(
            fixture.evidence_directory,
            0700
        ) == 0
    );

    assert(
        g_mkdir(
            fixture.documents_directory,
            0700
        ) == 0
    );

    fixture.task_manager =
        task_manager_new();

    assert(fixture.task_manager != NULL);

    return fixture;
}

/**
 * @brief Libère une fixture dont les fichiers ont déjà été supprimés.
 */
static void test_evidence_integrity_task_fixture_clear(
    TestEvidenceIntegrityTaskFixture *fixture
)
{
    assert(fixture != NULL);

    task_manager_free(
        fixture->task_manager
    );

    assert(
        g_rmdir(
            fixture->documents_directory
        ) == 0
    );

    assert(
        g_rmdir(
            fixture->evidence_directory
        ) == 0
    );

    assert(
        g_rmdir(
            fixture->root_directory
        ) == 0
    );

    g_free(
        fixture->documents_directory
    );

    g_free(
        fixture->evidence_directory
    );

    g_free(
        fixture->root_directory
    );

    fixture->task_manager = NULL;
    fixture->documents_directory = NULL;
    fixture->evidence_directory = NULL;
    fixture->root_directory = NULL;
}

/**
 * @brief Construit un chemin absolu dans Documents.
 */
static char *test_evidence_integrity_task_build_file_path(
    const TestEvidenceIntegrityTaskFixture *fixture,
    const char *file_name
)
{
    assert(fixture != NULL);
    assert(fixture->documents_directory != NULL);
    assert(file_name != NULL);

    return g_build_filename(
        fixture->documents_directory,
        file_name,
        NULL
    );
}

/**
 * @brief Construit le chemin relatif enregistré dans SQLite.
 */
static char *test_evidence_integrity_task_build_relative_path(
    const char *file_name
)
{
    assert(file_name != NULL);

    return g_build_filename(
        "01_Preuves_Originales",
        "Documents",
        file_name,
        NULL
    );
}

/**
 * @brief Démarre une tâche avec un callback de test.
 */
static BackgroundTask *test_evidence_integrity_task_start(
    TestEvidenceIntegrityTaskFixture *fixture,
    const EvidenceIntegrityTaskRequest *request,
    TestEvidenceIntegrityTaskCompletionContext *completion_context
)
{
    BackgroundTask *task =
        NULL;

    GError *error =
        NULL;

    assert(fixture != NULL);
    assert(fixture->task_manager != NULL);
    assert(request != NULL);
    assert(completion_context != NULL);

    task =
        evidence_integrity_task_start(
            fixture->task_manager,
            request,
            test_evidence_integrity_task_completed,
            completion_context,
            NULL,
            &error
        );

    assert(task != NULL);
    assert(error == NULL);

    assert(
        task_manager_get_count(
            fixture->task_manager
        ) == 1
    );

    return task;
}

/**
 * @brief Vérifie l'état final et retourne le résultat emprunté.
 */
static const EvidenceIntegrityVerificationResult *
test_evidence_integrity_task_assert_completed(
    BackgroundTask *task,
    EvidenceIntegrityStatus expected_status
)
{
    const EvidenceIntegrityVerificationResult *result =
        NULL;

    GError *error =
        NULL;

    assert(task != NULL);

    assert(
        background_task_get_state(
            task
        ) ==
        BACKGROUND_TASK_STATE_COMPLETED
    );

    assert(
        background_task_get_progress(
            task
        ) == 1.0
    );

    error =
        background_task_dup_error(
            task
        );

    assert(error == NULL);

    result =
        background_task_get_result(
            task
        );

    assert(result != NULL);

    assert(
        evidence_integrity_verification_result_get_status(
            result
        ) ==
        expected_status
    );

    return result;
}

/**
 * @brief Vérifie le refus des arguments invalides.
 */
static void test_evidence_integrity_task_invalid_arguments(void)
{
    EvidenceIntegrityTaskRequest request =
        {0};

    BackgroundTask *task =
        NULL;

    TaskManager *task_manager =
        NULL;

    GError *error =
        NULL;

    task =
        evidence_integrity_task_start(
            NULL,
            &request,
            NULL,
            NULL,
            NULL,
            &error
        );

    assert(task == NULL);
    assert(error != NULL);

    assert(
        error->domain ==
        EVIDENCE_INTEGRITY_TASK_ERROR
    );

    assert(
        error->code ==
        EVIDENCE_INTEGRITY_TASK_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    task_manager =
        task_manager_new();

    assert(task_manager != NULL);

    task =
        evidence_integrity_task_start(
            task_manager,
            &request,
            NULL,
            NULL,
            NULL,
            &error
        );

    assert(task == NULL);
    assert(error != NULL);

    assert(
        error->code ==
        EVIDENCE_INTEGRITY_TASK_ERROR_INVALID_ARGUMENT
    );

    assert(
        task_manager_get_count(
            task_manager
        ) == 0
    );

    g_clear_error(
        &error
    );

    task_manager_free(
        task_manager
    );
}

/**
 * @brief Vérifie une preuve intacte.
 */
static void test_evidence_integrity_task_valid(void)
{
    TestEvidenceIntegrityTaskFixture fixture =
        test_evidence_integrity_task_fixture_create();

    TestEvidenceIntegrityTaskCompletionContext completion_context =
        test_evidence_integrity_task_completion_context_create();

    EvidenceIntegrityTaskRequest request =
        {0};

    BackgroundTask *task =
        NULL;

    const EvidenceIntegrityVerificationResult *result =
        NULL;

    char *file_path =
        NULL;

    char *relative_path =
        NULL;

    GError *error =
        NULL;

    file_path =
        test_evidence_integrity_task_build_file_path(
            &fixture,
            "intact.txt"
        );

    relative_path =
        test_evidence_integrity_task_build_relative_path(
            "intact.txt"
        );

    assert(file_path != NULL);
    assert(relative_path != NULL);

    assert(
        g_file_set_contents(
            file_path,
            "abc",
            3,
            &error
        )
    );

    assert(error == NULL);

    request.investigation_root_path =
        fixture.root_directory;

    request.relative_path =
        relative_path;

    request.expected_sha256 =
        TEST_SHA256_ABC;

    task =
        test_evidence_integrity_task_start(
            &fixture,
            &request,
            &completion_context
        );

    test_evidence_integrity_task_wait(
        &completion_context
    );

    result =
        test_evidence_integrity_task_assert_completed(
            task,
            EVIDENCE_INTEGRITY_STATUS_VALID
        );

    assert(
        strcmp(
            evidence_integrity_verification_result_get_computed_sha256(
                result
            ),
            TEST_SHA256_ABC
        ) == 0
    );

    assert(
        evidence_integrity_verification_result_get_size_bytes(
            result
        ) == 3
    );

    assert(
        background_task_get_title(
            task
        ) != NULL
    );

    background_task_unref(
        task
    );

    assert(
        g_remove(
            file_path
        ) == 0
    );

    g_free(
        relative_path
    );

    g_free(
        file_path
    );

    test_evidence_integrity_task_completion_context_clear(
        &completion_context
    );

    test_evidence_integrity_task_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie qu'une empreinte différente produit MODIFIED.
 */
static void test_evidence_integrity_task_modified(void)
{
    TestEvidenceIntegrityTaskFixture fixture =
        test_evidence_integrity_task_fixture_create();

    TestEvidenceIntegrityTaskCompletionContext completion_context =
        test_evidence_integrity_task_completion_context_create();

    EvidenceIntegrityTaskRequest request =
        {0};

    BackgroundTask *task =
        NULL;

    char *file_path =
        NULL;

    char *relative_path =
        NULL;

    GError *error =
        NULL;

    file_path =
        test_evidence_integrity_task_build_file_path(
            &fixture,
            "modified.txt"
        );

    relative_path =
        test_evidence_integrity_task_build_relative_path(
            "modified.txt"
        );

    assert(file_path != NULL);
    assert(relative_path != NULL);

    assert(
        g_file_set_contents(
            file_path,
            "abc",
            3,
            &error
        )
    );

    assert(error == NULL);

    request.investigation_root_path =
        fixture.root_directory;

    request.relative_path =
        relative_path;

    request.expected_sha256 =
        TEST_SHA256_EMPTY;

    task =
        test_evidence_integrity_task_start(
            &fixture,
            &request,
            &completion_context
        );

    test_evidence_integrity_task_wait(
        &completion_context
    );

    test_evidence_integrity_task_assert_completed(
        task,
        EVIDENCE_INTEGRITY_STATUS_MODIFIED
    );

    background_task_unref(
        task
    );

    assert(
        g_remove(
            file_path
        ) == 0
    );

    g_free(
        relative_path
    );

    g_free(
        file_path
    );

    test_evidence_integrity_task_completion_context_clear(
        &completion_context
    );

    test_evidence_integrity_task_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie qu'un fichier absent produit MISSING.
 */
static void test_evidence_integrity_task_missing(void)
{
    TestEvidenceIntegrityTaskFixture fixture =
        test_evidence_integrity_task_fixture_create();

    TestEvidenceIntegrityTaskCompletionContext completion_context =
        test_evidence_integrity_task_completion_context_create();

    EvidenceIntegrityTaskRequest request =
        {0};

    BackgroundTask *task =
        NULL;

    char *relative_path =
        NULL;

    relative_path =
        test_evidence_integrity_task_build_relative_path(
            "absent.txt"
        );

    assert(relative_path != NULL);

    request.investigation_root_path =
        fixture.root_directory;

    request.relative_path =
        relative_path;

    request.expected_sha256 =
        TEST_SHA256_ABC;

    task =
        test_evidence_integrity_task_start(
            &fixture,
            &request,
            &completion_context
        );

    test_evidence_integrity_task_wait(
        &completion_context
    );

    test_evidence_integrity_task_assert_completed(
        task,
        EVIDENCE_INTEGRITY_STATUS_MISSING
    );

    background_task_unref(
        task
    );

    g_free(
        relative_path
    );

    test_evidence_integrity_task_completion_context_clear(
        &completion_context
    );

    test_evidence_integrity_task_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie qu'un dossier produit un résultat métier ERROR.
 */
static void test_evidence_integrity_task_error_result(void)
{
    TestEvidenceIntegrityTaskFixture fixture =
        test_evidence_integrity_task_fixture_create();

    TestEvidenceIntegrityTaskCompletionContext completion_context =
        test_evidence_integrity_task_completion_context_create();

    EvidenceIntegrityTaskRequest request =
        {0};

    BackgroundTask *task =
        NULL;

    request.investigation_root_path =
        fixture.root_directory;

    request.relative_path =
        "01_Preuves_Originales/Documents";

    request.expected_sha256 =
        TEST_SHA256_ABC;

    task =
        test_evidence_integrity_task_start(
            &fixture,
            &request,
            &completion_context
        );

    test_evidence_integrity_task_wait(
        &completion_context
    );

    test_evidence_integrity_task_assert_completed(
        task,
        EVIDENCE_INTEGRITY_STATUS_ERROR
    );

    /*
     * ERROR est un résultat métier. La BackgroundTask ne doit donc
     * pas être marquée FAILED.
     */
    assert(
        background_task_get_state(
            task
        ) ==
        BACKGROUND_TASK_STATE_COMPLETED
    );

    background_task_unref(
        task
    );

    test_evidence_integrity_task_completion_context_clear(
        &completion_context
    );

    test_evidence_integrity_task_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie que la tâche possède ses propres copies des chaînes.
 */
static void test_evidence_integrity_task_argument_ownership(void)
{
    TestEvidenceIntegrityTaskFixture fixture =
        test_evidence_integrity_task_fixture_create();

    TestEvidenceIntegrityTaskCompletionContext completion_context =
        test_evidence_integrity_task_completion_context_create();

    EvidenceIntegrityTaskRequest request =
        {0};

    BackgroundTask *task =
        NULL;

    char *root_path =
        NULL;

    char *relative_path =
        NULL;

    char *expected_sha256 =
        NULL;

    char *file_path =
        NULL;

    GError *error =
        NULL;

    file_path =
        test_evidence_integrity_task_build_file_path(
            &fixture,
            "owned.txt"
        );

    assert(file_path != NULL);

    assert(
        g_file_set_contents(
            file_path,
            "abc",
            3,
            &error
        )
    );

    assert(error == NULL);

    root_path =
        g_strdup(
            fixture.root_directory
        );

    relative_path =
        test_evidence_integrity_task_build_relative_path(
            "owned.txt"
        );

    expected_sha256 =
        g_strdup(
            TEST_SHA256_ABC
        );

    assert(root_path != NULL);
    assert(relative_path != NULL);
    assert(expected_sha256 != NULL);

    request.investigation_root_path =
        root_path;

    request.relative_path =
        relative_path;

    request.expected_sha256 =
        expected_sha256;

    task =
        test_evidence_integrity_task_start(
            &fixture,
            &request,
            &completion_context
        );

    /*
     * L'appelant peut libérer immédiatement ses chaînes.
     */
    g_free(
        expected_sha256
    );

    g_free(
        relative_path
    );

    g_free(
        root_path
    );

    test_evidence_integrity_task_wait(
        &completion_context
    );

    test_evidence_integrity_task_assert_completed(
        task,
        EVIDENCE_INTEGRITY_STATUS_VALID
    );

    background_task_unref(
        task
    );

    assert(
        g_remove(
            file_path
        ) == 0
    );

    g_free(
        file_path
    );

    test_evidence_integrity_task_completion_context_clear(
        &completion_context
    );

    test_evidence_integrity_task_fixture_clear(
        &fixture
    );
}

/**
 * @brief Annule la tâche après le premier bloc lu par FileHash.
 */
static void test_evidence_integrity_task_cancel_after_first_block(
    guint64 total_size_bytes,
    gpointer user_data
)
{
    TestEvidenceIntegrityTaskCancellationContext *context =
        user_data;

    BackgroundTask *task =
        NULL;

    assert(context != NULL);
    assert(total_size_bytes > 0);

    g_mutex_lock(
        &context->mutex
    );

    context->block_count++;

    if (context->block_count != 1)
    {
        g_mutex_unlock(
            &context->mutex
        );

        return;
    }

    while (!context->task_is_ready)
    {
        g_cond_wait(
            &context->condition,
            &context->mutex
        );
    }

    task =
        background_task_ref(
            context->task
        );

    g_mutex_unlock(
        &context->mutex
    );

    background_task_cancel(
        task
    );

    background_task_unref(
        task
    );
}

/**
 * @brief Vérifie l'annulation coopérative pendant le calcul.
 */
static void test_evidence_integrity_task_cancelled(void)
{
    TestEvidenceIntegrityTaskFixture fixture =
        test_evidence_integrity_task_fixture_create();

    TestEvidenceIntegrityTaskCompletionContext completion_context =
        test_evidence_integrity_task_completion_context_create();

    TestEvidenceIntegrityTaskCancellationContext cancellation_context =
        {0};

    EvidenceIntegrityTaskRequest request =
        {0};

    BackgroundTask *task =
        NULL;

    guint8 *file_data =
        NULL;

    char *file_path =
        NULL;

    char *relative_path =
        NULL;

    const gsize file_size =
        512U * 1024U;

    gsize data_index =
        0;

    GError *error =
        NULL;

    file_path =
        test_evidence_integrity_task_build_file_path(
            &fixture,
            "cancelled.bin"
        );

    relative_path =
        test_evidence_integrity_task_build_relative_path(
            "cancelled.bin"
        );

    assert(file_path != NULL);
    assert(relative_path != NULL);

    file_data =
        g_malloc(
            file_size
        );

    assert(file_data != NULL);

    for (data_index = 0;
         data_index < file_size;
         data_index++)
    {
        file_data[data_index] =
            (guint8) (data_index % 251U);
    }

    assert(
        g_file_set_contents(
            file_path,
            (const char *) file_data,
            (gssize) file_size,
            &error
        )
    );

    assert(error == NULL);

    g_mutex_init(
        &cancellation_context.mutex
    );

    g_cond_init(
        &cancellation_context.condition
    );

    file_hash_test_set_block_hook(
        test_evidence_integrity_task_cancel_after_first_block,
        &cancellation_context
    );

    request.investigation_root_path =
        fixture.root_directory;

    request.relative_path =
        relative_path;

    /*
     * La valeur exacte n'est pas importante : l'annulation intervient
     * avant la comparaison finale.
     */
    request.expected_sha256 =
        TEST_SHA256_ABC;

    task =
        test_evidence_integrity_task_start(
            &fixture,
            &request,
            &completion_context
        );

    g_mutex_lock(
        &cancellation_context.mutex
    );

    cancellation_context.task =
        task;

    cancellation_context.task_is_ready =
        TRUE;

    g_cond_signal(
        &cancellation_context.condition
    );

    g_mutex_unlock(
        &cancellation_context.mutex
    );

    test_evidence_integrity_task_wait(
        &completion_context
    );

    file_hash_test_set_block_hook(
        NULL,
        NULL
    );

    assert(
        background_task_get_state(
            task
        ) ==
        BACKGROUND_TASK_STATE_CANCELLED
    );

    assert(
        background_task_get_result(
            task
        ) == NULL
    );

    error =
        background_task_dup_error(
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

    g_clear_error(
        &error
    );

    assert(
        cancellation_context.block_count == 1
    );

    background_task_unref(
        task
    );

    g_cond_clear(
        &cancellation_context.condition
    );

    g_mutex_clear(
        &cancellation_context.mutex
    );

    assert(
        g_remove(
            file_path
        ) == 0
    );

    g_free(
        file_data
    );

    g_free(
        relative_path
    );

    g_free(
        file_path
    );

    test_evidence_integrity_task_completion_context_clear(
        &completion_context
    );

    test_evidence_integrity_task_fixture_clear(
        &fixture
    );
}

int main(void)
{
    test_evidence_integrity_task_invalid_arguments();
    test_evidence_integrity_task_valid();
    test_evidence_integrity_task_modified();
    test_evidence_integrity_task_missing();
    test_evidence_integrity_task_error_result();
    test_evidence_integrity_task_argument_ownership();
    test_evidence_integrity_task_cancelled();

    printf(
        "EvidenceIntegrityTask : tous les tests sont valides.\n"
    );

    return 0;
}
