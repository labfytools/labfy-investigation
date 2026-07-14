/******************************************************************************
 * @file application.c
 * @brief Implémentation du cycle de vie de l'application GTK.
 ******************************************************************************/

#include "core/application.h"
#include "views/folder_dialog.h"
#include "core/investigation.h"
#include "views/main_window.h"
#include "core/investigation_tree_builder.h"
#include "core/investigation_tree_model.h"

#include <gtk/gtk.h>

/**
 * @brief Identifiant unique utilisé par GLib pour l'application.
 */
#define APPLICATION_ID "com.labfytools.investigation"

/**
 * @brief Dimensions initiales de la fenêtre principale.
 */
#define DEFAULT_WINDOW_WIDTH 1000
#define DEFAULT_WINDOW_HEIGHT 650

/**
 * @struct Application
 * @brief État interne de l'application.
 *
 * Cette structure reste privée à ce module afin d'empêcher les autres
 * composants de manipuler directement les objets GTK.
 */
struct Application
{
    GtkApplication *gtk_application;
    MainWindow *main_window;
    Investigation *investigation;
    InvestigationTreeModel *tree_model;
};

/**
 * @brief Crée la fenêtre minimale lors de l'activation de l'application.
 *
 * Cette fonction est privée au module. Elle est appelée par GTK lorsque
 * l'application reçoit le signal "activate".
 *
 * @param gtk_application Application GTK ayant reçu le signal.
 * @param user_data Données utilisateur associées au signal.
 */

static void application_on_folder_selected(
    const char *folder_path,
    gpointer user_data
)
{
    Application *application = user_data;
    Investigation *new_investigation = NULL;
    InvestigationTreeModel *new_tree_model = NULL;

    if (application == NULL)
    {
        return;
    }

    if (folder_path == NULL)
    {
        g_print("Sélection annulée.\n");
        return;
    }

    new_investigation = investigation_new(folder_path);

    if (new_investigation == NULL)
    {
        g_warning(
            "Impossible de créer l'enquête à partir du dossier sélectionné."
        );
        return;
    }

    new_tree_model = investigation_tree_builder_build(
        investigation_get_root_path(new_investigation)
    );

    if (new_tree_model == NULL)
    {
        g_warning(
            "Impossible de construire l'arborescence de l'enquête."
        );

        investigation_free(new_investigation);
        return;
    }

    /*
     * Les nouveaux objets sont valides.
     * On peut maintenant remplacer les anciens sans perdre l'enquête
     * déjà ouverte en cas d'échec.
     */
    investigation_tree_model_free(application->tree_model);
    investigation_free(application->investigation);

    application->tree_model = new_tree_model;
    application->investigation = new_investigation;

    main_window_set_tree_model(
        application->main_window,
        application->tree_model
    );
}

/**
 * @brief Traite la sélection d'un nœud dans l'arborescence.
 *
 * @param node      Nœud sélectionné, ou NULL si aucune sélection.
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

    if (node == NULL)
    {
        g_print("Aucun nœud sélectionné.\n");
        return;
    }

    node_name = investigation_node_get_name(node);
    node_type = investigation_node_get_type(node);

    g_print(
        "Nœud sélectionné : %s\n",
        node_name != NULL ? node_name : "(sans nom)"
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

    /*
     * Le signal "activate" peut être reçu plusieurs fois. Si la fenêtre
     * existe déjà, il suffit de la présenter au lieu d'en créer une seconde.
     */
    if (application->main_window != NULL)
    {
        main_window_present(application->main_window);
        return;
    }

    application->main_window = main_window_new(gtk_application);

    if (application->main_window == NULL)
    {
        g_warning("Impossible de créer la fenêtre principale.");
        return;
    }

    main_window_set_tree_selection_callback(
        application->main_window,
        application_on_tree_node_selected,
        application
    );
    
    main_window_present(application->main_window);

    folder_dialog_select_folder(
        main_window_get_window(application->main_window),
        application_on_folder_selected,
        application
    );
}

Application *application_new(void)
{
    Application *application = NULL;

    application = g_new0(Application, 1);

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
    if (application == NULL || application->gtk_application == NULL)
    {
        return 1;
    }

    return g_application_run(
        G_APPLICATION(application->gtk_application),
        argc,
        argv
    );
}

void application_free(Application *application)
{
    if (application == NULL)
    {
        return;
    }

    investigation_tree_model_free(application->tree_model);
    investigation_free(application->investigation);
    main_window_free(application->main_window);

    if (application->gtk_application != NULL)
    {
        g_object_unref(application->gtk_application);
    }

    g_free(application);
}

