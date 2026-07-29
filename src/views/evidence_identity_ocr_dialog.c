#include "views/evidence_identity_ocr_dialog.h"

#include "core/file_hash.h"
#include "core/identity_ocr_workflow.h"
#include "core/ocr_analysis.h"
#include "widgets/ocr_provenance_overlay.h"

#include <string.h>

struct EvidenceIdentityOcrDialogResult {
    GtkWindow *dialog;
    char *person_identifier;
    char *temporary_identifier;
    char *sha256;
    char *mime_type;
    guint64 size;
    IdentityOcrRun *run;
};

typedef struct {
    char *root;
    char *relative;
    char *identifier;
    char *sha256;
    char *mime;
    char *type;
    char *side;
    char *languages;
    char *profile;
    char *executable;
    char *version;
    guint page;
    guint profile_index;
    guint64 generation;
    GWeakRef window;
} EvidenceIdentityOcrJob;

typedef struct {
    GtkWindow *window;
    GtkDropDown *person;
    GtkDropDown *type;
    GtkDropDown *side;
    GtkSpinButton *page;
    GtkDropDown *languages;
    GtkDropDown *profile;
    GtkButton *start;
    GtkButton *continue_with_ocr;
    GtkLabel *status;
    GtkTextView *raw_text;
    GtkTextView *corrected_text;
    GtkButton *save_transcription;
    GtkTextView *notes;
    GtkBox *fields;
    OcrProvenanceOverlay *overlay;
    GPtrArray *person_identifiers;
    GPtrArray *language_codes;
    char *file_path;
    char *temporary_identifier;
    char *sha256;
    char *mime_type;
    guint64 size;
    char *tesseract_path;
    char *tesseract_version;
    IdentityOcrRun *run;
    GTask *task;
    BackgroundTask *submission_task;
    GCancellable *cancellable;
    guint64 generation;
    EvidenceIdentityOcrDialogSessionCheck session_check;
    EvidenceIdentityOcrDialogCallback callback;
    gpointer user_data;
    GDestroyNotify user_data_destroy;
    gboolean completed;
    gboolean submission_pending;
    gboolean review_mode;
} EvidenceIdentityOcrDialogState;

static const char *const document_types[] = {
    "identity_card", "passport", "driving_licence",
    "residence_permit", "other"};
static const char *const document_sides[] = {
    "front", "back", "identity_page", "other_page"};
static const char *const profiles[] = {
    "none", "orientation", "grayscale", "upscale"};

static char *transcription_text(GtkTextView *view)
{
    GtkTextIter start, end;
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(view);
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    return gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
}

static void on_edit_transcription(GtkButton *button, gpointer data)
{
    EvidenceIdentityOcrDialogState *state = data;
    (void) button;
    gtk_text_view_set_editable(state->corrected_text, TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(state->save_transcription), TRUE);
}

static void on_save_transcription(GtkButton *button, gpointer data)
{
    EvidenceIdentityOcrDialogState *state = data;
    char *text = transcription_text(state->corrected_text);
    GDateTime *now = g_date_time_new_now_utc();
    char *timestamp = g_date_time_format(now, "%Y-%m-%dT%H:%M:%SZ");
    (void) button;
    if (identity_ocr_run_set_corrected_transcription(
            state->run, text, timestamp)) {
        gtk_text_view_set_editable(state->corrected_text, FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(state->save_transcription), FALSE);
    }
    g_date_time_unref(now);
    g_free(timestamp);
    g_free(text);
}

static void on_reset_transcription(GtkButton *button, gpointer data)
{
    EvidenceIdentityOcrDialogState *state = data;
    const char *raw = state->run != NULL
        ? identity_ocr_run_get_raw_text(state->run) : "";
    (void) button;
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(state->corrected_text),
        raw != NULL ? raw : "", -1);
    identity_ocr_run_reset_corrected_transcription(state->run);
    gtk_text_view_set_editable(state->corrected_text, TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(state->save_transcription), TRUE);
}

static gboolean set_initial_paned_position(gpointer data)
{
    GtkWidget *paned = data;
    int width;
    int position;
    if (g_object_get_data(
            G_OBJECT(paned), "identity-initial-position-set") != NULL)
        return G_SOURCE_REMOVE;
    width = gtk_widget_get_width(paned);
    if (width <= 0) return G_SOURCE_CONTINUE;
    position = MAX(520, (width * 2) / 3);
    position = MIN(position, MAX(0, width - 180));
    gtk_paned_set_position(GTK_PANED(paned), position);
    g_object_set_data(G_OBJECT(paned),
        "identity-initial-position-set", GINT_TO_POINTER(1));
    return G_SOURCE_REMOVE;
}

static const char *mime_from_path(const char *path)
{
    const char *dot = path != NULL ? strrchr(path, '.') : NULL;
    if (dot == NULL) return NULL;
    if (g_ascii_strcasecmp(dot, ".png") == 0) return "image/png";
    if (g_ascii_strcasecmp(dot, ".jpg") == 0 ||
        g_ascii_strcasecmp(dot, ".jpeg") == 0) return "image/jpeg";
    if (g_ascii_strcasecmp(dot, ".heic") == 0) return "image/heic";
    if (g_ascii_strcasecmp(dot, ".heif") == 0) return "image/heif";
    if (g_ascii_strcasecmp(dot, ".pdf") == 0) return "application/pdf";
    return NULL;
}

gboolean evidence_identity_ocr_dialog_file_is_compatible(
    const char *file_path)
{
    return mime_from_path(file_path) != NULL;
}

static void evidence_identity_ocr_job_free(gpointer data)
{
    EvidenceIdentityOcrJob *job = data;
    if (job == NULL) return;
    g_free(job->root); g_free(job->relative); g_free(job->identifier);
    g_free(job->sha256); g_free(job->mime); g_free(job->type);
    g_free(job->side); g_free(job->languages); g_free(job->profile);
    g_free(job->executable); g_free(job->version);
    g_weak_ref_clear(&job->window);
    g_free(job);
}

static void evidence_identity_ocr_dialog_state_free(gpointer data)
{
    EvidenceIdentityOcrDialogState *state = data;
    if (state == NULL) return;
    state->generation++;
    if (state->cancellable != NULL) g_cancellable_cancel(state->cancellable);
    g_clear_object(&state->task);
    if (state->submission_task != NULL) {
        background_task_cancel(state->submission_task);
        background_task_unref(state->submission_task);
    }
    g_clear_object(&state->cancellable);
    identity_ocr_run_free(state->run);
    g_clear_pointer(&state->overlay, ocr_provenance_overlay_free);
    g_clear_pointer(&state->person_identifiers, g_ptr_array_unref);
    g_clear_pointer(&state->language_codes, g_ptr_array_unref);
    g_free(state->file_path); g_free(state->temporary_identifier);
    g_free(state->sha256); g_free(state->mime_type);
    g_free(state->tesseract_path); g_free(state->tesseract_version);
    if (state->user_data_destroy != NULL)
        state->user_data_destroy(state->user_data);
    g_free(state);
}

static void evidence_identity_ocr_dialog_complete(
    EvidenceIdentityOcrDialogState *state,
    EvidenceIdentityOcrDialogResult *result)
{
    if (state == NULL || state->completed) {
        evidence_identity_ocr_dialog_result_free(result);
        return;
    }
    state->completed = TRUE;
    if (state->cancellable != NULL) g_cancellable_cancel(state->cancellable);
    gtk_widget_set_visible(GTK_WIDGET(state->window), FALSE);
    if (state->callback != NULL) state->callback(result, state->user_data);
    else evidence_identity_ocr_dialog_result_free(result);
    gtk_window_destroy(state->window);
}

static EvidenceIdentityOcrDialogResult *build_result(
    EvidenceIdentityOcrDialogState *state, gboolean with_ocr)
{
    EvidenceIdentityOcrDialogResult *result =
        g_new0(EvidenceIdentityOcrDialogResult, 1);
    result->dialog = g_object_ref(state->window);
    guint selected = gtk_drop_down_get_selected(state->person);
    if (with_ocr && selected > 0 &&
        selected - 1 < state->person_identifiers->len)
        result->person_identifier = g_strdup(g_ptr_array_index(
            state->person_identifiers, selected - 1));
    result->temporary_identifier =
        g_strdup(state->temporary_identifier);
    result->sha256 = g_strdup(state->sha256);
    result->mime_type = g_strdup(state->mime_type);
    result->size = state->size;
    if (with_ocr) {
        char *corrected = transcription_text(state->corrected_text);
        if (g_strcmp0(corrected,
                identity_ocr_run_get_raw_text(state->run)) == 0)
            identity_ocr_run_reset_corrected_transcription(state->run);
        else {
            GDateTime *now = g_date_time_new_now_utc();
            char *timestamp = g_date_time_format(
                now, "%Y-%m-%dT%H:%M:%SZ");
            identity_ocr_run_set_corrected_transcription(
                state->run, corrected, timestamp);
            g_free(timestamp);
            g_date_time_unref(now);
        }
        g_free(corrected);
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(state->notes);
        GtkTextIter start, end;
        gtk_text_buffer_get_bounds(buffer, &start, &end);
        char *notes = gtk_text_buffer_get_text(
            buffer, &start, &end, FALSE);
        identity_ocr_run_set_factual_notes(
            state->run, notes[0] != '\0' ? notes : NULL);
        g_free(notes);
        result->run = state->run;
        state->run = NULL;
    }
    return result;
}

static void on_cancel(GtkButton *button, gpointer data)
{
    (void) button;
    evidence_identity_ocr_dialog_complete(data, NULL);
}

static gboolean on_close(GtkWindow *window, gpointer data)
{
    EvidenceIdentityOcrDialogState *state = data;
    (void) window;
    if (state->completed) return FALSE;
    evidence_identity_ocr_dialog_complete(state, NULL);
    return TRUE;
}

static void on_without_ocr(GtkButton *button, gpointer data)
{
    EvidenceIdentityOcrDialogState *state = data;
    (void) button;
    evidence_identity_ocr_dialog_complete(state, build_result(state, FALSE));
}

static void on_continue_with_ocr(GtkButton *button, gpointer data)
{
    EvidenceIdentityOcrDialogState *state = data;
    guint selected = gtk_drop_down_get_selected(state->person);
    (void) button;
    if (state->submission_pending) return;
    if (state->session_check != NULL &&
        !state->session_check(state->user_data)) {
        gtk_label_set_text(state->status,
            "La session d’enquête a changé. Relancez l’analyse.");
        return;
    }
    if (state->run == NULL) {
        gtk_label_set_text(state->status,
            "Aucun résultat OCR valide n’est disponible.");
        return;
    }
    if (selected == 0 || selected == GTK_INVALID_LIST_POSITION) {
        gtk_label_set_text(state->status,
            "Sélectionnez une personne avant de poursuivre.");
        return;
    }
    state->submission_pending = TRUE;
    gtk_widget_set_sensitive(GTK_WIDGET(state->continue_with_ocr), FALSE);
    gtk_label_set_text(state->status, state->review_mode
        ? "Enregistrement de la révision…"
        : "Import OCR en cours…");
    if (state->callback != NULL)
        state->callback(build_result(state, TRUE), state->user_data);
    else {
        state->submission_pending = FALSE;
        gtk_widget_set_sensitive(
            GTK_WIDGET(state->continue_with_ocr), TRUE);
        gtk_label_set_text(state->status,
            "Impossible de poursuivre : aucun gestionnaire d’import.");
    }
}

static void on_field_accept(GtkButton *button, gpointer data)
{
    IdentityFieldObservation *field = g_object_get_data(
        G_OBJECT(button), "field");
    GtkEntry *entry = g_object_get_data(G_OBJECT(button), "entry");
    (void) data;
    identity_field_observation_accept(field);
    gtk_button_set_label(button, "Acceptée");
    gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
}

static void on_field_modify(GtkButton *button, gpointer data)
{
    IdentityFieldObservation *field = g_object_get_data(
        G_OBJECT(button), "field");
    GtkEntry *entry = g_object_get_data(G_OBJECT(button), "entry");
    (void) data;
    if (!identity_field_observation_modify(field,
            gtk_editable_get_text(GTK_EDITABLE(entry)),
            "Valeur revue individuellement pendant l’import.")) return;
    gtk_button_set_label(button, "Modifiée");
    gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
    gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
}

static void on_field_reedit(GtkButton *button, gpointer data)
{
    GtkEntry *entry = g_object_get_data(G_OBJECT(button), "entry");
    GtkButton *modify = g_object_get_data(G_OBJECT(button), "modify");
    (void) data;
    gtk_editable_set_editable(GTK_EDITABLE(entry), TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(modify), TRUE);
    gtk_widget_grab_focus(GTK_WIDGET(entry));
}

static void on_field_restore(GtkButton *button, gpointer data)
{
    IdentityFieldObservation *field = g_object_get_data(
        G_OBJECT(button), "field");
    GtkEntry *entry = g_object_get_data(G_OBJECT(button), "entry");
    GtkButton *modify = g_object_get_data(G_OBJECT(button), "modify");
    (void) data;
    if (!identity_field_observation_restore_raw(field)) return;
    gtk_editable_set_text(GTK_EDITABLE(entry),
        identity_field_observation_get_raw_value(field));
    gtk_editable_set_editable(GTK_EDITABLE(entry), TRUE);
    gtk_button_set_label(modify, "Modifier");
    gtk_widget_set_sensitive(GTK_WIDGET(modify), TRUE);
}

static void on_field_reject(GtkButton *button, gpointer data)
{
    IdentityFieldObservation *field = g_object_get_data(
        G_OBJECT(button), "field");
    (void) data;
    identity_field_observation_reject(field);
    gtk_button_set_label(button, "Rejetée");
}

static void on_field_show(GtkButton *button, gpointer data)
{
    EvidenceIdentityOcrDialogState *state = data;
    IdentityFieldObservation *field = g_object_get_data(
        G_OBJECT(button), "field");
    ocr_provenance_overlay_set_field(
        state->overlay, field, state->generation);
}

static void render_fields(EvidenceIdentityOcrDialogState *state)
{
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(
            GTK_WIDGET(state->fields))) != NULL)
        gtk_box_remove(state->fields, child);
    const GPtrArray *fields = identity_ocr_run_get_fields(state->run);
    for (guint index = 0; fields != NULL && index < fields->len; index++) {
        IdentityFieldObservation *field =
            g_ptr_array_index((GPtrArray *) fields, index);
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        char *description = g_strdup_printf(
            "%s — OCR brut : %s — origine : %s",
            identity_field_observation_get_code(field),
            identity_field_observation_get_raw_value(field) != NULL
                ? identity_field_observation_get_raw_value(field) : "absente",
            identity_field_observation_get_origin(field));
        GtkWidget *label = gtk_label_new(description);
        GtkWidget *entry = gtk_entry_new();
        GtkWidget *actions = gtk_flow_box_new();
        GtkWidget *show = gtk_button_new_with_label("Voir la zone");
        GtkWidget *accept = gtk_button_new_with_label("Accepter");
        GtkWidget *modify = gtk_button_new_with_label("Modifier");
        GtkWidget *reedit = gtk_button_new_with_label("Modifier à nouveau");
        GtkWidget *restore =
            gtk_button_new_with_label("Revenir à la valeur OCR");
        GtkWidget *reject = gtk_button_new_with_label("Rejeter");
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_label_set_wrap(GTK_LABEL(label), TRUE);
        gtk_label_set_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
        gtk_widget_set_hexpand(entry, TRUE);
        gtk_flow_box_set_selection_mode(
            GTK_FLOW_BOX(actions), GTK_SELECTION_NONE);
        gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(actions), 3);
        gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(actions), 1);
        gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(actions), 4);
        gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(actions), 4);
        gtk_editable_set_text(GTK_EDITABLE(entry),
            identity_field_observation_get_corrected_value(field) != NULL
                ? identity_field_observation_get_corrected_value(field)
                : identity_field_observation_get_raw_value(field));
        g_object_set_data(G_OBJECT(row), "identity-field-code",
            (gpointer) identity_field_observation_get_code(field));
        g_object_set_data(G_OBJECT(row), "identity-field-order",
            GUINT_TO_POINTER(identity_field_observation_get_order(field) + 1));
        g_object_set_data(G_OBJECT(row), "identity-value-entry", entry);
        g_object_set_data(G_OBJECT(row), "identity-modify-button", modify);
        g_object_set_data(G_OBJECT(row), "identity-reedit-button", reedit);
        GtkWidget *buttons[] = {show, accept, modify, restore, reject};
        for (guint i = 0; i < G_N_ELEMENTS(buttons); i++) {
            g_object_set_data(G_OBJECT(buttons[i]), "field", field);
            g_object_set_data(G_OBJECT(buttons[i]), "entry", entry);
        }
        g_object_set_data(G_OBJECT(reedit), "entry", entry);
        g_object_set_data(G_OBJECT(reedit), "modify", modify);
        g_object_set_data(G_OBJECT(restore), "modify", modify);
        g_signal_connect(show, "clicked", G_CALLBACK(on_field_show), state);
        g_signal_connect(accept, "clicked", G_CALLBACK(on_field_accept), state);
        g_signal_connect(modify, "clicked", G_CALLBACK(on_field_modify), state);
        g_signal_connect(reedit, "clicked", G_CALLBACK(on_field_reedit), state);
        g_signal_connect(restore, "clicked", G_CALLBACK(on_field_restore), state);
        g_signal_connect(reject, "clicked", G_CALLBACK(on_field_reject), state);
        gtk_flow_box_append(GTK_FLOW_BOX(actions), show);
        gtk_flow_box_append(GTK_FLOW_BOX(actions), accept);
        gtk_flow_box_append(GTK_FLOW_BOX(actions), modify);
        gtk_flow_box_append(GTK_FLOW_BOX(actions), reedit);
        if (identity_field_observation_get_raw_value(field) != NULL)
            gtk_flow_box_append(GTK_FLOW_BOX(actions), restore);
        gtk_flow_box_append(GTK_FLOW_BOX(actions), reject);
        gtk_box_append(GTK_BOX(row), label);
        gtk_box_append(GTK_BOX(row), entry);
        gtk_box_append(GTK_BOX(row), actions);
        gtk_box_append(state->fields, row);
        g_free(description);
    }
}

static void on_add_manual(GtkButton *button, gpointer data)
{
    EvidenceIdentityOcrDialogState *state = data;
    GtkEntry *entry = g_object_get_data(G_OBJECT(button), "entry");
    GtkDropDown *codes = g_object_get_data(G_OBJECT(button), "codes");
    static const char *const field_codes[] = {
        "document_type","issuing_country","issuing_authority",
        "document_number","surname","birth_name","given_names",
        "sex_as_printed","nationality","birth_date","birth_place",
        "issue_date","expiry_date","address_as_printed",
        "mrz_line_1","mrz_line_2","mrz_line_3"};
    guint selected = gtk_drop_down_get_selected(codes);
    const char *value = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (state->run == NULL || selected >= G_N_ELEMENTS(field_codes) ||
        value[0] == '\0') return;
    IdentityFieldObservation *field = identity_field_observation_new_manual(
        field_codes[selected], value,
        identity_ocr_run_get_fields(state->run)->len);
    identity_ocr_run_add_field(state->run, field);
    gtk_editable_set_text(GTK_EDITABLE(entry), "");
    render_fields(state);
}

static void workflow_worker(GTask *task, gpointer source,
    gpointer task_data, GCancellable *cancellable)
{
    EvidenceIdentityOcrJob *job = task_data;
    IdentityOcrWorkflowRequest request = {
        .root_path=job->root, .evidence_identifier=job->identifier,
        .relative_path=job->relative, .expected_sha256=job->sha256,
        .mime_type=job->mime, .document_type=job->type,
        .document_side=job->side, .languages=job->languages,
        .preprocessing_profile=job->profile,
        .tesseract_executable=job->executable,
        .tesseract_version=job->version, .page_number=job->page,
        .profile=(IdentityOcrPreprocessProfile)job->profile_index,
        .generation=job->generation};
    GError *error = NULL;
    IdentityOcrRun *run = identity_ocr_workflow_execute(
        &request, cancellable, &error);
    (void) source;
    if (run != NULL)
        g_task_return_pointer(task, run,
            (GDestroyNotify) identity_ocr_run_free);
    else
        g_task_return_error(task, error);
}

static void workflow_completed(GObject *source, GAsyncResult *result,
    gpointer data)
{
    EvidenceIdentityOcrJob *job = data;
    GtkWindow *window = g_weak_ref_get(&job->window);
    EvidenceIdentityOcrDialogState *state = window != NULL
        ? g_object_get_data(G_OBJECT(window),
            "evidence-identity-ocr-state") : NULL;
    GError *error = NULL;
    IdentityOcrRun *run = g_task_propagate_pointer(G_TASK(result), &error);
    (void) source;
    if (state != NULL && job->generation == state->generation &&
        (state->session_check == NULL ||
         state->session_check(state->user_data))) {
        identity_ocr_run_free(state->run);
        state->run = run;
        run = NULL;
        if (state->run != NULL) {
            gtk_text_buffer_set_text(gtk_text_view_get_buffer(state->raw_text),
                identity_ocr_run_get_raw_text(state->run), -1);
            gtk_text_buffer_set_text(
                gtk_text_view_get_buffer(state->corrected_text),
                identity_ocr_run_get_raw_text(state->run), -1);
            render_fields(state);
            gtk_text_view_set_editable(state->corrected_text, TRUE);
            gtk_widget_set_sensitive(
                GTK_WIDGET(state->save_transcription), TRUE);
            ocr_provenance_overlay_set_image(state->overlay,
                identity_ocr_run_get_preview(state->run), state->generation);
            gtk_label_set_text(state->status,
                "OCR terminé. Vérifiez chaque proposition.");
            gtk_widget_set_sensitive(
                GTK_WIDGET(state->continue_with_ocr), TRUE);
        } else
            gtk_label_set_text(state->status,
                error != NULL ? error->message : "OCR annulé.");
        g_clear_object(&state->task);
        g_clear_object(&state->cancellable);
    }
    identity_ocr_run_free(run);
    g_clear_error(&error);
    g_clear_object(&window);
}

static void on_start(GtkButton *button, gpointer data)
{
    EvidenceIdentityOcrDialogState *state = data;
    guint selected_language = gtk_drop_down_get_selected(state->languages);
    guint selected_type = gtk_drop_down_get_selected(state->type);
    guint selected_side = gtk_drop_down_get_selected(state->side);
    guint selected_profile = gtk_drop_down_get_selected(state->profile);
    (void) button;
    if (selected_language == GTK_INVALID_LIST_POSITION ||
        state->language_codes == NULL ||
        selected_language >= state->language_codes->len) return;
    state->generation++;
    if (state->cancellable != NULL) g_cancellable_cancel(state->cancellable);
    g_clear_object(&state->task);
    g_clear_object(&state->cancellable);
    EvidenceIdentityOcrJob *job = g_new0(EvidenceIdentityOcrJob, 1);
    job->root = g_path_get_dirname(state->file_path);
    job->relative = g_path_get_basename(state->file_path);
    job->identifier = g_strdup(state->temporary_identifier);
    job->sha256 = g_strdup(state->sha256);
    job->mime = g_strdup(state->mime_type);
    job->type = g_strdup(document_types[MIN(
        selected_type, G_N_ELEMENTS(document_types)-1)]);
    job->side = g_strdup(document_sides[MIN(
        selected_side, G_N_ELEMENTS(document_sides)-1)]);
    job->languages = g_strdup(g_ptr_array_index(
        state->language_codes, selected_language));
    job->profile_index = MIN(selected_profile, G_N_ELEMENTS(profiles)-1);
    job->profile = g_strdup(profiles[job->profile_index]);
    job->executable = g_strdup(state->tesseract_path);
    job->version = g_strdup(state->tesseract_version);
    job->page = (guint) gtk_spin_button_get_value_as_int(state->page);
    job->generation = state->generation;
    g_weak_ref_init(&job->window, G_OBJECT(state->window));
    state->cancellable = g_cancellable_new();
    state->task = g_task_new(NULL, state->cancellable,
        workflow_completed, job);
    g_task_set_task_data(state->task, job, evidence_identity_ocr_job_free);
    gtk_widget_set_sensitive(GTK_WIDGET(state->continue_with_ocr), FALSE);
    gtk_label_set_text(state->status,
        "OCR contrôlé en cours… Aucune écriture n’est effectuée.");
    g_task_run_in_thread(state->task, workflow_worker);
}

static gboolean populate_languages(EvidenceIdentityOcrDialogState *state,
    GError **error)
{
    char *raw = ocr_analysis_list_languages(
        state->tesseract_path, NULL, error);
    GPtrArray *parsed = raw != NULL
        ? ocr_analysis_parse_languages(raw) : NULL;
    state->language_codes = parsed != NULL
        ? ocr_analysis_build_language_choices(parsed) : NULL;
    g_free(raw);
    g_clear_pointer(&parsed, g_ptr_array_unref);
    if (state->language_codes == NULL ||
        state->language_codes->len == 0) return FALSE;
    GtkStringList *labels = gtk_string_list_new(NULL);
    for (guint i = 0; i < state->language_codes->len; i++)
        gtk_string_list_append(labels,
            g_ptr_array_index(state->language_codes, i));
    gtk_drop_down_set_model(state->languages, G_LIST_MODEL(labels));
    g_object_unref(labels);
    gtk_drop_down_set_selected(state->languages, 0);
    return TRUE;
}

gboolean evidence_identity_ocr_dialog_present(
    GtkWindow *parent, const char *file_path, const GPtrArray *persons,
    const char *preselected_person_identifier,
    const ToolInfo *tesseract_tool,
    EvidenceIdentityOcrDialogSessionCheck session_check,
    EvidenceIdentityOcrDialogCallback callback,
    gpointer user_data, GDestroyNotify user_data_destroy,
    GError **error)
{
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    if (parent == NULL || file_path == NULL || persons == NULL ||
        callback == NULL || !evidence_identity_ocr_dialog_file_is_compatible(
            file_path)) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
            "Les paramètres du dialogue OCR d’import sont invalides.");
        return FALSE;
    }
    EvidenceIdentityOcrDialogState *state =
        g_new0(EvidenceIdentityOcrDialogState, 1);
    state->file_path = g_strdup(file_path);
    state->temporary_identifier = g_uuid_string_random();
    state->mime_type = g_strdup(mime_from_path(file_path));
    state->session_check = session_check;
    state->callback = callback;
    state->user_data = user_data;
    state->user_data_destroy = user_data_destroy;
    state->person_identifiers = g_ptr_array_new_with_free_func(g_free);
    if (!file_hash_compute_sha256(
            file_path, NULL, &state->sha256, &state->size, error))
        goto failure;
    if (tesseract_tool != NULL &&
        tool_info_get_availability(tesseract_tool) ==
            TOOL_AVAILABILITY_AVAILABLE) {
        state->tesseract_path = g_strdup(
            tool_info_get_resolved_path(tesseract_tool));
        state->tesseract_version = g_strdup(
            tool_info_get_detected_version(tesseract_tool));
    }
    state->window = GTK_WINDOW(gtk_window_new());
    gtk_window_set_application(state->window,
        gtk_window_get_application(parent));
    gtk_window_set_transient_for(state->window, parent);
    gtk_window_set_modal(state->window, TRUE);
    gtk_window_set_title(state->window,
        "Import — OCR d’identité facultatif");
    gtk_window_set_default_size(state->window, 1200, 800);
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_name(root, "identity-form-content");
    gtk_widget_set_margin_start(root, 14);
    gtk_widget_set_margin_end(root, 14);
    gtk_widget_set_margin_top(root, 14);
    gtk_widget_set_margin_bottom(root, 14);
    GtkWidget *intro = gtk_label_new(
        "L’OCR est facultatif et ne confirme ni l’identité ni "
        "l’authenticité du document.");
    gtk_label_set_wrap(GTK_LABEL(intro), TRUE);
    gtk_label_set_xalign(GTK_LABEL(intro), 0.0f);
    gtk_box_append(GTK_BOX(root), intro);
    GtkStringList *person_labels = gtk_string_list_new(NULL);
    gtk_string_list_append(person_labels,
        "Sélectionner explicitement une personne existante");
    guint preselected = 0;
    for (guint i = 0; i < persons->len; i++) {
        EntityRecord *person = g_ptr_array_index((GPtrArray *) persons, i);
        if (g_strcmp0(entity_record_get_type_identifier(person),
                "person") != 0) continue;
        const char *identifier = entity_record_get_identifier(person);
        char *short_id = g_strndup(identifier, 8);
        char *label = g_strdup_printf("%s%s%s — %s",
            entity_record_get_label(person) != NULL
                ? entity_record_get_label(person)
                : entity_record_get_value(person),
            entity_record_get_label(person) != NULL ? " — " : "",
            entity_record_get_label(person) != NULL
                ? entity_record_get_value(person) : "",
            short_id);
        gtk_string_list_append(person_labels, label);
        g_ptr_array_add(state->person_identifiers, g_strdup(identifier));
        if (g_strcmp0(identifier, preselected_person_identifier) == 0)
            preselected = state->person_identifiers->len;
        g_free(short_id); g_free(label);
    }
    state->person = GTK_DROP_DOWN(gtk_drop_down_new(
        G_LIST_MODEL(person_labels), NULL));
    gtk_widget_set_name(GTK_WIDGET(state->person), "identity-person");
    gtk_drop_down_set_selected(state->person, preselected);
    gtk_box_append(GTK_BOX(root), GTK_WIDGET(state->person));
    state->type = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(
        (const char *[]){"Carte d’identité","Passeport","Permis",
            "Titre de séjour","Autre",NULL}));
    gtk_widget_set_name(GTK_WIDGET(state->type), "identity-document-type");
    state->side = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(
        (const char *[]){"Recto","Verso","Page d’identité",
            "Autre page",NULL}));
    gtk_widget_set_name(GTK_WIDGET(state->side), "identity-document-side");
    state->page = GTK_SPIN_BUTTON(
        gtk_spin_button_new_with_range(1, 999, 1));
    state->languages = GTK_DROP_DOWN(
        gtk_drop_down_new_from_strings(
            (const char *[]){"Langues indisponibles",NULL}));
    gtk_widget_set_name(GTK_WIDGET(state->languages), "identity-languages");
    state->profile = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(
        (const char *[]){"Aucun prétraitement","Orientation",
            "Niveaux de gris","Agrandissement",NULL}));
    gtk_box_append(GTK_BOX(root), GTK_WIDGET(state->type));
    gtk_box_append(GTK_BOX(root), GTK_WIDGET(state->side));
    gtk_box_append(GTK_BOX(root), GTK_WIDGET(state->page));
    gtk_box_append(GTK_BOX(root), GTK_WIDGET(state->languages));
    gtk_box_append(GTK_BOX(root), GTK_WIDGET(state->profile));
    state->start = GTK_BUTTON(gtk_button_new_with_label(
        "Analyser comme document d’identité"));
    gtk_box_append(GTK_BOX(root), GTK_WIDGET(state->start));
    state->status = GTK_LABEL(gtk_label_new(
        "Vous pouvez poursuivre l’import sans lancer l’OCR."));
    gtk_widget_set_name(GTK_WIDGET(state->status),
        "evidence-identity-ocr-status");
    gtk_label_set_wrap(state->status, TRUE);
    gtk_label_set_xalign(state->status, 0.0f);
    gtk_box_append(GTK_BOX(root), GTK_WIDGET(state->status));
    state->overlay = ocr_provenance_overlay_new();
    state->fields = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 6));
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_size_request(scroll, -1, 180);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll),
        GTK_WIDGET(state->fields));
    gtk_box_append(GTK_BOX(root), scroll);
    GtkWidget *manual = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkDropDown *codes = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(
        (const char *[]){"Type de document","Pays émetteur",
            "Autorité émettrice","Numéro","Nom","Nom de naissance",
            "Prénoms","Sexe","Nationalité","Date de naissance",
            "Lieu de naissance","Délivrance","Expiration","Adresse",
            "MRZ 1","MRZ 2","MRZ 3",NULL}));
    GtkWidget *manual_entry = gtk_entry_new();
    gtk_widget_set_hexpand(manual_entry, TRUE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(manual_entry),
        "Valeur visible mais omise par l’OCR");
    GtkWidget *add_manual =
        gtk_button_new_with_label("Ajouter un champ manquant");
    g_object_set_data(G_OBJECT(add_manual), "codes", codes);
    g_object_set_data(G_OBJECT(add_manual), "entry", manual_entry);
    g_signal_connect(add_manual, "clicked",
        G_CALLBACK(on_add_manual), state);
    gtk_box_append(GTK_BOX(manual), GTK_WIDGET(codes));
    gtk_box_append(GTK_BOX(manual), manual_entry);
    gtk_box_append(GTK_BOX(manual), add_manual);
    gtk_box_append(GTK_BOX(root), manual);
    gtk_box_append(GTK_BOX(root), gtk_label_new(
        "Notes factuelles : document tronqué, flou, masqué ou incomplet"));
    state->notes = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_widget_set_name(GTK_WIDGET(state->notes), "identity-notes");
    gtk_widget_set_size_request(GTK_WIDGET(state->notes), -1, 70);
    gtk_box_append(GTK_BOX(root), GTK_WIDGET(state->notes));
    state->raw_text = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_widget_set_name(GTK_WIDGET(state->raw_text), "identity-raw-text");
    gtk_text_view_set_editable(state->raw_text, FALSE);
    gtk_widget_set_size_request(GTK_WIDGET(state->raw_text), -1, 100);
    gtk_box_append(GTK_BOX(root), gtk_label_new("Texte OCR brut"));
    gtk_box_append(GTK_BOX(root), GTK_WIDGET(state->raw_text));
    gtk_box_append(GTK_BOX(root), gtk_label_new(
        "Transcription corrigée manuellement"));
    state->corrected_text = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_widget_set_name(GTK_WIDGET(state->corrected_text),
        "identity-corrected-transcription");
    gtk_text_view_set_wrap_mode(state->corrected_text, GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_editable(state->corrected_text, TRUE);
    gtk_widget_set_size_request(GTK_WIDGET(state->corrected_text), -1, 120);
    gtk_box_append(GTK_BOX(root), GTK_WIDGET(state->corrected_text));
    GtkWidget *transcription_actions = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(
        GTK_FLOW_BOX(transcription_actions), GTK_SELECTION_NONE);
    GtkWidget *edit_transcription =
        gtk_button_new_with_label("Modifier la transcription");
    state->save_transcription = GTK_BUTTON(
        gtk_button_new_with_label("Enregistrer la correction"));
    GtkWidget *reedit_transcription =
        gtk_button_new_with_label("Modifier à nouveau");
    gtk_widget_set_name(reedit_transcription,
        "identity-corrected-transcription-reedit");
    GtkWidget *reset_transcription =
        gtk_button_new_with_label("Réinitialiser depuis l’OCR brut");
    gtk_flow_box_append(GTK_FLOW_BOX(transcription_actions),
        edit_transcription);
    gtk_flow_box_append(GTK_FLOW_BOX(transcription_actions),
        GTK_WIDGET(state->save_transcription));
    gtk_flow_box_append(GTK_FLOW_BOX(transcription_actions),
        reedit_transcription);
    gtk_flow_box_append(GTK_FLOW_BOX(transcription_actions),
        reset_transcription);
    gtk_box_append(GTK_BOX(root), transcription_actions);
    g_signal_connect(edit_transcription, "clicked",
        G_CALLBACK(on_edit_transcription), state);
    g_signal_connect(state->save_transcription, "clicked",
        G_CALLBACK(on_save_transcription), state);
    g_signal_connect(reedit_transcription, "clicked",
        G_CALLBACK(on_edit_transcription), state);
    g_signal_connect(reset_transcription, "clicked",
        G_CALLBACK(on_reset_transcription), state);
    GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_name(actions, "identity-final-actions");
    gtk_widget_set_halign(actions, GTK_ALIGN_END);
    GtkWidget *cancel = gtk_button_new_with_label("Annuler");
    gtk_widget_set_name(cancel, "evidence-identity-ocr-cancel");
    GtkWidget *without = gtk_button_new_with_label("Importer sans OCR");
    state->continue_with_ocr = GTK_BUTTON(
        gtk_button_new_with_label("Continuer l’import avec OCR"));
    gtk_widget_set_name(GTK_WIDGET(state->continue_with_ocr),
        "evidence-identity-continue-with-ocr");
    gtk_widget_set_sensitive(GTK_WIDGET(state->continue_with_ocr), FALSE);
    gtk_box_append(GTK_BOX(actions), cancel);
    gtk_box_append(GTK_BOX(actions), without);
    gtk_box_append(GTK_BOX(actions),
        GTK_WIDGET(state->continue_with_ocr));
    GtkWidget *left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_name(left, "identity-left-panel");
    gtk_widget_set_size_request(left, 520, -1);
    GtkWidget *form_scroll = gtk_scrolled_window_new();
    gtk_widget_set_name(form_scroll, "identity-form-scroll");
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(form_scroll),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_propagate_natural_height(
        GTK_SCROLLED_WINDOW(form_scroll), FALSE);
    gtk_widget_set_vexpand(form_scroll, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(form_scroll), root);
    gtk_box_append(GTK_BOX(left), form_scroll);
    gtk_widget_set_margin_start(actions, 14);
    gtk_widget_set_margin_end(actions, 14);
    gtk_widget_set_margin_bottom(actions, 10);
    gtk_box_append(GTK_BOX(left), actions);
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_name(paned, "evidence-identity-ocr-paned");
    gtk_paned_set_start_child(GTK_PANED(paned), left);
    GtkWidget *overlay =
        ocr_provenance_overlay_get_widget(state->overlay);
    gtk_widget_set_size_request(overlay, 180, -1);
    gtk_widget_set_hexpand(overlay, TRUE);
    gtk_paned_set_end_child(GTK_PANED(paned), overlay);
    gtk_paned_set_resize_start_child(GTK_PANED(paned), FALSE);
    gtk_paned_set_resize_end_child(GTK_PANED(paned), TRUE);
    gtk_window_set_child(state->window, paned);
    g_object_set_data_full(G_OBJECT(state->window),
        "evidence-identity-ocr-state", state,
        evidence_identity_ocr_dialog_state_free);
    g_signal_connect(state->window, "close-request",
        G_CALLBACK(on_close), state);
    g_signal_connect(cancel, "clicked", G_CALLBACK(on_cancel), state);
    g_signal_connect(without, "clicked", G_CALLBACK(on_without_ocr), state);
    g_signal_connect(state->continue_with_ocr, "clicked",
        G_CALLBACK(on_continue_with_ocr), state);
    g_signal_connect(state->start, "clicked", G_CALLBACK(on_start), state);
    gtk_widget_set_sensitive(GTK_WIDGET(state->start),
        state->tesseract_path != NULL);
    if (state->tesseract_path != NULL &&
        !populate_languages(state, error)) goto failure_window;
    gtk_window_present(state->window);
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
        set_initial_paned_position, g_object_ref(paned), g_object_unref);
    return TRUE;

failure_window:
    /*
     * FALSE signifie que l'appelant conserve user_data. La fenêtre possède
     * déjà l'état, mais son destructeur ne doit donc pas libérer ce contexte.
     */
    state->user_data_destroy = NULL;
    gtk_window_destroy(state->window);
    return FALSE;
failure:
    evidence_identity_ocr_dialog_state_free(state);
    return FALSE;
}

gboolean evidence_identity_ocr_dialog_present_review(
    GtkWindow *parent,const char *file_path,const GPtrArray *persons,
    const char *preselected_person_identifier,
    const IdentityOcrRun *persisted_run,
    EvidenceIdentityOcrDialogSessionCheck session_check,
    EvidenceIdentityOcrDialogCallback callback,
    gpointer user_data,GDestroyNotify user_data_destroy,GError **error)
{
    if (persisted_run == NULL) {
        g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_INVALID_ARGUMENT,
            "L’analyse OCR à réviser est absente.");
        return FALSE;
    }
    if (!evidence_identity_ocr_dialog_present(parent,file_path,persons,
            preselected_person_identifier,NULL,session_check,callback,
            user_data,user_data_destroy,error)) return FALSE;
    GListModel *windows=gtk_window_get_toplevels();
    EvidenceIdentityOcrDialogState *state=NULL;
    for(guint i=0;i<g_list_model_get_n_items(windows);i++){
        GtkWindow *window=g_list_model_get_item(windows,i);
        if(gtk_window_get_transient_for(window)==parent)
            state=g_object_get_data(G_OBJECT(window),
                "evidence-identity-ocr-state");
        g_object_unref(window);
        if(state!=NULL)break;
    }
    if(state==NULL)return FALSE;
    identity_ocr_run_free(state->run);
    state->run=identity_ocr_run_copy(persisted_run);
    g_free(state->temporary_identifier);
    state->temporary_identifier=g_strdup(
        identity_ocr_run_get_evidence_id(persisted_run));
    gtk_window_set_title(state->window,"Réviser l’analyse OCR");
    state->review_mode=TRUE;
    gtk_widget_set_visible(GTK_WIDGET(state->start),FALSE);
    gtk_widget_set_visible(GTK_WIDGET(state->type),FALSE);
    gtk_widget_set_visible(GTK_WIDGET(state->side),FALSE);
    gtk_widget_set_visible(GTK_WIDGET(state->page),FALSE);
    gtk_widget_set_visible(GTK_WIDGET(state->languages),FALSE);
    gtk_widget_set_visible(GTK_WIDGET(state->profile),FALSE);
    gtk_button_set_label(state->continue_with_ocr,
        "Enregistrer la révision");
    gtk_widget_set_sensitive(GTK_WIDGET(state->continue_with_ocr),TRUE);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(state->raw_text),
        identity_ocr_run_get_raw_text(state->run),-1);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(state->corrected_text),
        identity_ocr_run_get_corrected_transcription(state->run)!=NULL
         ?identity_ocr_run_get_corrected_transcription(state->run)
         :identity_ocr_run_get_raw_text(state->run),-1);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(state->notes),
        identity_ocr_run_get_factual_notes(state->run)!=NULL
         ?identity_ocr_run_get_factual_notes(state->run):"",-1);
    render_fields(state);
    gtk_label_set_text(state->status,
        "Révision locale des données persistées — Tesseract n’est pas lancé.");
    return TRUE;
}

void evidence_identity_ocr_dialog_result_free(
    EvidenceIdentityOcrDialogResult *result)
{
    if (result == NULL) return;
    g_clear_object(&result->dialog);
    g_free(result->person_identifier);
    g_free(result->temporary_identifier);
    g_free(result->sha256);
    g_free(result->mime_type);
    identity_ocr_run_free(result->run);
    g_free(result);
}
gboolean evidence_identity_ocr_dialog_result_has_ocr(
    const EvidenceIdentityOcrDialogResult *result)
{ return result != NULL && result->run != NULL; }
const char *evidence_identity_ocr_dialog_result_get_person_identifier(
    const EvidenceIdentityOcrDialogResult *result)
{ return result != NULL ? result->person_identifier : NULL; }
const char *evidence_identity_ocr_dialog_result_get_temporary_identifier(
    const EvidenceIdentityOcrDialogResult *result)
{ return result != NULL ? result->temporary_identifier : NULL; }
const char *evidence_identity_ocr_dialog_result_get_sha256(
    const EvidenceIdentityOcrDialogResult *result)
{ return result != NULL ? result->sha256 : NULL; }
guint64 evidence_identity_ocr_dialog_result_get_size(
    const EvidenceIdentityOcrDialogResult *result)
{ return result != NULL ? result->size : 0; }
const char *evidence_identity_ocr_dialog_result_get_mime_type(
    const EvidenceIdentityOcrDialogResult *result)
{ return result != NULL ? result->mime_type : NULL; }
IdentityOcrRun *evidence_identity_ocr_dialog_result_steal_run(
    EvidenceIdentityOcrDialogResult *result)
{
    IdentityOcrRun *run = result != NULL ? result->run : NULL;
    if (result != NULL) result->run = NULL;
    return run;
}

GtkWindow *evidence_identity_ocr_dialog_result_get_dialog(
    const EvidenceIdentityOcrDialogResult *result)
{
    return result != NULL ? result->dialog : NULL;
}

void evidence_identity_ocr_dialog_finish_import(
    GtkWindow *dialog, const GError *error)
{
    EvidenceIdentityOcrDialogState *state = dialog != NULL
        ? g_object_get_data(G_OBJECT(dialog),
            "evidence-identity-ocr-state") : NULL;
    if (state == NULL || state->completed) return;
    if (error != NULL) {
        state->submission_pending = FALSE;
        gtk_widget_set_sensitive(
            GTK_WIDGET(state->continue_with_ocr), TRUE);
        char *message = g_strdup_printf(state->review_mode
            ? "La révision OCR a échoué : %s"
            : "L’import OCR a échoué : %s", error->message);
        gtk_label_set_text(state->status, message);
        g_free(message);
        return;
    }
    state->completed = TRUE;
    gtk_label_set_text(state->status, state->review_mode
        ? "Révision OCR enregistrée."
        : "Import OCR terminé.");
    if (state->submission_task != NULL) {
        background_task_unref(state->submission_task);
        state->submission_task = NULL;
    }
    gtk_window_close(state->window);
}

void evidence_identity_ocr_dialog_set_submission_task(
    GtkWindow *dialog, BackgroundTask *task)
{
    EvidenceIdentityOcrDialogState *state = dialog != NULL
        ? g_object_get_data(G_OBJECT(dialog),
            "evidence-identity-ocr-state") : NULL;
    if (state == NULL || state->completed) return;
    if (state->submission_task != NULL)
        background_task_unref(state->submission_task);
    state->submission_task = background_task_ref(task);
}
