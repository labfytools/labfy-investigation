/******************************************************************************
 * @file evidence_import_task.c
 * @brief Exécution asynchrone de l’import d’une preuve.
 ******************************************************************************/

#include "core/evidence_import_task.h"

#include "core/evidence_importer.h"
#include "database/database.h"
#include "models/evidence_record.h"

#include <gio/gio.h>
#include <glib.h>

/**
 * @brief Copie privée des paramètres nécessaires au worker.
 */
typedef struct
{
    char *database_path;

    char *source_path;
    char *destination_directory;
    char *relative_directory;

    char *type_identifier;
    char *collected_at;
    char *source;
    char *description;
} EvidenceImportTaskData;

/**
 * @brief Libère les données privées du worker.
 */
static void evidence_import_task_data_free(
    gpointer user_data
)
{
    EvidenceImportTaskData *task_data =
        user_data;

    if (task_data == NULL)
    {
        return;
    }

    g_free(
        task_data->description
    );

    g_free(
        task_data->source
    );

    g_free(
        task_data->collected_at
    );

    g_free(
        task_data->type_identifier
    );

    g_free(
        task_data->relative_directory
    );

    g_free(
        task_data->destination_directory
    );

    g_free(
        task_data->source_path
    );

    g_free(
        task_data->database_path
    );

    g_free(
        task_data
    );
}

/**
 * @brief Vérifie les paramètres obligatoires.
 */
static gboolean evidence_import_task_request_is_valid(
    const EvidenceImportTaskRequest *request
)
{
    if (request == NULL)
    {
        return FALSE;
    }

    if (request->database_path == NULL ||
        request->database_path[0] == '\0')
    {
        return FALSE;
    }

    if (request->source_path == NULL ||
        request->source_path[0] == '\0')
    {
        return FALSE;
    }

    if (request->destination_directory == NULL ||
        request->destination_directory[0] == '\0')
    {
        return FALSE;
    }

    if (request->relative_directory == NULL ||
        request->relative_directory[0] == '\0')
    {
        return FALSE;
    }

    if (request->type_identifier == NULL ||
        request->type_identifier[0] == '\0')
    {
        return FALSE;
    }

    return TRUE;
}

/**
 * @brief Copie les paramètres avant le lancement du thread.
 */
static EvidenceImportTaskData *
evidence_import_task_data_new(
    const EvidenceImportTaskRequest *request
)
{
    EvidenceImportTaskData *task_data =
        NULL;

    if (!evidence_import_task_request_is_valid(
            request
        ))
    {
        return NULL;
    }

    task_data =
        g_try_new0(
            EvidenceImportTaskData,
            1
        );

    if (task_data == NULL)
    {
        return NULL;
    }

    task_data->database_path =
        g_strdup(
            request->database_path
        );

    task_data->source_path =
        g_strdup(
            request->source_path
        );

    task_data->destination_directory =
        g_strdup(
            request->destination_directory
        );

    task_data->relative_directory =
        g_strdup(
            request->relative_directory
        );

    task_data->type_identifier =
        g_strdup(
            request->type_identifier
        );

    task_data->collected_at =
        g_strdup(
            request->collected_at
        );

    task_data->source =
        g_strdup(
            request->source
        );

    task_data->description =
        g_strdup(
            request->description
        );

    return task_data;
}

GQuark evidence_import_task_error_quark(void)
{
    return g_quark_from_static_string(
        "evidence-import-task-error-quark"
    );
}

/**
 * @brief Traduit l’annulation métier en annulation GIO.
 *
 * BackgroundTask reconnaît une annulation uniquement avec :
 *
 * G_IO_ERROR / G_IO_ERROR_CANCELLED.
 */
static void evidence_import_task_propagate_import_error(
    GError **error,
    GError *import_error
)
{
    if (import_error != NULL &&
        g_error_matches(
            import_error,
            EVIDENCE_IMPORTER_ERROR,
            EVIDENCE_IMPORTER_ERROR_CANCELLED
        ))
    {
        g_clear_error(
            &import_error
        );

        g_set_error_literal(
            error,
            G_IO_ERROR,
            G_IO_ERROR_CANCELLED,
            "L’import de la preuve a été annulé."
        );

        return;
    }

    if (import_error != NULL)
    {
        g_propagate_error(
            error,
            import_error
        );

        return;
    }

    g_set_error_literal(
        error,
        EVIDENCE_IMPORT_TASK_ERROR,
        EVIDENCE_IMPORT_TASK_ERROR_IMPORT,
        "L’import a échoué sans fournir d’erreur."
    );
}

/**
 * @brief Exécute l’import dans le thread secondaire.
 */
static gboolean evidence_import_task_worker(
    BackgroundTask *task,
    GCancellable *cancellable,
    gpointer worker_data,
    gpointer *result,
    GError **error
)
{
    EvidenceImportTaskData *task_data =
        worker_data;

    EvidenceImportRequest import_request =
        {0};

    Database *database = NULL;

    EvidenceImporter *evidence_importer =
        NULL;

    EvidenceRecord *evidence_record =
        NULL;

    GError *importer_error = NULL;
    GError *import_error = NULL;

    if (task == NULL ||
        cancellable == NULL ||
        task_data == NULL ||
        result == NULL ||
        error == NULL)
    {
        return FALSE;
    }

    *result = NULL;

    if (g_cancellable_set_error_if_cancelled(
            cancellable,
            error
        ))
    {
        return FALSE;
    }

    background_task_report_progress(
        task,
        0.05,
        "Préparation de l’import"
    );

    database =
        database_open(
            task_data->database_path
        );

    if (database == NULL)
    {
        g_set_error(
            error,
            EVIDENCE_IMPORT_TASK_ERROR,
            EVIDENCE_IMPORT_TASK_ERROR_DATABASE,
            "Impossible d’ouvrir la base SQLite : %s",
            task_data->database_path
        );

        return FALSE;
    }

    if (g_cancellable_set_error_if_cancelled(
            cancellable,
            error
        ))
    {
        database_close(
            database
        );

        return FALSE;
    }

    evidence_importer =
        evidence_importer_new(
            database,
            &importer_error
        );

    if (evidence_importer == NULL)
    {
        if (importer_error != NULL)
        {
            g_propagate_prefixed_error(
                error,
                importer_error,
                "Impossible de préparer l’import : "
            );
        }
        else
        {
            g_set_error_literal(
                error,
                EVIDENCE_IMPORT_TASK_ERROR,
                EVIDENCE_IMPORT_TASK_ERROR_IMPORT,
                "Impossible de créer le service d’import."
            );
        }

        database_close(
            database
        );

        return FALSE;
    }

    import_request.source_path =
        task_data->source_path;

    import_request.destination_directory =
        task_data->destination_directory;

    import_request.relative_directory =
        task_data->relative_directory;

    import_request.type_identifier =
        task_data->type_identifier;

    import_request.collected_at =
        task_data->collected_at;

    import_request.source =
        task_data->source;

    import_request.description =
        task_data->description;

    background_task_report_progress(
        task,
        0.20,
        "Copie et vérification de la preuve"
    );

    evidence_record =
        evidence_importer_import(
            evidence_importer,
            &import_request,
            cancellable,
            &import_error
        );

    if (evidence_record == NULL)
    {
        evidence_import_task_propagate_import_error(
            error,
            import_error
        );

        evidence_importer_free(
            evidence_importer
        );

        database_close(
            database
        );

        return FALSE;
    }

    background_task_report_progress(
        task,
        1.0,
        "Preuve importée"
    );

    evidence_importer_free(
        evidence_importer
    );

    database_close(
        database
    );

    *result =
        evidence_record;

    return TRUE;
}

/**
 * @brief Transmet une erreur secondaire au code appelant.
 */
static void evidence_import_task_propagate_start_error(
    GError **error,
    GError *start_error,
    const char *prefix
)
{
    if (start_error == NULL)
    {
        g_set_error_literal(
            error,
            EVIDENCE_IMPORT_TASK_ERROR,
            EVIDENCE_IMPORT_TASK_ERROR_IMPORT,
            "La tâche d’import n’a pas pu être démarrée."
        );

        return;
    }

    if (error != NULL)
    {
        g_propagate_prefixed_error(
            error,
            start_error,
            "%s",
            prefix
        );

        return;
    }

    g_clear_error(
        &start_error
    );
}

/**
 * @brief Libère le résultat EvidenceRecord d’une tâche.
 *
 * Cette fonction adapte evidence_record_free() au type GDestroyNotify.
 *
 * @param user_data Pointeur vers EvidenceRecord.
 */
static void evidence_import_task_result_free(
    gpointer user_data
)
{
    EvidenceRecord *evidence_record =
        user_data;

    evidence_record_free(
        evidence_record
    );
}

BackgroundTask *evidence_import_task_start(
    TaskManager *task_manager,
    const EvidenceImportTaskRequest *request,
    BackgroundTaskCompletionCallback completion_callback,
    gpointer completion_data,
    GDestroyNotify completion_data_destroy,
    GError **error
)
{
    EvidenceImportTaskData *task_data =
        NULL;

    BackgroundTask *task = NULL;

    char *source_name = NULL;
    char *task_title = NULL;

    GError *start_error = NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (task_manager == NULL ||
        !evidence_import_task_request_is_valid(
            request
        ))
    {
        g_set_error_literal(
            error,
            EVIDENCE_IMPORT_TASK_ERROR,
            EVIDENCE_IMPORT_TASK_ERROR_INVALID_ARGUMENT,
            "Les paramètres de la tâche d’import sont invalides."
        );

        return NULL;
    }

    task_data =
        evidence_import_task_data_new(
            request
        );

    if (task_data == NULL)
    {
        g_set_error_literal(
            error,
            EVIDENCE_IMPORT_TASK_ERROR,
            EVIDENCE_IMPORT_TASK_ERROR_IMPORT,
            "Impossible d’allouer les données de la tâche d’import."
        );

        return NULL;
    }

    source_name =
        g_path_get_basename(
            request->source_path
        );

    task_title =
        g_strdup_printf(
            "Importer la preuve : %s",
            source_name
        );

    task =
        background_task_new(
            task_title
        );

    g_free(
        task_title
    );

    g_free(
        source_name
    );

    if (task == NULL)
    {
        evidence_import_task_data_free(
            task_data
        );

        g_set_error_literal(
            error,
            EVIDENCE_IMPORT_TASK_ERROR,
            EVIDENCE_IMPORT_TASK_ERROR_IMPORT,
            "Impossible de créer la tâche d’import."
        );

        return NULL;
    }

    if (!task_manager_add(
            task_manager,
            task,
            &start_error
        ))
    {
        evidence_import_task_data_free(
            task_data
        );

        background_task_unref(
            task
        );

        evidence_import_task_propagate_start_error(
            error,
            start_error,
            "Impossible d’ajouter la tâche : "
        );

        return NULL;
    }

    if (!background_task_start(
            task,
            evidence_import_task_worker,
            task_data,
            evidence_import_task_data_free,
            evidence_import_task_result_free,
            completion_callback,
            completion_data,
            completion_data_destroy,
            &start_error
        ))
    {
        /*
         * background_task_start() n’a pas pris possession
         * de task_data ni de completion_data en cas d’échec.
         */
        evidence_import_task_data_free(
            task_data
        );

        task_manager_remove(
            task_manager,
            task
        );

        background_task_unref(
            task
        );

        evidence_import_task_propagate_start_error(
            error,
            start_error,
            "Impossible de démarrer la tâche : "
        );

        return NULL;
    }

    return task;
}
