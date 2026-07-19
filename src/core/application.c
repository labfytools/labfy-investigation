/******************************************************************************
 * @file application.c
 * @brief Implémentation du cycle de vie de l'application GTK.
 ******************************************************************************/

#include "core/application.h"

#include "core/investigation_node.h"
#include "core/investigation_project.h"
#include "core/investigation_session.h"
#include "core/investigation_tree_model.h"
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

#include <gtk/gtk.h>
#include <errno.h>

/**
 * @brief Identifiant unique utilisé par GLib pour l'application.
 */
#define APPLICATION_ID "com.labfytools.investigation"

/**
 * @brief Dossier physique recevant les documents importés.
 */
#define APPLICATION_EVIDENCE_DIRECTORY \
    "01_Preuves_Originales/Documents"

/**
 * @brief Type utilisé avant l’ajout du formulaire de métadonnées.
 */
#define APPLICATION_DEFAULT_EVIDENCE_TYPE \
    "document"

/**
 * @brief Erreurs produites lors du chargement d'une enquête.
 */
typedef enum
{
    APPLICATION_OPEN_ERROR_INVALID_ARGUMENT,
    APPLICATION_OPEN_ERROR_INVALID_PROJECT,
    APPLICATION_OPEN_ERROR_TREE_BUILD,
    APPLICATION_OPEN_ERROR_INSTALL
} ApplicationOpenError;

/**
 * @brief Domaine d'erreur du chargement d'une enquête.
 */
#define APPLICATION_OPEN_ERROR \
    application_open_error_quark()

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
};

/**
 * @brief Contexte conservé jusqu’à la fin d’un import.
 */
typedef struct
{
    Application *application;
    char *investigation_root_path;
} ApplicationEvidenceImportContext;

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
    const char *investigation_name = NULL;

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

    investigation_name = investigation_record_get_name(
        record
    );

    if (root_path == NULL ||
        root_path[0] == '\0' ||
        investigation_name == NULL ||
        investigation_name[0] == '\0')
    {
        return FALSE;
    }

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

    if (application_session_matches_root(
            application,
            context->investigation_root_path
        ))
    {
        if (application_refresh_investigation_tree(
                application,
                context->investigation_root_path
            ))
        {
            status_message =
                g_strdup_printf(
                    "Preuve importée : %s",
                    original_name != NULL
                        ? original_name
                        : "(nom indisponible)"
                );
        }
        else
        {
            /*
             * L’import reste valide même si le rafraîchissement visuel
             * échoue. Le fichier et la ligne SQLite ne doivent surtout
             * pas être présentés comme un échec.
             */
            status_message =
                g_strdup_printf(
                    "Preuve importée : %s — "
                    "arborescence non actualisée",
                    original_name != NULL
                        ? original_name
                        : "(nom indisponible)"
                );

            g_warning(
                "La preuve a été importée, mais l’arborescence de '%s' "
                "n’a pas pu être reconstruite.",
                context->investigation_root_path
            );
        }

        main_window_set_status(
            application->main_window,
            status_message
        );
    }
        g_message(
            "Preuve importée dans '%s' : %s",
            context->investigation_root_path,
            original_name != NULL
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
 * @brief Prépare et démarre l’import asynchrone d’un fichier.
 */
static void application_start_evidence_import(
    Application *application,
    const char *file_path
)
{
    const InvestigationProject *project = NULL;

    const char *root_path = NULL;
    const char *database_path = NULL;

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
        file_path[0] == '\0')
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

    destination_directory =
        g_build_filename(
            root_path,
            "01_Preuves_Originales",
            "Documents",
            NULL
        );

    if (destination_directory == NULL)
    {
        application_present_error(
            application,
            "Import impossible",
            "Impossible de construire le dossier de destination."
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
        APPLICATION_EVIDENCE_DIRECTORY;

    request.type_identifier =
        APPLICATION_DEFAULT_EVIDENCE_TYPE;

    request.collected_at = NULL;
    request.source = NULL;
    request.description = NULL;

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
    
    application_start_evidence_import(
        application,
        file_path
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

    investigation_tree_model_free(
        application->tree_model
    );

    investigation_session_close(
        application->session
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

    g_free(
        application
    );
}
