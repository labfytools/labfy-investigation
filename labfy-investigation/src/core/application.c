/******************************************************************************
 * @file application.c
 * @brief Implémentation du cycle de vie de l'application GTK.
 ******************************************************************************/

#include "core/application.h"
#include "views/folder_dialog.h"
#include "core/investigation.h"

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
    GtkWindow *window = GTK_WINDOW(user_data);
    Investigation *investigation = NULL;
    const char *root_path = NULL;
    const char *database_path = NULL;

    if (folder_path == NULL)
    {
        g_print("Sélection annulée.\n");
        return;
    }

    investigation = investigation_new(folder_path);

    if (investigation == NULL)
    {
        g_warning("Impossible de créer l'enquête à partir du dossier sélectionné.");
        return;
    }

    root_path = investigation_get_root_path(investigation);
    database_path = investigation_get_database_path(investigation);

    g_print("Dossier racine : %s\n", root_path);
    g_print("Base de données : %s\n", database_path);

    investigation_free(investigation);

    gtk_window_set_title(window, "Labfy Investigation");
}
static void application_on_activate(GtkApplication *gtk_application,
                                    gpointer user_data)
{
    GtkWidget *window = NULL;

    (void)user_data;

    window = gtk_application_window_new(gtk_application);

    gtk_window_set_title(GTK_WINDOW(window), "Labfy Investigation");
    gtk_window_set_default_size(
        GTK_WINDOW(window),
        DEFAULT_WINDOW_WIDTH,
        DEFAULT_WINDOW_HEIGHT
    );

    gtk_window_present(GTK_WINDOW(window));

    folder_dialog_select_folder(
        GTK_WINDOW(window),
        application_on_folder_selected,
        window
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

int application_run(Application *application, int argc, char **argv)
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

    if (application->gtk_application != NULL)
    {
        g_object_unref(application->gtk_application);
    }

    g_free(application);
}

