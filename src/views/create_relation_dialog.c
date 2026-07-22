/******************************************************************************
 * @file create_relation_dialog.c
 * @brief Dialogue GTK de préparation d'une relation entre deux entités.
 ******************************************************************************/

#include "views/create_relation_dialog.h"

#include "models/entity_record.h"
#include "models/evidence_record.h"
#include "models/relation_record.h"

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
    GPtrArray *evidence_identifiers;

    gint confidence;
};

/** @brief Copie privée des données nécessaires à la sélection d'une preuve. */
typedef struct
{
    char *identifier;
    char *name;
    char *type;
    char *date;
    char *sha256;
    char *absolute_path;
} CreateRelationEvidenceOption;

/** @brief Libère une option de preuve. */
static void create_relation_evidence_option_free(gpointer data)
{
    CreateRelationEvidenceOption *option = data;
    if (option == NULL) return;
    g_free(option->identifier); g_free(option->name); g_free(option->type);
    g_free(option->date); g_free(option->sha256); g_free(option->absolute_path);
    g_free(option);
}

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
    GPtrArray *evidence_options;
    GPtrArray *evidence_check_buttons;
    GtkWidget *evidence_preview_stack;
    GtkWidget *evidence_preview_status;
    GtkWidget *evidence_preview_picture;
    GtkWidget *evidence_preview_video;
    GtkWidget *evidence_metadata_label;

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

/** @brief Arrête la vidéo utilisée par l'aperçu des preuves. */
static void create_relation_dialog_clear_preview(
    CreateRelationDialogState *state)
{
    GtkMediaStream *stream = NULL;
    if (state == NULL || state->evidence_preview_video == NULL ||
        state->evidence_preview_picture == NULL) return;
    stream = gtk_video_get_media_stream(GTK_VIDEO(
        state->evidence_preview_video));
    if (stream != NULL) gtk_media_stream_pause(stream);
    gtk_video_set_media_stream(GTK_VIDEO(state->evidence_preview_video), NULL);
    gtk_picture_set_paintable(GTK_PICTURE(
        state->evidence_preview_picture), NULL);
}

/** @brief Affiche la preuve dont la case vient d'être manipulée. */
static void create_relation_dialog_on_evidence_toggled(GtkCheckButton *button,
    gpointer user_data)
{
    CreateRelationDialogState *state = user_data;
    CreateRelationEvidenceOption *option = g_object_get_data(
        G_OBJECT(button), "relation-evidence-option");
    GFile *file = NULL;
    GFileInfo *info = NULL;
    const char *content_type = NULL;
    char *mime = NULL;
    char *metadata = NULL;
    GError *error = NULL;
    if (state == NULL || option == NULL) return;
    create_relation_dialog_clear_preview(state);
    metadata = g_strdup_printf("%s\nType : %s\nDate : %s\nSHA-256 : %s",
        option->name, option->type, option->date, option->sha256);
    gtk_label_set_text(GTK_LABEL(state->evidence_metadata_label), metadata);
    g_free(metadata);
    if (option->absolute_path == NULL) goto unsupported;
    file = g_file_new_for_path(option->absolute_path);
    info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
        G_FILE_QUERY_INFO_NONE, NULL, &error);
    if (info != NULL) content_type = g_file_info_get_content_type(info);
    if (content_type != NULL) mime = g_content_type_get_mime_type(content_type);
    if (mime != NULL && g_str_has_prefix(mime, "image/"))
    {
        gtk_picture_set_filename(GTK_PICTURE(state->evidence_preview_picture),
            option->absolute_path);
        gtk_stack_set_visible_child_name(GTK_STACK(
            state->evidence_preview_stack), "image");
    }
    else if (mime != NULL && g_str_has_prefix(mime, "video/"))
    {
        GtkMediaStream *stream = gtk_media_file_new_for_filename(
            option->absolute_path);
        gtk_video_set_media_stream(GTK_VIDEO(state->evidence_preview_video),
            stream);
        g_object_unref(stream);
        gtk_stack_set_visible_child_name(GTK_STACK(
            state->evidence_preview_stack), "video");
    }
    else
    {
unsupported:
        gtk_label_set_text(GTK_LABEL(state->evidence_preview_status),
            "Aperçu indisponible pour ce format.");
        gtk_stack_set_visible_child_name(GTK_STACK(
            state->evidence_preview_stack), "status");
    }
    g_clear_error(&error); g_free(mime);
    g_clear_object(&info); g_clear_object(&file);
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
    create_relation_dialog_clear_preview(state);
    g_clear_pointer(&state->evidence_options, g_ptr_array_unref);
    g_clear_pointer(&state->evidence_check_buttons, g_ptr_array_unref);

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

    result->evidence_identifiers = g_ptr_array_new_with_free_func(g_free);
    if (result->evidence_identifiers != NULL)
    {
        guint evidence_index = 0;
        for (evidence_index = 0;
             evidence_index < state->evidence_check_buttons->len;
             evidence_index++)
        {
            GtkCheckButton *check_button = g_ptr_array_index(
                state->evidence_check_buttons, evidence_index);
            CreateRelationEvidenceOption *option = g_object_get_data(
                G_OBJECT(check_button), "relation-evidence-option");
            if (gtk_check_button_get_active(check_button) && option != NULL)
                g_ptr_array_add(result->evidence_identifiers,
                    g_strdup(option->identifier));
        }
    }

    g_free(
        justification_text
    );

    if (result->source_entity_identifier == NULL ||
        result->target_entity_identifier == NULL ||
        result->relation_type == NULL ||
        result->evidence_identifiers == NULL)
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

static gboolean create_relation_dialog_present_internal(
    GtkWindow *parent,
    const char *source_entity_identifier,
    const RelationRecord *existing_relation,
    const GPtrArray *entities,
    const GPtrArray *evidence_records,
    const GPtrArray *selected_evidence_identifiers,
    const char *investigation_root_path,
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

    GtkWidget *evidence_box = NULL;
    GtkWidget *evidence_list = NULL;
    GtkWidget *evidence_list_scrolled = NULL;
    GtkWidget *evidence_preview_box = NULL;
    GtkWidget *evidence_title = NULL;

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

    guint selected_target_index =
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
        entities == NULL || evidence_records == NULL ||
        investigation_root_path == NULL ||
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
    state->evidence_options = g_ptr_array_new_with_free_func(
        create_relation_evidence_option_free);
    state->evidence_check_buttons = g_ptr_array_new_with_free_func(
        g_object_unref);

    state->callback =
        callback;

    state->callback_data =
        user_data;

    target_labels =
        gtk_string_list_new(
            NULL
        );

    if (state->source_entity_identifier == NULL ||
        state->target_identifiers == NULL || state->evidence_options == NULL ||
        state->evidence_check_buttons == NULL ||
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

        if (existing_relation != NULL && g_strcmp0(entity_identifier,
                relation_record_get_target_entity_identifier(
                    existing_relation)) == 0)
        {
            selected_target_index = state->target_identifiers->len;
        }

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

    for (guint evidence_index = 0; evidence_index < evidence_records->len;
         evidence_index++)
    {
        const EvidenceRecord *record = g_ptr_array_index(evidence_records,
            evidence_index);
        CreateRelationEvidenceOption *option = NULL;
        char *candidate = NULL;
        char *canonical_root = NULL;
        char *canonical_path = NULL;
        const char *date = NULL;
        if (record == NULL) continue;
        option = g_try_new0(CreateRelationEvidenceOption, 1);
        if (option == NULL) continue;
        option->identifier = g_strdup(evidence_record_get_identifier(record));
        option->name = g_strdup(evidence_record_get_original_name(record));
        option->type = g_strdup(evidence_record_get_type_identifier(record));
        date = evidence_record_get_collected_at(record);
        if (date == NULL) date = evidence_record_get_imported_at(record);
        option->date = g_strdup(date != NULL ? date : "Date inconnue");
        option->sha256 = g_strdup(evidence_record_get_sha256(record));
        candidate = g_build_filename(investigation_root_path,
            evidence_record_get_relative_path(record), NULL);
        canonical_root = g_canonicalize_filename(investigation_root_path, NULL);
        canonical_path = g_canonicalize_filename(candidate, NULL);
        if (canonical_root != NULL && canonical_path != NULL &&
            g_str_has_prefix(canonical_path, canonical_root) &&
            (canonical_path[strlen(canonical_root)] == G_DIR_SEPARATOR ||
             canonical_path[strlen(canonical_root)] == '\0'))
            option->absolute_path = g_strdup(canonical_path);
        g_free(canonical_path); g_free(canonical_root); g_free(candidate);
        if (option->identifier == NULL || option->name == NULL ||
            option->type == NULL || option->sha256 == NULL)
            create_relation_evidence_option_free(option);
        else
            g_ptr_array_add(state->evidence_options, option);
    }

    state->window =
        GTK_WINDOW(
            gtk_window_new()
        );

    gtk_window_set_title(
        state->window,
        existing_relation != NULL
            ? "Modifier la relation"
            : "Ajouter une relation"
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
        920,
        780
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
        existing_relation != NULL
            ? "<b>Modifier la relation</b>"
            : "<b>Créer une relation entre deux entités</b>"
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
        selected_target_index
    );

    gtk_widget_set_sensitive(GTK_WIDGET(state->target_drop_down),
        existing_relation == NULL);

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
        existing_relation != NULL
            ? relation_record_get_confidence(existing_relation)
            : 80.0
    );

    state->justification_text_view =
        GTK_TEXT_VIEW(
            gtk_text_view_new()
        );

    if (existing_relation != NULL)
    {
        const char *relation_type = relation_record_get_relation_type(
            existing_relation);
        const char *label = relation_record_get_label(existing_relation);
        const char *justification = relation_record_get_justification(
            existing_relation);
        gtk_editable_set_text(GTK_EDITABLE(state->relation_type_entry),
            relation_type != NULL ? relation_type : "");
        gtk_editable_set_text(GTK_EDITABLE(state->label_entry),
            label != NULL ? label : "");
        gtk_text_buffer_set_text(gtk_text_view_get_buffer(
            state->justification_text_view),
            justification != NULL ? justification : "", -1);
    }

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

    evidence_title = gtk_label_new("Pièces jointes à la relation");
    gtk_label_set_xalign(GTK_LABEL(evidence_title), 0.0F);
    evidence_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    evidence_list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    evidence_list_scrolled = gtk_scrolled_window_new();
    gtk_widget_set_size_request(evidence_list_scrolled, 360, 240);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(evidence_list_scrolled),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(evidence_list_scrolled),
        evidence_list);
    for (guint evidence_index = 0;
         evidence_index < state->evidence_options->len; evidence_index++)
    {
        CreateRelationEvidenceOption *option = g_ptr_array_index(
            state->evidence_options, evidence_index);
        char *display = g_strdup_printf("%s — %s — %s", option->name,
            option->type, option->date);
        GtkWidget *check = gtk_check_button_new_with_label(display);
        gboolean active = FALSE;
        g_free(display);
        if (selected_evidence_identifiers != NULL)
            for (guint selected_index = 0;
                 selected_index < selected_evidence_identifiers->len;
                 selected_index++)
                if (g_strcmp0(option->identifier, g_ptr_array_index(
                        selected_evidence_identifiers, selected_index)) == 0)
                    active = TRUE;
        gtk_check_button_set_active(GTK_CHECK_BUTTON(check), active);
        g_object_set_data(G_OBJECT(check), "relation-evidence-option", option);
        g_signal_connect(check, "toggled", G_CALLBACK(
            create_relation_dialog_on_evidence_toggled), state);
        gtk_box_append(GTK_BOX(evidence_list), check);
        g_ptr_array_add(state->evidence_check_buttons, g_object_ref(check));
    }
    if (state->evidence_options->len == 0)
        gtk_box_append(GTK_BOX(evidence_list), gtk_label_new(
            "Aucune preuve enregistrée dans l’enquête."));

    evidence_preview_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_hexpand(evidence_preview_box, TRUE);
    state->evidence_metadata_label = gtk_label_new(
        "Sélectionnez une preuve pour afficher son aperçu.");
    gtk_label_set_xalign(GTK_LABEL(state->evidence_metadata_label), 0.0F);
    gtk_label_set_wrap(GTK_LABEL(state->evidence_metadata_label), TRUE);
    gtk_label_set_selectable(GTK_LABEL(state->evidence_metadata_label), TRUE);
    state->evidence_preview_stack = gtk_stack_new();
    gtk_widget_set_size_request(state->evidence_preview_stack, 420, 220);
    state->evidence_preview_status = gtk_label_new("Aucun aperçu.");
    state->evidence_preview_picture = gtk_picture_new();
    gtk_picture_set_content_fit(GTK_PICTURE(state->evidence_preview_picture),
        GTK_CONTENT_FIT_CONTAIN);
    state->evidence_preview_video = gtk_video_new();
    gtk_video_set_autoplay(GTK_VIDEO(state->evidence_preview_video), FALSE);
    gtk_stack_add_named(GTK_STACK(state->evidence_preview_stack),
        state->evidence_preview_status, "status");
    gtk_stack_add_named(GTK_STACK(state->evidence_preview_stack),
        state->evidence_preview_picture, "image");
    gtk_stack_add_named(GTK_STACK(state->evidence_preview_stack),
        state->evidence_preview_video, "video");
    gtk_box_append(GTK_BOX(evidence_preview_box),
        state->evidence_metadata_label);
    gtk_box_append(GTK_BOX(evidence_preview_box), state->evidence_preview_stack);
    gtk_box_append(GTK_BOX(evidence_box), evidence_list_scrolled);
    gtk_box_append(GTK_BOX(evidence_box), evidence_preview_box);
    gtk_grid_attach(GTK_GRID(grid), evidence_title, 0, 5, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), evidence_box, 0, 6, 2, 1);

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
            existing_relation != NULL ? "Enregistrer" : "Ajouter"
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

gboolean create_relation_dialog_present(GtkWindow *parent,
    const char *source_entity_identifier, const GPtrArray *entities,
    const GPtrArray *evidence_records, const char *investigation_root_path,
    CreateRelationDialogCallback callback, gpointer user_data, GError **error)
{
    return create_relation_dialog_present_internal(parent,
        source_entity_identifier, NULL, entities, evidence_records, NULL,
        investigation_root_path, callback, user_data, error);
}

gboolean edit_relation_dialog_present(GtkWindow *parent,
    const RelationRecord *relation_record, const GPtrArray *entities,
    const GPtrArray *evidence_records,
    const GPtrArray *selected_evidence_identifiers,
    const char *investigation_root_path,
    CreateRelationDialogCallback callback, gpointer user_data, GError **error)
{
    if (relation_record == NULL)
    {
        g_set_error_literal(error, CREATE_RELATION_DIALOG_ERROR,
            CREATE_RELATION_DIALOG_ERROR_INVALID_ARGUMENT,
            "La relation à modifier est invalide.");
        return FALSE;
    }
    return create_relation_dialog_present_internal(parent,
        relation_record_get_source_entity_identifier(relation_record),
        relation_record, entities, evidence_records,
        selected_evidence_identifiers, investigation_root_path,
        callback, user_data, error);
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

    g_clear_pointer(&result->evidence_identifiers, g_ptr_array_unref);

    g_free(
        result
    );
}

const GPtrArray *create_relation_dialog_result_get_evidence_identifiers(
    const CreateRelationDialogResult *result)
{
    return result != NULL ? result->evidence_identifiers : NULL;
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
