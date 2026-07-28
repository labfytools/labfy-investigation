/******************************************************************************
 * @file create_person_dialog.c
 * @brief Formulaire GTK de création d'une personne observée.
 ******************************************************************************/
#include "views/create_person_dialog.h"
#include "core/evidence_preview_task.h"
#include "core/person_confirmation_summary.h"
#include "core/person_dialog_lifecycle.h"
#include "models/evidence_selection_model.h"

struct CreatePersonDialogResult
{
    PersonEntityInput input;
    char *designation;
    char *declared_name;
    char *pseudonym;
    char *status;
    char *notes;
    char *evidence_identifier;
    GPtrArray *role_assignments;
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
    GtkDropDown *type_filter;
    GtkEntry *search;
    GtkTextView *notes;
    GtkLabel *error;
    GtkLabel *metadata;
    GtkLabel *preview_status;
    GtkPicture *preview_picture;
    GtkLabel *progress;
    GtkLabel *summary;
    GtkStack *stack;
    GtkButton *previous;
    GtkButton *next;
    GtkButton *create;
    GtkCheckButton *roles[9];
    guint step;
    GPtrArray *evidence_identifiers;
    GPtrArray *visible_records;
    GPtrArray *type_codes;
    EvidenceSelectionModel *selection_model;
    char *investigation_root_path;
    TaskManager *task_manager;
    BackgroundTask *preview_task;
    PersonDialogLifecycle *lifecycle;
    CreatePersonDialogCallback callback;
    CreatePersonDialogSessionCheck session_check;
    gpointer user_data;
    GDestroyNotify user_data_destroy;
} CreatePersonDialogState;

typedef struct {
    GWeakRef window;
    char *evidence_identifier;
    guint64 generation;
} CreatePersonPreviewContext;
static const char *const status_codes[] = {"unknown", "suspected", "confirmed"};
static void create_person_dialog_clear_preview(CreatePersonDialogState *state);
static void create_person_dialog_select_record(CreatePersonDialogState *state,
    const EvidenceRecord *record);

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
    if (state->preview_task != NULL) {
        background_task_cancel(state->preview_task);
        background_task_unref(state->preview_task);
    }
    person_dialog_lifecycle_cancel(state->lifecycle);
    g_clear_pointer(&state->evidence_identifiers, g_ptr_array_unref);
    g_clear_pointer(&state->visible_records, g_ptr_array_unref);
    g_clear_pointer(&state->type_codes, g_ptr_array_unref);
    g_clear_pointer(&state->selection_model, evidence_selection_model_free);
    g_clear_pointer(&state->lifecycle, person_dialog_lifecycle_free);
    g_free(state->investigation_root_path);
    if (state->user_data_destroy != NULL)
        state->user_data_destroy(state->user_data);
    g_free(state);
}

static void create_person_dialog_rebuild_evidence(CreatePersonDialogState *state)
{
    GtkStringList *labels = gtk_string_list_new(NULL);
    const EvidenceRecord *selected =
        evidence_selection_model_get_selected(state->selection_model);
    const char *selected_id = selected != NULL
        ? evidence_record_get_identifier(selected) : NULL;
    guint selected_index = 0;
    gtk_string_list_append(labels, "Aucune preuve associée");
    g_clear_pointer(&state->visible_records, g_ptr_array_unref);
    state->visible_records =
        evidence_selection_model_list_visible(state->selection_model);
    for (guint i = 0; i < state->visible_records->len; i++) {
        EvidenceRecord *record = g_ptr_array_index(state->visible_records, i);
        gtk_string_list_append(labels,
            evidence_record_get_original_name(record));
        if (g_strcmp0(selected_id,
                evidence_record_get_identifier(record)) == 0)
            selected_index = i + 1;
    }
    gtk_drop_down_set_model(state->evidence, G_LIST_MODEL(labels));
    gtk_drop_down_set_selected(state->evidence, selected_index);
    g_object_unref(labels);
    if (selected_index == 0) {
        evidence_selection_model_select(state->selection_model, NULL);
        create_person_dialog_clear_preview(state);
    }
}

static void create_person_dialog_on_search_changed(
    GtkEditable *editable, gpointer data)
{
    CreatePersonDialogState *state = data;
    evidence_selection_model_set_query(state->selection_model,
        gtk_editable_get_text(editable));
    create_person_dialog_rebuild_evidence(state);
}

static void create_person_dialog_on_type_changed(
    GtkDropDown *dropdown, GParamSpec *pspec, gpointer data)
{
    CreatePersonDialogState *state = data;
    guint selected = gtk_drop_down_get_selected(dropdown);
    (void) pspec;
    evidence_selection_model_set_type(state->selection_model,
        selected > 0 && selected - 1 < state->type_codes->len
            ? g_ptr_array_index(state->type_codes, selected - 1) : NULL);
    create_person_dialog_rebuild_evidence(state);
}

static void create_person_dialog_on_evidence_changed(
    GtkDropDown *dropdown, GParamSpec *pspec, gpointer data)
{
    CreatePersonDialogState *state = data;
    guint selected = gtk_drop_down_get_selected(dropdown);
    (void) pspec;
    create_person_dialog_select_record(state,
        selected > 0 && selected - 1 < state->visible_records->len
            ? g_ptr_array_index(state->visible_records, selected - 1) : NULL);
}
/** @brief Termine le dialogue comme une annulation. */
static void create_person_dialog_cancel(CreatePersonDialogState *state)
{
    if (state == NULL ||
        !person_dialog_lifecycle_cancel(state->lifecycle)) return;
    if (state->preview_task != NULL) background_task_cancel(state->preview_task);
    if (state->callback != NULL) state->callback(NULL, state->user_data);
}

static void preview_context_free(gpointer data)
{
    CreatePersonPreviewContext *context = data;
    if (context == NULL) return;
    g_weak_ref_clear(&context->window);
    g_free(context->evidence_identifier);
    g_free(context);
}

static const char *integrity_label(EvidenceIntegrityStatus status)
{
    switch (status) {
        case EVIDENCE_INTEGRITY_STATUS_VALID: return "Valide";
        case EVIDENCE_INTEGRITY_STATUS_MISSING: return "Fichier absent";
        case EVIDENCE_INTEGRITY_STATUS_MODIFIED: return "Invalide";
        case EVIDENCE_INTEGRITY_STATUS_ERROR: return "Erreur de vérification";
        default: return "Non vérifiée";
    }
}

static void create_person_dialog_clear_preview(CreatePersonDialogState *state)
{
    if (state->preview_task != NULL) {
        background_task_cancel(state->preview_task);
        background_task_unref(state->preview_task);
        state->preview_task = NULL;
    }
    person_dialog_lifecycle_begin_preview(state->lifecycle);
    gtk_picture_set_paintable(state->preview_picture, NULL);
    gtk_label_set_text(state->preview_status, "Aucune preuve sélectionnée.");
    gtk_label_set_text(state->metadata, "");
}

static void create_person_dialog_preview_completed(
    BackgroundTask *task, gpointer data)
{
    CreatePersonPreviewContext *context = data;
    GtkWindow *window = g_weak_ref_get(&context->window);
    CreatePersonDialogState *state = window != NULL
        ? g_object_get_data(G_OBJECT(window), "person-dialog-state") : NULL;
    EvidencePreviewResult *result = background_task_get_result(task);
    if (state != NULL && state->session_check != NULL &&
        !state->session_check(state->user_data)) {
        person_dialog_lifecycle_cancel(state->lifecycle);
        gtk_label_set_text(state->preview_status,
            "Aperçu annulé : l'enquête active a changé.");
        gtk_widget_set_sensitive(GTK_WIDGET(state->create), FALSE);
    } else if (state != NULL &&
        person_dialog_lifecycle_accepts_preview(
            state->lifecycle, context->generation) &&
        evidence_preview_result_matches(result, context->evidence_identifier,
            context->generation)) {
        GError *error = NULL;
        GdkTexture *texture = gdk_texture_new_from_bytes(
            result->png_bytes, &error);
        if (texture != NULL) {
            gtk_picture_set_paintable(state->preview_picture,
                GDK_PAINTABLE(texture));
            gtk_label_set_text(state->preview_status, "Aperçu intègre.");
            g_object_unref(texture);
        } else {
            gtk_label_set_text(state->preview_status,
                error != NULL ? error->message : "Aperçu illisible.");
        }
        g_clear_error(&error);
    } else if (state != NULL &&
        person_dialog_lifecycle_accepts_preview(
            state->lifecycle, context->generation)) {
        GError *error = background_task_dup_error(task);
        gtk_label_set_text(state->preview_status,
            error != NULL ? error->message : "Aperçu annulé.");
        g_clear_error(&error);
    }
    if (state != NULL && state->preview_task == task) {
        background_task_unref(state->preview_task);
        state->preview_task = NULL;
    }
    g_clear_object(&window);
}

static void create_person_dialog_select_record(CreatePersonDialogState *state,
    const EvidenceRecord *record)
{
    char *size = NULL, *sha = NULL, *text = NULL;
    const char *mime;
    CreatePersonPreviewContext *context;
    EvidencePreviewRequest *request;
    GError *error = NULL;
    create_person_dialog_clear_preview(state);
    if (record == NULL) return;
    evidence_selection_model_select(state->selection_model,
        evidence_record_get_identifier(record));
    size = g_format_size(evidence_record_get_size_bytes(record));
    sha = g_strndup(evidence_record_get_sha256(record), 12);
    mime = evidence_record_get_mime_type(record);
    text = g_strdup_printf(
        "Nom : %s\nType : %s (%s)\nMIME : %s\nTaille : %s\n"
        "Import : %s\nSHA-256 : %s…\nIntégrité : %s\nDescription : %s",
        evidence_record_get_original_name(record),
        evidence_record_get_type_label(record),
        evidence_record_get_type_identifier(record),
        mime != NULL ? mime : "Non renseigné", size,
        evidence_record_get_imported_at(record), sha,
        integrity_label(evidence_record_get_integrity_status(record)),
        evidence_record_get_description(record) != NULL
            ? evidence_record_get_description(record) : "Aucune");
    gtk_label_set_text(state->metadata, text);
    if (mime == NULL || !(g_str_equal(mime, "image/png") ||
            g_str_equal(mime, "image/jpeg"))) {
        gtk_label_set_text(state->preview_status,
            "Aperçu indisponible pour ce format.");
        goto cleanup;
    }
    gtk_label_set_text(state->preview_status,
        "Vérification et chargement en cours…");
    request = evidence_preview_request_new(state->investigation_root_path,
        evidence_record_get_identifier(record),
        evidence_record_get_relative_path(record),
        evidence_record_get_sha256(record), mime,
        person_dialog_lifecycle_get_generation(state->lifecycle));
    context = g_new0(CreatePersonPreviewContext, 1);
    g_weak_ref_init(&context->window, G_OBJECT(state->window));
    context->evidence_identifier =
        g_strdup(evidence_record_get_identifier(record));
    context->generation =
        person_dialog_lifecycle_get_generation(state->lifecycle);
    state->preview_task = evidence_preview_task_start(state->task_manager,
        request, create_person_dialog_preview_completed, context,
        preview_context_free, &error);
    evidence_preview_request_free(request);
    if (state->preview_task == NULL) {
        preview_context_free(context);
        gtk_label_set_text(state->preview_status,
            error != NULL ? error->message : "Aperçu impossible.");
    }
    g_clear_error(&error);
cleanup:
    g_free(size); g_free(sha); g_free(text);
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
    const EvidenceRecord *selected_record =
        evidence_selection_model_get_selected(state->selection_model);
    (void) button;
    if (state->session_check != NULL &&
        !state->session_check(state->user_data)) {
        gtk_label_set_text(state->error,
            "L'enquête active a changé : création refusée.");
        gtk_widget_set_visible(GTK_WIDGET(state->error), TRUE);
        return;
    }
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
    if (selected_record != NULL)
        result->evidence_identifier = g_strdup(
            evidence_record_get_identifier(selected_record));
    result->input.designation = result->designation;
    result->input.declared_name = result->declared_name;
    result->input.pseudonym = result->pseudonym;
    result->input.identification_status = result->status;
    result->input.notes = result->notes;
    result->input.confidence = gtk_spin_button_get_value_as_int(state->confidence);
    result->input.evidence_identifier = result->evidence_identifier;
    result->role_assignments = g_ptr_array_new_with_free_func(
        (GDestroyNotify) person_role_assignment_input_free);
    static const char *const role_codes[] = {
        "alleged_author", "presented_identity",
        "potentially_impersonated_identity", "victim", "witness",
        "declared_bank_holder", "intermediary", "mentioned_person", "other"};
    for (guint i = 0; i < G_N_ELEMENTS(role_codes); i++)
        if (gtk_check_button_get_active(state->roles[i])) {
            PersonRoleAssignmentInput assignment = {
                .role_code = (char *) role_codes[i],
                .evidence_identifier = result->evidence_identifier,
                .provenance_kind = "manual",
                .has_confidence = FALSE
            };
            g_ptr_array_add(result->role_assignments,
                person_role_assignment_input_copy(&assignment));
        }
    result->input.role_assignments = result->role_assignments;
    if (!person_dialog_lifecycle_complete(state->lifecycle)) {
        create_person_dialog_result_free(result);
        g_free(notes);
        return;
    }
    if (state->callback != NULL) state->callback(result, state->user_data);
    else create_person_dialog_result_free(result);
    g_free(notes); gtk_window_close(state->window);
}

static void create_person_dialog_update_navigation(CreatePersonDialogState *state)
{
    static const char *const pages[] = {"person", "roles", "evidence", "summary"};
    static const char *const steps[] = {
        "1 Personne", "2 Rôles", "3 Preuve", "4 Confirmation"};
    GString *progress = g_string_new(NULL);
    gtk_stack_set_visible_child_name(state->stack, pages[state->step]);
    for (guint i = 0; i < G_N_ELEMENTS(steps); i++) {
        char *escaped = g_markup_escape_text(steps[i], -1);
        if (i > 0) g_string_append(progress, "  →  ");
        if (i == state->step)
            g_string_append_printf(progress, "<b>%s</b>", escaped);
        else
            g_string_append(progress, escaped);
        g_free(escaped);
    }
    gtk_label_set_markup(state->progress, progress->str);
    g_string_free(progress, TRUE);
    if (state->step == 3) {
        static const char *const status_labels[] = {
            "Inconnu", "Présumé", "Confirmé"};
        GPtrArray *role_labels = g_ptr_array_new();
        const EvidenceRecord *evidence =
            evidence_selection_model_get_selected(state->selection_model);
        char *notes = create_person_dialog_get_notes(state);
        char *text;
        for (guint i = 0; i < G_N_ELEMENTS(state->roles); i++)
            if (gtk_check_button_get_active(state->roles[i]))
                g_ptr_array_add(role_labels, (gpointer)
                    gtk_check_button_get_label(state->roles[i]));
        text = person_confirmation_summary_build(
            gtk_editable_get_text(GTK_EDITABLE(state->designation)),
            gtk_editable_get_text(GTK_EDITABLE(state->name)),
            gtk_editable_get_text(GTK_EDITABLE(state->pseudonym)),
            gtk_drop_down_get_selected(state->status) <
                G_N_ELEMENTS(status_labels)
                ? status_labels[gtk_drop_down_get_selected(state->status)]
                : "Non renseigné",
            gtk_spin_button_get_value_as_int(state->confidence), notes,
            role_labels, evidence);
        gtk_label_set_text(state->summary, text);
        g_free(text);
        g_free(notes);
        g_ptr_array_unref(role_labels);
    }
    gtk_widget_set_sensitive(GTK_WIDGET(state->previous), state->step > 0);
    gtk_widget_set_visible(GTK_WIDGET(state->next), state->step < 3);
    gtk_widget_set_visible(GTK_WIDGET(state->create), state->step == 3);
}
static void create_person_dialog_on_previous(GtkButton *button, gpointer data)
{
    CreatePersonDialogState *state = data; (void) button;
    if (state->step > 0) state->step--;
    create_person_dialog_update_navigation(state);
}
static void create_person_dialog_on_next(GtkButton *button, gpointer data)
{
    CreatePersonDialogState *state = data; (void) button;
    if (state->step == 0 &&
        gtk_editable_get_text(GTK_EDITABLE(state->designation))[0] == '\0') {
        gtk_label_set_text(state->error,
            "La désignation de la personne est obligatoire.");
        gtk_widget_set_visible(GTK_WIDGET(state->error), TRUE);
        return;
    }
    gtk_widget_set_visible(GTK_WIDGET(state->error), FALSE);
    if (state->step < 3) state->step++;
    create_person_dialog_update_navigation(state);
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
    const GPtrArray *records, const char *investigation_root_path,
    TaskManager *task_manager, CreatePersonDialogSessionCheck session_check,
    CreatePersonDialogCallback callback, gpointer data,
    GDestroyNotify data_destroy)
{
    static const char *const statuses[] = {"Inconnu", "Présumé", "Confirmé", NULL};
    CreatePersonDialogState *state = NULL;
    GtkWidget *box = NULL, *grid = NULL, *actions = NULL, *cancel = NULL;
    GtkWidget *roles_box = NULL, *evidence_box = NULL, *summary = NULL;
    GtkStringList *labels = NULL, *type_labels = NULL;
    if (parent == NULL || investigation_root_path == NULL ||
        task_manager == NULL) return FALSE;
    state = g_new0(CreatePersonDialogState, 1);
    state->callback = callback; state->session_check = session_check;
    state->user_data = data; state->user_data_destroy = data_destroy;
    state->evidence_identifiers = g_ptr_array_new_with_free_func(g_free);
    state->selection_model = evidence_selection_model_new(records);
    state->visible_records =
        evidence_selection_model_list_visible(state->selection_model);
    state->type_codes =
        evidence_selection_model_list_type_codes(state->selection_model);
    state->investigation_root_path = g_strdup(investigation_root_path);
    state->task_manager = task_manager;
    state->lifecycle = person_dialog_lifecycle_new();
    state->window = GTK_WINDOW(gtk_window_new());
    gtk_window_set_application(state->window,
        gtk_window_get_application(parent));
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
    labels = gtk_string_list_new(NULL);
    gtk_string_list_append(labels, "Aucune preuve associée");
    for (guint i = 0; i < state->visible_records->len; i++)
    {
        EvidenceRecord *record = g_ptr_array_index(state->visible_records, i);
        const char *id = evidence_record_get_identifier(record);
        const char *name = evidence_record_get_original_name(record);
        if (id == NULL || name == NULL) continue;
        gtk_string_list_append(labels, name);
        g_ptr_array_add(state->evidence_identifiers, g_strdup(id));
    }
    state->evidence = GTK_DROP_DOWN(gtk_drop_down_new(G_LIST_MODEL(labels), NULL));
    labels = NULL;
    state->search = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(state->search,
        "Rechercher par nom, description ou type");
    type_labels = gtk_string_list_new(NULL);
    gtk_string_list_append(type_labels, "Tous les types");
    for (guint i = 0; i < state->type_codes->len; i++)
        gtk_string_list_append(type_labels,
            g_ptr_array_index(state->type_codes, i));
    state->type_filter = GTK_DROP_DOWN(
        gtk_drop_down_new(G_LIST_MODEL(type_labels), NULL));
    /*
     * gtk_drop_down_new() prend la propriété complète du modèle. Une
     * libération ici laisserait les vues internes du GtkDropDown avec un
     * GListModel détruit et ferait échouer leur nettoyage à la fermeture.
     */
    type_labels = NULL;
    state->notes = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_wrap_mode(state->notes, GTK_WRAP_WORD_CHAR);
    gtk_widget_set_size_request(GTK_WIDGET(state->notes), -1, 90);
    create_person_dialog_add_row(GTK_GRID(grid), 0, "Désignation", GTK_WIDGET(state->designation));
    create_person_dialog_add_row(GTK_GRID(grid), 1, "Nom déclaré", GTK_WIDGET(state->name));
    create_person_dialog_add_row(GTK_GRID(grid), 2, "Pseudonyme", GTK_WIDGET(state->pseudonym));
    create_person_dialog_add_row(GTK_GRID(grid), 3, "Identification", GTK_WIDGET(state->status));
    create_person_dialog_add_row(GTK_GRID(grid), 4, "Confiance (%)", GTK_WIDGET(state->confidence));
    create_person_dialog_add_row(GTK_GRID(grid), 5, "Notes factuelles", GTK_WIDGET(state->notes));
    state->stack = GTK_STACK(gtk_stack_new());
    gtk_stack_set_transition_type(state->stack,
        GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_add_titled(state->stack, grid, "person", "1 — Personne");
    roles_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    static const char *const role_codes[] = {
        "alleged_author", "presented_identity",
        "potentially_impersonated_identity", "victim", "witness",
        "declared_bank_holder", "intermediary", "mentioned_person", "other"};
    for (guint i = 0; i < G_N_ELEMENTS(role_codes); i++) {
        state->roles[i] = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(
            person_role_assignment_role_label(role_codes[i])));
        gtk_box_append(GTK_BOX(roles_box), GTK_WIDGET(state->roles[i]));
    }
    gtk_stack_add_titled(state->stack, roles_box, "roles", "2 — Rôles");
    evidence_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_box_append(GTK_BOX(evidence_box), gtk_label_new(
        "Sélectionnez une preuve existante (aucun import dans cette version)."));
    gtk_box_append(GTK_BOX(evidence_box), GTK_WIDGET(state->search));
    gtk_box_append(GTK_BOX(evidence_box), GTK_WIDGET(state->type_filter));
    gtk_box_append(GTK_BOX(evidence_box), GTK_WIDGET(state->evidence));
    state->metadata = GTK_LABEL(gtk_label_new(""));
    gtk_label_set_xalign(state->metadata, 0.0f);
    gtk_label_set_selectable(state->metadata, TRUE);
    state->preview_status = GTK_LABEL(gtk_label_new(
        "Aucune preuve sélectionnée."));
    state->preview_picture = GTK_PICTURE(gtk_picture_new());
    gtk_picture_set_can_shrink(state->preview_picture, TRUE);
    gtk_picture_set_content_fit(state->preview_picture,
        GTK_CONTENT_FIT_CONTAIN);
    gtk_widget_set_size_request(GTK_WIDGET(state->preview_picture), 480, 260);
    gtk_box_append(GTK_BOX(evidence_box), GTK_WIDGET(state->metadata));
    gtk_box_append(GTK_BOX(evidence_box), GTK_WIDGET(state->preview_status));
    gtk_box_append(GTK_BOX(evidence_box), GTK_WIDGET(state->preview_picture));
    gtk_stack_add_titled(state->stack, evidence_box, "evidence", "3 — Preuve");
    state->summary = GTK_LABEL(gtk_label_new(""));
    summary = GTK_WIDGET(state->summary);
    gtk_label_set_wrap(state->summary, TRUE);
    gtk_label_set_xalign(state->summary, 0.0f);
    gtk_label_set_selectable(state->summary, TRUE);
    gtk_stack_add_titled(state->stack, summary, "summary", "4 — Confirmation");
    state->error = GTK_LABEL(gtk_label_new(NULL)); gtk_widget_set_visible(GTK_WIDGET(state->error), FALSE);
    actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8); gtk_widget_set_halign(actions, GTK_ALIGN_END);
    cancel = gtk_button_new_with_label("Annuler");
    state->previous = GTK_BUTTON(gtk_button_new_with_label("Précédent"));
    state->next = GTK_BUTTON(gtk_button_new_with_label("Suivant"));
    state->create = GTK_BUTTON(gtk_button_new_with_label("Créer la personne"));
    gtk_widget_add_css_class(GTK_WIDGET(state->create), "suggested-action");
    gtk_box_append(GTK_BOX(actions), cancel);
    gtk_box_append(GTK_BOX(actions), GTK_WIDGET(state->previous));
    gtk_box_append(GTK_BOX(actions), GTK_WIDGET(state->next));
    gtk_box_append(GTK_BOX(actions), GTK_WIDGET(state->create));
    state->progress = GTK_LABEL(gtk_label_new(NULL));
    gtk_label_set_xalign(state->progress, 0.0f);
    gtk_box_append(GTK_BOX(box), GTK_WIDGET(state->progress));
    gtk_box_append(GTK_BOX(box), GTK_WIDGET(state->stack));
    gtk_box_append(GTK_BOX(box), GTK_WIDGET(state->error));
    gtk_box_append(GTK_BOX(box), actions); gtk_window_set_child(state->window, box);
    g_signal_connect(state->window, "close-request", G_CALLBACK(create_person_dialog_on_close), state);
    g_signal_connect(cancel, "clicked", G_CALLBACK(create_person_dialog_on_cancel), state);
    g_signal_connect(state->previous, "clicked",
        G_CALLBACK(create_person_dialog_on_previous), state);
    g_signal_connect(state->next, "clicked",
        G_CALLBACK(create_person_dialog_on_next), state);
    g_signal_connect(state->create, "clicked",
        G_CALLBACK(create_person_dialog_on_create), state);
    g_signal_connect(state->search, "changed",
        G_CALLBACK(create_person_dialog_on_search_changed), state);
    g_signal_connect(state->type_filter, "notify::selected",
        G_CALLBACK(create_person_dialog_on_type_changed), state);
    g_signal_connect(state->evidence, "notify::selected",
        G_CALLBACK(create_person_dialog_on_evidence_changed), state);
    create_person_dialog_update_navigation(state);
    g_object_set_data_full(G_OBJECT(state->window), "person-dialog-state", state,
        create_person_dialog_state_free);
    gtk_window_present(state->window); return TRUE;
}
void create_person_dialog_result_free(CreatePersonDialogResult *result)
{
    if (result == NULL) return;
    g_free(result->designation); g_free(result->declared_name);
    g_free(result->pseudonym); g_free(result->status); g_free(result->notes);
    g_free(result->evidence_identifier);
    g_clear_pointer(&result->role_assignments, g_ptr_array_unref);
    g_free(result);
}
const PersonEntityInput *create_person_dialog_result_get_input(
    const CreatePersonDialogResult *result)
{
    return result != NULL ? &result->input : NULL;
}
