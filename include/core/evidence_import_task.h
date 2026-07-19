/******************************************************************************
 * @file evidence_import_task.h
 * @brief Tâche asynchrone d’import d’une preuve.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_EVIDENCE_IMPORT_TASK_H
#define LABFY_INVESTIGATION_EVIDENCE_IMPORT_TASK_H

#include "core/background_task.h"
#include "core/task_manager.h"

#include <glib.h>

/**
 * @brief Codes d’erreur propres à la tâche d’import.
 */
typedef enum
{
    EVIDENCE_IMPORT_TASK_ERROR_INVALID_ARGUMENT,
    EVIDENCE_IMPORT_TASK_ERROR_DATABASE,
    EVIDENCE_IMPORT_TASK_ERROR_IMPORT
} EvidenceImportTaskError;

/**
 * @brief Domaine d’erreur du module.
 */
#define EVIDENCE_IMPORT_TASK_ERROR \
    evidence_import_task_error_quark()

/**
 * @brief Paramètres nécessaires à une tâche d’import.
 *
 * Toutes les chaînes sont copiées avant le démarrage du worker.
 */
typedef struct
{
    const char *database_path;

    const char *source_path;
    const char *destination_directory;
    const char *relative_directory;

    const char *type_identifier;
    const char *collected_at;
    const char *source;
    const char *description;
} EvidenceImportTaskRequest;

/**
 * @brief Retourne le domaine d’erreur du module.
 *
 * @return Quark GLib du domaine d’erreur.
 */
GQuark evidence_import_task_error_quark(void);

/**
 * @brief Crée, enregistre et démarre une tâche d’import.
 *
 * La tâche ouvre sa propre connexion SQLite dans le thread secondaire.
 *
 * En cas de succès :
 *
 * - TaskManager conserve une référence ;
 * - le worker conserve une référence pendant son exécution ;
 * - l’appelant reçoit sa référence initiale et doit la libérer avec
 *   background_task_unref().
 *
 * completion_data est transféré à BackgroundTask uniquement si le
 * démarrage réussit. En cas d’échec, l’appelant en reste propriétaire.
 *
 * @param task_manager Gestionnaire recevant la tâche.
 * @param request Paramètres de l’import.
 * @param completion_callback Callback final facultatif.
 * @param completion_data Données du callback final.
 * @param completion_data_destroy Destructeur de completion_data.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouvelle tâche démarrée, ou NULL.
 */
BackgroundTask *evidence_import_task_start(
    TaskManager *task_manager,
    const EvidenceImportTaskRequest *request,
    BackgroundTaskCompletionCallback completion_callback,
    gpointer completion_data,
    GDestroyNotify completion_data_destroy,
    GError **error
);

#endif
