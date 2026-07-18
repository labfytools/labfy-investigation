/******************************************************************************
 * @file tool_initializer.c
 * @brief Initialisation asynchrone des outils externes.
 ******************************************************************************/

#include "core/tool_initializer.h"
#include "core/tool_catalog.h"

#include <gio/gio.h>
#include <glib.h>

/**
 * @struct ToolInitializer
 * @brief État interne de l’initialiseur d’outils.
 */
struct ToolInitializer
{
    gatomicrefcount reference_count;
    GMutex mutex;

    TaskManager *task_manager;
    ToolRegistry *tool_registry;

    BackgroundTask *task;

    ToolInitializationSummary last_summary;

    gboolean has_last_summary;
    gboolean is_running;
};

static gboolean tool_initializer_ensure_catalog_registered(
    ToolInitializer *tool_initializer,
    GError **error
);

/**
 * @brief Ajoute une référence interne à l’initialiseur.
 *
 * @param tool_initializer Initialiseur concerné.
 *
 * @return L’initialiseur fourni, ou NULL.
 */
static ToolInitializer *tool_initializer_ref_internal(
    ToolInitializer *tool_initializer
)
{
    if (tool_initializer == NULL)
    {
        return NULL;
    }

    g_atomic_ref_count_inc(
        &tool_initializer->reference_count
    );

    return tool_initializer;
}

/**
 * @brief Libère une référence interne à l’initialiseur.
 *
 * @param tool_initializer Initialiseur concerné.
 */
static void tool_initializer_unref_internal(
    ToolInitializer *tool_initializer
)
{
    BackgroundTask *task = NULL;
    ToolRegistry *tool_registry = NULL;

    if (tool_initializer == NULL)
    {
        return;
    }

    if (!g_atomic_ref_count_dec(
        &tool_initializer->reference_count
    ))
    {
        return;
    }

    g_mutex_lock(
        &tool_initializer->mutex
    );

    task = tool_initializer->task;
    tool_registry = tool_initializer->tool_registry;

    tool_initializer->task = NULL;
    tool_initializer->tool_registry = NULL;
    tool_initializer->task_manager = NULL;
    tool_initializer->is_running = FALSE;

    g_mutex_unlock(
        &tool_initializer->mutex
    );

    background_task_unref(
        task
    );

    tool_registry_free(
        tool_registry
    );

    g_mutex_clear(
        &tool_initializer->mutex
    );

    g_free(
        tool_initializer
    );
}

/**
 * @brief Adapte tool_initializer_unref_internal() à GDestroyNotify.
 *
 * @param user_data Initialiseur concerné.
 */
static void tool_initializer_unref_notify(
    gpointer user_data
)
{
    tool_initializer_unref_internal(
        user_data
    );
}

/**
 * @brief Produit une erreur ToolInitializer à partir d’une erreur interne.
 *
 * @param error Emplacement recevant l’erreur.
 * @param error_code Code ToolInitializer.
 * @param message Message principal.
 * @param cause Erreur interne facultative.
 */
static void tool_initializer_set_wrapped_error(
    GError **error,
    ToolInitializerError error_code,
    const char *message,
    const GError *cause
)
{
    if (cause != NULL)
    {
        g_set_error(
            error,
            TOOL_INITIALIZER_ERROR,
            error_code,
            "%s : %s",
            message,
            cause->message
        );

        return;
    }

    g_set_error_literal(
        error,
        TOOL_INITIALIZER_ERROR,
        error_code,
        message
    );
}

/**
 * @brief Enregistre, recherche et interroge les outils externes.
 */
static gboolean tool_initializer_bootstrap_worker(
    BackgroundTask *task,
    GCancellable *cancellable,
    gpointer worker_data,
    gpointer *result,
    GError **error
)
{
    ToolInitializer *tool_initializer =
        worker_data;

    ToolRegistry *tool_registry = NULL;

    ToolInitializationSummary *summary = NULL;

    gsize tool_count = 0;
    gsize tool_index = 0;

    GError *local_error = NULL;

    if (task == NULL ||
        cancellable == NULL ||
        tool_initializer == NULL ||
        result == NULL)
    {
        g_set_error_literal(
            error,
            TOOL_INITIALIZER_ERROR,
            TOOL_INITIALIZER_ERROR_INTERNAL_STATE,
            "Le contexte du worker d’initialisation est invalide."
        );

        return FALSE;
    }

    tool_registry =
        tool_initializer->tool_registry;

    if (tool_registry == NULL)
    {
        g_set_error_literal(
            error,
            TOOL_INITIALIZER_ERROR,
            TOOL_INITIALIZER_ERROR_INTERNAL_STATE,
            "Le registre interne des outils est invalide."
        );

        return FALSE;
    }

    if (g_cancellable_set_error_if_cancelled(
        cancellable,
        error
    ))
    {
        return FALSE;
    }

    background_task_report_progress(
        task,
        0.05,
        "Enregistrement du catalogue des outils"
    );

    if (!tool_initializer_ensure_catalog_registered(
        tool_initializer,
        error
    ))
    {
        return FALSE;
    }

    if (g_cancellable_set_error_if_cancelled(
        cancellable,
        error
    ))
    {
        return FALSE;
    }

    background_task_report_progress(
        task,
        0.15,
        "Recherche des exécutables installés"
    );

    if (!tool_registry_refresh(
        tool_registry,
        &local_error
    ))
    {
        tool_initializer_set_wrapped_error(
            error,
            TOOL_INITIALIZER_ERROR_REGISTRY_REFRESH,
            "La recherche des exécutables a échoué",
            local_error
        );

        g_clear_error(
            &local_error
        );

        return FALSE;
    }

    summary = g_new0(
        ToolInitializationSummary,
        1
    );

    tool_count =
        tool_catalog_get_count();

    summary->total_count =
        tool_count;

    for (tool_index = 0;
         tool_index < tool_count;
         tool_index++)
    {
        const ToolCatalogEntry *catalog_entry = NULL;
        const ToolInfo *tool_info = NULL;

        const char *identifier = NULL;
        const char *display_name = NULL;

        char *detected_version = NULL;
        char *status_message = NULL;

        double progress = 0.0;

        if (g_cancellable_set_error_if_cancelled(
            cancellable,
            error
        ))
        {
            g_free(
                summary
            );

            return FALSE;
        }

        catalog_entry =
            tool_catalog_get_entry(
                tool_index
            );

        if (catalog_entry != NULL)
        {
            identifier =
                tool_catalog_entry_get_identifier(
                    catalog_entry
                );

            display_name =
                tool_catalog_entry_get_display_name(
                    catalog_entry
                );
        }

        if (catalog_entry == NULL ||
            identifier == NULL ||
            display_name == NULL)
        {
            g_set_error(
                error,
                TOOL_INITIALIZER_ERROR,
                TOOL_INITIALIZER_ERROR_INTERNAL_STATE,
                "L’entrée de catalogue numéro %"
                G_GSIZE_FORMAT
                " est invalide.",
                tool_index
            );

            g_free(
                summary
            );

            return FALSE;
        }

        status_message =
            g_strdup_printf(
                "Vérification de %s",
                display_name
            );

        progress =
            0.15 +
            (
                0.80 *
                (
                    (double) tool_index /
                    (double) tool_count
                )
            );

        background_task_report_progress(
            task,
            progress,
            status_message
        );

        g_free(
            status_message
        );

        tool_info =
            tool_registry_find(
                tool_registry,
                identifier
            );

        if (tool_info == NULL)
        {
            g_set_error(
                error,
                TOOL_INITIALIZER_ERROR,
                TOOL_INITIALIZER_ERROR_INTERNAL_STATE,
                "L’outil '%s' est absent du registre.",
                identifier
            );

            g_free(
                summary
            );

            return FALSE;
        }

        switch (tool_info_get_availability(
            tool_info
        ))
        {
            case TOOL_AVAILABILITY_MISSING:
                summary->missing_count++;

                /*
                 * Une nouvelle détection peut rendre absent un outil
                 * précédemment disponible. Sa version devient alors
                 * obsolète et doit être effacée.
                 */
                if (!tool_registry_set_version(
                    tool_registry,
                    identifier,
                    NULL,
                    &local_error
                ))
                {
                    tool_initializer_set_wrapped_error(
                        error,
                        TOOL_INITIALIZER_ERROR_INTERNAL_STATE,
                        "La version obsolète n’a pas pu être effacée",
                        local_error
                    );

                    g_clear_error(
                        &local_error
                    );

                    g_free(
                        summary
                    );

                    return FALSE;
                }

                break;

            case TOOL_AVAILABILITY_AVAILABLE:
                summary->available_count++;

                if (tool_catalog_detect_version(
                    tool_registry,
                    identifier,
                    cancellable,
                    &detected_version,
                    &local_error
                ))
                {
                    if (!tool_registry_set_version(
                        tool_registry,
                        identifier,
                        detected_version,
                        &local_error
                    ))
                    {
                        tool_initializer_set_wrapped_error(
                            error,
                            TOOL_INITIALIZER_ERROR_INTERNAL_STATE,
                            "La version détectée n’a pas pu être enregistrée",
                            local_error
                        );

                        g_clear_error(
                            &local_error
                        );

                        g_free(
                            detected_version
                        );

                        g_free(
                            summary
                        );

                        return FALSE;
                    }

                    summary->version_detected_count++;
                }
                else
                {
                    /*
                     * Une annulation arrête toute la tâche.
                     *
                     * Une erreur propre à la commande de version reste
                     * locale à l’outil : l’exécutable demeure disponible.
                     */
                    if (g_error_matches(
                        local_error,
                        G_IO_ERROR,
                        G_IO_ERROR_CANCELLED
                    ))
                    {
                        g_propagate_error(
                            error,
                            local_error
                        );

                        g_free(
                            detected_version
                        );

                        g_free(
                            summary
                        );

                        return FALSE;
                    }

                    g_clear_error(
                        &local_error
                    );

                    if (!tool_registry_set_version(
                        tool_registry,
                        identifier,
                        NULL,
                        &local_error
                    ))
                    {
                        tool_initializer_set_wrapped_error(
                            error,
                            TOOL_INITIALIZER_ERROR_INTERNAL_STATE,
                            "La version précédente n’a pas pu être effacée",
                            local_error
                        );

                        g_clear_error(
                            &local_error
                        );

                        g_free(
                            detected_version
                        );

                        g_free(
                            summary
                        );

                        return FALSE;
                    }

                    summary->version_failed_count++;
                }

                g_free(
                    detected_version
                );

                break;

            case TOOL_AVAILABILITY_UNKNOWN:
            default:
                g_set_error(
                    error,
                    TOOL_INITIALIZER_ERROR,
                    TOOL_INITIALIZER_ERROR_INTERNAL_STATE,
                    "La disponibilité de l’outil '%s' est incohérente.",
                    identifier
                );

                g_free(
                    summary
                );

                return FALSE;
        }
    }

    if (g_cancellable_set_error_if_cancelled(
        cancellable,
        error
    ))
    {
        g_free(
            summary
        );

        return FALSE;
    }

    {
        char *summary_message = NULL;

        summary_message =
            g_strdup_printf(
                "Initialisation terminée : %"
                G_GSIZE_FORMAT
                " disponibles, %"
                G_GSIZE_FORMAT
                " absents, %"
                G_GSIZE_FORMAT
                " versions détectées, %"
                G_GSIZE_FORMAT
                " non détectées",
                summary->available_count,
                summary->missing_count,
                summary->version_detected_count,
                summary->version_failed_count
            );

        background_task_report_progress(
            task,
            1.0,
            summary_message
        );

        g_free(
            summary_message
        );
    }

    *result = summary;

    return TRUE;
}

/**
 * @brief Garantit que le registre contient exactement le catalogue initial.
 *
 * Le premier démarrage enregistre les outils.
 * Les démarrages suivants réutilisent le registre existant.
 *
 * @param tool_initializer Initialiseur concerné.
 * @param error Emplacement recevant une erreur.
 *
 * @return TRUE si le registre est prêt.
 */
static gboolean tool_initializer_ensure_catalog_registered(
    ToolInitializer *tool_initializer,
    GError **error
)
{
    ToolRegistry *tool_registry = NULL;

    gsize registry_count = 0;
    gsize catalog_count = 0;
    gsize entry_index = 0;

    GError *local_error = NULL;

    if (tool_initializer == NULL ||
        tool_initializer->tool_registry == NULL)
    {
        g_set_error_literal(
            error,
            TOOL_INITIALIZER_ERROR,
            TOOL_INITIALIZER_ERROR_INTERNAL_STATE,
            "Le registre interne des outils est invalide."
        );

        return FALSE;
    }

    tool_registry =
        tool_initializer->tool_registry;

    registry_count =
        tool_registry_get_count(
            tool_registry
        );

    catalog_count =
        tool_catalog_get_count();

    if (registry_count == 0)
    {
        if (!tool_catalog_register_defaults(
            tool_registry,
            &local_error
        ))
        {
            tool_initializer_set_wrapped_error(
                error,
                TOOL_INITIALIZER_ERROR_CATALOG_REGISTRATION,
                "Le catalogue d’outils n’a pas pu être enregistré",
                local_error
            );

            g_clear_error(
                &local_error
            );

            return FALSE;
        }

        return TRUE;
    }

    /*
     * Le registre est conservé entre deux initialisations.
     *
     * Un registre partiellement rempli indique une incohérence,
     * car tool_catalog_register_defaults() est atomique.
     */
    if (registry_count != catalog_count)
    {
        g_set_error(
            error,
            TOOL_INITIALIZER_ERROR,
            TOOL_INITIALIZER_ERROR_CATALOG_REGISTRATION,
            "Le registre contient %" G_GSIZE_FORMAT
            " outils alors que le catalogue en contient %"
            G_GSIZE_FORMAT ".",
            registry_count,
            catalog_count
        );

        return FALSE;
    }

    for (entry_index = 0;
         entry_index < catalog_count;
         entry_index++)
    {
        const ToolCatalogEntry *catalog_entry = NULL;
        const char *identifier = NULL;

        catalog_entry =
            tool_catalog_get_entry(
                entry_index
            );

        
        if (catalog_entry != NULL)
        {
            identifier =
                tool_catalog_entry_get_identifier(
                    catalog_entry
                );
        }

        if (catalog_entry == NULL ||
            identifier == NULL ||
            tool_registry_find(
                tool_registry,
                identifier
            ) == NULL)
        {
            g_set_error(
                error,
                TOOL_INITIALIZER_ERROR,
                TOOL_INITIALIZER_ERROR_CATALOG_REGISTRATION,
                "L’entrée de catalogue numéro %"
                G_GSIZE_FORMAT
                " est absente ou invalide.",
                entry_index
            );

            return FALSE;
        }
    }

    return TRUE;
}

/**
 * @brief Finalise l’état de ToolInitializer sur le contexte principal.
 *
 * @param task Tâche terminée.
 * @param user_data Initialiseur concerné.
 */
static void tool_initializer_bootstrap_completed(
    BackgroundTask *task,
    gpointer user_data
)
{
    ToolInitializer *tool_initializer =
        user_data;

    const ToolInitializationSummary *summary =
        NULL;

    if (task == NULL ||
        tool_initializer == NULL)
    {
        return;
    }

    if (background_task_get_state(
        task
    ) == BACKGROUND_TASK_STATE_COMPLETED)
    {
        summary = background_task_get_result(
            task
        );
    }

    g_mutex_lock(
        &tool_initializer->mutex
    );

    if (summary != NULL)
    {
        tool_initializer->last_summary =
            *summary;

        tool_initializer->has_last_summary =
            TRUE;
    }

    tool_initializer->is_running = FALSE;

    g_mutex_unlock(
        &tool_initializer->mutex
    );
}

GQuark tool_initializer_error_quark(void)
{
    return g_quark_from_static_string(
        "labfy-investigation-tool-initializer-error"
    );
}

ToolInitializer *tool_initializer_new(
    TaskManager *task_manager,
    GError **error
)
{
    ToolInitializer *tool_initializer = NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (task_manager == NULL)
    {
        g_set_error_literal(
            error,
            TOOL_INITIALIZER_ERROR,
            TOOL_INITIALIZER_ERROR_INVALID_ARGUMENT,
            "Le gestionnaire de tâches est invalide."
        );

        return NULL;
    }

    tool_initializer = g_new0(
        ToolInitializer,
        1
    );

    g_atomic_ref_count_init(
        &tool_initializer->reference_count
    );

    g_mutex_init(
        &tool_initializer->mutex
    );

    tool_initializer->task_manager =
        task_manager;

    tool_initializer->tool_registry =
        tool_registry_new();

    tool_initializer->task = NULL;
    tool_initializer->has_last_summary = FALSE;
    tool_initializer->is_running = FALSE;

    return tool_initializer;
}

void tool_initializer_free(
    ToolInitializer *tool_initializer
)
{
    BackgroundTask *task = NULL;

    if (tool_initializer == NULL)
    {
        return;
    }

    task = tool_initializer_get_task(
        tool_initializer
    );

    if (task != NULL)
    {
        background_task_cancel(
            task
        );

        background_task_unref(
            task
        );
    }

    tool_initializer_unref_internal(
        tool_initializer
    );
}

ToolRegistry *tool_initializer_get_registry(
    ToolInitializer *tool_initializer
)
{
    if (tool_initializer == NULL)
    {
        return NULL;
    }

    return tool_initializer->tool_registry;
}

gboolean tool_initializer_start(
    ToolInitializer *tool_initializer,
    GError **error
)
{
    ToolInitializationSummary previous_summary = {0};
    gboolean previous_has_last_summary = FALSE;
    BackgroundTask *new_task = NULL;
    BackgroundTask *previous_task = NULL;

    ToolInitializer *worker_reference = NULL;
    ToolInitializer *completion_reference = NULL;

    TaskManager *task_manager = NULL;

    GError *local_error = NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (tool_initializer == NULL)
    {
        g_set_error_literal(
            error,
            TOOL_INITIALIZER_ERROR,
            TOOL_INITIALIZER_ERROR_INVALID_ARGUMENT,
            "L’initialiseur d’outils est invalide."
        );

        return FALSE;
    }

    /*
     * is_running sert également d’état STARTING.
     *
     * Cela empêche deux appels simultanés de créer chacun une tâche.
     */
    g_mutex_lock(
        &tool_initializer->mutex
    );

    if (tool_initializer->is_running)
    {
        g_mutex_unlock(
            &tool_initializer->mutex
        );

        g_set_error_literal(
            error,
            TOOL_INITIALIZER_ERROR,
            TOOL_INITIALIZER_ERROR_ALREADY_RUNNING,
            "Une initialisation des outils est déjà en cours."
        );

        return FALSE;
    }

    previous_summary =
        tool_initializer->last_summary;

    previous_has_last_summary =
        tool_initializer->has_last_summary;

    tool_initializer->last_summary =
        (ToolInitializationSummary) {0};

    tool_initializer->has_last_summary =
        FALSE;

    tool_initializer->is_running = TRUE;
    task_manager = tool_initializer->task_manager;

    g_mutex_unlock(
        &tool_initializer->mutex
    );

    new_task = background_task_new(
        "Initialisation des outils externes"
    );

    if (new_task == NULL)
    {
        g_mutex_lock(
            &tool_initializer->mutex
        );

        tool_initializer->is_running = FALSE;

        tool_initializer->last_summary =
            previous_summary;

        tool_initializer->has_last_summary =
            previous_has_last_summary;
        
        g_mutex_unlock(
            &tool_initializer->mutex
        );

        g_set_error_literal(
            error,
            TOOL_INITIALIZER_ERROR,
            TOOL_INITIALIZER_ERROR_TASK_CREATION,
            "La tâche d’initialisation n’a pas pu être créée."
        );

        return FALSE;
    }

    if (!task_manager_add(
        task_manager,
        new_task,
        &local_error
    ))
    {
        g_mutex_lock(
            &tool_initializer->mutex
        );

        tool_initializer->is_running = FALSE;

        g_mutex_unlock(
            &tool_initializer->mutex
        );

        tool_initializer->last_summary =
            previous_summary;

        tool_initializer->has_last_summary =
            previous_has_last_summary;  

        tool_initializer_set_wrapped_error(
            error,
            TOOL_INITIALIZER_ERROR_TASK_REGISTRATION,
            "La tâche d’initialisation n’a pas pu être enregistrée",
            local_error
        );

        g_clear_error(
            &local_error
        );

        background_task_unref(
            new_task
        );

        return FALSE;
    }

    /*
     * La référence initiale de new_task devient la référence possédée
     * par ToolInitializer.
     */
    g_mutex_lock(
        &tool_initializer->mutex
    );

    previous_task = tool_initializer->task;
    tool_initializer->task = new_task;

    g_mutex_unlock(
        &tool_initializer->mutex
    );

    worker_reference =
        tool_initializer_ref_internal(
            tool_initializer
        );

    completion_reference =
        tool_initializer_ref_internal(
            tool_initializer
        );

    if (!background_task_start(
        new_task,
        tool_initializer_bootstrap_worker,
        worker_reference,
        tool_initializer_unref_notify,
        g_free,
        tool_initializer_bootstrap_completed,
        completion_reference,
        tool_initializer_unref_notify,
        &local_error
    ))
    {
        /*
         * En cas d’échec, BackgroundTask ne possède ni worker_reference
         * ni completion_reference.
         */
        tool_initializer_unref_internal(
            worker_reference
        );

        tool_initializer_unref_internal(
            completion_reference
        );

        g_mutex_lock(
            &tool_initializer->mutex
        );

        tool_initializer->last_summary =
            previous_summary;

        tool_initializer->has_last_summary =
            previous_has_last_summary;

        tool_initializer->task =
            previous_task;

        tool_initializer->is_running =
            FALSE;

        g_mutex_unlock(
            &tool_initializer->mutex
        );

        task_manager_remove(
            task_manager,
            new_task
        );

        tool_initializer_set_wrapped_error(
            error,
            TOOL_INITIALIZER_ERROR_TASK_START,
            "La tâche d’initialisation n’a pas pu être démarrée",
            local_error
        );

        g_clear_error(
            &local_error
        );

        g_mutex_lock(
            &tool_initializer->mutex
        );

        tool_initializer->last_summary =
            (ToolInitializationSummary) {0};

        tool_initializer->has_last_summary =
            FALSE;

        g_mutex_unlock(
            &tool_initializer->mutex
        );

        background_task_unref(
            new_task
        );

        return FALSE;
    }

    /*
     * L’ancienne tâche n’est plus la tâche courante.
     */
    background_task_unref(
        previous_task
    );

    return TRUE;
}

gboolean tool_initializer_cancel(
    ToolInitializer *tool_initializer,
    GError **error
)
{
    BackgroundTask *task = NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (tool_initializer == NULL)
    {
        g_set_error_literal(
            error,
            TOOL_INITIALIZER_ERROR,
            TOOL_INITIALIZER_ERROR_INVALID_ARGUMENT,
            "L’initialiseur d’outils est invalide."
        );

        return FALSE;
    }

    g_mutex_lock(
        &tool_initializer->mutex
    );

    if (tool_initializer->is_running &&
        tool_initializer->task != NULL)
    {
        task = background_task_ref(
            tool_initializer->task
        );
    }

    g_mutex_unlock(
        &tool_initializer->mutex
    );

    if (task == NULL)
    {
        g_set_error_literal(
            error,
            TOOL_INITIALIZER_ERROR,
            TOOL_INITIALIZER_ERROR_NOT_RUNNING,
            "Aucune initialisation des outils n’est en cours."
        );

        return FALSE;
    }

    background_task_cancel(
        task
    );

    background_task_unref(
        task
    );

    return TRUE;
}

gboolean tool_initializer_is_running(
    const ToolInitializer *tool_initializer
)
{
    ToolInitializer *mutable_initializer = NULL;
    gboolean is_running = FALSE;

    if (tool_initializer == NULL)
    {
        return FALSE;
    }

    mutable_initializer =
        (ToolInitializer *) tool_initializer;

    g_mutex_lock(
        &mutable_initializer->mutex
    );

    is_running =
        mutable_initializer->is_running;

    g_mutex_unlock(
        &mutable_initializer->mutex
    );

    return is_running;
}

BackgroundTask *tool_initializer_get_task(
    const ToolInitializer *tool_initializer
)
{
    ToolInitializer *mutable_initializer = NULL;
    BackgroundTask *task = NULL;

    if (tool_initializer == NULL)
    {
        return NULL;
    }

    mutable_initializer =
        (ToolInitializer *) tool_initializer;

    g_mutex_lock(
        &mutable_initializer->mutex
    );

    if (mutable_initializer->task != NULL)
    {
        task = background_task_ref(
            mutable_initializer->task
        );
    }

    g_mutex_unlock(
        &mutable_initializer->mutex
    );

    return task;
}

gboolean tool_initializer_get_last_summary(
    const ToolInitializer *tool_initializer,
    ToolInitializationSummary *out_summary
)
{
    ToolInitializer *mutable_initializer = NULL;
    gboolean has_last_summary = FALSE;

    if (tool_initializer == NULL ||
        out_summary == NULL)
    {
        return FALSE;
    }

    mutable_initializer =
        (ToolInitializer *) tool_initializer;

    g_mutex_lock(
        &mutable_initializer->mutex
    );

    has_last_summary =
        mutable_initializer->has_last_summary;

    if (has_last_summary)
    {
        *out_summary =
            mutable_initializer->last_summary;
    }

    g_mutex_unlock(
        &mutable_initializer->mutex
    );

    return has_last_summary;
}
