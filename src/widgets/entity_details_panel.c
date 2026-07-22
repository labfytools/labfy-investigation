/******************************************************************************
 * @file entity_details_panel.c
 * @brief Implémentation du volet latéral de détails d'une entité.
 ******************************************************************************/

#include "widgets/entity_details_panel.h"

#include "models/entity_record.h"
#include "models/evidence_record.h"

#include <glib.h>

/**
 * @struct EntityDetailsPanel
 * @brief État privé du volet latéral.
 */
struct EntityDetailsPanel
{
    GtkWidget *root_revealer;
    GtkWidget *frame;
    GtkWidget *add_relation_button;
    GtkWidget *close_button;

    GtkWidget *entity_title_label;
    GtkWidget *entity_value_label;
    GtkWidget *entity_type_label;
    GtkWidget *entity_description_label;
    GtkWidget *entity_confidence_label;
    GtkWidget *entity_status_label;
    GtkWidget *entity_created_at_label;
    GtkWidget *entity_updated_at_label;
    GtkWidget *entity_identifier_label;
    GtkWidget *person_role_box;
    GtkDropDown *person_role_dropdown;
    GtkSpinButton *person_confidence_spin;
    GtkWidget *person_confidence_button;
    GtkEntry *person_name_entry;
    GtkWidget *person_name_button;
    GtkWidget *person_evidence_button;
    GtkWidget *person_evidence_summary_label;

    char *selected_entity_identifier;

    EntityDetailsPanelCloseCallback close_callback;
    gpointer close_user_data;

    EntityDetailsPanelAddRelationCallback
        add_relation_callback;

    gpointer
        add_relation_user_data;

    EntityDetailsPanelPersonRoleCallback person_role_callback;
    gpointer person_role_user_data;
    gboolean updating_person_role;
    EntityDetailsPanelPersonConfidenceCallback person_confidence_callback;
    gpointer person_confidence_user_data;
    EntityDetailsPanelPersonNameCallback person_name_callback;
    gpointer person_name_user_data;
    EntityDetailsPanelPersonEvidenceCallback person_evidence_callback;
    gpointer person_evidence_user_data;
};
/** @brief Relaie la demande de gestion des preuves d'une personne. */
static void entity_details_panel_on_person_evidence_clicked(GtkButton *button,
    gpointer user_data)
{
    EntityDetailsPanel *panel = user_data; (void) button;
    if (panel != NULL && panel->person_evidence_callback != NULL &&
        panel->selected_entity_identifier != NULL)
        panel->person_evidence_callback(panel->selected_entity_identifier,
            panel->person_evidence_user_data);
}

/** @brief Relaie le nouveau nom affiché explicitement enregistré. */
static void entity_details_panel_on_person_name_clicked(GtkButton *button,
    gpointer user_data)
{
    EntityDetailsPanel *details_panel = user_data;
    const char *name = NULL;
    (void) button;
    if (details_panel == NULL || details_panel->person_name_callback == NULL ||
        details_panel->selected_entity_identifier == NULL) return;
    name = gtk_editable_get_text(GTK_EDITABLE(details_panel->person_name_entry));
    if (name == NULL || name[0] == '\0') return;
    details_panel->person_name_callback(
        details_panel->selected_entity_identifier, name,
        details_panel->person_name_user_data);
}

/** @brief Relaie la confiance explicitement enregistrée par l'utilisateur. */
static void entity_details_panel_on_person_confidence_clicked(
    GtkButton *button, gpointer user_data)
{
    EntityDetailsPanel *details_panel = user_data;
    (void) button;
    if (details_panel == NULL ||
        details_panel->person_confidence_callback == NULL ||
        details_panel->selected_entity_identifier == NULL) return;
    details_panel->person_confidence_callback(
        details_panel->selected_entity_identifier,
        gtk_spin_button_get_value_as_int(
            details_panel->person_confidence_spin),
        details_panel->person_confidence_user_data);
}

/** @brief Relaie une catégorie choisie explicitement par l'utilisateur. */
static void entity_details_panel_on_person_role_changed(GObject *object,
    GParamSpec *parameter, gpointer user_data)
{
    EntityDetailsPanel *details_panel = user_data;
    guint selected = GTK_INVALID_LIST_POSITION;
    (void) object; (void) parameter;
    if (details_panel == NULL || details_panel->updating_person_role ||
        details_panel->person_role_callback == NULL ||
        details_panel->selected_entity_identifier == NULL)
        return;
    selected = gtk_drop_down_get_selected(details_panel->person_role_dropdown);
    if (selected > PERSON_ROLE_IMPERSONATED_IDENTITY) return;
    details_panel->person_role_callback(
        details_panel->selected_entity_identifier, (PersonRole) selected,
        details_panel->person_role_user_data);
}

/**
 * @brief Définit une valeur avec un texte de remplacement.
 */
static void entity_details_panel_set_field_text(
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
        value != NULL &&
        value[0] != '\0'
            ? value
            : fallback
    );
}

/**
 * @brief Ajoute une ligne de métadonnée dans la grille.
 *
 * @return Label GTK destiné à recevoir la valeur.
 */
static GtkWidget *entity_details_panel_add_field(
    GtkGrid *grid,
    gint row,
    const char *title
)
{
    GtkWidget *title_label =
        NULL;

    GtkWidget *value_label =
        NULL;

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
 * @brief Convertit le statut métier en texte utilisateur.
 */
static const char *entity_details_panel_get_status_text(
    EntityStatus status
)
{
    switch (status)
    {
        case ENTITY_STATUS_ACTIVE:
            return "Active";

        case ENTITY_STATUS_ARCHIVED:
            return "Archivée";

        case ENTITY_STATUS_DELETED:
            return "Supprimée";

        case ENTITY_STATUS_UNKNOWN:
        default:
            return "Inconnu";
    }
}

/**
 * @brief Met à jour l'état du bouton d'ajout d'une relation.
 */
static void entity_details_panel_update_add_relation_button(
    EntityDetailsPanel *details_panel
)
{
    gboolean enabled =
        FALSE;

    if (details_panel == NULL ||
        details_panel->add_relation_button == NULL)
    {
        return;
    }

    enabled =
        details_panel->add_relation_callback != NULL &&
        details_panel->selected_entity_identifier != NULL &&
        g_uuid_string_is_valid(
            details_panel->selected_entity_identifier
        );

    gtk_widget_set_sensitive(
        details_panel->add_relation_button,
        enabled
    );
}

/**
 * @brief Transmet la demande d'ajout d'une relation.
 */
static void entity_details_panel_on_add_relation_clicked(
    GtkButton *button,
    gpointer user_data
)
{
    EntityDetailsPanel *details_panel =
        user_data;

    (void) button;

    if (details_panel == NULL ||
        details_panel->add_relation_callback == NULL ||
        details_panel->selected_entity_identifier == NULL ||
        !g_uuid_string_is_valid(
            details_panel->selected_entity_identifier
        ))
    {
        return;
    }

    details_panel->add_relation_callback(
        details_panel->selected_entity_identifier,
        details_panel->add_relation_user_data
    );
}

/**
 * @brief Ferme le volet depuis le bouton de l'interface.
 */
static void entity_details_panel_on_close_clicked(
    GtkButton *button,
    gpointer user_data
)
{
    EntityDetailsPanel *details_panel =
        user_data;

    (void) button;

    if (details_panel == NULL)
    {
        return;
    }

    entity_details_panel_clear(
        details_panel
    );

    if (details_panel->close_callback != NULL)
    {
        details_panel->close_callback(
            details_panel->close_user_data
        );
    }
}

EntityDetailsPanel *entity_details_panel_new(void)
{
    EntityDetailsPanel *details_panel =
        NULL;

    GtkWidget *content_box =
        NULL;

    GtkWidget *header_box =
        NULL;

    GtkWidget *header_label =
        NULL;

    GtkWidget *separator =
        NULL;

    GtkWidget *scrolled_window =
        NULL;

    GtkWidget *details_box =
        NULL;

    GtkWidget *details_grid =
        NULL;

    GtkStringList *person_role_labels = NULL;

    details_panel =
        g_try_new0(
            EntityDetailsPanel,
            1
        );

    if (details_panel == NULL)
    {
        return NULL;
    }

    details_panel->root_revealer =
        gtk_revealer_new();

    details_panel->frame =
        gtk_frame_new(
            NULL
        );

    details_panel->add_relation_button =
        gtk_button_new_from_icon_name(
            "list-add-symbolic"
        );

    details_panel->close_button =
        gtk_button_new_from_icon_name(
            "window-close-symbolic"
        );

    content_box =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            0
        );

    header_box =
        gtk_box_new(
            GTK_ORIENTATION_HORIZONTAL,
            8
        );

    header_label =
        gtk_label_new(
            "Détails de l’entité"
        );

    separator =
        gtk_separator_new(
            GTK_ORIENTATION_HORIZONTAL
        );

    scrolled_window =
        gtk_scrolled_window_new();

    details_box =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            16
        );

    details_grid =
        gtk_grid_new();

    if (details_panel->root_revealer == NULL ||
        details_panel->frame == NULL ||
        details_panel->add_relation_button == NULL ||
        details_panel->close_button == NULL ||
        content_box == NULL ||
        header_box == NULL ||
        header_label == NULL ||
        separator == NULL ||
        scrolled_window == NULL ||
        details_box == NULL ||
        details_grid == NULL)
    {
        entity_details_panel_free(
            details_panel
        );

        return NULL;
    }

    gtk_widget_set_size_request(
        details_panel->root_revealer,
        360,
        -1
    );

    gtk_widget_set_halign(
        details_panel->root_revealer,
        GTK_ALIGN_END
    );

    gtk_widget_set_valign(
        details_panel->root_revealer,
        GTK_ALIGN_FILL
    );

    gtk_widget_set_vexpand(
        details_panel->root_revealer,
        TRUE
    );

    gtk_revealer_set_transition_type(
        GTK_REVEALER(
            details_panel->root_revealer
        ),
        GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT
    );

    gtk_revealer_set_transition_duration(
        GTK_REVEALER(
            details_panel->root_revealer
        ),
        180
    );

    gtk_revealer_set_reveal_child(
        GTK_REVEALER(
            details_panel->root_revealer
        ),
        FALSE
    );

    gtk_widget_set_margin_top(
        details_panel->frame,
        12
    );

    gtk_widget_set_margin_end(
        details_panel->frame,
        12
    );

    gtk_widget_set_margin_bottom(
        details_panel->frame,
        12
    );

    gtk_widget_add_css_class(
        details_panel->frame,
        "view"
    );

    gtk_widget_set_margin_start(
        header_box,
        16
    );

    gtk_widget_set_margin_end(
        header_box,
        8
    );

    gtk_widget_set_margin_top(
        header_box,
        8
    );

    gtk_widget_set_margin_bottom(
        header_box,
        8
    );

    gtk_label_set_xalign(
        GTK_LABEL(header_label),
        0.0F
    );

    gtk_widget_set_hexpand(
        header_label,
        TRUE
    );

    gtk_widget_set_halign(
        header_label,
        GTK_ALIGN_FILL
    );

    gtk_widget_add_css_class(
        header_label,
        "heading"
    );

    gtk_widget_set_tooltip_text(
        details_panel->add_relation_button,
        "Ajouter une relation depuis cette entité"
    );

    gtk_widget_add_css_class(
        details_panel->add_relation_button,
        "flat"
    );

    gtk_widget_set_sensitive(
        details_panel->add_relation_button,
        FALSE
    );

    gtk_widget_set_tooltip_text(
        details_panel->close_button,
        "Fermer le volet"
    );

    gtk_widget_add_css_class(
        details_panel->close_button,
        "flat"
    );

    gtk_box_append(
        GTK_BOX(header_box),
        header_label
    );

    gtk_box_append(
        GTK_BOX(header_box),
        details_panel->add_relation_button
    );

    gtk_box_append(
        GTK_BOX(header_box),
        details_panel->close_button
    );

    gtk_box_append(
        GTK_BOX(content_box),
        header_box
    );

    gtk_box_append(
        GTK_BOX(content_box),
        separator
    );

    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(scrolled_window),
        GTK_POLICY_NEVER,
        GTK_POLICY_AUTOMATIC
    );

    gtk_widget_set_vexpand(
        scrolled_window,
        TRUE
    );

    gtk_widget_set_margin_start(
        details_box,
        20
    );

    gtk_widget_set_margin_end(
        details_box,
        20
    );

    gtk_widget_set_margin_top(
        details_box,
        20
    );

    gtk_widget_set_margin_bottom(
        details_box,
        20
    );

    details_panel->entity_title_label =
        gtk_label_new(
            NULL
        );

    if (details_panel->entity_title_label == NULL)
    {
        entity_details_panel_free(
            details_panel
        );

        return NULL;
    }

    gtk_label_set_xalign(
        GTK_LABEL(
            details_panel->entity_title_label
        ),
        0.0F
    );

    gtk_label_set_wrap(
        GTK_LABEL(
            details_panel->entity_title_label
        ),
        TRUE
    );

    gtk_label_set_selectable(
        GTK_LABEL(
            details_panel->entity_title_label
        ),
        TRUE
    );

    gtk_widget_add_css_class(
        details_panel->entity_title_label,
        "title-3"
    );

    gtk_box_append(
        GTK_BOX(details_box),
        details_panel->entity_title_label
    );

    gtk_grid_set_column_spacing(
        GTK_GRID(details_grid),
        16
    );

    gtk_grid_set_row_spacing(
        GTK_GRID(details_grid),
        12
    );

    gtk_widget_set_hexpand(
        details_grid,
        TRUE
    );

    details_panel->entity_value_label =
        entity_details_panel_add_field(
            GTK_GRID(details_grid),
            0,
            "Valeur"
        );

    details_panel->entity_type_label =
        entity_details_panel_add_field(
            GTK_GRID(details_grid),
            1,
            "Type"
        );

    details_panel->entity_description_label =
        entity_details_panel_add_field(
            GTK_GRID(details_grid),
            2,
            "Description"
        );

    details_panel->entity_confidence_label =
        entity_details_panel_add_field(
            GTK_GRID(details_grid),
            3,
            "Confiance"
        );

    details_panel->entity_status_label =
        entity_details_panel_add_field(
            GTK_GRID(details_grid),
            4,
            "Statut"
        );

    details_panel->entity_created_at_label =
        entity_details_panel_add_field(
            GTK_GRID(details_grid),
            5,
            "Créée le"
        );

    details_panel->entity_updated_at_label =
        entity_details_panel_add_field(
            GTK_GRID(details_grid),
            6,
            "Modifiée le"
        );

    details_panel->entity_identifier_label =
        entity_details_panel_add_field(
            GTK_GRID(details_grid),
            7,
            "Identifiant"
        );

    details_panel->person_role_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    person_role_labels = gtk_string_list_new(NULL);
    for (guint role = PERSON_ROLE_UNCATEGORIZED;
         role <= PERSON_ROLE_IMPERSONATED_IDENTITY; role++)
        gtk_string_list_append(person_role_labels,
            person_role_get_label((PersonRole) role));
    details_panel->person_role_dropdown = GTK_DROP_DOWN(
        gtk_drop_down_new(G_LIST_MODEL(person_role_labels), NULL));
    person_role_labels = NULL;
    gtk_box_append(GTK_BOX(details_panel->person_role_box),
        gtk_label_new("Catégorie dans l’enquête"));
    gtk_box_append(GTK_BOX(details_panel->person_role_box),
        GTK_WIDGET(details_panel->person_role_dropdown));
    details_panel->person_confidence_spin = GTK_SPIN_BUTTON(
        gtk_spin_button_new_with_range(0.0, 100.0, 1.0));
    details_panel->person_confidence_button = gtk_button_new_with_label(
        "Enregistrer la confiance");
    gtk_box_append(GTK_BOX(details_panel->person_role_box),
        gtk_label_new("Niveau de confiance (%)"));
    gtk_box_append(GTK_BOX(details_panel->person_role_box),
        GTK_WIDGET(details_panel->person_confidence_spin));
    gtk_box_append(GTK_BOX(details_panel->person_role_box),
        details_panel->person_confidence_button);
    details_panel->person_name_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(details_panel->person_name_entry,
        "Nom réel ou nom affiché");
    details_panel->person_name_button = gtk_button_new_with_label(
        "Enregistrer le nom");
    gtk_box_append(GTK_BOX(details_panel->person_role_box),
        gtk_label_new("Nom affiché dans le graphe"));
    gtk_box_append(GTK_BOX(details_panel->person_role_box),
        GTK_WIDGET(details_panel->person_name_entry));
    gtk_box_append(GTK_BOX(details_panel->person_role_box),
        details_panel->person_name_button);
    details_panel->person_evidence_button = gtk_button_new_with_label(
        "Gérer les pièces jointes");
    details_panel->person_evidence_summary_label = gtk_label_new(
        "Pièces jointes : chargement…");
    gtk_label_set_xalign(GTK_LABEL(
        details_panel->person_evidence_summary_label), 0.0F);
    gtk_label_set_wrap(GTK_LABEL(
        details_panel->person_evidence_summary_label), TRUE);
    gtk_label_set_selectable(GTK_LABEL(
        details_panel->person_evidence_summary_label), TRUE);
    gtk_box_append(GTK_BOX(details_panel->person_role_box),
        gtk_label_new("Preuves associées à cette personne"));
    gtk_box_append(GTK_BOX(details_panel->person_role_box),
        details_panel->person_evidence_summary_label);
    gtk_box_append(GTK_BOX(details_panel->person_role_box),
        details_panel->person_evidence_button);
    gtk_box_append(GTK_BOX(details_box), details_panel->person_role_box);
    g_signal_connect(details_panel->person_role_dropdown, "notify::selected",
        G_CALLBACK(entity_details_panel_on_person_role_changed), details_panel);
    g_signal_connect(details_panel->person_confidence_button, "clicked",
        G_CALLBACK(entity_details_panel_on_person_confidence_clicked),
        details_panel);
    g_signal_connect(details_panel->person_name_button, "clicked",
        G_CALLBACK(entity_details_panel_on_person_name_clicked), details_panel);
    g_signal_connect(details_panel->person_evidence_button, "clicked",
        G_CALLBACK(entity_details_panel_on_person_evidence_clicked), details_panel);

    if (details_panel->entity_value_label == NULL ||
        details_panel->entity_type_label == NULL ||
        details_panel->entity_description_label == NULL ||
        details_panel->entity_confidence_label == NULL ||
        details_panel->entity_status_label == NULL ||
        details_panel->entity_created_at_label == NULL ||
        details_panel->entity_updated_at_label == NULL ||
        details_panel->entity_identifier_label == NULL)
    {
        entity_details_panel_free(
            details_panel
        );

        return NULL;
    }

    gtk_box_append(
        GTK_BOX(details_box),
        details_grid
    );

    gtk_scrolled_window_set_child(
        GTK_SCROLLED_WINDOW(scrolled_window),
        details_box
    );

    gtk_box_append(
        GTK_BOX(content_box),
        scrolled_window
    );

    gtk_frame_set_child(
        GTK_FRAME(details_panel->frame),
        content_box
    );

    gtk_revealer_set_child(
        GTK_REVEALER(
            details_panel->root_revealer
        ),
        details_panel->frame
    );

    g_signal_connect(
        details_panel->add_relation_button,
        "clicked",
        G_CALLBACK(
            entity_details_panel_on_add_relation_clicked
        ),
        details_panel
    );

    g_signal_connect(
        details_panel->close_button,
        "clicked",
        G_CALLBACK(
            entity_details_panel_on_close_clicked
        ),
        details_panel
    );

    entity_details_panel_clear(
        details_panel
    );

    return details_panel;
}

GtkWidget *entity_details_panel_get_widget(
    const EntityDetailsPanel *details_panel
)
{
    if (details_panel == NULL)
    {
        return NULL;
    }

    return details_panel->root_revealer;
}

void entity_details_panel_set_entity(
    EntityDetailsPanel *details_panel,
    const EntityRecord *entity_record
)
{
    const char *title =
        NULL;

    char *confidence_text =
        NULL;

    if (details_panel == NULL)
    {
        return;
    }

    g_clear_pointer(
        &details_panel->selected_entity_identifier,
        g_free
    );

    if (entity_record == NULL)
    {
        entity_details_panel_clear(
            details_panel
        );

        return;
    }

    details_panel->selected_entity_identifier =
        g_strdup(
            entity_record_get_identifier(
                entity_record
            )
        );

    gboolean is_person = g_strcmp0(entity_record_get_type_identifier(
        entity_record), "person") == 0;
    gtk_widget_set_visible(details_panel->person_role_box, is_person);
    details_panel->updating_person_role = TRUE;
    gtk_drop_down_set_selected(details_panel->person_role_dropdown,
        (guint) entity_record_get_person_role(entity_record));
    gtk_spin_button_set_value(details_panel->person_confidence_spin,
        entity_record_get_confidence(entity_record));
    gtk_editable_set_text(GTK_EDITABLE(details_panel->person_name_entry),
        entity_record_get_label(entity_record) != NULL
            ? entity_record_get_label(entity_record) : "");
    details_panel->updating_person_role = FALSE;

    entity_details_panel_update_add_relation_button(
        details_panel
    );

    title =
        entity_record_get_label(
            entity_record
        );

    if (title == NULL ||
        title[0] == '\0')
    {
        title =
            entity_record_get_value(
                entity_record
            );
    }

    if (title == NULL ||
        title[0] == '\0')
    {
        title =
            entity_record_get_identifier(
                entity_record
            );
    }

    confidence_text =
        g_strdup_printf(
            "%d %%",
            entity_record_get_confidence(
                entity_record
            )
        );

    entity_details_panel_set_field_text(
        details_panel->entity_title_label,
        title,
        "(entité sans libellé)"
    );

    entity_details_panel_set_field_text(
        details_panel->entity_value_label,
        entity_record_get_value(
            entity_record
        ),
        "(valeur inconnue)"
    );

    entity_details_panel_set_field_text(
        details_panel->entity_type_label,
        entity_record_get_type_identifier(
            entity_record
        ),
        "(type inconnu)"
    );

    entity_details_panel_set_field_text(
        details_panel->entity_description_label,
        entity_record_get_description(
            entity_record
        ),
        "Non renseignée"
    );

    entity_details_panel_set_field_text(
        details_panel->entity_confidence_label,
        confidence_text,
        "Inconnue"
    );

    entity_details_panel_set_field_text(
        details_panel->entity_status_label,
        entity_details_panel_get_status_text(
            entity_record_get_status(
                entity_record
            )
        ),
        "Inconnu"
    );

    entity_details_panel_set_field_text(
        details_panel->entity_created_at_label,
        entity_record_get_created_at(
            entity_record
        ),
        "(date inconnue)"
    );

    entity_details_panel_set_field_text(
        details_panel->entity_updated_at_label,
        entity_record_get_updated_at(
            entity_record
        ),
        "(date inconnue)"
    );

    entity_details_panel_set_field_text(
        details_panel->entity_identifier_label,
        entity_record_get_identifier(
            entity_record
        ),
        "(identifiant inconnu)"
    );

    gtk_revealer_set_reveal_child(
        GTK_REVEALER(
            details_panel->root_revealer
        ),
        TRUE
    );
    gtk_widget_set_can_target(details_panel->root_revealer, TRUE);

    g_free(
        confidence_text
    );
}

void entity_details_panel_clear(
    EntityDetailsPanel *details_panel
)
{
    if (details_panel == NULL)
    {
        return;
    }

    g_clear_pointer(
        &details_panel->selected_entity_identifier,
        g_free
    );

    entity_details_panel_update_add_relation_button(
        details_panel
    );

    entity_details_panel_set_field_text(
        details_panel->entity_title_label,
        NULL,
        ""
    );

    entity_details_panel_set_field_text(
        details_panel->entity_value_label,
        NULL,
        ""
    );

    entity_details_panel_set_field_text(
        details_panel->entity_type_label,
        NULL,
        ""
    );

    entity_details_panel_set_field_text(
        details_panel->entity_description_label,
        NULL,
        ""
    );

    entity_details_panel_set_field_text(
        details_panel->entity_confidence_label,
        NULL,
        ""
    );

    entity_details_panel_set_field_text(
        details_panel->entity_status_label,
        NULL,
        ""
    );

    entity_details_panel_set_field_text(
        details_panel->entity_created_at_label,
        NULL,
        ""
    );

    entity_details_panel_set_field_text(
        details_panel->entity_updated_at_label,
        NULL,
        ""
    );

    entity_details_panel_set_field_text(
        details_panel->entity_identifier_label,
        NULL,
        ""
    );

    if (details_panel->root_revealer != NULL)
    {
        gtk_revealer_set_reveal_child(
            GTK_REVEALER(
                details_panel->root_revealer
            ),
            FALSE
        );
        /* Le revealer fermé ne doit pas bloquer le canvas sous l'overlay. */
        gtk_widget_set_can_target(details_panel->root_revealer, FALSE);
    }
    if (details_panel->person_role_box != NULL)
        gtk_widget_set_visible(details_panel->person_role_box, FALSE);
}

void entity_details_panel_set_close_callback(
    EntityDetailsPanel *details_panel,
    EntityDetailsPanelCloseCallback callback,
    gpointer user_data
)
{
    if (details_panel == NULL)
    {
        return;
    }

    details_panel->close_callback =
        callback;

    details_panel->close_user_data =
        user_data;
}

void entity_details_panel_set_add_relation_callback(
    EntityDetailsPanel *details_panel,
    EntityDetailsPanelAddRelationCallback callback,
    gpointer user_data
)
{
    if (details_panel == NULL)
    {
        return;
    }

    details_panel->add_relation_callback =
        callback;

    details_panel->add_relation_user_data =
        user_data;

    entity_details_panel_update_add_relation_button(
        details_panel
    );
}

void entity_details_panel_set_person_role_callback(
    EntityDetailsPanel *details_panel,
    EntityDetailsPanelPersonRoleCallback callback, gpointer user_data)
{
    if (details_panel == NULL) return;
    details_panel->person_role_callback = callback;
    details_panel->person_role_user_data = user_data;
}

void entity_details_panel_set_person_confidence_callback(
    EntityDetailsPanel *details_panel,
    EntityDetailsPanelPersonConfidenceCallback callback, gpointer user_data)
{
    if (details_panel == NULL) return;
    details_panel->person_confidence_callback = callback;
    details_panel->person_confidence_user_data = user_data;
}

void entity_details_panel_set_person_name_callback(
    EntityDetailsPanel *details_panel,
    EntityDetailsPanelPersonNameCallback callback, gpointer user_data)
{
    if (details_panel == NULL) return;
    details_panel->person_name_callback = callback;
    details_panel->person_name_user_data = user_data;
}
void entity_details_panel_set_person_evidence_callback(
    EntityDetailsPanel *details_panel,
    EntityDetailsPanelPersonEvidenceCallback callback, gpointer user_data)
{
    if (details_panel == NULL) return;
    details_panel->person_evidence_callback = callback;
    details_panel->person_evidence_user_data = user_data;
}
void entity_details_panel_set_person_evidences(EntityDetailsPanel *panel,
    const GPtrArray *records)
{
    GString *text = NULL;
    if (panel == NULL || panel->person_evidence_summary_label == NULL) return;
    text = g_string_new(NULL);
    if (records == NULL || records->len == 0)
        g_string_append(text, "Aucune pièce jointe");
    else
        for (guint index = 0; index < records->len; index++)
        {
            const EvidenceRecord *record = g_ptr_array_index(records, index);
            if (record == NULL) continue;
            if (text->len > 0) g_string_append_c(text, '\n');
            g_string_append_printf(text, "• %s — %s",
                evidence_record_get_original_name(record),
                evidence_record_get_type_identifier(record));
        }
    gtk_label_set_text(GTK_LABEL(panel->person_evidence_summary_label),
        text->str);
    g_string_free(text, TRUE);
}

gboolean entity_details_panel_is_open(
    const EntityDetailsPanel *details_panel
)
{
    if (details_panel == NULL ||
        details_panel->root_revealer == NULL)
    {
        return FALSE;
    }

    return gtk_revealer_get_reveal_child(
        GTK_REVEALER(
            details_panel->root_revealer
        )
    );
}

void entity_details_panel_free(
    EntityDetailsPanel *details_panel
)
{
    if (details_panel == NULL)
    {
        return;
    }

    if (details_panel->add_relation_button != NULL)
    {
        g_signal_handlers_disconnect_by_data(
            details_panel->add_relation_button,
            details_panel
        );
    }

    if (details_panel->close_button != NULL)
    {
        g_signal_handlers_disconnect_by_data(
            details_panel->close_button,
            details_panel
        );
    }

    details_panel->close_callback =
        NULL;

    details_panel->close_user_data =
        NULL;

    details_panel->add_relation_callback =
        NULL;

    details_panel->add_relation_user_data =
        NULL;

    g_clear_pointer(
        &details_panel->selected_entity_identifier,
        g_free
    );

    details_panel->root_revealer =
        NULL;

    details_panel->frame =
        NULL;

    details_panel->add_relation_button =
        NULL;

    details_panel->close_button =
        NULL;

    details_panel->entity_title_label =
        NULL;

    details_panel->entity_value_label =
        NULL;

    details_panel->entity_type_label =
        NULL;

    details_panel->entity_description_label =
        NULL;

    details_panel->entity_confidence_label =
        NULL;

    details_panel->entity_status_label =
        NULL;

    details_panel->entity_created_at_label =
        NULL;

    details_panel->entity_updated_at_label =
        NULL;

    details_panel->entity_identifier_label =
        NULL;

    g_free(
        details_panel
    );
}
