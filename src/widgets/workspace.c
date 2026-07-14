/******************************************************************************
 * @file workspace.c
 * @brief Implémentation de la zone principale de travail.
 ******************************************************************************/

#include "widgets/workspace.h"

#include <glib.h>

/**
 * @struct Workspace
 * @brief Représentation interne de la zone de travail.
 *
 * Cette structure reste privée au module. Les autres fichiers utilisent
 * uniquement le type opaque déclaré dans workspace.h.
 */
struct Workspace
{
    GtkWidget *root_widget;
    GtkWidget *welcome_box;
    GtkWidget *title_label;
    GtkWidget *status_label;
    GtkWidget *instruction_label;

    GtkWidget *node_box;
    GtkWidget *node_name_label;
    GtkWidget *node_type_label;
    GtkWidget *node_parent_label;
    GtkWidget *node_children_label;
};

static void workspace_show_welcome(
    Workspace *workspace
)
{
    if (workspace == NULL)
    {
        return;
    }

    gtk_widget_set_visible(
        workspace->welcome_box,
        TRUE
    );

    gtk_widget_set_visible(
        workspace->node_box,
        FALSE
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

    workspace->welcome_box = gtk_box_new(
        GTK_ORIENTATION_VERTICAL,
        12
    );

    if (workspace->welcome_box == NULL)
    {
        workspace_free(workspace);
        return NULL;
    }

    /*
     * Le conteneur d'accueil est centré horizontalement et verticalement
     * dans l'espace disponible.
     */
    gtk_widget_set_halign(
        workspace->welcome_box,
        GTK_ALIGN_CENTER
    );

    gtk_widget_set_valign(
        workspace->welcome_box,
        GTK_ALIGN_CENTER
    );

    workspace->title_label = gtk_label_new(
        "Labfy Investigation"
    );

    workspace->status_label = gtk_label_new(
        "Aucune enquête ouverte"
    );

    workspace->instruction_label = gtk_label_new(
        "Sélectionnez ou créez une enquête"
    );

    gtk_box_append(
        GTK_BOX(workspace->welcome_box),
        workspace->title_label
    );

    gtk_box_append(
        GTK_BOX(workspace->welcome_box),
        workspace->status_label
    );

    gtk_box_append(
        GTK_BOX(workspace->welcome_box),
        workspace->instruction_label
    );

    workspace->node_box = gtk_box_new(
    GTK_ORIENTATION_VERTICAL,
    12
    );

    if (workspace->node_box == NULL)
    {
        workspace_free(workspace);
        return NULL;
    }

    gtk_widget_set_halign(
        workspace->node_box,
        GTK_ALIGN_CENTER
    );

    gtk_widget_set_valign(
        workspace->node_box,
        GTK_ALIGN_CENTER
    );

    workspace->node_name_label = gtk_label_new(NULL);
    workspace->node_type_label = gtk_label_new(NULL);
    workspace->node_parent_label = gtk_label_new(NULL);
    workspace->node_children_label = gtk_label_new(NULL);

    gtk_box_append(
        GTK_BOX(workspace->node_box),
        workspace->node_name_label
    );

    gtk_box_append(
        GTK_BOX(workspace->node_box),
        workspace->node_type_label
    );

    gtk_box_append(
        GTK_BOX(workspace->node_box),
        workspace->node_parent_label
    );

    gtk_box_append(
        GTK_BOX(workspace->node_box),
        workspace->node_children_label
    );

    gtk_box_append(
        GTK_BOX(workspace->root_widget),
        workspace->welcome_box
    );

    gtk_box_append(
        GTK_BOX(workspace->root_widget),
        workspace->node_box
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
    InvestigationNodeType node_type;
    size_t children_count = 0;

    char *type_text = NULL;
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
    node_type = investigation_node_get_type(node);
    parent_node = investigation_node_get_parent(node);

    gtk_label_set_text(
        GTK_LABEL(workspace->node_name_label),
        node_name != NULL ? node_name : "(sans nom)"
    );

    if (node_type == INVESTIGATION_NODE_DIRECTORY)
    {
        type_text = g_strdup("Type : dossier");
    }
    else
    {
        type_text = g_strdup("Type : fichier");
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

    gtk_widget_set_visible(
        workspace->welcome_box,
        FALSE
    );

    gtk_widget_set_visible(
        workspace->node_box,
        TRUE
    );

    g_free(children_text);
    g_free(parent_text);
    g_free(type_text);
}

void workspace_free(Workspace *workspace)
{
    if (workspace == NULL)
    {
        return;
    }

    /*
     * Les widgets GTK sont intégrés dans l'arbre de widgets de MainWindow.
     * GTK gère donc leur destruction.
     */
    g_free(workspace);
}
