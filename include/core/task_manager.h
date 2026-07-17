/******************************************************************************
 * @file task_manager.h
 * @brief Gestionnaire central des tâches exécutées en arrière-plan.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_TASK_MANAGER_H
#define LABFY_INVESTIGATION_TASK_MANAGER_H

#include "core/background_task.h"

#include <glib.h>

/**
 * @brief Représentation opaque du gestionnaire de tâches.
 */
typedef struct TaskManager TaskManager;

/**
 * @brief Codes d’erreur du gestionnaire de tâches.
 */
typedef enum
{
    TASK_MANAGER_ERROR_INVALID_ARGUMENT,
    TASK_MANAGER_ERROR_ALREADY_ADDED
} TaskManagerError;

/**
 * @brief Domaine d’erreur du gestionnaire de tâches.
 */
#define TASK_MANAGER_ERROR \
    task_manager_error_quark()

/**
 * @brief Callback appelé lorsque la collection de tâches change.
 *
 * Ce callback est utilisé lorsqu’une tâche est ajoutée, retirée,
 * nettoyée ou lorsqu’une demande globale d’annulation est effectuée.
 *
 * Il est appelé sans que le mutex interne du gestionnaire soit verrouillé.
 *
 * @param task_manager Gestionnaire ayant changé.
 * @param user_data Données associées au callback.
 */
typedef void (*TaskManagerChangedCallback)(
    TaskManager *task_manager,
    gpointer user_data
);

/**
 * @brief Retourne le domaine d’erreur du module.
 *
 * @return Quark GLib du domaine d’erreur.
 */
GQuark task_manager_error_quark(void);

/**
 * @brief Crée un gestionnaire de tâches vide.
 *
 * @return Nouveau gestionnaire, ou NULL en cas d’échec.
 */
TaskManager *task_manager_new(void);

/**
 * @brief Libère le gestionnaire et ses références sur les tâches.
 *
 * Cette fonction accepte task_manager == NULL.
 *
 * Les tâches continuent d’exister si d’autres composants possèdent
 * encore leurs propres références.
 *
 * @param task_manager Gestionnaire à libérer.
 */
void task_manager_free(
    TaskManager *task_manager
);

/**
 * @brief Ajoute une tâche au gestionnaire.
 *
 * Le gestionnaire prend une référence supplémentaire sur la tâche.
 * L’appelant conserve la propriété de sa propre référence.
 *
 * Une même instance de BackgroundTask ne peut pas être ajoutée deux fois.
 *
 * @param task_manager Gestionnaire concerné.
 * @param task Tâche à ajouter.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return TRUE si la tâche est ajoutée, sinon FALSE.
 */
gboolean task_manager_add(
    TaskManager *task_manager,
    BackgroundTask *task,
    GError **error
);

/**
 * @brief Retourne le nombre de tâches actuellement suivies.
 *
 * @param task_manager Gestionnaire concerné.
 *
 * @return Nombre de tâches.
 */
gsize task_manager_get_count(
    const TaskManager *task_manager
);

/**
 * @brief Retourne une référence vers une tâche selon son index.
 *
 * L’appelant doit libérer la référence retournée avec :
 *
 * @code
 * background_task_unref(task);
 * @endcode
 *
 * @param task_manager Gestionnaire concerné.
 * @param index Index de la tâche.
 *
 * @return Nouvelle référence vers la tâche, ou NULL si l’index est invalide.
 */
BackgroundTask *task_manager_get_task(
    const TaskManager *task_manager,
    gsize index
);

/**
 * @brief Retire une tâche du gestionnaire.
 *
 * La tâche n’est pas annulée automatiquement.
 *
 * La référence détenue par le gestionnaire est libérée.
 *
 * @param task_manager Gestionnaire concerné.
 * @param task Tâche à retirer.
 *
 * @return TRUE si la tâche était présente et a été retirée, sinon FALSE.
 */
gboolean task_manager_remove(
    TaskManager *task_manager,
    BackgroundTask *task
);

/**
 * @brief Retire toutes les tâches terminées.
 *
 * États concernés :
 *
 * - BACKGROUND_TASK_STATE_COMPLETED ;
 * - BACKGROUND_TASK_STATE_FAILED ;
 * - BACKGROUND_TASK_STATE_CANCELLED.
 *
 * Les tâches en attente ou en cours sont conservées.
 *
 * @param task_manager Gestionnaire concerné.
 *
 * @return Nombre de tâches retirées.
 */
gsize task_manager_remove_finished(
    TaskManager *task_manager
);

/**
 * @brief Demande l’annulation de toutes les tâches en cours.
 *
 * L’annulation reste coopérative.
 *
 * Les tâches ne sont pas retirées du gestionnaire.
 *
 * @param task_manager Gestionnaire concerné.
 */
void task_manager_cancel_all(
    TaskManager *task_manager
);

/**
 * @brief Définit le callback signalant un changement du gestionnaire.
 *
 * Le gestionnaire devient propriétaire de user_data après cet appel.
 *
 * Lorsqu’un nouveau callback remplace l’ancien, les anciennes données
 * sont détruites avec leur fonction de destruction.
 *
 * Le callback peut être désactivé en fournissant callback == NULL.
 *
 * @param task_manager Gestionnaire concerné.
 * @param callback Nouveau callback, ou NULL.
 * @param user_data Données transmises au callback.
 * @param user_data_destroy Fonction de destruction associée.
 */
void task_manager_set_changed_callback(
    TaskManager *task_manager,
    TaskManagerChangedCallback callback,
    gpointer user_data,
    GDestroyNotify user_data_destroy
);

#endif
