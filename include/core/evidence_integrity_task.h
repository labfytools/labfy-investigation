/******************************************************************************
 * @file evidence_integrity_task.h
 * @brief Tâche asynchrone de vérification d'intégrité d'une preuve.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_EVIDENCE_INTEGRITY_TASK_H
#define LABFY_INVESTIGATION_EVIDENCE_INTEGRITY_TASK_H

#include "core/background_task.h"
#include "core/task_manager.h"

#include <glib.h>

/**
 * @brief Codes d'erreur propres à la tâche de vérification.
 */
typedef enum
{
    EVIDENCE_INTEGRITY_TASK_ERROR_INVALID_ARGUMENT,
    EVIDENCE_INTEGRITY_TASK_ERROR_VERIFY
} EvidenceIntegrityTaskError;

/**
 * @brief Domaine d'erreur du module.
 */
#define EVIDENCE_INTEGRITY_TASK_ERROR \
    evidence_integrity_task_error_quark()

/**
 * @brief Paramètres nécessaires à une vérification d'intégrité.
 *
 * Toutes les chaînes sont copiées avant le démarrage du worker.
 */
typedef struct
{
    const char *investigation_root_path;
    const char *relative_path;
    const char *expected_sha256;
} EvidenceIntegrityTaskRequest;

/**
 * @brief Retourne le domaine d'erreur du module.
 */
GQuark evidence_integrity_task_error_quark(void);

/**
 * @brief Crée, enregistre et démarre une tâche de vérification.
 *
 * Le résultat final est un EvidenceIntegrityVerificationResult emprunté
 * à la BackgroundTask. Il reste valide tant que la tâche existe.
 *
 * En cas de succès :
 *
 * - TaskManager conserve une référence ;
 * - le worker conserve une référence pendant son exécution ;
 * - l'appelant reçoit sa référence initiale et doit la libérer avec
 *   background_task_unref().
 *
 * completion_data est transféré à BackgroundTask uniquement si le
 * démarrage réussit. En cas d'échec, l'appelant en reste propriétaire.
 *
 * @param task_manager Gestionnaire recevant la tâche.
 * @param request Paramètres de la vérification.
 * @param completion_callback Callback final facultatif.
 * @param completion_data Données du callback final.
 * @param completion_data_destroy Destructeur de completion_data.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouvelle tâche démarrée, ou NULL.
 */
BackgroundTask *evidence_integrity_task_start(
    TaskManager *task_manager,
    const EvidenceIntegrityTaskRequest *request,
    BackgroundTaskCompletionCallback completion_callback,
    gpointer completion_data,
    GDestroyNotify completion_data_destroy,
    GError **error
);

#endif
