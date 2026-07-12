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
};

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

    gtk_box_append(
        GTK_BOX(workspace->root_widget),
        workspace->welcome_box
    );

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
