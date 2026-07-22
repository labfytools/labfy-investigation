/******************************************************************************
 * @file create_person_dialog.c
 * @brief Formulaire GTK de création d'une personne observée.
 ******************************************************************************/
#include "views/create_person_dialog.h"

struct CreatePersonDialogResult
{
    PersonEntityInput input;
    char *designation;
    char *declared_name;
    char *pseudonym;
    char *status;
    char *notes;
    char *evidence_identifier;
};
typedef struct
{
    GtkWindow *window;
    GtkEntry *designation;
    GtkEntry *name;
    GtkEntry *pseudonym;
    GtkDropDown *status;
    GtkSpinButton *confidence;
    GtkDropDown *evidence;
    GtkTextView *notes;
    GtkLabel *error;
    GPtrArray *evidence_identifiers;
    CreatePersonDialogCallback callback;
    gpointer user_data;
    gboolean completed;
} CreatePersonDialogState;
static const char *const status_codes[] = {"unknown", "suspected", "confirmed"};

/** @brief Copie et nettoie un texte facultatif. */
static char *create_person_dialog_copy(const char *text)
{
    char *copy = text != NULL ? g_strdup(text) : NULL;
    if (copy == NULL) return NULL;
    g_strstrip(copy);
    if (copy[0] == '\0') { g_free(copy); return NULL; }
    return copy;
}
/** @brief Extrait les notes de la zone de texte. */
static char *create_person_dialog_get_notes(CreatePersonDialogState *state)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(state->notes);
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    return gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
}
/** @brief Libère l'état attaché à la fenêtre. */
static void create_person_dialog_state_free(gpointer data)
{
    CreatePersonDialogState *state = data;
    if (state == NULL) return;
    g_clear_pointer(&state->evidence_identifiers, g_ptr_array_unref);
    g_free(state);
}
/** @brief Termine le dialogue comme une annulation. */
static void create_person_dialog_cancel(CreatePersonDialogState *state)
{
    if (state == NULL || state->completed) return;
    state->completed = TRUE;
    if (state->callback != NULL) state->callback(NULL, state->user_data);
}
/** @brief Traite la fermeture native. */
static gboolean create_person_dialog_on_close(GtkWindow *window, gpointer data)
{
    (void) window; create_person_dialog_cancel(data); return FALSE;
}
/** @brief Traite le bouton Annuler. */
static void create_person_dialog_on_cancel(GtkButton *button, gpointer data)
{
    CreatePersonDialogState *state = data;
    (void) button; create_person_dialog_cancel(state); gtk_window_close(state->window);
}
/** @brief Valide puis transmet les valeurs du formulaire. */
static void create_person_dialog_on_create(GtkButton *button, gpointer data)
{
    CreatePersonDialogState *state = data;
    CreatePersonDialogResult *result = NULL;
    char *notes = NULL;
    guint status = gtk_drop_down_get_selected(state->status);
    guint evidence = gtk_drop_down_get_selected(state->evidence);
    (void) button;
    if (status >= G_N_ELEMENTS(status_codes) ||
        gtk_editable_get_text(GTK_EDITABLE(state->designation))[0] == '\0')
    {
        gtk_label_set_text(state->error, "La désignation de la personne est obligatoire.");
        gtk_widget_set_visible(GTK_WIDGET(state->error), TRUE); return;
    }
    notes = create_person_dialog_get_notes(state);
    result = g_new0(CreatePersonDialogResult, 1);
    result->designation = create_person_dialog_copy(
        gtk_editable_get_text(GTK_EDITABLE(state->designation)));
    result->declared_name = create_person_dialog_copy(
        gtk_editable_get_text(GTK_EDITABLE(state->name)));
    result->pseudonym = create_person_dialog_copy(
        gtk_editable_get_text(GTK_EDITABLE(state->pseudonym)));
    result->status = g_strdup(status_codes[status]);
    result->notes = create_person_dialog_copy(notes);
    if (evidence > 0 && evidence - 1 < state->evidence_identifiers->len)
        result->evidence_identifier = g_strdup(g_ptr_array_index(
            state->evidence_identifiers, evidence - 1));
    result->input.designation = result->designation;
    result->input.declared_name = result->declared_name;
    result->input.pseudonym = result->pseudonym;
    result->input.identification_status = result->status;
    result->input.notes = result->notes;
    result->input.confidence = gtk_spin_button_get_value_as_int(state->confidence);
    result->input.evidence_identifier = result->evidence_identifier;
    state->completed = TRUE;
    if (state->callback != NULL) state->callback(result, state->user_data);
    else create_person_dialog_result_free(result);
    g_free(notes); gtk_window_close(state->window);
}
/** @brief Ajoute une ligne libellée au formulaire. */
static void create_person_dialog_add_row(GtkGrid *grid, int row,
    const char *label, GtkWidget *widget)
{
    GtkWidget *caption = gtk_label_new(label);
    gtk_label_set_xalign(GTK_LABEL(caption), 0.0f);
    gtk_widget_set_hexpand(widget, TRUE);
    gtk_grid_attach(grid, caption, 0, row, 1, 1);
    gtk_grid_attach(grid, widget, 1, row, 1, 1);
}
gboolean create_person_dialog_present(GtkWindow *parent,
    const GPtrArray *records, CreatePersonDialogCallback callback, gpointer data)
{
    static const char *const statuses[] = {"Inconnu", "Présumé", "Confirmé", NULL};
    CreatePersonDialogState *state = NULL;
    GtkWidget *box = NULL, *grid = NULL, *actions = NULL, *cancel = NULL, *create = NULL;
    GtkStringList *labels = NULL;
    if (parent == NULL) return FALSE;
    state = g_new0(CreatePersonDialogState, 1);
    state->callback = callback; state->user_data = data;
    state->evidence_identifiers = g_ptr_array_new_with_free_func(g_free);
    state->window = GTK_WINDOW(gtk_window_new());
    gtk_window_set_title(state->window, "Ajouter une personne");
    gtk_window_set_transient_for(state->window, parent);
    gtk_window_set_modal(state->window, TRUE);
    gtk_window_set_default_size(state->window, 580, 500);
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(box, 16); gtk_widget_set_margin_end(box, 16);
    gtk_widget_set_margin_top(box, 16); gtk_widget_set_margin_bottom(box, 16);
    grid = gtk_grid_new(); gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    state->designation = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(state->designation, "Personne présumée liée aux comptes");
    state->name = GTK_ENTRY(gtk_entry_new());
    state->pseudonym = GTK_ENTRY(gtk_entry_new());
    state->status = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(statuses));
    gtk_drop_down_set_selected(state->status, 1);
    state->confidence = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(0, 100, 5));
    gtk_spin_button_set_value(state->confidence, 30);
    labels = gtk_string_list_new(NULL); gtk_string_list_append(labels, "Aucune preuve associée");
    for (guint i = 0; records != NULL && i < records->len; i++)
    {
        EvidenceRecord *record = g_ptr_array_index((GPtrArray *) records, i);
        const char *id = evidence_record_get_identifier(record);
        const char *name = evidence_record_get_original_name(record);
        if (id == NULL || name == NULL) continue;
        gtk_string_list_append(labels, name);
        g_ptr_array_add(state->evidence_identifiers, g_strdup(id));
    }
    state->evidence = GTK_DROP_DOWN(gtk_drop_down_new(G_LIST_MODEL(labels), NULL));
    labels = NULL;
    state->notes = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_wrap_mode(state->notes, GTK_WRAP_WORD_CHAR);
    gtk_widget_set_size_request(GTK_WIDGET(state->notes), -1, 90);
    create_person_dialog_add_row(GTK_GRID(grid), 0, "Désignation", GTK_WIDGET(state->designation));
    create_person_dialog_add_row(GTK_GRID(grid), 1, "Nom déclaré", GTK_WIDGET(state->name));
    create_person_dialog_add_row(GTK_GRID(grid), 2, "Pseudonyme", GTK_WIDGET(state->pseudonym));
    create_person_dialog_add_row(GTK_GRID(grid), 3, "Identification", GTK_WIDGET(state->status));
    create_person_dialog_add_row(GTK_GRID(grid), 4, "Confiance (%)", GTK_WIDGET(state->confidence));
    create_person_dialog_add_row(GTK_GRID(grid), 5, "Preuve associée", GTK_WIDGET(state->evidence));
    create_person_dialog_add_row(GTK_GRID(grid), 6, "Notes factuelles", GTK_WIDGET(state->notes));
    state->error = GTK_LABEL(gtk_label_new(NULL)); gtk_widget_set_visible(GTK_WIDGET(state->error), FALSE);
    actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8); gtk_widget_set_halign(actions, GTK_ALIGN_END);
    cancel = gtk_button_new_with_label("Annuler"); create = gtk_button_new_with_label("Ajouter au graphe");
    gtk_widget_add_css_class(create, "suggested-action");
    gtk_box_append(GTK_BOX(actions), cancel); gtk_box_append(GTK_BOX(actions), create);
    gtk_box_append(GTK_BOX(box), grid); gtk_box_append(GTK_BOX(box), GTK_WIDGET(state->error));
    gtk_box_append(GTK_BOX(box), actions); gtk_window_set_child(state->window, box);
    g_signal_connect(state->window, "close-request", G_CALLBACK(create_person_dialog_on_close), state);
    g_signal_connect(cancel, "clicked", G_CALLBACK(create_person_dialog_on_cancel), state);
    g_signal_connect(create, "clicked", G_CALLBACK(create_person_dialog_on_create), state);
    g_object_set_data_full(G_OBJECT(state->window), "person-dialog-state", state,
        create_person_dialog_state_free);
    gtk_window_present(state->window); return TRUE;
}
void create_person_dialog_result_free(CreatePersonDialogResult *result)
{
    if (result == NULL) return;
    g_free(result->designation); g_free(result->declared_name);
    g_free(result->pseudonym); g_free(result->status); g_free(result->notes);
    g_free(result->evidence_identifier); g_free(result);
}
const PersonEntityInput *create_person_dialog_result_get_input(
    const CreatePersonDialogResult *result)
{
    return result != NULL ? &result->input : NULL;
}
