/******************************************************************************
 * @file main_window.c
 * @brief Implémentation de la fenêtre principale.
 ******************************************************************************/

#include "views/main_window.h"
#include "widgets/sidebar.h"
#include "widgets/workspace.h"

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
 * @brief Position initiale de la séparation horizontale.
 */
#define MAIN_WINDOW_SIDEBAR_POSITION 250

/**
 * @struct MainWindow
 * @brief Représentation interne de la fenêtre principale.
 *
 * Cette structure reste privée au module. Elle conserve les composants
 * nécessaires à l'organisation générale de l'interface.
 */
struct MainWindow
{
    GtkWindow *window;
    GtkWidget *main_box;
    GtkWidget *main_paned;
    Workspace *workspace;
    GtkWidget *status_label;
    Sidebar *sidebar;
};

MainWindow *main_window_new(GtkApplication *application)
{
    MainWindow *main_window = NULL;
    GtkWidget *sidebar_widget = NULL;
    GtkWidget *workspace_widget = NULL;

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

    /*
     * La boîte principale organise verticalement :
     *
     * 1. la zone centrale ;
     * 2. la barre d'état.
     */
    main_window->main_box = gtk_box_new(
        GTK_ORIENTATION_VERTICAL,
        0
    );

    /*
     * GtkPaned sépare horizontalement le panneau latéral
     * et la zone de travail.
     */
    main_window->main_paned = gtk_paned_new(
        GTK_ORIENTATION_HORIZONTAL
    );

    gtk_widget_set_hexpand(
        main_window->main_paned,
        TRUE
    );

    gtk_widget_set_vexpand(
        main_window->main_paned,
        TRUE
    );

    /*
     * Création du panneau latéral via son propre module.
     *
     * MainWindow ne connaît pas son contenu interne.
     */
    main_window->sidebar = sidebar_new();

    if (main_window->sidebar == NULL)
    {
        main_window_free(main_window);
        return NULL;
    }

    sidebar_widget = sidebar_get_widget(
        main_window->sidebar
    );

    if (sidebar_widget == NULL)
    {
        main_window_free(main_window);
        return NULL;
    }

    main_window->workspace = workspace_new();

    if (main_window->workspace == NULL)
    {
        main_window_free(main_window);
        return NULL;
    }

    workspace_widget = workspace_get_widget(
        main_window->workspace
    );

    if (workspace_widget == NULL)
    {
        main_window_free(main_window);
        return NULL;
    }
    
    /*
     * Placement des deux composants dans GtkPaned.
     */
    gtk_paned_set_start_child(
        GTK_PANED(main_window->main_paned),
        sidebar_widget
    );

    gtk_paned_set_end_child(
        GTK_PANED(main_window->main_paned),
        workspace_widget
    );

    /*
     * Position initiale de la poignée de séparation.
     */
    gtk_paned_set_position(
        GTK_PANED(main_window->main_paned),
        MAIN_WINDOW_SIDEBAR_POSITION
    );

    /*
     * La sidebar conserve sa largeur lorsque la fenêtre est agrandie.
     * La zone de travail récupère l'espace supplémentaire.
     */
    gtk_paned_set_resize_start_child(
        GTK_PANED(main_window->main_paned),
        FALSE
    );

    gtk_paned_set_resize_end_child(
        GTK_PANED(main_window->main_paned),
        TRUE
    );

    /*
     * Les deux panneaux peuvent être réduits manuellement.
     */
    gtk_paned_set_shrink_start_child(
        GTK_PANED(main_window->main_paned),
        TRUE
    );

    gtk_paned_set_shrink_end_child(
        GTK_PANED(main_window->main_paned),
        TRUE
    );

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

    /*
     * Assemblage vertical :
     *
     * GtkPaned
     * Barre d'état
     */
    gtk_box_append(
        GTK_BOX(main_window->main_box),
        main_window->main_paned
    );

    gtk_box_append(
        GTK_BOX(main_window->main_box),
        main_window->status_label
    );

    gtk_window_set_child(
        main_window->window,
        main_window->main_box
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

void main_window_set_tree_model(
    MainWindow *main_window,
    const InvestigationTreeModel *tree_model
)
{
    if (main_window == NULL)
    {
        return;
    }

    sidebar_set_tree_model(
        main_window->sidebar,
        tree_model
    );
}

void main_window_free(MainWindow *main_window)
{
    if (main_window == NULL)
    {
        return;
    }

    /*
     * La structure Sidebar a été allouée par sidebar_new().
     * MainWindow en est donc propriétaire et doit la libérer.
     *
     * Les widgets GTK, eux, restent gérés par GTK.
     */
    workspace_free(main_window->workspace);
    sidebar_free(main_window->sidebar);

    g_free(main_window);
}
