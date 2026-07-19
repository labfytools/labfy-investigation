/******************************************************************************
 * @file test_evidence_import_task.c
 * @brief Tests de la tâche asynchrone d’import d’une preuve.
 ******************************************************************************/

#include "core/evidence_import_task.h"

#include "core/background_task.h"
#include "core/task_manager.h"
#include "dao/evidence_dao.h"
#include "database/database.h"
#include "models/evidence_record.h"
#include "core/evidence_importer_test.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

#define TEST_RELATIVE_DIRECTORY \
    "01_Preuves_Originales/Documents"

/**
 * @brief Contexte utilisé pour attendre la fin asynchrone.
 */
typedef struct
{
    GMainLoop *main_loop;

    gboolean completed;
    gboolean timed_out;
} TestCompletionContext;

/**
 * @brief Environnement temporaire d’un test.
 */
typedef struct
{
    char *root_directory;
    char *database_path;

    char *source_directory;
    char *evidence_directory;
    char *destination_directory;

    TaskManager *task_manager;
} TestEvidenceImportTaskFixture;

/**
 * @brief Quitte la boucle principale lorsque la tâche se termine.
 */
static void test_evidence_import_task_completed(
    BackgroundTask *task,
    gpointer user_data
)
{
    TestCompletionContext *context =
        user_data;

    assert(task != NULL);
    assert(context != NULL);

    context->completed = TRUE;

    g_main_loop_quit(
        context->main_loop
    );
}

/**
 * @brief Empêche un test asynchrone de rester bloqué.
 */
static gboolean test_evidence_import_task_timeout(
    gpointer user_data
)
{
    TestCompletionContext *context =
        user_data;

    assert(context != NULL);

    context->timed_out = TRUE;

    g_main_loop_quit(
        context->main_loop
    );

    return G_SOURCE_REMOVE;
}

/**
 * @brief Attend la fin d’une tâche avec un timeout.
 */
static void test_evidence_import_task_wait(
    TestCompletionContext *context
)
{
    guint timeout_source_id = 0;

    assert(context != NULL);
    assert(context->main_loop != NULL);

    timeout_source_id =
        g_timeout_add_seconds(
            5,
            test_evidence_import_task_timeout,
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
}

/**
 * @brief Compte les fichiers présents dans la destination.
 */
static guint test_evidence_import_task_count_destination_entries(
    const TestEvidenceImportTaskFixture *fixture
)
{
    GDir *directory = NULL;

    const char *entry_name = NULL;

    guint entry_count = 0;

    GError *error = NULL;

    assert(fixture != NULL);

    directory =
        g_dir_open(
            fixture->destination_directory,
            0,
            &error
        );

    assert(directory != NULL);
    assert(error == NULL);

    while ((entry_name = g_dir_read_name(directory)) != NULL)
    {
        assert(entry_name[0] != '\0');

        entry_count++;
    }

    g_dir_close(
        directory
    );

    return entry_count;
}

/**
 * @brief Crée une enquête temporaire.
 */
static TestEvidenceImportTaskFixture
test_evidence_import_task_fixture_create(void)
{
    TestEvidenceImportTaskFixture fixture =
        {0};

    GError *error = NULL;

    fixture.root_directory =
        g_dir_make_tmp(
            "labfy-evidence-import-task-test-XXXXXX",
            &error
        );

    assert(fixture.root_directory != NULL);
    assert(error == NULL);

    fixture.database_path =
        g_build_filename(
            fixture.root_directory,
            "Enquete.sqlite",
            NULL
        );

    fixture.source_directory =
        g_build_filename(
            fixture.root_directory,
            "sources",
            NULL
        );

    fixture.evidence_directory =
        g_build_filename(
            fixture.root_directory,
            "01_Preuves_Originales",
            NULL
        );

    fixture.destination_directory =
        g_build_filename(
            fixture.evidence_directory,
            "Documents",
            NULL
        );

    assert(fixture.database_path != NULL);
    assert(fixture.source_directory != NULL);
    assert(fixture.evidence_directory != NULL);
    assert(fixture.destination_directory != NULL);

    assert(
        g_mkdir(
            fixture.source_directory,
            0700
        ) == 0
    );

    assert(
        g_mkdir(
            fixture.evidence_directory,
            0700
        ) == 0
    );

    assert(
        g_mkdir(
            fixture.destination_directory,
            0700
        ) == 0
    );

    assert(
        database_initialize(
            fixture.database_path,
            "Enquete_EvidenceImportTask",
            fixture.root_directory
        )
    );

    fixture.task_manager =
        task_manager_new();

    assert(fixture.task_manager != NULL);

    return fixture;
}

/**
 * @brief Détruit une fixture déjà débarrassée de ses fichiers.
 */
static void test_evidence_import_task_fixture_clear(
    TestEvidenceImportTaskFixture *fixture
)
{
    assert(fixture != NULL);

    task_manager_free(
        fixture->task_manager
    );

    assert(
        g_remove(
            fixture->database_path
        ) == 0
    );

    assert(
        g_rmdir(
            fixture->destination_directory
        ) == 0
    );

    assert(
        g_rmdir(
            fixture->evidence_directory
        ) == 0
    );

    assert(
        g_rmdir(
            fixture->source_directory
        ) == 0
    );

    assert(
        g_rmdir(
            fixture->root_directory
        ) == 0
    );

    g_free(
        fixture->destination_directory
    );

    g_free(
        fixture->evidence_directory
    );

    g_free(
        fixture->source_directory
    );

    g_free(
        fixture->database_path
    );

    g_free(
        fixture->root_directory
    );
}

/**
 * @brief Vérifie le refus des paramètres invalides.
 */
static void test_evidence_import_task_invalid_arguments(void)
{
    EvidenceImportTaskRequest request =
        {0};

    BackgroundTask *task = NULL;

    GError *error = NULL;

    task =
        evidence_import_task_start(
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
        EVIDENCE_IMPORT_TASK_ERROR
    );

    assert(
        error->code ==
        EVIDENCE_IMPORT_TASK_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );
}

/**
 * @brief Vérifie un import asynchrone complet.
 */
static void test_evidence_import_task_success(void)
{
    TestEvidenceImportTaskFixture fixture =
        test_evidence_import_task_fixture_create();

    TestCompletionContext completion_context =
        {0};

    EvidenceImportTaskRequest request =
        {0};

    BackgroundTask *task = NULL;

    EvidenceRecord *evidence_record = NULL;

    Database *database = NULL;
    EvidenceDao *evidence_dao = NULL;

    char *source_path = NULL;
    char *destination_path = NULL;

    guint64 evidence_count = 0;

    GError *error = NULL;

    source_path =
        g_build_filename(
            fixture.source_directory,
            "preuve_asynchrone.txt",
            NULL
        );

    assert(source_path != NULL);

    assert(
        g_file_set_contents(
            source_path,
            "preuve asynchrone",
            -1,
            &error
        )
    );

    assert(error == NULL);

    completion_context.main_loop =
        g_main_loop_new(
            NULL,
            FALSE
        );

    assert(completion_context.main_loop != NULL);

    request.database_path =
        fixture.database_path;

    request.source_path =
        source_path;

    request.destination_directory =
        fixture.destination_directory;

    request.relative_directory =
        TEST_RELATIVE_DIRECTORY;

    request.type_identifier =
        "document";

    request.source =
        "Test automatisé EvidenceImportTask.";

    request.description =
        "Import réalisé depuis un thread secondaire.";

    assert(
        test_evidence_import_task_count_destination_entries(
            &fixture
        ) == 0
    );

    task =
        evidence_import_task_start(
            fixture.task_manager,
            &request,
            test_evidence_import_task_completed,
            &completion_context,
            NULL,
            &error
        );

    assert(task != NULL);
    assert(error == NULL);

    assert(
        task_manager_get_count(
            fixture.task_manager
        ) == 1
    );

    test_evidence_import_task_wait(
        &completion_context
    );

    assert(
        test_evidence_import_task_count_destination_entries(
            &fixture
        ) == 1
    );

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

    assert(
        background_task_dup_error(
            task
        ) == NULL
    );

    evidence_record =
        background_task_get_result(
            task
        );

    assert(evidence_record != NULL);

    assert(
        strcmp(
            evidence_record_get_original_name(
                evidence_record
            ),
            "preuve_asynchrone.txt"
        ) == 0
    );

    assert(
        evidence_record_get_integrity_status(
            evidence_record
        ) ==
        EVIDENCE_INTEGRITY_STATUS_VALID
    );

    destination_path =
        g_build_filename(
            fixture.destination_directory,
            evidence_record_get_internal_name(
                evidence_record
            ),
            NULL
        );

    assert(destination_path != NULL);

    assert(
        g_file_test(
            destination_path,
            G_FILE_TEST_IS_REGULAR
        )
    );

    /*
     * Le worker a fermé sa connexion.
     * Une nouvelle connexion permet de vérifier la persistance.
     */
    database =
        database_open(
            fixture.database_path
        );

    assert(database != NULL);

    evidence_dao =
        evidence_dao_new(
            database,
            &error
        );

    assert(evidence_dao != NULL);
    assert(error == NULL);

    assert(
        evidence_dao_count(
            evidence_dao,
            &evidence_count,
            &error
        )
    );

    assert(error == NULL);
    assert(evidence_count == 1);

    evidence_dao_free(
        evidence_dao
    );

    database_close(
        database
    );

    assert(
        g_remove(
            destination_path
        ) == 0
    );

    assert(
        g_remove(
            source_path
        ) == 0
    );

    g_free(
        destination_path
    );

    g_free(
        source_path
    );

    g_main_loop_unref(
        completion_context.main_loop
    );

    /*
     * La tâche reste encore possédée par le manager.
     */
    background_task_unref(
        task
    );

    test_evidence_import_task_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie l’annulation après la copie de la preuve.
 */
static void test_evidence_import_task_cancelled_after_copy(void)
{
    TestEvidenceImportTaskFixture fixture =
        test_evidence_import_task_fixture_create();

    TestCompletionContext completion_context =
        {0};

    EvidenceImportTaskRequest request =
        {0};

    BackgroundTask *task = NULL;

    char *source_path = NULL;

    guint64 evidence_count = 42;

    Database *database = NULL;
    EvidenceDao *evidence_dao = NULL;

    GError *error = NULL;

    source_path =
        g_build_filename(
            fixture.source_directory,
            "preuve_annulee.txt",
            NULL
        );

    assert(source_path != NULL);

    assert(
        g_file_set_contents(
            source_path,
            "preuve annulée après copie",
            -1,
            &error
        )
    );

    assert(error == NULL);

    completion_context.main_loop =
        g_main_loop_new(
            NULL,
            FALSE
        );

    assert(completion_context.main_loop != NULL);

    request.database_path =
        fixture.database_path;

    request.source_path =
        source_path;

    request.destination_directory =
        fixture.destination_directory;

    request.relative_directory =
        TEST_RELATIVE_DIRECTORY;

    request.type_identifier =
        "document";

    evidence_importer_test_reset_hooks();

    evidence_importer_test_set_cancel_after_copy(
        TRUE
    );

    task =
        evidence_import_task_start(
            fixture.task_manager,
            &request,
            test_evidence_import_task_completed,
            &completion_context,
            NULL,
            &error
        );

    assert(task != NULL);
    assert(error == NULL);

    test_evidence_import_task_wait(
        &completion_context
    );

    assert(
        background_task_get_state(
            task
        ) ==
        BACKGROUND_TASK_STATE_CANCELLED
    );

    assert(
        test_evidence_import_task_count_destination_entries(
            &fixture
        ) == 0
    );

    database =
        database_open(
            fixture.database_path
        );

    assert(database != NULL);

    evidence_dao =
        evidence_dao_new(
            database,
            &error
        );

    assert(evidence_dao != NULL);
    assert(error == NULL);

    assert(
        evidence_dao_count(
            evidence_dao,
            &evidence_count,
            &error
        )
    );

    assert(error == NULL);
    assert(evidence_count == 0);

    evidence_dao_free(
        evidence_dao
    );

    database_close(
        database
    );

    /*
     * Le fichier original reste intact.
     */
    assert(
        g_file_test(
            source_path,
            G_FILE_TEST_IS_REGULAR
        )
    );

    evidence_importer_test_reset_hooks();

    background_task_unref(
        task
    );

    g_main_loop_unref(
        completion_context.main_loop
    );

    assert(
        g_remove(
            source_path
        ) == 0
    );

    g_free(
        source_path
    );

    test_evidence_import_task_fixture_clear(
        &fixture
    );
}

int main(void)
{
    test_evidence_import_task_invalid_arguments();
    test_evidence_import_task_success();
    test_evidence_import_task_cancelled_after_copy();

    printf(
        "EvidenceImportTask : premiers tests valides.\n"
    );

    return 0;
}
