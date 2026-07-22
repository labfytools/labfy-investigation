/******************************************************************************
 * @file create_relation_dialog.c
 * @brief Dialogue GTK de préparation d'une relation entre deux entités.
 ******************************************************************************/

#include "views/create_relation_dialog.h"

#include "models/entity_record.h"

#include <glib.h>

/**
 * @struct CreateRelationDialogResult
 * @brief Valeurs validées par le dialogue.
 */
struct CreateRelationDialogResult
{
    char *source_entity_identifier;
    char *target_entity_identifier;
    char *relation_type;
    char *label;
    char *justification;

    gint confidence;
};

/**
 * @brief État privé conservé pendant l'affichage du dialogue.
 */
typedef struct
{
    GtkWindow *window;

    GtkDropDown *target_drop_down;
    GtkEntry *relation_type_entry;
    GtkEntry *label_entry;
    GtkTextView *justification_text_view;
    GtkSpinButton *confidence_spin_button;
    GtkLabel *error_label;

    char *source_entity_identifier;
    GPtrArray *target_identifiers;

    CreateRelationDialogCallback callback;
    gpointer callback_data;

    gboolean completed;
} CreateRelationDialogState;

/**
 * @brief Retourne une copie nettoyée ou NULL pour une valeur vide.
 */
static char *create_relation_dialog_duplicate_optional_text(
    const char *value
)
{
    char *copy =
        NULL;

    if (value == NULL)
    {
        return NULL;
    }

    copy =
        g_strdup(
            value
        );

    if (copy == NULL)
    {
        return NULL;
    }

    g_strstrip(
        copy
    );

    if (copy[0] == '\0')
    {
        g_free(
            copy
        );

        return NULL;
    }

    return copy;
}

/**
 * @brief Retourne une copie nettoyée d'un texte obligatoire.
 */
static char *create_relation_dialog_duplicate_required_text(
    const char *value
)
{
    char *copy =
        create_relation_dialog_duplicate_optional_text(
            value
        );

    return copy;
}

/**
 * @brief Retourne le texte principal d'une entité.
 */
static const char *create_relation_dialog_get_entity_title(
    const EntityRecord *entity_record
)
{
    const char *value =
        NULL;

    if (entity_record == NULL)
    {
        return NULL;
    }

    value =
        entity_record_get_label(
            entity_record
        );

    if (value != NULL &&
        value[0] != '\0')
    {
        return value;
    }

    value =
        entity_record_get_value(
            entity_record
        );

    if (value != NULL &&
        value[0] != '\0')
    {
        return value;
    }

    return entity_record_get_identifier(
        entity_record
    );
}

/**
 * @brief Construit le libellé d'une entité dans une liste.
 */
static char *create_relation_dialog_duplicate_entity_display(
    const EntityRecord *entity_record
)
{
    const char *title =
        NULL;

    const char *type_identifier =
        NULL;

    if (entity_record == NULL)
    {
        return NULL;
    }

    title =
        create_relation_dialog_get_entity_title(
            entity_record
        );

    type_identifier =
        entity_record_get_type_identifier(
            entity_record
        );

    if (title == NULL ||
        title[0] == '\0')
    {
        return NULL;
    }

    if (type_identifier == NULL ||
        type_identifier[0] == '\0')
    {
        return g_strdup(
            title
        );
    }

    return g_strdup_printf(
        "%s — %s",
        title,
        type_identifier
    );
}

/**
 * @brief Retourne le texte complet de la justification.
 */
static char *create_relation_dialog_get_justification_text(
    CreateRelationDialogState *state
)
{
    GtkTextBuffer *buffer =
        NULL;

    GtkTextIter start;
    GtkTextIter end;

    if (state == NULL ||
        state->justification_text_view == NULL)
    {
        return NULL;
    }

    buffer =
        gtk_text_view_get_buffer(
            state->justification_text_view
        );

    gtk_text_buffer_get_bounds(
        buffer,
        &start,
        &end
    );

    return gtk_text_buffer_get_text(
        buffer,
        &start,
        &end,
        FALSE
    );
}

/**
 * @brief Affiche une erreur de validation dans le dialogue.
 */
static void create_relation_dialog_show_error(
    CreateRelationDialogState *state,
    const char *message
)
{
    if (state == NULL ||
        state->error_label == NULL)
    {
        return;
    }

    gtk_label_set_text(
        state->error_label,
        message != NULL &&
        message[0] != '\0'
            ? message
            : "Les informations de la relation sont invalides."
    );

    gtk_widget_set_visible(
        GTK_WIDGET(
            state->error_label
        ),
        TRUE
    );
}

/**
 * @brief Libère l'état privé du dialogue.
 */
static void create_relation_dialog_state_free(
    gpointer user_data
)
{
    CreateRelationDialogState *state =
        user_data;

    if (state == NULL)
    {
        return;
    }

    g_clear_pointer(
        &state->target_identifiers,
        g_ptr_array_unref
    );

    g_free(
        state->source_entity_identifier
    );

    g_free(
        state
    );
}

/**
 * @brief Termine le dialogue avec une annulation.
 */
static void create_relation_dialog_cancel(
    CreateRelationDialogState *state
)
{
    if (state == NULL ||
        state->completed)
    {
        return;
    }

    state->completed =
        TRUE;

    if (state->callback != NULL)
    {
        state->callback(
            NULL,
            state->callback_data
        );
    }
}

/**
 * @brief Traite la fermeture native de la fenêtre.
 */
static gboolean create_relation_dialog_on_close_requested(
    GtkWindow *window,
    gpointer user_data
)
{
    CreateRelationDialogState *state =
        user_data;

    (void) window;

    create_relation_dialog_cancel(
        state
    );

    return FALSE;
}

/**
 * @brief Traite le bouton Annuler.
 */
static void create_relation_dialog_on_cancel_clicked(
    GtkButton *button,
    gpointer user_data
)
{
    CreateRelationDialogState *state =
        user_data;

    (void) button;

    if (state == NULL)
    {
        return;
    }

    create_relation_dialog_cancel(
        state
    );

    gtk_window_close(
        state->window
    );
}

/**
 * @brief Traite la validation du formulaire.
 */
static void create_relation_dialog_on_create_clicked(
    GtkButton *button,
    gpointer user_data
)
{
    CreateRelationDialogState *state =
        user_data;

    CreateRelationDialogResult *result =
        NULL;

    guint selected_index =
        GTK_INVALID_LIST_POSITION;

    const char *target_identifier =
        NULL;

    const char *relation_type =
        NULL;

    const char *label =
        NULL;

    char *justification_text =
        NULL;

    gint confidence =
        0;

    GError *validation_error =
        NULL;

    (void) button;

    if (state == NULL ||
        state->completed)
    {
        return;
    }

    selected_index =
        gtk_drop_down_get_selected(
            state->target_drop_down
        );

    if (selected_index ==
            GTK_INVALID_LIST_POSITION ||
        selected_index >=
            state->target_identifiers->len)
    {
        create_relation_dialog_show_error(
            state,
            "Sélectionnez une entité cible."
        );

        return;
    }

    target_identifier =
        g_ptr_array_index(
            state->target_identifiers,
            selected_index
        );

    relation_type =
        gtk_editable_get_text(
            GTK_EDITABLE(
                state->relation_type_entry
            )
        );

    label =
        gtk_editable_get_text(
            GTK_EDITABLE(
                state->label_entry
            )
        );

    confidence =
        gtk_spin_button_get_value_as_int(
            state->confidence_spin_button
        );

    justification_text =
        create_relation_dialog_get_justification_text(
            state
        );

    if (!create_relation_dialog_validate_values(
            state->source_entity_identifier,
            target_identifier,
            relation_type,
            confidence,
            &validation_error
        ))
    {
        create_relation_dialog_show_error(
            state,
            validation_error != NULL
                ? validation_error->message
                : "Les informations de la relation sont invalides."
        );

        g_clear_error(
            &validation_error
        );

        g_free(
            justification_text
        );

        return;
    }

    result =
        g_try_new0(
            CreateRelationDialogResult,
            1
        );

    if (result == NULL)
    {
        create_relation_dialog_show_error(
            state,
            "Impossible d'allouer le résultat du dialogue."
        );

        g_free(
            justification_text
        );

        return;
    }

    result->source_entity_identifier =
        g_strdup(
            state->source_entity_identifier
        );

    result->target_entity_identifier =
        g_strdup(
            target_identifier
        );

    result->relation_type =
        create_relation_dialog_duplicate_required_text(
            relation_type
        );

    result->label =
        create_relation_dialog_duplicate_optional_text(
            label
        );

    result->justification =
        create_relation_dialog_duplicate_optional_text(
            justification_text
        );

    result->confidence =
        confidence;

    g_free(
        justification_text
    );

    if (result->source_entity_identifier == NULL ||
        result->target_entity_identifier == NULL ||
        result->relation_type == NULL)
    {
        create_relation_dialog_result_free(
            result
        );

        create_relation_dialog_show_error(
            state,
            "Impossible de conserver les informations de la relation."
        );

        return;
    }

    state->completed =
        TRUE;

    if (state->callback != NULL)
    {
        state->callback(
            result,
            state->callback_data
        );
    }
    else
    {
        create_relation_dialog_result_free(
            result
        );
    }

    gtk_window_close(
        state->window
    );
}

GQuark create_relation_dialog_error_quark(void)
{
    return g_quark_from_static_string(
        "create-relation-dialog-error-quark"
    );
}

gboolean create_relation_dialog_validate_values(
    const char *source_entity_identifier,
    const char *target_entity_identifier,
    const char *relation_type,
    gint confidence,
    GError **error
)
{
    char *normalized_relation_type =
        NULL;

    gboolean valid =
        FALSE;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (source_entity_identifier == NULL ||
        !g_uuid_string_is_valid(
            source_entity_identifier
        ))
    {
        g_set_error_literal(
            error,
            CREATE_RELATION_DIALOG_ERROR,
            CREATE_RELATION_DIALOG_ERROR_INVALID_DATA,
            "L'identifiant de l'entité source est invalide."
        );

        return FALSE;
    }

    if (target_entity_identifier == NULL ||
        !g_uuid_string_is_valid(
            target_entity_identifier
        ))
    {
        g_set_error_literal(
            error,
            CREATE_RELATION_DIALOG_ERROR,
            CREATE_RELATION_DIALOG_ERROR_INVALID_DATA,
            "L'identifiant de l'entité cible est invalide."
        );

        return FALSE;
    }

    if (g_strcmp0(
            source_entity_identifier,
            target_entity_identifier
        ) == 0)
    {
        g_set_error_literal(
            error,
            CREATE_RELATION_DIALOG_ERROR,
            CREATE_RELATION_DIALOG_ERROR_INVALID_DATA,
            "Une entité ne peut pas être reliée à elle-même."
        );

        return FALSE;
    }

    normalized_relation_type =
        create_relation_dialog_duplicate_required_text(
            relation_type
        );

    if (normalized_relation_type == NULL)
    {
        g_set_error_literal(
            error,
            CREATE_RELATION_DIALOG_ERROR,
            CREATE_RELATION_DIALOG_ERROR_INVALID_DATA,
            "Le type de relation est obligatoire."
        );

        return FALSE;
    }

    if (confidence < 0 ||
        confidence > 100)
    {
        g_set_error_literal(
            error,
            CREATE_RELATION_DIALOG_ERROR,
            CREATE_RELATION_DIALOG_ERROR_INVALID_DATA,
            "Le niveau de confiance doit être compris entre 0 et 100."
        );

        goto cleanup;
    }

    valid =
        TRUE;

cleanup:

    g_free(
        normalized_relation_type
    );

    return valid;
}

gboolean create_relation_dialog_present(
    GtkWindow *parent,
    const char *source_entity_identifier,
    const GPtrArray *entities,
    CreateRelationDialogCallback callback,
    gpointer user_data,
    GError **error
)
{
    CreateRelationDialogState *state =
        NULL;

    GtkStringList *target_labels =
        NULL;

    GtkWidget *root_box =
        NULL;

    GtkWidget *title_label =
        NULL;

    GtkWidget *source_title_label =
        NULL;

    GtkWidget *source_value_label =
        NULL;

    GtkWidget *grid =
        NULL;

    GtkWidget *target_label =
        NULL;

    GtkWidget *relation_type_label =
        NULL;

    GtkWidget *label_label =
        NULL;

    GtkWidget *confidence_label =
        NULL;

    GtkWidget *justification_label =
        NULL;

    GtkWidget *justification_scrolled_window =
        NULL;

    GtkWidget *button_box =
        NULL;

    GtkWidget *cancel_button =
        NULL;

    GtkWidget *create_button =
        NULL;

    const char *source_display =
        source_entity_identifier;

    guint entity_index =
        0;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (parent == NULL ||
        !GTK_IS_WINDOW(
            parent
        ) ||
        source_entity_identifier == NULL ||
        !g_uuid_string_is_valid(
            source_entity_identifier
        ) ||
        entities == NULL ||
        callback == NULL)
    {
        g_set_error_literal(
            error,
            CREATE_RELATION_DIALOG_ERROR,
            CREATE_RELATION_DIALOG_ERROR_INVALID_ARGUMENT,
            "Les arguments du dialogue de relation sont invalides."
        );

        return FALSE;
    }

    state =
        g_try_new0(
            CreateRelationDialogState,
            1
        );

    if (state == NULL)
    {
        g_set_error_literal(
            error,
            CREATE_RELATION_DIALOG_ERROR,
            CREATE_RELATION_DIALOG_ERROR_MEMORY,
            "Impossible d'allouer l'état du dialogue."
        );

        return FALSE;
    }

    state->source_entity_identifier =
        g_strdup(
            source_entity_identifier
        );

    state->target_identifiers =
        g_ptr_array_new_with_free_func(
            g_free
        );

    state->callback =
        callback;

    state->callback_data =
        user_data;

    target_labels =
        gtk_string_list_new(
            NULL
        );

    if (state->source_entity_identifier == NULL ||
        state->target_identifiers == NULL ||
        target_labels == NULL)
    {
        g_clear_object(
            &target_labels
        );

        create_relation_dialog_state_free(
            state
        );

        g_set_error_literal(
            error,
            CREATE_RELATION_DIALOG_ERROR,
            CREATE_RELATION_DIALOG_ERROR_MEMORY,
            "Impossible de préparer les listes du dialogue."
        );

        return FALSE;
    }

    for (entity_index = 0;
         entity_index < entities->len;
         entity_index++)
    {
        const EntityRecord *entity_record =
            g_ptr_array_index(
                entities,
                entity_index
            );

        const char *entity_identifier =
            NULL;

        char *display_text =
            NULL;

        char *identifier_copy =
            NULL;

        if (entity_record == NULL)
        {
            continue;
        }

        entity_identifier =
            entity_record_get_identifier(
                entity_record
            );

        if (entity_identifier == NULL ||
            !g_uuid_string_is_valid(
                entity_identifier
            ))
        {
            continue;
        }

        if (g_strcmp0(
                entity_identifier,
                source_entity_identifier
            ) == 0)
        {
            const char *candidate_source_display =
                create_relation_dialog_get_entity_title(
                    entity_record
                );

            if (candidate_source_display != NULL &&
                candidate_source_display[0] != '\0')
            {
                source_display =
                    candidate_source_display;
            }

            continue;
        }

        display_text =
            create_relation_dialog_duplicate_entity_display(
                entity_record
            );

        identifier_copy =
            g_strdup(
                entity_identifier
            );

        if (display_text == NULL ||
            identifier_copy == NULL)
        {
            g_free(
                identifier_copy
            );

            g_free(
                display_text
            );

            g_object_unref(
                target_labels
            );

            create_relation_dialog_state_free(
                state
            );

            g_set_error_literal(
                error,
                CREATE_RELATION_DIALOG_ERROR,
                CREATE_RELATION_DIALOG_ERROR_MEMORY,
                "Impossible de copier une entité cible."
            );

            return FALSE;
        }

        gtk_string_list_append(
            target_labels,
            display_text
        );

        g_ptr_array_add(
            state->target_identifiers,
            identifier_copy
        );

        g_free(
            display_text
        );
    }

    if (state->target_identifiers->len == 0)
    {
        g_object_unref(
            target_labels
        );

        create_relation_dialog_state_free(
            state
        );

        g_set_error_literal(
            error,
            CREATE_RELATION_DIALOG_ERROR,
            CREATE_RELATION_DIALOG_ERROR_EMPTY_TARGETS,
            "Aucune autre entité ne peut être utilisée comme cible."
        );

        return FALSE;
    }

    state->window =
        GTK_WINDOW(
            gtk_window_new()
        );

    gtk_window_set_title(
        state->window,
        "Ajouter une relation"
    );

    gtk_window_set_transient_for(
        state->window,
        parent
    );

    gtk_window_set_modal(
        state->window,
        TRUE
    );

    gtk_window_set_destroy_with_parent(
        state->window,
        TRUE
    );

    gtk_window_set_default_size(
        state->window,
        640,
        560
    );

    root_box =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            12
        );

    gtk_widget_set_margin_top(
        root_box,
        18
    );

    gtk_widget_set_margin_bottom(
        root_box,
        18
    );

    gtk_widget_set_margin_start(
        root_box,
        18
    );

    gtk_widget_set_margin_end(
        root_box,
        18
    );

    title_label =
        gtk_label_new(
            NULL
        );

    gtk_label_set_markup(
        GTK_LABEL(
            title_label
        ),
        "<b>Créer une relation entre deux entités</b>"
    );

    gtk_label_set_xalign(
        GTK_LABEL(
            title_label
        ),
        0.0F
    );

    source_title_label =
        gtk_label_new(
            "Entité source"
        );

    gtk_label_set_xalign(
        GTK_LABEL(
            source_title_label
        ),
        0.0F
    );

    source_value_label =
        gtk_label_new(
            source_display
        );

    gtk_label_set_xalign(
        GTK_LABEL(
            source_value_label
        ),
        0.0F
    );

    gtk_label_set_wrap(
        GTK_LABEL(
            source_value_label
        ),
        TRUE
    );

    gtk_label_set_selectable(
        GTK_LABEL(
            source_value_label
        ),
        TRUE
    );

    grid =
        gtk_grid_new();

    gtk_grid_set_row_spacing(
        GTK_GRID(
            grid
        ),
        10
    );

    gtk_grid_set_column_spacing(
        GTK_GRID(
            grid
        ),
        12
    );

    gtk_widget_set_vexpand(
        grid,
        TRUE
    );

    target_label =
        gtk_label_new(
            "Entité cible"
        );

    relation_type_label =
        gtk_label_new(
            "Type de relation"
        );

    label_label =
        gtk_label_new(
            "Libellé"
        );

    confidence_label =
        gtk_label_new(
            "Confiance"
        );

    justification_label =
        gtk_label_new(
            "Justification"
        );

    gtk_label_set_xalign(
        GTK_LABEL(
            target_label
        ),
        0.0F
    );

    gtk_label_set_xalign(
        GTK_LABEL(
            relation_type_label
        ),
        0.0F
    );

    gtk_label_set_xalign(
        GTK_LABEL(
            label_label
        ),
        0.0F
    );

    gtk_label_set_xalign(
        GTK_LABEL(
            confidence_label
        ),
        0.0F
    );

    gtk_label_set_xalign(
        GTK_LABEL(
            justification_label
        ),
        0.0F
    );

    state->target_drop_down =
        GTK_DROP_DOWN(
            gtk_drop_down_new(
                G_LIST_MODEL(
                    target_labels
                ),
                NULL
            )
        );

    /*
     * Le constructeur prend possession de la référence du modèle.
     * Le dialogue ne doit donc pas appeler g_object_unref() ici.
     */
    target_labels =
        NULL;

    gtk_drop_down_set_selected(
        state->target_drop_down,
        0
    );

    gtk_widget_set_hexpand(
        GTK_WIDGET(
            state->target_drop_down
        ),
        TRUE
    );

    state->relation_type_entry =
        GTK_ENTRY(
            gtk_entry_new()
        );

    gtk_entry_set_placeholder_text(
        state->relation_type_entry,
        "Ex. : utilise, contrôle, appartient à"
    );

    state->label_entry =
        GTK_ENTRY(
            gtk_entry_new()
        );

    gtk_entry_set_placeholder_text(
        state->label_entry,
        "Libellé facultatif affiché dans le graphe"
    );

    state->confidence_spin_button =
        GTK_SPIN_BUTTON(
            gtk_spin_button_new_with_range(
                0.0,
                100.0,
                1.0
            )
        );

    gtk_spin_button_set_value(
        state->confidence_spin_button,
        80.0
    );

    state->justification_text_view =
        GTK_TEXT_VIEW(
            gtk_text_view_new()
        );

    gtk_text_view_set_wrap_mode(
        state->justification_text_view,
        GTK_WRAP_WORD_CHAR
    );

    justification_scrolled_window =
        gtk_scrolled_window_new();

    gtk_scrolled_window_set_child(
        GTK_SCROLLED_WINDOW(
            justification_scrolled_window
        ),
        GTK_WIDGET(
            state->justification_text_view
        )
    );

    gtk_widget_set_size_request(
        justification_scrolled_window,
        -1,
        140
    );

    gtk_grid_attach(
        GTK_GRID(
            grid
        ),
        target_label,
        0,
        0,
        1,
        1
    );

    gtk_grid_attach(
        GTK_GRID(
            grid
        ),
        GTK_WIDGET(
            state->target_drop_down
        ),
        1,
        0,
        1,
        1
    );

    gtk_grid_attach(
        GTK_GRID(
            grid
        ),
        relation_type_label,
        0,
        1,
        1,
        1
    );

    gtk_grid_attach(
        GTK_GRID(
            grid
        ),
        GTK_WIDGET(
            state->relation_type_entry
        ),
        1,
        1,
        1,
        1
    );

    gtk_grid_attach(
        GTK_GRID(
            grid
        ),
        label_label,
        0,
        2,
        1,
        1
    );

    gtk_grid_attach(
        GTK_GRID(
            grid
        ),
        GTK_WIDGET(
            state->label_entry
        ),
        1,
        2,
        1,
        1
    );

    gtk_grid_attach(
        GTK_GRID(
            grid
        ),
        confidence_label,
        0,
        3,
        1,
        1
    );

    gtk_grid_attach(
        GTK_GRID(
            grid
        ),
        GTK_WIDGET(
            state->confidence_spin_button
        ),
        1,
        3,
        1,
        1
    );

    gtk_grid_attach(
        GTK_GRID(
            grid
        ),
        justification_label,
        0,
        4,
        1,
        1
    );

    gtk_grid_attach(
        GTK_GRID(
            grid
        ),
        justification_scrolled_window,
        1,
        4,
        1,
        1
    );

    state->error_label =
        GTK_LABEL(
            gtk_label_new(
                NULL
            )
        );

    gtk_label_set_xalign(
        state->error_label,
        0.0F
    );

    gtk_label_set_wrap(
        state->error_label,
        TRUE
    );

    gtk_widget_add_css_class(
        GTK_WIDGET(
            state->error_label
        ),
        "error"
    );

    gtk_widget_set_visible(
        GTK_WIDGET(
            state->error_label
        ),
        FALSE
    );

    button_box =
        gtk_box_new(
            GTK_ORIENTATION_HORIZONTAL,
            8
        );

    gtk_widget_set_halign(
        button_box,
        GTK_ALIGN_END
    );

    cancel_button =
        gtk_button_new_with_label(
            "Annuler"
        );

    create_button =
        gtk_button_new_with_label(
            "Ajouter"
        );

    gtk_widget_add_css_class(
        create_button,
        "suggested-action"
    );

    gtk_box_append(
        GTK_BOX(
            button_box
        ),
        cancel_button
    );

    gtk_box_append(
        GTK_BOX(
            button_box
        ),
        create_button
    );

    gtk_box_append(
        GTK_BOX(
            root_box
        ),
        title_label
    );

    gtk_box_append(
        GTK_BOX(
            root_box
        ),
        source_title_label
    );

    gtk_box_append(
        GTK_BOX(
            root_box
        ),
        source_value_label
    );

    gtk_box_append(
        GTK_BOX(
            root_box
        ),
        grid
    );

    gtk_box_append(
        GTK_BOX(
            root_box
        ),
        GTK_WIDGET(
            state->error_label
        )
    );

    gtk_box_append(
        GTK_BOX(
            root_box
        ),
        button_box
    );

    gtk_window_set_child(
        state->window,
        root_box
    );

    g_object_set_data_full(
        G_OBJECT(
            state->window
        ),
        "labfy-create-relation-dialog-state",
        state,
        create_relation_dialog_state_free
    );

    g_signal_connect(
        state->window,
        "close-request",
        G_CALLBACK(
            create_relation_dialog_on_close_requested
        ),
        state
    );

    g_signal_connect(
        cancel_button,
        "clicked",
        G_CALLBACK(
            create_relation_dialog_on_cancel_clicked
        ),
        state
    );

    g_signal_connect(
        create_button,
        "clicked",
        G_CALLBACK(
            create_relation_dialog_on_create_clicked
        ),
        state
    );

    gtk_window_set_default_widget(
        state->window,
        create_button
    );

    gtk_widget_grab_focus(
        GTK_WIDGET(
            state->relation_type_entry
        )
    );

    gtk_window_present(
        state->window
    );

    return TRUE;
}

void create_relation_dialog_result_free(
    CreateRelationDialogResult *result
)
{
    if (result == NULL)
    {
        return;
    }

    g_free(
        result->justification
    );

    g_free(
        result->label
    );

    g_free(
        result->relation_type
    );

    g_free(
        result->target_entity_identifier
    );

    g_free(
        result->source_entity_identifier
    );

    g_free(
        result
    );
}

const char *create_relation_dialog_result_get_source_identifier(
    const CreateRelationDialogResult *result
)
{
    return result != NULL
        ? result->source_entity_identifier
        : NULL;
}

const char *create_relation_dialog_result_get_target_identifier(
    const CreateRelationDialogResult *result
)
{
    return result != NULL
        ? result->target_entity_identifier
        : NULL;
}

const char *create_relation_dialog_result_get_relation_type(
    const CreateRelationDialogResult *result
)
{
    return result != NULL
        ? result->relation_type
        : NULL;
}

const char *create_relation_dialog_result_get_label(
    const CreateRelationDialogResult *result
)
{
    return result != NULL
        ? result->label
        : NULL;
}

const char *create_relation_dialog_result_get_justification(
    const CreateRelationDialogResult *result
)
{
    return result != NULL
        ? result->justification
        : NULL;
}

gint create_relation_dialog_result_get_confidence(
    const CreateRelationDialogResult *result
)
{
    return result != NULL
        ? result->confidence
        : 0;
}
