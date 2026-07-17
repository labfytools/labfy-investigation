/******************************************************************************
 * @file main_window.c
 * @brief Implémentation de la fenêtre principale.
 ******************************************************************************/

#include "views/main_window.h"
#include "widgets/sidebar.h"
#include "widgets/workspace.h"
#include "widgets/investigation_tree_view.h"

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
 * @brief Titre par défaut de la fenêtre.
 */
#define MAIN_WINDOW_DEFAULT_TITLE \
    "Labfy Investigation"

/**
 * @brief Texte affiché lorsqu'aucune enquête n'est ouverte.
 */
#define MAIN_WINDOW_NO_INVESTIGATION_STATUS \
    "Aucune enquête ouverte"

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
    GtkWidget *action_bar;
    GtkWidget *new_investigation_button;
    GtkWidget *open_investigation_button;
    GtkWidget *main_paned;
    GtkWidget *status_label;

    Sidebar *sidebar;
    Workspace *workspace;

    MainWindowNewInvestigationCallback
        new_investigation_callback;

    gpointer
        new_investigation_user_data;

    MainWindowOpenInvestigationCallback
        open_investigation_callback;

    gpointer
        open_investigation_user_data;
};

/**
 * @brief Transmet la demande de création d'une enquête au contrôleur.
 *
 * @param button Bouton ayant reçu le clic.
 * @param user_data Pointeur vers MainWindow.
 */
static void main_window_on_new_investigation_clicked(
    GtkButton *button,
    gpointer user_data
)
{
    MainWindow *main_window = user_data;

    (void) button;

    if (main_window == NULL ||
        main_window->new_investigation_callback == NULL)
    {
        return;
    }

    main_window->new_investigation_callback(
        main_window->new_investigation_user_data
    );
}

/**
 * @brief Transmet la demande d'ouverture d'une enquête au contrôleur.
 *
 * @param button Bouton ayant reçu le clic.
 * @param user_data Pointeur vers MainWindow.
 */
static void main_window_on_open_investigation_clicked(
    GtkButton *button,
    gpointer user_data
)
{
    MainWindow *main_window = user_data;

    (void) button;

    if (main_window == NULL ||
        main_window->open_investigation_callback == NULL)
    {
        return;
    }

    main_window->open_investigation_callback(
        main_window->open_investigation_user_data
    );
}

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
        MAIN_WINDOW_DEFAULT_TITLE
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
     * Barre regroupant les actions générales de l'application.
     */
    main_window->action_bar = gtk_box_new(
        GTK_ORIENTATION_HORIZONTAL,
        8
    );

    gtk_widget_set_margin_start(
        main_window->action_bar,
        8
    );

    gtk_widget_set_margin_end(
        main_window->action_bar,
        8
    );

    gtk_widget_set_margin_top(
        main_window->action_bar,
        8
    );

    gtk_widget_set_margin_bottom(
        main_window->action_bar,
        8
    );

    main_window->new_investigation_button =
        gtk_button_new_with_label(
            "Nouvelle enquête"
        );

    main_window->open_investigation_button =
        gtk_button_new_with_label(
            "Ouvrir une enquête"
        );

    gtk_box_append(
        GTK_BOX(main_window->action_bar),
        main_window->new_investigation_button
    );

    gtk_box_append(
        GTK_BOX(main_window->action_bar),
        main_window->open_investigation_button
    );

    g_signal_connect(
        main_window->new_investigation_button,
        "clicked",
        G_CALLBACK(
            main_window_on_new_investigation_clicked
        ),
        main_window
    );

    g_signal_connect(
        main_window->open_investigation_button,
        "clicked",
        G_CALLBACK(
            main_window_on_open_investigation_clicked
        ),
        main_window
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
        MAIN_WINDOW_NO_INVESTIGATION_STATUS
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
     * Barre d'actions
     * GtkPaned
     * Barre d'état
     */
    gtk_box_append(
        GTK_BOX(main_window->main_box),
        main_window->action_bar
    );

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

void main_window_set_investigation(
    MainWindow *main_window,
    const char *investigation_name,
    const char *investigation_root_path
)
{
    char *window_title = NULL;
    char *status_text = NULL;

    gboolean has_name = FALSE;
    gboolean has_root_path = FALSE;

    if (main_window == NULL)
    {
        return;
    }

    has_name =
        investigation_name != NULL &&
        investigation_name[0] != '\0';

    has_root_path =
        investigation_root_path != NULL &&
        investigation_root_path[0] != '\0';

    if (has_name)
    {
        window_title = g_strdup_printf(
            "%s — %s",
            MAIN_WINDOW_DEFAULT_TITLE,
            investigation_name
        );
    }
    else
    {
        window_title = g_strdup(
            MAIN_WINDOW_DEFAULT_TITLE
        );
    }

    if (has_name && has_root_path)
    {
        status_text = g_strdup_printf(
            "Enquête ouverte : %s — %s",
            investigation_name,
            investigation_root_path
        );
    }
    else if (has_name)
    {
        status_text = g_strdup_printf(
            "Enquête ouverte : %s",
            investigation_name
        );
    }
    else if (has_root_path)
    {
        status_text = g_strdup_printf(
            "Enquête ouverte : %s",
            investigation_root_path
        );
    }
    else
    {
        status_text = g_strdup(
            MAIN_WINDOW_NO_INVESTIGATION_STATUS
        );
    }

    if (main_window->window != NULL)
    {
        gtk_window_set_title(
            main_window->window,
            window_title
        );
    }

    if (main_window->status_label != NULL)
    {
        gtk_label_set_text(
            GTK_LABEL(main_window->status_label),
            status_text
        );
    }

    g_free(status_text);
    g_free(window_title);
}

void main_window_set_status(
    MainWindow *main_window,
    const char *status_text
)
{
    const char *safe_status_text = NULL;

    if (main_window == NULL ||
        main_window->status_label == NULL)
    {
        return;
    }

    safe_status_text =
        status_text != NULL &&
        status_text[0] != '\0'
            ? status_text
            : MAIN_WINDOW_NO_INVESTIGATION_STATUS;

    gtk_label_set_text(
        GTK_LABEL(main_window->status_label),
        safe_status_text
    );
}

void main_window_set_tree_selection_callback(
    MainWindow *main_window,
    InvestigationTreeViewSelectionCallback callback,
    gpointer user_data
)
{
    if (main_window == NULL)
    {
        return;
    }

    sidebar_set_selection_callback(
        main_window->sidebar,
        callback,
        user_data
    );
}

void main_window_set_new_investigation_callback(
    MainWindow *main_window,
    MainWindowNewInvestigationCallback callback,
    gpointer user_data
)
{
    if (main_window == NULL)
    {
        return;
    }

    main_window->new_investigation_callback = callback;
    main_window->new_investigation_user_data = user_data;
}

void main_window_set_open_investigation_callback(
    MainWindow *main_window,
    MainWindowOpenInvestigationCallback callback,
    gpointer user_data
)
{
    if (main_window == NULL)
    {
        return;
    }

    main_window->open_investigation_callback = callback;
    main_window->open_investigation_user_data = user_data;
}

void main_window_set_selected_node(
    MainWindow *main_window,
    const InvestigationNode *node
)
{
    if (main_window == NULL)
    {
        return;
    }

    workspace_set_selected_node(
        main_window->workspace,
        node
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
