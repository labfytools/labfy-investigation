/******************************************************************************
 * @file application.c
 * @brief Implémentation du cycle de vie de l'application GTK.
 ******************************************************************************/

#include "core/application.h"

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
