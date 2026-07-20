/******************************************************************************
 * @file evidence_tree_view.c
 * @brief Affichage GTK hiérarchique des catégories et preuves importées.
 ******************************************************************************/

#include "widgets/evidence_tree_view.h"

#include "widgets/evidence_category_item.h"
#include "widgets/evidence_list_item.h"

#include <gio/gio.h>
#include <glib-object.h>
#include <gtk/gtk.h>

/**
 * @struct EvidenceTreeView
 * @brief État privé de la vue hiérarchique.
 */
struct EvidenceTreeView
{
    GtkWidget *root_widget;
    GtkWidget *list_view;

    GtkListItemFactory *factory;
    GtkSingleSelection *selection_model;

    EvidenceCategoryModel *category_model;

    EvidenceTreeViewSelectionCallback selection_callback;
    gpointer selection_user_data;
};

/**
 * @brief Retourne le modèle enfant d'une catégorie.
 *
 * Les preuves sont des feuilles et retournent donc NULL.
 *
 * @param item Catégorie ou preuve examinée.
 * @param user_data Données privées inutilisées.
 *
 * @return Nouveau GListModel possédé par GtkTreeListModel, ou NULL.
 */
static GListModel *evidence_tree_view_create_children_model(
    gpointer item,
    gpointer user_data
)
{
    EvidenceCategoryItem *evidence_category_item =
        NULL;

    GListModel *children =
        NULL;

    (void) user_data;

    if (item == NULL ||
        !EVIDENCE_IS_CATEGORY_ITEM(item))
    {
        return NULL;
    }

    evidence_category_item =
        EVIDENCE_CATEGORY_ITEM(
            item
        );

    children =
        evidence_category_item_get_children(
            evidence_category_item
        );

    if (children == NULL ||
        g_list_model_get_n_items(children) == 0)
    {
        return NULL;
    }

    /*
     * GtkTreeListModel prend possession de la référence retournée.
     */
    return g_object_ref(
        children
    );
}

/**
 * @brief Retourne une icône adaptée au type métier d'une preuve.
 */
static const char *evidence_tree_view_get_evidence_icon_name(
    const EvidenceListItem *evidence_list_item
)
{
    const char *type_identifier =
        NULL;

    if (evidence_list_item == NULL)
    {
        return "text-x-generic-symbolic";
    }

    type_identifier =
        evidence_list_item_get_type_identifier(
            evidence_list_item
        );

    if (g_strcmp0(
            type_identifier,
            "image"
        ) == 0)
    {
        return "image-x-generic-symbolic";
    }

    if (g_strcmp0(
            type_identifier,
            "video"
        ) == 0)
    {
        return "video-x-generic-symbolic";
    }

    if (g_strcmp0(
            type_identifier,
            "audio"
        ) == 0)
    {
        return "audio-x-generic-symbolic";
    }

    if (g_strcmp0(
            type_identifier,
            "email"
        ) == 0)
    {
        return "mail-unread-symbolic";
    }

    return "text-x-generic-symbolic";
}

/**
 * @brief Crée les widgets constituant une ligne.
 *
 * Structure :
 *
 * GtkTreeExpander
 * └── GtkBox
 *     ├── GtkImage
 *     ├── GtkLabel principal
 *     └── GtkLabel secondaire
 */
static void evidence_tree_view_factory_setup(
    GtkSignalListItemFactory *factory,
    GtkListItem *list_item,
    gpointer user_data
)
{
    GtkWidget *expander =
        NULL;

    GtkWidget *row_box =
        NULL;

    GtkWidget *image =
        NULL;

    GtkWidget *primary_label =
        NULL;

    GtkWidget *secondary_label =
        NULL;

    (void) factory;
    (void) user_data;

    expander =
        gtk_tree_expander_new();

    row_box =
        gtk_box_new(
            GTK_ORIENTATION_HORIZONTAL,
            6
        );

    image =
        gtk_image_new();

    primary_label =
        gtk_label_new(
            NULL
        );

    secondary_label =
        gtk_label_new(
            NULL
        );

    gtk_image_set_pixel_size(
        GTK_IMAGE(image),
        16
    );

    gtk_label_set_xalign(
        GTK_LABEL(primary_label),
        0.0F
    );

    gtk_label_set_ellipsize(
        GTK_LABEL(primary_label),
        PANGO_ELLIPSIZE_END
    );

    gtk_widget_set_hexpand(
        primary_label,
        TRUE
    );

    gtk_widget_set_halign(
        primary_label,
        GTK_ALIGN_FILL
    );

    gtk_label_set_xalign(
        GTK_LABEL(secondary_label),
        1.0F
    );

    gtk_widget_set_halign(
        secondary_label,
        GTK_ALIGN_END
    );

    gtk_widget_add_css_class(
        secondary_label,
        "dim-label"
    );

    gtk_widget_set_margin_end(
        row_box,
        8
    );

    gtk_box_append(
        GTK_BOX(row_box),
        image
    );

    gtk_box_append(
        GTK_BOX(row_box),
        primary_label
    );

    gtk_box_append(
        GTK_BOX(row_box),
        secondary_label
    );

    gtk_tree_expander_set_child(
        GTK_TREE_EXPANDER(expander),
        row_box
    );

    gtk_list_item_set_child(
        list_item,
        expander
    );

    /*
     * Le focus doit rester dans GtkTreeExpander pour que les commandes
     * clavier d'ouverture et de fermeture restent accessibles.
     */
    gtk_list_item_set_focusable(
        list_item,
        FALSE
    );
}

/**
 * @brief Relie une ligne GTK à une catégorie ou une preuve.
 */
static void evidence_tree_view_factory_bind(
    GtkSignalListItemFactory *factory,
    GtkListItem *list_item,
    gpointer user_data
)
{
    GtkTreeListRow *tree_row =
        NULL;

    GObject *item =
        NULL;

    GtkWidget *expander =
        NULL;

    GtkWidget *row_box =
        NULL;

    GtkWidget *image =
        NULL;

    GtkWidget *primary_label =
        NULL;

    GtkWidget *secondary_label =
        NULL;

    char *count_text =
        NULL;

    (void) factory;
    (void) user_data;

    tree_row =
        GTK_TREE_LIST_ROW(
            gtk_list_item_get_item(
                list_item
            )
        );

    if (tree_row == NULL)
    {
        return;
    }

    item =
        gtk_tree_list_row_get_item(
            tree_row
        );

    if (item == NULL)
    {
        return;
    }

    expander =
        gtk_list_item_get_child(
            list_item
        );

    if (expander == NULL)
    {
        return;
    }

    row_box =
        gtk_tree_expander_get_child(
            GTK_TREE_EXPANDER(
                expander
            )
        );

    if (row_box == NULL)
    {
        return;
    }

    image =
        gtk_widget_get_first_child(
            row_box
        );

    if (image == NULL)
    {
        return;
    }

    primary_label =
        gtk_widget_get_next_sibling(
            image
        );

    if (primary_label == NULL)
    {
        return;
    }

    secondary_label =
        gtk_widget_get_next_sibling(
            primary_label
        );

    if (secondary_label == NULL)
    {
        return;
    }

    gtk_tree_expander_set_list_row(
        GTK_TREE_EXPANDER(
            expander
        ),
        tree_row
    );

    gtk_widget_remove_css_class(
        primary_label,
        "heading"
    );

    gtk_widget_set_tooltip_text(
        row_box,
        NULL
    );

    if (EVIDENCE_IS_CATEGORY_ITEM(item))
    {
        EvidenceCategoryItem *evidence_category_item =
            EVIDENCE_CATEGORY_ITEM(
                item
            );

        gtk_image_set_from_icon_name(
            GTK_IMAGE(image),
            "folder-symbolic"
        );

        gtk_label_set_text(
            GTK_LABEL(primary_label),
            evidence_category_item_get_label(
                evidence_category_item
            )
        );

        gtk_widget_add_css_class(
            primary_label,
            "heading"
        );

        count_text =
            g_strdup_printf(
                "%u",
                evidence_category_item_get_count(
                    evidence_category_item
                )
            );

        gtk_label_set_text(
            GTK_LABEL(secondary_label),
            count_text != NULL
                ? count_text
                : ""
        );

        g_free(
            count_text
        );

        return;
    }

    if (EVIDENCE_IS_LIST_ITEM(item))
    {
        EvidenceListItem *evidence_list_item =
            EVIDENCE_LIST_ITEM(
                item
            );

        const char *original_name =
            evidence_list_item_get_original_name(
                evidence_list_item
            );

        gtk_image_set_from_icon_name(
            GTK_IMAGE(image),
            evidence_tree_view_get_evidence_icon_name(
                evidence_list_item
            )
        );

        gtk_label_set_text(
            GTK_LABEL(primary_label),
            original_name != NULL
                ? original_name
                : ""
        );

        gtk_label_set_text(
            GTK_LABEL(secondary_label),
            evidence_list_item_get_integrity_status_label(
                evidence_list_item
            )
        );

        gtk_widget_set_tooltip_text(
            row_box,
            original_name
        );

        return;
    }

    gtk_image_clear(
        GTK_IMAGE(image)
    );

    gtk_label_set_text(
        GTK_LABEL(primary_label),
        ""
    );

    gtk_label_set_text(
        GTK_LABEL(secondary_label),
        ""
    );
}

/**
 * @brief Détache le contenu d'une ligne réutilisée par GTK.
 */
static void evidence_tree_view_factory_unbind(
    GtkSignalListItemFactory *factory,
    GtkListItem *list_item,
    gpointer user_data
)
{
    GtkWidget *expander =
        NULL;

    GtkWidget *row_box =
        NULL;

    GtkWidget *image =
        NULL;

    GtkWidget *primary_label =
        NULL;

    GtkWidget *secondary_label =
        NULL;

    (void) factory;
    (void) user_data;

    expander =
        gtk_list_item_get_child(
            list_item
        );

    if (expander == NULL)
    {
        return;
    }

    row_box =
        gtk_tree_expander_get_child(
            GTK_TREE_EXPANDER(
                expander
            )
        );

    if (row_box != NULL)
    {
        image =
            gtk_widget_get_first_child(
                row_box
            );

        if (image != NULL)
        {
            primary_label =
                gtk_widget_get_next_sibling(
                    image
                );
        }

        if (primary_label != NULL)
        {
            secondary_label =
                gtk_widget_get_next_sibling(
                    primary_label
                );
        }
    }

    gtk_tree_expander_set_list_row(
        GTK_TREE_EXPANDER(
            expander
        ),
        NULL
    );

    if (image != NULL)
    {
        gtk_image_clear(
            GTK_IMAGE(image)
        );
    }

    if (primary_label != NULL)
    {
        gtk_widget_remove_css_class(
            primary_label,
            "heading"
        );

        gtk_label_set_text(
            GTK_LABEL(primary_label),
            ""
        );
    }

    if (secondary_label != NULL)
    {
        gtk_label_set_text(
            GTK_LABEL(secondary_label),
            ""
        );
    }

    if (row_box != NULL)
    {
        gtk_widget_set_tooltip_text(
            row_box,
            NULL
        );
    }
}

/**
 * @brief Transmet une sélection au callback public.
 */
static void evidence_tree_view_emit_selection(
    EvidenceTreeView *evidence_tree_view,
    const char *evidence_identifier
)
{
    if (evidence_tree_view == NULL ||
        evidence_tree_view->selection_callback == NULL)
    {
        return;
    }

    evidence_tree_view->selection_callback(
        evidence_identifier,
        evidence_tree_view->selection_user_data
    );
}

/**
 * @brief Réagit au changement de ligne sélectionnée.
 */
static void evidence_tree_view_on_selection_changed(
    GObject *selection_model,
    GParamSpec *property_spec,
    gpointer user_data
)
{
    EvidenceTreeView *evidence_tree_view =
        user_data;

    GObject *selected_item =
        NULL;

    GtkTreeListRow *tree_row =
        NULL;

    GObject *item =
        NULL;

    const char *evidence_identifier =
        NULL;

    (void) property_spec;

    if (evidence_tree_view == NULL)
    {
        return;
    }

    selected_item =
        gtk_single_selection_get_selected_item(
            GTK_SINGLE_SELECTION(
                selection_model
            )
        );

    if (selected_item == NULL ||
        !GTK_IS_TREE_LIST_ROW(selected_item))
    {
        evidence_tree_view_emit_selection(
            evidence_tree_view,
            NULL
        );

        return;
    }

    tree_row =
        GTK_TREE_LIST_ROW(
            selected_item
        );

    item =
        gtk_tree_list_row_get_item(
            tree_row
        );

    if (item != NULL &&
        EVIDENCE_IS_LIST_ITEM(item))
    {
        evidence_identifier =
            evidence_list_item_get_identifier(
                EVIDENCE_LIST_ITEM(
                    item
                )
            );
    }

    /*
     * Une catégorie sélectionnée transmet NULL.
     */
    evidence_tree_view_emit_selection(
        evidence_tree_view,
        evidence_identifier
    );
}

/**
 * @brief Retire le modèle actuellement affiché.
 */
static void evidence_tree_view_clear_model(
    EvidenceTreeView *evidence_tree_view
)
{
    if (evidence_tree_view == NULL)
    {
        return;
    }

    if (evidence_tree_view->list_view != NULL)
    {
        gtk_list_view_set_model(
            GTK_LIST_VIEW(
                evidence_tree_view->list_view
            ),
            NULL
        );
    }

    g_clear_object(
        &evidence_tree_view->selection_model
    );

    evidence_tree_view->category_model =
        NULL;

    evidence_tree_view_emit_selection(
        evidence_tree_view,
        NULL
    );
}

EvidenceTreeView *evidence_tree_view_new(void)
{
    EvidenceTreeView *evidence_tree_view =
        NULL;

    evidence_tree_view =
        g_try_new0(
            EvidenceTreeView,
            1
        );

    if (evidence_tree_view == NULL)
    {
        return NULL;
    }

    evidence_tree_view->factory =
        GTK_LIST_ITEM_FACTORY(
            gtk_signal_list_item_factory_new()
        );

    if (evidence_tree_view->factory == NULL)
    {
        evidence_tree_view_free(
            evidence_tree_view
        );

        return NULL;
    }

    g_signal_connect(
        evidence_tree_view->factory,
        "setup",
        G_CALLBACK(
            evidence_tree_view_factory_setup
        ),
        NULL
    );

    g_signal_connect(
        evidence_tree_view->factory,
        "bind",
        G_CALLBACK(
            evidence_tree_view_factory_bind
        ),
        NULL
    );

    g_signal_connect(
        evidence_tree_view->factory,
        "unbind",
        G_CALLBACK(
            evidence_tree_view_factory_unbind
        ),
        NULL
    );

    evidence_tree_view->list_view =
        gtk_list_view_new(
            NULL,
            g_object_ref(
                evidence_tree_view->factory
            )
        );

    if (evidence_tree_view->list_view == NULL)
    {
        evidence_tree_view_free(
            evidence_tree_view
        );

        return NULL;
    }

    evidence_tree_view->root_widget =
        gtk_scrolled_window_new();

    if (evidence_tree_view->root_widget == NULL)
    {
        evidence_tree_view_free(
            evidence_tree_view
        );

        return NULL;
    }

    gtk_widget_set_hexpand(
        evidence_tree_view->root_widget,
        TRUE
    );

    gtk_widget_set_vexpand(
        evidence_tree_view->root_widget,
        TRUE
    );

    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(
            evidence_tree_view->root_widget
        ),
        GTK_POLICY_AUTOMATIC,
        GTK_POLICY_AUTOMATIC
    );

    gtk_scrolled_window_set_child(
        GTK_SCROLLED_WINDOW(
            evidence_tree_view->root_widget
        ),
        evidence_tree_view->list_view
    );

    return evidence_tree_view;
}

GtkWidget *evidence_tree_view_get_widget(
    const EvidenceTreeView *evidence_tree_view
)
{
    if (evidence_tree_view == NULL)
    {
        return NULL;
    }

    return evidence_tree_view->root_widget;
}

void evidence_tree_view_set_model(
    EvidenceTreeView *evidence_tree_view,
    EvidenceCategoryModel *evidence_category_model
)
{
    GListModel *category_list_model =
        NULL;

    GtkTreeListModel *tree_list_model =
        NULL;

    GtkSingleSelection *selection_model =
        NULL;

    if (evidence_tree_view == NULL)
    {
        return;
    }

    evidence_tree_view_clear_model(
        evidence_tree_view
    );

    if (evidence_category_model == NULL)
    {
        return;
    }

    category_list_model =
        evidence_category_model_get_list_model(
            evidence_category_model
        );

    if (category_list_model == NULL)
    {
        return;
    }

    tree_list_model =
        gtk_tree_list_model_new(
            g_object_ref(
                category_list_model
            ),
            FALSE,
            FALSE,
            evidence_tree_view_create_children_model,
            NULL,
            NULL
        );

    if (tree_list_model == NULL)
    {
        return;
    }

    selection_model =
        gtk_single_selection_new(
            G_LIST_MODEL(
                tree_list_model
            )
        );

    if (selection_model == NULL)
    {
        g_object_unref(
            tree_list_model
        );

        return;
    }

    gtk_single_selection_set_autoselect(
        selection_model,
        FALSE
    );

    gtk_single_selection_set_can_unselect(
        selection_model,
        TRUE
    );

    g_signal_connect(
        selection_model,
        "notify::selected-item",
        G_CALLBACK(
            evidence_tree_view_on_selection_changed
        ),
        evidence_tree_view
    );

    evidence_tree_view->selection_model =
        selection_model;

    evidence_tree_view->category_model =
        evidence_category_model;

    gtk_list_view_set_model(
        GTK_LIST_VIEW(
            evidence_tree_view->list_view
        ),
        GTK_SELECTION_MODEL(
            evidence_tree_view->selection_model
        )
    );
}

void evidence_tree_view_set_selection_callback(
    EvidenceTreeView *evidence_tree_view,
    EvidenceTreeViewSelectionCallback callback,
    gpointer user_data
)
{
    if (evidence_tree_view == NULL)
    {
        return;
    }

    evidence_tree_view->selection_callback =
        callback;

    evidence_tree_view->selection_user_data =
        user_data;
}

void evidence_tree_view_free(
    EvidenceTreeView *evidence_tree_view
)
{
    if (evidence_tree_view == NULL)
    {
        return;
    }

    evidence_tree_view_clear_model(
        evidence_tree_view
    );

    g_clear_object(
        &evidence_tree_view->factory
    );

    /*
     * root_widget et list_view sont gérés par l'arbre GTK
     * une fois intégrés dans la fenêtre.
     */
    g_free(
        evidence_tree_view
    );
}
