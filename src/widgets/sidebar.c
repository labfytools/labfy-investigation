/******************************************************************************
 * @file sidebar.c
 * @brief Implémentation du panneau de navigation latéral.
 ******************************************************************************/

#include "widgets/sidebar.h"
#include "core/investigation_node.h"
#include "widgets/investigation_tree_view.h"
#include "widgets/evidence_tree_view.h"

#include <glib.h>
#include <string.h>

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
 * @brief Identifiant de la page contenant les entités.
 */
#define SIDEBAR_PAGE_ENTITY \
    "entity"

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

    GtkWidget *page_switcher_scrolled_window;
    GtkWidget *page_switcher;
    GtkWidget *stack;

    GtkWidget *folder_page;

    GtkWidget *evidence_page;
    GtkWidget *evidence_content_stack;
    GtkWidget *evidence_empty_page;
    GtkWidget *evidence_empty_label;

    GtkWidget *entity_page;
    GtkWidget *entity_search_entry;
    GtkWidget *entity_count_label;
    GtkWidget *entity_scrolled_window;
    GtkWidget *entity_list_box;
    GtkWidget *entity_empty_label;
    GtkWidget *entity_no_result_label;

    InvestigationTreeView *tree_view;
    EvidenceTreeView *evidence_tree_view;

    SidebarEvidenceSelectionCallback
        evidence_selection_callback;

    gpointer evidence_selection_user_data;

    SidebarEntitySelectionCallback
        entity_selection_callback;

    gpointer entity_selection_user_data;
};

/**
 * @brief Filtre une ligne d'entité avec le texte de recherche courant.
 */
static gboolean sidebar_filter_entity_row(
    GtkListBoxRow *row,
    gpointer user_data
)
{
    Sidebar *sidebar = user_data;
    const EntityRecord *entity_record = NULL;
    const char *search_text = NULL;
    char *normalized_search = NULL;
    char *normalized_value = NULL;
    char *normalized_type = NULL;
    gboolean visible = TRUE;

    if (sidebar == NULL || row == NULL)
    {
        return FALSE;
    }

    search_text = gtk_editable_get_text(
        GTK_EDITABLE(sidebar->entity_search_entry)
    );

    if (search_text == NULL || search_text[0] == '\0')
    {
        return TRUE;
    }

    entity_record = g_object_get_data(
        G_OBJECT(row),
        "entity-record"
    );

    if (entity_record == NULL)
    {
        return FALSE;
    }

    normalized_search = g_utf8_casefold(search_text, -1);
    normalized_value = g_utf8_casefold(
        entity_record_get_value(entity_record),
        -1
    );
    normalized_type = g_utf8_casefold(
        entity_record_get_type_identifier(entity_record),
        -1
    );

    visible = normalized_search != NULL &&
        ((normalized_value != NULL &&
          strstr(normalized_value, normalized_search) != NULL) ||
         (normalized_type != NULL &&
          strstr(normalized_type, normalized_search) != NULL));

    g_free(normalized_type);
    g_free(normalized_value);
    g_free(normalized_search);

    return visible;
}

/**
 * @brief Réapplique le filtre local des entités.
 */
static void sidebar_on_entity_search_changed(
    GtkSearchEntry *search_entry,
    gpointer user_data
)
{
    Sidebar *sidebar = user_data;

    (void) search_entry;

    if (sidebar != NULL && sidebar->entity_list_box != NULL)
    {
        gtk_list_box_invalidate_filter(
            GTK_LIST_BOX(sidebar->entity_list_box)
        );
    }
}

/**
 * @brief Relaie la sélection provenant de la liste d'entités.
 */
static void sidebar_on_entity_row_selected(
    GtkListBox *list_box,
    GtkListBoxRow *row,
    gpointer user_data
)
{
    Sidebar *sidebar = user_data;
    const EntityRecord *entity_record = NULL;
    const char *entity_identifier = NULL;

    (void) list_box;

    if (sidebar == NULL ||
        sidebar->entity_selection_callback == NULL)
    {
        return;
    }

    if (row != NULL)
    {
        entity_record = g_object_get_data(
            G_OBJECT(row),
            "entity-record"
        );
        entity_identifier = entity_record_get_identifier(entity_record);
    }

    sidebar->entity_selection_callback(
        entity_identifier,
        sidebar->entity_selection_user_data
    );
}

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

    sidebar->page_switcher_scrolled_window =
        gtk_scrolled_window_new();

    if (sidebar->page_switcher == NULL ||
        sidebar->page_switcher_scrolled_window == NULL)
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
        sidebar->page_switcher_scrolled_window,
        TRUE
    );

    gtk_widget_set_halign(
        sidebar->page_switcher,
        GTK_ALIGN_START
    );

    gtk_widget_set_margin_bottom(
        sidebar->page_switcher,
        12
    );

    gtk_scrolled_window_set_min_content_height(
        GTK_SCROLLED_WINDOW(
            sidebar->page_switcher_scrolled_window
        ),
        48
    );

    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(
            sidebar->page_switcher_scrolled_window
        ),
        GTK_POLICY_AUTOMATIC,
        GTK_POLICY_NEVER
    );

    gtk_scrolled_window_set_has_frame(
        GTK_SCROLLED_WINDOW(
            sidebar->page_switcher_scrolled_window
        ),
        FALSE
    );

    gtk_scrolled_window_set_overlay_scrolling(
        GTK_SCROLLED_WINDOW(
            sidebar->page_switcher_scrolled_window
        ),
        FALSE
    );

    gtk_scrolled_window_set_placement(
        GTK_SCROLLED_WINDOW(
            sidebar->page_switcher_scrolled_window
        ),
        GTK_CORNER_TOP_LEFT
    );

    gtk_scrolled_window_set_child(
        GTK_SCROLLED_WINDOW(
            sidebar->page_switcher_scrolled_window
        ),
        sidebar->page_switcher
    );

    gtk_widget_set_margin_start(
        sidebar->page_switcher_scrolled_window,
        8
    );

    gtk_widget_set_margin_end(
        sidebar->page_switcher_scrolled_window,
        8
    );

    gtk_widget_set_margin_bottom(
        sidebar->page_switcher_scrolled_window,
        8
    );

    gtk_box_append(
        GTK_BOX(
            sidebar->root_widget
        ),
        sidebar->page_switcher_scrolled_window
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
     * Page Entités : recherche locale, compteur et liste sélectionnable.
     */
    sidebar->entity_page = gtk_box_new(
        GTK_ORIENTATION_VERTICAL,
        8
    );
    sidebar->entity_search_entry = gtk_search_entry_new();
    sidebar->entity_count_label = gtk_label_new("0 entité");
    sidebar->entity_scrolled_window = gtk_scrolled_window_new();
    sidebar->entity_list_box = gtk_list_box_new();
    sidebar->entity_empty_label = gtk_label_new(
        "Aucune entité active dans cette enquête."
    );
    sidebar->entity_no_result_label = gtk_label_new(
        "Aucune entité ne correspond à la recherche."
    );

    if (sidebar->entity_page == NULL ||
        sidebar->entity_search_entry == NULL ||
        sidebar->entity_count_label == NULL ||
        sidebar->entity_scrolled_window == NULL ||
        sidebar->entity_list_box == NULL ||
        sidebar->entity_empty_label == NULL ||
        sidebar->entity_no_result_label == NULL)
    {
        sidebar_free(sidebar);
        return NULL;
    }

    gtk_widget_set_margin_start(sidebar->entity_search_entry, 8);
    gtk_widget_set_margin_end(sidebar->entity_search_entry, 8);
    gtk_search_entry_set_placeholder_text(
        GTK_SEARCH_ENTRY(sidebar->entity_search_entry),
        "Rechercher une entité"
    );
    gtk_widget_set_halign(
        sidebar->entity_count_label,
        GTK_ALIGN_START
    );
    gtk_widget_set_margin_start(sidebar->entity_count_label, 12);
    gtk_widget_add_css_class(
        sidebar->entity_count_label,
        "dim-label"
    );
    gtk_widget_set_hexpand(sidebar->entity_scrolled_window, TRUE);
    gtk_widget_set_vexpand(sidebar->entity_scrolled_window, TRUE);
    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(sidebar->entity_scrolled_window),
        GTK_POLICY_NEVER,
        GTK_POLICY_AUTOMATIC
    );
    gtk_scrolled_window_set_has_frame(
        GTK_SCROLLED_WINDOW(sidebar->entity_scrolled_window),
        FALSE
    );
    gtk_widget_set_hexpand(sidebar->entity_list_box, TRUE);
    gtk_scrolled_window_set_child(
        GTK_SCROLLED_WINDOW(sidebar->entity_scrolled_window),
        sidebar->entity_list_box
    );
    gtk_list_box_set_selection_mode(
        GTK_LIST_BOX(sidebar->entity_list_box),
        GTK_SELECTION_SINGLE
    );
    gtk_list_box_set_filter_func(
        GTK_LIST_BOX(sidebar->entity_list_box),
        sidebar_filter_entity_row,
        sidebar,
        NULL
    );
    gtk_label_set_wrap(
        GTK_LABEL(sidebar->entity_no_result_label),
        TRUE
    );
    gtk_widget_set_margin_start(sidebar->entity_no_result_label, 20);
    gtk_widget_set_margin_end(sidebar->entity_no_result_label, 20);
    gtk_widget_set_margin_top(sidebar->entity_no_result_label, 20);
    gtk_list_box_set_placeholder(
        GTK_LIST_BOX(sidebar->entity_list_box),
        sidebar->entity_no_result_label
    );
    gtk_widget_set_halign(sidebar->entity_empty_label, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(sidebar->entity_empty_label, GTK_ALIGN_CENTER);
    gtk_widget_set_vexpand(sidebar->entity_empty_label, TRUE);
    gtk_label_set_wrap(GTK_LABEL(sidebar->entity_empty_label), TRUE);

    g_signal_connect(
        sidebar->entity_search_entry,
        "search-changed",
        G_CALLBACK(sidebar_on_entity_search_changed),
        sidebar
    );
    g_signal_connect(
        sidebar->entity_list_box,
        "row-selected",
        G_CALLBACK(sidebar_on_entity_row_selected),
        sidebar
    );

    gtk_box_append(
        GTK_BOX(sidebar->entity_page),
        sidebar->entity_search_entry
    );
    gtk_box_append(
        GTK_BOX(sidebar->entity_page),
        sidebar->entity_count_label
    );
    gtk_box_append(
        GTK_BOX(sidebar->entity_page),
        sidebar->entity_scrolled_window
    );
    gtk_box_append(
        GTK_BOX(sidebar->entity_page),
        sidebar->entity_empty_label
    );
    gtk_stack_add_titled(
        GTK_STACK(sidebar->stack),
        sidebar->entity_page,
        SIDEBAR_PAGE_ENTITY,
        "Entités"
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

void sidebar_set_entity_model(
    Sidebar *sidebar,
    const InvestigationGraphModel *graph_model
)
{
    GPtrArray *entity_records = NULL;
    GtkWidget *child = NULL;
    guint entity_index = 0;
    char *count_text = NULL;

    if (sidebar == NULL || sidebar->entity_list_box == NULL)
    {
        return;
    }

    while ((child = gtk_widget_get_first_child(
                sidebar->entity_list_box
            )) != NULL)
    {
        gtk_list_box_remove(
            GTK_LIST_BOX(sidebar->entity_list_box),
            child
        );
    }

    if (graph_model != NULL)
    {
        entity_records = investigation_graph_model_list_entities(
            graph_model,
            NULL
        );
    }

    for (entity_index = 0;
         entity_records != NULL && entity_index < entity_records->len;
         entity_index++)
    {
        const EntityRecord *entity_record = g_ptr_array_index(
            entity_records,
            entity_index
        );
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        GtkWidget *value_label = gtk_label_new(
            entity_record_get_value(entity_record)
        );
        GtkWidget *type_label = gtk_label_new(
            entity_record_get_type_identifier(entity_record)
        );

        if (row == NULL || content == NULL ||
            value_label == NULL || type_label == NULL)
        {
            continue;
        }

        gtk_widget_set_margin_start(content, 12);
        gtk_widget_set_margin_end(content, 12);
        gtk_widget_set_margin_top(content, 8);
        gtk_widget_set_margin_bottom(content, 8);
        gtk_label_set_xalign(GTK_LABEL(value_label), 0.0F);
        gtk_label_set_ellipsize(
            GTK_LABEL(value_label),
            PANGO_ELLIPSIZE_END
        );
        gtk_label_set_xalign(GTK_LABEL(type_label), 0.0F);
        gtk_widget_add_css_class(type_label, "dim-label");
        gtk_box_append(GTK_BOX(content), value_label);
        gtk_box_append(GTK_BOX(content), type_label);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), content);
        g_object_set_data(
            G_OBJECT(row),
            "entity-record",
            (gpointer) entity_record
        );
        gtk_list_box_append(
            GTK_LIST_BOX(sidebar->entity_list_box),
            row
        );
    }

    count_text = g_strdup_printf(
        "%u %s",
        entity_records != NULL ? entity_records->len : 0U,
        entity_records != NULL && entity_records->len > 1U
            ? "entités"
            : "entité"
    );
    gtk_label_set_text(
        GTK_LABEL(sidebar->entity_count_label),
        count_text != NULL ? count_text : "0 entité"
    );
    gtk_widget_set_visible(
        sidebar->entity_empty_label,
        entity_records == NULL || entity_records->len == 0U
    );
    gtk_widget_set_visible(
        sidebar->entity_scrolled_window,
        entity_records != NULL && entity_records->len > 0U
    );

    g_free(count_text);
    g_clear_pointer(&entity_records, g_ptr_array_unref);
}

void sidebar_set_entity_selection_callback(
    Sidebar *sidebar,
    SidebarEntitySelectionCallback callback,
    gpointer user_data
)
{
    if (sidebar == NULL)
    {
        return;
    }

    sidebar->entity_selection_callback = callback;
    sidebar->entity_selection_user_data = user_data;
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

    sidebar->entity_selection_callback = NULL;
    sidebar->entity_selection_user_data = NULL;

    /*
     * Les widgets GTK sont intégrés dans l'arbre de widgets de la fenêtre.
     * GTK gère leur destruction lorsque la fenêtre est détruite.
     * Nous libérons uniquement la structure Sidebar.
     */
    g_free(sidebar);
}
