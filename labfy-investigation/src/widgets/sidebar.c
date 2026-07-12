/******************************************************************************
 * @file sidebar.c
 * @brief Implémentation du panneau de navigation latéral.
 ******************************************************************************/

#include "widgets/sidebar.h"

#include <glib.h>

/**
 * @brief Largeur initiale du panneau latéral.
 */
#define SIDEBAR_DEFAULT_WIDTH 250

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
};

Sidebar *sidebar_new(void)
{
    Sidebar *sidebar = NULL;

    sidebar = g_new0(Sidebar, 1);

    sidebar->root_widget = gtk_box_new(
        GTK_ORIENTATION_VERTICAL,
        0
    );

    if (sidebar->root_widget == NULL)
    {
        sidebar_free(sidebar);
        return NULL;
    }

    gtk_widget_set_size_request(
        sidebar->root_widget,
        SIDEBAR_DEFAULT_WIDTH,
        -1
    );

    sidebar->title_label = gtk_label_new(
        "Dossier d'enquête"
    );

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
        12
    );

    gtk_box_append(
        GTK_BOX(sidebar->root_widget),
        sidebar->title_label
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

void sidebar_free(Sidebar *sidebar)
{
    if (sidebar == NULL)
    {
        return;
    }

    /*
     * Les widgets GTK sont intégrés dans l'arbre de widgets de la fenêtre.
     * GTK gère leur destruction lorsque la fenêtre est détruite.
     * Nous libérons uniquement la structure Sidebar.
     */
    g_free(sidebar);
}
