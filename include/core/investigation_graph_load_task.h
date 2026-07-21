/******************************************************************************
 * @file investigation_graph_load_task.h
 * @brief Chargement asynchrone du graphe métier d'une enquête.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_INVESTIGATION_GRAPH_LOAD_TASK_H
#define LABFY_INVESTIGATION_INVESTIGATION_GRAPH_LOAD_TASK_H

#include "models/investigation_graph_model.h"

#include <glib.h>

G_BEGIN_DECLS

/**
 * @brief Tâche opaque de chargement asynchrone d'un graphe d'enquête.
 *
 * Chaque exécution ouvre sa propre connexion SQLite dans le worker.
 *
 * L'objet peut être réutilisé après la finalisation complète d'une exécution.
 */
typedef struct InvestigationGraphLoadTask InvestigationGraphLoadTask;

/**
 * @brief Catégories d'erreurs de la tâche de chargement.
 */
typedef enum
{
    /**
     * Un argument public est invalide.
     */
    INVESTIGATION_GRAPH_LOAD_TASK_ERROR_INVALID_ARGUMENT,

    /**
     * Une allocation mémoire a échoué.
     */
    INVESTIGATION_GRAPH_LOAD_TASK_ERROR_MEMORY,

    /**
     * Une exécution est déjà active.
     */
    INVESTIGATION_GRAPH_LOAD_TASK_ERROR_ALREADY_RUNNING,

    /**
     * La connexion SQLite propre au worker n'a pas pu être ouverte.
     */
    INVESTIGATION_GRAPH_LOAD_TASK_ERROR_DATABASE_OPEN,

    /**
     * Le graphe n'a pas pu être construit depuis SQLite.
     */
    INVESTIGATION_GRAPH_LOAD_TASK_ERROR_LOAD,

    /**
     * Le chargement a été annulé.
     */
    INVESTIGATION_GRAPH_LOAD_TASK_ERROR_CANCELLED,

    /**
     * Une incohérence interne empêche la finalisation.
     */
    INVESTIGATION_GRAPH_LOAD_TASK_ERROR_INTERNAL
} InvestigationGraphLoadTaskError;

/**
 * @brief Domaine d'erreur de la tâche.
 */
#define INVESTIGATION_GRAPH_LOAD_TASK_ERROR \
    investigation_graph_load_task_error_quark()

/**
 * @brief Callback final exécuté sur le contexte principal ayant démarré la
 * tâche.
 *
 * En cas de succès, graph_model appartient au destinataire du callback.
 *
 * En cas d'échec, error est empruntée et valable uniquement pendant le
 * callback.
 *
 * @param load_task Tâche ayant terminé.
 * @param graph_model Graphe transféré, ou NULL.
 * @param error Erreur empruntée, ou NULL.
 * @param user_data Données fournies au démarrage.
 */
typedef void (*InvestigationGraphLoadTaskCallback)(
    InvestigationGraphLoadTask *load_task,
    InvestigationGraphModel *graph_model,
    const GError *error,
    gpointer user_data
);

/**
 * @brief Retourne le domaine d'erreur de la tâche.
 */
GQuark investigation_graph_load_task_error_quark(void);

/**
 * @brief Crée une tâche associée à un chemin SQLite.
 *
 * Le chemin est copié. La base n'est pas ouverte par le constructeur.
 *
 * @param database_path Chemin non vide de la base SQLite.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouvelle tâche, ou NULL en cas d'échec.
 */
InvestigationGraphLoadTask *investigation_graph_load_task_new(
    const char *database_path,
    GError **error
);

/**
 * @brief Libère la référence publique de la tâche.
 *
 * Si une exécution est active, elle est annulée et la destruction réelle est
 * différée jusqu'à la fin du worker. Aucun callback utilisateur n'est alors
 * invoqué.
 *
 * Cette fonction accepte NULL.
 *
 * @param load_task Tâche à libérer.
 */
void investigation_graph_load_task_free(
    InvestigationGraphLoadTask *load_task
);

/**
 * @brief Démarre le chargement en arrière-plan.
 *
 * En cas de succès, la tâche prend possession de user_data et appellera
 * user_data_destroy exactement une fois après le callback, ou lors de la
 * destruction anticipée de la tâche.
 *
 * En cas d'échec immédiat, l'appelant conserve la propriété de user_data.
 *
 * @param load_task Tâche valide et inactive.
 * @param callback Callback final obligatoire.
 * @param user_data Données facultatives du callback.
 * @param user_data_destroy Destructeur facultatif de user_data.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return TRUE si l'exécution a été lancée.
 */
gboolean investigation_graph_load_task_start(
    InvestigationGraphLoadTask *load_task,
    InvestigationGraphLoadTaskCallback callback,
    gpointer user_data,
    GDestroyNotify user_data_destroy,
    GError **error
);

/**
 * @brief Demande l'annulation coopérative de l'exécution active.
 *
 * Cette fonction accepte NULL et peut être appelée plusieurs fois.
 *
 * @param load_task Tâche concernée.
 */
void investigation_graph_load_task_cancel(
    InvestigationGraphLoadTask *load_task
);

/**
 * @brief Indique si une exécution est active.
 *
 * @param load_task Tâche à consulter.
 *
 * @return TRUE uniquement entre le démarrage accepté et la finalisation.
 */
gboolean investigation_graph_load_task_is_running(
    const InvestigationGraphLoadTask *load_task
);

G_END_DECLS

#endif
