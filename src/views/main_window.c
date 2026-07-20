/******************************************************************************
 * @file main_window.c
 * @brief Implémentation de la fenêtre principale.
 ******************************************************************************/

#include "views/main_window.h"
#include "widgets/sidebar.h"
#include "widgets/workspace.h"
#include "widgets/investigation_tree_view.h"
#include "widgets/task_panel.h"

#include <glib.h>

/**
 * @brief Largeur initiale de la fenêtre principale.
 */
#define MAIN_WINDOW_DEFAULT_WIDTH 1100

/**
 * @brief Hauteur initiale de la fenêtre principale.
 */
#define MAIN_WINDOW_DEFAULT_HEIGHT 650

/**
 * @brief Position initiale de la séparation horizontale.
 */
#define MAIN_WINDOW_SIDEBAR_POSITION 320

/**
 * @brief Position initiale de la séparation verticale.
 */
#define MAIN_WINDOW_TASK_PANEL_POSITION 430

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
    GtkWidget *import_evidence_button;
    GtkWidget *content_paned;
    GtkWidget *main_paned;
    GtkWidget *status_label;
    GtkWidget *quit_button;

    Sidebar *sidebar;
    Workspace *workspace;
    TaskPanel *task_panel;

    MainWindowNewInvestigationCallback
        new_investigation_callback;

    gpointer
        new_investigation_user_data;

    MainWindowOpenInvestigationCallback
        open_investigation_callback;

    gpointer
        open_investigation_user_data;

    MainWindowImportEvidenceCallback
        import_evidence_callback;

    gpointer
        import_evidence_user_data;

    MainWindowQuitCallback
        quit_callback;

    gpointer
        quit_user_data;

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

/**
 * @brief Transmet la demande d'import d'une preuve au contrôleur.
 *
 * @param button Bouton ayant reçu le clic.
 * @param user_data Pointeur vers MainWindow.
 */
static void main_window_on_import_evidence_clicked(
    GtkButton *button,
    gpointer user_data
)
{
    MainWindow *main_window = user_data;

    (void) button;

    if (main_window == NULL ||
        main_window->import_evidence_callback == NULL)
    {
        return;
    }

    main_window->import_evidence_callback(
        main_window->import_evidence_user_data
    );
}

/**
 * @brief Transmet la demande de fermeture au contrôleur.
 *
 * @param button Bouton ayant reçu le clic.
 * @param user_data Pointeur vers MainWindow.
 */
static void main_window_on_quit_clicked(
    GtkButton *button,
    gpointer user_data
)
{
    MainWindow *main_window = user_data;

    (void) button;

    if (main_window == NULL ||
        main_window->quit_callback == NULL)
    {
        return;
    }

    main_window->quit_callback(
        main_window->quit_user_data
    );
}

/**
 * @brief Crée un bouton d'action compact avec une icône.
 *
 * @param icon_name Nom de l'icône symbolique GTK.
 * @param tooltip_text Texte de l'infobulle.
 *
 * @return Nouveau bouton GTK.
 */
static GtkWidget *main_window_create_action_button(
    const char *icon_name,
    const char *tooltip_text
)
{
    GtkWidget *button = NULL;

    if (icon_name == NULL ||
        icon_name[0] == '\0')
    {
        return NULL;
    }

    button =
        gtk_button_new_from_icon_name(
            icon_name
        );

    if (button == NULL)
    {
        return NULL;
    }

    gtk_widget_add_css_class(
        button,
        "flat"
    );

    if (tooltip_text != NULL &&
        tooltip_text[0] != '\0')
    {
        gtk_widget_set_tooltip_text(
            button,
            tooltip_text
        );
    }

    return button;
}

MainWindow *main_window_new(
    GtkApplication *application,
    TaskManager *task_manager
)
{
    MainWindow *main_window = NULL;
    GtkWidget *sidebar_widget = NULL;
    GtkWidget *workspace_widget = NULL;
    GtkWidget *task_panel_widget = NULL;
    GtkWidget *action_spacer = NULL;

    if (application == NULL ||
        task_manager == NULL)
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
        4
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
        main_window_create_action_button(
            "document-new-symbolic",
            "Créer une nouvelle enquête"
        );

    main_window->open_investigation_button =
        main_window_create_action_button(
            "document-open-symbolic",
            "Ouvrir une enquête"
        );

    main_window->import_evidence_button =
        main_window_create_action_button(
            "document-save-symbolic",
            "Importer une preuve"
        );

    main_window->quit_button =
        main_window_create_action_button(
            "application-exit-symbolic",
            "Quitter Labfy Investigation"
        );

    action_spacer =
        gtk_box_new(
            GTK_ORIENTATION_HORIZONTAL,
            0
        );

    gtk_widget_set_hexpand(
        action_spacer,
        TRUE
    );

    /*
     * Aucun import n'est autorisé sans enquête ouverte.
     */
    gtk_widget_set_sensitive(
        main_window->import_evidence_button,
        FALSE
    );

    gtk_box_append(
        GTK_BOX(main_window->action_bar),
        main_window->new_investigation_button
    );

    gtk_box_append(
        GTK_BOX(main_window->action_bar),
        main_window->open_investigation_button
    );

    gtk_box_append(
        GTK_BOX(main_window->action_bar),
        main_window->import_evidence_button
    );

    gtk_box_append(
        GTK_BOX(main_window->action_bar),
        action_spacer
    );

    gtk_box_append(
        GTK_BOX(main_window->action_bar),
        main_window->quit_button
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

    g_signal_connect(
        main_window->import_evidence_button,
        "clicked",
        G_CALLBACK(
            main_window_on_import_evidence_clicked
        ),
        main_window
    );

    g_signal_connect(
        main_window->quit_button,
        "clicked",
        G_CALLBACK(
            main_window_on_quit_clicked
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
    
    main_window->task_panel = task_panel_new(
        task_manager
    );

    if (main_window->task_panel == NULL)
    {
        main_window_free(
            main_window
        );

        return NULL;
    }

    task_panel_widget = task_panel_get_widget(
        main_window->task_panel
    );

    if (task_panel_widget == NULL)
    {
        main_window_free(
            main_window
        );

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

    main_window->content_paned = gtk_paned_new(
        GTK_ORIENTATION_VERTICAL
    );

    gtk_widget_set_hexpand(
        main_window->content_paned,
        TRUE
    );

    gtk_widget_set_vexpand(
        main_window->content_paned,
        TRUE
    );

    gtk_paned_set_start_child(
        GTK_PANED(main_window->content_paned),
        main_window->main_paned
    );

    gtk_paned_set_end_child(
        GTK_PANED(main_window->content_paned),
        task_panel_widget
    );

    gtk_paned_set_position(
        GTK_PANED(main_window->content_paned),
        MAIN_WINDOW_TASK_PANEL_POSITION
    );

    gtk_paned_set_resize_start_child(
        GTK_PANED(main_window->content_paned),
        TRUE
    );

    gtk_paned_set_resize_end_child(
        GTK_PANED(main_window->content_paned),
        FALSE
    );

    gtk_paned_set_shrink_start_child(
        GTK_PANED(main_window->content_paned),
        TRUE
    );

    gtk_paned_set_shrink_end_child(
        GTK_PANED(main_window->content_paned),
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
        main_window->content_paned
    );

    gtk_box_append(
        GTK_BOX(main_window->main_box),
        main_window->status_label
    );

    gtk_window_set_child(
        main_window->window,
        main_window->main_box
    );

    /*
     * Ce volet vertical sépare la zone principale du panneau d'activité.
     */
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

void main_window_set_evidence_model(
    MainWindow *main_window,
    EvidenceCategoryModel *evidence_category_model
)
{
    if (main_window == NULL)
    {
        return;
    }

    sidebar_set_evidence_model(
        main_window->sidebar,
        evidence_category_model
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

void main_window_set_evidence_selection_callback(
    MainWindow *main_window,
    MainWindowEvidenceSelectionCallback callback,
    gpointer user_data
)
{
    if (main_window == NULL)
    {
        return;
    }

    sidebar_set_evidence_selection_callback(
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

void main_window_set_import_evidence_callback(
    MainWindow *main_window,
    MainWindowImportEvidenceCallback callback,
    gpointer user_data
)
{
    if (main_window == NULL)
    {
        return;
    }

    main_window->import_evidence_callback =
        callback;

    main_window->import_evidence_user_data =
        user_data;
}

void main_window_set_import_evidence_enabled(
    MainWindow *main_window,
    gboolean enabled
)
{
    if (main_window == NULL ||
        main_window->import_evidence_button == NULL)
    {
        return;
    }

    gtk_widget_set_sensitive(
        main_window->import_evidence_button,
        enabled
    );
}

void main_window_set_quit_callback(
    MainWindow *main_window,
    MainWindowQuitCallback callback,
    gpointer user_data
)
{
    if (main_window == NULL)
    {
        return;
    }

    main_window->quit_callback = callback;
    main_window->quit_user_data = user_data;
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

void main_window_set_selected_evidence(
    MainWindow *main_window,
    const EvidenceRecord *evidence_record
)
{
    if (main_window == NULL)
    {
        return;
    }

    workspace_set_selected_evidence(
        main_window->workspace,
        evidence_record
    );
}

void main_window_free(
    MainWindow *main_window
)
{
    if (main_window == NULL)
    {
        return;
    }

    /*
     * TaskPanel possède un timer et un callback enregistré dans
     * TaskManager. Ils doivent être retirés avant la destruction
     * des widgets GTK.
     */
    task_panel_free(
        main_window->task_panel
    );

    main_window->task_panel = NULL;

    /*
     * Empêche la destruction du modèle de transmettre une dernière
     * sélection à Application.
     */
    if (main_window->sidebar != NULL)
    {
        sidebar_set_selection_callback(
            main_window->sidebar,
            NULL,
            NULL
        );

        /*
         * Le modèle est détaché pendant que le GtkListView existe
         * encore.
         */
        sidebar_set_tree_model(
            main_window->sidebar,
            NULL
        );

                sidebar_set_evidence_selection_callback(
            main_window->sidebar,
            NULL,
            NULL
        );

        sidebar_set_evidence_model(
            main_window->sidebar,
            NULL
        );
    }

    /*
     * Les structures qui manipulent encore leurs widgets doivent être
     * nettoyées avant gtk_window_destroy().
     */
    sidebar_free(
        main_window->sidebar
    );

    workspace_free(
        main_window->workspace
    );

    main_window->sidebar = NULL;
    main_window->workspace = NULL;

    /*
     * Les modules ne manipulent plus leurs widgets.
     * GTK peut maintenant détruire tout l'arbre sans callback vers
     * des structures déjà libérées.
     */
    if (main_window->window != NULL)
    {
        gtk_window_destroy(
            main_window->window
        );

        main_window->window = NULL;
    }

    g_free(
        main_window
    );
}
