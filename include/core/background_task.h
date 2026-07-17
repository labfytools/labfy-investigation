/******************************************************************************
 * @file background_task.h
 * @brief Tâche générique exécutée en arrière-plan.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_BACKGROUND_TASK_H
#define LABFY_INVESTIGATION_BACKGROUND_TASK_H

#include <gio/gio.h>

/**
 * @brief Représentation opaque d'une tâche asynchrone.
 */
typedef struct BackgroundTask BackgroundTask;

/**
 * @brief États possibles d'une tâche.
 */
typedef enum
{
    BACKGROUND_TASK_STATE_PENDING,
    BACKGROUND_TASK_STATE_RUNNING,
    BACKGROUND_TASK_STATE_COMPLETED,
    BACKGROUND_TASK_STATE_FAILED,
    BACKGROUND_TASK_STATE_CANCELLED
} BackgroundTaskState;

/**
 * @brief Codes d'erreur propres au module BackgroundTask.
 */
typedef enum
{
    BACKGROUND_TASK_ERROR_INVALID_ARGUMENT,
    BACKGROUND_TASK_ERROR_ALREADY_STARTED,
    BACKGROUND_TASK_ERROR_WORKER_PROTOCOL
} BackgroundTaskError;

/**
 * @brief Domaine d'erreur du module BackgroundTask.
 */
#define BACKGROUND_TASK_ERROR \
    background_task_error_quark()

/**
 * @brief Fonction exécutée dans un thread secondaire.
 *
 * En cas de succès, la fonction retourne TRUE et peut placer un résultat
 * dans result.
 *
 * En cas d'échec, elle retourne FALSE et renseigne normalement error.
 *
 * @param task Tâche en cours d'exécution.
 * @param cancellable Objet d'annulation associé à la tâche.
 * @param worker_data Données privées du worker.
 * @param result Emplacement recevant le résultat alloué.
 * @param error Emplacement recevant une erreur.
 *
 * @return TRUE en cas de succès, sinon FALSE.
 */
typedef gboolean (*BackgroundTaskWorker)(
    BackgroundTask *task,
    GCancellable *cancellable,
    gpointer worker_data,
    gpointer *result,
    GError **error
);

/**
 * @brief Callback appelé lorsque la tâche est terminée.
 *
 * Ce callback est appelé après la mise à jour de l'état final.
 *
 * @param task Tâche terminée.
 * @param user_data Données privées du callback.
 */
typedef void (*BackgroundTaskCompletionCallback)(
    BackgroundTask *task,
    gpointer user_data
);

/**
 * @brief Retourne le domaine d'erreur du module.
 *
 * @return Quark GLib du domaine d'erreur.
 */
GQuark background_task_error_quark(void);

/**
 * @brief Crée une nouvelle tâche en attente.
 *
 * @param title Titre non vide de la tâche.
 *
 * @return Nouvelle tâche, ou NULL si le titre est invalide.
 */
BackgroundTask *background_task_new(
    const char *title
);

/**
 * @brief Ajoute une référence à une tâche.
 *
 * @param task Tâche concernée.
 *
 * @return La tâche fournie, ou NULL.
 */
BackgroundTask *background_task_ref(
    BackgroundTask *task
);

/**
 * @brief Libère une référence à une tâche.
 *
 * La fonction accepte task == NULL.
 *
 * @param task Tâche concernée.
 */
void background_task_unref(
    BackgroundTask *task
);

/**
 * @brief Démarre l'exécution asynchrone d'une tâche.
 *
 * Si le démarrage réussit, la tâche devient propriétaire de worker_data
 * et completion_data.
 *
 * Si le démarrage échoue, l'appelant conserve leur propriété.
 *
 * Le futur résultat produit par le worker sera détruit avec
 * result_destroy lors de la destruction de la tâche.
 *
 * @param task Tâche à démarrer.
 * @param worker Fonction exécutée dans un thread secondaire.
 * @param worker_data Données transmises au worker.
 * @param worker_data_destroy Fonction de destruction de worker_data.
 * @param result_destroy Fonction de destruction du futur résultat.
 * @param completion_callback Callback final facultatif.
 * @param completion_data Données transmises au callback final.
 * @param completion_data_destroy Fonction de destruction associée.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return TRUE si la tâche a été lancée, sinon FALSE.
 */
gboolean background_task_start(
    BackgroundTask *task,
    BackgroundTaskWorker worker,
    gpointer worker_data,
    GDestroyNotify worker_data_destroy,
    GDestroyNotify result_destroy,
    BackgroundTaskCompletionCallback completion_callback,
    gpointer completion_data,
    GDestroyNotify completion_data_destroy,
    GError **error
);

/**
 * @brief Demande l'annulation coopérative de la tâche.
 *
 * @param task Tâche concernée.
 */
void background_task_cancel(
    BackgroundTask *task
);

/**
 * @brief Indique si une annulation a été demandée.
 *
 * @param task Tâche concernée.
 *
 * @return TRUE si l'annulation a été demandée, sinon FALSE.
 */
gboolean background_task_is_cancelled(
    const BackgroundTask *task
);

/**
 * @brief Met à jour la progression d'une tâche en cours.
 *
 * La progression est automatiquement limitée à l'intervalle
 * compris entre 0.0 et 1.0.
 *
 * @param task Tâche concernée.
 * @param progress Nouvelle progression.
 * @param status_message Message facultatif, copié par le module.
 */
void background_task_report_progress(
    BackgroundTask *task,
    double progress,
    const char *status_message
);

/**
 * @brief Retourne le titre immuable d'une tâche.
 *
 * Le pointeur retourné est emprunté.
 *
 * @param task Tâche concernée.
 *
 * @return Titre de la tâche, ou NULL.
 */
const char *background_task_get_title(
    const BackgroundTask *task
);

/**
 * @brief Retourne l'état courant d'une tâche.
 *
 * @param task Tâche concernée.
 *
 * @return État courant de la tâche.
 */
BackgroundTaskState background_task_get_state(
    const BackgroundTask *task
);

/**
 * @brief Retourne la progression courante.
 *
 * @param task Tâche concernée.
 *
 * @return Progression comprise entre 0.0 et 1.0.
 */
double background_task_get_progress(
    const BackgroundTask *task
);

/**
 * @brief Copie le message de progression courant.
 *
 * L'appelant doit libérer la chaîne avec g_free().
 *
 * @param task Tâche concernée.
 *
 * @return Nouvelle chaîne, ou NULL.
 */
char *background_task_dup_status_message(
    const BackgroundTask *task
);

/**
 * @brief Retourne la date de démarrage monotone en microsecondes.
 *
 * @param task Tâche concernée.
 *
 * @return Date de démarrage, ou 0 si la tâche n'a pas démarré.
 */
gint64 background_task_get_started_at_us(
    const BackgroundTask *task
);

/**
 * @brief Retourne la date de fin monotone en microsecondes.
 *
 * @param task Tâche concernée.
 *
 * @return Date de fin, ou 0 si la tâche n'est pas terminée.
 */
gint64 background_task_get_finished_at_us(
    const BackgroundTask *task
);

/**
 * @brief Retourne une copie de l'erreur finale.
 *
 * L'appelant doit libérer l'erreur avec g_error_free().
 *
 * @param task Tâche concernée.
 *
 * @return Nouvelle copie de l'erreur, ou NULL.
 */
GError *background_task_dup_error(
    const BackgroundTask *task
);

/**
 * @brief Retourne le résultat final de la tâche.
 *
 * Le pointeur retourné est emprunté et ne doit pas être libéré.
 * Il n'est exploitable qu'après l'état BACKGROUND_TASK_STATE_COMPLETED.
 *
 * @param task Tâche concernée.
 *
 * @return Résultat final, ou NULL.
 */
gpointer background_task_get_result(
    const BackgroundTask *task
);

#endif
