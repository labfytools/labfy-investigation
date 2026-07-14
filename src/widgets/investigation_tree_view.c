/******************************************************************************
 * @file investigation_tree_view.c
 * @brief Affichage GTK4 de l'arborescence d'une enquête.
 ******************************************************************************/

#include "widgets/investigation_tree_view.h"

#include "core/investigation_node.h"

#include <gio/gio.h>
#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>

/*
 * InvestigationTreeItem
 * ---------------------
 *
 * GListStore exige des objets dérivés de GObject.
 *
 * Cette classe privée adapte un InvestigationNode à l'écosystème GTK.
 * Elle ne possède jamais le nœud métier : elle conserve seulement son adresse.
 */

typedef struct _InvestigationTreeItem
{
    GObject parent_instance;

    const InvestigationNode *node;
} InvestigationTreeItem;

typedef struct _InvestigationTreeItemClass
{
    GObjectClass parent_class;
} InvestigationTreeItemClass;

G_DEFINE_TYPE(
    InvestigationTreeItem,
    investigation_tree_item,
    G_TYPE_OBJECT
)

static void investigation_tree_item_class_init(
    InvestigationTreeItemClass *item_class
)
{
    (void)item_class;
}

static void investigation_tree_item_init(
    InvestigationTreeItem *item
)
{
    item->node = NULL;
}

/**
 * @brief Crée un adaptateur GTK pour un nœud métier.
 *
 * L'adaptateur ne devient pas propriétaire du nœud.
 *
 * @param node Nœud métier observé.
 *
 * @return Un nouvel adaptateur, ou NULL.
 */
static InvestigationTreeItem *investigation_tree_item_new(
    const InvestigationNode *node
)
{
    InvestigationTreeItem *item = NULL;

    if (node == NULL)
    {
        return NULL;
    }

    item = g_object_new(
        investigation_tree_item_get_type(),
        NULL
    );

    if (item == NULL)
    {
        return NULL;
    }

    item->node = node;

    return item;
}

/**
 * @brief Retourne le nœud métier observé par l'adaptateur.
 *
 * @param item Adaptateur à consulter.
 *
 * @return Le nœud observé, ou NULL.
 */
static const InvestigationNode *investigation_tree_item_get_node(
    const InvestigationTreeItem *item
)
{
    if (item == NULL)
    {
        return NULL;
    }

    return item->node;
}

/**
 * @struct InvestigationTreeView
 * @brief Représentation interne de la vue d'arborescence.
 */
struct InvestigationTreeView
{
    GtkWidget *root_widget;
    GtkWidget *list_view;

    GtkListItemFactory *factory;
    GtkSingleSelection *selection_model;

    const InvestigationTreeModel *tree_model;

    InvestigationTreeViewSelectionCallback selection_callback;
    gpointer selection_user_data;
};

/**
 * @brief Crée une liste GTK contenant les enfants d'un nœud.
 *
 * Cette fonction est appelée par GtkTreeListModel lorsqu'un dossier doit
 * être développé.
 *
 * @param item      Adaptateur représentant le nœud concerné.
 * @param user_data Données privées inutilisées.
 *
 * @return Une liste GTK contenant les enfants, ou NULL si le nœud ne possède
 *         aucun enfant.
 */
static GListModel *investigation_tree_view_create_children_model(
    gpointer item,
    gpointer user_data
)
{
    InvestigationTreeItem *tree_item = item;
    const InvestigationNode *node = NULL;
    GListStore *children_store = NULL;
    size_t children_count = 0;

    (void)user_data;

    if (tree_item == NULL)
    {
        return NULL;
    }

    node = investigation_tree_item_get_node(tree_item);

    if (node == NULL)
    {
        return NULL;
    }

    if (investigation_node_get_type(node) !=
        INVESTIGATION_NODE_DIRECTORY)
    {
        return NULL;
    }

    children_count =
        investigation_node_get_children_count(node);

    if (children_count == 0)
    {
        return NULL;
    }

    children_store = g_list_store_new(
        investigation_tree_item_get_type()
    );

    if (children_store == NULL)
    {
        return NULL;
    }

    for (size_t index = 0; index < children_count; ++index)
    {
        const InvestigationNode *child_node = NULL;
        InvestigationTreeItem *child_item = NULL;

        child_node = investigation_node_get_child(
            node,
            index
        );

        if (child_node == NULL)
        {
            continue;
        }

        child_item = investigation_tree_item_new(
            child_node
        );

        if (child_item == NULL)
        {
            continue;
        }

        /*
         * GListStore prend sa propre référence sur l'objet.
         */
        g_list_store_append(
            children_store,
            child_item
        );

        g_object_unref(child_item);
    }

    return G_LIST_MODEL(children_store);
}

/**
 * @brief Crée les widgets constituant une ligne de l'arborescence.
 *
 * Structure créée :
 *
 * GtkTreeExpander
 * └── GtkBox horizontal
 *     ├── GtkImage
 *     └── GtkLabel
 *
 * @param factory   Fabrique GTK.
 * @param list_item Élément de liste à préparer.
 * @param user_data Données privées inutilisées.
 */
static void investigation_tree_view_factory_setup(
    GtkSignalListItemFactory *factory,
    GtkListItem *list_item,
    gpointer user_data
)
{
    GtkWidget *expander = NULL;
    GtkWidget *row_box = NULL;
    GtkWidget *image = NULL;
    GtkWidget *label = NULL;

    (void)factory;
    (void)user_data;

    expander = gtk_tree_expander_new();

    row_box = gtk_box_new(
        GTK_ORIENTATION_HORIZONTAL,
        6
    );

    image = gtk_image_new();
    label = gtk_label_new(NULL);

    gtk_label_set_xalign(
        GTK_LABEL(label),
        0.0f
    );

    gtk_widget_set_halign(
        label,
        GTK_ALIGN_START
    );

    gtk_widget_set_hexpand(
        label,
        TRUE
    );

    gtk_box_append(
        GTK_BOX(row_box),
        image
    );

    gtk_box_append(
        GTK_BOX(row_box),
        label
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
     * Le focus reste dans GtkTreeExpander afin que les raccourcis
     * d'ouverture et de fermeture fonctionnent.
     */
    gtk_list_item_set_focusable(
        list_item,
        FALSE
    );
}

static const char *investigation_tree_view_get_icon_name(
    const InvestigationNode *node
)
{
    const char *name = NULL;
    const char *extension = NULL;

    if (node == NULL)
    {
        return "text-x-generic-symbolic";
    }

    if (investigation_node_get_type(node) ==
        INVESTIGATION_NODE_DIRECTORY)
    {
        return "folder-symbolic";
    }

    name = investigation_node_get_name(node);

    if (name == NULL)
    {
        return "text-x-generic-symbolic";
    }

    extension = strrchr(name, '.');

    if (extension == NULL)
    {
        return "text-x-generic-symbolic";
    }

    if (g_ascii_strcasecmp(extension, ".sqlite") == 0 ||
        g_ascii_strcasecmp(extension, ".db") == 0)
    {
        return "folder-database-symbolic";
    }

    if (g_ascii_strcasecmp(extension, ".jpg") == 0 ||
        g_ascii_strcasecmp(extension, ".jpeg") == 0 ||
        g_ascii_strcasecmp(extension, ".png") == 0 ||
        g_ascii_strcasecmp(extension, ".webp") == 0)
    {
        return "image-x-generic-symbolic";
    }

    if (g_ascii_strcasecmp(extension, ".pdf") == 0)
    {
        return "application-pdf-symbolic";
    }

    if (g_ascii_strcasecmp(extension, ".csv") == 0)
    {
        return "x-office-spreadsheet-symbolic";
    }

    if (g_ascii_strcasecmp(extension, ".zip") == 0 ||
        g_ascii_strcasecmp(extension, ".tar") == 0 ||
        g_ascii_strcasecmp(extension, ".gz") == 0 ||
        g_ascii_strcasecmp(extension, ".xz") == 0)
    {
        return "package-x-generic-symbolic";
    }

    if (g_ascii_strcasecmp(extension, ".mp4") == 0 ||
        g_ascii_strcasecmp(extension, ".mkv") == 0 ||
        g_ascii_strcasecmp(extension, ".avi") == 0 ||
        g_ascii_strcasecmp(extension, ".mov") == 0)
    {
        return "video-x-generic-symbolic";
    }

    return "text-x-generic-symbolic";
}

/**
 * @brief Relie une ligne GTK à un nœud métier.
 *
 * @param factory   Fabrique GTK.
 * @param list_item Élément de liste à remplir.
 * @param user_data Données privées inutilisées.
 */
static void investigation_tree_view_factory_bind(
    GtkSignalListItemFactory *factory,
    GtkListItem *list_item,
    gpointer user_data
)
{
    GtkTreeListRow *tree_row = NULL;
    InvestigationTreeItem *tree_item = NULL;
    const InvestigationNode *node = NULL;
    const char *icon_name = NULL;

    GtkWidget *expander = NULL;
    GtkWidget *row_box = NULL;
    GtkWidget *image = NULL;
    GtkWidget *label = NULL;

    const char *node_name = NULL;

    (void)factory;
    (void)user_data;

    tree_row = GTK_TREE_LIST_ROW(
        gtk_list_item_get_item(list_item)
    );

    if (tree_row == NULL)
    {
        return;
    }

    tree_item = gtk_tree_list_row_get_item(
        tree_row
    );

    if (tree_item == NULL)
    {
        return;
    }

    node = investigation_tree_item_get_node(
        tree_item
    );

    if (node == NULL)
    {
        return;
    }

    expander = gtk_list_item_get_child(
        list_item
    );

    if (expander == NULL)
    {
        return;
    }

    row_box = gtk_tree_expander_get_child(
        GTK_TREE_EXPANDER(expander)
    );

    if (row_box == NULL)
    {
        return;
    }

    image = gtk_widget_get_first_child(
        row_box
    );

    if (image == NULL)
    {
        return;
    }

    label = gtk_widget_get_next_sibling(
        image
    );

    if (label == NULL)
    {
        return;
    }

    node_name = investigation_node_get_name(
        node
    );

    icon_name = investigation_tree_view_get_icon_name(
        node
    );

    gtk_tree_expander_set_list_row(
        GTK_TREE_EXPANDER(expander),
        tree_row
    );

    /*
     * Pour l'instant l'image reste vide.
     * Elle sera configurée dans la suite du ticket #019.
     */
    gtk_image_set_from_icon_name(
        GTK_IMAGE(image),
        icon_name
    );
    
    gtk_image_set_pixel_size(
        GTK_IMAGE(image),
        16
    );
    
    gtk_label_set_text(
        GTK_LABEL(label),
        node_name != NULL ? node_name : ""
    );
}

/**
 * @brief Détache les données d'une ligne réutilisée par GTK.
 *
 * @param factory   Fabrique GTK.
 * @param list_item Élément de liste à nettoyer.
 * @param user_data Données privées inutilisées.
 */
static void investigation_tree_view_factory_unbind(
    GtkSignalListItemFactory *factory,
    GtkListItem *list_item,
    gpointer user_data
)
{
    GtkWidget *expander = NULL;
    GtkWidget *row_box = NULL;
    GtkWidget *image = NULL;
    GtkWidget *label = NULL;

    (void)factory;
    (void)user_data;

    expander = gtk_list_item_get_child(
        list_item
    );

    if (expander == NULL)
    {
        return;
    }

    row_box = gtk_tree_expander_get_child(
        GTK_TREE_EXPANDER(expander)
    );

    if (row_box != NULL)
    {
        image = gtk_widget_get_first_child(
            row_box
        );

        if (image != NULL)
        {
            label = gtk_widget_get_next_sibling(
                image
            );
        }
    }

    gtk_tree_expander_set_list_row(
        GTK_TREE_EXPANDER(expander),
        NULL
    );

    if (image != NULL)
    {
        gtk_image_clear(
            GTK_IMAGE(image)
        );
    }

    if (label != NULL)
    {
        gtk_label_set_text(
            GTK_LABEL(label),
            ""
        );
    }
}

/**
 * @brief Transmet le nœud sélectionné au code appelant.
 *
 * @param tree_view Vue contenant le callback.
 * @param node      Nœud sélectionné, ou NULL.
 */
static void investigation_tree_view_emit_selection(
    InvestigationTreeView *tree_view,
    const InvestigationNode *node
)
{
    if (tree_view == NULL)
    {
        return;
    }

    if (tree_view->selection_callback == NULL)
    {
        return;
    }

    tree_view->selection_callback(
        node,
        tree_view->selection_user_data
    );
}

/**
 * @brief Réagit au changement de l'élément sélectionné.
 *
 * @param selection_model Modèle GTK ayant changé de sélection.
 * @param property_spec   Propriété ayant déclenché la notification.
 * @param user_data       Pointeur vers InvestigationTreeView.
 */
static void investigation_tree_view_on_selection_changed(
    GObject *selection_model,
    GParamSpec *property_spec,
    gpointer user_data
)
{
    InvestigationTreeView *tree_view = user_data;
    GObject *selected_item = NULL;
    GtkTreeListRow *tree_row = NULL;
    InvestigationTreeItem *tree_item = NULL;
    const InvestigationNode *node = NULL;

    (void)property_spec;

    if (tree_view == NULL)
    {
        return;
    }

    selected_item =
        gtk_single_selection_get_selected_item(
            GTK_SINGLE_SELECTION(selection_model)
        );

    if (selected_item == NULL)
    {
        investigation_tree_view_emit_selection(
            tree_view,
            NULL
        );

        return;
    }

    tree_row = GTK_TREE_LIST_ROW(
        selected_item
    );

    tree_item = gtk_tree_list_row_get_item(
        tree_row
    );

    if (tree_item == NULL)
    {
        investigation_tree_view_emit_selection(
            tree_view,
            NULL
        );

        return;
    }

    node = investigation_tree_item_get_node(
        tree_item
    );

    investigation_tree_view_emit_selection(
        tree_view,
        node
    );
}

/**
 * @brief Retire le modèle GTK actuellement affiché.
 *
 * @param tree_view Vue à nettoyer.
 */
static void investigation_tree_view_clear_model(
    InvestigationTreeView *tree_view
)
{
    if (tree_view == NULL)
    {
        return;
    }

    gtk_list_view_set_model(
        GTK_LIST_VIEW(tree_view->list_view),
        NULL
    );

    g_clear_object(
        &tree_view->selection_model
    );

    tree_view->tree_model = NULL;

    investigation_tree_view_emit_selection(
        tree_view,
        NULL
    );
}

InvestigationTreeView *investigation_tree_view_new(void)
{
    InvestigationTreeView *tree_view = NULL;

    tree_view = g_new0(
        InvestigationTreeView,
        1
    );

    if (tree_view == NULL)
    {
        return NULL;
    }

    tree_view->factory =
        GTK_LIST_ITEM_FACTORY(
            gtk_signal_list_item_factory_new()
        );

    if (tree_view->factory == NULL)
    {
        investigation_tree_view_free(tree_view);
        return NULL;
    }

    g_signal_connect(
        tree_view->factory,
        "setup",
        G_CALLBACK(
            investigation_tree_view_factory_setup
        ),
        NULL
    );

    g_signal_connect(
        tree_view->factory,
        "bind",
        G_CALLBACK(
            investigation_tree_view_factory_bind
        ),
        NULL
    );

    g_signal_connect(
        tree_view->factory,
        "unbind",
        G_CALLBACK(
            investigation_tree_view_factory_unbind
        ),
        NULL
    );

    tree_view->list_view = gtk_list_view_new(
        NULL,
        g_object_ref(tree_view->factory)
    );

    if (tree_view->list_view == NULL)
    {
        investigation_tree_view_free(tree_view);
        return NULL;
    }

    tree_view->root_widget =
        gtk_scrolled_window_new();

    if (tree_view->root_widget == NULL)
    {
        investigation_tree_view_free(tree_view);
        return NULL;
    }

    gtk_widget_set_hexpand(
        tree_view->root_widget,
        TRUE
    );

    gtk_widget_set_vexpand(
        tree_view->root_widget,
        TRUE
    );

    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(tree_view->root_widget),
        GTK_POLICY_AUTOMATIC,
        GTK_POLICY_AUTOMATIC
    );

    gtk_scrolled_window_set_child(
        GTK_SCROLLED_WINDOW(tree_view->root_widget),
        tree_view->list_view
    );

    return tree_view;
}

GtkWidget *investigation_tree_view_get_widget(
    const InvestigationTreeView *tree_view
)
{
    if (tree_view == NULL)
    {
        return NULL;
    }

    return tree_view->root_widget;
}

void investigation_tree_view_set_model(
    InvestigationTreeView *tree_view,
    const InvestigationTreeModel *tree_model
)
{
    const InvestigationNode *root_node = NULL;

    InvestigationTreeItem *root_item = NULL;
    GListStore *root_store = NULL;
    GtkTreeListModel *gtk_tree_model = NULL;
    GtkSingleSelection *selection_model = NULL;

    if (tree_view == NULL)
    {
        return;
    }

    investigation_tree_view_clear_model(
        tree_view
    );

    if (tree_model == NULL)
    {
        return;
    }

    root_node =
        investigation_tree_model_get_root(
            tree_model
        );

    if (root_node == NULL)
    {
        return;
    }

    root_store = g_list_store_new(
        investigation_tree_item_get_type()
    );

    if (root_store == NULL)
    {
        return;
    }

    root_item = investigation_tree_item_new(
        root_node
    );

    if (root_item == NULL)
    {
        g_object_unref(root_store);
        return;
    }

    g_list_store_append(
        root_store,
        root_item
    );

    g_object_unref(root_item);

    gtk_tree_model = gtk_tree_list_model_new(
        G_LIST_MODEL(root_store),
        FALSE,
        FALSE,
        investigation_tree_view_create_children_model,
        NULL,
        NULL
    );

    if (gtk_tree_model == NULL)
    {
        return;
    }

    selection_model = gtk_single_selection_new(
        G_LIST_MODEL(gtk_tree_model)
    );

    if (selection_model == NULL)
    {
        g_object_unref(gtk_tree_model);
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
            investigation_tree_view_on_selection_changed
        ),
        tree_view
    );

    tree_view->selection_model =
        selection_model;

    tree_view->tree_model =
        tree_model;

    gtk_list_view_set_model(
        GTK_LIST_VIEW(tree_view->list_view),
        GTK_SELECTION_MODEL(
            tree_view->selection_model
        )
    );
}

void investigation_tree_view_set_selection_callback(
    InvestigationTreeView *tree_view,
    InvestigationTreeViewSelectionCallback callback,
    gpointer user_data
)
{
    if (tree_view == NULL)
    {
        return;
    }

    tree_view->selection_callback =
        callback;

    tree_view->selection_user_data =
        user_data;
}

void investigation_tree_view_free(
    InvestigationTreeView *tree_view
)
{
    if (tree_view == NULL)
    {
        return;
    }

    investigation_tree_view_clear_model(
        tree_view
    );

    g_clear_object(
        &tree_view->factory
    );

    /*
     * root_widget et list_view appartiennent à l'arbre GTK
     * une fois intégrés dans la fenêtre.
     */
    g_free(tree_view);
}
