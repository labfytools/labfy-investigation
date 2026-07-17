/******************************************************************************
 * @file tool_process.h
 * @brief Exécution sécurisée des outils externes.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_TOOL_PROCESS_H
#define LABFY_INVESTIGATION_TOOL_PROCESS_H

#include <gio/gio.h>
#include <glib.h>

G_BEGIN_DECLS

/**
 * @brief Erreurs produites pendant l’exécution d’un outil externe.
 */
typedef enum
{
    /**
     * Un argument transmis à la fonction est invalide.
     */
    TOOL_PROCESS_ERROR_INVALID_ARGUMENT,

    /**
     * Le processus n’a pas pu être créé ou lancé.
     */
    TOOL_PROCESS_ERROR_SPAWN,

    /**
     * La communication avec le processus a échoué.
     */
    TOOL_PROCESS_ERROR_COMMUNICATION,

    /**
     * L’exécution a été annulée.
     */
    TOOL_PROCESS_ERROR_CANCELLED,

    /**
     * Le processus s’est terminé sans produire un résultat exploitable.
     */
    TOOL_PROCESS_ERROR_INVALID_RESULT
} ToolProcessError;

/**
 * @brief Domaine d’erreur de ToolProcess.
 */
#define TOOL_PROCESS_ERROR \
    tool_process_error_quark()

/**
 * @brief Résultat opaque d’une exécution externe.
 */
typedef struct ToolProcessResult ToolProcessResult;

/**
 * @brief Retourne le domaine d’erreur de ToolProcess.
 *
 * @return Quark GLib du domaine d’erreur.
 */
GQuark tool_process_error_quark(void);

/**
 * @brief Exécute un outil externe sans passer par un shell.
 *
 * Le programme et chacun de ses arguments sont transmis séparément à
 * GSubprocess.
 *
 * Un code de sortie différent de zéro ne constitue pas une erreur de cette
 * fonction. Dans ce cas, la fonction retourne TRUE et le code de sortie est
 * disponible dans ToolProcessResult.
 *
 * @param executable_path Chemin de l’exécutable.
 * @param arguments Arguments supplémentaires terminés par NULL, ou NULL.
 * @param working_directory Dossier de travail, ou NULL.
 * @param cancellable Objet d’annulation facultatif.
 * @param out_result Emplacement recevant le résultat.
 * @param error Emplacement facultatif pour l’erreur.
 *
 * @return TRUE lorsqu’un résultat exploitable a été obtenu, sinon FALSE.
 */
gboolean tool_process_run(
    const char *executable_path,
    const char *const arguments[],
    const char *working_directory,
    GCancellable *cancellable,
    ToolProcessResult **out_result,
    GError **error
);

/**
 * @brief Libère un résultat d’exécution.
 *
 * @param result Résultat à libérer, ou NULL.
 */
void tool_process_result_free(
    ToolProcessResult *result
);

/**
 * @brief Retourne une nouvelle référence sur la sortie standard.
 *
 * La référence retournée doit être libérée avec g_bytes_unref().
 *
 * @param result Résultat consulté.
 *
 * @return Nouvelle référence sur stdout, ou NULL.
 */
GBytes *tool_process_result_ref_stdout(
    const ToolProcessResult *result
);

/**
 * @brief Retourne une nouvelle référence sur la sortie d’erreur.
 *
 * La référence retournée doit être libérée avec g_bytes_unref().
 *
 * @param result Résultat consulté.
 *
 * @return Nouvelle référence sur stderr, ou NULL.
 */
GBytes *tool_process_result_ref_stderr(
    const ToolProcessResult *result
);

/**
 * @brief Indique si le processus s’est terminé normalement.
 *
 * @param result Résultat consulté.
 *
 * @return TRUE si le processus a appelé exit() ou retourné depuis main().
 */
gboolean tool_process_result_exited_normally(
    const ToolProcessResult *result
);

/**
 * @brief Retourne le code de sortie du processus.
 *
 * @param result Résultat consulté.
 *
 * @return Code de sortie, ou -1 si le processus ne s’est pas terminé
 *         normalement.
 */
int tool_process_result_get_exit_status(
    const ToolProcessResult *result
);

/**
 * @brief Indique si le processus a été terminé par un signal.
 *
 * @param result Résultat consulté.
 *
 * @return TRUE si le processus a été terminé par un signal.
 */
gboolean tool_process_result_was_signaled(
    const ToolProcessResult *result
);

/**
 * @brief Retourne le signal ayant terminé le processus.
 *
 * @param result Résultat consulté.
 *
 * @return Numéro du signal, ou zéro si aucun signal n’est disponible.
 */
int tool_process_result_get_termination_signal(
    const ToolProcessResult *result
);

/**
 * @brief Indique si le programme s’est terminé avec succès.
 *
 * Le succès fonctionnel signifie :
 *
 * - fin normale ;
 * - code de sortie égal à zéro.
 *
 * @param result Résultat consulté.
 *
 * @return TRUE si le programme a retourné zéro.
 */
gboolean tool_process_result_is_success(
    const ToolProcessResult *result
);

G_END_DECLS

#endif
