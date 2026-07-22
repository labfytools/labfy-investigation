/******************************************************************************
 * @file evidence_metadata_dialog.c
 * @brief Dialogue de modification des métadonnées éditables d'une preuve.
 ******************************************************************************/

#include "views/evidence_metadata_dialog.h"

#include "models/evidence_type.h"

struct EvidenceMetadataDialogResult
{
    char *type_identifier;
    char *source;
    char *description;
};

typedef struct
{
    GtkWindow *window;
    GtkDropDown *type_dropdown;
    GtkEditable *source_entry;
    GtkTextBuffer *description_buffer;
    GtkStringList *type_labels;
    GPtrArray *type_identifiers;
    EvidenceMetadataDialogCallback callback;
    gpointer user_data;
    gboolean completed;
} EvidenceMetadataDialogContext;

/** @brief Copie un texte facultatif et transforme une chaîne vide en NULL. */
static char *evidence_metadata_dialog_copy_optional(const char *text)
{
    char *copy = text != NULL ? g_strdup(text) : NULL;
    if (copy != NULL) g_strstrip(copy);
    if (copy != NULL && copy[0] == '\0') g_clear_pointer(&copy, g_free);
    return copy;
}

/** @brief Libère le contexte détenu par la fenêtre. */
static void evidence_metadata_dialog_context_free(gpointer data)
{
    EvidenceMetadataDialogContext *context = data;
    if (context == NULL) return;
    g_clear_pointer(&context->type_identifiers, g_ptr_array_unref);
    g_clear_object(&context->type_labels);
    g_free(context);
}

/** @brief Détruit la fenêtre hors de la pile du signal GTK courant. */
static gboolean evidence_metadata_dialog_destroy_idle(gpointer data)
{
    GtkWindow *window = GTK_WINDOW(data);
    gtk_window_destroy(window);
    g_object_unref(window);
    return G_SOURCE_REMOVE;
}

/** @brief Termine le dialogue et transmet éventuellement un résultat. */
static void evidence_metadata_dialog_complete(
    EvidenceMetadataDialogContext *context,
    EvidenceMetadataDialogResult *result
)
{
    if (context == NULL || context->completed) return;
    context->completed = TRUE;
    if (context->callback != NULL) context->callback(result, context->user_data);
    else evidence_metadata_dialog_result_free(result);
    gtk_widget_set_visible(GTK_WIDGET(context->window), FALSE);
    g_idle_add(
        evidence_metadata_dialog_destroy_idle,
        g_object_ref(context->window)
    );
}

/** @brief Annule la modification. */
static void evidence_metadata_dialog_on_cancel(
    GtkButton *button, gpointer user_data
)
{
    (void) button;
    evidence_metadata_dialog_complete(user_data, NULL);
}

/** @brief Traite la fermeture native comme une annulation explicite. */
static gboolean evidence_metadata_dialog_on_close_request(
    GtkWindow *window, gpointer user_data
)
{
    (void) window;
    evidence_metadata_dialog_complete(user_data, NULL);
    return TRUE;
}

/** @brief Construit et transmet les valeurs modifiées. */
static void evidence_metadata_dialog_on_save(
    GtkButton *button, gpointer user_data
)
{
    EvidenceMetadataDialogContext *context = user_data;
    EvidenceMetadataDialogResult *result = NULL;
    GtkTextIter start; GtkTextIter end; char *description = NULL;
    guint selected = 0U;
    (void) button;
    if (context == NULL) return;
    selected = gtk_drop_down_get_selected(context->type_dropdown);
    if (selected >= context->type_identifiers->len) return;
    gtk_text_buffer_get_bounds(context->description_buffer, &start, &end);
    description = gtk_text_buffer_get_text(
        context->description_buffer, &start, &end, FALSE);
    result = g_new0(EvidenceMetadataDialogResult, 1);
    result->type_identifier = g_strdup(
        g_ptr_array_index(context->type_identifiers, selected));
    result->source = evidence_metadata_dialog_copy_optional(
        gtk_editable_get_text(context->source_entry));
    result->description = evidence_metadata_dialog_copy_optional(description);
    g_free(description);
    evidence_metadata_dialog_complete(context, result);
}

gboolean evidence_metadata_dialog_present(
    GtkWindow *parent, const EvidenceRecord *record,
    const GPtrArray *evidence_types, EvidenceMetadataDialogCallback callback,
    gpointer user_data
)
{
    EvidenceMetadataDialogContext *context = NULL;
    GtkStringList *labels = NULL; GtkWidget *main_box = NULL;
    GtkWidget *description_view = NULL; GtkWidget *description_scroll = NULL;
    GtkWidget *buttons = NULL; GtkWidget *cancel = NULL; GtkWidget *save = NULL;
    guint selected = 0U;
    if (record == NULL || evidence_types == NULL || evidence_types->len == 0U ||
        callback == NULL) return FALSE;
    context = g_new0(EvidenceMetadataDialogContext, 1);
    context->window = GTK_WINDOW(gtk_window_new());
    context->callback = callback; context->user_data = user_data;
    context->type_identifiers = g_ptr_array_new_with_free_func(g_free);
    labels = gtk_string_list_new(NULL);
    context->type_labels = labels;
    for (guint index = 0U; index < evidence_types->len; index++)
    {
        const EvidenceType *type = g_ptr_array_index((GPtrArray *) evidence_types, index);
        gtk_string_list_append(labels, evidence_type_get_label(type));
        g_ptr_array_add(context->type_identifiers,
            g_strdup(evidence_type_get_code(type)));
        if (g_strcmp0(evidence_type_get_code(type),
                evidence_record_get_type_identifier(record)) == 0) selected = index;
    }
    gtk_window_set_title(context->window, "Modifier les métadonnées");
    gtk_window_set_default_size(context->window, 560, 420);
    gtk_window_set_modal(context->window, TRUE);
    gtk_window_set_destroy_with_parent(context->window, TRUE);
    if (parent != NULL) gtk_window_set_transient_for(context->window, parent);
    g_object_set_data_full(G_OBJECT(context->window), "evidence-metadata-context",
        context, evidence_metadata_dialog_context_free);
    main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(main_box, 18); gtk_widget_set_margin_end(main_box, 18);
    gtk_widget_set_margin_top(main_box, 18); gtk_widget_set_margin_bottom(main_box, 18);
    gtk_box_append(GTK_BOX(main_box), gtk_label_new(
        "Le fichier, son UUID, sa taille, sa date d'import et son SHA-256 resteront inchangés."));
    context->type_dropdown = GTK_DROP_DOWN(gtk_drop_down_new(
        G_LIST_MODEL(labels), NULL));
    gtk_drop_down_set_selected(context->type_dropdown, selected);
    gtk_box_append(GTK_BOX(main_box), gtk_label_new("Type de preuve"));
    gtk_box_append(GTK_BOX(main_box), GTK_WIDGET(context->type_dropdown));
    context->source_entry = GTK_EDITABLE(gtk_entry_new());
    gtk_editable_set_text(context->source_entry,
        evidence_record_get_source(record) != NULL
            ? evidence_record_get_source(record) : "");
    gtk_box_append(GTK_BOX(main_box), gtk_label_new("Source"));
    gtk_box_append(GTK_BOX(main_box), GTK_WIDGET(context->source_entry));
    description_view = gtk_text_view_new();
    context->description_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(description_view));
    gtk_text_buffer_set_text(context->description_buffer,
        evidence_record_get_description(record) != NULL
            ? evidence_record_get_description(record) : "", -1);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(description_view), GTK_WRAP_WORD_CHAR);
    description_scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(description_scroll, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(description_scroll), description_view);
    gtk_box_append(GTK_BOX(main_box), gtk_label_new("Description"));
    gtk_box_append(GTK_BOX(main_box), description_scroll);
    buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(buttons, GTK_ALIGN_END);
    cancel = gtk_button_new_with_label("Annuler");
    save = gtk_button_new_with_label("Enregistrer");
    gtk_widget_add_css_class(save, "suggested-action");
    g_signal_connect(cancel, "clicked", G_CALLBACK(evidence_metadata_dialog_on_cancel), context);
    g_signal_connect(save, "clicked", G_CALLBACK(evidence_metadata_dialog_on_save), context);
    g_signal_connect(context->window, "close-request",
        G_CALLBACK(evidence_metadata_dialog_on_close_request), context);
    gtk_box_append(GTK_BOX(buttons), cancel); gtk_box_append(GTK_BOX(buttons), save);
    gtk_box_append(GTK_BOX(main_box), buttons);
    gtk_window_set_child(context->window, main_box);
    gtk_window_present(context->window);
    return TRUE;
}

void evidence_metadata_dialog_result_free(EvidenceMetadataDialogResult *result)
{
    if (result == NULL) return;
    g_free(result->type_identifier); g_free(result->source);
    g_free(result->description); g_free(result);
}
const char *evidence_metadata_dialog_result_get_type_identifier(
    const EvidenceMetadataDialogResult *result)
{ return result != NULL ? result->type_identifier : NULL; }
const char *evidence_metadata_dialog_result_get_source(
    const EvidenceMetadataDialogResult *result)
{ return result != NULL ? result->source : NULL; }
const char *evidence_metadata_dialog_result_get_description(
    const EvidenceMetadataDialogResult *result)
{ return result != NULL ? result->description : NULL; }
