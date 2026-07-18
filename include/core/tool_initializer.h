/******************************************************************************
 * @file tool_initializer.h
 * @brief Initialisation asynchrone des outils externes.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_TOOL_INITIALIZER_H
#define LABFY_INVESTIGATION_TOOL_INITIALIZER_H

#include "core/background_task.h"
#include "core/task_manager.h"
#include "core/tool_registry.h"

#include <glib.h>

G_BEGIN_DECLS

/**
 * @brief Représentation opaque de l’initialiseur d’outils.
 */
typedef struct ToolInitializer ToolInitializer;

/**
 * @brief Résumé d’une initialisation terminée avec succès.
 */
typedef struct
{
    gsize total_count;
    gsize available_count;
    gsize missing_count;
    gsize version_detected_count;
    gsize version_failed_count;
} ToolInitializationSummary;

/**
 * @brief Fonction appelée après la fin d’une initialisation.
 *
 * Le callback est exécuté sur le contexte principal GLib.
 *
 * summary est non NULL uniquement après une réussite.
 * error est non NULL uniquement lorsqu’une erreur est disponible.
 *
 * Les pointeurs summary et error sont empruntés et restent valides
 * uniquement pendant l’appel du callback.
 */
typedef void (*ToolInitializerCompletedCallback)(
    ToolInitializer *tool_initializer,
    BackgroundTaskState final_state,
    const ToolInitializationSummary *summary,
    const GError *error,
    gpointer user_data
);

/**
 * @brief Codes d’erreur de ToolInitializer.
 */
typedef enum
{
    /**
     * Un argument transmis au module est invalide.
     */
    TOOL_INITIALIZER_ERROR_INVALID_ARGUMENT,

    /**
     * Une initialisation est déjà en cours.
     */
    TOOL_INITIALIZER_ERROR_ALREADY_RUNNING,

    /**
     * Aucune initialisation n’est actuellement en cours.
     */
    TOOL_INITIALIZER_ERROR_NOT_RUNNING,

    /**
     * La tâche d’arrière-plan n’a pas pu être créée.
     */
    TOOL_INITIALIZER_ERROR_TASK_CREATION,

    /**
     * La tâche n’a pas pu être ajoutée au TaskManager.
     */
    TOOL_INITIALIZER_ERROR_TASK_REGISTRATION,

    /**
     * La tâche n’a pas pu être démarrée.
     */
    TOOL_INITIALIZER_ERROR_TASK_START,

    /**
     * Le catalogue n’a pas pu être enregistré.
     */
    TOOL_INITIALIZER_ERROR_CATALOG_REGISTRATION,

    /**
     * Le registre n’a pas pu rechercher les exécutables.
     */
    TOOL_INITIALIZER_ERROR_REGISTRY_REFRESH,

    /**
     * L’état interne de l’initialiseur est incohérent.
     */
    TOOL_INITIALIZER_ERROR_INTERNAL_STATE
} ToolInitializerError;

/**
 * @brief Domaine d’erreur du module.
 */
#define TOOL_INITIALIZER_ERROR \
    tool_initializer_error_quark()

/**
 * @brief Retourne le domaine d’erreur de ToolInitializer.
 *
 * @return Quark GLib du domaine d’erreur.
 */
GQuark tool_initializer_error_quark(void);

/**
 * @brief Crée un initialiseur d’outils externes.
 *
 * L’initialiseur :
 *
 * - conserve une référence empruntée vers task_manager ;
 * - crée et possède son propre ToolRegistry ;
 * - ne prend pas possession de task_manager.
 *
 * task_manager doit rester valide pendant les appels publics utilisant
 * l’initialiseur.
 *
 * @param task_manager Gestionnaire recevant la tâche d’initialisation.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouvel initialiseur, ou NULL en cas d’échec.
 */
ToolInitializer *tool_initializer_new(
    TaskManager *task_manager,
    GError **error
);

/**
 * @brief Libère la référence détenue par l’appelant.
 *
 * Si une initialisation est encore active, son annulation est demandée.
 * La fonction ne bloque pas en attendant la fin du worker.
 *
 * Une référence interne conserve les ressources nécessaires jusqu’à la
 * finalisation réelle de la tâche.
 *
 * La fonction accepte tool_initializer == NULL.
 *
 * @param tool_initializer Initialiseur à libérer.
 */
void tool_initializer_free(
    ToolInitializer *tool_initializer
);

/**
 * @brief Retourne le registre appartenant à l’initialiseur.
 *
 * Le pointeur retourné est emprunté et ne doit jamais être libéré par
 * l’appelant.
 *
 * Il ne doit plus être utilisé après tool_initializer_free().
 *
 * @param tool_initializer Initialiseur consulté.
 *
 * @return Registre emprunté, ou NULL.
 */
ToolRegistry *tool_initializer_get_registry(
    ToolInitializer *tool_initializer
);

/**
 * @brief Enregistre le callback appelé après chaque initialisation.
 *
 * Le même callback est conservé pour les lancements suivants.
 *
 * En passant callback == NULL, l’enregistrement actuel est supprimé.
 *
 * Lorsque destroy_notify est non NULL, ToolInitializer devient
 * responsable de user_data. La fonction de destruction sera appelée
 * exactement une fois lorsque :
 *
 * - le callback est remplacé ;
 * - le callback est supprimé ;
 * - l’initialiseur est détruit.
 *
 * Le callback est exécuté sur le contexte principal GLib.
 *
 * @param tool_initializer Initialiseur concerné.
 * @param callback Fonction appelée après la fin, ou NULL.
 * @param user_data Données transmises au callback.
 * @param destroy_notify Fonction libérant user_data, ou NULL.
 */
void tool_initializer_set_completed_callback(
    ToolInitializer *tool_initializer,
    ToolInitializerCompletedCallback callback,
    gpointer user_data,
    GDestroyNotify destroy_notify
);

/**
 * @brief Démarre l’initialisation en arrière-plan.
 *
 * La fonction :
 *
 * - crée une nouvelle BackgroundTask ;
 * - l’ajoute au TaskManager ;
 * - lance son worker ;
 * - refuse un second démarrage tant qu’une tâche est active.
 *
 * Une nouvelle exécution peut être lancée après la fin ou l’annulation
 * de la précédente.
 *
 * @param tool_initializer Initialiseur concerné.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return TRUE si la tâche a été lancée, sinon FALSE.
 */
gboolean tool_initializer_start(
    ToolInitializer *tool_initializer,
    GError **error
);

/**
 * @brief Demande l’annulation de l’initialisation active.
 *
 * L’annulation est coopérative.
 *
 * @param tool_initializer Initialiseur concerné.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return TRUE si une annulation a été demandée, sinon FALSE.
 */
gboolean tool_initializer_cancel(
    ToolInitializer *tool_initializer,
    GError **error
);

/**
 * @brief Indique si une initialisation est en cours.
 *
 * @param tool_initializer Initialiseur consulté.
 *
 * @return TRUE si une tâche est en attente ou en cours, sinon FALSE.
 */
gboolean tool_initializer_is_running(
    const ToolInitializer *tool_initializer
);

/**
 * @brief Retourne une référence vers la tâche actuelle ou la dernière tâche.
 *
 * L’appelant devient propriétaire d’une référence et doit la libérer avec :
 *
 * @code
 * background_task_unref(task);
 * @endcode
 *
 * @param tool_initializer Initialiseur consulté.
 *
 * @return Nouvelle référence vers la tâche, ou NULL.
 */
BackgroundTask *tool_initializer_get_task(
    const ToolInitializer *tool_initializer
);

/**
 * @brief Copie le résumé de la dernière initialisation réussie.
 *
 * Le résumé n’est disponible que lorsque la dernière tâche s’est terminée
 * dans l’état BACKGROUND_TASK_STATE_COMPLETED.
 *
 * @param tool_initializer Initialiseur consulté.
 * @param out_summary Structure recevant une copie du résumé.
 *
 * @return TRUE si un résumé est disponible, sinon FALSE.
 */
gboolean tool_initializer_get_last_summary(
    const ToolInitializer *tool_initializer,
    ToolInitializationSummary *out_summary
);

G_END_DECLS

#endif
