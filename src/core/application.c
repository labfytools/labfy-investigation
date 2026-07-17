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

#include <gtk/gtk.h>

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
};

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
        gtk_application
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

    main_window_set_quit_callback(
        application->main_window,
        application_on_quit_requested,
        application
    );

    main_window_present(
        application->main_window
    );
}

Application *application_new(void)
{
    Application *application = NULL;

    application = g_new0(
        Application,
        1
    );

    application->gtk_application = gtk_application_new(
        APPLICATION_ID,
        G_APPLICATION_DEFAULT_FLAGS
    );

    if (application->gtk_application == NULL)
    {
        g_free(application);
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

    investigation_tree_model_free(
        application->tree_model
    );

    investigation_session_close(
        application->session
    );

    main_window_free(
        application->main_window
    );

    if (application->gtk_application != NULL)
    {
        g_object_unref(
            application->gtk_application
        );
    }

    g_free(application);
}
