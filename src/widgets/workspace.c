/******************************************************************************
 * @file workspace.c
 * @brief Implémentation de la zone principale de travail.
 ******************************************************************************/

#include "widgets/workspace.h"

#include "widgets/entity_details_panel.h"
#include "widgets/investigation_graph_view.h"

#include <glib.h>

#define WORKSPACE_PAGE_WELCOME "welcome"
#define WORKSPACE_PAGE_NODE_INFORMATION "node-information"
#define WORKSPACE_PAGE_EVIDENCE_INFORMATION \
    "evidence-information"
#define WORKSPACE_PAGE_GRAPH_STATE \
    "graph-state"
#define WORKSPACE_PAGE_GRAPH_VIEW \
    "graph-view"

/**
 * @brief État métier représenté par la page du graphe.
 */
typedef enum
{
    WORKSPACE_GRAPH_STATE_EMPTY,
    WORKSPACE_GRAPH_STATE_LOADING,
    WORKSPACE_GRAPH_STATE_READY,
    WORKSPACE_GRAPH_STATE_ERROR
} WorkspaceGraphState;

/**
 * @struct Workspace
 * @brief Représentation interne de la zone de travail.
 */
struct Workspace
{
    GtkWidget *root_widget;
    GtkWidget *stack;

    GtkWidget *welcome_page;

    GtkWidget *graph_state_page;
    GtkWidget *graph_spinner;
    GtkWidget *graph_title_label;
    GtkWidget *graph_details_label;

    GtkWidget *graph_view_page;

    InvestigationGraphView *graph_view;
    EntityDetailsPanel *entity_details_panel;

    GtkWidget *node_page;

    GtkWidget *evidence_page;
    GtkWidget *evidence_name_label;

    GtkWidget *evidence_type_label;
    GtkWidget *evidence_integrity_label;
    GtkWidget *evidence_size_label;
    GtkWidget *evidence_imported_at_label;
    GtkWidget *evidence_collected_at_label;
    GtkWidget *evidence_source_label;
    GtkWidget *evidence_description_label;
    GtkWidget *evidence_relative_path_label;
    GtkWidget *evidence_internal_name_label;
    GtkWidget *evidence_identifier_label;
    GtkWidget *evidence_sha256_label;
    GtkWidget *verify_evidence_button;

    char *selected_evidence_identifier;

    WorkspaceVerifyEvidenceCallback
        verify_evidence_callback;

    gpointer
        verify_evidence_user_data;

    WorkspaceGraphNodeMovedCallback
        graph_node_moved_callback;

    gpointer
        graph_node_moved_user_data;

    GtkWidget *node_name_label;
    GtkWidget *node_path_label;
    GtkWidget *node_type_label;
    GtkWidget *node_parent_label;
    GtkWidget *node_children_label;

    const InvestigationGraphModel *graph_model;
    const InvestigationGraphLayout *graph_layout;

    WorkspaceGraphState graph_state;
};

/**
 * @brief Ajoute une ligne de métadonnée dans la grille.
 *
 * @return Label GTK destiné à recevoir la valeur.
 */
static GtkWidget *workspace_add_evidence_field(
    GtkGrid *grid,
    gint row,
    const char *title
)
{
    GtkWidget *title_label = NULL;
    GtkWidget *value_label = NULL;

    if (grid == NULL ||
        title == NULL ||
        title[0] == '\0')
    {
        return NULL;
    }

    title_label =
        gtk_label_new(
            title
        );

    value_label =
        gtk_label_new(
            NULL
        );

    if (title_label == NULL ||
        value_label == NULL)
    {
        g_clear_object(
            &value_label
        );

        g_clear_object(
            &title_label
        );

        return NULL;
    }

    gtk_label_set_xalign(
        GTK_LABEL(title_label),
        0.0F
    );

    gtk_widget_set_valign(
        title_label,
        GTK_ALIGN_START
    );

    gtk_widget_add_css_class(
        title_label,
        "dim-label"
    );

    gtk_label_set_xalign(
        GTK_LABEL(value_label),
        0.0F
    );

    gtk_label_set_wrap(
        GTK_LABEL(value_label),
        TRUE
    );

    gtk_label_set_wrap_mode(
        GTK_LABEL(value_label),
        PANGO_WRAP_WORD_CHAR
    );

    gtk_label_set_selectable(
        GTK_LABEL(value_label),
        TRUE
    );

    gtk_widget_set_hexpand(
        value_label,
        TRUE
    );

    gtk_widget_set_halign(
        value_label,
        GTK_ALIGN_FILL
    );

    gtk_widget_set_valign(
        value_label,
        GTK_ALIGN_START
    );

    gtk_grid_attach(
        grid,
        title_label,
        0,
        row,
        1,
        1
    );

    gtk_grid_attach(
        grid,
        value_label,
        1,
        row,
        1,
        1
    );

    return value_label;
}

/**
 * @brief Définit une valeur avec un texte de remplacement.
 */
static void workspace_set_field_text(
    GtkWidget *label,
    const char *value,
    const char *fallback
)
{
    if (label == NULL)
    {
        return;
    }

    gtk_label_set_text(
        GTK_LABEL(label),
        value != NULL && value[0] != '\0'
            ? value
            : fallback
    );
}

/**
 * @brief Convertit un statut d'intégrité en texte utilisateur.
 */
static const char *workspace_get_integrity_status_text(
    EvidenceIntegrityStatus integrity_status
)
{
    switch (integrity_status)
    {
        case EVIDENCE_INTEGRITY_STATUS_UNKNOWN:
            return "Non vérifiée";

        case EVIDENCE_INTEGRITY_STATUS_VALID:
            return "Valide";

        case EVIDENCE_INTEGRITY_STATUS_MISSING:
            return "Fichier absent";

        case EVIDENCE_INTEGRITY_STATUS_MODIFIED:
            return "Fichier modifié";

        case EVIDENCE_INTEGRITY_STATUS_ERROR:
            return "Erreur de vérification";

        default:
            return "Statut inconnu";
    }
}

/**
 * @brief Transmet la sélection du graphe au volet de détails.
 */
static void workspace_on_graph_node_moved(
    const char *entity_identifier,
    double x,
    double y,
    gpointer user_data
)
{
    Workspace *workspace =
        user_data;

    if (workspace == NULL ||
        workspace->graph_node_moved_callback == NULL ||
        entity_identifier == NULL ||
        entity_identifier[0] == '\0')
    {
        return;
    }

    workspace->graph_node_moved_callback(
        entity_identifier,
        x,
        y,
        workspace->graph_node_moved_user_data
    );
}

/**
 * @brief Transmet la sélection du graphe au volet de détails.
 */
static void workspace_on_graph_entity_selected(
    const EntityRecord *entity_record,
    gpointer user_data
)
{
    Workspace *workspace =
        user_data;

    if (workspace == NULL ||
        workspace->entity_details_panel == NULL)
    {
        return;
    }

    entity_details_panel_set_entity(
        workspace->entity_details_panel,
        entity_record
    );
}

/**
 * @brief Désélectionne le nœud lorsque le volet est fermé manuellement.
 */
static void workspace_on_entity_details_closed(
    gpointer user_data
)
{
    Workspace *workspace =
        user_data;

    if (workspace == NULL ||
        workspace->graph_view == NULL)
    {
        return;
    }

    investigation_graph_view_clear_selection(
        workspace->graph_view
    );
}

/**
 * @brief Affiche l'état principal courant du Workspace.
 *
 * Sans enquête, la page d'accueil est utilisée. Le chargement et les erreurs
 * utilisent la page d'état. Un graphe prêt utilise la vue graphique.
 */
static void workspace_show_default(
    Workspace *workspace
)
{
    if (workspace == NULL)
    {
        return;
    }

    switch (workspace->graph_state)
    {
        case WORKSPACE_GRAPH_STATE_EMPTY:
            gtk_stack_set_visible_child_name(
                GTK_STACK(workspace->stack),
                WORKSPACE_PAGE_WELCOME
            );

            return;

        case WORKSPACE_GRAPH_STATE_READY:
            gtk_stack_set_visible_child_name(
                GTK_STACK(workspace->stack),
                WORKSPACE_PAGE_GRAPH_VIEW
            );

            return;

        case WORKSPACE_GRAPH_STATE_LOADING:
        case WORKSPACE_GRAPH_STATE_ERROR:
        default:
            gtk_stack_set_visible_child_name(
                GTK_STACK(workspace->stack),
                WORKSPACE_PAGE_GRAPH_STATE
            );

            return;
    }
}

/**
 * @brief Transmet la demande de vérification de la preuve affichée.
 */
static void workspace_on_verify_evidence_clicked(
    GtkButton *button,
    gpointer user_data
)
{
    Workspace *workspace =
        user_data;

    (void) button;

    if (workspace == NULL ||
        workspace->verify_evidence_callback == NULL ||
        workspace->selected_evidence_identifier == NULL ||
        workspace->selected_evidence_identifier[0] == '\0')
    {
        return;
    }

    workspace->verify_evidence_callback(
        workspace->selected_evidence_identifier,
        workspace->verify_evidence_user_data
    );
}

Workspace *workspace_new(void)
{
    GtkWidget *evidence_content = NULL;
    GtkWidget *evidence_separator = NULL;
    GtkWidget *evidence_grid = NULL;

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

        /*
     * Page détaillée d'une preuve.
     */
    workspace->evidence_page =
        gtk_scrolled_window_new();

    if (workspace->evidence_page == NULL)
    {
        workspace_free(
            workspace
        );

        return NULL;
    }

    gtk_widget_set_hexpand(
        workspace->evidence_page,
        TRUE
    );

    gtk_widget_set_vexpand(
        workspace->evidence_page,
        TRUE
    );

    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(
            workspace->evidence_page
        ),
        GTK_POLICY_AUTOMATIC,
        GTK_POLICY_AUTOMATIC
    );

    evidence_content =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            16
        );

    if (evidence_content == NULL)
    {
        workspace_free(
            workspace
        );

        return NULL;
    }

    gtk_widget_set_margin_start(
        evidence_content,
        24
    );

    gtk_widget_set_margin_end(
        evidence_content,
        24
    );

    gtk_widget_set_margin_top(
        evidence_content,
        24
    );

    gtk_widget_set_margin_bottom(
        evidence_content,
        24
    );

    workspace->evidence_name_label =
        gtk_label_new(
            NULL
        );

    if (workspace->evidence_name_label == NULL)
    {
        workspace_free(
            workspace
        );

        return NULL;
    }

    gtk_label_set_xalign(
        GTK_LABEL(
            workspace->evidence_name_label
        ),
        0.0F
    );

    gtk_label_set_wrap(
        GTK_LABEL(
            workspace->evidence_name_label
        ),
        TRUE
    );

    gtk_label_set_selectable(
        GTK_LABEL(
            workspace->evidence_name_label
        ),
        TRUE
    );

    gtk_widget_add_css_class(
        workspace->evidence_name_label,
        "title-2"
    );

    gtk_box_append(
        GTK_BOX(evidence_content),
        workspace->evidence_name_label
    );

    workspace->verify_evidence_button =
        gtk_button_new_with_label(
            "Vérifier l'intégrité"
        );

    if (workspace->verify_evidence_button == NULL)
    {
        workspace_free(
            workspace
        );

        return NULL;
    }

    gtk_widget_set_halign(
        workspace->verify_evidence_button,
        GTK_ALIGN_START
    );

    gtk_widget_set_tooltip_text(
        workspace->verify_evidence_button,
        "Recalculer le SHA-256 et le comparer à l'empreinte enregistrée"
    );

    /*
     * Aucun bouton actif tant qu'aucune preuve n'est affichée.
     */
    gtk_widget_set_sensitive(
        workspace->verify_evidence_button,
        FALSE
    );

    g_signal_connect(
        workspace->verify_evidence_button,
        "clicked",
        G_CALLBACK(
            workspace_on_verify_evidence_clicked
        ),
        workspace
    );

    gtk_box_append(
        GTK_BOX(evidence_content),
        workspace->verify_evidence_button
    );

    evidence_separator =
        gtk_separator_new(
            GTK_ORIENTATION_HORIZONTAL
        );

    gtk_box_append(
        GTK_BOX(evidence_content),
        evidence_separator
    );

    evidence_grid =
        gtk_grid_new();

    if (evidence_grid == NULL)
    {
        workspace_free(
            workspace
        );

        return NULL;
    }

    gtk_grid_set_column_spacing(
        GTK_GRID(evidence_grid),
        24
    );

    gtk_grid_set_row_spacing(
        GTK_GRID(evidence_grid),
        12
    );

    gtk_widget_set_hexpand(
        evidence_grid,
        TRUE
    );

    workspace->evidence_type_label =
        workspace_add_evidence_field(
            GTK_GRID(evidence_grid),
            0,
            "Type"
        );

    workspace->evidence_integrity_label =
        workspace_add_evidence_field(
            GTK_GRID(evidence_grid),
            1,
            "Intégrité"
        );

    workspace->evidence_size_label =
        workspace_add_evidence_field(
            GTK_GRID(evidence_grid),
            2,
            "Taille"
        );

    workspace->evidence_imported_at_label =
        workspace_add_evidence_field(
            GTK_GRID(evidence_grid),
            3,
            "Importée le"
        );

    workspace->evidence_collected_at_label =
        workspace_add_evidence_field(
            GTK_GRID(evidence_grid),
            4,
            "Collectée le"
        );

    workspace->evidence_source_label =
        workspace_add_evidence_field(
            GTK_GRID(evidence_grid),
            5,
            "Source"
        );

    workspace->evidence_description_label =
        workspace_add_evidence_field(
            GTK_GRID(evidence_grid),
            6,
            "Description"
        );

    workspace->evidence_relative_path_label =
        workspace_add_evidence_field(
            GTK_GRID(evidence_grid),
            7,
            "Chemin relatif"
        );

    workspace->evidence_internal_name_label =
        workspace_add_evidence_field(
            GTK_GRID(evidence_grid),
            8,
            "Nom interne"
        );

    workspace->evidence_identifier_label =
        workspace_add_evidence_field(
            GTK_GRID(evidence_grid),
            9,
            "Identifiant"
        );

    workspace->evidence_sha256_label =
        workspace_add_evidence_field(
            GTK_GRID(evidence_grid),
            10,
            "SHA-256"
        );

    if (workspace->evidence_type_label == NULL ||
        workspace->evidence_integrity_label == NULL ||
        workspace->evidence_size_label == NULL ||
        workspace->evidence_imported_at_label == NULL ||
        workspace->evidence_collected_at_label == NULL ||
        workspace->evidence_source_label == NULL ||
        workspace->evidence_description_label == NULL ||
        workspace->evidence_relative_path_label == NULL ||
        workspace->evidence_internal_name_label == NULL ||
        workspace->evidence_identifier_label == NULL ||
        workspace->evidence_sha256_label == NULL)
    {
        workspace_free(
            workspace
        );

        return NULL;
    }

    gtk_box_append(
        GTK_BOX(evidence_content),
        evidence_grid
    );

    gtk_scrolled_window_set_child(
        GTK_SCROLLED_WINDOW(
            workspace->evidence_page
        ),
        evidence_content
    );

    gtk_stack_add_named(
        GTK_STACK(workspace->stack),
        workspace->evidence_page,
        WORKSPACE_PAGE_EVIDENCE_INFORMATION
    );

    /*
     * Page d'état utilisée pendant le chargement ou en cas d'erreur.
     */
    workspace->graph_state_page =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            12
        );

    if (workspace->graph_state_page == NULL)
    {
        workspace_free(
            workspace
        );

        return NULL;
    }

    gtk_widget_set_halign(
        workspace->graph_state_page,
        GTK_ALIGN_CENTER
    );

    gtk_widget_set_valign(
        workspace->graph_state_page,
        GTK_ALIGN_CENTER
    );

    workspace->graph_spinner =
        gtk_spinner_new();

    workspace->graph_title_label =
        gtk_label_new(
            NULL
        );

    workspace->graph_details_label =
        gtk_label_new(
            NULL
        );

    if (workspace->graph_spinner == NULL ||
        workspace->graph_title_label == NULL ||
        workspace->graph_details_label == NULL)
    {
        workspace_free(
            workspace
        );

        return NULL;
    }

    gtk_widget_add_css_class(
        workspace->graph_title_label,
        "title-2"
    );

    gtk_label_set_wrap(
        GTK_LABEL(
            workspace->graph_details_label
        ),
        TRUE
    );

    gtk_label_set_justify(
        GTK_LABEL(
            workspace->graph_details_label
        ),
        GTK_JUSTIFY_CENTER
    );

    gtk_widget_set_visible(
        workspace->graph_spinner,
        FALSE
    );

    gtk_box_append(
        GTK_BOX(
            workspace->graph_state_page
        ),
        workspace->graph_spinner
    );

    gtk_box_append(
        GTK_BOX(
            workspace->graph_state_page
        ),
        workspace->graph_title_label
    );

    gtk_box_append(
        GTK_BOX(
            workspace->graph_state_page
        ),
        workspace->graph_details_label
    );

    gtk_stack_add_named(
        GTK_STACK(workspace->stack),
        workspace->graph_state_page,
        WORKSPACE_PAGE_GRAPH_STATE
    );

    /*
     * Page graphique : canvas principal et volet de détails superposé.
     */
    workspace->graph_view_page =
        gtk_overlay_new();

    if (workspace->graph_view_page == NULL)
    {
        workspace_free(
            workspace
        );

        return NULL;
    }

    gtk_widget_set_hexpand(
        workspace->graph_view_page,
        TRUE
    );

    gtk_widget_set_vexpand(
        workspace->graph_view_page,
        TRUE
    );

    workspace->graph_view =
        investigation_graph_view_new();

    workspace->entity_details_panel =
        entity_details_panel_new();

    if (workspace->graph_view == NULL ||
        workspace->entity_details_panel == NULL)
    {
        workspace_free(
            workspace
        );

        return NULL;
    }

    investigation_graph_view_set_selection_callback(
        workspace->graph_view,
        workspace_on_graph_entity_selected,
        workspace
    );

    investigation_graph_view_set_node_moved_callback(
        workspace->graph_view,
        workspace_on_graph_node_moved,
        workspace
    );

    entity_details_panel_set_close_callback(
        workspace->entity_details_panel,
        workspace_on_entity_details_closed,
        workspace
    );

    gtk_overlay_set_child(
        GTK_OVERLAY(
            workspace->graph_view_page
        ),
        investigation_graph_view_get_widget(
            workspace->graph_view
        )
    );

    gtk_overlay_add_overlay(
        GTK_OVERLAY(
            workspace->graph_view_page
        ),
        entity_details_panel_get_widget(
            workspace->entity_details_panel
        )
    );

    gtk_overlay_set_clip_overlay(
        GTK_OVERLAY(
            workspace->graph_view_page
        ),
        entity_details_panel_get_widget(
            workspace->entity_details_panel
        ),
        TRUE
    );

    gtk_stack_add_named(
        GTK_STACK(workspace->stack),
        workspace->graph_view_page,
        WORKSPACE_PAGE_GRAPH_VIEW
    );

    workspace->graph_model =
        NULL;

    workspace->graph_layout =
        NULL;

    workspace->graph_state =
        WORKSPACE_GRAPH_STATE_EMPTY;

    workspace_show_default(
        workspace
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

    g_clear_pointer(
        &workspace->selected_evidence_identifier,
        g_free
    );

    if (workspace->verify_evidence_button != NULL)
    {
        gtk_widget_set_sensitive(
            workspace->verify_evidence_button,
            FALSE
        );
    }

    if (node == NULL)
    {
        workspace_show_default(workspace);
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

void workspace_set_selected_evidence(
    Workspace *workspace,
    const EvidenceRecord *evidence_record
)
{
    char *human_size = NULL;
    char *size_text = NULL;

    guint64 size_bytes = 0;

    if (workspace == NULL)
    {
        return;
    }

    g_clear_pointer(
        &workspace->selected_evidence_identifier,
        g_free
    );

    if (workspace->verify_evidence_button != NULL)
    {
        gtk_widget_set_sensitive(
            workspace->verify_evidence_button,
            FALSE
        );
    }

    if (evidence_record == NULL)
    {
        workspace_show_default(
            workspace
        );

        return;
    }

    workspace->selected_evidence_identifier =
        g_strdup(
            evidence_record_get_identifier(
                evidence_record
            )
        );

    if (workspace->selected_evidence_identifier == NULL)
    {
        workspace_show_default(
            workspace
        );

        return;
    }

    gtk_widget_set_sensitive(
        workspace->verify_evidence_button,
        TRUE
    );

    size_bytes =
        evidence_record_get_size_bytes(
            evidence_record
        );

    human_size =
        g_format_size_full(
            size_bytes,
            G_FORMAT_SIZE_IEC_UNITS
        );

    size_text =
        g_strdup_printf(
            "%s (%" G_GUINT64_FORMAT " octets)",
            human_size != NULL
                ? human_size
                : "taille inconnue",
            size_bytes
        );

    workspace_set_field_text(
        workspace->evidence_name_label,
        evidence_record_get_original_name(
            evidence_record
        ),
        "(sans nom)"
    );

    workspace_set_field_text(
        workspace->evidence_type_label,
        evidence_record_get_type_identifier(
            evidence_record
        ),
        "(type inconnu)"
    );

    workspace_set_field_text(
        workspace->evidence_integrity_label,
        workspace_get_integrity_status_text(
            evidence_record_get_integrity_status(
                evidence_record
            )
        ),
        "Statut inconnu"
    );

    workspace_set_field_text(
        workspace->evidence_size_label,
        size_text,
        "Taille inconnue"
    );

    workspace_set_field_text(
        workspace->evidence_imported_at_label,
        evidence_record_get_imported_at(
            evidence_record
        ),
        "(date inconnue)"
    );

    workspace_set_field_text(
        workspace->evidence_collected_at_label,
        evidence_record_get_collected_at(
            evidence_record
        ),
        "Non renseignée"
    );

    workspace_set_field_text(
        workspace->evidence_source_label,
        evidence_record_get_source(
            evidence_record
        ),
        "Non renseignée"
    );

    workspace_set_field_text(
        workspace->evidence_description_label,
        evidence_record_get_description(
            evidence_record
        ),
        "Non renseignée"
    );

    workspace_set_field_text(
        workspace->evidence_relative_path_label,
        evidence_record_get_relative_path(
            evidence_record
        ),
        "(chemin inconnu)"
    );

    workspace_set_field_text(
        workspace->evidence_internal_name_label,
        evidence_record_get_internal_name(
            evidence_record
        ),
        "(nom interne inconnu)"
    );

    workspace_set_field_text(
        workspace->evidence_identifier_label,
        evidence_record_get_identifier(
            evidence_record
        ),
        "(identifiant inconnu)"
    );

    workspace_set_field_text(
        workspace->evidence_sha256_label,
        evidence_record_get_sha256(
            evidence_record
        ),
        "(empreinte inconnue)"
    );

    gtk_stack_set_visible_child_name(
        GTK_STACK(workspace->stack),
        WORKSPACE_PAGE_EVIDENCE_INFORMATION
    );

    g_free(
        size_text
    );

    g_free(
        human_size
    );
}

void workspace_set_graph_loading(
    Workspace *workspace
)
{
    if (workspace == NULL)
    {
        return;
    }

    g_clear_pointer(
        &workspace->selected_evidence_identifier,
        g_free
    );

    if (workspace->verify_evidence_button != NULL)
    {
        gtk_widget_set_sensitive(
            workspace->verify_evidence_button,
            FALSE
        );
    }

    entity_details_panel_clear(
        workspace->entity_details_panel
    );

    investigation_graph_view_clear(
        workspace->graph_view
    );

    workspace->graph_model =
        NULL;

    workspace->graph_layout =
        NULL;

    workspace->graph_state =
        WORKSPACE_GRAPH_STATE_LOADING;

    gtk_label_set_text(
        GTK_LABEL(
            workspace->graph_title_label
        ),
        "Chargement de l’enquête…"
    );

    gtk_label_set_text(
        GTK_LABEL(
            workspace->graph_details_label
        ),
        "Lecture des entités et des relations"
    );

    gtk_widget_set_visible(
        workspace->graph_spinner,
        TRUE
    );

    gtk_spinner_start(
        GTK_SPINNER(
            workspace->graph_spinner
        )
    );

    workspace_show_default(
        workspace
    );
}

void workspace_set_graph(
    Workspace *workspace,
    const InvestigationGraphModel *graph_model,
    const InvestigationGraphLayout *graph_layout
)
{
    if (workspace == NULL)
    {
        return;
    }

    if (graph_model == NULL)
    {
        workspace_clear_graph(
            workspace
        );

        return;
    }

    g_clear_pointer(
        &workspace->selected_evidence_identifier,
        g_free
    );

    if (workspace->verify_evidence_button != NULL)
    {
        gtk_widget_set_sensitive(
            workspace->verify_evidence_button,
            FALSE
        );
    }

    entity_details_panel_clear(
        workspace->entity_details_panel
    );

    investigation_graph_view_clear(
        workspace->graph_view
    );

    workspace->graph_model =
        graph_model;

    workspace->graph_layout =
        graph_layout;

    investigation_graph_view_set_layout(
        workspace->graph_view,
        graph_layout
    );

    investigation_graph_view_set_graph(
        workspace->graph_view,
        graph_model
    );

    workspace->graph_state =
        WORKSPACE_GRAPH_STATE_READY;

    gtk_spinner_stop(
        GTK_SPINNER(
            workspace->graph_spinner
        )
    );

    gtk_widget_set_visible(
        workspace->graph_spinner,
        FALSE
    );

    gtk_label_set_text(
        GTK_LABEL(
            workspace->graph_title_label
        ),
        ""
    );

    gtk_label_set_text(
        GTK_LABEL(
            workspace->graph_details_label
        ),
        ""
    );

    workspace_show_default(
        workspace
    );
}

void workspace_set_graph_error(
    Workspace *workspace,
    const char *message
)
{
    if (workspace == NULL)
    {
        return;
    }

    g_clear_pointer(
        &workspace->selected_evidence_identifier,
        g_free
    );

    if (workspace->verify_evidence_button != NULL)
    {
        gtk_widget_set_sensitive(
            workspace->verify_evidence_button,
            FALSE
        );
    }

    entity_details_panel_clear(
        workspace->entity_details_panel
    );

    investigation_graph_view_clear(
        workspace->graph_view
    );

    workspace->graph_model =
        NULL;

    workspace->graph_layout =
        NULL;

    workspace->graph_state =
        WORKSPACE_GRAPH_STATE_ERROR;

    gtk_spinner_stop(
        GTK_SPINNER(
            workspace->graph_spinner
        )
    );

    gtk_widget_set_visible(
        workspace->graph_spinner,
        FALSE
    );

    gtk_label_set_text(
        GTK_LABEL(
            workspace->graph_title_label
        ),
        "Impossible de charger le graphe de l’enquête"
    );

    gtk_label_set_text(
        GTK_LABEL(
            workspace->graph_details_label
        ),
        message != NULL &&
        message[0] != '\0'
            ? message
            : "Aucun détail supplémentaire n’est disponible."
    );

    workspace_show_default(
        workspace
    );
}

void workspace_clear_graph(
    Workspace *workspace
)
{
    if (workspace == NULL)
    {
        return;
    }

    g_clear_pointer(
        &workspace->selected_evidence_identifier,
        g_free
    );

    if (workspace->verify_evidence_button != NULL)
    {
        gtk_widget_set_sensitive(
            workspace->verify_evidence_button,
            FALSE
        );
    }

    entity_details_panel_clear(
        workspace->entity_details_panel
    );

    investigation_graph_view_clear(
        workspace->graph_view
    );

    workspace->graph_model =
        NULL;

    workspace->graph_layout =
        NULL;

    workspace->graph_state =
        WORKSPACE_GRAPH_STATE_EMPTY;

    gtk_spinner_stop(
        GTK_SPINNER(
            workspace->graph_spinner
        )
    );

    gtk_widget_set_visible(
        workspace->graph_spinner,
        FALSE
    );

    gtk_label_set_text(
        GTK_LABEL(
            workspace->graph_title_label
        ),
        ""
    );

    gtk_label_set_text(
        GTK_LABEL(
            workspace->graph_details_label
        ),
        ""
    );

    workspace_show_default(
        workspace
    );
}

void workspace_set_verify_evidence_callback(
    Workspace *workspace,
    WorkspaceVerifyEvidenceCallback callback,
    gpointer user_data
)
{
    if (workspace == NULL)
    {
        return;
    }

    workspace->verify_evidence_callback =
        callback;

    workspace->verify_evidence_user_data =
        user_data;
}

void workspace_set_graph_node_moved_callback(
    Workspace *workspace,
    WorkspaceGraphNodeMovedCallback callback,
    gpointer user_data
)
{
    if (workspace == NULL)
    {
        return;
    }

    workspace->graph_node_moved_callback =
        callback;

    workspace->graph_node_moved_user_data =
        user_data;
}

void workspace_free(Workspace *workspace)
{
    if (workspace == NULL)
    {
        return;
    }

    investigation_graph_view_set_selection_callback(
        workspace->graph_view,
        NULL,
        NULL
    );

    investigation_graph_view_set_node_moved_callback(
        workspace->graph_view,
        NULL,
        NULL
    );

    entity_details_panel_set_close_callback(
        workspace->entity_details_panel,
        NULL,
        NULL
    );

    entity_details_panel_clear(
        workspace->entity_details_panel
    );

    investigation_graph_view_clear(
        workspace->graph_view
    );

    investigation_graph_view_free(
        workspace->graph_view
    );

    entity_details_panel_free(
        workspace->entity_details_panel
    );

    workspace->graph_view =
        NULL;

    workspace->entity_details_panel =
        NULL;

    workspace->graph_model =
        NULL;

    workspace->graph_layout =
        NULL;

    workspace->verify_evidence_callback =
        NULL;

    workspace->verify_evidence_user_data =
        NULL;

    workspace->graph_node_moved_callback =
        NULL;

    workspace->graph_node_moved_user_data =
        NULL;

    g_clear_pointer(
        &workspace->selected_evidence_identifier,
        g_free
    );

    g_free(workspace);
}
