/******************************************************************************
 * @file application.c
 * @brief Implémentation du cycle de vie de l'application GTK.
 ******************************************************************************/

#include "core/application.h"

#include "core/investigation_node.h"
#include "core/investigation_project.h"
#include "core/investigation_session.h"
#include "core/investigation_tree_model.h"
#include "core/investigation_graph_load_task.h"
#include "models/investigation_graph_layout.h"
#include "models/investigation_graph_model.h"
#include "models/investigation_record.h"
#include "core/investigation_tree_builder.h"
#include "views/create_investigation_dialog.h"
#include "views/folder_dialog.h"
#include "views/main_window.h"
#include "views/application_message_dialog.h"
#include "core/task_manager.h"
#include "core/background_task.h"
#include "core/tool_initializer.h"
#include "views/file_dialog.h"
#include "core/evidence_import_task.h"
#include "models/evidence_record.h"
#include "dao/evidence_type_dao.h"
#include "dao/graph_node_position_dao.h"
#include "views/evidence_import_dialog.h"
#include "views/create_relation_dialog.h"
#include "dao/evidence_dao.h"
#include "dao/entity_dao.h"
#include "models/evidence_type.h"
#include "widgets/evidence_list_model.h"
#include "widgets/evidence_category_model.h"
#include "core/evidence_integrity_task.h"
#include "core/evidence_integrity_verifier.h"
#include "core/relation_service.h"
#include "database/database.h"

#include <gtk/gtk.h>
#include <errno.h>

/**
 * @brief Identifiant unique utilisé par GLib pour l'application.
 */
#define APPLICATION_ID "com.labfytools.investigation"

/**
 * @brief Erreurs produites lors du chargement d'une enquête.
 */
typedef enum
{
    APPLICATION_OPEN_ERROR_INVALID_ARGUMENT,
    APPLICATION_OPEN_ERROR_INVALID_PROJECT,
    APPLICATION_OPEN_ERROR_TREE_BUILD,
    APPLICATION_OPEN_ERROR_INSTALL,
    APPLICATION_OPEN_ERROR_EVIDENCE
} ApplicationOpenError;

/**
 * @brief Domaine d'erreur du chargement d'une enquête.
 */
#define APPLICATION_OPEN_ERROR \
    application_open_error_quark()

/**
 * @brief Dossier racine contenant les preuves originales.
 */
#define APPLICATION_EVIDENCE_ROOT_DIRECTORY \
    "01_Preuves_Originales"

/**
 * @brief Retourne le domaine d'erreur du chargement d'une enquête.
 *
 * @return Quark GLib du domaine d'erreur.
 */
static GQuark application_open_error_quark(void)
{
    return g_quark_from_static_string(
        "labfy-investigation-application-open-error"
    );
}

/**
 * @brief État interne de l'application.
 *
 * Cette structure reste privée au module afin d'empêcher les autres
 * composants de manipuler directement les objets GTK.
 */
struct Application
{
    GtkApplication *gtk_application;
    MainWindow *main_window;
    InvestigationSession *session;
    InvestigationTreeModel *tree_model;
    TaskManager *task_manager;
    ToolInitializer *tool_initializer;
    EvidenceListModel *evidence_list_model;
    EvidenceCategoryModel *evidence_category_model;

    InvestigationGraphLoadTask *graph_load_task;
    InvestigationGraphModel *graph_model;
    InvestigationGraphLayout *graph_layout;
    guint64 graph_load_generation;

    char *selected_evidence_identifier;
};

/**
 * @brief Contexte ApplicationOpenErrorconservé jusqu’à la fin d’un import.
 */
typedef struct
{
    Application *application;
    char *investigation_root_path;
} ApplicationEvidenceImportContext;

/**
 * @brief Contexte conservé jusqu'à la fin d'un chargement de graphe.
 */
typedef struct
{
    Application *application;
    guint64 generation;
} ApplicationGraphLoadContext;

/**
 * @brief Contexte conservé pendant le dialogue de métadonnées.
 *
 * Le chemin fourni par FileDialog n'est valide que pendant son callback.
 * Il doit donc être copié avant l'ouverture du second dialogue.
 */
typedef struct
{
    Application *application;
    char *file_path;
} ApplicationEvidenceDialogContext;

/**
 * @brief Contexte conservé jusqu'à la fin d'une vérification.
 */
typedef struct
{
    Application *application;

    char *investigation_root_path;
    char *database_path;

    char *evidence_identifier;
    char *original_name;
} ApplicationEvidenceIntegrityContext;

/**
 * @brief Libère le contexte du dialogue de métadonnées.
 */
static void application_evidence_dialog_context_free(
    gpointer user_data
)
{
    ApplicationEvidenceDialogContext *context =
        user_data;

    if (context == NULL)
    {
        return;
    }

    g_free(
        context->file_path
    );

    g_free(
        context
    );
}

/**
 * @brief Libère le contexte d’un import asynchrone.
 */
static void application_evidence_import_context_free(
    gpointer user_data
)
{
    ApplicationEvidenceImportContext *context =
        user_data;

    if (context == NULL)
    {
        return;
    }

    g_free(
        context->investigation_root_path
    );

    g_free(
        context
    );
}

/**
 * @brief Libère le contexte d'une vérification d'intégrité.
 */
static void application_evidence_integrity_context_free(
    gpointer user_data
)
{
    ApplicationEvidenceIntegrityContext *context =
        user_data;

    if (context == NULL)
    {
        return;
    }

    g_free(
        context->original_name
    );

    g_free(
        context->evidence_identifier
    );

    g_free(
        context->database_path
    );

    g_free(
        context->investigation_root_path
    );

    g_free(
        context
    );
}

/**
 * @brief Libère le contexte d'un chargement de graphe.
 */
static void application_graph_load_context_free(
    gpointer user_data
)
{
    g_free(
        user_data
    );
}

/**
 * @brief Annule et détache le chargement de graphe courant.
 *
 * Chaque appel invalide les résultats appartenant à la génération
 * précédente, même lorsqu'aucune tâche n'est encore active.
 */
static void application_cancel_graph_loading(
    Application *application
)
{
    InvestigationGraphLoadTask *load_task =
        NULL;

    if (application == NULL)
    {
        return;
    }

    application->graph_load_generation++;

    load_task =
        application->graph_load_task;

    application->graph_load_task =
        NULL;

    if (load_task == NULL)
    {
        return;
    }

    investigation_graph_load_task_cancel(
        load_task
    );

    investigation_graph_load_task_free(
        load_task
    );
}

/**
 * @brief Détache puis libère le graphe possédé par Application.
 */
static void application_clear_graph(
    Application *application
)
{
    InvestigationGraphModel *graph_model =
        NULL;

    InvestigationGraphLayout *graph_layout =
        NULL;

    if (application == NULL)
    {
        return;
    }

    if (application->main_window != NULL)
    {
        main_window_clear_graph(
            application->main_window
        );
    }

    graph_model =
        application->graph_model;

    graph_layout =
        application->graph_layout;

    application->graph_model =
        NULL;

    application->graph_layout =
        NULL;

    investigation_graph_model_free(
        graph_model
    );

    investigation_graph_layout_free(
        graph_layout
    );
}

/**
 * @brief Traite le résultat d'un chargement asynchrone de graphe.
 *
 * Le callback est exécuté sur le contexte principal GLib.
 */
static void application_on_graph_loaded(
    InvestigationGraphLoadTask *load_task,
    InvestigationGraphModel *graph_model,
    const GError *error,
    gpointer user_data
)
{
    ApplicationGraphLoadContext *context =
        user_data;

    Application *application =
        NULL;

    InvestigationGraphLayout *graph_layout =
        NULL;

    char *status_message =
        NULL;

    gboolean is_current_result =
        FALSE;

    if (context != NULL)
    {
        application =
            context->application;
    }

    graph_layout =
        investigation_graph_load_task_take_layout(
            load_task
        );

    if (application != NULL)
    {
        is_current_result =
            load_task ==
                application->graph_load_task &&
            context->generation ==
                application->graph_load_generation;
    }

    /*
     * Un résultat appartenant à une ancienne enquête ne doit jamais
     * modifier l'interface actuelle.
     */
    if (!is_current_result)
    {
        investigation_graph_model_free(
            graph_model
        );

        investigation_graph_layout_free(
            graph_layout
        );

        investigation_graph_load_task_free(
            load_task
        );

        return;
    }

    application->graph_load_task =
        NULL;

    if (error != NULL)
    {
        investigation_graph_model_free(
            graph_model
        );

        investigation_graph_layout_free(
            graph_layout
        );

        main_window_set_graph_error(
            application->main_window,
            error->message
        );

        main_window_set_status(
            application->main_window,
            "Le graphe de l'enquête n'a pas pu être chargé."
        );

        investigation_graph_load_task_free(
            load_task
        );

        return;
    }

    if (graph_model == NULL ||
        graph_layout == NULL)
    {
        investigation_graph_model_free(
            graph_model
        );

        investigation_graph_layout_free(
            graph_layout
        );

        main_window_set_graph_error(
            application->main_window,
            "Le chargement s'est terminé sans fournir "
            "un graphe et une disposition valides."
        );

        main_window_set_status(
            application->main_window,
            "Le graphe de l'enquête est indisponible."
        );

        investigation_graph_load_task_free(
            load_task
        );

        return;
    }

    /*
     * MainWindow et Workspace doivent être détachés de l'ancien modèle
     * avant sa libération.
     */
    application_clear_graph(
        application
    );

    application->graph_model =
        graph_model;

    application->graph_layout =
        graph_layout;

    main_window_set_graph(
        application->main_window,
        application->graph_model,
        application->graph_layout
    );

    status_message =
        g_strdup_printf(
            "Graphe chargé : %" G_GUINT64_FORMAT
            " entité(s), %" G_GUINT64_FORMAT
            " relation(s), %u position(s) restaurée(s).",
            investigation_graph_model_get_entity_count(
                application->graph_model
            ),
            investigation_graph_model_get_relation_count(
                application->graph_model
            ),
            investigation_graph_layout_get_count(
                application->graph_layout
            )
        );

    main_window_set_status(
        application->main_window,
        status_message != NULL
            ? status_message
            : "Graphe chargé."
    );

    g_free(
        status_message
    );

    investigation_graph_load_task_free(
        load_task
    );
}

/**
 * @brief Démarre le chargement asynchrone du graphe d'une enquête.
 */
static void application_start_graph_loading(
    Application *application,
    const char *database_path
)
{
    InvestigationGraphLoadTask *load_task =
        NULL;

    ApplicationGraphLoadContext *context =
        NULL;

    GError *error =
        NULL;

    if (application == NULL ||
        application->main_window == NULL)
    {
        return;
    }

    /*
     * Cette opération invalide l'ancienne génération et libère
     * l'ancien chargement avant d'en créer un nouveau.
     */
    application_cancel_graph_loading(
        application
    );

    application_clear_graph(
        application
    );

    if (database_path == NULL ||
        database_path[0] == '\0')
    {
        main_window_set_graph_error(
            application->main_window,
            "Le chemin de la base SQLite est invalide."
        );

        return;
    }

    main_window_set_graph_loading(
        application->main_window
    );

    load_task =
        investigation_graph_load_task_new(
            database_path,
            &error
        );

    if (load_task == NULL)
    {
        main_window_set_graph_error(
            application->main_window,
            error != NULL
                ? error->message
                : "Impossible de préparer le chargement du graphe."
        );

        g_clear_error(
            &error
        );

        return;
    }

    context =
        g_try_new0(
            ApplicationGraphLoadContext,
            1
        );

    if (context == NULL)
    {
        investigation_graph_load_task_free(
            load_task
        );

        main_window_set_graph_error(
            application->main_window,
            "Impossible d'allouer le contexte de chargement."
        );

        return;
    }

    context->application =
        application;

    context->generation =
        application->graph_load_generation;

    application->graph_load_task =
        load_task;

    if (!investigation_graph_load_task_start(
            load_task,
            application_on_graph_loaded,
            context,
            application_graph_load_context_free,
            &error
        ))
    {
        /*
         * En cas d'échec immédiat, la tâche n'a pas pris possession
         * du contexte fourni.
         */
        application->graph_load_task =
            NULL;

        application_graph_load_context_free(
            context
        );

        investigation_graph_load_task_free(
            load_task
        );

        main_window_set_graph_error(
            application->main_window,
            error != NULL
                ? error->message
                : "Impossible de démarrer le chargement du graphe."
        );

        g_clear_error(
            &error
        );
    }
}

/**
 * @brief Vérifie que l’enquête active correspond à un chemin racine.
 */
static gboolean application_session_matches_root(
    const Application *application,
    const char *expected_root_path
)
{
    const InvestigationProject *project = NULL;
    const char *current_root_path = NULL;

    if (application == NULL ||
        application->session == NULL ||
        expected_root_path == NULL)
    {
        return FALSE;
    }

    project =
        investigation_session_get_project(
            application->session
        );

    if (project == NULL)
    {
        return FALSE;
    }

    current_root_path =
        investigation_project_get_root_path(
            project
        );

    return g_strcmp0(
        current_root_path,
        expected_root_path
    ) == 0;
}

/**
 * @brief Mémorise l'identifiant de la preuve actuellement affichée.
 */
static void application_set_selected_evidence_identifier(
    Application *application,
    const char *evidence_identifier
)
{
    char *identifier_copy =
        NULL;

    if (application == NULL)
    {
        return;
    }

    if (evidence_identifier != NULL &&
        g_uuid_string_is_valid(
            evidence_identifier
        ))
    {
        identifier_copy =
            g_strdup(
                evidence_identifier
            );
    }

    g_free(
        application->selected_evidence_identifier
    );

    application->selected_evidence_identifier =
        identifier_copy;
}

static void application_present_error(
    Application *application,
    const char *title,
    const char *message
);

/**
 * @brief Traite la fin de l’initialisation des outils externes.
 *
 * Ce callback est exécuté sur le contexte principal GLib.
 *
 * @param tool_initializer Initialiseur ayant terminé.
 * @param final_state État final de la tâche.
 * @param summary Résumé emprunté en cas de réussite.
 * @param error Erreur empruntée en cas d’échec.
 * @param user_data Pointeur emprunté vers Application.
 */
static void application_on_tool_initialization_completed(
    ToolInitializer *tool_initializer,
    BackgroundTaskState final_state,
    const ToolInitializationSummary *summary,
    const GError *error,
    gpointer user_data
)
{
    Application *application = user_data;

    if (application == NULL ||
        tool_initializer == NULL)
    {
        return;
    }

    if (tool_initializer !=
        application->tool_initializer)
    {
        g_warning(
            "Notification reçue depuis un initialiseur inconnu."
        );

        return;
    }

    switch (final_state)
    {
        case BACKGROUND_TASK_STATE_COMPLETED:
            if (summary == NULL)
            {
                g_warning(
                    "L’initialisation des outils est terminée "
                    "sans résumé valide."
                );

                return;
            }

            g_message(
                "Outils externes initialisés : "
                "%" G_GSIZE_FORMAT " disponibles, "
                "%" G_GSIZE_FORMAT " absents, "
                "%" G_GSIZE_FORMAT " versions détectées, "
                "%" G_GSIZE_FORMAT " versions non détectées.",
                summary->available_count,
                summary->missing_count,
                summary->version_detected_count,
                summary->version_failed_count
            );

            break;

        case BACKGROUND_TASK_STATE_CANCELLED:
            g_message(
                "L’initialisation des outils externes a été annulée."
            );

            break;

        case BACKGROUND_TASK_STATE_FAILED:
            g_warning(
                "L’initialisation des outils externes a échoué : %s",
                error != NULL
                    ? error->message
                    : "erreur inconnue"
            );

            application_present_error(
                application,
                "Initialisation des outils impossible",
                error != NULL
                    ? error->message
                    : "Les outils externes n’ont pas pu être initialisés."
            );

            break;

        case BACKGROUND_TASK_STATE_PENDING:
        case BACKGROUND_TASK_STATE_RUNNING:
        default:
            g_warning(
                "Notification de fin reçue avec un état invalide : %d.",
                (int) final_state
            );

            break;
    }
}

/**
 * @brief Démarre la détection asynchrone des outils externes.
 *
 * Une erreur empêche seulement l’initialisation des outils.
 * Elle ne doit pas fermer l’application.
 *
 * @param application Application concernée.
 */
static void application_start_tool_initialization(
    Application *application
)
{
    GError *error = NULL;

    if (application == NULL ||
        application->tool_initializer == NULL)
    {
        return;
    }

    if (!tool_initializer_start(
        application->tool_initializer,
        &error
    ))
    {
        g_warning(
            "Impossible d’initialiser les outils externes : %s",
            error != NULL
            ? error->message
            : "erreur inconnue"
        );

        application_present_error(
            application,
            "Initialisation des outils impossible",
            error != NULL
            ? error->message
            : "Les outils externes n’ont pas pu être initialisés."
        );

        g_clear_error(
            &error
        );
    }
}

/**
 * @brief Affiche une erreur dans une fenêtre GTK.
 *
 * @param application Application propriétaire de la fenêtre principale.
 * @param title Titre compréhensible par l'utilisateur.
 * @param message Description de l'erreur.
 */
static void application_present_error(
    Application *application,
    const char *title,
    const char *message
)
{
    GtkWindow *parent_window = NULL;

    if (application != NULL &&
        application->main_window != NULL)
    {
        parent_window = main_window_get_window(
            application->main_window
        );
    }

    application_message_dialog_present(
        parent_window,
        APPLICATION_MESSAGE_DIALOG_ERROR,
        title,
        message
    );
}

static void application_on_tool_initialization_completed(
    ToolInitializer *tool_initializer,
    BackgroundTaskState final_state,
    const ToolInitializationSummary *summary,
    const GError *error,
    gpointer user_data
);

static void application_present_error(
    Application *application,
    const char *title,
    const char *message
);

/**
 * @brief Charge les preuves de la session et construit leurs modèles GTK.
 *
 * Les nouveaux modèles sont entièrement préparés avant de remplacer
 * ceux actuellement affichés.
 *
 * En cas d'échec, les anciens modèles restent la propriété de
 * l'application et ne sont pas modifiés.
 */
static gboolean application_refresh_evidence_models(
    Application *application,
    GError **error
)
{
    Database *database = NULL;

    EvidenceDao *evidence_dao = NULL;
    EvidenceTypeDao *evidence_type_dao = NULL;

    GPtrArray *evidence_records = NULL;
    GPtrArray *evidence_types = NULL;

    GHashTable *type_labels = NULL;

    EvidenceListModel *new_evidence_list_model =
        NULL;

    EvidenceCategoryModel *new_evidence_category_model =
        NULL;

    GError *local_error = NULL;

    guint type_index = 0;

    gboolean refreshed = FALSE;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (application == NULL ||
        application->main_window == NULL ||
        application->session == NULL)
    {
        g_set_error_literal(
            error,
            APPLICATION_OPEN_ERROR,
            APPLICATION_OPEN_ERROR_EVIDENCE,
            "L'application ou la session d'enquête est invalide."
        );

        return FALSE;
    }

    database =
        investigation_session_get_database(
            application->session
        );

    if (database == NULL)
    {
        g_set_error_literal(
            error,
            APPLICATION_OPEN_ERROR,
            APPLICATION_OPEN_ERROR_EVIDENCE,
            "La session ne fournit aucune connexion SQLite."
        );

        return FALSE;
    }

    evidence_dao =
        evidence_dao_new(
            database,
            &local_error
        );

    if (evidence_dao == NULL)
    {
        if (local_error != NULL)
        {
            if (error != NULL)
            {
                g_propagate_prefixed_error(
                    error,
                    local_error,
                    "Impossible de créer le DAO des preuves : "
                );

                local_error = NULL;
            }
            else
            {
                g_clear_error(
                    &local_error
                );
            }
        }
        else
        {
            g_set_error_literal(
                error,
                APPLICATION_OPEN_ERROR,
                APPLICATION_OPEN_ERROR_EVIDENCE,
                "Impossible de créer le DAO des preuves."
            );
        }

        goto cleanup;
    }

    evidence_records =
        evidence_dao_list_all(
            evidence_dao,
            &local_error
        );

    if (evidence_records == NULL)
    {
        if (local_error != NULL)
        {
            if (error != NULL)
            {
                g_propagate_prefixed_error(
                    error,
                    local_error,
                    "Impossible de charger les preuves : "
                );

                local_error = NULL;
            }
            else
            {
                g_clear_error(
                    &local_error
                );
            }
        }
        else
        {
            g_set_error_literal(
                error,
                APPLICATION_OPEN_ERROR,
                APPLICATION_OPEN_ERROR_EVIDENCE,
                "Impossible de charger les preuves."
            );
        }

        goto cleanup;
    }

    evidence_type_dao =
        evidence_type_dao_new(
            database,
            &local_error
        );

    if (evidence_type_dao == NULL)
    {
        if (local_error != NULL)
        {
            if (error != NULL)
            {
                g_propagate_prefixed_error(
                    error,
                    local_error,
                    "Impossible de créer le DAO des types de preuve : "
                );

                local_error = NULL;
            }
            else
            {
                g_clear_error(
                    &local_error
                );
            }
        }
        else
        {
            g_set_error_literal(
                error,
                APPLICATION_OPEN_ERROR,
                APPLICATION_OPEN_ERROR_EVIDENCE,
                "Impossible de créer le DAO des types de preuve."
            );
        }

        goto cleanup;
    }

    evidence_types =
        evidence_type_dao_list_all(
            evidence_type_dao,
            &local_error
        );

    if (evidence_types == NULL)
    {
        if (local_error != NULL)
        {
            if (error != NULL)
            {
                g_propagate_prefixed_error(
                    error,
                    local_error,
                    "Impossible de charger les types de preuve : "
                );

                local_error = NULL;
            }
            else
            {
                g_clear_error(
                    &local_error
                );
            }
        }
        else
        {
            g_set_error_literal(
                error,
                APPLICATION_OPEN_ERROR,
                APPLICATION_OPEN_ERROR_EVIDENCE,
                "Impossible de charger les types de preuve."
            );
        }

        goto cleanup;
    }

    type_labels =
        g_hash_table_new_full(
            g_str_hash,
            g_str_equal,
            g_free,
            g_free
        );

    if (type_labels == NULL)
    {
        g_set_error_literal(
            error,
            APPLICATION_OPEN_ERROR,
            APPLICATION_OPEN_ERROR_EVIDENCE,
            "Impossible d'allouer la table des libellés."
        );

        goto cleanup;
    }

    for (type_index = 0;
         type_index < evidence_types->len;
         type_index++)
    {
        const EvidenceType *evidence_type =
            NULL;

        const char *type_code = NULL;
        const char *type_label = NULL;

        evidence_type =
            g_ptr_array_index(
                evidence_types,
                type_index
            );

        if (evidence_type == NULL)
        {
            continue;
        }

        type_code =
            evidence_type_get_code(
                evidence_type
            );

        type_label =
            evidence_type_get_label(
                evidence_type
            );

        if (type_code == NULL ||
            type_code[0] == '\0' ||
            type_label == NULL ||
            type_label[0] == '\0')
        {
            continue;
        }

        g_hash_table_replace(
            type_labels,
            g_strdup(
                type_code
            ),
            g_strdup(
                type_label
            )
        );
    }

    new_evidence_list_model =
        evidence_list_model_new();

    new_evidence_category_model =
        evidence_category_model_new();

    if (new_evidence_list_model == NULL ||
        new_evidence_category_model == NULL)
    {
        g_set_error_literal(
            error,
            APPLICATION_OPEN_ERROR,
            APPLICATION_OPEN_ERROR_EVIDENCE,
            "Impossible d'allouer les modèles d'affichage des preuves."
        );

        goto cleanup;
    }

    if (!evidence_list_model_replace(
            new_evidence_list_model,
            evidence_records,
            &local_error
        ))
    {
        if (local_error != NULL)
        {
            if (error != NULL)
            {
                g_propagate_prefixed_error(
                    error,
                    local_error,
                    "Impossible de préparer la liste des preuves : "
                );

                local_error = NULL;
            }
            else
            {
                g_clear_error(
                    &local_error
                );
            }
        }

        goto cleanup;
    }

    if (!evidence_category_model_replace(
            new_evidence_category_model,
            evidence_list_model_get_list_model(
                new_evidence_list_model
            ),
            type_labels,
            &local_error
        ))
    {
        if (local_error != NULL)
        {
            if (error != NULL)
            {
                g_propagate_prefixed_error(
                    error,
                    local_error,
                    "Impossible de regrouper les preuves : "
                );

                local_error = NULL;
            }
            else
            {
                g_clear_error(
                    &local_error
                );
            }
        }

        goto cleanup;
    }

    /*
     * La vue doit être détachée avant la libération de son ancien modèle.
     */
    main_window_set_evidence_model(
        application->main_window,
        NULL
    );

    evidence_category_model_free(
        application->evidence_category_model
    );

    evidence_list_model_free(
        application->evidence_list_model
    );

    application->evidence_list_model =
        new_evidence_list_model;

    application->evidence_category_model =
        new_evidence_category_model;

    new_evidence_list_model =
        NULL;

    new_evidence_category_model =
        NULL;

    main_window_set_evidence_model(
        application->main_window,
        application->evidence_category_model
    );

    refreshed = TRUE;

cleanup:

    g_clear_error(
        &local_error
    );

    evidence_category_model_free(
        new_evidence_category_model
    );

    evidence_list_model_free(
        new_evidence_list_model
    );

    if (type_labels != NULL)
    {
        g_hash_table_unref(
            type_labels
        );
    }

    if (evidence_types != NULL)
    {
        g_ptr_array_unref(
            evidence_types
        );
    }

    if (evidence_records != NULL)
    {
        g_ptr_array_unref(
            evidence_records
        );
    }

    evidence_type_dao_free(
        evidence_type_dao
    );

    evidence_dao_free(
        evidence_dao
    );

    return refreshed;
}

/**
 * @brief Installe une nouvelle session et son arbre dans l'application.
 *
 * En cas de succès, Application devient propriétaire de new_session
 * et de new_tree_model.
 *
 * En cas d'échec, le code appelant conserve la propriété des deux objets.
 *
 * @param application Application à mettre à jour.
 * @param new_session Nouvelle session valide.
 * @param new_tree_model Nouvel arbre valide.
 *
 * @return TRUE si l'installation réussit, sinon FALSE.
 */
static gboolean application_install_session(
    Application *application,
    InvestigationSession *new_session,
    InvestigationTreeModel *new_tree_model
)
{
    const InvestigationProject *project = NULL;
    const InvestigationRecord *record = NULL;

    const char *root_path = NULL;
    const char *database_path = NULL;
    const char *investigation_name = NULL;

    GError *evidence_error = NULL;

    if (application == NULL ||
        application->main_window == NULL ||
        new_session == NULL ||
        new_tree_model == NULL)
    {
        return FALSE;
    }

    project = investigation_session_get_project(
        new_session
    );

    record = investigation_session_get_record(
        new_session
    );

    if (project == NULL ||
        record == NULL)
    {
        return FALSE;
    }

    root_path = investigation_project_get_root_path(
        project
    );

    database_path =
        investigation_project_get_database_path(
            project
        );

    investigation_name = investigation_record_get_name(
        record
    );

    if (root_path == NULL ||
        root_path[0] == '\0' ||
        database_path == NULL ||
        database_path[0] == '\0' ||
        investigation_name == NULL ||
        investigation_name[0] == '\0')
    {
        return FALSE;
    }

    application_set_selected_evidence_identifier(
        application,
        NULL
    );

    main_window_set_selected_evidence(
        application->main_window,
        NULL
    );

    /*
     * Les nouveaux objets sont entièrement valides.
     * Les anciens peuvent maintenant être libérés.
     */
    investigation_tree_model_free(
        application->tree_model
    );

    investigation_session_close(
        application->session
    );

    application->tree_model = new_tree_model;
    application->session = new_session;

    main_window_set_tree_model(
        application->main_window,
        application->tree_model
    );

    main_window_set_investigation(
        application->main_window,
        investigation_name,
        root_path
    );

    main_window_set_import_evidence_enabled(
        application->main_window,
        TRUE
    );

    application_start_graph_loading(
        application,
        database_path
    );

    /*
     * L'ancienne liste ne doit jamais rester affichée après
     * un changement d'enquête.
     */
    main_window_set_evidence_model(
        application->main_window,
        NULL
    );

    evidence_category_model_free(
        application->evidence_category_model
    );

    evidence_list_model_free(
        application->evidence_list_model
    );

    application->evidence_category_model =
        NULL;

    application->evidence_list_model =
        NULL;

    if (!application_refresh_evidence_models(
            application,
            &evidence_error
        ))
    {
        g_warning(
            "Impossible de charger les preuves de l'enquête : %s",
            evidence_error != NULL
                ? evidence_error->message
                : "erreur inconnue"
        );

        application_present_error(
            application,
            "Preuves indisponibles",
            evidence_error != NULL
                ? evidence_error->message
                : "Les preuves enregistrées n'ont pas pu être chargées."
        );

        g_clear_error(
            &evidence_error
        );
    }

    return TRUE;
}

/**
 * @brief Ouvre une enquête et l'installe dans l'application.
 *
 * En cas de succès, Application devient propriétaire de la nouvelle
 * session et du nouvel arbre.
 *
 * En cas d'échec, l'état précédent de l'application reste inchangé.
 *
 * @param application Application à mettre à jour.
 * @param root_path Chemin racine de l'enquête.
 * @param error Emplacement facultatif pour l'erreur.
 *
 * @return TRUE si l'enquête est ouverte et installée, sinon FALSE.
 */
static gboolean application_open_and_install_investigation(
    Application *application,
    const char *root_path,
    GError **error
)
{
    InvestigationSession *new_session = NULL;
    InvestigationTreeModel *new_tree_model = NULL;

    const InvestigationProject *project = NULL;
    const char *canonical_root_path = NULL;

    GError *session_error = NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (application == NULL ||
        application->main_window == NULL)
    {
        g_set_error_literal(
            error,
            APPLICATION_OPEN_ERROR,
            APPLICATION_OPEN_ERROR_INVALID_ARGUMENT,
            "L'application ou sa fenêtre principale est invalide."
        );

        return FALSE;
    }

    if (root_path == NULL ||
        root_path[0] == '\0')
    {
        g_set_error_literal(
            error,
            APPLICATION_OPEN_ERROR,
            APPLICATION_OPEN_ERROR_INVALID_ARGUMENT,
            "Le chemin racine de l'enquête est invalide."
        );

        return FALSE;
    }

    new_session = investigation_session_open(
        root_path,
        &session_error
    );

    if (new_session == NULL)
    {
        if (session_error != NULL)
        {
            if (error != NULL)
            {
                g_propagate_prefixed_error(
                    error,
                    session_error,
                    "Impossible d'ouvrir la session : "
                );
            }
            else
            {
                g_clear_error(
                    &session_error
                );
            }
        }
        else
        {
            g_set_error_literal(
                error,
                APPLICATION_OPEN_ERROR,
                APPLICATION_OPEN_ERROR_INVALID_PROJECT,
                "Impossible d'ouvrir la session de l'enquête."
            );
        }

        return FALSE;
    }

    project = investigation_session_get_project(
        new_session
    );

    if (project == NULL)
    {
        g_set_error_literal(
            error,
            APPLICATION_OPEN_ERROR,
            APPLICATION_OPEN_ERROR_INVALID_PROJECT,
            "La session ne fournit aucun projet valide."
        );

        investigation_session_close(
            new_session
        );

        return FALSE;
    }

    canonical_root_path =
        investigation_project_get_root_path(
            project
        );

    if (canonical_root_path == NULL ||
        canonical_root_path[0] == '\0')
    {
        g_set_error_literal(
            error,
            APPLICATION_OPEN_ERROR,
            APPLICATION_OPEN_ERROR_INVALID_PROJECT,
            "Le projet ne fournit aucun chemin racine valide."
        );

        investigation_session_close(
            new_session
        );

        return FALSE;
    }

    new_tree_model = investigation_tree_builder_build(
        canonical_root_path
    );

    if (new_tree_model == NULL)
    {
        g_set_error(
            error,
            APPLICATION_OPEN_ERROR,
            APPLICATION_OPEN_ERROR_TREE_BUILD,
            "Impossible de construire l'arborescence de l'enquête "
            "située dans '%s'.",
            canonical_root_path
        );

        investigation_session_close(
            new_session
        );

        return FALSE;
    }

    if (!application_install_session(
            application,
            new_session,
            new_tree_model
        ))
    {
        g_set_error_literal(
            error,
            APPLICATION_OPEN_ERROR,
            APPLICATION_OPEN_ERROR_INSTALL,
            "Impossible d'installer la session dans l'application."
        );

        investigation_tree_model_free(
            new_tree_model
        );

        investigation_session_close(
            new_session
        );

        return FALSE;
    }

    /*
     * La propriété de new_session et new_tree_model a été
     * transférée à Application.
     */
    return TRUE;
}

/**
 * @brief Traite le dossier d'enquête sélectionné par l'utilisateur.
 *
 * @param folder_path Chemin sélectionné, ou NULL en cas d'annulation.
 * @param user_data Pointeur vers Application.
 */
static void application_on_folder_selected(
    const char *folder_path,
    gpointer user_data
)
{
    Application *application = user_data;
    GError *error = NULL;

    if (application == NULL)
    {
        return;
    }

    /*
     * L'annulation du sélecteur ne constitue pas une erreur.
     */
    if (folder_path == NULL)
    {
        return;
    }

    if (!application_open_and_install_investigation(
            application,
            folder_path,
            &error
        ))
    {
        g_warning(
            "Impossible d'ouvrir l'enquête '%s' : %s",
            folder_path,
            error != NULL
                ? error->message
                : "erreur inconnue"
        );

        application_present_error(
            application,
            "Ouverture impossible",
            error != NULL
                ? error->message
                : "L'enquête sélectionnée n'a pas pu être ouverte."
        );

        g_clear_error(
            &error
        );
    }
}

/**
 * @brief Traite la demande de création d'une nouvelle enquête.
 *
 * @param parent_directory Dossier dans lequel créer l'enquête.
 * @param investigation_name Nom nettoyé de l'enquête.
 * @param user_data Pointeur vers Application.
 */
static void application_on_create_investigation(
    const char *parent_directory,
    const char *investigation_name,
    gpointer user_data
)
{
    Application *application = user_data;

    char *created_root_path = NULL;
    char *user_message = NULL;

    GError *error = NULL;

    if (application == NULL)
    {
        return;
    }

    /*
     * Une annulation du dialogue transmet deux pointeurs NULL.
     */
    if (parent_directory == NULL ||
        investigation_name == NULL)
    {
        return;
    }

    created_root_path = investigation_project_create(
        parent_directory,
        investigation_name
    );

    if (created_root_path == NULL)
    {
        g_warning(
            "Impossible de créer l'enquête '%s' dans '%s'.",
            investigation_name,
            parent_directory
        );

        user_message = g_strdup_printf(
            "L'enquête « %s » n'a pas pu être créée dans :\n"
            "%s\n\n"
            "Vérifiez que le dossier existe et que vous disposez "
            "des droits d'écriture nécessaires.",
            investigation_name,
            parent_directory
        );

        application_present_error(
            application,
            "Création impossible",
            user_message
        );

        g_free(
            user_message
        );

        return;
    }

    if (!application_open_and_install_investigation(
            application,
            created_root_path,
            &error
        ))
    {
        g_warning(
            "L'enquête a été créée dans '%s', mais son "
            "ouverture a échoué : %s",
            created_root_path,
            error != NULL
                ? error->message
                : "erreur inconnue"
        );

        user_message = g_strdup_printf(
            "L'enquête a bien été créée sur le disque dans :\n"
            "%s\n\n"
            "Labfy Investigation n'a cependant pas pu l'ouvrir :\n"
            "%s",
            created_root_path,
            error != NULL
                ? error->message
                : "Aucun détail supplémentaire n'est disponible."
        );

        application_present_error(
            application,
            "Enquête créée mais non ouverte",
            user_message
        );

        g_free(
            user_message
        );

        g_clear_error(
            &error
        );
    }
    g_free(
        created_root_path
    );
}

/**
 * @brief Ouvre le dialogue de création d'une enquête.
 *
 * @param user_data Pointeur vers Application.
 */
static void application_on_new_investigation_requested(
    gpointer user_data
)
{
    Application *application = user_data;

    if (application == NULL ||
        application->main_window == NULL)
    {
        return;
    }

    create_investigation_dialog_present(
        main_window_get_window(
            application->main_window
        ),
        application_on_create_investigation,
        application
    );
}

/**
 * @brief Ouvre le sélecteur d'une enquête existante.
 *
 * @param user_data Pointeur vers Application.
 */
static void application_on_open_investigation_requested(
    gpointer user_data
)
{
    Application *application = user_data;

    if (application == NULL ||
        application->main_window == NULL)
    {
        return;
    }

    folder_dialog_select_folder(
        main_window_get_window(
            application->main_window
        ),
        application_on_folder_selected,
        application
    );
}

/**
 * @brief Reconstruit et remplace l’arborescence active.
 *
 * Le nouveau modèle est entièrement construit avant que l’ancien
 * soit détaché et libéré.
 *
 * @param application Application concernée.
 * @param investigation_root_path Racine à parcourir.
 *
 * @return TRUE si le remplacement a réussi.
 */
static gboolean application_refresh_investigation_tree(
    Application *application,
    const char *investigation_root_path
)
{
    InvestigationTreeModel *new_tree_model = NULL;
    InvestigationTreeModel *old_tree_model = NULL;

    if (application == NULL ||
        application->main_window == NULL ||
        investigation_root_path == NULL ||
        investigation_root_path[0] == '\0')
    {
        return FALSE;
    }

    new_tree_model =
        investigation_tree_builder_build(
            investigation_root_path
        );

    if (new_tree_model == NULL)
    {
        return FALSE;
    }

    old_tree_model =
        application->tree_model;

    /*
     * MainWindow transmet le nouveau modèle à la Sidebar.
     *
     * InvestigationTreeView détache d’abord son ancien modèle GTK,
     * puis construit le nouveau. L’ancien modèle métier reste vivant
     * pendant toute cette opération.
     */
    main_window_set_tree_model(
        application->main_window,
        new_tree_model
    );

    application->tree_model =
        new_tree_model;

    /*
     * La vue ne référence plus les anciens nœuds.
     */
    investigation_tree_model_free(
        old_tree_model
    );

    return TRUE;
}

/**
 * @brief Traite la fin d’une tâche d’import.
 *
 * @param task Tâche d’import terminée.
 * @param user_data Contexte ApplicationEvidenceImportContext.
 */
static void application_on_evidence_import_completed(
    BackgroundTask *task,
    gpointer user_data
)
{
    ApplicationEvidenceImportContext *context =
        user_data;

    Application *application = NULL;

    BackgroundTaskState state;

    EvidenceRecord *evidence_record = NULL;

    const char *original_name = NULL;

    char *status_message = NULL;

    GError *error = NULL;

    GError *evidence_refresh_error =
        NULL;

    gboolean tree_refreshed =
        FALSE;

    gboolean evidence_refreshed =
        FALSE;

    if (task == NULL ||
        context == NULL)
    {
        return;
    }

    application =
        context->application;

    if (application == NULL ||
        application->main_window == NULL)
    {
        return;
    }

    main_window_set_import_evidence_enabled(
        application->main_window,
        application->session != NULL
    );

    state =
        background_task_get_state(
            task
        );

    if (state ==
        BACKGROUND_TASK_STATE_COMPLETED)
    {
        evidence_record =
            background_task_get_result(
                task
            );

        if (evidence_record == NULL)
        {
            application_present_error(
                application,
                "Import incomplet",
                "La tâche est terminée sans fournir de preuve."
            );

            return;
        }

        original_name =
            evidence_record_get_original_name(
                evidence_record
            );

        /*
         * L'utilisateur peut avoir changé d'enquête pendant
         * l'exécution asynchrone de l'import.
         *
         * Dans ce cas, la preuve reste correctement importée dans
         * l'ancienne enquête, mais l'interface actuelle ne doit pas
         * être rafraîchie avec ses données.
         */
        if (application_session_matches_root(
                application,
                context->investigation_root_path
            ))
        {
            tree_refreshed =
                application_refresh_investigation_tree(
                    application,
                    context->investigation_root_path
                );

            evidence_refreshed =
                application_refresh_evidence_models(
                    application,
                    &evidence_refresh_error
                );

            if (!tree_refreshed)
            {
                g_warning(
                    "La preuve a été importée, mais l'arborescence de '%s' "
                    "n'a pas pu être reconstruite.",
                    context->investigation_root_path
                );
            }

            if (!evidence_refreshed)
            {
                g_warning(
                    "La preuve a été importée, mais la liste des preuves "
                    "n'a pas pu être actualisée : %s",
                    evidence_refresh_error != NULL
                        ? evidence_refresh_error->message
                        : "erreur inconnue"
                );
            }

            if (tree_refreshed &&
                evidence_refreshed)
            {
                status_message =
                    g_strdup_printf(
                        "Preuve importée : %s",
                        original_name != NULL &&
                        original_name[0] != '\0'
                            ? original_name
                            : "(nom indisponible)"
                    );
            }
            else if (!tree_refreshed &&
                     evidence_refreshed)
            {
                status_message =
                    g_strdup_printf(
                        "Preuve importée : %s — "
                        "arborescence non actualisée",
                        original_name != NULL &&
                        original_name[0] != '\0'
                            ? original_name
                            : "(nom indisponible)"
                    );
            }
            else if (tree_refreshed &&
                     !evidence_refreshed)
            {
                status_message =
                    g_strdup_printf(
                        "Preuve importée : %s — "
                        "liste des preuves non actualisée",
                        original_name != NULL &&
                        original_name[0] != '\0'
                            ? original_name
                            : "(nom indisponible)"
                    );
            }
            else
            {
                status_message =
                    g_strdup_printf(
                        "Preuve importée : %s — "
                        "interface non actualisée",
                        original_name != NULL &&
                        original_name[0] != '\0'
                            ? original_name
                            : "(nom indisponible)"
                    );
            }

            main_window_set_status(
                application->main_window,
                status_message
            );

            g_clear_error(
                &evidence_refresh_error
            );
        }

        g_message(
            "Preuve importée dans '%s' : %s",
            context->investigation_root_path,
            original_name != NULL &&
            original_name[0] != '\0'
                ? original_name
                : "(nom indisponible)"
        );

        g_free(
            status_message
        );

        return;
    }

    if (state ==
        BACKGROUND_TASK_STATE_CANCELLED)
    {
        if (application_session_matches_root(
                application,
                context->investigation_root_path
            ))
        {
            main_window_set_status(
                application->main_window,
                "Import de preuve annulé."
            );
        }

        return;
    }

    error =
        background_task_dup_error(
            task
        );

    g_warning(
        "L’import de la preuve a échoué : %s",
        error != NULL
            ? error->message
            : "erreur inconnue"
    );

    application_present_error(
        application,
        "Import impossible",
        error != NULL
            ? error->message
            : "La preuve n’a pas pu être importée."
    );

    g_clear_error(
        &error
    );
}

/**
 * @brief Retourne le dossier de stockage associé à un type de preuve.
 *
 * La chaîne retournée est statique et ne doit pas être libérée.
 */
static const char *application_get_evidence_subdirectory(
    const char *type_identifier
)
{
    if (g_strcmp0(
            type_identifier,
            "screenshot"
        ) == 0)
    {
        return "Captures_Ecran";
    }

    if (g_strcmp0(
            type_identifier,
            "photo"
        ) == 0)
    {
        return "Photos";
    }

    if (g_strcmp0(
            type_identifier,
            "video"
        ) == 0)
    {
        return "Videos";
    }

    if (g_strcmp0(
            type_identifier,
            "email"
        ) == 0)
    {
        return "Emails";
    }

    if (g_strcmp0(
            type_identifier,
            "audio"
        ) == 0)
    {
        return "Audios";
    }

    if (g_strcmp0(
            type_identifier,
            "archive"
        ) == 0)
    {
        return "Archives";
    }

    if (g_strcmp0(
            type_identifier,
            "conversation"
        ) == 0)
    {
        return "Conversations";
    }

    /*
     * Documents, textes, autres types et types inconnus.
     */
    return "Documents";
}

/**
 * @brief Prépare et démarre l’import asynchrone d’un fichier.
 */
static void application_start_evidence_import(
    Application *application,
    const char *file_path,
    const char *type_identifier,
    const char *collected_at,
    const char *source,
    const char *description
)
{
    const InvestigationProject *project = NULL;

    const char *root_path = NULL;
    const char *database_path = NULL;

    const char *evidence_subdirectory = NULL;

    char *relative_directory = NULL;

    EvidenceImportTaskRequest request =
        {0};

    ApplicationEvidenceImportContext *context =
        NULL;

    BackgroundTask *task = NULL;

    char *destination_directory = NULL;
    char *source_name = NULL;
    char *status_message = NULL;

    GError *error = NULL;

    if (application == NULL ||
        application->main_window == NULL ||
        application->session == NULL ||
        application->task_manager == NULL ||
        file_path == NULL ||
        file_path[0] == '\0' ||
        type_identifier == NULL ||
        type_identifier[0] == '\0' ||
        collected_at == NULL ||
        collected_at[0] == '\0')
    {
        return;
    }

    project =
        investigation_session_get_project(
            application->session
        );

    if (project == NULL)
    {
        application_present_error(
            application,
            "Import impossible",
            "La session ne fournit aucun projet d’enquête."
        );

        return;
    }

    root_path =
        investigation_project_get_root_path(
            project
        );

    database_path =
        investigation_project_get_database_path(
            project
        );

    if (root_path == NULL ||
        root_path[0] == '\0' ||
        database_path == NULL ||
        database_path[0] == '\0')
    {
        application_present_error(
            application,
            "Import impossible",
            "Les chemins de l’enquête sont invalides."
        );

        return;
    }

    evidence_subdirectory =
        application_get_evidence_subdirectory(
            type_identifier
        );

    relative_directory =
        g_build_filename(
            APPLICATION_EVIDENCE_ROOT_DIRECTORY,
            evidence_subdirectory,
            NULL
        );

    if (relative_directory == NULL)
    {
        application_present_error(
            application,
            "Import impossible",
            "Impossible de construire le chemin relatif de la preuve."
        );

        return;
    }

    destination_directory =
        g_build_filename(
            root_path,
            relative_directory,
            NULL
        );

    if (destination_directory == NULL)
    {
        application_present_error(
            application,
            "Import impossible",
            "Impossible de construire le dossier de destination."
        );

        g_free(
            relative_directory
        );

        return;
    }

    /*
     * Le dossier devrait exister dans une enquête normale.
     * Cette création rend néanmoins l’import robuste face à un dossier
     * supprimé accidentellement.
     */
    if (g_mkdir_with_parents(
            destination_directory,
            0700
        ) != 0)
    {
        status_message =
            g_strdup_printf(
                "Impossible de préparer le dossier '%s' : %s",
                destination_directory,
                g_strerror(errno)
            );

        application_present_error(
            application,
            "Import impossible",
            status_message
        );

        g_free(
            status_message
        );

        g_free(
            destination_directory
        );

        return;
    }

    context =
        g_try_new0(
            ApplicationEvidenceImportContext,
            1
        );

    if (context == NULL)
    {
        application_present_error(
            application,
            "Import impossible",
            "Impossible d’allouer le contexte de la tâche."
        );

        g_free(
            destination_directory
        );

        return;
    }

    context->application =
        application;

    context->investigation_root_path =
        g_strdup(
            root_path
        );

    if (context->investigation_root_path == NULL)
    {
        application_evidence_import_context_free(
            context
        );

        g_free(
            destination_directory
        );

        application_present_error(
            application,
            "Import impossible",
            "Impossible de conserver le chemin de l’enquête."
        );

        return;
    }

    request.database_path =
        database_path;

    request.source_path =
        file_path;

    request.destination_directory =
        destination_directory;

    request.relative_directory =
        relative_directory;

    request.type_identifier =
        type_identifier;

    request.collected_at =
        collected_at;

    request.source =
        source;

    request.description =
        description;

    task =
        evidence_import_task_start(
            application->task_manager,
            &request,
            application_on_evidence_import_completed,
            context,
            application_evidence_import_context_free,
            &error
        );

    if (task == NULL)
    {
        /*
         * La tâche n’a pas pris possession du contexte.
         */
        application_evidence_import_context_free(
            context
        );

        application_present_error(
            application,
            "Import impossible",
            error != NULL
                ? error->message
                : "La tâche d’import n’a pas pu être démarrée."
        );

        g_clear_error(
            &error
        );

        g_free(
            destination_directory
        );

        g_free(
            relative_directory
        );

        return;
    }

    /*
     * Le TaskManager et le worker conservent désormais leurs références.
     */
    background_task_unref(
        task
    );

    main_window_set_import_evidence_enabled(
        application->main_window,
        FALSE
    );

    source_name =
        g_path_get_basename(
            file_path
        );

    status_message =
        g_strdup_printf(
            "Import en cours : %s",
            source_name != NULL
                ? source_name
                : file_path
        );

    main_window_set_status(
        application->main_window,
        status_message
    );

    g_free(
        status_message
    );

    g_free(
        source_name
    );

    g_free(
        destination_directory
    );

    g_free(
        relative_directory
    );
}

/**
 * @brief Traite la validation ou l'annulation des métadonnées.
 *
 * @param result Résultat possédé par ce callback, ou NULL.
 * @param user_data Contexte ApplicationEvidenceDialogContext.
 */
static void application_on_evidence_metadata_completed(
    EvidenceImportDialogResult *result,
    gpointer user_data
)
{
    ApplicationEvidenceDialogContext *context =
        user_data;

    Application *application = NULL;

    if (context == NULL)
    {
        evidence_import_dialog_result_free(
            result
        );

        return;
    }

    application =
        context->application;

    if (application == NULL ||
        application->main_window == NULL)
    {
        evidence_import_dialog_result_free(
            result
        );

        application_evidence_dialog_context_free(
            context
        );

        return;
    }

    /*
     * Une annulation du dialogue ne doit lancer aucune tâche.
     */
    if (result == NULL)
    {
        main_window_set_status(
            application->main_window,
            "Import de preuve annulé."
        );

        application_evidence_dialog_context_free(
            context
        );

        return;
    }

    application_start_evidence_import(
        application,
        context->file_path,
        evidence_import_dialog_result_get_type_identifier(
            result
        ),
        evidence_import_dialog_result_get_collected_at(
            result
        ),
        evidence_import_dialog_result_get_source(
            result
        ),
        evidence_import_dialog_result_get_description(
            result
        )
    );

    evidence_import_dialog_result_free(
        result
    );

    application_evidence_dialog_context_free(
        context
    );
}

/**
 * @brief Traite le fichier de preuve sélectionné.
 *
 * @param file_path Chemin local sélectionné, ou NULL.
 * @param error Erreur de sélection, ou NULL.
 * @param user_data Pointeur vers Application.
 */
static void application_on_evidence_file_selected(
    const char *file_path,
    const GError *error,
    gpointer user_data
)
{
    Application *application =
        user_data;

    if (application == NULL ||
        application->main_window == NULL)
    {
        return;
    }

    /*
     * Une annulation ne constitue pas une erreur.
     */
    if (file_path == NULL &&
        error == NULL)
    {
        main_window_set_status(
            application->main_window,
            "Import de preuve annulé."
        );

        return;
    }

    if (error != NULL)
    {
        g_warning(
            "Impossible de sélectionner le fichier : %s",
            error->message
        );

        application_present_error(
            application,
            "Sélection impossible",
            error->message
        );

        return;
    }

    if (file_path == NULL ||
        file_path[0] == '\0')
    {
        application_present_error(
            application,
            "Sélection impossible",
            "Le chemin du fichier sélectionné est invalide."
        );

        return;
    }

    Database *database = NULL;
    EvidenceTypeDao *evidence_type_dao = NULL;
    GPtrArray *evidence_types = NULL;

    ApplicationEvidenceDialogContext *dialog_context =
        NULL;

    GError *dialog_error = NULL;

    database =
        investigation_session_get_database(
            application->session
        );

    if (database == NULL)
    {
        application_present_error(
            application,
            "Import impossible",
            "La session ne fournit aucune connexion SQLite."
        );

        return;
    }

    evidence_type_dao =
        evidence_type_dao_new(
            database,
            &dialog_error
        );

    if (evidence_type_dao == NULL)
    {
        application_present_error(
            application,
            "Types de preuve indisponibles",
            dialog_error != NULL
            ? dialog_error->message
            : "Impossible de créer le DAO des types de preuve."
        );

        g_clear_error(
            &dialog_error
        );

        return;
    }

    evidence_types =
        evidence_type_dao_list_all(
            evidence_type_dao,
            &dialog_error
        );

    evidence_type_dao_free(
        evidence_type_dao
    );

    if (evidence_types == NULL)
    {
        application_present_error(
            application,
            "Types de preuve indisponibles",
            dialog_error != NULL
            ? dialog_error->message
            : "Impossible de lire les types de preuve."
        );

        g_clear_error(
            &dialog_error
        );

        return;
    }

    if (evidence_types->len == 0)
    {
        application_present_error(
            application,
            "Types de preuve indisponibles",
            "Aucun type de preuve n'est enregistré dans la base."
        );

        g_ptr_array_unref(
            evidence_types
        );

        return;
    }

    dialog_context =
        g_try_new0(
            ApplicationEvidenceDialogContext,
            1
        );

    if (dialog_context == NULL)
    {
        application_present_error(
            application,
            "Import impossible",
            "Impossible d'allouer le contexte du dialogue."
        );

        g_ptr_array_unref(
            evidence_types
        );

        return;
    }

    dialog_context->application =
        application;

    dialog_context->file_path =
        g_strdup(
            file_path
        );

    if (dialog_context->file_path == NULL)
    {
        application_present_error(
            application,
            "Import impossible",
            "Impossible de conserver le chemin du fichier."
        );

        application_evidence_dialog_context_free(
            dialog_context
        );

        g_ptr_array_unref(
            evidence_types
        );

        return;
    }

    if (!evidence_import_dialog_present(
        main_window_get_window(
            application->main_window
        ),
        dialog_context->file_path,
        evidence_types,
        application_on_evidence_metadata_completed,
        dialog_context,
        &dialog_error
    ))
    {
        application_present_error(
            application,
            "Dialogue d'import impossible",
            dialog_error != NULL
            ? dialog_error->message
            : "Le dialogue de métadonnées n'a pas pu être ouvert."
        );

        g_clear_error(
            &dialog_error
        );

        application_evidence_dialog_context_free(
            dialog_context
        );
    }

    g_ptr_array_unref(
        evidence_types
    );
}

/**
 * @brief Traite la demande d'import d'une preuve.
 *
 * Le sélecteur de fichier sera ajouté à l'étape suivante.
 *
 * @param user_data Pointeur vers Application.
 */
static void application_on_import_evidence_requested(
    gpointer user_data
)
{
    Application *application = user_data;

    if (application == NULL ||
        application->main_window == NULL)
    {
        return;
    }

    if (application->session == NULL)
    {
        main_window_set_import_evidence_enabled(
            application->main_window,
            FALSE
        );

        return;
    }

    file_dialog_select_file(
        main_window_get_window(
            application->main_window
        ),
        application_on_evidence_file_selected,
        application
    );
}

/**
 * @brief Traite la sélection d'un nœud dans l'arborescence.
 *
 * @param node Nœud sélectionné, ou NULL si aucune sélection.
 * @param user_data Pointeur vers Application.
 */
static void application_on_tree_node_selected(
    const InvestigationNode *node,
    gpointer user_data
)
{
    Application *application = user_data;
    const char *node_name = NULL;
    InvestigationNodeType node_type;

    if (application == NULL)
    {
        return;
    }

    main_window_set_selected_node(
        application->main_window,
        node
    );

    if (node == NULL)
    {
        g_print("Aucun nœud sélectionné.\n");
        return;
    }

    node_name = investigation_node_get_name(
        node
    );

    node_type = investigation_node_get_type(
        node
    );

    g_print(
        "Nœud sélectionné : %s\n",
        node_name != NULL
            ? node_name
            : "(sans nom)"
    );

    if (node_type == INVESTIGATION_NODE_DIRECTORY)
    {
        g_print("Type : dossier\n");
    }
    else
    {
        g_print("Type : fichier\n");
    }
}

/**
 * @brief Traite la sélection d'une preuve dans la Sidebar.
 *
 * L'identifiant est emprunté uniquement pendant cet appel.
 * La preuve complète est relue depuis SQLite avant d'être transmise
 * au Workspace.
 *
 * @param evidence_identifier UUID de la preuve, ou NULL.
 * @param user_data Pointeur vers Application.
 */
static void application_on_evidence_selected(
    const char *evidence_identifier,
    gpointer user_data
)
{
    Application *application =
        user_data;

    Database *database =
        NULL;

    EvidenceDao *evidence_dao =
        NULL;

    EvidenceRecord *evidence_record =
        NULL;

    const char *original_name =
        NULL;

    char *status_message =
        NULL;

    GError *error =
        NULL;

    if (application == NULL ||
        application->main_window == NULL)
    {
        return;
    }

    /*
     * Une catégorie ou une sélection vide transmet NULL.
     */
    if (evidence_identifier == NULL ||
        evidence_identifier[0] == '\0')
    {
        application_set_selected_evidence_identifier(
            application,
            NULL
        );

        main_window_set_selected_evidence(
            application->main_window,
            NULL
        );

        application_set_selected_evidence_identifier(
            application,
            NULL
        );

        return;
    }

    if (application->session == NULL)
    {
        main_window_set_selected_evidence(
            application->main_window,
            NULL
        );

        return;
    }

    database =
        investigation_session_get_database(
            application->session
        );

    if (database == NULL)
    {
        main_window_set_selected_evidence(
            application->main_window,
            NULL
        );

        application_present_error(
            application,
            "Preuve indisponible",
            "La session ne fournit aucune connexion SQLite."
        );

        return;
    }

    evidence_dao =
        evidence_dao_new(
            database,
            &error
        );

    if (evidence_dao == NULL)
    {
        g_warning(
            "Impossible de créer le DAO des preuves : %s",
            error != NULL
                ? error->message
                : "erreur inconnue"
        );

        main_window_set_selected_evidence(
            application->main_window,
            NULL
        );

        application_present_error(
            application,
            "Preuve indisponible",
            error != NULL
                ? error->message
                : "Impossible d'accéder aux preuves enregistrées."
        );

        g_clear_error(
            &error
        );

        return;
    }

    evidence_record =
        evidence_dao_find_by_identifier(
            evidence_dao,
            evidence_identifier,
            &error
        );

    evidence_dao_free(
        evidence_dao
    );

    if (error != NULL)
    {
        g_warning(
            "Impossible de charger la preuve '%s' : %s",
            evidence_identifier,
            error->message
        );

        main_window_set_selected_evidence(
            application->main_window,
            NULL
        );

        application_present_error(
            application,
            "Preuve indisponible",
            error->message
        );

        g_clear_error(
            &error
        );

        return;
    }

    /*
     * Une preuve absente retourne NULL sans erreur.
     */
    if (evidence_record == NULL)
    {
        g_warning(
            "La preuve '%s' n'existe plus dans la base.",
            evidence_identifier
        );

        main_window_set_selected_evidence(
            application->main_window,
            NULL
        );

        main_window_set_status(
            application->main_window,
            "La preuve sélectionnée est introuvable."
        );

        return;
    }

    application_set_selected_evidence_identifier(
        application,
        evidence_identifier
    );

    /*
     * Workspace copie immédiatement les valeurs dans ses widgets GTK.
     */
    main_window_set_selected_evidence(
        application->main_window,
        evidence_record
    );

    original_name =
        evidence_record_get_original_name(
            evidence_record
        );

    status_message =
        g_strdup_printf(
            "Preuve sélectionnée : %s",
            original_name != NULL &&
            original_name[0] != '\0'
                ? original_name
                : "(sans nom)"
        );

    main_window_set_status(
        application->main_window,
        status_message
    );

    g_free(
        status_message
    );

    evidence_record_free(
        evidence_record
    );
}


/**
 * @brief Traite la validation ou l'annulation du dialogue de relation.
 *
 * Cette première étape valide le parcours complet de l'interface.
 * L'écriture SQLite sera branchée après validation de la compilation.
 */
static void application_on_create_relation_completed(
    CreateRelationDialogResult *result,
    gpointer user_data
)
{
    Application *application =
        user_data;

    const InvestigationProject *project =
        NULL;

    Database *database =
        NULL;

    RelationService *relation_service =
        NULL;

    RelationRecord *relation_record =
        NULL;

    GDateTime *current_date_time =
        NULL;

    const char *source_identifier =
        NULL;

    const char *target_identifier =
        NULL;

    const char *relation_type =
        NULL;

    const char *database_path =
        NULL;

    char *relation_identifier =
        NULL;

    char *timestamp =
        NULL;

    GError *error =
        NULL;

    gboolean relation_created =
        FALSE;

    if (application == NULL ||
        application->main_window == NULL)
    {
        create_relation_dialog_result_free(
            result
        );

        return;
    }

    if (result == NULL)
    {
        main_window_set_status(
            application->main_window,
            "Ajout de relation annulé."
        );

        return;
    }

    if (application->session == NULL)
    {
        application_present_error(
            application,
            "Ajout de relation impossible",
            "Aucune enquête n'est actuellement ouverte."
        );

        goto cleanup;
    }

    source_identifier =
        create_relation_dialog_result_get_source_identifier(
            result
        );

    target_identifier =
        create_relation_dialog_result_get_target_identifier(
            result
        );

    relation_type =
        create_relation_dialog_result_get_relation_type(
            result
        );

    database =
        investigation_session_get_database(
            application->session
        );

    project =
        investigation_session_get_project(
            application->session
        );

    if (database == NULL ||
        project == NULL)
    {
        application_present_error(
            application,
            "Ajout de relation impossible",
            "La session d'enquête est invalide."
        );

        goto cleanup;
    }

    database_path =
        investigation_project_get_database_path(
            project
        );

    if (database_path == NULL ||
        database_path[0] == '\0')
    {
        application_present_error(
            application,
            "Ajout de relation impossible",
            "Le chemin de la base SQLite est invalide."
        );

        goto cleanup;
    }

    relation_identifier =
        g_uuid_string_random();

    current_date_time =
        g_date_time_new_now_utc();

    if (current_date_time != NULL)
    {
        timestamp =
            g_date_time_format(
                current_date_time,
                "%Y-%m-%dT%H:%M:%SZ"
            );
    }

    if (relation_identifier == NULL ||
        timestamp == NULL)
    {
        application_present_error(
            application,
            "Ajout de relation impossible",
            "Impossible de préparer l'identifiant ou la date "
            "de la nouvelle relation."
        );

        goto cleanup;
    }

    relation_record =
        relation_record_new(
            relation_identifier,
            source_identifier,
            target_identifier,
            relation_type,
            create_relation_dialog_result_get_label(
                result
            ),
            create_relation_dialog_result_get_justification(
                result
            ),
            create_relation_dialog_result_get_confidence(
                result
            ),
            timestamp,
            timestamp,
            RELATION_STATUS_ACTIVE,
            &error
        );

    if (relation_record == NULL)
    {
        application_present_error(
            application,
            "Relation invalide",
            error != NULL
                ? error->message
                : "Les informations de la relation sont invalides."
        );

        g_clear_error(
            &error
        );

        goto cleanup;
    }

    relation_service =
        relation_service_new(
            database,
            &error
        );

    if (relation_service == NULL)
    {
        application_present_error(
            application,
            "Ajout de relation impossible",
            error != NULL
                ? error->message
                : "Impossible de préparer le service des relations."
        );

        g_clear_error(
            &error
        );

        goto cleanup;
    }

    /*
     * Aucune preuve n'est associée pendant ce premier parcours.
     * Le service accepte donc un tableau NULL.
     */
    if (!relation_service_create(
            relation_service,
            relation_record,
            NULL,
            &error
        ))
    {
        g_warning(
            "Impossible d'enregistrer la relation '%s' : %s",
            relation_identifier,
            error != NULL
                ? error->message
                : "erreur inconnue"
        );

        application_present_error(
            application,
            "Enregistrement de la relation impossible",
            error != NULL
                ? error->message
                : "La relation n'a pas pu être enregistrée."
        );

        g_clear_error(
            &error
        );

        goto cleanup;
    }

    relation_created =
        TRUE;

    g_message(
        "Relation enregistrée : %s -> %s (%s), identifiant %s.",
        source_identifier,
        target_identifier,
        relation_type,
        relation_identifier
    );

    main_window_set_status(
        application->main_window,
        "Relation enregistrée. Actualisation du graphe…"
    );

    /*
     * Le graphe n'est rechargé qu'après le COMMIT réussi.
     * En cas d'échec SQLite, l'affichage courant reste intact.
     */
    application_start_graph_loading(
        application,
        database_path
    );

cleanup:

    if (!relation_created &&
        application->main_window != NULL)
    {
        main_window_set_status(
            application->main_window,
            "La relation n'a pas été enregistrée."
        );
    }

    relation_service_free(
        relation_service
    );

    relation_record_free(
        relation_record
    );

    if (current_date_time != NULL)
    {
        g_date_time_unref(
            current_date_time
        );
    }

    g_free(
        timestamp
    );

    g_free(
        relation_identifier
    );

    create_relation_dialog_result_free(
        result
    );
}

/**
 * @brief Ouvre le dialogue de création depuis la fiche d'une entité.
 */
static void application_on_add_relation_requested(
    const char *source_entity_identifier,
    gpointer user_data
)
{
    Application *application =
        user_data;

    Database *database =
        NULL;

    EntityDao *entity_dao =
        NULL;

    GPtrArray *entities =
        NULL;

    GError *error =
        NULL;

    if (application == NULL ||
        application->main_window == NULL ||
        application->session == NULL ||
        source_entity_identifier == NULL ||
        !g_uuid_string_is_valid(
            source_entity_identifier
        ))
    {
        return;
    }

    database =
        investigation_session_get_database(
            application->session
        );

    if (database == NULL)
    {
        application_present_error(
            application,
            "Relations indisponibles",
            "La session ne fournit aucune connexion SQLite."
        );

        return;
    }

    entity_dao =
        entity_dao_new(
            database,
            &error
        );

    if (entity_dao == NULL)
    {
        application_present_error(
            application,
            "Relations indisponibles",
            error != NULL
                ? error->message
                : "Impossible d'accéder aux entités."
        );

        g_clear_error(
            &error
        );

        return;
    }

    entities =
        entity_dao_list_all(
            entity_dao,
            &error
        );

    entity_dao_free(
        entity_dao
    );

    if (entities == NULL)
    {
        application_present_error(
            application,
            "Relations indisponibles",
            error != NULL
                ? error->message
                : "Impossible de charger les entités."
        );

        g_clear_error(
            &error
        );

        return;
    }

    if (!create_relation_dialog_present(
            main_window_get_window(
                application->main_window
            ),
            source_entity_identifier,
            entities,
            application_on_create_relation_completed,
            application,
            &error
        ))
    {
        application_present_error(
            application,
            "Ajout de relation impossible",
            error != NULL
                ? error->message
                : "Le dialogue de relation n'a pas pu être ouvert."
        );

        g_clear_error(
            &error
        );
    }

    g_ptr_array_unref(
        entities
    );
}

/**
 * @brief Enregistre le statut d'intégrité dans l'enquête d'origine.
 *
 * Une connexion indépendante est utilisée afin que la bonne base soit
 * mise à jour même si l'utilisateur a changé d'enquête entre-temps.
 */
static gboolean application_persist_evidence_integrity_status(
    const ApplicationEvidenceIntegrityContext *context,
    EvidenceIntegrityStatus integrity_status,
    GError **error
)
{
    Database *database =
        NULL;

    EvidenceDao *evidence_dao =
        NULL;

    gboolean updated =
        FALSE;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (context == NULL ||
        context->database_path == NULL ||
        context->database_path[0] == '\0' ||
        context->evidence_identifier == NULL ||
        !g_uuid_string_is_valid(
            context->evidence_identifier
        ))
    {
        g_set_error_literal(
            error,
            APPLICATION_OPEN_ERROR,
            APPLICATION_OPEN_ERROR_EVIDENCE,
            "Le contexte de mise à jour de l'intégrité est invalide."
        );

        return FALSE;
    }

    database =
        database_open(
            context->database_path
        );

    if (database == NULL)
    {
        g_set_error(
            error,
            APPLICATION_OPEN_ERROR,
            APPLICATION_OPEN_ERROR_EVIDENCE,
            "Impossible d'ouvrir la base SQLite : %s",
            context->database_path
        );

        return FALSE;
    }

    evidence_dao =
        evidence_dao_new(
            database,
            error
        );

    if (evidence_dao == NULL)
    {
        database_close(
            database
        );

        return FALSE;
    }

    updated =
        evidence_dao_update_integrity_status(
            evidence_dao,
            context->evidence_identifier,
            integrity_status,
            error
        );

    evidence_dao_free(
        evidence_dao
    );

    database_close(
        database
    );

    return updated;
}

/**
 * @brief Traite la fin d'une vérification d'intégrité.
 */
static void application_on_evidence_integrity_completed(
    BackgroundTask *task,
    gpointer user_data
)
{
    ApplicationEvidenceIntegrityContext *context =
        user_data;

    Application *application =
        NULL;

    const EvidenceIntegrityVerificationResult *result =
        NULL;

    EvidenceIntegrityStatus integrity_status =
        EVIDENCE_INTEGRITY_STATUS_ERROR;

    BackgroundTaskState task_state;

    const char *diagnostic =
        NULL;

    const char *display_name =
        NULL;

    char *status_message =
        NULL;

    GError *error =
        NULL;

    GError *refresh_error =
        NULL;

    gboolean session_matches =
        FALSE;

    gboolean evidence_was_selected =
        FALSE;

    if (task == NULL ||
        context == NULL)
    {
        return;
    }

    application =
        context->application;

    if (application == NULL)
    {
        return;
    }

    task_state =
        background_task_get_state(
            task
        );

    session_matches =
        application_session_matches_root(
            application,
            context->investigation_root_path
        );

    display_name =
        context->original_name != NULL &&
        context->original_name[0] != '\0'
            ? context->original_name
            : context->evidence_identifier;

    if (task_state ==
        BACKGROUND_TASK_STATE_COMPLETED)
    {
        result =
            background_task_get_result(
                task
            );

        if (result == NULL)
        {
            if (session_matches)
            {
                application_present_error(
                    application,
                    "Vérification incomplète",
                    "La tâche s'est terminée sans fournir de résultat."
                );
            }

            return;
        }

        integrity_status =
            evidence_integrity_verification_result_get_status(
                result
            );

        /*
         * UNKNOWN après une vérification complète serait incohérent.
         */
        if (integrity_status ==
            EVIDENCE_INTEGRITY_STATUS_UNKNOWN)
        {
            integrity_status =
                EVIDENCE_INTEGRITY_STATUS_ERROR;
        }

        diagnostic =
            evidence_integrity_verification_result_get_diagnostic(
                result
            );

        if (!application_persist_evidence_integrity_status(
                context,
                integrity_status,
                &error
            ))
        {
            g_warning(
                "Impossible d'enregistrer le statut de la preuve '%s' : %s",
                context->evidence_identifier,
                error != NULL
                    ? error->message
                    : "erreur inconnue"
            );

            if (session_matches)
            {
                application_present_error(
                    application,
                    "Statut non enregistré",
                    error != NULL
                        ? error->message
                        : "Le résultat n'a pas pu être enregistré."
                );
            }

            g_clear_error(
                &error
            );

            return;
        }

        if (session_matches)
        {
            /*
             * La reconstruction du modèle peut provoquer temporairement
             * une sélection vide. On mémorise donc l'état avant celle-ci.
             */
            evidence_was_selected =
                g_strcmp0(
                    application->selected_evidence_identifier,
                    context->evidence_identifier
                ) == 0;

            if (!application_refresh_evidence_models(
                    application,
                    &refresh_error
                ))
            {
                g_warning(
                    "Le statut a été enregistré, mais la liste "
                    "des preuves n'a pas été actualisée : %s",
                    refresh_error != NULL
                        ? refresh_error->message
                        : "erreur inconnue"
                );
            }

            g_clear_error(
                &refresh_error
            );

            /*
             * On ne force pas l'affichage d'une ancienne preuve lorsque
             * l'utilisateur en a sélectionné une autre entre-temps.
             */
            if (evidence_was_selected)
            {
                application_on_evidence_selected(
                    context->evidence_identifier,
                    application
                );
            }

            switch (integrity_status)
            {
                case EVIDENCE_INTEGRITY_STATUS_VALID:
                    status_message =
                        g_strdup_printf(
                            "Intégrité valide : %s",
                            display_name
                        );

                    break;

                case EVIDENCE_INTEGRITY_STATUS_MODIFIED:
                    status_message =
                        g_strdup_printf(
                            "Alerte d'intégrité — fichier modifié : %s",
                            display_name
                        );

                    break;

                case EVIDENCE_INTEGRITY_STATUS_MISSING:
                    status_message =
                        g_strdup_printf(
                            "Preuve absente du disque : %s",
                            display_name
                        );

                    break;

                case EVIDENCE_INTEGRITY_STATUS_ERROR:
                case EVIDENCE_INTEGRITY_STATUS_UNKNOWN:
                default:
                    status_message =
                        g_strdup_printf(
                            "Erreur de vérification : %s",
                            display_name
                        );

                    break;
            }

            main_window_set_status(
                application->main_window,
                status_message
            );

            if (integrity_status ==
                    EVIDENCE_INTEGRITY_STATUS_ERROR &&
                diagnostic != NULL &&
                diagnostic[0] != '\0')
            {
                application_present_error(
                    application,
                    "Vérification impossible",
                    diagnostic
                );
            }
        }

        g_message(
            "Vérification de '%s' terminée avec le statut %d.",
            context->evidence_identifier,
            (int) integrity_status
        );

        g_free(
            status_message
        );

        return;
    }

    if (task_state ==
        BACKGROUND_TASK_STATE_CANCELLED)
    {
        if (session_matches)
        {
            main_window_set_status(
                application->main_window,
                "Vérification d'intégrité annulée."
            );
        }

        return;
    }

    error =
        background_task_dup_error(
            task
        );

    g_warning(
        "La vérification de la preuve '%s' a échoué : %s",
        context->evidence_identifier,
        error != NULL
            ? error->message
            : "erreur inconnue"
    );

    if (session_matches)
    {
        application_present_error(
            application,
            "Vérification impossible",
            error != NULL
                ? error->message
                : "La vérification n'a pas pu être exécutée."
        );
    }

    g_clear_error(
        &error
    );
}

/**
 * @brief Charge la preuve et démarre sa vérification asynchrone.
 */
static void application_start_evidence_integrity_verification(
    Application *application,
    const char *evidence_identifier
)
{
    const InvestigationProject *project =
        NULL;

    Database *database =
        NULL;

    EvidenceDao *evidence_dao =
        NULL;

    EvidenceRecord *evidence_record =
        NULL;

    ApplicationEvidenceIntegrityContext *context =
        NULL;

    EvidenceIntegrityTaskRequest request =
        {0};

    BackgroundTask *task =
        NULL;

    const char *root_path =
        NULL;

    const char *database_path =
        NULL;

    const char *relative_path =
        NULL;

    const char *expected_sha256 =
        NULL;

    const char *original_name =
        NULL;

    char *status_message =
        NULL;

    GError *error =
        NULL;

    if (application == NULL ||
        application->main_window == NULL ||
        application->session == NULL ||
        application->task_manager == NULL ||
        evidence_identifier == NULL ||
        !g_uuid_string_is_valid(
            evidence_identifier
        ))
    {
        return;
    }

    project =
        investigation_session_get_project(
            application->session
        );

    database =
        investigation_session_get_database(
            application->session
        );

    if (project == NULL ||
        database == NULL)
    {
        application_present_error(
            application,
            "Vérification impossible",
            "La session d'enquête est invalide."
        );

        return;
    }

    root_path =
        investigation_project_get_root_path(
            project
        );

    database_path =
        investigation_project_get_database_path(
            project
        );

    if (root_path == NULL ||
        root_path[0] == '\0' ||
        database_path == NULL ||
        database_path[0] == '\0')
    {
        application_present_error(
            application,
            "Vérification impossible",
            "Les chemins de l'enquête sont invalides."
        );

        return;
    }

    evidence_dao =
        evidence_dao_new(
            database,
            &error
        );

    if (evidence_dao == NULL)
    {
        application_present_error(
            application,
            "Vérification impossible",
            error != NULL
                ? error->message
                : "Impossible d'accéder aux preuves."
        );

        g_clear_error(
            &error
        );

        return;
    }

    evidence_record =
        evidence_dao_find_by_identifier(
            evidence_dao,
            evidence_identifier,
            &error
        );

    evidence_dao_free(
        evidence_dao
    );

    evidence_dao =
        NULL;

    if (error != NULL)
    {
        application_present_error(
            application,
            "Vérification impossible",
            error->message
        );

        g_clear_error(
            &error
        );

        return;
    }

    if (evidence_record == NULL)
    {
        application_present_error(
            application,
            "Preuve introuvable",
            "La preuve n'existe plus dans la base SQLite."
        );

        return;
    }

    relative_path =
        evidence_record_get_relative_path(
            evidence_record
        );

    expected_sha256 =
        evidence_record_get_sha256(
            evidence_record
        );

    original_name =
        evidence_record_get_original_name(
            evidence_record
        );

    if (relative_path == NULL ||
        relative_path[0] == '\0' ||
        expected_sha256 == NULL ||
        expected_sha256[0] == '\0')
    {
        application_present_error(
            application,
            "Vérification impossible",
            "La preuve ne contient pas les données nécessaires."
        );

        evidence_record_free(
            evidence_record
        );

        return;
    }

    context =
        g_try_new0(
            ApplicationEvidenceIntegrityContext,
            1
        );

    if (context == NULL)
    {
        application_present_error(
            application,
            "Vérification impossible",
            "Impossible d'allouer le contexte de la tâche."
        );

        evidence_record_free(
            evidence_record
        );

        return;
    }

    context->application =
        application;

    context->investigation_root_path =
        g_strdup(
            root_path
        );

    context->database_path =
        g_strdup(
            database_path
        );

    context->evidence_identifier =
        g_strdup(
            evidence_identifier
        );

    context->original_name =
        g_strdup(
            original_name
        );

    if (context->investigation_root_path == NULL ||
        context->database_path == NULL ||
        context->evidence_identifier == NULL)
    {
        application_evidence_integrity_context_free(
            context
        );

        evidence_record_free(
            evidence_record
        );

        application_present_error(
            application,
            "Vérification impossible",
            "Impossible de copier les paramètres de la tâche."
        );

        return;
    }

    request.investigation_root_path =
        root_path;

    request.relative_path =
        relative_path;

    request.expected_sha256 =
        expected_sha256;

    task =
        evidence_integrity_task_start(
            application->task_manager,
            &request,
            application_on_evidence_integrity_completed,
            context,
            application_evidence_integrity_context_free,
            &error
        );

    if (task == NULL)
    {
        /*
         * La propriété du contexte n'a pas été transférée.
         */
        application_evidence_integrity_context_free(
            context
        );

        application_present_error(
            application,
            "Vérification impossible",
            error != NULL
                ? error->message
                : "La tâche n'a pas pu être démarrée."
        );

        g_clear_error(
            &error
        );

        evidence_record_free(
            evidence_record
        );

        return;
    }

    /*
     * BackgroundTask et TaskManager possèdent désormais leurs références.
     */
    background_task_unref(
        task
    );

    status_message =
        g_strdup_printf(
            "Vérification en cours : %s",
            original_name != NULL &&
            original_name[0] != '\0'
                ? original_name
                : evidence_identifier
        );

    main_window_set_status(
        application->main_window,
        status_message
    );

    g_free(
        status_message
    );

    evidence_record_free(
        evidence_record
    );
}

/**
 * @brief Reçoit la demande de vérification d'une preuve.
 *
 * Cette première étape valide uniquement le relais de l'interface.
 * Le lancement de la tâche sera ajouté ensuite.
 *
 * @param evidence_identifier UUID de la preuve.
 * @param user_data Pointeur vers Application.
 */
/**
 * @brief Traite la demande de vérification provenant du Workspace.
 */
static void application_on_verify_evidence_requested(
    const char *evidence_identifier,
    gpointer user_data
)
{
    Application *application =
        user_data;

    application_start_evidence_integrity_verification(
        application,
        evidence_identifier
    );
}

/**
 * @brief Ferme proprement l'application.
 *
 * @param user_data Pointeur vers Application.
 */
/**
 * @brief Enregistre la nouvelle position logique d'un nœud.
 *
 * Le modèle en mémoire est mis à jour avant l'UPSERT SQLite. En cas d'échec
 * SQLite, le déplacement reste visible pendant la session, mais un message
 * explicite indique qu'il ne survivra pas à la réouverture de l'enquête.
 */
static void application_on_graph_node_moved(
    const char *entity_identifier,
    double x,
    double y,
    gpointer user_data
)
{
    Application *application =
        user_data;

    Database *database =
        NULL;

    GraphNodePositionDao *position_dao =
        NULL;

    GError *error =
        NULL;

    if (application == NULL ||
        application->main_window == NULL ||
        application->session == NULL ||
        application->graph_layout == NULL ||
        entity_identifier == NULL ||
        !g_uuid_string_is_valid(
            entity_identifier
        ))
    {
        return;
    }

    if (!investigation_graph_layout_set_position(
            application->graph_layout,
            entity_identifier,
            x,
            y,
            &error
        ))
    {
        g_warning(
            "Impossible de mettre à jour la position du nœud '%s' : %s",
            entity_identifier,
            error != NULL
                ? error->message
                : "erreur inconnue"
        );

        application_present_error(
            application,
            "Position invalide",
            error != NULL
                ? error->message
                : "La position du nœud ne peut pas être conservée."
        );

        g_clear_error(
            &error
        );

        return;
    }

    database =
        investigation_session_get_database(
            application->session
        );

    if (database == NULL)
    {
        main_window_set_status(
            application->main_window,
            "Nœud déplacé, mais position non enregistrée."
        );

        application_present_error(
            application,
            "Enregistrement impossible",
            "La session ne fournit aucune connexion SQLite."
        );

        return;
    }

    position_dao =
        graph_node_position_dao_new(
            database,
            &error
        );

    if (position_dao == NULL)
    {
        g_warning(
            "Impossible de créer le DAO des positions : %s",
            error != NULL
                ? error->message
                : "erreur inconnue"
        );

        main_window_set_status(
            application->main_window,
            "Nœud déplacé, mais position non enregistrée."
        );

        application_present_error(
            application,
            "Enregistrement impossible",
            error != NULL
                ? error->message
                : "Impossible d'accéder aux positions du graphe."
        );

        g_clear_error(
            &error
        );

        return;
    }

    if (!graph_node_position_dao_upsert(
            position_dao,
            entity_identifier,
            x,
            y,
            &error
        ))
    {
        g_warning(
            "Impossible d'enregistrer la position du nœud '%s' : %s",
            entity_identifier,
            error != NULL
                ? error->message
                : "erreur inconnue"
        );

        main_window_set_status(
            application->main_window,
            "Nœud déplacé, mais position non enregistrée."
        );

        application_present_error(
            application,
            "Enregistrement impossible",
            error != NULL
                ? error->message
                : "La position du nœud n'a pas pu être enregistrée."
        );

        g_clear_error(
            &error
        );

        graph_node_position_dao_free(
            position_dao
        );

        return;
    }

    graph_node_position_dao_free(
        position_dao
    );

    main_window_set_status(
        application->main_window,
        "Position du nœud enregistrée."
    );
}

/**
 * @brief Supprime les positions persistées et restaure la grille automatique.
 *
 * Cette fonction est appelée uniquement après une confirmation explicite.
 * L'affichage et le modèle en mémoire restent inchangés en cas d'échec SQLite.
 */
static void application_reset_graph_layout(
    Application *application
)
{
    Database *database =
        NULL;

    GraphNodePositionDao *position_dao =
        NULL;

    GError *error =
        NULL;

    if (application == NULL ||
        application->main_window == NULL ||
        application->session == NULL ||
        application->graph_model == NULL ||
        application->graph_layout == NULL)
    {
        return;
    }

    database =
        investigation_session_get_database(
            application->session
        );

    if (database == NULL)
    {
        application_present_error(
            application,
            "Réinitialisation impossible",
            "La session ne fournit aucune connexion SQLite."
        );

        return;
    }

    position_dao =
        graph_node_position_dao_new(
            database,
            &error
        );

    if (position_dao == NULL)
    {
        g_warning(
            "Impossible de créer le DAO des positions : %s",
            error != NULL
                ? error->message
                : "erreur inconnue"
        );

        application_present_error(
            application,
            "Réinitialisation impossible",
            error != NULL
                ? error->message
                : "Impossible d'accéder aux positions enregistrées."
        );

        g_clear_error(
            &error
        );

        return;
    }

    if (!graph_node_position_dao_delete_all(
            position_dao,
            &error
        ))
    {
        g_warning(
            "Impossible de supprimer les positions du graphe : %s",
            error != NULL
                ? error->message
                : "erreur inconnue"
        );

        application_present_error(
            application,
            "Réinitialisation impossible",
            error != NULL
                ? error->message
                : "Les positions enregistrées n'ont pas pu être supprimées."
        );

        g_clear_error(
            &error
        );

        graph_node_position_dao_free(
            position_dao
        );

        return;
    }

    graph_node_position_dao_free(
        position_dao
    );

    investigation_graph_layout_clear(
        application->graph_layout
    );

    main_window_reset_graph_layout(
        application->main_window
    );

    main_window_set_status(
        application->main_window,
        "Disposition du graphe réinitialisée."
    );
}

/**
 * @brief Traite la réponse du dialogue de confirmation.
 */
static void application_on_reset_graph_layout_confirmed(
    gboolean confirmed,
    gpointer user_data
)
{
    Application *application =
        user_data;

    if (!confirmed)
    {
        return;
    }

    application_reset_graph_layout(
        application
    );
}

/**
 * @brief Demande confirmation avant toute suppression de position.
 */
static void application_on_show_graph_requested(
    gpointer user_data
)
{
    Application *application =
        user_data;

    if (application == NULL ||
        application->main_window == NULL ||
        application->graph_model == NULL)
    {
        return;
    }

    application_set_selected_evidence_identifier(
        application,
        NULL
    );

    /*
     * Passer NULL restaure l'état principal du Workspace. Quand le graphe
     * est prêt, workspace_show_default() affiche automatiquement le canvas.
     */
    main_window_set_selected_evidence(
        application->main_window,
        NULL
    );

    main_window_set_status(
        application->main_window,
        "Graphe de l'enquête affiché."
    );
}

/**
 * @brief Demande confirmation avant toute suppression de position.
 */
static void application_on_reset_graph_layout_requested(
    gpointer user_data
)
{
    Application *application =
        user_data;

    if (application == NULL ||
        application->main_window == NULL ||
        application->session == NULL ||
        application->graph_model == NULL ||
        application->graph_layout == NULL)
    {
        return;
    }

    application_message_dialog_present_confirmation(
        main_window_get_window(
            application->main_window
        ),
        APPLICATION_MESSAGE_DIALOG_WARNING,
        "Réinitialiser la disposition du graphe",
        "Toutes les positions personnalisées des nœuds seront supprimées. "
        "Le graphe retrouvera sa disposition automatique.\n\n"
        "Cette action ne peut pas être annulée.",
        "Réinitialiser",
        application_on_reset_graph_layout_confirmed,
        application
    );
}

/**
 * @brief Ferme proprement l'application.
 *
 * @param user_data Pointeur vers Application.
 */
static void application_on_quit_requested(
    gpointer user_data
)
{
    Application *application = user_data;

    if (application == NULL ||
        application->gtk_application == NULL)
    {
        return;
    }

    task_manager_cancel_all(
        application->task_manager
    );

    g_application_quit(
        G_APPLICATION(
            application->gtk_application
        )
    );
}

/**
 * @brief Crée la fenêtre principale lors de l'activation.
 *
 * Le signal activate peut être reçu plusieurs fois. Si la fenêtre existe
 * déjà, elle est simplement présentée.
 *
 * @param gtk_application Application GTK ayant reçu le signal.
 * @param user_data Pointeur vers Application.
 */
static void application_on_activate(
    GtkApplication *gtk_application,
    gpointer user_data
)
{
    Application *application = user_data;

    if (application == NULL)
    {
        return;
    }

    if (application->main_window != NULL)
    {
        main_window_present(
            application->main_window
        );

        return;
    }

    application->main_window = main_window_new(
        gtk_application,
        application->task_manager
    );

    if (application->main_window == NULL)
    {
        g_warning(
            "Impossible de créer la fenêtre principale."
        );

        return;
    }

    main_window_set_tree_selection_callback(
        application->main_window,
        application_on_tree_node_selected,
        application
    );

    main_window_set_evidence_selection_callback(
        application->main_window,
        application_on_evidence_selected,
        application
    );

    main_window_set_verify_evidence_callback(
        application->main_window,
        application_on_verify_evidence_requested,
        application
    );

    main_window_set_graph_node_moved_callback(
        application->main_window,
        application_on_graph_node_moved,
        application
    );

    main_window_set_reset_graph_layout_callback(
        application->main_window,
        application_on_reset_graph_layout_requested,
        application
    );

    main_window_set_show_graph_callback(
        application->main_window,
        application_on_show_graph_requested,
        application
    );

    main_window_set_add_relation_callback(
        application->main_window,
        application_on_add_relation_requested,
        application
    );

    main_window_set_new_investigation_callback(
        application->main_window,
        application_on_new_investigation_requested,
        application
    );

    main_window_set_open_investigation_callback(
        application->main_window,
        application_on_open_investigation_requested,
        application
    );

    main_window_set_import_evidence_callback(
        application->main_window,
        application_on_import_evidence_requested,
        application
    );

    main_window_set_import_evidence_enabled(
        application->main_window,
        application->session != NULL
    );

    main_window_set_quit_callback(
        application->main_window,
        application_on_quit_requested,
        application
    );

    main_window_present(
        application->main_window
    );

    /*
     * Le démarrage est non bloquant. Le worker s’exécute dans un
     * thread secondaire et sa progression apparaît dans TaskPanel.
     */
    application_start_tool_initialization(
        application
    );
}

Application *application_new(void)
{
    Application *application = NULL;
    GError *error = NULL;

    application = g_new0(
        Application,
        1
    );

    application->task_manager =
        task_manager_new();

    if (application->task_manager == NULL)
    {
        g_free(
            application
        );

        return NULL;
    }

    application->tool_initializer =
        tool_initializer_new(
            application->task_manager,
            &error
        );

    if (application->tool_initializer == NULL)
    {
        g_warning(
            "Impossible de créer l’initialiseur des outils : %s",
            error != NULL
            ? error->message
            : "erreur inconnue"
        );

        g_clear_error(
            &error
        );

        task_manager_free(
            application->task_manager
        );

        g_free(
            application
        );

        return NULL;
    }

    tool_initializer_set_completed_callback(
        application->tool_initializer,
        application_on_tool_initialization_completed,
        application,
        NULL
    );

    application->gtk_application = gtk_application_new(
        APPLICATION_ID,
        G_APPLICATION_DEFAULT_FLAGS
    );

    if (application->gtk_application == NULL)
    {
        tool_initializer_free(
            application->tool_initializer
        );

        task_manager_free(
            application->task_manager
        );

        g_free(
            application
        );

        return NULL;
    }

    g_signal_connect(
        application->gtk_application,
        "activate",
        G_CALLBACK(application_on_activate),
        application
    );

    return application;
}

int application_run(
    Application *application,
    int argc,
    char **argv
)
{
    if (application == NULL ||
        application->gtk_application == NULL)
    {
        return 1;
    }

    return g_application_run(
        G_APPLICATION(
            application->gtk_application
        ),
        argc,
        argv
    );
}

const InvestigationSession *application_get_session(
    const Application *application
)
{
    if (application == NULL)
    {
        return NULL;
    }

    return application->session;
}

void application_free(
    Application *application
)
{
    if (application == NULL)
    {
        return;
    }

    if (application->tool_initializer != NULL)
    {
        tool_initializer_set_completed_callback(
            application->tool_initializer,
            NULL,
            NULL,
            NULL
        );
    }

    task_manager_cancel_all(
        application->task_manager
    );

    application_cancel_graph_loading(
        application
    );

    application_clear_graph(
        application
    );

    investigation_tree_model_free(
        application->tree_model
    );

    investigation_session_close(
        application->session
    );

    if (application->main_window != NULL)
    {
        main_window_set_evidence_model(
            application->main_window,
            NULL
        );
    }

    evidence_category_model_free(
        application->evidence_category_model
    );

    evidence_list_model_free(
        application->evidence_list_model
    );

    /*
     * MainWindow contient TaskPanel.
     * Il doit être détruit avant TaskManager.
     */
    main_window_free(
        application->main_window
    );

    /*
     * ToolInitializer emprunte TaskManager.
     * Il doit être libéré avant celui-ci.
     */
    tool_initializer_free(
        application->tool_initializer
    );

    task_manager_free(
        application->task_manager
    );

    if (application->gtk_application != NULL)
    {
        g_object_unref(
            application->gtk_application
        );
    }

    g_clear_pointer(
        &application->selected_evidence_identifier,
        g_free
    );

    g_free(
        application
    );
}
