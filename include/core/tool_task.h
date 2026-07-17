/******************************************************************************
 * @file tool_task.h
 * @brief Exécution d'un outil externe dans une tâche d'arrière-plan.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_TOOL_TASK_H
#define LABFY_INVESTIGATION_TOOL_TASK_H

#include "core/background_task.h"
#include "core/tool_process.h"
#include "core/tool_registry.h"

#include <gio/gio.h>
#include <glib.h>

G_BEGIN_DECLS

/**
 * @brief Erreurs produites par ToolTask.
 */
typedef enum
{
    /**
     * Un argument transmis au module est invalide.
     */
    TOOL_TASK_ERROR_INVALID_ARGUMENT,

    /**
     * L'identifiant demandé n'existe pas dans le registre.
     */
    TOOL_TASK_ERROR_TOOL_NOT_FOUND,

    /**
     * La disponibilité de l'outil n'a pas encore été vérifiée.
     */
    TOOL_TASK_ERROR_TOOL_NOT_CHECKED,

    /**
     * L'outil a été vérifié mais son exécutable est absent.
     */
    TOOL_TASK_ERROR_TOOL_MISSING,

    /**
     * L'outil est disponible, mais son chemin résolu est invalide.
     */
    TOOL_TASK_ERROR_INVALID_TOOL_STATE,

    /**
     * La BackgroundTask n'a pas pu être créée.
     */
    TOOL_TASK_ERROR_TASK_CREATION,

    /**
     * La BackgroundTask n'a pas pu être démarrée.
     */
    TOOL_TASK_ERROR_TASK_START,

    /**
     * L'exécution externe a échoué.
     */
    TOOL_TASK_ERROR_PROCESS
} ToolTaskError;

/**
 * @brief Domaine d'erreur de ToolTask.
 */
#define TOOL_TASK_ERROR \
    tool_task_error_quark()

/**
 * @brief Exécution préparée d'un outil externe.
 *
 * Cette structure possède :
 *
 * - la BackgroundTask avant son transfert éventuel à d'autres modules ;
 * - les données du worker avant le démarrage ;
 * - les copies de l'identifiant, du chemin et des arguments.
 */
typedef struct ToolTask ToolTask;

/**
 * @brief Résultat opaque produit par une ToolTask.
 */
typedef struct ToolTaskResult ToolTaskResult;

/**
 * @brief Retourne le domaine d'erreur de ToolTask.
 *
 * @return Quark GLib du domaine d'erreur.
 */
GQuark tool_task_error_quark(void);

/**
 * @brief Prépare une tâche exécutant un outil enregistré.
 *
 * Cette fonction ne démarre pas la tâche.
 *
 * Toutes les informations nécessaires à l'exécution sont copiées. Le
 * ToolTask créé ne dépend donc plus de la durée de vie du registre ni des
 * chaînes fournies.
 *
 * L'outil doit avoir été vérifié avec tool_registry_refresh() et être dans
 * l'état TOOL_AVAILABILITY_AVAILABLE.
 *
 * @param tool_registry Registre contenant l'outil.
 * @param tool_identifier Identifiant interne de l'outil.
 * @param task_title Titre affiché pour la BackgroundTask.
 * @param arguments Tableau d'arguments terminé par NULL, ou NULL.
 * @param working_directory Dossier de travail facultatif, ou NULL.
 * @param error Emplacement facultatif pour l'erreur.
 *
 * @return Nouveau ToolTask, ou NULL en cas d'échec.
 */
ToolTask *tool_task_new(
    const ToolRegistry *tool_registry,
    const char *tool_identifier,
    const char *task_title,
    const char *const arguments[],
    const char *working_directory,
    GError **error
);

/**
 * @brief Libère une ToolTask.
 *
 * Avant le démarrage, cette fonction libère également les données préparées
 * pour le worker.
 *
 * Après un démarrage réussi, les données du worker appartiennent à la
 * BackgroundTask.
 *
 * La fonction accepte NULL.
 *
 * @param tool_task Tâche à libérer.
 */
void tool_task_free(
    ToolTask *tool_task
);

/**
 * @brief Retourne la BackgroundTask associée.
 *
 * Le pointeur retourné est emprunté. Il reste valide tant que ToolTask
 * conserve sa propre référence ou qu'un autre propriétaire, par exemple
 * TaskManager, conserve une référence.
 *
 * Cette fonction permet notamment d'ajouter la tâche au TaskManager avant
 * son démarrage.
 *
 * @param tool_task ToolTask consultée.
 *
 * @return BackgroundTask associée, ou NULL.
 */
BackgroundTask *tool_task_get_background_task(
    const ToolTask *tool_task
);

/**
 * @brief Démarre l'exécution asynchrone de l'outil.
 *
 * Si le démarrage réussit :
 *
 * - la BackgroundTask devient propriétaire des données du worker ;
 * - ToolTask ne doit plus les libérer ;
 * - ToolTask peut être libérée dès qu'un autre propriétaire conserve une
 *   référence sur la BackgroundTask.
 *
 * Si le démarrage échoue, ToolTask conserve la propriété de ses données.
 *
 * @param tool_task ToolTask à démarrer.
 * @param completion_callback Callback final facultatif.
 * @param completion_data Données facultatives du callback.
 * @param completion_data_destroy Destructeur facultatif des données.
 * @param error Emplacement facultatif pour l'erreur.
 *
 * @return TRUE si la tâche a été démarrée, sinon FALSE.
 */
gboolean tool_task_start(
    ToolTask *tool_task,
    BackgroundTaskCompletionCallback completion_callback,
    gpointer completion_data,
    GDestroyNotify completion_data_destroy,
    GError **error
);

/**
 * @brief Retourne le résultat d'une BackgroundTask créée par ToolTask.
 *
 * Cette fonction doit uniquement être appelée avec une BackgroundTask
 * obtenue par tool_task_get_background_task().
 *
 * Le pointeur retourné est emprunté et appartient à la BackgroundTask.
 * Il n'est disponible qu'après l'état BACKGROUND_TASK_STATE_COMPLETED.
 *
 * @param background_task Tâche créée par ToolTask.
 *
 * @return Résultat ToolTask emprunté, ou NULL.
 */
const ToolTaskResult *tool_task_result_from_background_task(
    const BackgroundTask *background_task
);

/**
 * @brief Libère un résultat ToolTask.
 *
 * Cette fonction est normalement transmise comme destructeur du résultat à
 * background_task_start().
 *
 * La fonction accepte NULL.
 *
 * @param result Résultat à libérer.
 */
void tool_task_result_free(
    ToolTaskResult *result
);

/**
 * @brief Retourne l'identifiant de l'outil exécuté.
 *
 * La chaîne retournée est empruntée.
 *
 * @param result Résultat consulté.
 *
 * @return Identifiant de l'outil, ou NULL.
 */
const char *tool_task_result_get_tool_identifier(
    const ToolTaskResult *result
);

/**
 * @brief Retourne le chemin de l'exécutable utilisé.
 *
 * La chaîne retournée est empruntée.
 *
 * @param result Résultat consulté.
 *
 * @return Chemin de l'exécutable, ou NULL.
 */
const char *tool_task_result_get_executable_path(
    const ToolTaskResult *result
);

/**
 * @brief Retourne le nombre d'arguments transmis à l'outil.
 *
 * Le chemin de l'exécutable, utilisé comme argv[0], n'est pas compté.
 *
 * @param result Résultat consulté.
 *
 * @return Nombre d'arguments.
 */
gsize tool_task_result_get_argument_count(
    const ToolTaskResult *result
);

/**
 * @brief Retourne un argument transmis à l'outil.
 *
 * La chaîne retournée est empruntée.
 *
 * @param result Résultat consulté.
 * @param index Index de l'argument.
 *
 * @return Argument correspondant, ou NULL si l'index est invalide.
 */
const char *tool_task_result_get_argument(
    const ToolTaskResult *result,
    gsize index
);

/**
 * @brief Retourne le dossier de travail utilisé.
 *
 * La chaîne retournée est empruntée.
 *
 * @param result Résultat consulté.
 *
 * @return Dossier de travail, ou NULL.
 */
const char *tool_task_result_get_working_directory(
    const ToolTaskResult *result
);

/**
 * @brief Retourne le résultat brut du processus externe.
 *
 * Le pointeur retourné est emprunté et ne doit pas être libéré.
 *
 * @param result Résultat consulté.
 *
 * @return Résultat ToolProcess, ou NULL.
 */
const ToolProcessResult *tool_task_result_get_process_result(
    const ToolTaskResult *result
);

G_END_DECLS

#endif
