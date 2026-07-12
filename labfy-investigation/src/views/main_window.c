/******************************************************************************
 * @file main_window.c
 * @brief Implémentation de la fenêtre principale.
 ******************************************************************************/

#include "views/main_window.h"

#include <glib.h>

/**
 * @brief Largeur initiale de la fenêtre principale.
 */
#define MAIN_WINDOW_DEFAULT_WIDTH 1000

/**
 * @brief Hauteur initiale de la fenêtre principale.
 */
#define MAIN_WINDOW_DEFAULT_HEIGHT 650

/**
 * @struct MainWindow
 * @brief Représentation interne de la fenêtre principale.
 *
 * Cette structure reste privée au module. Les autres fichiers utilisent
 * uniquement le type opaque déclaré dans main_window.h.
 */
struct MainWindow
{
    GtkWindow *window;
    GtkWidget *status_label;
};

MainWindow *main_window_new(GtkApplication *application)
{
    MainWindow *main_window = NULL;
    GtkWidget *main_box = NULL;
    GtkWidget *workspace = NULL;

    if (application == NULL)
    {
        return NULL;
    }

    main_window = g_new0(MainWindow, 1);

    main_window->window = GTK_WINDOW(
        gtk_application_window_new(application)
    );

    if (main_window->window == NULL)
    {
        main_window_free(main_window);
        return NULL;
    }

    gtk_window_set_title(
        main_window->window,
        "Labfy Investigation"
    );

    gtk_window_set_default_size(
        main_window->window,
        MAIN_WINDOW_DEFAULT_WIDTH,
        MAIN_WINDOW_DEFAULT_HEIGHT
    );

    main_box = gtk_box_new(
        GTK_ORIENTATION_VERTICAL,
        0
    );

    workspace = gtk_box_new(
        GTK_ORIENTATION_VERTICAL,
        0
    );

    gtk_widget_set_hexpand(workspace, TRUE);
    gtk_widget_set_vexpand(workspace, TRUE);

    main_window->status_label = gtk_label_new(
        "Aucune enquête ouverte"
    );

    gtk_widget_set_halign(
        main_window->status_label,
        GTK_ALIGN_START
    );

    gtk_widget_set_margin_start(
        main_window->status_label,
        8
    );

    gtk_widget_set_margin_end(
        main_window->status_label,
        8
    );

    gtk_widget_set_margin_top(
        main_window->status_label,
        6
    );

    gtk_widget_set_margin_bottom(
        main_window->status_label,
        6
    );

    gtk_box_append(
        GTK_BOX(main_box),
        workspace
    );

    gtk_box_append(
        GTK_BOX(main_box),
        main_window->status_label
    );

    gtk_window_set_child(
        main_window->window,
        main_box
    );

    return main_window;
}

void main_window_present(MainWindow *main_window)
{
    if (main_window == NULL || main_window->window == NULL)
    {
        return;
    }

    gtk_window_present(main_window->window);
}

GtkWindow *main_window_get_window(
    const MainWindow *main_window
)
{
    if (main_window == NULL)
    {
        return NULL;
    }

    return main_window->window;
}

void main_window_free(MainWindow *main_window)
{
    if (main_window == NULL)
    {
        return;
    }

    /*
     * La fenêtre principale est possédée et détruite par GTK.
     * MainWindow ne possède que sa structure d'encapsulation.
     */

    g_free(main_window);
}
