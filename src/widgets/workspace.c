/******************************************************************************
 * @file workspace.c
 * @brief Implémentation de la zone principale de travail.
 ******************************************************************************/

#include "widgets/workspace.h"

#include <glib.h>

#define WORKSPACE_PAGE_WELCOME "welcome"
#define WORKSPACE_PAGE_NODE_INFORMATION "node-information"

/**
 * @struct Workspace
 * @brief Représentation interne de la zone de travail.
 */
struct Workspace
{
    GtkWidget *root_widget;
    GtkWidget *stack;

    GtkWidget *welcome_page;
    GtkWidget *welcome_title_label;
    GtkWidget *welcome_status_label;
    GtkWidget *welcome_instruction_label;

    GtkWidget *node_page;
    GtkWidget *node_name_label;
    GtkWidget *node_path_label;
    GtkWidget *node_type_label;
    GtkWidget *node_parent_label;
    GtkWidget *node_children_label;
};

/**
 * @brief Affiche la page d'accueil.
 */
static void workspace_show_welcome(
    Workspace *workspace
)
{
    if (workspace == NULL)
    {
        return;
    }

    gtk_stack_set_visible_child_name(
        GTK_STACK(workspace->stack),
        WORKSPACE_PAGE_WELCOME
    );
}

Workspace *workspace_new(void)
{
    Workspace *workspace = NULL;

    workspace = g_new0(Workspace, 1);

    workspace->root_widget = gtk_box_new(
        GTK_ORIENTATION_VERTICAL,
        0
    );

    if (workspace->root_widget == NULL)
    {
        workspace_free(workspace);
        return NULL;
    }

    gtk_widget_set_hexpand(
        workspace->root_widget,
        TRUE
    );

    gtk_widget_set_vexpand(
        workspace->root_widget,
        TRUE
    );

    workspace->stack = gtk_stack_new();

    if (workspace->stack == NULL)
    {
        workspace_free(workspace);
        return NULL;
    }

    gtk_widget_set_hexpand(
        workspace->stack,
        TRUE
    );

    gtk_widget_set_vexpand(
        workspace->stack,
        TRUE
    );

    gtk_box_append(
        GTK_BOX(workspace->root_widget),
        workspace->stack
    );

    /*
     * Page d'accueil.
     */
    workspace->welcome_page = gtk_box_new(
        GTK_ORIENTATION_VERTICAL,
        12
    );

    if (workspace->welcome_page == NULL)
    {
        workspace_free(workspace);
        return NULL;
    }

    gtk_widget_set_halign(
        workspace->welcome_page,
        GTK_ALIGN_CENTER
    );

    gtk_widget_set_valign(
        workspace->welcome_page,
        GTK_ALIGN_CENTER
    );

    workspace->welcome_title_label = gtk_label_new(
        "Labfy Investigation"
    );

    workspace->welcome_status_label = gtk_label_new(
        "Aucune enquête ouverte"
    );

    workspace->welcome_instruction_label = gtk_label_new(
        "Sélectionnez ou créez une enquête"
    );

    gtk_box_append(
        GTK_BOX(workspace->welcome_page),
        workspace->welcome_title_label
    );

    gtk_box_append(
        GTK_BOX(workspace->welcome_page),
        workspace->welcome_status_label
    );

    gtk_box_append(
        GTK_BOX(workspace->welcome_page),
        workspace->welcome_instruction_label
    );

    gtk_stack_add_named(
        GTK_STACK(workspace->stack),
        workspace->welcome_page,
        WORKSPACE_PAGE_WELCOME
    );

    /*
     * Page d'informations sur le nœud.
     */
    workspace->node_page = gtk_box_new(
        GTK_ORIENTATION_VERTICAL,
        12
    );

    if (workspace->node_page == NULL)
    {
        workspace_free(workspace);
        return NULL;
    }

    gtk_widget_set_halign(
        workspace->node_page,
        GTK_ALIGN_CENTER
    );

    gtk_widget_set_valign(
        workspace->node_page,
        GTK_ALIGN_CENTER
    );

    workspace->node_name_label = gtk_label_new(NULL);
    workspace->node_path_label = gtk_label_new(NULL);
    workspace->node_type_label = gtk_label_new(NULL);
    workspace->node_parent_label = gtk_label_new(NULL);
    workspace->node_children_label = gtk_label_new(NULL);

    gtk_box_append(
        GTK_BOX(workspace->node_page),
        workspace->node_name_label
    );

    gtk_box_append(
        GTK_BOX(workspace->node_page),
        workspace->node_path_label
    );

    gtk_label_set_wrap(
        GTK_LABEL(workspace->node_path_label),
        TRUE
    );

    gtk_label_set_selectable(
        GTK_LABEL(workspace->node_path_label),
        TRUE
    );

    gtk_widget_set_halign(
        workspace->node_path_label,
        GTK_ALIGN_CENTER
    );

    gtk_box_append(
        GTK_BOX(workspace->node_page),
        workspace->node_type_label
    );

    gtk_box_append(
        GTK_BOX(workspace->node_page),
        workspace->node_parent_label
    );

    gtk_box_append(
        GTK_BOX(workspace->node_page),
        workspace->node_children_label
    );

    gtk_stack_add_named(
        GTK_STACK(workspace->stack),
        workspace->node_page,
        WORKSPACE_PAGE_NODE_INFORMATION
    );

    workspace_show_welcome(workspace);

    return workspace;
}

GtkWidget *workspace_get_widget(
    const Workspace *workspace
)
{
    if (workspace == NULL)
    {
        return NULL;
    }

    return workspace->root_widget;
}

void workspace_set_selected_node(
    Workspace *workspace,
    const InvestigationNode *node
)
{
    const char *node_name = NULL;
    const InvestigationNode *parent_node = NULL;
    const char *parent_name = NULL;
    const char *node_path = NULL;
    InvestigationNodeType node_type;
    size_t children_count = 0;

    const char *type_text = NULL;
    char *parent_text = NULL;
    char *children_text = NULL;

    if (workspace == NULL)
    {
        return;
    }

    if (node == NULL)
    {
        workspace_show_welcome(workspace);
        return;
    }

    node_name = investigation_node_get_name(node);
    node_path = investigation_node_get_path(node);
    node_type = investigation_node_get_type(node);
    parent_node = investigation_node_get_parent(node);

    gtk_label_set_text(
        GTK_LABEL(workspace->node_name_label),
        node_name != NULL ? node_name : "(sans nom)"
    );

    gtk_label_set_text(
        GTK_LABEL(workspace->node_path_label),
        node_path != NULL ? node_path : "(chemin indisponible)"
    );

    if (node_type == INVESTIGATION_NODE_DIRECTORY)
    {
        type_text = "Type : dossier";
    }
    else
    {
        type_text = "Type : fichier";
    }

    gtk_label_set_text(
        GTK_LABEL(workspace->node_type_label),
        type_text
    );

    if (parent_node != NULL)
    {
        parent_name = investigation_node_get_name(parent_node);

        parent_text = g_strdup_printf(
            "Parent : %s",
            parent_name != NULL ? parent_name : "(sans nom)"
        );
    }
    else
    {
        parent_text = g_strdup("Parent : aucun");
    }

    gtk_label_set_text(
        GTK_LABEL(workspace->node_parent_label),
        parent_text
    );

    if (node_type == INVESTIGATION_NODE_DIRECTORY)
    {
        children_count =
            investigation_node_get_children_count(node);

        children_text = g_strdup_printf(
            "Enfants : %zu",
            children_count
        );

        gtk_label_set_text(
            GTK_LABEL(workspace->node_children_label),
            children_text
        );

        gtk_widget_set_visible(
            workspace->node_children_label,
            TRUE
        );
    }
    else
    {
        gtk_label_set_text(
            GTK_LABEL(workspace->node_children_label),
            ""
        );

        gtk_widget_set_visible(
            workspace->node_children_label,
            FALSE
        );
    }

    gtk_stack_set_visible_child_name(
        GTK_STACK(workspace->stack),
        WORKSPACE_PAGE_NODE_INFORMATION
    );

    g_free(children_text);
    g_free(parent_text);
}

void workspace_free(Workspace *workspace)
{
    if (workspace == NULL)
    {
        return;
    }

    g_free(workspace);
}
