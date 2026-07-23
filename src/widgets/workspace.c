/******************************************************************************
 * @file workspace.c
 * @brief Implémentation de la zone principale de travail.
 ******************************************************************************/

#include "widgets/workspace.h"

#include "models/entity_record.h"
#include "models/investigation_graph_model.h"
#include "models/osint_selection_context.h"
#include "models/osint_action_catalog.h"
#include "models/relation_record.h"
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
    GtkWidget *graph_toolbar;
    GtkWidget *reset_graph_layout_button;
    GtkWidget *osint_tools_button;
    GtkWidget *osint_tools_popover;
    GtkWidget *osint_tools_context_label;
    GtkWidget *osint_tools_status_label;
    GtkWidget *osint_tools_actions_box;
    GtkWidget *graph_help_button;
    GtkWidget *graph_help_popover;

    InvestigationGraphView *graph_view;
    EntityDetailsPanel *entity_details_panel;
    GtkWidget *relation_details_panel;
    GtkWidget *relation_details_title_label;
    GtkWidget *relation_details_summary_label;
    GtkWidget *relation_evidences_label;
    GtkWidget *edit_relation_button;
    GtkWidget *delete_relation_button;
    GtkWidget *close_relation_details_button;
    char *selected_relation_identifier;

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
    GtkWidget *edit_evidence_button;
    GtkWidget *analyze_eml_button;
    GtkWidget *analyze_rib_button;
    GtkWidget *extract_metadata_button;
    GtkWidget *recover_pdf_password_button;
    GtkWidget *evidence_preview_stack;
    GtkWidget *evidence_preview_status;
    GtkWidget *evidence_preview_text;
    GtkWidget *evidence_preview_picture;
    GtkWidget *evidence_preview_video;
    GtkWidget *evidence_preview_expand_button;
    char *evidence_preview_path;
    gboolean evidence_preview_is_video;

    char *selected_evidence_identifier;

    WorkspaceVerifyEvidenceCallback
        verify_evidence_callback;

    gpointer
        verify_evidence_user_data;

    WorkspaceEditEvidenceCallback edit_evidence_callback;
    gpointer edit_evidence_user_data;
    WorkspaceAnalyzeEmlCallback analyze_eml_callback;
    gpointer analyze_eml_user_data;
    WorkspaceAnalyzeRibCallback analyze_rib_callback;
    gpointer analyze_rib_user_data;
    WorkspaceExtractMetadataCallback extract_metadata_callback;
    gpointer extract_metadata_user_data;
    WorkspaceRecoverPdfPasswordCallback recover_pdf_password_callback;
    gpointer recover_pdf_password_user_data;

    WorkspaceGraphNodeMovedCallback
        graph_node_moved_callback;

    gpointer
        graph_node_moved_user_data;

    WorkspaceExtractionDropCallback extraction_drop_callback;
    gpointer extraction_drop_user_data;

    WorkspaceResetGraphLayoutCallback
        reset_graph_layout_callback;

    gpointer
        reset_graph_layout_user_data;

    WorkspaceAddRelationCallback
        add_relation_callback;

    gpointer
        add_relation_user_data;

    WorkspaceEditRelationCallback edit_relation_callback;
    WorkspaceDeleteRelationCallback delete_relation_callback;
    gpointer delete_relation_user_data;
    WorkspaceRelationSelectedCallback relation_selected_callback;
    gpointer relation_selected_user_data;
    gpointer edit_relation_user_data;
    WorkspacePersonRoleCallback person_role_callback;
    gpointer person_role_user_data;
    WorkspacePersonConfidenceCallback person_confidence_callback;
    gpointer person_confidence_user_data;
    WorkspacePersonNameCallback person_name_callback;
    gpointer person_name_user_data;
    WorkspacePersonEvidenceCallback person_evidence_callback;
    gpointer person_evidence_user_data;
    WorkspaceEntitySelectedCallback entity_selected_callback;
    gpointer entity_selected_user_data;

    WorkspaceOsintActionCallback
        osint_action_callback;

    gpointer
        osint_action_user_data;

    GtkWidget *node_name_label;
    GtkWidget *node_path_label;
    GtkWidget *node_type_label;
    GtkWidget *node_parent_label;
    GtkWidget *node_children_label;

    const InvestigationGraphModel *graph_model;
    const InvestigationGraphLayout *graph_layout;

    OsintSelectionContext *osint_selection_context;
    OsintActionCatalog *osint_action_catalog;

    WorkspaceGraphState graph_state;
};

/**
 * @brief Traite localement l'aperçu ou relaie une action OSINT exécutable.
 */
static void workspace_on_osint_action_clicked(
    GtkButton *button,
    gpointer user_data
)
{
    Workspace *workspace = user_data;
    const char *action_identifier = g_object_get_data(
        G_OBJECT(button),
        "osint-action-identifier"
    );
    const char *target_value = NULL;
    const char *target_identifier = NULL;

    if (workspace == NULL || workspace->osint_tools_status_label == NULL)
    {
        return;
    }

    if (g_strcmp0(action_identifier, "selection-preview") == 0)
    {
        gtk_label_set_text(
            GTK_LABEL(workspace->osint_tools_status_label),
            "Aperçu local : aucun outil externe exécuté."
        );
        return;
    }

    target_value = osint_selection_context_get_value(
        workspace->osint_selection_context
    );
    target_identifier = osint_selection_context_get_identifier(
        workspace->osint_selection_context
    );

    if (workspace->osint_action_callback == NULL ||
        action_identifier == NULL || target_identifier == NULL ||
        target_value == NULL)
    {
        gtk_label_set_text(
            GTK_LABEL(workspace->osint_tools_status_label),
            "Impossible de transmettre cette action OSINT."
        );
        return;
    }

    gtk_label_set_text(
        GTK_LABEL(workspace->osint_tools_status_label),
        g_str_has_prefix(action_identifier, "history-")
            ? "Ouverture de l'historique enregistré."
            : "Exécution lancée ; suivez sa progression dans les tâches."
    );

    gtk_popover_popdown(
        GTK_POPOVER(workspace->osint_tools_popover)
    );

    workspace->osint_action_callback(
        action_identifier,
        target_identifier,
        target_value,
        workspace->osint_action_user_data
    );
}

/**
 * @brief Actualise le menu OSINT selon la sélection du graphe.
 *
 * Aucun outil n'est encore exposé par ce ticket. Le menu prépare le point
 * d'entrée contextuel et explique explicitement cet état à l'utilisateur.
 */
static void workspace_update_osint_tools_menu(
    Workspace *workspace,
    const OsintSelectionContext *context
)
{
    const char *selection_kind =
        NULL;

    const char *selection_identifier =
        NULL;

    char *context_text =
        NULL;

    GPtrArray *compatible_actions =
        NULL;

    GtkWidget *child =
        NULL;

    if (workspace == NULL ||
        workspace->osint_tools_button == NULL ||
        workspace->osint_tools_context_label == NULL ||
        workspace->osint_tools_status_label == NULL ||
        workspace->osint_tools_actions_box == NULL)
    {
        return;
    }

    while ((child = gtk_widget_get_first_child(
                workspace->osint_tools_actions_box
            )) != NULL)
    {
        gtk_box_remove(GTK_BOX(workspace->osint_tools_actions_box), child);
    }

    if (osint_selection_context_get_kind(context) ==
        OSINT_SELECTION_CONTEXT_KIND_ENTITY)
    {
        selection_kind =
            "Entité";

        selection_identifier =
            osint_selection_context_get_identifier(context);
    }
    else if (osint_selection_context_get_kind(context) ==
             OSINT_SELECTION_CONTEXT_KIND_RELATION)
    {
        selection_kind =
            "Relation";

        selection_identifier =
            osint_selection_context_get_identifier(context);
    }

    gtk_widget_set_sensitive(
        workspace->osint_tools_button,
        selection_identifier != NULL &&
        selection_identifier[0] != '\0'
    );

    if (selection_identifier == NULL ||
        selection_identifier[0] == '\0')
    {
        gtk_label_set_text(
            GTK_LABEL(
                workspace->osint_tools_context_label
            ),
            "Sélectionnez un nœud du graphe."
        );

        gtk_label_set_text(
            GTK_LABEL(
                workspace->osint_tools_status_label
            ),
            "Les outils compatibles apparaîtront ici."
        );

        return;
    }

    compatible_actions = osint_action_catalog_list_compatible(
        workspace->osint_action_catalog,
        context
    );

    {
        GtkWidget *history_button = gtk_button_new_with_label(
            "Historique OSINT"
        );
        const char *history_action =
            osint_selection_context_get_kind(context) ==
                OSINT_SELECTION_CONTEXT_KIND_RELATION
                ? "history-relation" : "history-entity";
        gtk_widget_set_tooltip_text(
            history_button,
            "Consulter les exécutions OSINT enregistrées pour cette sélection"
        );
        g_object_set_data_full(
            G_OBJECT(history_button), "osint-action-identifier",
            g_strdup(history_action), g_free
        );
        g_signal_connect(
            history_button, "clicked",
            G_CALLBACK(workspace_on_osint_action_clicked), workspace
        );
        gtk_box_append(
            GTK_BOX(workspace->osint_tools_actions_box), history_button
        );
    }

    for (guint action_index = 0;
         compatible_actions != NULL &&
         action_index < compatible_actions->len;
         action_index++)
    {
        const OsintAction *action = g_ptr_array_index(
            compatible_actions,
            action_index
        );
        const char *tool_version = osint_action_get_tool_version(action);
        char *button_label = tool_version != NULL && tool_version[0] != '\0'
            ? g_strdup_printf("%s — %s", osint_action_get_label(action), tool_version)
            : g_strdup(osint_action_get_label(action));
        GtkWidget *action_button = gtk_button_new_with_label(button_label);

        g_free(button_label);

        if (action_button == NULL)
        {
            continue;
        }

        gtk_widget_set_sensitive(
            action_button,
            osint_action_is_available(action)
        );
        gtk_widget_set_tooltip_text(
            action_button,
            osint_action_is_available(action)
                ? osint_action_get_description(action)
                : osint_action_get_unavailable_reason(action)
        );
        g_object_set_data_full(
            G_OBJECT(action_button),
            "osint-action-identifier",
            g_strdup(osint_action_get_identifier(action)),
            g_free
        );
        g_signal_connect(
            action_button,
            "clicked",
            G_CALLBACK(workspace_on_osint_action_clicked),
            workspace
        );
        gtk_box_append(
            GTK_BOX(workspace->osint_tools_actions_box),
            action_button
        );
    }

    context_text =
        g_strdup_printf(
            "%s sélectionnée\n%s",
            selection_kind,
            selection_identifier
        );

    gtk_label_set_text(
        GTK_LABEL(
            workspace->osint_tools_context_label
        ),
        context_text != NULL
            ? context_text
            : selection_kind
    );

    gtk_label_set_text(
        GTK_LABEL(
            workspace->osint_tools_status_label
        ),
        compatible_actions != NULL && compatible_actions->len > 0U
            ? "Actions compatibles avec la sélection."
            : "Aucune action OSINT compatible."
    );

    g_free(
        context_text
    );

    g_clear_pointer(&compatible_actions, g_ptr_array_unref);
}

/** @brief Ouvre le menu OSINT depuis le bouton de la fiche entité. */
static void workspace_on_entity_osint_requested(const char *entity_identifier,
    gpointer user_data)
{
    Workspace *workspace = user_data;
    (void) entity_identifier;
    if (workspace == NULL || workspace->osint_tools_popover == NULL) return;
    workspace_update_osint_tools_menu(workspace,
        workspace->osint_selection_context);
    gtk_popover_popup(GTK_POPOVER(workspace->osint_tools_popover));
}

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

/** @brief Affiche une erreur asynchrone produite par le moteur vidéo. */
static void workspace_on_video_error_changed(GObject *object,
    GParamSpec *parameter, gpointer user_data)
{
    Workspace *workspace = user_data;
    const GError *error = NULL;
    (void) parameter;
    if (workspace == NULL || !GTK_IS_MEDIA_STREAM(object)) return;
    error = gtk_media_stream_get_error(GTK_MEDIA_STREAM(object));
    if (error == NULL) return;
    gtk_label_set_text(GTK_LABEL(workspace->evidence_preview_status),
        error->message != NULL ? error->message :
        "Le moteur vidéo ne peut pas lire ce fichier.");
    gtk_stack_set_visible_child_name(GTK_STACK(
        workspace->evidence_preview_stack), "status");
    gtk_widget_set_sensitive(workspace->evidence_preview_expand_button,
        FALSE);
}

/** @brief Arrête et détache le média vidéo actuellement affiché. */
static void workspace_clear_evidence_preview(Workspace *workspace)
{
    GtkMediaStream *stream = NULL;
    if (workspace == NULL) return;
    if (workspace->evidence_preview_video != NULL)
    {
        stream = gtk_video_get_media_stream(
            GTK_VIDEO(workspace->evidence_preview_video));
        if (stream != NULL)
        {
            g_signal_handlers_disconnect_by_data(stream, workspace);
            gtk_media_stream_pause(stream);
        }
        gtk_video_set_media_stream(GTK_VIDEO(
            workspace->evidence_preview_video), NULL);
    }
    if (workspace->evidence_preview_picture != NULL)
        gtk_picture_set_paintable(GTK_PICTURE(
            workspace->evidence_preview_picture), NULL);
    g_clear_pointer(&workspace->evidence_preview_path, g_free);
    workspace->evidence_preview_is_video = FALSE;
    if (workspace->evidence_preview_expand_button != NULL)
        gtk_widget_set_sensitive(workspace->evidence_preview_expand_button,
            FALSE);
}

/** @brief Ouvre le média courant dans une fenêtre d'aperçu agrandie. */
static void workspace_on_expand_evidence_preview(GtkButton *button,
    gpointer user_data)
{
    Workspace *workspace = user_data;
    GtkWindow *window = NULL;
    GtkWidget *content = NULL;
    (void) button;
    if (workspace == NULL || workspace->evidence_preview_path == NULL) return;
    window = GTK_WINDOW(gtk_window_new());
    gtk_window_set_title(window, "Aperçu de la preuve");
    gtk_window_set_default_size(window, 960, 720);
    if (workspace->evidence_preview_is_video)
    {
        content = gtk_video_new_for_filename(workspace->evidence_preview_path);
        gtk_video_set_autoplay(GTK_VIDEO(content), FALSE);
    }
    else
    {
        content = gtk_picture_new_for_filename(workspace->evidence_preview_path);
        gtk_picture_set_can_shrink(GTK_PICTURE(content), TRUE);
        gtk_picture_set_content_fit(GTK_PICTURE(content), GTK_CONTENT_FIT_CONTAIN);
    }
    gtk_window_set_child(window, content);
    gtk_window_present(window);
}

/** @brief Convertit le statut d'une relation en texte utilisateur. */
static const char *workspace_get_relation_status_text(RelationStatus status)
{
    switch (status)
    {
        case RELATION_STATUS_ACTIVE: return "Active";
        case RELATION_STATUS_ARCHIVED: return "Archivée";
        case RELATION_STATUS_DELETED: return "Supprimée";
        case RELATION_STATUS_DISPUTED: return "Contestée";
        case RELATION_STATUS_UNKNOWN:
        default: return "Inconnu";
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
static void workspace_on_graph_node_selected(
    const EntityRecord *entity_record,
    const RelationRecord *relation_record,
    gpointer user_data
)
{
    Workspace *workspace =
        user_data;

    if (workspace == NULL)
    {
        return;
    }

    g_clear_pointer(
        &workspace->osint_selection_context,
        osint_selection_context_free
    );
    g_clear_pointer(&workspace->selected_relation_identifier, g_free);

    if (entity_record != NULL)
    {
        workspace->osint_selection_context =
            osint_selection_context_new_entity(entity_record, NULL);
        if (workspace->entity_selected_callback != NULL)
            workspace->entity_selected_callback(
                entity_record_get_identifier(entity_record),
                workspace->entity_selected_user_data);
    }

    if (workspace->entity_details_panel != NULL)
    {
        entity_details_panel_set_entity(
            workspace->entity_details_panel,
            entity_record
        );
    }

    if (workspace->relation_details_panel != NULL)
    {
        gtk_widget_set_can_target(workspace->relation_details_panel,
            relation_record != NULL);
        gtk_widget_set_visible(
            workspace->relation_details_panel,
            relation_record != NULL
        );
    }

    if (relation_record != NULL &&
        workspace->relation_details_title_label != NULL &&
        workspace->relation_details_summary_label != NULL)
    {
        workspace->selected_relation_identifier = g_strdup(
            relation_record_get_identifier(relation_record));
        const EntityRecord *source_entity =
            investigation_graph_model_find_entity(
                workspace->graph_model,
                relation_record_get_source_entity_identifier(relation_record)
            );
        const EntityRecord *target_entity =
            investigation_graph_model_find_entity(
                workspace->graph_model,
                relation_record_get_target_entity_identifier(relation_record)
            );

        workspace->osint_selection_context =
            osint_selection_context_new_relation(
                relation_record,
                source_entity,
                target_entity,
                NULL
            );
        const char *label = relation_record_get_label(relation_record);
        char *summary = g_strdup_printf(
            "%s → %s\n\nType : %s\nConfiance : %d %%\nStatut : %s\n"
            "Justification : %s\nCréée le : %s\nModifiée le : %s\nUUID : %s",
            source_entity != NULL ? entity_record_get_value(source_entity) :
                "Source inconnue",
            target_entity != NULL ? entity_record_get_value(target_entity) :
                "Cible inconnue",
            relation_record_get_relation_type(relation_record),
            relation_record_get_confidence(relation_record),
            workspace_get_relation_status_text(
                relation_record_get_status(relation_record)),
            relation_record_get_justification(relation_record) != NULL
                ? relation_record_get_justification(relation_record)
                : "Non renseignée",
            relation_record_get_created_at(relation_record),
            relation_record_get_updated_at(relation_record),
            relation_record_get_identifier(relation_record)
        );

        gtk_label_set_text(
            GTK_LABEL(workspace->relation_details_title_label),
            label != NULL && label[0] != '\0' ? label : "Relation"
        );
        gtk_label_set_text(
            GTK_LABEL(workspace->relation_details_summary_label),
            summary != NULL ? summary : "Informations indisponibles"
        );
        g_free(summary);

        if (workspace->relation_selected_callback != NULL)
            workspace->relation_selected_callback(
                workspace->selected_relation_identifier,
                workspace->relation_selected_user_data);
    }

    workspace_update_osint_tools_menu(
        workspace,
        workspace->osint_selection_context
    );
}

/** @brief Relaie la demande de modification de la relation affichée. */
static void workspace_on_edit_relation_clicked(GtkButton *button,
    gpointer user_data)
{
    Workspace *workspace = user_data;
    (void) button;
    if (workspace == NULL || workspace->edit_relation_callback == NULL ||
        workspace->selected_relation_identifier == NULL)
    {
        return;
    }
    workspace->edit_relation_callback(workspace->selected_relation_identifier,
        workspace->edit_relation_user_data);
}

/** @brief Relaie la demande de suppression de la relation affichée. */
static void workspace_on_delete_relation_clicked(GtkButton *button,
    gpointer user_data)
{
    Workspace *workspace = user_data;
    (void) button;
    if (workspace == NULL || workspace->delete_relation_callback == NULL ||
        workspace->selected_relation_identifier == NULL) return;
    workspace->delete_relation_callback(workspace->selected_relation_identifier,
        workspace->delete_relation_user_data);
}

/** @brief Ferme le volet de relation et désélectionne le graphe. */
static void workspace_on_close_relation_details_clicked(GtkButton *button,
    gpointer user_data)
{
    Workspace *workspace = user_data;
    (void) button;
    if (workspace != NULL)
        investigation_graph_view_clear_selection(workspace->graph_view);
}

/**
 * @brief Relaie la demande d'ajout d'une relation.
 */
static void workspace_on_add_relation_requested(
    const char *source_entity_identifier,
    gpointer user_data
)
{
    Workspace *workspace =
        user_data;

    if (workspace == NULL ||
        workspace->add_relation_callback == NULL ||
        source_entity_identifier == NULL ||
        !g_uuid_string_is_valid(
            source_entity_identifier
        ))
    {
        return;
    }

    workspace->add_relation_callback(
        source_entity_identifier,
        workspace->add_relation_user_data
    );
}

/** @brief Relaie le changement de catégorie d'une personne. */
static void workspace_on_person_role_changed(const char *entity_identifier,
    PersonRole role, gpointer user_data)
{
    Workspace *workspace = user_data;
    if (workspace != NULL && workspace->person_role_callback != NULL)
        workspace->person_role_callback(entity_identifier, role,
            workspace->person_role_user_data);
}

/** @brief Relaie la modification de confiance d'une personne. */
static void workspace_on_person_confidence_changed(
    const char *entity_identifier, gint confidence, gpointer user_data)
{
    Workspace *workspace = user_data;
    if (workspace != NULL && workspace->person_confidence_callback != NULL)
        workspace->person_confidence_callback(entity_identifier, confidence,
            workspace->person_confidence_user_data);
}

/** @brief Relaie la modification du nom affiché d'une personne. */
static void workspace_on_person_name_changed(const char *entity_identifier,
    const char *display_name, gpointer user_data)
{
    Workspace *workspace = user_data;
    if (workspace != NULL && workspace->person_name_callback != NULL)
        workspace->person_name_callback(entity_identifier, display_name,
            workspace->person_name_user_data);
}
/** @brief Relaie la gestion des preuves d'une personne. */
static void workspace_on_person_evidence_requested(const char *identifier,
    gpointer user_data)
{
    Workspace *workspace = user_data;
    if (workspace != NULL && workspace->person_evidence_callback != NULL)
        workspace->person_evidence_callback(identifier,
            workspace->person_evidence_user_data);
}

static void workspace_on_extraction_dropped(const char *file_path,
    const char *target_entity_identifier, gpointer user_data)
{
    Workspace *workspace = user_data;
    if (workspace != NULL && workspace->extraction_drop_callback != NULL)
        workspace->extraction_drop_callback(file_path, target_entity_identifier,
            workspace->extraction_drop_user_data);
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
static void workspace_on_reset_graph_layout_clicked(
    GtkButton *button,
    gpointer user_data
)
{
    Workspace *workspace =
        user_data;

    (void) button;

    if (workspace == NULL ||
        workspace->graph_state != WORKSPACE_GRAPH_STATE_READY ||
        workspace->graph_model == NULL ||
        workspace->reset_graph_layout_callback == NULL)
    {
        return;
    }

    workspace->reset_graph_layout_callback(
        workspace->reset_graph_layout_user_data
    );
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

/** @brief Transmet la demande de modification de la preuve affichée. */
static void workspace_on_edit_evidence_clicked(
    GtkButton *button, gpointer user_data
)
{
    Workspace *workspace = user_data;
    (void) button;
    if (workspace == NULL || workspace->edit_evidence_callback == NULL ||
        workspace->selected_evidence_identifier == NULL) return;
    workspace->edit_evidence_callback(
        workspace->selected_evidence_identifier,
        workspace->edit_evidence_user_data);
}
/** @brief Transmet la demande d'analyse de la preuve EML affichée. */
static void workspace_on_analyze_eml_clicked(GtkButton *button, gpointer data)
{
    Workspace *workspace = data; (void) button;
    if (workspace != NULL && workspace->analyze_eml_callback != NULL &&
        workspace->selected_evidence_identifier != NULL)
        workspace->analyze_eml_callback(workspace->selected_evidence_identifier,
            workspace->analyze_eml_user_data);
}
/** @brief Transmet la demande d'analyse OCR du RIB affiché. */
static void workspace_on_analyze_rib_clicked(GtkButton *button, gpointer data)
{
    Workspace *workspace = data; (void) button;
    if (workspace != NULL && workspace->analyze_rib_callback != NULL &&
        workspace->selected_evidence_identifier != NULL)
        workspace->analyze_rib_callback(workspace->selected_evidence_identifier,
            workspace->analyze_rib_user_data);
}

/** @brief Transmet la demande d'extraction des métadonnées. */
static void workspace_on_extract_metadata_clicked(GtkButton *button,
    gpointer data)
{
    Workspace *workspace = data;
    (void) button;
    if (workspace != NULL && workspace->extract_metadata_callback != NULL &&
        workspace->selected_evidence_identifier != NULL)
        workspace->extract_metadata_callback(
            workspace->selected_evidence_identifier,
            workspace->extract_metadata_user_data);
}

/** @brief Transmet la demande de récupération du mot de passe PDF. */
static void workspace_on_recover_pdf_password_clicked(GtkButton *button,
    gpointer data)
{
    Workspace *workspace = data;
    (void) button;
    if (workspace != NULL && workspace->recover_pdf_password_callback != NULL &&
        workspace->selected_evidence_identifier != NULL)
        workspace->recover_pdf_password_callback(
            workspace->selected_evidence_identifier,
            workspace->recover_pdf_password_user_data);
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

    workspace->evidence_preview_stack = gtk_stack_new();
    workspace->evidence_preview_status = gtk_label_new(
        "Aucun aperçu disponible pour cette preuve.");
    workspace->evidence_preview_picture = gtk_picture_new();
    workspace->evidence_preview_video = gtk_video_new();
    workspace->evidence_preview_expand_button =
        gtk_button_new_with_label("Agrandir l’aperçu");
    gtk_widget_set_size_request(workspace->evidence_preview_stack, -1, 360);
    gtk_widget_set_hexpand(workspace->evidence_preview_stack, TRUE);
    gtk_picture_set_can_shrink(GTK_PICTURE(
        workspace->evidence_preview_picture), TRUE);
    gtk_picture_set_content_fit(GTK_PICTURE(
        workspace->evidence_preview_picture), GTK_CONTENT_FIT_CONTAIN);
    gtk_video_set_autoplay(GTK_VIDEO(workspace->evidence_preview_video), FALSE);
    gtk_stack_add_named(GTK_STACK(workspace->evidence_preview_stack),
        workspace->evidence_preview_status, "status");
    workspace->evidence_preview_text = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(workspace->evidence_preview_text), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(workspace->evidence_preview_text), GTK_WRAP_WORD_CHAR);
    gtk_widget_set_vexpand(workspace->evidence_preview_text, TRUE);
    gtk_stack_add_named(GTK_STACK(workspace->evidence_preview_stack),
        workspace->evidence_preview_text, "text");
    gtk_stack_add_named(GTK_STACK(workspace->evidence_preview_stack),
        workspace->evidence_preview_picture, "image");
    gtk_stack_add_named(GTK_STACK(workspace->evidence_preview_stack),
        workspace->evidence_preview_video, "video");
    gtk_stack_set_visible_child_name(GTK_STACK(
        workspace->evidence_preview_stack), "status");
    gtk_widget_set_halign(workspace->evidence_preview_expand_button,
        GTK_ALIGN_START);
    gtk_widget_set_sensitive(workspace->evidence_preview_expand_button, FALSE);
    g_signal_connect(workspace->evidence_preview_expand_button, "clicked",
        G_CALLBACK(workspace_on_expand_evidence_preview), workspace);
    gtk_box_append(GTK_BOX(evidence_content),
        workspace->evidence_preview_stack);
    gtk_box_append(GTK_BOX(evidence_content),
        workspace->evidence_preview_expand_button);

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

    workspace->edit_evidence_button = gtk_button_new_with_label(
        "Modifier les métadonnées");
    gtk_widget_set_halign(workspace->edit_evidence_button, GTK_ALIGN_START);
    gtk_widget_set_sensitive(workspace->edit_evidence_button, FALSE);
    gtk_widget_set_tooltip_text(workspace->edit_evidence_button,
        "Corriger le classement sans modifier le fichier original");
    g_signal_connect(workspace->edit_evidence_button, "clicked",
        G_CALLBACK(workspace_on_edit_evidence_clicked), workspace);
    gtk_box_append(GTK_BOX(evidence_content), workspace->edit_evidence_button);
    workspace->analyze_eml_button = gtk_button_new_with_label(
        "Analyser les en-têtes EML");
    gtk_widget_set_halign(workspace->analyze_eml_button, GTK_ALIGN_START);
    gtk_widget_set_sensitive(workspace->analyze_eml_button, FALSE);
    gtk_widget_set_tooltip_text(workspace->analyze_eml_button,
        "Créer une copie vérifiée puis analyser localement ses en-têtes");
    g_signal_connect(workspace->analyze_eml_button, "clicked",
        G_CALLBACK(workspace_on_analyze_eml_clicked), workspace);
    gtk_box_append(GTK_BOX(evidence_content), workspace->analyze_eml_button);
    workspace->analyze_rib_button = gtk_button_new_with_label(
        "Analyser le RIB par OCR");
    gtk_widget_set_halign(workspace->analyze_rib_button, GTK_ALIGN_START);
    gtk_widget_set_sensitive(workspace->analyze_rib_button, FALSE);
    g_signal_connect(workspace->analyze_rib_button, "clicked",
        G_CALLBACK(workspace_on_analyze_rib_clicked), workspace);
    gtk_box_append(GTK_BOX(evidence_content), workspace->analyze_rib_button);
    workspace->extract_metadata_button = gtk_button_new_with_label(
        "Extraire les métadonnées");
    gtk_widget_set_halign(workspace->extract_metadata_button, GTK_ALIGN_START);
    gtk_widget_set_sensitive(workspace->extract_metadata_button, FALSE);
    gtk_widget_set_tooltip_text(workspace->extract_metadata_button,
        "Analyser localement une copie avec ExifTool");
    g_signal_connect(workspace->extract_metadata_button, "clicked",
        G_CALLBACK(workspace_on_extract_metadata_clicked), workspace);
    gtk_box_append(GTK_BOX(evidence_content),
        workspace->extract_metadata_button);
    workspace->recover_pdf_password_button = gtk_button_new_with_label(
        "Récupérer le mot de passe PDF");
    gtk_widget_set_halign(workspace->recover_pdf_password_button,
        GTK_ALIGN_START);
    gtk_widget_set_sensitive(workspace->recover_pdf_password_button, FALSE);
    g_signal_connect(workspace->recover_pdf_password_button, "clicked",
        G_CALLBACK(workspace_on_recover_pdf_password_clicked), workspace);
    gtk_box_append(GTK_BOX(evidence_content),
        workspace->recover_pdf_password_button);

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

    workspace->relation_details_panel = gtk_box_new(
        GTK_ORIENTATION_VERTICAL,
        12
    );
    workspace->relation_details_title_label = gtk_label_new(NULL);
    workspace->relation_details_summary_label = gtk_label_new(NULL);
    workspace->relation_evidences_label = gtk_label_new(
        "Pièces jointes : chargement…");
    workspace->edit_relation_button = gtk_button_new_with_label("Modifier");
    workspace->delete_relation_button = gtk_button_new_with_label(
        "Supprimer la relation");
    workspace->close_relation_details_button =
        gtk_button_new_from_icon_name("window-close-symbolic");

    if (workspace->graph_view == NULL ||
        workspace->entity_details_panel == NULL ||
        workspace->relation_details_panel == NULL ||
        workspace->relation_details_title_label == NULL ||
        workspace->relation_details_summary_label == NULL ||
        workspace->relation_evidences_label == NULL ||
        workspace->edit_relation_button == NULL ||
        workspace->delete_relation_button == NULL ||
        workspace->close_relation_details_button == NULL)
    {
        workspace_free(
            workspace
        );

        return NULL;
    }

    gtk_widget_set_size_request(workspace->relation_details_panel, 300, -1);
    gtk_widget_set_halign(workspace->relation_details_panel, GTK_ALIGN_END);
    gtk_widget_set_valign(workspace->relation_details_panel, GTK_ALIGN_START);
    gtk_widget_set_margin_end(workspace->relation_details_panel, 12);
    gtk_widget_set_margin_top(workspace->relation_details_panel, 56);
    gtk_widget_set_margin_start(workspace->relation_details_panel, 12);
    gtk_widget_set_margin_bottom(workspace->relation_details_panel, 12);
    /* `view` fournit le fond opaque du thème, comme le volet d'entité. */
    gtk_widget_add_css_class(workspace->relation_details_panel, "view");
    gtk_widget_add_css_class(workspace->relation_details_panel, "card");
    gtk_widget_set_margin_start(workspace->relation_details_title_label, 16);
    gtk_widget_set_margin_end(workspace->relation_details_title_label, 16);
    gtk_widget_set_margin_top(workspace->relation_details_title_label, 16);
    gtk_label_set_xalign(
        GTK_LABEL(workspace->relation_details_title_label),
        0.0F
    );
    gtk_widget_add_css_class(
        workspace->relation_details_title_label,
        "title-3"
    );
    gtk_widget_set_margin_start(workspace->relation_details_summary_label, 16);
    gtk_widget_set_margin_end(workspace->relation_details_summary_label, 16);
    gtk_widget_set_margin_bottom(workspace->relation_details_summary_label, 16);
    gtk_label_set_xalign(
        GTK_LABEL(workspace->relation_details_summary_label),
        0.0F
    );
    gtk_label_set_wrap(
        GTK_LABEL(workspace->relation_details_summary_label),
        TRUE
    );
    gtk_label_set_selectable(
        GTK_LABEL(workspace->relation_details_summary_label),
        TRUE
    );
    gtk_widget_set_margin_start(workspace->relation_evidences_label, 16);
    gtk_widget_set_margin_end(workspace->relation_evidences_label, 16);
    gtk_widget_set_margin_bottom(workspace->relation_evidences_label, 12);
    gtk_label_set_xalign(GTK_LABEL(workspace->relation_evidences_label), 0.0F);
    gtk_label_set_wrap(GTK_LABEL(workspace->relation_evidences_label), TRUE);
    gtk_label_set_selectable(GTK_LABEL(workspace->relation_evidences_label),
        TRUE);
    gtk_box_append(
        GTK_BOX(workspace->relation_details_panel),
        workspace->relation_details_title_label
    );
    gtk_widget_set_tooltip_text(workspace->close_relation_details_button,
        "Fermer le volet");
    gtk_widget_add_css_class(workspace->close_relation_details_button, "flat");
    gtk_box_append(GTK_BOX(workspace->relation_details_panel),
        workspace->close_relation_details_button);
    gtk_box_append(
        GTK_BOX(workspace->relation_details_panel),
        workspace->relation_details_summary_label
    );
    gtk_box_append(GTK_BOX(workspace->relation_details_panel),
        workspace->relation_evidences_label);
    gtk_box_append(GTK_BOX(workspace->relation_details_panel),
        workspace->edit_relation_button);
    gtk_widget_add_css_class(workspace->delete_relation_button,
        "destructive-action");
    gtk_box_append(GTK_BOX(workspace->relation_details_panel),
        workspace->delete_relation_button);
    g_signal_connect(workspace->edit_relation_button, "clicked",
        G_CALLBACK(workspace_on_edit_relation_clicked), workspace);
    g_signal_connect(workspace->delete_relation_button, "clicked",
        G_CALLBACK(workspace_on_delete_relation_clicked), workspace);
    g_signal_connect(workspace->close_relation_details_button, "clicked",
        G_CALLBACK(workspace_on_close_relation_details_clicked), workspace);
    gtk_widget_set_visible(workspace->relation_details_panel, FALSE);
    gtk_widget_set_can_target(workspace->relation_details_panel, FALSE);

    investigation_graph_view_set_selection_callback(
        workspace->graph_view,
        workspace_on_graph_node_selected,
        workspace
    );

    investigation_graph_view_set_node_moved_callback(
        workspace->graph_view,
        workspace_on_graph_node_moved,
        workspace
    );
    investigation_graph_view_set_extraction_drop_callback(
        workspace->graph_view, workspace_on_extraction_dropped, workspace);

    entity_details_panel_set_close_callback(
        workspace->entity_details_panel,
        workspace_on_entity_details_closed,
        workspace
    );

    entity_details_panel_set_add_relation_callback(
        workspace->entity_details_panel,
        workspace_on_add_relation_requested,
        workspace
    );

    entity_details_panel_set_person_role_callback(
        workspace->entity_details_panel, workspace_on_person_role_changed,
        workspace);
    entity_details_panel_set_person_confidence_callback(
        workspace->entity_details_panel,
        workspace_on_person_confidence_changed, workspace);
    entity_details_panel_set_person_name_callback(
        workspace->entity_details_panel, workspace_on_person_name_changed,
        workspace);
    entity_details_panel_set_person_evidence_callback(
        workspace->entity_details_panel,
        workspace_on_person_evidence_requested, workspace);
    entity_details_panel_set_osint_callback(workspace->entity_details_panel,
        workspace_on_entity_osint_requested, workspace);

    gtk_overlay_set_child(
        GTK_OVERLAY(
            workspace->graph_view_page
        ),
        investigation_graph_view_get_widget(
            workspace->graph_view
        )
    );

    workspace->graph_toolbar =
        gtk_box_new(
            GTK_ORIENTATION_HORIZONTAL,
            4
        );

    workspace->reset_graph_layout_button =
        gtk_button_new_from_icon_name(
            "view-refresh-symbolic"
        );

    workspace->osint_tools_button =
        gtk_menu_button_new();

    workspace->osint_tools_popover =
        gtk_popover_new();

    GtkWidget *osint_tools_content =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            8
        );

    GtkWidget *osint_tools_title =
        gtk_label_new(
            "Outils OSINT"
        );

    workspace->osint_tools_context_label =
        gtk_label_new(
            "Sélectionnez un nœud du graphe."
        );

    workspace->osint_tools_status_label =
        gtk_label_new(
            "Les outils compatibles apparaîtront ici."
        );

    workspace->osint_tools_actions_box = gtk_box_new(
        GTK_ORIENTATION_VERTICAL,
        6
    );

    workspace->osint_action_catalog =
        osint_action_catalog_new_defaults();

    if (workspace->graph_toolbar == NULL ||
        workspace->reset_graph_layout_button == NULL ||
        workspace->osint_tools_button == NULL ||
        workspace->osint_tools_popover == NULL ||
        osint_tools_content == NULL ||
        osint_tools_title == NULL ||
        workspace->osint_tools_context_label == NULL ||
        workspace->osint_tools_status_label == NULL ||
        workspace->osint_tools_actions_box == NULL ||
        workspace->osint_action_catalog == NULL)
    {
        workspace_free(
            workspace
        );

        return NULL;
    }

    gtk_widget_set_halign(
        workspace->graph_toolbar,
        GTK_ALIGN_START
    );

    gtk_widget_set_valign(
        workspace->graph_toolbar,
        GTK_ALIGN_START
    );

    gtk_widget_set_margin_start(
        workspace->graph_toolbar,
        12
    );

    gtk_widget_set_margin_top(
        workspace->graph_toolbar,
        12
    );

    gtk_widget_add_css_class(
        workspace->graph_toolbar,
        "toolbar"
    );

    gtk_widget_add_css_class(
        workspace->reset_graph_layout_button,
        "flat"
    );

    gtk_widget_set_tooltip_text(
        workspace->reset_graph_layout_button,
        "Réinitialiser la disposition du graphe"
    );

    gtk_widget_set_sensitive(
        workspace->reset_graph_layout_button,
        FALSE
    );

    gtk_menu_button_set_label(
        GTK_MENU_BUTTON(
            workspace->osint_tools_button
        ),
        "Outils OSINT"
    );

    gtk_widget_add_css_class(
        workspace->osint_tools_button,
        "flat"
    );

    gtk_widget_set_tooltip_text(
        workspace->osint_tools_button,
        "Afficher les outils compatibles avec la sélection"
    );

    gtk_widget_set_sensitive(
        workspace->osint_tools_button,
        FALSE
    );

    gtk_widget_set_margin_start(
        osint_tools_content,
        16
    );

    gtk_widget_set_margin_end(
        osint_tools_content,
        16
    );

    gtk_widget_set_margin_top(
        osint_tools_content,
        14
    );

    gtk_widget_set_margin_bottom(
        osint_tools_content,
        14
    );

    gtk_label_set_xalign(
        GTK_LABEL(
            osint_tools_title
        ),
        0.0F
    );

    gtk_widget_add_css_class(
        osint_tools_title,
        "heading"
    );

    gtk_label_set_xalign(
        GTK_LABEL(
            workspace->osint_tools_context_label
        ),
        0.0F
    );

    gtk_label_set_selectable(
        GTK_LABEL(
            workspace->osint_tools_context_label
        ),
        TRUE
    );

    gtk_label_set_xalign(
        GTK_LABEL(
            workspace->osint_tools_status_label
        ),
        0.0F
    );

    gtk_widget_add_css_class(
        workspace->osint_tools_status_label,
        "dim-label"
    );

    gtk_box_append(
        GTK_BOX(
            osint_tools_content
        ),
        osint_tools_title
    );

    gtk_box_append(
        GTK_BOX(
            osint_tools_content
        ),
        workspace->osint_tools_context_label
    );

    gtk_box_append(
        GTK_BOX(osint_tools_content),
        workspace->osint_tools_actions_box
    );

    gtk_box_append(
        GTK_BOX(
            osint_tools_content
        ),
        workspace->osint_tools_status_label
    );

    gtk_popover_set_child(
        GTK_POPOVER(
            workspace->osint_tools_popover
        ),
        osint_tools_content
    );

    gtk_menu_button_set_popover(
        GTK_MENU_BUTTON(
            workspace->osint_tools_button
        ),
        workspace->osint_tools_popover
    );

    g_signal_connect(
        workspace->reset_graph_layout_button,
        "clicked",
        G_CALLBACK(
            workspace_on_reset_graph_layout_clicked
        ),
        workspace
    );

    gtk_box_append(
        GTK_BOX(
            workspace->graph_toolbar
        ),
        workspace->reset_graph_layout_button
    );

    gtk_box_append(
        GTK_BOX(
            workspace->graph_toolbar
        ),
        workspace->osint_tools_button
    );

    gtk_overlay_add_overlay(
        GTK_OVERLAY(
            workspace->graph_view_page
        ),
        workspace->graph_toolbar
    );

    gtk_overlay_set_clip_overlay(
        GTK_OVERLAY(
            workspace->graph_view_page
        ),
        workspace->graph_toolbar,
        TRUE
    );

    workspace->graph_help_button =
        gtk_menu_button_new();

    workspace->graph_help_popover =
        gtk_popover_new();

    GtkWidget *graph_help_content =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            8
        );

    GtkWidget *graph_help_title =
        gtk_label_new(
            "Commandes du graphe"
        );

    GtkWidget *graph_help_text =
        gtk_label_new(
            "• Clic : sélectionner un nœud\n"
            "• Glisser un nœud : le déplacer\n"
            "• Maj + glisser : déplacer la vue\n"
            "• Ctrl + molette : zoomer\n"
            "• Pincement : zoomer"
        );

    if (workspace->graph_help_button == NULL ||
        workspace->graph_help_popover == NULL ||
        graph_help_content == NULL ||
        graph_help_title == NULL ||
        graph_help_text == NULL)
    {
        workspace_free(
            workspace
        );

        return NULL;
    }

    gtk_menu_button_set_label(
        GTK_MENU_BUTTON(
            workspace->graph_help_button
        ),
        "?"
    );

    gtk_widget_add_css_class(
        workspace->graph_help_button,
        "flat"
    );

    gtk_widget_set_tooltip_text(
        workspace->graph_help_button,
        "Afficher les commandes du graphe"
    );

    gtk_widget_set_can_target(
        workspace->graph_help_button,
        TRUE
    );

    gtk_widget_set_halign(
        workspace->graph_help_button,
        GTK_ALIGN_END
    );

    gtk_widget_set_valign(
        workspace->graph_help_button,
        GTK_ALIGN_START
    );

    gtk_widget_set_margin_end(
        workspace->graph_help_button,
        12
    );

    gtk_widget_set_margin_top(
        workspace->graph_help_button,
        12
    );

    gtk_widget_set_margin_start(
        graph_help_content,
        16
    );

    gtk_widget_set_margin_end(
        graph_help_content,
        16
    );

    gtk_widget_set_margin_top(
        graph_help_content,
        14
    );

    gtk_widget_set_margin_bottom(
        graph_help_content,
        14
    );

    gtk_label_set_xalign(
        GTK_LABEL(
            graph_help_title
        ),
        0.0F
    );

    gtk_widget_add_css_class(
        graph_help_title,
        "heading"
    );

    gtk_label_set_xalign(
        GTK_LABEL(
            graph_help_text
        ),
        0.0F
    );

    gtk_label_set_wrap(
        GTK_LABEL(
            graph_help_text
        ),
        TRUE
    );

    gtk_label_set_wrap_mode(
        GTK_LABEL(
            graph_help_text
        ),
        PANGO_WRAP_WORD_CHAR
    );

    gtk_label_set_max_width_chars(
        GTK_LABEL(
            graph_help_text
        ),
        44
    );

    gtk_box_append(
        GTK_BOX(
            graph_help_content
        ),
        graph_help_title
    );

    gtk_box_append(
        GTK_BOX(
            graph_help_content
        ),
        graph_help_text
    );

    gtk_popover_set_child(
        GTK_POPOVER(
            workspace->graph_help_popover
        ),
        graph_help_content
    );

    gtk_menu_button_set_popover(
        GTK_MENU_BUTTON(
            workspace->graph_help_button
        ),
        workspace->graph_help_popover
    );

    gtk_overlay_add_overlay(
        GTK_OVERLAY(
            workspace->graph_view_page
        ),
        entity_details_panel_get_widget(
            workspace->entity_details_panel
        )
    );

    gtk_overlay_add_overlay(
        GTK_OVERLAY(workspace->graph_view_page),
        workspace->relation_details_panel
    );

    gtk_overlay_set_clip_overlay(
        GTK_OVERLAY(workspace->graph_view_page),
        workspace->relation_details_panel,
        TRUE
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

    gtk_overlay_add_overlay(
        GTK_OVERLAY(
            workspace->graph_view_page
        ),
        workspace->graph_help_button
    );

    gtk_overlay_set_clip_overlay(
        GTK_OVERLAY(
            workspace->graph_view_page
        ),
        workspace->graph_help_button,
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
    if (workspace->edit_evidence_button != NULL)
        gtk_widget_set_sensitive(workspace->edit_evidence_button, FALSE);
    if (workspace->analyze_eml_button != NULL)
        gtk_widget_set_sensitive(workspace->analyze_eml_button, FALSE);
    if (workspace->analyze_rib_button != NULL)
        gtk_widget_set_sensitive(workspace->analyze_rib_button, FALSE);
    if (workspace->extract_metadata_button != NULL)
        gtk_widget_set_sensitive(workspace->extract_metadata_button, FALSE);
    if (workspace->recover_pdf_password_button != NULL)
        gtk_widget_set_sensitive(workspace->recover_pdf_password_button, FALSE);

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
        node_type == INVESTIGATION_NODE_FILE
            ? WORKSPACE_PAGE_EVIDENCE_INFORMATION
            : WORKSPACE_PAGE_NODE_INFORMATION
    );

    if (node_type == INVESTIGATION_NODE_FILE)
    {
        gtk_label_set_text(GTK_LABEL(workspace->evidence_name_label),
            node_name != NULL ? node_name : "Fichier");
        workspace_set_evidence_preview(workspace, node_path, node_name);
    }

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

    workspace_clear_evidence_preview(workspace);
    if (workspace->evidence_preview_stack != NULL)
        gtk_stack_set_visible_child_name(GTK_STACK(
            workspace->evidence_preview_stack), "status");

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
    gtk_widget_set_sensitive(workspace->edit_evidence_button, TRUE);
    {
        const char *name = evidence_record_get_original_name(evidence_record);
        char *lower = name != NULL ? g_ascii_strdown(name, -1) : NULL;
        gtk_widget_set_sensitive(workspace->analyze_eml_button,
            lower != NULL && g_str_has_suffix(lower, ".eml"));
        gtk_widget_set_sensitive(workspace->analyze_rib_button,
            lower != NULL && (g_str_has_suffix(lower, ".jpg") ||
            g_str_has_suffix(lower, ".jpeg") || g_str_has_suffix(lower, ".png")));
        gtk_widget_set_sensitive(workspace->extract_metadata_button,
            lower != NULL && (g_str_has_suffix(lower, ".jpg") ||
            g_str_has_suffix(lower, ".jpeg") ||
            g_str_has_suffix(lower, ".png") ||
            g_str_has_suffix(lower, ".mov") ||
            g_str_has_suffix(lower, ".mp4") ||
            g_str_has_suffix(lower, ".pdf")));
        gtk_widget_set_sensitive(workspace->recover_pdf_password_button,
            lower != NULL && g_str_has_suffix(lower, ".pdf"));
        g_free(lower);
    }

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

void workspace_set_evidence_preview(Workspace *workspace,
    const char *file_path, const char *display_name)
{
    GFile *file = NULL;
    GFileInfo *info = NULL;
    const char *content_type = NULL;
    char *mime_type = NULL;
    GtkMediaStream *stream = NULL;
    GError *error = NULL;
    if (workspace == NULL) return;
    workspace_clear_evidence_preview(workspace);
    if (file_path == NULL || file_path[0] == '\0')
    {
        gtk_label_set_text(GTK_LABEL(workspace->evidence_preview_status),
            "Aucun aperçu disponible pour cette preuve.");
        gtk_stack_set_visible_child_name(GTK_STACK(
            workspace->evidence_preview_stack), "status");
        return;
    }
    file = g_file_new_for_path(file_path);
    info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
        G_FILE_QUERY_INFO_NONE, NULL, &error);
    if (info != NULL) content_type = g_file_info_get_content_type(info);
    if (content_type != NULL) mime_type = g_content_type_get_mime_type(
        content_type);
    if (mime_type != NULL && g_str_has_prefix(mime_type, "image/"))
    {
        gtk_picture_set_filename(GTK_PICTURE(
            workspace->evidence_preview_picture), file_path);
        gtk_stack_set_visible_child_name(GTK_STACK(
            workspace->evidence_preview_stack), "image");
        workspace->evidence_preview_is_video = FALSE;
    }
    else if (mime_type != NULL && g_str_has_prefix(mime_type, "video/"))
    {
        stream = gtk_media_file_new_for_filename(file_path);
        g_signal_connect(stream, "notify::error",
            G_CALLBACK(workspace_on_video_error_changed), workspace);
        gtk_video_set_media_stream(GTK_VIDEO(
            workspace->evidence_preview_video), stream);
        g_object_unref(stream);
        gtk_stack_set_visible_child_name(GTK_STACK(
            workspace->evidence_preview_stack), "video");
        workspace->evidence_preview_is_video = TRUE;
    }
    else
    {
        char *message = NULL;
        if ((mime_type != NULL && g_strcmp0(mime_type, "text/plain") == 0) ||
            g_str_has_suffix(file_path, ".txt") ||
            g_str_has_suffix(file_path, ".log") ||
            g_str_has_suffix(file_path, ".json"))
        {
            char *contents = NULL;
            gsize length = 0;
            if (g_file_get_contents(file_path, &contents, &length, NULL))
            {
                GtkTextBuffer *buffer = gtk_text_view_get_buffer(
                    GTK_TEXT_VIEW(workspace->evidence_preview_text));
                gtk_text_buffer_set_text(buffer, contents, (gint) MIN(length, 200000));
                gtk_stack_set_visible_child_name(GTK_STACK(
                    workspace->evidence_preview_stack), "text");
                workspace->evidence_preview_is_video = FALSE;
                g_free(contents);
                goto text_preview_ready;
            }
            g_free(contents);
        }
        message = g_strdup_printf(
            "Aperçu indisponible pour %s%s%s.",
            display_name != NULL ? display_name : "ce fichier",
            error != NULL ? " : " : "",
            error != NULL ? error->message : "format non pris en charge");
        gtk_label_set_text(GTK_LABEL(workspace->evidence_preview_status),
            message);
        gtk_stack_set_visible_child_name(GTK_STACK(
            workspace->evidence_preview_stack), "status");
        g_free(message);
        goto cleanup;
    }
text_preview_ready:
    workspace->evidence_preview_path = g_strdup(file_path);
    gtk_widget_set_sensitive(workspace->evidence_preview_expand_button,
        workspace->evidence_preview_path != NULL);
cleanup:
    g_clear_error(&error);
    g_free(mime_type);
    g_clear_object(&info);
    g_clear_object(&file);
}

gboolean workspace_select_graph_entity(
    Workspace *workspace,
    const char *entity_identifier
)
{
    if (workspace == NULL || workspace->graph_view == NULL)
    {
        return FALSE;
    }

    return investigation_graph_view_select_entity(
        workspace->graph_view,
        entity_identifier
    );
}

gboolean workspace_select_graph_relation(
    Workspace *workspace,
    const char *relation_identifier
)
{
    if (workspace == NULL || workspace->graph_view == NULL)
    {
        return FALSE;
    }

    return investigation_graph_view_select_relation(
        workspace->graph_view,
        relation_identifier
    );
}

void workspace_set_osint_tool_state(
    Workspace *workspace,
    const char *tool_identifier,
    OsintActionToolState state,
    const char *version
)
{
    if (workspace == NULL || workspace->osint_action_catalog == NULL)
    {
        return;
    }

    osint_action_catalog_update_tool_state(
        workspace->osint_action_catalog,
        tool_identifier,
        state,
        version
    );
    workspace_update_osint_tools_menu(
        workspace,
        workspace->osint_selection_context
    );
}

void workspace_set_osint_action_callback(
    Workspace *workspace,
    WorkspaceOsintActionCallback callback,
    gpointer user_data
)
{
    if (workspace == NULL)
    {
        return;
    }

    workspace->osint_action_callback = callback;
    workspace->osint_action_user_data = user_data;
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

    gtk_widget_set_sensitive(
        workspace->reset_graph_layout_button,
        FALSE
    );

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

    gtk_widget_set_sensitive(
        workspace->reset_graph_layout_button,
        TRUE
    );

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

    gtk_widget_set_sensitive(
        workspace->reset_graph_layout_button,
        FALSE
    );

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

    gtk_widget_set_sensitive(
        workspace->reset_graph_layout_button,
        FALSE
    );

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

void workspace_set_edit_evidence_callback(
    Workspace *workspace, WorkspaceEditEvidenceCallback callback,
    gpointer user_data
)
{
    if (workspace == NULL) return;
    workspace->edit_evidence_callback = callback;
    workspace->edit_evidence_user_data = user_data;
}
void workspace_set_analyze_eml_callback(Workspace *workspace,
    WorkspaceAnalyzeEmlCallback callback, gpointer user_data)
{
    if (workspace == NULL) return;
    workspace->analyze_eml_callback = callback;
    workspace->analyze_eml_user_data = user_data;
}
void workspace_set_analyze_rib_callback(Workspace *workspace,
    WorkspaceAnalyzeRibCallback callback, gpointer user_data)
{
    if (workspace == NULL) return;
    workspace->analyze_rib_callback = callback;
    workspace->analyze_rib_user_data = user_data;
}
void workspace_set_extract_metadata_callback(Workspace *workspace,
    WorkspaceExtractMetadataCallback callback, gpointer user_data)
{
    if (workspace == NULL) return;
    workspace->extract_metadata_callback = callback;
    workspace->extract_metadata_user_data = user_data;
}
void workspace_set_recover_pdf_password_callback(Workspace *workspace,
    WorkspaceRecoverPdfPasswordCallback callback, gpointer user_data)
{
    if (workspace == NULL) return;
    workspace->recover_pdf_password_callback = callback;
    workspace->recover_pdf_password_user_data = user_data;
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

void workspace_set_extraction_drop_callback(
    Workspace *workspace,
    WorkspaceExtractionDropCallback callback,
    gpointer user_data)
{
    if (workspace == NULL) return;
    workspace->extraction_drop_callback = callback;
    workspace->extraction_drop_user_data = user_data;
}

void workspace_set_reset_graph_layout_callback(
    Workspace *workspace,
    WorkspaceResetGraphLayoutCallback callback,
    gpointer user_data
)
{
    if (workspace == NULL)
    {
        return;
    }

    workspace->reset_graph_layout_callback =
        callback;

    workspace->reset_graph_layout_user_data =
        user_data;
}

void workspace_set_add_relation_callback(
    Workspace *workspace,
    WorkspaceAddRelationCallback callback,
    gpointer user_data
)
{
    if (workspace == NULL)
    {
        return;
    }

    workspace->add_relation_callback =
        callback;

    workspace->add_relation_user_data =
        user_data;
}

void workspace_set_edit_relation_callback(Workspace *workspace,
    WorkspaceEditRelationCallback callback, gpointer user_data)
{
    if (workspace == NULL) return;
    workspace->edit_relation_callback = callback;
    workspace->edit_relation_user_data = user_data;
}

void workspace_set_delete_relation_callback(Workspace *workspace,
    WorkspaceDeleteRelationCallback callback, gpointer user_data)
{
    if (workspace == NULL) return;
    workspace->delete_relation_callback = callback;
    workspace->delete_relation_user_data = user_data;
}

void workspace_set_relation_selected_callback(Workspace *workspace,
    WorkspaceRelationSelectedCallback callback, gpointer user_data)
{
    if (workspace == NULL) return;
    workspace->relation_selected_callback = callback;
    workspace->relation_selected_user_data = user_data;
}

void workspace_set_relation_evidences(Workspace *workspace,
    const GPtrArray *evidence_records)
{
    GString *text = NULL;
    if (workspace == NULL || workspace->relation_evidences_label == NULL)
        return;
    text = g_string_new("Pièces jointes");
    if (evidence_records == NULL || evidence_records->len == 0)
        g_string_append(text, " : aucune");
    else
    {
        g_string_append_printf(text, " (%u) :", evidence_records->len);
        for (guint index = 0; index < evidence_records->len; index++)
        {
            const EvidenceRecord *record = g_ptr_array_index(evidence_records,
                index);
            if (record == NULL) continue;
            g_string_append_printf(text, "\n• %s — %s",
                evidence_record_get_original_name(record),
                evidence_record_get_type_identifier(record));
        }
    }
    gtk_label_set_text(GTK_LABEL(workspace->relation_evidences_label),
        text->str);
    g_string_free(text, TRUE);
}

void workspace_set_person_role_callback(Workspace *workspace,
    WorkspacePersonRoleCallback callback, gpointer user_data)
{
    if (workspace == NULL) return;
    workspace->person_role_callback = callback;
    workspace->person_role_user_data = user_data;
}

void workspace_set_person_confidence_callback(Workspace *workspace,
    WorkspacePersonConfidenceCallback callback, gpointer user_data)
{
    if (workspace == NULL) return;
    workspace->person_confidence_callback = callback;
    workspace->person_confidence_user_data = user_data;
}

void workspace_set_person_name_callback(Workspace *workspace,
    WorkspacePersonNameCallback callback, gpointer user_data)
{
    if (workspace == NULL) return;
    workspace->person_name_callback = callback;
    workspace->person_name_user_data = user_data;
}
void workspace_set_person_evidence_callback(Workspace *workspace,
    WorkspacePersonEvidenceCallback callback, gpointer user_data)
{
    if (workspace == NULL) return;
    workspace->person_evidence_callback = callback;
    workspace->person_evidence_user_data = user_data;
}
void workspace_set_entity_selected_callback(Workspace *workspace,
    WorkspaceEntitySelectedCallback callback, gpointer user_data)
{
    if (workspace == NULL) return;
    workspace->entity_selected_callback = callback;
    workspace->entity_selected_user_data = user_data;
}
void workspace_set_person_evidences(Workspace *workspace,
    const GPtrArray *records)
{
    if (workspace == NULL) return;
    entity_details_panel_set_person_evidences(
        workspace->entity_details_panel, records);
}

void workspace_reset_graph_layout(
    Workspace *workspace
)
{
    if (workspace == NULL ||
        workspace->graph_view == NULL ||
        workspace->graph_state != WORKSPACE_GRAPH_STATE_READY ||
        workspace->graph_model == NULL)
    {
        return;
    }

    entity_details_panel_clear(
        workspace->entity_details_panel
    );

    investigation_graph_view_clear_selection(
        workspace->graph_view
    );

    investigation_graph_view_reset_layout(
        workspace->graph_view
    );
}

void workspace_free(Workspace *workspace)
{
    if (workspace == NULL)
    {
        return;
    }

    workspace_clear_evidence_preview(workspace);

    g_clear_pointer(
        &workspace->osint_selection_context,
        osint_selection_context_free
    );
    g_clear_pointer(&workspace->selected_relation_identifier, g_free);

    g_clear_pointer(
        &workspace->osint_action_catalog,
        osint_action_catalog_free
    );

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

    entity_details_panel_set_add_relation_callback(
        workspace->entity_details_panel,
        NULL,
        NULL
    );
    entity_details_panel_set_person_role_callback(
        workspace->entity_details_panel, NULL, NULL);

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

    workspace->reset_graph_layout_callback =
        NULL;

    workspace->reset_graph_layout_user_data =
        NULL;

    workspace->add_relation_callback =
        NULL;

    workspace->add_relation_user_data =
        NULL;

    workspace->graph_toolbar =
        NULL;

    workspace->reset_graph_layout_button =
        NULL;

    workspace->graph_help_button =
        NULL;

    workspace->graph_help_popover =
        NULL;

    g_clear_pointer(
        &workspace->selected_evidence_identifier,
        g_free
    );

    g_free(workspace);
}
