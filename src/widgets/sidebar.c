/******************************************************************************
 * @file sidebar.c
 * @brief Implémentation du panneau de navigation latéral.
 ******************************************************************************/

#include "widgets/sidebar.h"
#include "core/investigation_node.h"
#include "widgets/investigation_tree_view.h"
#include "widgets/evidence_tree_view.h"

#include <glib.h>

/**
 * @brief Largeur initiale du panneau latéral.
 */
#define SIDEBAR_DEFAULT_WIDTH 320

/**
 * @brief Identifiant de la page contenant l'arborescence.
 */
#define SIDEBAR_PAGE_FOLDER \
    "folder"

/**
 * @brief Identifiant de la page contenant les preuves.
 */
#define SIDEBAR_PAGE_EVIDENCE \
    "evidence"

/**
 * @brief État vide de l'onglet Preuves.
 */
#define SIDEBAR_EVIDENCE_STATE_EMPTY \
    "empty"

/**
 * @brief État contenant l'arbre des preuves.
 */
#define SIDEBAR_EVIDENCE_STATE_TREE \
    "tree"

/**
 * @struct Sidebar
 * @brief Représentation interne du panneau latéral.
 *
 * Cette structure reste privée au module. Les autres fichiers utilisent
 * uniquement le type opaque déclaré dans sidebar.h.
 */
struct Sidebar
{
    GtkWidget *root_widget;
    GtkWidget *title_label;

    GtkWidget *page_switcher;
    GtkWidget *stack;

    GtkWidget *folder_page;

    GtkWidget *evidence_page;
    GtkWidget *evidence_content_stack;
    GtkWidget *evidence_empty_page;
    GtkWidget *evidence_empty_label;

    InvestigationTreeView *tree_view;
    EvidenceTreeView *evidence_tree_view;

    SidebarEvidenceSelectionCallback
        evidence_selection_callback;

    gpointer evidence_selection_user_data;
};

/**
 * @brief Relaie la sélection provenant de EvidenceTreeView.
 */
static void sidebar_on_evidence_selected(
    const char *evidence_identifier,
    gpointer user_data
)
{
    Sidebar *sidebar =
        user_data;

    if (sidebar == NULL ||
        sidebar->evidence_selection_callback == NULL)
    {
        return;
    }

    sidebar->evidence_selection_callback(
        evidence_identifier,
        sidebar->evidence_selection_user_data
    );
}

Sidebar *sidebar_new(void)
{
    Sidebar *sidebar = NULL;

    GtkWidget *tree_view_widget = NULL;
    GtkWidget *evidence_tree_widget = NULL;

    sidebar =
        g_new0(
            Sidebar,
            1
        );

    sidebar->root_widget =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            0
        );

    if (sidebar->root_widget == NULL)
    {
        sidebar_free(
            sidebar
        );

        return NULL;
    }

    gtk_widget_set_size_request(
        sidebar->root_widget,
        SIDEBAR_DEFAULT_WIDTH,
        -1
    );

    /*
     * Titre de l'enquête active.
     */
    sidebar->title_label =
        gtk_label_new(
            "Dossier d'enquête"
        );

    if (sidebar->title_label == NULL)
    {
        sidebar_free(
            sidebar
        );

        return NULL;
    }

    gtk_widget_set_halign(
        sidebar->title_label,
        GTK_ALIGN_START
    );

    gtk_widget_set_margin_start(
        sidebar->title_label,
        12
    );

    gtk_widget_set_margin_end(
        sidebar->title_label,
        12
    );

    gtk_widget_set_margin_top(
        sidebar->title_label,
        12
    );

    gtk_widget_set_margin_bottom(
        sidebar->title_label,
        8
    );

    gtk_box_append(
        GTK_BOX(
            sidebar->root_widget
        ),
        sidebar->title_label
    );

    /*
     * GtkStack contenant les pages Dossier et Preuves.
     */
    sidebar->stack =
        gtk_stack_new();

    if (sidebar->stack == NULL)
    {
        sidebar_free(
            sidebar
        );

        return NULL;
    }

    gtk_widget_set_hexpand(
        sidebar->stack,
        TRUE
    );

    gtk_widget_set_vexpand(
        sidebar->stack,
        TRUE
    );

    gtk_stack_set_transition_type(
        GTK_STACK(
            sidebar->stack
        ),
        GTK_STACK_TRANSITION_TYPE_CROSSFADE
    );

    /*
     * Boutons permettant de sélectionner une page du GtkStack.
     */
    sidebar->page_switcher =
        gtk_stack_switcher_new();

    if (sidebar->page_switcher == NULL)
    {
        sidebar_free(
            sidebar
        );

        return NULL;
    }

    gtk_stack_switcher_set_stack(
        GTK_STACK_SWITCHER(
            sidebar->page_switcher
        ),
        GTK_STACK(
            sidebar->stack
        )
    );

    gtk_widget_set_hexpand(
        sidebar->page_switcher,
        TRUE
    );

    gtk_widget_set_halign(
        sidebar->page_switcher,
        GTK_ALIGN_CENTER
    );

    gtk_widget_set_margin_start(
        sidebar->page_switcher,
        8
    );

    gtk_widget_set_margin_end(
        sidebar->page_switcher,
        8
    );

    gtk_widget_set_margin_bottom(
        sidebar->page_switcher,
        8
    );

    gtk_box_append(
        GTK_BOX(
            sidebar->root_widget
        ),
        sidebar->page_switcher
    );

    /*
     * Page Dossier.
     */
    sidebar->folder_page =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            0
        );

    if (sidebar->folder_page == NULL)
    {
        sidebar_free(
            sidebar
        );

        return NULL;
    }

    gtk_widget_set_hexpand(
        sidebar->folder_page,
        TRUE
    );

    gtk_widget_set_vexpand(
        sidebar->folder_page,
        TRUE
    );

    sidebar->tree_view =
        investigation_tree_view_new();

    if (sidebar->tree_view == NULL)
    {
        sidebar_free(
            sidebar
        );

        return NULL;
    }

    tree_view_widget =
        investigation_tree_view_get_widget(
            sidebar->tree_view
        );

    if (tree_view_widget == NULL)
    {
        sidebar_free(
            sidebar
        );

        return NULL;
    }

    gtk_widget_set_hexpand(
        tree_view_widget,
        TRUE
    );

    gtk_widget_set_vexpand(
        tree_view_widget,
        TRUE
    );

    gtk_box_append(
        GTK_BOX(
            sidebar->folder_page
        ),
        tree_view_widget
    );

    gtk_stack_add_titled(
        GTK_STACK(
            sidebar->stack
        ),
        sidebar->folder_page,
        SIDEBAR_PAGE_FOLDER,
        "Dossier"
    );

    /*
     * Page Preuves.
     */
    sidebar->evidence_page =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            0
        );

    if (sidebar->evidence_page == NULL)
    {
        sidebar_free(
            sidebar
        );

        return NULL;
    }

    gtk_widget_set_hexpand(
        sidebar->evidence_page,
        TRUE
    );

    gtk_widget_set_vexpand(
        sidebar->evidence_page,
        TRUE
    );

    /*
     * Stack interne alternant entre l'état vide
     * et l'arborescence des preuves.
     */
    sidebar->evidence_content_stack =
        gtk_stack_new();

    if (sidebar->evidence_content_stack == NULL)
    {
        sidebar_free(
            sidebar
        );

        return NULL;
    }

    gtk_widget_set_hexpand(
        sidebar->evidence_content_stack,
        TRUE
    );

    gtk_widget_set_vexpand(
        sidebar->evidence_content_stack,
        TRUE
    );

    gtk_stack_set_transition_type(
        GTK_STACK(
            sidebar->evidence_content_stack
        ),
        GTK_STACK_TRANSITION_TYPE_CROSSFADE
    );

    /*
     * État vide.
     */
    sidebar->evidence_empty_page =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            0
        );

    if (sidebar->evidence_empty_page == NULL)
    {
        sidebar_free(
            sidebar
        );

        return NULL;
    }

    gtk_widget_set_hexpand(
        sidebar->evidence_empty_page,
        TRUE
    );

    gtk_widget_set_vexpand(
        sidebar->evidence_empty_page,
        TRUE
    );

    sidebar->evidence_empty_label =
        gtk_label_new(
            "Aucune preuve importée dans cette enquête."
        );

    if (sidebar->evidence_empty_label == NULL)
    {
        sidebar_free(
            sidebar
        );

        return NULL;
    }

    gtk_label_set_wrap(
        GTK_LABEL(
            sidebar->evidence_empty_label
        ),
        TRUE
    );

    gtk_label_set_justify(
        GTK_LABEL(
            sidebar->evidence_empty_label
        ),
        GTK_JUSTIFY_CENTER
    );

    gtk_widget_set_halign(
        sidebar->evidence_empty_label,
        GTK_ALIGN_CENTER
    );

    gtk_widget_set_valign(
        sidebar->evidence_empty_label,
        GTK_ALIGN_CENTER
    );

    gtk_widget_set_hexpand(
        sidebar->evidence_empty_label,
        TRUE
    );

    gtk_widget_set_vexpand(
        sidebar->evidence_empty_label,
        TRUE
    );

    gtk_widget_set_margin_start(
        sidebar->evidence_empty_label,
        20
    );

    gtk_widget_set_margin_end(
        sidebar->evidence_empty_label,
        20
    );

    gtk_box_append(
        GTK_BOX(
            sidebar->evidence_empty_page
        ),
        sidebar->evidence_empty_label
    );

    gtk_stack_add_named(
        GTK_STACK(
            sidebar->evidence_content_stack
        ),
        sidebar->evidence_empty_page,
        SIDEBAR_EVIDENCE_STATE_EMPTY
    );

    /*
     * Arborescence des catégories et preuves.
     */
    sidebar->evidence_tree_view =
        evidence_tree_view_new();

    if (sidebar->evidence_tree_view == NULL)
    {
        sidebar_free(
            sidebar
        );

        return NULL;
    }

    evidence_tree_widget =
        evidence_tree_view_get_widget(
            sidebar->evidence_tree_view
        );

    if (evidence_tree_widget == NULL)
    {
        sidebar_free(
            sidebar
        );

        return NULL;
    }

    gtk_widget_set_hexpand(
        evidence_tree_widget,
        TRUE
    );

    gtk_widget_set_vexpand(
        evidence_tree_widget,
        TRUE
    );

    evidence_tree_view_set_selection_callback(
        sidebar->evidence_tree_view,
        sidebar_on_evidence_selected,
        sidebar
    );

    gtk_stack_add_named(
        GTK_STACK(
            sidebar->evidence_content_stack
        ),
        evidence_tree_widget,
        SIDEBAR_EVIDENCE_STATE_TREE
    );

    /*
     * Aucun modèle n'est encore installé.
     */
    gtk_stack_set_visible_child_name(
        GTK_STACK(
            sidebar->evidence_content_stack
        ),
        SIDEBAR_EVIDENCE_STATE_EMPTY
    );

    gtk_box_append(
        GTK_BOX(
            sidebar->evidence_page
        ),
        sidebar->evidence_content_stack
    );

    gtk_stack_add_titled(
        GTK_STACK(
            sidebar->stack
        ),
        sidebar->evidence_page,
        SIDEBAR_PAGE_EVIDENCE,
        "Preuves"
    );

    /*
     * L'onglet Dossier reste sélectionné au démarrage.
     */
    gtk_stack_set_visible_child_name(
        GTK_STACK(
            sidebar->stack
        ),
        SIDEBAR_PAGE_FOLDER
    );

    gtk_box_append(
        GTK_BOX(
            sidebar->root_widget
        ),
        sidebar->stack
    );

    return sidebar;
}

GtkWidget *sidebar_get_widget(
    const Sidebar *sidebar
)
{
    if (sidebar == NULL)
    {
        return NULL;
    }

    return sidebar->root_widget;
}

void sidebar_set_tree_model(
    Sidebar *sidebar,
    const InvestigationTreeModel *tree_model
)
{
    const InvestigationNode *root_node = NULL;
    const char *root_name = NULL;

    if (sidebar == NULL)
    {
        return;
    }

    investigation_tree_view_set_model(
        sidebar->tree_view,
        tree_model
    );

    if (tree_model == NULL)
    {
        gtk_label_set_text(
            GTK_LABEL(sidebar->title_label),
            "Dossier d'enquête"
        );

        return;
    }

    root_node = investigation_tree_model_get_root(tree_model);

    if (root_node == NULL)
    {
        gtk_label_set_text(
            GTK_LABEL(sidebar->title_label),
            "Dossier d'enquête"
        );

        return;
    }

    root_name = investigation_node_get_name(root_node);

    if (root_name == NULL || root_name[0] == '\0')
    {
        gtk_label_set_text(
            GTK_LABEL(sidebar->title_label),
            "Dossier d'enquête"
        );

        return;
    }

    gtk_label_set_text(
        GTK_LABEL(sidebar->title_label),
        root_name
    );
}

void sidebar_set_selection_callback(
    Sidebar *sidebar,
    InvestigationTreeViewSelectionCallback callback,
    gpointer user_data
)
{
    if (sidebar == NULL)
    {
        return;
    }

    investigation_tree_view_set_selection_callback(
        sidebar->tree_view,
        callback,
        user_data
    );
}

void sidebar_set_evidence_model(
    Sidebar *sidebar,
    EvidenceCategoryModel *evidence_category_model
)
{
    if (sidebar == NULL ||
        sidebar->evidence_tree_view == NULL ||
        sidebar->evidence_content_stack == NULL)
    {
        return;
    }

    evidence_tree_view_set_model(
        sidebar->evidence_tree_view,
        evidence_category_model
    );

    if (evidence_category_model == NULL ||
        evidence_category_model_get_count(
            evidence_category_model
        ) == 0)
    {
        gtk_stack_set_visible_child_name(
            GTK_STACK(
                sidebar->evidence_content_stack
            ),
            SIDEBAR_EVIDENCE_STATE_EMPTY
        );

        return;
    }

    gtk_stack_set_visible_child_name(
        GTK_STACK(
            sidebar->evidence_content_stack
        ),
        SIDEBAR_EVIDENCE_STATE_TREE
    );
}

void sidebar_set_evidence_selection_callback(
    Sidebar *sidebar,
    SidebarEvidenceSelectionCallback callback,
    gpointer user_data
)
{
    if (sidebar == NULL)
    {
        return;
    }

    sidebar->evidence_selection_callback =
        callback;

    sidebar->evidence_selection_user_data =
        user_data;
}

void sidebar_free(Sidebar *sidebar)
{
    if (sidebar == NULL)
    {
        return;
    }

    investigation_tree_view_free(sidebar->tree_view);
    
    if (sidebar->evidence_tree_view != NULL)
    {
        evidence_tree_view_set_selection_callback(
            sidebar->evidence_tree_view,
            NULL,
            NULL
        );

        evidence_tree_view_free(
            sidebar->evidence_tree_view
        );
    }

    /*
     * Les widgets GTK sont intégrés dans l'arbre de widgets de la fenêtre.
     * GTK gère leur destruction lorsque la fenêtre est détruite.
     * Nous libérons uniquement la structure Sidebar.
     */
    g_free(sidebar);
}
