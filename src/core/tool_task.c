/******************************************************************************
 * @file tool_task.c
 * @brief Exécution d'un outil externe dans une tâche d'arrière-plan.
 ******************************************************************************/

#include "core/tool_task.h"

/**
 * @struct ToolTaskWorkerData
 * @brief Données privées transmises au worker.
 */
typedef struct
{
    char *tool_identifier;
    char *tool_display_name;
    char *executable_path;

    char **arguments;
    gsize argument_count;

    char *working_directory;
} ToolTaskWorkerData;

/**
 * @struct ToolTask
 * @brief Exécution externe préparée.
 */
struct ToolTask
{
    BackgroundTask *background_task;
    ToolTaskWorkerData *worker_data;

    gboolean started;
};

/**
 * @struct ToolTaskResult
 * @brief Résultat d'une exécution externe en arrière-plan.
 */
struct ToolTaskResult
{
    char *tool_identifier;
    char *executable_path;

    char **arguments;
    gsize argument_count;

    char *working_directory;

    ToolProcessResult *process_result;
};

/**
 * @brief Vérifie qu'une chaîne est définie et non vide.
 *
 * @param text Chaîne à vérifier.
 *
 * @return TRUE si la chaîne est valide.
 */
static gboolean tool_task_string_is_valid(
    const char *text
)
{
    return text != NULL &&
           text[0] != '\0';
}

/**
 * @brief Duplique un tableau d'arguments terminé par NULL.
 *
 * Un tableau vide terminé par NULL est créé lorsque arguments vaut NULL.
 *
 * @param arguments Arguments à copier, ou NULL.
 * @param out_argument_count Emplacement recevant le nombre d'arguments.
 *
 * @return Nouveau tableau terminé par NULL.
 */
static char **tool_task_duplicate_arguments(
    const char *const arguments[],
    gsize *out_argument_count
)
{
    char **arguments_copy = NULL;

    gsize argument_count = 0;
    gsize argument_index = 0;

    if (out_argument_count != NULL)
    {
        *out_argument_count = 0;
    }

    if (arguments != NULL)
    {
        while (arguments[argument_count] != NULL)
        {
            argument_count++;
        }
    }

    arguments_copy = g_new0(
        char *,
        argument_count + 1
    );

    for (argument_index = 0;
         argument_index < argument_count;
         argument_index++)
    {
        arguments_copy[argument_index] =
            g_strdup(
                arguments[argument_index]
            );
    }

    arguments_copy[argument_count] = NULL;

    if (out_argument_count != NULL)
    {
        *out_argument_count =
            argument_count;
    }

    return arguments_copy;
}

/**
 * @brief Libère les données privées du worker.
 *
 * @param user_data Pointeur vers ToolTaskWorkerData.
 */
static void tool_task_worker_data_free(
    gpointer user_data
)
{
    ToolTaskWorkerData *worker_data =
        user_data;

    if (worker_data == NULL)
    {
        return;
    }

    g_clear_pointer(
        &worker_data->tool_identifier,
        g_free
    );

    g_clear_pointer(
        &worker_data->tool_display_name,
        g_free
    );

    g_clear_pointer(
        &worker_data->executable_path,
        g_free
    );

    g_clear_pointer(
        &worker_data->arguments,
        g_strfreev
    );

    g_clear_pointer(
        &worker_data->working_directory,
        g_free
    );

    g_free(
        worker_data
    );
}

/**
 * @brief Adapte tool_task_result_free() à GDestroyNotify.
 *
 * @param user_data Pointeur vers ToolTaskResult.
 */
static void tool_task_result_destroy(
    gpointer user_data
)
{
    tool_task_result_free(
        user_data
    );
}

/**
 * @brief Produit une erreur ToolTask avec une cause facultative.
 *
 * @param error Emplacement facultatif pour l'erreur.
 * @param error_code Code ToolTask.
 * @param context_message Description de l'échec.
 * @param cause Erreur d'origine facultative.
 */
static void tool_task_set_wrapped_error(
    GError **error,
    ToolTaskError error_code,
    const char *context_message,
    const GError *cause
)
{
    const char *cause_message = NULL;

    if (cause == NULL ||
        cause->message == NULL)
    {
        g_set_error_literal(
            error,
            TOOL_TASK_ERROR,
            error_code,
            context_message
        );

        return;
    }

    cause_message = cause->message;

    g_set_error(
        error,
        TOOL_TASK_ERROR,
        error_code,
        "%s : %s",
        context_message,
        cause_message
    );
}

/**
 * @brief Crée le résultat ToolTask à partir des données du worker.
 *
 * La fonction devient propriétaire de process_result.
 *
 * @param worker_data Données de l'exécution.
 * @param process_result Résultat brut du processus.
 *
 * @return Nouveau résultat.
 */
static ToolTaskResult *tool_task_result_new(
    const ToolTaskWorkerData *worker_data,
    ToolProcessResult *process_result
)
{
    ToolTaskResult *task_result = NULL;

    if (worker_data == NULL ||
        process_result == NULL)
    {
        return NULL;
    }

    task_result = g_new0(
        ToolTaskResult,
        1
    );

    task_result->tool_identifier =
        g_strdup(
            worker_data->tool_identifier
        );

    task_result->executable_path =
        g_strdup(
            worker_data->executable_path
        );

    task_result->arguments =
        tool_task_duplicate_arguments(
            (const char *const *)
                worker_data->arguments,
            &task_result->argument_count
        );

    task_result->working_directory =
        g_strdup(
            worker_data->working_directory
        );

    task_result->process_result =
        process_result;

    return task_result;
}

/**
 * @brief Exécute l'outil externe dans le thread secondaire.
 *
 * @param background_task Tâche en cours.
 * @param cancellable Objet d'annulation.
 * @param worker_data_pointer Données privées du worker.
 * @param result Emplacement recevant ToolTaskResult.
 * @param error Emplacement recevant une erreur.
 *
 * @return TRUE lorsqu'un résultat exploitable a été produit.
 */
static gboolean tool_task_worker(
    BackgroundTask *background_task,
    GCancellable *cancellable,
    gpointer worker_data_pointer,
    gpointer *result,
    GError **error
)
{
    ToolTaskWorkerData *worker_data =
        worker_data_pointer;

    ToolProcessResult *process_result = NULL;
    ToolTaskResult *task_result = NULL;

    GError *process_error = NULL;

    char *execution_status = NULL;
    char *final_status = NULL;

    gboolean process_success = FALSE;

    if (background_task == NULL ||
        cancellable == NULL ||
        worker_data == NULL ||
        result == NULL ||
        error == NULL)
    {
        g_set_error_literal(
            error,
            TOOL_TASK_ERROR,
            TOOL_TASK_ERROR_INVALID_ARGUMENT,
            "Les arguments du worker ToolTask sont invalides."
        );

        return FALSE;
    }

    *result = NULL;

    background_task_report_progress(
        background_task,
        0.0,
        "Préparation de l'exécution"
    );

    /*
     * Une annulation demandée avant le lancement doit être transmise
     * sous la forme attendue par BackgroundTask.
     */
    if (g_cancellable_set_error_if_cancelled(
            cancellable,
            error
        ))
    {
        return FALSE;
    }

    execution_status = g_strdup_printf(
        "Exécution de %s",
        tool_task_string_is_valid(
            worker_data->tool_display_name
        )
            ? worker_data->tool_display_name
            : worker_data->tool_identifier
    );

    background_task_report_progress(
        background_task,
        0.1,
        execution_status
    );

    g_free(
        execution_status
    );

    execution_status = NULL;

    process_success = tool_process_run(
        worker_data->executable_path,
        (const char *const *)
            worker_data->arguments,
        worker_data->working_directory,
        cancellable,
        &process_result,
        &process_error
    );

    if (!process_success)
    {
        /*
         * BackgroundTask reconnaît une annulation lorsque le domaine
         * d'erreur est G_IO_ERROR et le code G_IO_ERROR_CANCELLED.
         */
        if (process_error != NULL &&
            g_error_matches(
                process_error,
                TOOL_PROCESS_ERROR,
                TOOL_PROCESS_ERROR_CANCELLED
            ))
        {
            g_set_error(
                error,
                G_IO_ERROR,
                G_IO_ERROR_CANCELLED,
                "%s",
                process_error->message
            );
        }
        else
        {
            tool_task_set_wrapped_error(
                error,
                TOOL_TASK_ERROR_PROCESS,
                "L'exécution de l'outil externe a échoué",
                process_error
            );
        }

        g_clear_error(
            &process_error
        );

        return FALSE;
    }

    if (process_result == NULL)
    {
        g_set_error_literal(
            error,
            TOOL_TASK_ERROR,
            TOOL_TASK_ERROR_PROCESS,
            "ToolProcess n'a produit aucun résultat exploitable."
        );

        return FALSE;
    }

    background_task_report_progress(
        background_task,
        0.9,
        "Traitement du résultat"
    );

    task_result = tool_task_result_new(
        worker_data,
        process_result
    );

    if (task_result == NULL)
    {
        tool_process_result_free(
            process_result
        );

        g_set_error_literal(
            error,
            TOOL_TASK_ERROR,
            TOOL_TASK_ERROR_PROCESS,
            "Le résultat de l'outil n'a pas pu être construit."
        );

        return FALSE;
    }

    /*
     * ToolTaskResult possède maintenant ToolProcessResult.
     */
    process_result = NULL;

    if (tool_process_result_is_success(
            task_result->process_result
        ))
    {
        final_status = g_strdup(
            "Exécution terminée avec succès"
        );
    }
    else if (tool_process_result_exited_normally(
                 task_result->process_result
             ))
    {
        final_status = g_strdup_printf(
            "Exécution terminée avec le code %d",
            tool_process_result_get_exit_status(
                task_result->process_result
            )
        );
    }
    else if (tool_process_result_was_signaled(
                 task_result->process_result
             ))
    {
        final_status = g_strdup_printf(
            "Exécution terminée par le signal %d",
            tool_process_result_get_termination_signal(
                task_result->process_result
            )
        );
    }
    else
    {
        final_status = g_strdup(
            "Exécution terminée"
        );
    }

    background_task_report_progress(
        background_task,
        1.0,
        final_status
    );

    g_free(
        final_status
    );

    *result = task_result;

    return TRUE;
}

GQuark tool_task_error_quark(void)
{
    return g_quark_from_static_string(
        "labfy-investigation-tool-task-error"
    );
}

ToolTask *tool_task_new(
    const ToolRegistry *tool_registry,
    const char *tool_identifier,
    const char *task_title,
    const char *const arguments[],
    const char *working_directory,
    GError **error
)
{
    ToolTask *tool_task = NULL;
    ToolTaskWorkerData *worker_data = NULL;

    const ToolInfo *tool_info = NULL;

    const char *tool_display_name = NULL;
    const char *executable_path = NULL;

    ToolAvailability availability;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (tool_registry == NULL ||
        !tool_task_string_is_valid(
            tool_identifier
        ) ||
        !tool_task_string_is_valid(
            task_title
        ) ||
        (working_directory != NULL &&
         working_directory[0] == '\0'))
    {
        g_set_error_literal(
            error,
            TOOL_TASK_ERROR,
            TOOL_TASK_ERROR_INVALID_ARGUMENT,
            "Les arguments fournis à ToolTask sont invalides."
        );

        return NULL;
    }

    tool_info = tool_registry_find(
        tool_registry,
        tool_identifier
    );

    if (tool_info == NULL)
    {
        g_set_error(
            error,
            TOOL_TASK_ERROR,
            TOOL_TASK_ERROR_TOOL_NOT_FOUND,
            "Aucun outil portant l'identifiant '%s' n'est enregistré.",
            tool_identifier
        );

        return NULL;
    }

    availability = tool_info_get_availability(
        tool_info
    );

    if (availability ==
        TOOL_AVAILABILITY_UNKNOWN)
    {
        g_set_error(
            error,
            TOOL_TASK_ERROR,
            TOOL_TASK_ERROR_TOOL_NOT_CHECKED,
            "La disponibilité de l'outil '%s' n'a pas été vérifiée.",
            tool_identifier
        );

        return NULL;
    }

    if (availability ==
        TOOL_AVAILABILITY_MISSING)
    {
        g_set_error(
            error,
            TOOL_TASK_ERROR,
            TOOL_TASK_ERROR_TOOL_MISSING,
            "L'outil '%s' est absent de la machine.",
            tool_identifier
        );

        return NULL;
    }

    executable_path =
        tool_info_get_resolved_path(
            tool_info
        );

    if (!tool_task_string_is_valid(
            executable_path
        ))
    {
        g_set_error(
            error,
            TOOL_TASK_ERROR,
            TOOL_TASK_ERROR_INVALID_TOOL_STATE,
            "L'outil '%s' est disponible mais ne possède aucun chemin.",
            tool_identifier
        );

        return NULL;
    }

    tool_display_name =
        tool_info_get_display_name(
            tool_info
        );

    worker_data = g_new0(
        ToolTaskWorkerData,
        1
    );

    worker_data->tool_identifier =
        g_strdup(
            tool_identifier
        );

    worker_data->tool_display_name =
        g_strdup(
            tool_display_name
        );

    worker_data->executable_path =
        g_strdup(
            executable_path
        );

    worker_data->arguments =
        tool_task_duplicate_arguments(
            arguments,
            &worker_data->argument_count
        );

    worker_data->working_directory =
        g_strdup(
            working_directory
        );

    tool_task = g_new0(
        ToolTask,
        1
    );

    tool_task->background_task =
        background_task_new(
            task_title
        );

    if (tool_task->background_task == NULL)
    {
        tool_task_worker_data_free(
            worker_data
        );

        g_free(
            tool_task
        );

        g_set_error_literal(
            error,
            TOOL_TASK_ERROR,
            TOOL_TASK_ERROR_TASK_CREATION,
            "La tâche d'arrière-plan n'a pas pu être créée."
        );

        return NULL;
    }

    tool_task->worker_data =
        worker_data;

    tool_task->started =
        FALSE;

    return tool_task;
}

void tool_task_free(
    ToolTask *tool_task
)
{
    if (tool_task == NULL)
    {
        return;
    }

    /*
     * Avant le démarrage, ToolTask possède les données du worker.
     *
     * Après un démarrage réussi, le pointeur vaut NULL car la propriété
     * a été transférée à BackgroundTask.
     */
    if (tool_task->worker_data != NULL)
    {
        tool_task_worker_data_free(
            tool_task->worker_data
        );

        tool_task->worker_data = NULL;
    }

    background_task_unref(
        tool_task->background_task
    );

    tool_task->background_task = NULL;

    g_free(
        tool_task
    );
}

BackgroundTask *tool_task_get_background_task(
    const ToolTask *tool_task
)
{
    if (tool_task == NULL)
    {
        return NULL;
    }

    return tool_task->background_task;
}

gboolean tool_task_start(
    ToolTask *tool_task,
    BackgroundTaskCompletionCallback completion_callback,
    gpointer completion_data,
    GDestroyNotify completion_data_destroy,
    GError **error
)
{
    GError *start_error = NULL;
    gboolean start_success = FALSE;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (tool_task == NULL ||
        tool_task->background_task == NULL ||
        tool_task->worker_data == NULL)
    {
        g_set_error_literal(
            error,
            TOOL_TASK_ERROR,
            TOOL_TASK_ERROR_INVALID_ARGUMENT,
            "La ToolTask fournie est invalide."
        );

        return FALSE;
    }

    if (tool_task->started ||
        background_task_get_state(
            tool_task->background_task
        ) != BACKGROUND_TASK_STATE_PENDING)
    {
        g_set_error_literal(
            error,
            TOOL_TASK_ERROR,
            TOOL_TASK_ERROR_TASK_START,
            "La ToolTask a déjà été démarrée."
        );

        return FALSE;
    }

    start_success = background_task_start(
        tool_task->background_task,
        tool_task_worker,
        tool_task->worker_data,
        tool_task_worker_data_free,
        tool_task_result_destroy,
        completion_callback,
        completion_data,
        completion_data_destroy,
        &start_error
    );

    if (!start_success)
    {
        tool_task_set_wrapped_error(
            error,
            TOOL_TASK_ERROR_TASK_START,
            "La tâche d'arrière-plan n'a pas pu être démarrée",
            start_error
        );

        g_clear_error(
            &start_error
        );

        /*
         * BackgroundTask n'a pas pris possession des données.
         */
        return FALSE;
    }

    /*
     * La propriété des données du worker appartient désormais à
     * BackgroundTask.
     */
    tool_task->worker_data = NULL;
    tool_task->started = TRUE;

    return TRUE;
}

const ToolTaskResult *tool_task_result_from_background_task(
    const BackgroundTask *background_task
)
{
    if (background_task == NULL ||
        background_task_get_state(
            background_task
        ) != BACKGROUND_TASK_STATE_COMPLETED)
    {
        return NULL;
    }

    /*
     * Précondition publique :
     * background_task doit provenir de ToolTask.
     */
    return background_task_get_result(
        background_task
    );
}

void tool_task_result_free(
    ToolTaskResult *result
)
{
    if (result == NULL)
    {
        return;
    }

    g_clear_pointer(
        &result->tool_identifier,
        g_free
    );

    g_clear_pointer(
        &result->executable_path,
        g_free
    );

    g_clear_pointer(
        &result->arguments,
        g_strfreev
    );

    g_clear_pointer(
        &result->working_directory,
        g_free
    );

    g_clear_pointer(
        &result->process_result,
        tool_process_result_free
    );

    g_free(
        result
    );
}

const char *tool_task_result_get_tool_identifier(
    const ToolTaskResult *result
)
{
    if (result == NULL)
    {
        return NULL;
    }

    return result->tool_identifier;
}

const char *tool_task_result_get_executable_path(
    const ToolTaskResult *result
)
{
    if (result == NULL)
    {
        return NULL;
    }

    return result->executable_path;
}

gsize tool_task_result_get_argument_count(
    const ToolTaskResult *result
)
{
    if (result == NULL)
    {
        return 0;
    }

    return result->argument_count;
}

const char *tool_task_result_get_argument(
    const ToolTaskResult *result,
    gsize index
)
{
    if (result == NULL ||
        result->arguments == NULL ||
        index >= result->argument_count)
    {
        return NULL;
    }

    return result->arguments[index];
}

const char *tool_task_result_get_working_directory(
    const ToolTaskResult *result
)
{
    if (result == NULL)
    {
        return NULL;
    }

    return result->working_directory;
}

const ToolProcessResult *tool_task_result_get_process_result(
    const ToolTaskResult *result
)
{
    if (result == NULL)
    {
        return NULL;
    }

    return result->process_result;
}
