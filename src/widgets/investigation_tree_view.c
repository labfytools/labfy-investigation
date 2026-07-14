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

/*
 * InvestigationTreeItem
 * ---------------------
 *
 * GListStore exige des objets dérivés de GObject.
 *
 * Cette classe privée adapte donc un InvestigationNode à l'écosystème GTK.
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

    item->node = node;

    return item;
}

/**
 * @brief Retourne le nœud métier observé par l'adaptateur.
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
 * @return Une liste GTK contenant les enfants, ou NULL pour un fichier
 *         ou un dossier sans enfant.
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

    children_count = investigation_node_get_children_count(node);

    if (children_count == 0)
    {
        return NULL;
    }

    children_store = g_list_store_new(
        investigation_tree_item_get_type()
    );

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

        child_item = investigation_tree_item_new(child_node);

        if (child_item == NULL)
        {
            continue;
        }

        /*
         * GListStore prend sa propre référence sur l'objet.
         * Nous pouvons donc libérer notre référence locale juste après.
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
 * Une ligne contient :
 *
 * GtkTreeExpander
 * └── GtkLabel
 */
static void investigation_tree_view_factory_setup(
    GtkSignalListItemFactory *factory,
    GtkListItem *list_item,
    gpointer user_data
)
{
    GtkWidget *expander = NULL;
    GtkWidget *label = NULL;

    (void)factory;
    (void)user_data;

    expander = gtk_tree_expander_new();
    label = gtk_label_new(NULL);

    gtk_widget_set_halign(
        label,
        GTK_ALIGN_START
    );

    gtk_widget_set_hexpand(
        label,
        TRUE
    );

    gtk_tree_expander_set_child(
        GTK_TREE_EXPANDER(expander),
        label
    );

    gtk_list_item_set_child(
        list_item,
        expander
    );

    /*
     * Le focus doit appartenir au GtkTreeExpander afin que ses raccourcis
     * clavier d'ouverture et de fermeture fonctionnent correctement.
     */
    gtk_list_item_set_focusable(
        list_item,
        FALSE
    );
}

/**
 * @brief Relie une ligne GTK à un nœud métier.
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
    GtkWidget *expander = NULL;
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

    tree_item = gtk_tree_list_row_get_item(tree_row);

    if (tree_item == NULL)
    {
        return;
    }

    node = investigation_tree_item_get_node(tree_item);

    if (node == NULL)
    {
        return;
    }

    expander = gtk_list_item_get_child(list_item);

    if (expander == NULL)
    {
        return;
    }

    label = gtk_tree_expander_get_child(
        GTK_TREE_EXPANDER(expander)
    );

    if (label == NULL)
    {
        return;
    }

    node_name = investigation_node_get_name(node);

    gtk_tree_expander_set_list_row(
        GTK_TREE_EXPANDER(expander),
        tree_row
    );

    gtk_label_set_text(
        GTK_LABEL(label),
        node_name != NULL ? node_name : ""
    );
}

/**
 * @brief Détache les données d'une ligne réutilisée par GTK.
 */
static void investigation_tree_view_factory_unbind(
    GtkSignalListItemFactory *factory,
    GtkListItem *list_item,
    gpointer user_data
)
{
    GtkWidget *expander = NULL;
    GtkWidget *label = NULL;

    (void)factory;
    (void)user_data;

    expander = gtk_list_item_get_child(list_item);

    if (expander == NULL)
    {
        return;
    }

    label = gtk_tree_expander_get_child(
        GTK_TREE_EXPANDER(expander)
    );

    gtk_tree_expander_set_list_row(
        GTK_TREE_EXPANDER(expander),
        NULL
    );

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
 * GtkSingleSelection expose une GtkTreeListRow. Cette fonction récupère
 * l'adaptateur privé contenu dans cette ligne, puis le nœud métier associé.
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

    selected_item = gtk_single_selection_get_selected_item(
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

    tree_row = GTK_TREE_LIST_ROW(selected_item);

    tree_item = gtk_tree_list_row_get_item(tree_row);

    if (tree_item == NULL)
    {
        investigation_tree_view_emit_selection(
            tree_view,
            NULL
        );

        return;
    }

    node = investigation_tree_item_get_node(tree_item);

    investigation_tree_view_emit_selection(
        tree_view,
        node
    );
}

/**
 * @brief Retire le modèle GTK actuellement affiché.
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
        G_CALLBACK(investigation_tree_view_factory_setup),
        NULL
    );

    g_signal_connect(
        tree_view->factory,
        "bind",
        G_CALLBACK(investigation_tree_view_factory_bind),
        NULL
    );

    g_signal_connect(
        tree_view->factory,
        "unbind",
        G_CALLBACK(investigation_tree_view_factory_unbind),
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

    tree_view->root_widget = gtk_scrolled_window_new();

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

    investigation_tree_view_clear_model(tree_view);

    if (tree_model == NULL)
    {
        return;
    }

    root_node = investigation_tree_model_get_root(tree_model);

    if (root_node == NULL)
    {
        return;
    }

    root_store = g_list_store_new(
        investigation_tree_item_get_type()
    );

    root_item = investigation_tree_item_new(root_node);

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

    /*
     * gtk_tree_list_model_new() prend possession de root_store.
     *
     * passthrough = FALSE :
     * le modèle expose des GtkTreeListRow, nécessaires à GtkTreeExpander.
     *
     * autoexpand = FALSE :
     * les dossiers commencent repliés.
     */
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

    /*
     * GtkNoSelection satisfait l'interface GtkSelectionModel demandée
     * par GtkListView, sans encore autoriser la sélection.
     *
     * Le constructeur prend possession de gtk_tree_model.
     */
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
        G_CALLBACK(investigation_tree_view_on_selection_changed),
        tree_view
    );

    tree_view->selection_model = selection_model;
    tree_view->tree_model = tree_model;

    gtk_list_view_set_model(
        GTK_LIST_VIEW(tree_view->list_view),
        GTK_SELECTION_MODEL(tree_view->selection_model)
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

    tree_view->selection_callback = callback;
    tree_view->selection_user_data = user_data;
}

void investigation_tree_view_free(
    InvestigationTreeView *tree_view
)
{
    if (tree_view == NULL)
    {
        return;
    }

    investigation_tree_view_clear_model(tree_view);

    g_clear_object(
        &tree_view->factory
    );

    /*
     * root_widget et list_view appartiennent à l'arbre de widgets GTK
     * une fois le composant intégré à une fenêtre.
     */
    g_free(tree_view);
}
