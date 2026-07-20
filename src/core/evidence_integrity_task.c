/******************************************************************************
 * @file evidence_integrity_task.c
 * @brief Exécution asynchrone de la vérification d'intégrité.
 ******************************************************************************/

#include "core/evidence_integrity_task.h"

#include "core/evidence_integrity_verifier.h"

#include <gio/gio.h>
#include <glib.h>

/**
 * @brief Copie privée des paramètres nécessaires au worker.
 */
typedef struct
{
    char *investigation_root_path;
    char *relative_path;
    char *expected_sha256;
} EvidenceIntegrityTaskData;

/**
 * @brief Libère les données privées du worker.
 */
static void evidence_integrity_task_data_free(
    gpointer user_data
)
{
    EvidenceIntegrityTaskData *task_data =
        user_data;

    if (task_data == NULL)
    {
        return;
    }

    g_free(
        task_data->expected_sha256
    );

    g_free(
        task_data->relative_path
    );

    g_free(
        task_data->investigation_root_path
    );

    g_free(
        task_data
    );
}

/**
 * @brief Vérifie les paramètres obligatoires.
 */
static gboolean evidence_integrity_task_request_is_valid(
    const EvidenceIntegrityTaskRequest *request
)
{
    if (request == NULL)
    {
        return FALSE;
    }

    if (request->investigation_root_path == NULL ||
        request->investigation_root_path[0] == '\0')
    {
        return FALSE;
    }

    if (request->relative_path == NULL ||
        request->relative_path[0] == '\0')
    {
        return FALSE;
    }

    if (request->expected_sha256 == NULL ||
        request->expected_sha256[0] == '\0')
    {
        return FALSE;
    }

    return TRUE;
}

/**
 * @brief Copie les paramètres avant le lancement du thread.
 */
static EvidenceIntegrityTaskData *
evidence_integrity_task_data_new(
    const EvidenceIntegrityTaskRequest *request
)
{
    EvidenceIntegrityTaskData *task_data =
        NULL;

    if (!evidence_integrity_task_request_is_valid(
            request
        ))
    {
        return NULL;
    }

    task_data =
        g_try_new0(
            EvidenceIntegrityTaskData,
            1
        );

    if (task_data == NULL)
    {
        return NULL;
    }

    task_data->investigation_root_path =
        g_strdup(
            request->investigation_root_path
        );

    task_data->relative_path =
        g_strdup(
            request->relative_path
        );

    task_data->expected_sha256 =
        g_strdup(
            request->expected_sha256
        );

    return task_data;
}

/**
 * @brief Libère le résultat de vérification d'une tâche.
 */
static void evidence_integrity_task_result_free(
    gpointer user_data
)
{
    evidence_integrity_verification_result_free(
        user_data
    );
}

/**
 * @brief Traduit une annulation métier en annulation GIO.
 *
 * BackgroundTask reconnaît l'état CANCELLED uniquement avec :
 *
 * G_IO_ERROR / G_IO_ERROR_CANCELLED.
 */
static void evidence_integrity_task_propagate_verify_error(
    GError **error,
    GError *verify_error
)
{
    if (verify_error != NULL &&
        g_error_matches(
            verify_error,
            EVIDENCE_INTEGRITY_VERIFIER_ERROR,
            EVIDENCE_INTEGRITY_VERIFIER_ERROR_CANCELLED
        ))
    {
        g_clear_error(
            &verify_error
        );

        g_set_error_literal(
            error,
            G_IO_ERROR,
            G_IO_ERROR_CANCELLED,
            "La vérification d'intégrité a été annulée."
        );

        return;
    }

    if (verify_error != NULL)
    {
        g_propagate_error(
            error,
            verify_error
        );

        return;
    }

    g_set_error_literal(
        error,
        EVIDENCE_INTEGRITY_TASK_ERROR,
        EVIDENCE_INTEGRITY_TASK_ERROR_VERIFY,
        "La vérification a échoué sans fournir d'erreur."
    );
}

/**
 * @brief Retourne le message final correspondant au statut métier.
 */
static const char *evidence_integrity_task_get_status_message(
    EvidenceIntegrityStatus integrity_status
)
{
    switch (integrity_status)
    {
        case EVIDENCE_INTEGRITY_STATUS_VALID:
            return "Intégrité vérifiée : fichier intact";

        case EVIDENCE_INTEGRITY_STATUS_MODIFIED:
            return "Intégrité compromise : empreinte différente";

        case EVIDENCE_INTEGRITY_STATUS_MISSING:
            return "Vérification terminée : fichier absent";

        case EVIDENCE_INTEGRITY_STATUS_ERROR:
            return "Vérification terminée : erreur de lecture";

        case EVIDENCE_INTEGRITY_STATUS_UNKNOWN:
        default:
            return "Vérification terminée";
    }
}

/**
 * @brief Exécute la vérification dans le thread secondaire.
 */
static gboolean evidence_integrity_task_worker(
    BackgroundTask *task,
    GCancellable *cancellable,
    gpointer worker_data,
    gpointer *result,
    GError **error
)
{
    EvidenceIntegrityTaskData *task_data =
        worker_data;

    EvidenceIntegrityVerificationResult *verification_result =
        NULL;

    EvidenceIntegrityStatus integrity_status =
        EVIDENCE_INTEGRITY_STATUS_UNKNOWN;

    GError *verify_error =
        NULL;

    if (task == NULL ||
        cancellable == NULL ||
        task_data == NULL ||
        result == NULL ||
        error == NULL)
    {
        return FALSE;
    }

    *result =
        NULL;

    if (g_cancellable_set_error_if_cancelled(
            cancellable,
            error
        ))
    {
        return FALSE;
    }

    background_task_report_progress(
        task,
        0.10,
        "Préparation de la vérification"
    );

    verification_result =
        evidence_integrity_verifier_verify(
            task_data->investigation_root_path,
            task_data->relative_path,
            task_data->expected_sha256,
            cancellable,
            &verify_error
        );

    if (verification_result == NULL)
    {
        evidence_integrity_task_propagate_verify_error(
            error,
            verify_error
        );

        return FALSE;
    }

    integrity_status =
        evidence_integrity_verification_result_get_status(
            verification_result
        );

    background_task_report_progress(
        task,
        1.0,
        evidence_integrity_task_get_status_message(
            integrity_status
        )
    );

    *result =
        verification_result;

    return TRUE;
}

/**
 * @brief Transmet une erreur secondaire au code appelant.
 */
static void evidence_integrity_task_propagate_start_error(
    GError **error,
    GError *start_error,
    const char *prefix
)
{
    if (start_error == NULL)
    {
        g_set_error_literal(
            error,
            EVIDENCE_INTEGRITY_TASK_ERROR,
            EVIDENCE_INTEGRITY_TASK_ERROR_VERIFY,
            "La tâche de vérification n'a pas pu être démarrée."
        );

        return;
    }

    if (error != NULL)
    {
        g_propagate_prefixed_error(
            error,
            start_error,
            "%s",
            prefix
        );

        return;
    }

    g_clear_error(
        &start_error
    );
}

GQuark evidence_integrity_task_error_quark(void)
{
    return g_quark_from_static_string(
        "evidence-integrity-task-error-quark"
    );
}

BackgroundTask *evidence_integrity_task_start(
    TaskManager *task_manager,
    const EvidenceIntegrityTaskRequest *request,
    BackgroundTaskCompletionCallback completion_callback,
    gpointer completion_data,
    GDestroyNotify completion_data_destroy,
    GError **error
)
{
    EvidenceIntegrityTaskData *task_data =
        NULL;

    BackgroundTask *task =
        NULL;

    char *evidence_name =
        NULL;

    char *task_title =
        NULL;

    GError *start_error =
        NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (task_manager == NULL ||
        !evidence_integrity_task_request_is_valid(
            request
        ))
    {
        g_set_error_literal(
            error,
            EVIDENCE_INTEGRITY_TASK_ERROR,
            EVIDENCE_INTEGRITY_TASK_ERROR_INVALID_ARGUMENT,
            "Les paramètres de la tâche de vérification sont invalides."
        );

        return NULL;
    }

    task_data =
        evidence_integrity_task_data_new(
            request
        );

    if (task_data == NULL)
    {
        g_set_error_literal(
            error,
            EVIDENCE_INTEGRITY_TASK_ERROR,
            EVIDENCE_INTEGRITY_TASK_ERROR_VERIFY,
            "Impossible d'allouer les données de la tâche "
            "de vérification."
        );

        return NULL;
    }

    evidence_name =
        g_path_get_basename(
            request->relative_path
        );

    task_title =
        g_strdup_printf(
            "Vérifier l'intégrité : %s",
            evidence_name
        );

    task =
        background_task_new(
            task_title
        );

    g_free(
        task_title
    );

    g_free(
        evidence_name
    );

    if (task == NULL)
    {
        evidence_integrity_task_data_free(
            task_data
        );

        g_set_error_literal(
            error,
            EVIDENCE_INTEGRITY_TASK_ERROR,
            EVIDENCE_INTEGRITY_TASK_ERROR_VERIFY,
            "Impossible de créer la tâche de vérification."
        );

        return NULL;
    }

    if (!task_manager_add(
            task_manager,
            task,
            &start_error
        ))
    {
        evidence_integrity_task_data_free(
            task_data
        );

        background_task_unref(
            task
        );

        evidence_integrity_task_propagate_start_error(
            error,
            start_error,
            "Impossible d'ajouter la tâche : "
        );

        return NULL;
    }

    if (!background_task_start(
            task,
            evidence_integrity_task_worker,
            task_data,
            evidence_integrity_task_data_free,
            evidence_integrity_task_result_free,
            completion_callback,
            completion_data,
            completion_data_destroy,
            &start_error
        ))
    {
        /*
         * background_task_start() n'a pas pris possession
         * de task_data ni de completion_data en cas d'échec.
         */
        evidence_integrity_task_data_free(
            task_data
        );

        task_manager_remove(
            task_manager,
            task
        );

        background_task_unref(
            task
        );

        evidence_integrity_task_propagate_start_error(
            error,
            start_error,
            "Impossible de démarrer la tâche : "
        );

        return NULL;
    }

    return task;
}

