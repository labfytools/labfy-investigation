/******************************************************************************
 * @file tool_process.c
 * @brief Exécution sécurisée des outils externes.
 ******************************************************************************/

#include "core/tool_process.h"

/**
 * @struct ToolProcessResult
 * @brief Résultat interne d’une exécution externe.
 */
struct ToolProcessResult
{
    GBytes *stdout_bytes;
    GBytes *stderr_bytes;

    gboolean exited_normally;
    int exit_status;

    gboolean was_signaled;
    int termination_signal;
};

/**
 * @brief Vérifie qu’une chaîne est définie et non vide.
 *
 * @param text Chaîne à vérifier.
 *
 * @return TRUE si la chaîne est valide.
 */
static gboolean tool_process_string_is_valid(
    const char *text
)
{
    return text != NULL &&
           text[0] != '\0';
}

/**
 * @brief Construit le vecteur argv transmis à GSubprocess.
 *
 * Les chaînes ne sont pas dupliquées. Le tableau les emprunte uniquement
 * pendant la création du processus.
 *
 * @param executable_path Chemin de l’exécutable.
 * @param arguments Arguments supplémentaires, ou NULL.
 *
 * @return Nouveau GPtrArray terminé par NULL.
 */
static GPtrArray *tool_process_build_argv(
    const char *executable_path,
    const char *const arguments[]
)
{
    GPtrArray *argument_vector = NULL;
    gsize argument_index = 0;

    argument_vector = g_ptr_array_new();

    g_ptr_array_add(
        argument_vector,
        (gpointer) executable_path
    );

    if (arguments != NULL)
    {
        for (argument_index = 0;
             arguments[argument_index] != NULL;
             argument_index++)
        {
            g_ptr_array_add(
                argument_vector,
                (gpointer) arguments[argument_index]
            );
        }
    }

    /*
     * argv doit obligatoirement se terminer par NULL.
     */
    g_ptr_array_add(
        argument_vector,
        NULL
    );

    return argument_vector;
}

/**
 * @brief Produit une erreur ToolProcess à partir d’une erreur interne.
 *
 * @param error Emplacement facultatif de l’erreur.
 * @param error_code Code ToolProcess.
 * @param context_message Description de l’opération ayant échoué.
 * @param cause Erreur interne facultative.
 */
static void tool_process_set_wrapped_error(
    GError **error,
    ToolProcessError error_code,
    const char *context_message,
    const GError *cause
)
{
    const char *cause_message = NULL;

    cause_message =
        cause != NULL &&
        cause->message != NULL
            ? cause->message
            : "erreur inconnue";

    g_set_error(
        error,
        TOOL_PROCESS_ERROR,
        error_code,
        "%s : %s",
        context_message,
        cause_message
    );
}

/**
 * @brief Force l’arrêt d’un processus puis attend sa terminaison.
 *
 * Cette fonction est utilisée après une annulation ou une erreur de
 * communication afin de ne pas abandonner le processus enfant.
 *
 * @param subprocess Processus à arrêter.
 */
static void tool_process_force_exit_and_wait(
    GSubprocess *subprocess
)
{
    GError *wait_error = NULL;

    if (subprocess == NULL)
    {
        return;
    }

    g_subprocess_force_exit(
        subprocess
    );

    /*
     * L’attente ne doit pas utiliser le GCancellable déjà annulé.
     */
    if (!g_subprocess_wait(
            subprocess,
            NULL,
            &wait_error
        ))
    {
        g_clear_error(
            &wait_error
        );
    }
}

GQuark tool_process_error_quark(void)
{
    return g_quark_from_static_string(
        "labfy-investigation-tool-process-error"
    );
}

gboolean tool_process_run(
    const char *executable_path,
    const char *const arguments[],
    const char *working_directory,
    GCancellable *cancellable,
    ToolProcessResult **out_result,
    GError **error
)
{
    GPtrArray *argument_vector = NULL;

    GSubprocessLauncher *launcher = NULL;
    GSubprocess *subprocess = NULL;

    GBytes *stdout_bytes = NULL;
    GBytes *stderr_bytes = NULL;

    ToolProcessResult *result = NULL;

    GError *local_error = NULL;

    GSubprocessFlags subprocess_flags =
        G_SUBPROCESS_FLAGS_STDOUT_PIPE |
        G_SUBPROCESS_FLAGS_STDERR_PIPE;

    gboolean communication_success = FALSE;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (!tool_process_string_is_valid(
            executable_path
        ) ||
        out_result == NULL ||
        *out_result != NULL ||
        (working_directory != NULL &&
         working_directory[0] == '\0'))
    {
        g_set_error_literal(
            error,
            TOOL_PROCESS_ERROR,
            TOOL_PROCESS_ERROR_INVALID_ARGUMENT,
            "Les arguments fournis à ToolProcess sont invalides."
        );

        return FALSE;
    }

    /*
     * Ne pas lancer un processus si l’opération est déjà annulée.
     */
    if (cancellable != NULL &&
        g_cancellable_is_cancelled(
            cancellable
        ))
    {
        g_set_error_literal(
            error,
            TOOL_PROCESS_ERROR,
            TOOL_PROCESS_ERROR_CANCELLED,
            "L’exécution de l’outil a été annulée avant son lancement."
        );

        return FALSE;
    }

    argument_vector = tool_process_build_argv(
        executable_path,
        arguments
    );

    if (argument_vector == NULL)
    {
        g_set_error_literal(
            error,
            TOOL_PROCESS_ERROR,
            TOOL_PROCESS_ERROR_INVALID_ARGUMENT,
            "Le vecteur d’arguments n’a pas pu être construit."
        );

        return FALSE;
    }

    launcher = g_subprocess_launcher_new(
        subprocess_flags
    );

    if (launcher == NULL)
    {
        g_ptr_array_unref(
            argument_vector
        );

        g_set_error_literal(
            error,
            TOOL_PROCESS_ERROR,
            TOOL_PROCESS_ERROR_SPAWN,
            "Le lanceur de processus n’a pas pu être créé."
        );

        return FALSE;
    }

    if (working_directory != NULL)
    {
        g_subprocess_launcher_set_cwd(
            launcher,
            working_directory
        );
    }

    /*
     * Chaque argument est transmis séparément.
     *
     * Aucun shell n’interprète les espaces, étoiles, points-virgules
     * ou substitutions présents dans les arguments.
     */
    subprocess = g_subprocess_launcher_spawnv(
        launcher,
        (const char *const *) argument_vector->pdata,
        &local_error
    );

    g_clear_object(
        &launcher
    );

    g_ptr_array_unref(
        argument_vector
    );

    argument_vector = NULL;

    if (subprocess == NULL)
    {
        tool_process_set_wrapped_error(
            error,
            TOOL_PROCESS_ERROR_SPAWN,
            "Impossible de lancer l’outil externe",
            local_error
        );

        g_clear_error(
            &local_error
        );

        return FALSE;
    }

    communication_success = g_subprocess_communicate(
        subprocess,
        NULL,
        cancellable,
        &stdout_bytes,
        &stderr_bytes,
        &local_error
    );

    if (!communication_success)
    {
        gboolean was_cancelled = FALSE;

        was_cancelled =
            (local_error != NULL &&
             g_error_matches(
                 local_error,
                 G_IO_ERROR,
                 G_IO_ERROR_CANCELLED
             )) ||
            (cancellable != NULL &&
             g_cancellable_is_cancelled(
                 cancellable
             ));

        /*
         * En cas d’erreur, GSubprocess ne garantit pas que les valeurs
         * de sortie soient exploitables.
         */
        g_clear_pointer(
            &stdout_bytes,
            g_bytes_unref
        );

        g_clear_pointer(
            &stderr_bytes,
            g_bytes_unref
        );

        tool_process_force_exit_and_wait(
            subprocess
        );

        if (was_cancelled)
        {
            tool_process_set_wrapped_error(
                error,
                TOOL_PROCESS_ERROR_CANCELLED,
                "L’exécution de l’outil a été annulée",
                local_error
            );
        }
        else
        {
            tool_process_set_wrapped_error(
                error,
                TOOL_PROCESS_ERROR_COMMUNICATION,
                "La communication avec l’outil externe a échoué",
                local_error
            );
        }

        g_clear_error(
            &local_error
        );

        g_clear_object(
            &subprocess
        );

        return FALSE;
    }

    /*
     * communicate() a réussi : le processus est terminé et son état
     * peut maintenant être consulté.
     */
    result = g_new0(
        ToolProcessResult,
        1
    );

    if (stdout_bytes != NULL)
    {
        result->stdout_bytes =
            stdout_bytes;

        stdout_bytes = NULL;
    }
    else
    {
        result->stdout_bytes =
            g_bytes_new_static(
                "",
                0
            );
    }

    if (stderr_bytes != NULL)
    {
        result->stderr_bytes =
            stderr_bytes;

        stderr_bytes = NULL;
    }
    else
    {
        result->stderr_bytes =
            g_bytes_new_static(
                "",
                0
            );
    }

    result->exited_normally =
        g_subprocess_get_if_exited(
            subprocess
        );

    result->was_signaled =
        g_subprocess_get_if_signaled(
            subprocess
        );

    result->exit_status = -1;
    result->termination_signal = 0;

    if (result->exited_normally)
    {
        result->exit_status =
            g_subprocess_get_exit_status(
                subprocess
            );
    }

    if (result->was_signaled)
    {
        result->termination_signal =
            g_subprocess_get_term_sig(
                subprocess
            );
    }

    /*
     * Un processus terminé doit avoir soit quitté normalement,
     * soit été terminé par un signal.
     */
    if (!result->exited_normally &&
        !result->was_signaled)
    {
        tool_process_result_free(
            result
        );

        result = NULL;

        g_clear_object(
            &subprocess
        );

        g_set_error_literal(
            error,
            TOOL_PROCESS_ERROR,
            TOOL_PROCESS_ERROR_INVALID_RESULT,
            "Le processus s’est terminé dans un état non exploitable."
        );

        return FALSE;
    }

    g_clear_object(
        &subprocess
    );

    *out_result = result;

    return TRUE;
}

void tool_process_result_free(
    ToolProcessResult *result
)
{
    if (result == NULL)
    {
        return;
    }

    g_clear_pointer(
        &result->stdout_bytes,
        g_bytes_unref
    );

    g_clear_pointer(
        &result->stderr_bytes,
        g_bytes_unref
    );

    g_free(
        result
    );
}

GBytes *tool_process_result_ref_stdout(
    const ToolProcessResult *result
)
{
    if (result == NULL ||
        result->stdout_bytes == NULL)
    {
        return NULL;
    }

    return g_bytes_ref(
        result->stdout_bytes
    );
}

GBytes *tool_process_result_ref_stderr(
    const ToolProcessResult *result
)
{
    if (result == NULL ||
        result->stderr_bytes == NULL)
    {
        return NULL;
    }

    return g_bytes_ref(
        result->stderr_bytes
    );
}

gboolean tool_process_result_exited_normally(
    const ToolProcessResult *result
)
{
    if (result == NULL)
    {
        return FALSE;
    }

    return result->exited_normally;
}

int tool_process_result_get_exit_status(
    const ToolProcessResult *result
)
{
    if (result == NULL ||
        !result->exited_normally)
    {
        return -1;
    }

    return result->exit_status;
}

gboolean tool_process_result_was_signaled(
    const ToolProcessResult *result
)
{
    if (result == NULL)
    {
        return FALSE;
    }

    return result->was_signaled;
}

int tool_process_result_get_termination_signal(
    const ToolProcessResult *result
)
{
    if (result == NULL ||
        !result->was_signaled)
    {
        return 0;
    }

    return result->termination_signal;
}

gboolean tool_process_result_is_success(
    const ToolProcessResult *result
)
{
    if (result == NULL)
    {
        return FALSE;
    }

    return result->exited_normally &&
           result->exit_status == 0;
}
