/******************************************************************************
 * @file evidence_import_dialog.c
 * @brief Dialogue GTK de saisie des métadonnées d'une preuve.
 ******************************************************************************/

#include "views/evidence_import_dialog.h"
#include "models/evidence_type.h"

#include <string.h>

#include <glib.h>

/**
 * @struct EvidenceImportDialogResult
 * @brief Métadonnées validées avant le lancement d'un import.
 */
struct EvidenceImportDialogResult
{
    char *type_identifier;
    char *collected_at;
    char *source;
    char *description;
};

/**
 * @brief État privé conservé pendant l'affichage du dialogue.
 */
typedef struct
{
    GtkWindow *window;
    GtkDropDown *type_drop_down;
    GtkEntry *collected_at_entry;
    GtkEntry *source_entry;
    GtkTextView *description_text_view;
    GtkLabel *error_label;

    char *file_path;
    GPtrArray *type_codes;

    EvidenceImportDialogCallback callback;
    gpointer callback_data;

    gboolean completed;
} EvidenceImportDialogState;

/**
 * @brief Vérifie qu'une chaîne contient du texte non blanc.
 */
static gboolean evidence_import_dialog_value_has_text(
    const char *value
)
{
    char *normalized_value = NULL;
    gboolean has_text = FALSE;

    if (value == NULL)
    {
        return FALSE;
    }

    normalized_value =
        g_strdup(
            value
        );

    if (normalized_value == NULL)
    {
        return FALSE;
    }

    g_strstrip(
        normalized_value
    );

    has_text =
        normalized_value[0] != '\0';

    g_free(
        normalized_value
    );

    return has_text;
}

/**
 * @brief Vérifie qu'une date contient un fuseau horaire explicite.
 *
 * Les formats acceptés sont notamment :
 *
 * - 2026-07-20T08:30:00Z
 * - 2026-07-20T10:30:00+02
 * - 2026-07-20T10:30:00+02:00
 */
static gboolean evidence_import_dialog_has_explicit_timezone(
    const char *value
)
{
    gsize length = 0;
    const char *offset = NULL;

    if (value == NULL)
    {
        return FALSE;
    }

    length =
        strlen(
            value
        );

    if (length == 0)
    {
        return FALSE;
    }

    /*
     * Fuseau UTC explicite.
     */
    if (value[length - 1] == 'Z' ||
        value[length - 1] == 'z')
    {
        return TRUE;
    }

    /*
     * Vérifie une terminaison de la forme +02:00 ou -05:30.
     */
    if (length >= 6)
    {
        offset =
            value + length - 6;

        if ((offset[0] == '+' ||
             offset[0] == '-') &&
            g_ascii_isdigit(offset[1]) &&
            g_ascii_isdigit(offset[2]) &&
            offset[3] == ':' &&
            g_ascii_isdigit(offset[4]) &&
            g_ascii_isdigit(offset[5]))
        {
            return TRUE;
        }
    }

    /*
     * Vérifie une terminaison ISO 8601 courte :
     * +02 ou -05.
     */
    if (length >= 3)
    {
        offset =
            value + length - 3;

        if ((offset[0] == '+' ||
             offset[0] == '-') &&
            g_ascii_isdigit(offset[1]) &&
            g_ascii_isdigit(offset[2]))
        {
            return TRUE;
        }
    }

    return FALSE;
}

/**
 * @brief Convertit une date ISO 8601 en UTC au format EvidenceRecord.
 *
 * Exemple :
 *
 * 2026-07-20T09:16:01.492569+02
 * devient :
 * 2026-07-20T07:16:01Z
 *
 * @return Nouvelle chaîne possédée par l'appelant, ou NULL.
 */
static char *evidence_import_dialog_normalize_collected_at(
    const char *value,
    GError **error
)
{
    char *normalized_value = NULL;
    char *utc_value = NULL;

    GDateTime *date_time = NULL;
    GDateTime *utc_date_time = NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (!evidence_import_dialog_value_has_text(
            value
        ))
    {
        g_set_error_literal(
            error,
            EVIDENCE_IMPORT_DIALOG_ERROR,
            EVIDENCE_IMPORT_DIALOG_ERROR_INVALID_DATA,
            "La date de collecte est obligatoire."
        );

        return NULL;
    }

    normalized_value =
        g_strdup(
            value
        );

    if (normalized_value == NULL)
    {
        g_set_error_literal(
            error,
            EVIDENCE_IMPORT_DIALOG_ERROR,
            EVIDENCE_IMPORT_DIALOG_ERROR_MEMORY,
            "Impossible de copier la date de collecte."
        );

        return NULL;
    }

    g_strstrip(
        normalized_value
    );

    date_time =
        g_date_time_new_from_iso8601(
            normalized_value,
            NULL
        );

    g_free(
        normalized_value
    );

    if (date_time == NULL)
    {
        g_set_error_literal(
            error,
            EVIDENCE_IMPORT_DIALOG_ERROR,
            EVIDENCE_IMPORT_DIALOG_ERROR_INVALID_DATA,
            "La date de collecte n'est pas une date ISO 8601 valide."
        );

        return NULL;
    }

    utc_date_time =
        g_date_time_to_utc(
            date_time
        );

    g_date_time_unref(
        date_time
    );

    if (utc_date_time == NULL)
    {
        g_set_error_literal(
            error,
            EVIDENCE_IMPORT_DIALOG_ERROR,
            EVIDENCE_IMPORT_DIALOG_ERROR_MEMORY,
            "Impossible de convertir la date de collecte en UTC."
        );

        return NULL;
    }

    utc_value =
        g_date_time_format(
            utc_date_time,
            "%Y-%m-%dT%H:%M:%SZ"
        );

    g_date_time_unref(
        utc_date_time
    );

    if (utc_value == NULL)
    {
        g_set_error_literal(
            error,
            EVIDENCE_IMPORT_DIALOG_ERROR,
            EVIDENCE_IMPORT_DIALOG_ERROR_MEMORY,
            "Impossible de formater la date de collecte."
        );

        return NULL;
    }

    return utc_value;
}

GQuark evidence_import_dialog_error_quark(void)
{
    return g_quark_from_static_string(
        "evidence-import-dialog-error-quark"
    );
}

/**
 * @brief Retourne une copie nettoyée ou NULL pour une valeur vide.
 */
static char *evidence_import_dialog_duplicate_optional_text(
    const char *value
)
{
    char *normalized_value = NULL;

    if (value == NULL)
    {
        return NULL;
    }

    normalized_value =
        g_strdup(
            value
        );

    if (normalized_value == NULL)
    {
        return NULL;
    }

    g_strstrip(
        normalized_value
    );

    if (normalized_value[0] == '\0')
    {
        g_free(
            normalized_value
        );

        return NULL;
    }

    return normalized_value;
}

/**
 * @brief Affiche un message de validation dans le dialogue.
 */
static void evidence_import_dialog_show_error(
    EvidenceImportDialogState *state,
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
        message != NULL
            ? message
            : "Les métadonnées sont invalides."
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
static void evidence_import_dialog_state_free(
    gpointer user_data
)
{
    EvidenceImportDialogState *state =
        user_data;

    if (state == NULL)
    {
        return;
    }

    g_free(
        state->file_path
    );

    g_ptr_array_unref(
        state->type_codes
    );

    g_free(
        state
    );
}

/**
 * @brief Retourne le contenu complet de la description.
 */
static char *evidence_import_dialog_get_description_text(
    EvidenceImportDialogState *state
)
{
    GtkTextBuffer *buffer = NULL;

    GtkTextIter start;
    GtkTextIter end;

    if (state == NULL ||
        state->description_text_view == NULL)
    {
        return NULL;
    }

    buffer =
        gtk_text_view_get_buffer(
            state->description_text_view
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
 * @brief Termine le dialogue avec une annulation.
 */
static void evidence_import_dialog_cancel(
    EvidenceImportDialogState *state
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
static gboolean evidence_import_dialog_on_close_requested(
    GtkWindow *window,
    gpointer user_data
)
{
    EvidenceImportDialogState *state =
        user_data;

    (void) window;

    evidence_import_dialog_cancel(
        state
    );

    /*
     * FALSE laisse GTK exécuter son comportement normal de fermeture.
     */
    return FALSE;
}

/**
 * @brief Traite le bouton Annuler.
 */
static void evidence_import_dialog_on_cancel_clicked(
    GtkButton *button,
    gpointer user_data
)
{
    EvidenceImportDialogState *state =
        user_data;

    (void) button;

    if (state == NULL)
    {
        return;
    }

    evidence_import_dialog_cancel(
        state
    );

    gtk_window_close(
        state->window
    );
}

/**
 * @brief Traite la validation du formulaire.
 */
static void evidence_import_dialog_on_import_clicked(
    GtkButton *button,
    gpointer user_data
)
{
    EvidenceImportDialogState *state =
        user_data;

    EvidenceImportDialogResult *result = NULL;

    guint selected_index =
        GTK_INVALID_LIST_POSITION;

    const char *type_identifier = NULL;
    const char *collected_at = NULL;
    const char *source = NULL;

    char *description = NULL;
    char *normalized_collected_at = NULL;

    GError *normalization_error = NULL;
    GError *validation_error = NULL;

    (void) button;

    if (state == NULL ||
        state->completed)
    {
        return;
    }

    selected_index =
        gtk_drop_down_get_selected(
            state->type_drop_down
        );

    if (selected_index ==
            GTK_INVALID_LIST_POSITION ||
        selected_index >= state->type_codes->len)
    {
        evidence_import_dialog_show_error(
            state,
            "Sélectionnez un type de preuve."
        );

        return;
    }

    type_identifier =
        g_ptr_array_index(
            state->type_codes,
            selected_index
        );

    collected_at =
        gtk_editable_get_text(
            GTK_EDITABLE(
                state->collected_at_entry
            )
        );

    source =
        gtk_editable_get_text(
            GTK_EDITABLE(
                state->source_entry
            )
        );

    description =
        evidence_import_dialog_get_description_text(
            state
        );

    if (!evidence_import_dialog_validate_values(
            state->file_path,
            type_identifier,
            collected_at,
            &validation_error
        ))
    {
        evidence_import_dialog_show_error(
            state,
            validation_error != NULL
                ? validation_error->message
                : "Les métadonnées sont invalides."
        );

        g_clear_error(
            &validation_error
        );

        g_free(
            description
        );

        return;
    }

    normalized_collected_at =
        evidence_import_dialog_normalize_collected_at(
            collected_at,
            &normalization_error
        );

    if (normalized_collected_at == NULL)
    {
        evidence_import_dialog_show_error(
            state,
            normalization_error != NULL
                ? normalization_error->message
                : "Impossible de normaliser la date de collecte."
        );

        g_clear_error(
            &normalization_error
        );

        g_free(
            description
        );

        return;
    }

    result =
        g_try_new0(
            EvidenceImportDialogResult,
            1
        );

    if (result == NULL)
    {
        g_free(
            normalized_collected_at
        );

        g_free(
            description
        );

        evidence_import_dialog_show_error(
            state,
            "Impossible d'allouer le résultat du formulaire."
        );

        return;
    }

    g_strstrip(
        normalized_collected_at
    );

    result->type_identifier =
        g_strdup(
            type_identifier
        );

    result->collected_at =
        normalized_collected_at;

    result->source =
        evidence_import_dialog_duplicate_optional_text(
            source
        );

    result->description =
        evidence_import_dialog_duplicate_optional_text(
            description
        );

    g_free(
        description
    );

    if (result->type_identifier == NULL)
    {
        evidence_import_dialog_result_free(
            result
        );

        evidence_import_dialog_show_error(
            state,
            "Impossible de conserver le type de preuve."
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
        evidence_import_dialog_result_free(
            result
        );
    }

    gtk_window_close(
        state->window
    );
}

gboolean evidence_import_dialog_present(
    GtkWindow *parent,
    const char *file_path,
    const GPtrArray *evidence_types,
    EvidenceImportDialogCallback callback,
    gpointer user_data,
    GError **error
)
{
    EvidenceImportDialogState *state = NULL;

    GtkStringList *type_labels = NULL;

    GtkWidget *root_box = NULL;
    GtkWidget *title_label = NULL;
    GtkWidget *path_title_label = NULL;
    GtkWidget *path_label = NULL;

    GtkWidget *grid = NULL;

    GtkWidget *type_label = NULL;
    GtkWidget *collected_at_label = NULL;
    GtkWidget *source_label = NULL;
    GtkWidget *description_label = NULL;

    GtkWidget *description_scrolled_window = NULL;

    GtkWidget *button_box = NULL;
    GtkWidget *cancel_button = NULL;
    GtkWidget *import_button = NULL;

    GDateTime *current_date_time = NULL;
    char *current_date_time_iso8601 = NULL;

    guint default_type_index = 0;
    guint type_index = 0;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (parent == NULL ||
        !GTK_IS_WINDOW(parent) ||
        file_path == NULL ||
        file_path[0] == '\0' ||
        evidence_types == NULL ||
        callback == NULL)
    {
        g_set_error_literal(
            error,
            EVIDENCE_IMPORT_DIALOG_ERROR,
            EVIDENCE_IMPORT_DIALOG_ERROR_INVALID_ARGUMENT,
            "Les arguments du dialogue d'import sont invalides."
        );

        return FALSE;
    }

    if (evidence_types->len == 0)
    {
        g_set_error_literal(
            error,
            EVIDENCE_IMPORT_DIALOG_ERROR,
            EVIDENCE_IMPORT_DIALOG_ERROR_EMPTY_TYPES,
            "Aucun type de preuve n'est disponible."
        );

        return FALSE;
    }

    state =
        g_try_new0(
            EvidenceImportDialogState,
            1
        );

    if (state == NULL)
    {
        g_set_error_literal(
            error,
            EVIDENCE_IMPORT_DIALOG_ERROR,
            EVIDENCE_IMPORT_DIALOG_ERROR_MEMORY,
            "Impossible d'allouer l'état du dialogue."
        );

        return FALSE;
    }

    state->file_path =
        g_strdup(
            file_path
        );

    state->type_codes =
        g_ptr_array_new_with_free_func(
            g_free
        );

    state->callback =
        callback;

    state->callback_data =
        user_data;

    if (state->file_path == NULL ||
        state->type_codes == NULL)
    {
        evidence_import_dialog_state_free(
            state
        );

        g_set_error_literal(
            error,
            EVIDENCE_IMPORT_DIALOG_ERROR,
            EVIDENCE_IMPORT_DIALOG_ERROR_MEMORY,
            "Impossible de préparer les données du dialogue."
        );

        return FALSE;
    }

    type_labels =
        gtk_string_list_new(
            NULL
        );

    if (type_labels == NULL)
    {
        evidence_import_dialog_state_free(
            state
        );

        g_set_error_literal(
            error,
            EVIDENCE_IMPORT_DIALOG_ERROR,
            EVIDENCE_IMPORT_DIALOG_ERROR_MEMORY,
            "Impossible de créer la liste des types de preuve."
        );

        return FALSE;
    }

    for (type_index = 0;
         type_index < evidence_types->len;
         type_index++)
    {
        const EvidenceType *evidence_type =
            g_ptr_array_index(
                evidence_types,
                type_index
            );

        const char *code =
            evidence_type_get_code(
                evidence_type
            );

        const char *label =
            evidence_type_get_label(
                evidence_type
            );

        char *code_copy = NULL;

        if (evidence_type == NULL ||
            code == NULL ||
            code[0] == '\0' ||
            label == NULL ||
            label[0] == '\0')
        {
            g_object_unref(
                type_labels
            );

            evidence_import_dialog_state_free(
                state
            );

            g_set_error_literal(
                error,
                EVIDENCE_IMPORT_DIALOG_ERROR,
                EVIDENCE_IMPORT_DIALOG_ERROR_INVALID_ARGUMENT,
                "La liste contient un type de preuve invalide."
            );

            return FALSE;
        }

        code_copy =
            g_strdup(
                code
            );

        if (code_copy == NULL)
        {
            g_object_unref(
                type_labels
            );

            evidence_import_dialog_state_free(
                state
            );

            g_set_error_literal(
                error,
                EVIDENCE_IMPORT_DIALOG_ERROR,
                EVIDENCE_IMPORT_DIALOG_ERROR_MEMORY,
                "Impossible de copier un type de preuve."
            );

            return FALSE;
        }

        g_ptr_array_add(
            state->type_codes,
            code_copy
        );

        gtk_string_list_append(
            type_labels,
            label
        );

        if (g_strcmp0(
                code,
                "document"
            ) == 0)
        {
            default_type_index =
                type_index;
        }
    }

    state->window =
        GTK_WINDOW(
            gtk_window_new()
        );

    gtk_window_set_title(
        state->window,
        "Importer une preuve"
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
        620,
        520
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
        GTK_LABEL(title_label),
        "<b>Saisir les métadonnées de la preuve</b>"
    );

    gtk_label_set_xalign(
        GTK_LABEL(title_label),
        0.0F
    );

    gtk_box_append(
        GTK_BOX(root_box),
        title_label
    );

    path_title_label =
        gtk_label_new(
            "Fichier sélectionné"
        );

    gtk_label_set_xalign(
        GTK_LABEL(path_title_label),
        0.0F
    );

    gtk_box_append(
        GTK_BOX(root_box),
        path_title_label
    );

    path_label =
        gtk_label_new(
            state->file_path
        );

    gtk_label_set_xalign(
        GTK_LABEL(path_label),
        0.0F
    );

    gtk_label_set_wrap(
        GTK_LABEL(path_label),
        TRUE
    );

    gtk_label_set_selectable(
        GTK_LABEL(path_label),
        TRUE
    );

    gtk_box_append(
        GTK_BOX(root_box),
        path_label
    );

    grid =
        gtk_grid_new();

    gtk_grid_set_row_spacing(
        GTK_GRID(grid),
        10
    );

    gtk_grid_set_column_spacing(
        GTK_GRID(grid),
        12
    );

    gtk_widget_set_vexpand(
        grid,
        TRUE
    );

    type_label =
        gtk_label_new(
            "Type de preuve"
        );

    gtk_label_set_xalign(
        GTK_LABEL(type_label),
        0.0F
    );

    state->type_drop_down =
        GTK_DROP_DOWN(
            gtk_drop_down_new(
                G_LIST_MODEL(type_labels),
                NULL
            )
        );

    /*
     * gtk_drop_down_new() prend possession de la référence
     * de type_labels.
     */
    type_labels = NULL;

    gtk_drop_down_set_selected(
        state->type_drop_down,
        default_type_index
    );

    gtk_widget_set_hexpand(
        GTK_WIDGET(
            state->type_drop_down
        ),
        TRUE
    );

    collected_at_label =
        gtk_label_new(
            "Date de collecte"
        );

    gtk_label_set_xalign(
        GTK_LABEL(collected_at_label),
        0.0F
    );

    state->collected_at_entry =
        GTK_ENTRY(
            gtk_entry_new()
        );

    gtk_entry_set_placeholder_text(
        state->collected_at_entry,
        "2026-07-20T10:30:00+02:00"
    );

    current_date_time =
        g_date_time_new_now_local();

    if (current_date_time != NULL)
    {
        current_date_time_iso8601 =
            g_date_time_format_iso8601(
                current_date_time
            );

        g_date_time_unref(
            current_date_time
        );
    }

    if (current_date_time_iso8601 != NULL)
    {
        gtk_editable_set_text(
            GTK_EDITABLE(
                state->collected_at_entry
            ),
            current_date_time_iso8601
        );

        g_free(
            current_date_time_iso8601
        );
    }

    source_label =
        gtk_label_new(
            "Source"
        );

    gtk_label_set_xalign(
        GTK_LABEL(source_label),
        0.0F
    );

    state->source_entry =
        GTK_ENTRY(
            gtk_entry_new()
        );

    gtk_entry_set_placeholder_text(
        state->source_entry,
        "Ex. : conversation Instagram, email reçu..."
    );

    description_label =
        gtk_label_new(
            "Description"
        );

    gtk_label_set_xalign(
        GTK_LABEL(description_label),
        0.0F
    );

    state->description_text_view =
        GTK_TEXT_VIEW(
            gtk_text_view_new()
        );

    gtk_text_view_set_wrap_mode(
        state->description_text_view,
        GTK_WRAP_WORD_CHAR
    );

    description_scrolled_window =
        gtk_scrolled_window_new();

    gtk_scrolled_window_set_child(
        GTK_SCROLLED_WINDOW(
            description_scrolled_window
        ),
        GTK_WIDGET(
            state->description_text_view
        )
    );

    gtk_widget_set_size_request(
        description_scrolled_window,
        -1,
        130
    );

    gtk_grid_attach(
        GTK_GRID(grid),
        type_label,
        0,
        0,
        1,
        1
    );

    gtk_grid_attach(
        GTK_GRID(grid),
        GTK_WIDGET(
            state->type_drop_down
        ),
        1,
        0,
        1,
        1
    );

    gtk_grid_attach(
        GTK_GRID(grid),
        collected_at_label,
        0,
        1,
        1,
        1
    );

    gtk_grid_attach(
        GTK_GRID(grid),
        GTK_WIDGET(
            state->collected_at_entry
        ),
        1,
        1,
        1,
        1
    );

    gtk_grid_attach(
        GTK_GRID(grid),
        source_label,
        0,
        2,
        1,
        1
    );

    gtk_grid_attach(
        GTK_GRID(grid),
        GTK_WIDGET(
            state->source_entry
        ),
        1,
        2,
        1,
        1
    );

    gtk_grid_attach(
        GTK_GRID(grid),
        description_label,
        0,
        3,
        1,
        1
    );

    gtk_grid_attach(
        GTK_GRID(grid),
        description_scrolled_window,
        1,
        3,
        1,
        1
    );

    gtk_box_append(
        GTK_BOX(root_box),
        grid
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

    gtk_box_append(
        GTK_BOX(root_box),
        GTK_WIDGET(
            state->error_label
        )
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

    import_button =
        gtk_button_new_with_label(
            "Importer"
        );

    gtk_widget_add_css_class(
        import_button,
        "suggested-action"
    );

    gtk_box_append(
        GTK_BOX(button_box),
        cancel_button
    );

    gtk_box_append(
        GTK_BOX(button_box),
        import_button
    );

    gtk_box_append(
        GTK_BOX(root_box),
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
        "labfy-evidence-import-dialog-state",
        state,
        evidence_import_dialog_state_free
    );

    g_signal_connect(
        state->window,
        "close-request",
        G_CALLBACK(
            evidence_import_dialog_on_close_requested
        ),
        state
    );

    g_signal_connect(
        cancel_button,
        "clicked",
        G_CALLBACK(
            evidence_import_dialog_on_cancel_clicked
        ),
        state
    );

    g_signal_connect(
        import_button,
        "clicked",
        G_CALLBACK(
            evidence_import_dialog_on_import_clicked
        ),
        state
    );

    gtk_window_set_default_widget(
        state->window,
        import_button
    );

    gtk_window_present(
        state->window
    );

    return TRUE;
}

gboolean evidence_import_dialog_validate_values(
    const char *file_path,
    const char *type_identifier,
    const char *collected_at,
    GError **error
)
{
    char *normalized_type_identifier = NULL;
    char *normalized_collected_at = NULL;

    GDateTime *collected_date_time = NULL;

    gboolean valid = FALSE;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (file_path == NULL ||
        file_path[0] == '\0')
    {
        g_set_error_literal(
            error,
            EVIDENCE_IMPORT_DIALOG_ERROR,
            EVIDENCE_IMPORT_DIALOG_ERROR_INVALID_DATA,
            "Le chemin du fichier est obligatoire."
        );

        return FALSE;
    }

    /*
     * Le chemin n'est pas normalisé avec g_strstrip(), car les espaces
     * peuvent faire partie d'un nom de fichier Unix valide.
     */
    if (!g_file_test(
            file_path,
            G_FILE_TEST_IS_REGULAR
        ))
    {
        g_set_error(
            error,
            EVIDENCE_IMPORT_DIALOG_ERROR,
            EVIDENCE_IMPORT_DIALOG_ERROR_INVALID_DATA,
            "Le fichier sélectionné est absent ou n'est pas un fichier "
            "ordinaire : %s",
            file_path
        );

        return FALSE;
    }

    if (!evidence_import_dialog_value_has_text(
            type_identifier
        ))
    {
        g_set_error_literal(
            error,
            EVIDENCE_IMPORT_DIALOG_ERROR,
            EVIDENCE_IMPORT_DIALOG_ERROR_INVALID_DATA,
            "Le type de preuve est obligatoire."
        );

        return FALSE;
    }

    normalized_type_identifier =
        g_strdup(
            type_identifier
        );

    if (normalized_type_identifier == NULL)
    {
        g_set_error_literal(
            error,
            EVIDENCE_IMPORT_DIALOG_ERROR,
            EVIDENCE_IMPORT_DIALOG_ERROR_MEMORY,
            "Impossible de normaliser le type de preuve."
        );

        return FALSE;
    }

    g_strstrip(
        normalized_type_identifier
    );

    if (!evidence_import_dialog_value_has_text(
            collected_at
        ))
    {
        g_set_error_literal(
            error,
            EVIDENCE_IMPORT_DIALOG_ERROR,
            EVIDENCE_IMPORT_DIALOG_ERROR_INVALID_DATA,
            "La date de collecte est obligatoire."
        );

        goto cleanup;
    }

    normalized_collected_at =
        g_strdup(
            collected_at
        );

    if (normalized_collected_at == NULL)
    {
        g_set_error_literal(
            error,
            EVIDENCE_IMPORT_DIALOG_ERROR,
            EVIDENCE_IMPORT_DIALOG_ERROR_MEMORY,
            "Impossible de normaliser la date de collecte."
        );

        goto cleanup;
    }

    g_strstrip(
        normalized_collected_at
    );

    if (!evidence_import_dialog_has_explicit_timezone(
            normalized_collected_at
        ))
    {
        g_set_error_literal(
            error,
            EVIDENCE_IMPORT_DIALOG_ERROR,
            EVIDENCE_IMPORT_DIALOG_ERROR_INVALID_DATA,
            "La date de collecte doit contenir un fuseau horaire."
        );

        goto cleanup;
    }

    collected_date_time =
        g_date_time_new_from_iso8601(
            normalized_collected_at,
            NULL
        );

    if (collected_date_time == NULL)
    {
        g_set_error_literal(
            error,
            EVIDENCE_IMPORT_DIALOG_ERROR,
            EVIDENCE_IMPORT_DIALOG_ERROR_INVALID_DATA,
            "La date de collecte n'est pas une date ISO 8601 valide."
        );

        goto cleanup;
    }

    valid = TRUE;

cleanup:

    if (collected_date_time != NULL)
    {
        g_date_time_unref(
            collected_date_time
        );
    }

    g_free(
        normalized_collected_at
    );

    g_free(
        normalized_type_identifier
    );

    return valid;
}

void evidence_import_dialog_result_free(
    EvidenceImportDialogResult *result
)
{
    if (result == NULL)
    {
        return;
    }

    g_free(
        result->type_identifier
    );

    g_free(
        result->collected_at
    );

    g_free(
        result->source
    );

    g_free(
        result->description
    );

    g_free(
        result
    );
}

const char *evidence_import_dialog_result_get_type_identifier(
    const EvidenceImportDialogResult *result
)
{
    if (result == NULL)
    {
        return NULL;
    }

    return result->type_identifier;
}

const char *evidence_import_dialog_result_get_collected_at(
    const EvidenceImportDialogResult *result
)
{
    if (result == NULL)
    {
        return NULL;
    }

    return result->collected_at;
}

const char *evidence_import_dialog_result_get_source(
    const EvidenceImportDialogResult *result
)
{
    if (result == NULL)
    {
        return NULL;
    }

    return result->source;
}

const char *evidence_import_dialog_result_get_description(
    const EvidenceImportDialogResult *result
)
{
    if (result == NULL)
    {
        return NULL;
    }

    return result->description;
}
