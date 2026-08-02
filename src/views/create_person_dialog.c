#include "views/create_person_dialog.h"
#include "views/dialog_geometry.h"
#include "views/person_vocabulary_adapter.h"
#include "views/identity_ocr_option_adapter.h"
#include "views/person_factual_relation_editor.h"
#include "views/person_ocr_projection_editor.h"
#include "views/person_creation_confirmation.h"
#include "core/evidence_staging.h"
#include "core/evidence_staging_task.h"
#include "core/person_confirmation_summary.h"
#include "core/person_dialog_lifecycle.h"
#include "core/identity_ocr_preprocessor.h"
#include "core/identity_ocr_workflow.h"
#include "core/identity_field_extractor.h"
#include "core/ocr_analysis.h"
#include "models/evidence_selection_model.h"
#include "models/person_evidence_selection.h"
#include "models/identity_ocr.h"
#include "widgets/evidence_preview_widget.h"
#include "widgets/ocr_provenance_overlay.h"

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
    PersonEvidenceSelection *evidence_selection;
    EvidenceStaging *staging;
    GPtrArray *ocr_runs;
    GPtrArray *factual_relations;
    GPtrArray *ocr_projections;
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
    GtkDropDown *retained;
    GtkDropDown *retained_type;
    GtkDropDown *type_filter;
    GtkEntry *search;
    GtkTextView *notes;
    GtkLabel *error;
    EvidencePreviewWidget *preview;
    GtkLabel *progress;
    GtkLabel *summary;
    GtkScrolledWindow *summary_scroll;
    GtkStack *stack;
    GtkButton *previous;
    GtkButton *next;
    GtkButton *create;
    GtkButton *add_existing;
    GtkButton *remove_retained;
    GtkButton *import_files;
    GtkButton *ocr_start;
    GtkButton *ocr_cancel;
    GtkDropDown *ocr_document_type;
    GtkDropDown *ocr_document_side;
    GtkDropDown *ocr_languages;
    GtkDropDown *ocr_profile;
    GtkSpinButton *ocr_page;
    GtkTextView *ocr_text;
    GtkTextView *ocr_corrected_text;
    GtkStack *ocr_transcription_stack;
    GtkBox *ocr_fields;
    GtkDropDown *ocr_manual_code;
    GtkEntry *ocr_manual_value;
    GtkTextView *ocr_factual_notes;
    GtkLabel *ocr_status;
    OcrProvenanceOverlay *ocr_overlay;
    GPtrArray *ocr_language_codes;
    GtkStringList *evidence_labels;
    GtkStringList *retained_labels;
    GtkStringList *type_filter_labels;
    GPtrArray *role_buttons;
    PersonVocabularyAdapter *vocabularies;
    guint step;
    GPtrArray *evidence_identifiers;
    GPtrArray *visible_records;
    GPtrArray *type_codes;
    EvidenceSelectionModel *selection_model;
    PersonEvidenceSelection *person_evidence_selection;
    EvidenceStaging *staging;
    char *investigation_root_path;
    TaskManager *task_manager;
    BackgroundTask *staging_task;
    GTask *ocr_task;
    GCancellable *ocr_cancellable;
    GPtrArray *ocr_runs;
    char *tesseract_path;
    char *tesseract_version;
    guint64 ocr_generation;
    PersonDialogLifecycle *lifecycle;
    CreatePersonDialogCallback callback;
    CreatePersonDialogSessionCheck session_check;
    gpointer user_data;
    GDestroyNotify user_data_destroy;
    gulong evidence_selected_handler;
    gulong retained_selected_handler;
    gboolean updating_evidence;
    gboolean updating_retained;
    PersonFactualRelationEditor *factual_relation_editor;
    PersonOcrProjectionEditor *ocr_projection_editor;
} CreatePersonDialogState;

typedef struct {
    char *root, *identifier, *relative, *sha256, *mime;
    char *document_type, *document_side, *languages, *profile;
    char *executable, *version;
    guint page;
    guint profile_index;
    guint64 generation;
    GWeakRef window;
} IdentityOcrJob;

typedef struct {
    char *executable;
    GWeakRef window;
} IdentityLanguageJob;

typedef struct {
    GWeakRef window;
    guint64 generation;
} CreatePersonStagingContext;
static void create_person_dialog_clear_preview(CreatePersonDialogState *state);
static void create_person_dialog_select_record(CreatePersonDialogState *state,
    const EvidenceRecord *record, const char *business_type);
static void create_person_dialog_on_retained_changed(
    GtkDropDown *dropdown, GParamSpec *pspec, gpointer data);
static void create_person_dialog_render_ocr_fields(
    CreatePersonDialogState *state, IdentityOcrRun *run);

static void identity_language_job_free(gpointer data)
{
    IdentityLanguageJob *job = data;
    if (job == NULL) return;
    g_free(job->executable);
    g_weak_ref_clear(&job->window);
    g_free(job);
}

static void identity_languages_worker(GTask *task, gpointer source,
    gpointer task_data, GCancellable *cancellable)
{
    IdentityLanguageJob *job = task_data;
    GError *error = NULL;
    char *raw = ocr_analysis_list_languages(job->executable, cancellable,
        &error);
    GPtrArray *parsed = raw != NULL ? ocr_analysis_parse_languages(raw) : NULL;
    GPtrArray *choices = parsed != NULL
        ? ocr_analysis_build_language_choices(parsed) : NULL;
    g_free(raw);
    g_clear_pointer(&parsed, g_ptr_array_unref);
    (void) source;
    if (choices != NULL) g_task_return_pointer(task, choices,
        (GDestroyNotify) g_ptr_array_unref);
    else g_task_return_error(task, error);
}
static void identity_languages_completed(GObject *source,
    GAsyncResult *result, gpointer data)
{
    IdentityLanguageJob *job = data;
    GtkWindow *window = g_weak_ref_get(&job->window);
    CreatePersonDialogState *state = window != NULL
        ? g_object_get_data(G_OBJECT(window), "person-dialog-state") : NULL;
    GError *error = NULL;
    GPtrArray *choices = g_task_propagate_pointer(G_TASK(result), &error);
    (void) source;
    if (state != NULL) {
        g_clear_pointer(&state->ocr_language_codes, g_ptr_array_unref);
        state->ocr_language_codes = choices;
        choices = NULL;
        GtkStringList *labels = gtk_string_list_new(NULL);
        for (guint i = 0; state->ocr_language_codes != NULL &&
             i < state->ocr_language_codes->len; i++) {
            const char *code = g_ptr_array_index(state->ocr_language_codes, i);
            gtk_string_list_append(labels,
                identity_ocr_option_adapter_language_label(code));
        }
        gtk_drop_down_set_model(state->ocr_languages, G_LIST_MODEL(labels));
        gtk_drop_down_set_selected(state->ocr_languages,
            identity_ocr_option_adapter_default_language_index(
                state->ocr_language_codes));
        gtk_widget_set_sensitive(GTK_WIDGET(state->ocr_start),
            state->ocr_language_codes != NULL &&
            state->ocr_language_codes->len > 0);
        if (state->ocr_language_codes == NULL ||
            state->ocr_language_codes->len == 0)
            gtk_label_set_text(state->ocr_status,
                "Aucune langue Tesseract installée.");
        g_object_unref(labels);
    }
    g_clear_pointer(&choices, g_ptr_array_unref);
    g_clear_error(&error);
    g_clear_object(&window);
}

static char *create_person_dialog_get_notes(CreatePersonDialogState *state)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(state->notes);
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    return gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
}

static void identity_ocr_job_free(gpointer data)
{
    IdentityOcrJob *job = data;
    if (job == NULL) return;
    g_free(job->root); g_free(job->identifier); g_free(job->relative);
    g_free(job->sha256); g_free(job->mime); g_free(job->document_type);
    g_free(job->document_side); g_free(job->languages); g_free(job->profile);
    g_free(job->executable); g_free(job->version);
    g_weak_ref_clear(&job->window); g_free(job);
}

static void create_person_dialog_ocr_worker(GTask *task, gpointer source,
    gpointer task_data, GCancellable *cancellable)
{
    IdentityOcrJob *job = task_data;
    IdentityOcrWorkflowRequest request = {
        .root_path = job->root,
        .evidence_identifier = job->identifier,
        .relative_path = job->relative,
        .expected_sha256 = job->sha256,
        .mime_type = job->mime,
        .document_type = job->document_type,
        .document_side = job->document_side,
        .languages = job->languages,
        .preprocessing_profile = job->profile,
        .tesseract_executable = job->executable,
        .tesseract_version = job->version,
        .page_number = job->page,
        .profile = (IdentityOcrPreprocessProfile) job->profile_index,
        .generation = job->generation
    };
    GError *error = NULL;
    (void) source;
    IdentityOcrRun *run = identity_ocr_workflow_execute(
        &request, cancellable, &error);
    if (run != NULL)
        g_task_return_pointer(task, run,
            (GDestroyNotify) identity_ocr_run_free);
    else
        g_task_return_error(task, error != NULL ? error :
            g_error_new_literal(G_IO_ERROR, G_IO_ERROR_FAILED,
                "L’OCR contrôlé a échoué."));
}

static void create_person_dialog_on_field_accept(GtkButton *button,
    gpointer data)
{
    IdentityFieldObservation *field = g_object_get_data(
        G_OBJECT(button), "identity-field");
    (void) data;
    identity_field_observation_accept(field);
    gtk_button_set_label(button, "Acceptée");
    gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
    GtkEntry *entry = g_object_get_data(
        G_OBJECT(button), "identity-entry");
    if (entry != NULL)
        gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
}

static void create_person_dialog_on_field_reedit(GtkButton *button,
    gpointer data)
{
    GtkEntry *entry = g_object_get_data(
        G_OBJECT(button), "identity-entry");
    GtkButton *save = g_object_get_data(
        G_OBJECT(button), "identity-save");
    (void) data;
    if (entry == NULL || save == NULL) return;
    gtk_editable_set_editable(GTK_EDITABLE(entry), TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(save), TRUE);
    gtk_widget_grab_focus(GTK_WIDGET(entry));
    gtk_editable_set_position(GTK_EDITABLE(entry), -1);
}

static void create_person_dialog_on_field_restore(GtkButton *button,
    gpointer data)
{
    IdentityFieldObservation *field = g_object_get_data(
        G_OBJECT(button), "identity-field");
    GtkEntry *entry = g_object_get_data(
        G_OBJECT(button), "identity-entry");
    GtkButton *save = g_object_get_data(
        G_OBJECT(button), "identity-save");
    (void) data;
    if (!identity_field_observation_restore_raw(field) || entry == NULL)
        return;
    gtk_editable_set_text(GTK_EDITABLE(entry),
        identity_field_observation_get_raw_value(field));
    gtk_editable_set_editable(GTK_EDITABLE(entry), TRUE);
    if (save != NULL) {
        gtk_button_set_label(save, "Enregistrer la correction");
        gtk_widget_set_sensitive(GTK_WIDGET(save), TRUE);
    }
}

static void create_person_dialog_on_field_modify(GtkButton *button,
    gpointer data)
{
    IdentityFieldObservation *field = g_object_get_data(
        G_OBJECT(button), "identity-field");
    (void) data;
    GtkEntry *entry = g_object_get_data(
        G_OBJECT(button), "identity-entry");
    const char *value = entry != NULL
        ? gtk_editable_get_text(GTK_EDITABLE(entry))
        : identity_field_observation_get_raw_value(field);
    if (!identity_field_observation_modify(field, value,
            "Valeur revue individuellement dans l’assistant.")) {
        if (entry != NULL)
            gtk_widget_add_css_class(GTK_WIDGET(entry), "error");
        return;
    }
    if (entry != NULL) {
        gtk_widget_remove_css_class(GTK_WIDGET(entry), "error");
        gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
    }
    gtk_button_set_label(button, "Modifiée");
    gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
}

static void create_person_dialog_on_field_reject(GtkButton *button,
    gpointer data)
{
    IdentityFieldObservation *field = g_object_get_data(
        G_OBJECT(button), "identity-field");
    (void) data;
    identity_field_observation_reject(field);
    gtk_button_set_label(button, "Rejetée");
}

static void create_person_dialog_on_field_show(GtkButton *button,
    gpointer data)
{
    CreatePersonDialogState *state = data;
    IdentityFieldObservation *field = g_object_get_data(
        G_OBJECT(button), "identity-field");
    ocr_provenance_overlay_set_field(state->ocr_overlay, field,
        state->ocr_generation);
}

static void create_person_dialog_on_add_manual_field(GtkButton *button,
    gpointer data)
{
    static const char *const codes[] = {
        "document_type","issuing_country","issuing_authority",
        "document_number","surname","birth_name","given_names",
        "sex_as_printed","nationality","birth_date","birth_place",
        "issue_date","expiry_date","address_as_printed",
        "mrz_line_1","mrz_line_2","mrz_line_3"};
    CreatePersonDialogState *state = data;
    guint selected = gtk_drop_down_get_selected(state->ocr_manual_code);
    const char *value =
        gtk_editable_get_text(GTK_EDITABLE(state->ocr_manual_value));
    (void) button;
    if (state->ocr_runs == NULL || state->ocr_runs->len == 0 ||
        selected >= G_N_ELEMENTS(codes) || value[0] == '\0') return;
    IdentityOcrRun *run = g_ptr_array_index(
        state->ocr_runs, state->ocr_runs->len - 1);
    IdentityFieldObservation *field =
        identity_field_observation_new_manual(
            codes[selected], value,
            identity_ocr_run_get_fields(run)->len);
    if (field == NULL) return;
    identity_ocr_run_add_field(run, field);
    gtk_editable_set_text(GTK_EDITABLE(state->ocr_manual_value), "");
    create_person_dialog_render_ocr_fields(state, run);
}

static void create_person_dialog_capture_factual_notes(
    CreatePersonDialogState *state)
{
    if (state == NULL || state->ocr_runs == NULL ||
        state->ocr_runs->len == 0) return;
    GtkTextBuffer *buffer =
        gtk_text_view_get_buffer(state->ocr_factual_notes);
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    char *notes = gtk_text_buffer_get_text(
        buffer, &start, &end, FALSE);
    IdentityOcrRun *run = g_ptr_array_index(
        state->ocr_runs, state->ocr_runs->len - 1);
    identity_ocr_run_set_factual_notes(run,
        notes[0] != '\0' ? notes : NULL);
    g_free(notes);
}

static char *create_person_dialog_text_view_contents(GtkTextView *view)
{
    GtkTextIter start;
    GtkTextIter end;
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(view);
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    return gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
}

static void create_person_dialog_capture_corrected_transcription(
    CreatePersonDialogState *state)
{
    if (state == NULL || state->ocr_corrected_text == NULL ||
        state->ocr_runs == NULL || state->ocr_runs->len == 0) return;
    IdentityOcrRun *run = g_ptr_array_index(
        state->ocr_runs, state->ocr_runs->len - 1);
    char *text = create_person_dialog_text_view_contents(
        state->ocr_corrected_text);
    const char *raw = identity_ocr_run_get_raw_text(run);
    if (g_strcmp0(text, raw) == 0)
        identity_ocr_run_reset_corrected_transcription(run);
    else {
        GDateTime *now = g_date_time_new_now_utc();
        char *timestamp = g_date_time_format(
            now, "%Y-%m-%dT%H:%M:%SZ");
        identity_ocr_run_set_corrected_transcription(
            run, text, timestamp);
        g_free(timestamp);
        g_date_time_unref(now);
    }
    g_free(text);
}

static void create_person_dialog_on_reset_transcription(
    GtkButton *button, gpointer data)
{
    CreatePersonDialogState *state = data;
    (void) button;
    if (state->ocr_runs == NULL || state->ocr_runs->len == 0) return;
    IdentityOcrRun *run = g_ptr_array_index(
        state->ocr_runs, state->ocr_runs->len - 1);
    identity_ocr_run_reset_corrected_transcription(run);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(
        state->ocr_corrected_text),
        identity_ocr_run_get_raw_text(run), -1);
    gtk_text_view_set_editable(state->ocr_corrected_text, TRUE);
}

static void create_person_dialog_render_ocr_fields(
    CreatePersonDialogState *state, IdentityOcrRun *run)
{
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(
            GTK_WIDGET(state->ocr_fields))) != NULL)
        gtk_box_remove(state->ocr_fields, child);
    const GPtrArray *fields = identity_ocr_run_get_fields(run);
    for (guint i = 0; fields != NULL && i < fields->len; i++) {
        IdentityFieldObservation *field = g_ptr_array_index(
            (GPtrArray *) fields, i);
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        const IdentitySourceBox *box =
            identity_field_observation_get_box(field);
        char *source = box != NULL && box->available
            ? g_strdup_printf("page %d, zone %d,%d %dx%d, confiance %.1f",
                box->page, box->x, box->y, box->width, box->height,
                identity_field_observation_get_confidence(field))
            : g_strdup("Zone source indisponible");
        char *label = g_strdup_printf(
            "%s\nValeur OCR brute : %s\nOrigine : %s — %s",
            identity_field_observation_get_code(field),
            identity_field_observation_get_raw_value(field) != NULL
                ? identity_field_observation_get_raw_value(field)
                : "absente",
            identity_field_observation_get_origin(field), source);
        GtkWidget *field_label = gtk_label_new(label);
        gtk_label_set_wrap(GTK_LABEL(field_label), TRUE);
        gtk_label_set_xalign(GTK_LABEL(field_label), 0.0f);
        gtk_box_append(GTK_BOX(row), field_label);
        GtkWidget *entry = gtk_entry_new();
        gtk_widget_set_hexpand(entry, TRUE);
        gtk_widget_set_tooltip_text(entry,
            "Modifiez la valeur puis choisissez « Modifier ».");
        gtk_editable_set_text(GTK_EDITABLE(entry),
            identity_field_observation_get_corrected_value(field) != NULL
                ? identity_field_observation_get_corrected_value(field)
                : identity_field_observation_get_raw_value(field));
        gtk_box_append(GTK_BOX(row), entry);
        GtkWidget *actions =
            gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        GtkWidget *accept = gtk_button_new_with_label("Accepter");
        GtkWidget *modify = gtk_button_new_with_label("Modifier");
        GtkWidget *reedit = gtk_button_new_with_label("Modifier à nouveau");
        GtkWidget *restore =
            gtk_button_new_with_label("Revenir à la valeur OCR");
        GtkWidget *reject = gtk_button_new_with_label("Rejeter");
        GtkWidget *show = gtk_button_new_with_label("Voir la zone");
        g_object_set_data(G_OBJECT(row), "identity-field-code",
            (gpointer) identity_field_observation_get_code(field));
        g_object_set_data(G_OBJECT(row), "identity-field-order",
            GUINT_TO_POINTER(identity_field_observation_get_order(field) + 1));
        g_object_set_data(G_OBJECT(row), "identity-value-entry", entry);
        g_object_set_data(G_OBJECT(row), "identity-accept-button", accept);
        g_object_set_data(G_OBJECT(row), "identity-modify-button", modify);
        g_object_set_data(G_OBJECT(row), "identity-reedit-button", reedit);
        g_object_set_data(G_OBJECT(row), "identity-restore-button", restore);
        g_object_set_data(G_OBJECT(row), "identity-reject-button", reject);
        g_object_set_data(G_OBJECT(accept), "identity-field", field);
        g_object_set_data(G_OBJECT(accept), "identity-entry", entry);
        g_object_set_data(G_OBJECT(modify), "identity-field", field);
        g_object_set_data(G_OBJECT(modify), "identity-entry", entry);
        g_object_set_data(G_OBJECT(reject), "identity-field", field);
        g_object_set_data(G_OBJECT(reedit), "identity-entry", entry);
        g_object_set_data(G_OBJECT(reedit), "identity-save", modify);
        g_object_set_data(G_OBJECT(restore), "identity-field", field);
        g_object_set_data(G_OBJECT(restore), "identity-entry", entry);
        g_object_set_data(G_OBJECT(restore), "identity-save", modify);
        g_object_set_data(G_OBJECT(show), "identity-field", field);
        gtk_box_append(GTK_BOX(actions), show);
        gtk_box_append(GTK_BOX(actions), accept);
        gtk_box_append(GTK_BOX(actions), modify);
        gtk_box_append(GTK_BOX(actions), reedit);
        if (identity_field_observation_get_raw_value(field) != NULL)
            gtk_box_append(GTK_BOX(actions), restore);
        gtk_box_append(GTK_BOX(actions), reject);
        gtk_box_append(GTK_BOX(row), actions);
        g_signal_connect(accept, "clicked",
            G_CALLBACK(create_person_dialog_on_field_accept), state);
        g_signal_connect(modify, "clicked",
            G_CALLBACK(create_person_dialog_on_field_modify), state);
        g_signal_connect(reject, "clicked",
            G_CALLBACK(create_person_dialog_on_field_reject), state);
        g_signal_connect(reedit, "clicked",
            G_CALLBACK(create_person_dialog_on_field_reedit), state);
        g_signal_connect(restore, "clicked",
            G_CALLBACK(create_person_dialog_on_field_restore), state);
        g_signal_connect(show, "clicked",
            G_CALLBACK(create_person_dialog_on_field_show), state);
        gtk_box_append(state->ocr_fields, row);
        g_free(source);
        g_free(label);
    }
}

static void create_person_dialog_ocr_completed(GObject *source,
    GAsyncResult *result, gpointer data)
{
    IdentityOcrJob *job = data;
    GtkWindow *window = g_weak_ref_get(&job->window);
    CreatePersonDialogState *state = window != NULL
        ? g_object_get_data(G_OBJECT(window), "person-dialog-state") : NULL;
    GError *error = NULL;
    IdentityOcrRun *run = g_task_propagate_pointer(G_TASK(result), &error);
    (void) source;
    if (state != NULL && state->ocr_generation == job->generation &&
        (state->session_check == NULL ||
         state->session_check(state->user_data))) {
        if (run != NULL) {
            gtk_text_buffer_set_text(gtk_text_view_get_buffer(state->ocr_text),
                identity_ocr_run_get_raw_text(run), -1);
            gtk_text_buffer_set_text(gtk_text_view_get_buffer(
                state->ocr_corrected_text),
                identity_ocr_run_get_raw_text(run), -1);
            gtk_text_view_set_editable(state->ocr_corrected_text, TRUE);
            gtk_stack_set_visible_child_name(
                state->ocr_transcription_stack, "corrected");
            create_person_dialog_render_ocr_fields(state, run);
            ocr_provenance_overlay_set_image(state->ocr_overlay,
                identity_ocr_run_get_preview(run), state->ocr_generation);
            ocr_provenance_overlay_set_page(state->ocr_overlay,
                identity_ocr_run_get_page(run), state->ocr_generation);
            const GPtrArray *fields = identity_ocr_run_get_fields(run);
            if (fields != NULL && fields->len > 0)
                ocr_provenance_overlay_set_field(state->ocr_overlay,
                    g_ptr_array_index((GPtrArray *) fields, 0),
                    state->ocr_generation);
            g_ptr_array_add(state->ocr_runs, run);
            if (state->factual_relation_editor != NULL) {
                person_factual_relation_editor_set_available_ocr_runs(
                    state->factual_relation_editor, state->ocr_runs);
            }
            run = NULL;
            gtk_label_set_text(state->ocr_status,
                "OCR terminé. Chaque proposition reste À vérifier.");
        } else
            gtk_label_set_text(state->ocr_status,
                error != NULL ? error->message : "OCR annulé.");
        g_clear_object(&state->ocr_task);
        g_clear_object(&state->ocr_cancellable);
    }
    identity_ocr_run_free(run);
    g_clear_error(&error);
    g_clear_object(&window);
}

static void create_person_dialog_on_ocr_cancel(GtkButton *button,
    gpointer data)
{
    CreatePersonDialogState *state = data;
    (void) button;
    state->ocr_generation++;
    if (state->ocr_cancellable != NULL)
        g_cancellable_cancel(state->ocr_cancellable);
    gtk_label_set_text(state->ocr_status, "Annulation de l’OCR…");
    ocr_provenance_overlay_clear(state->ocr_overlay, state->ocr_generation);
}

static void create_person_dialog_on_ocr_start(GtkButton *button,
    gpointer data)
{
    static const char *const types[] = {"identity_card","passport",
        "driving_licence","residence_permit","other"};
    static const char *const sides[] = {"front","back","identity_page",
        "other_page"};
    CreatePersonDialogState *state = data;
    const PersonEvidenceSelectionItem *item =
        person_evidence_selection_get_active(state->person_evidence_selection);
    IdentityOcrJob *job;
    const EvidenceRecord *record;
    (void) button;
    if (item == NULL || state->tesseract_path == NULL) {
        gtk_label_set_text(state->ocr_status,
            "Tesseract est absent ou aucune preuve compatible n’est retenue.");
        return;
    }
    job = g_new0(IdentityOcrJob, 1);
    job->root = g_strdup(state->investigation_root_path);
    job->identifier = g_strdup(
        person_evidence_selection_item_get_identifier(item));
    job->sha256 = g_strdup(person_evidence_selection_item_get_sha256(item));
    job->mime = g_strdup(person_evidence_selection_item_get_effective_ocr_mime(item));
    if (person_evidence_selection_item_get_origin(item) ==
        PERSON_EVIDENCE_ORIGIN_EXISTING) {
        record = person_evidence_selection_item_get_record(item);
        job->relative = g_strdup(evidence_record_get_relative_path(record));
    } else {
        g_free(job->root);
        job->root = g_path_get_dirname(
            person_evidence_selection_item_get_staging_path(item));
        job->relative = g_path_get_basename(
            person_evidence_selection_item_get_staging_path(item));
    }
    guint selected = gtk_drop_down_get_selected(state->ocr_document_type);
    job->document_type = g_strdup(types[MIN(selected,
        G_N_ELEMENTS(types)-1)]);
    selected = gtk_drop_down_get_selected(state->ocr_document_side);
    job->document_side = g_strdup(sides[MIN(selected,
        G_N_ELEMENTS(sides)-1)]);
    selected = gtk_drop_down_get_selected(state->ocr_languages);
    if (selected == GTK_INVALID_LIST_POSITION ||
        state->ocr_language_codes == NULL ||
        selected >= state->ocr_language_codes->len) {
        identity_ocr_job_free(job);
        gtk_label_set_text(state->ocr_status,
            "Aucune langue Tesseract disponible.");
        return;
    }
    job->languages = g_strdup(g_ptr_array_index(
        state->ocr_language_codes, selected));
    static const char *const profiles[] = {
        "none", "orientation", "grayscale", "upscale"};
    job->profile_index = gtk_drop_down_get_selected(state->ocr_profile);
    job->profile = g_strdup(profiles[MIN(job->profile_index,
        G_N_ELEMENTS(profiles)-1)]);
    job->page = (guint) gtk_spin_button_get_value_as_int(state->ocr_page);
    job->executable = g_strdup(state->tesseract_path);
    job->version = g_strdup(state->tesseract_version);
    job->generation = ++state->ocr_generation;
    g_weak_ref_init(&job->window, G_OBJECT(state->window));
    g_clear_object(&state->ocr_cancellable);
    state->ocr_cancellable = g_cancellable_new();
    state->ocr_task = g_task_new(NULL, state->ocr_cancellable,
        create_person_dialog_ocr_completed, job);
    g_task_set_task_data(state->ocr_task, job, identity_ocr_job_free);
    gtk_label_set_text(state->ocr_status,
        "OCR contrôlé en cours… Aucune donnée n’est encore persistée.");
    g_task_run_in_thread(state->ocr_task, create_person_dialog_ocr_worker);
}
static void create_person_dialog_state_free(gpointer data)
{
    CreatePersonDialogState *state = data;
    if (state == NULL) return;
    g_clear_pointer(&state->preview, evidence_preview_widget_free);
    if (state->staging_task != NULL) {
        background_task_cancel(state->staging_task);
        background_task_unref(state->staging_task);
    }
    state->ocr_generation++;
    if (state->ocr_cancellable != NULL)
        g_cancellable_cancel(state->ocr_cancellable);
    g_clear_object(&state->ocr_task);
    g_clear_object(&state->ocr_cancellable);
    g_clear_pointer(&state->ocr_runs, g_ptr_array_unref);
    g_clear_pointer(&state->ocr_language_codes, g_ptr_array_unref);
    g_clear_pointer(&state->ocr_overlay, ocr_provenance_overlay_free);
    g_clear_pointer(&state->role_buttons, g_ptr_array_unref);
    g_clear_pointer(&state->vocabularies, person_vocabulary_adapter_free);
    person_dialog_lifecycle_cancel(state->lifecycle);
    g_clear_pointer(&state->evidence_identifiers, g_ptr_array_unref);
    g_clear_pointer(&state->visible_records, g_ptr_array_unref);
    g_clear_pointer(&state->type_codes, g_ptr_array_unref);
    g_clear_pointer(&state->selection_model, evidence_selection_model_free);
    g_clear_pointer(&state->person_evidence_selection,
        person_evidence_selection_free);
    g_clear_pointer(&state->staging, evidence_staging_free);
    g_clear_pointer(&state->lifecycle, person_dialog_lifecycle_free);
    g_clear_object(&state->evidence_labels);
    g_clear_object(&state->retained_labels);
    g_clear_object(&state->type_filter_labels);
    g_free(state->investigation_root_path);
    g_free(state->tesseract_path);
    g_free(state->tesseract_version);
    person_factual_relation_editor_free(state->factual_relation_editor);
    person_ocr_projection_editor_free(state->ocr_projection_editor);
    if (state->user_data_destroy != NULL)
        state->user_data_destroy(state->user_data);
    g_free(state);
}

static void create_person_dialog_rebuild_retained(
    CreatePersonDialogState *state)
{
    GPtrArray *labels = g_ptr_array_new_with_free_func(g_free);
    const PersonEvidenceSelectionItem *active =
        person_evidence_selection_get_active(
            state->person_evidence_selection);
    const char *active_identifier = active != NULL
        ? person_evidence_selection_item_get_identifier(active) : NULL;
    guint selected = GTK_INVALID_LIST_POSITION;
    for (guint i = 0; i < person_evidence_selection_get_count(
            state->person_evidence_selection); i++) {
        const PersonEvidenceSelectionItem *item =
            person_evidence_selection_get(state->person_evidence_selection, i);
        char *label = g_strdup_printf("%s — %s — %s",
            person_evidence_selection_item_get_original_name(item),
            person_evidence_selection_item_get_origin(item) ==
                PERSON_EVIDENCE_ORIGIN_STAGED ? "nouvelle" : "existante",
            person_evidence_selection_item_get_type_identifier(item));
        g_ptr_array_add(labels, label);
        if (g_strcmp0(active_identifier,
                person_evidence_selection_item_get_identifier(item)) == 0)
            selected = i;
    }
    g_ptr_array_add(labels, NULL);
    state->updating_retained = TRUE;
    if (state->retained_selected_handler != 0)
        g_signal_handler_block(state->retained,
            state->retained_selected_handler);
    gtk_string_list_splice(state->retained_labels, 0,
        g_list_model_get_n_items(G_LIST_MODEL(state->retained_labels)),
        (const char * const *) labels->pdata);
    gtk_drop_down_set_selected(state->retained, selected);
    if (state->retained_selected_handler != 0)
        g_signal_handler_unblock(state->retained,
            state->retained_selected_handler);
    state->updating_retained = FALSE;
    g_ptr_array_unref(labels);
    if (selected != GTK_INVALID_LIST_POSITION)
        create_person_dialog_on_retained_changed(
            state->retained, NULL, state);
}

static void create_person_dialog_on_add_existing(
    GtkButton *button, gpointer data)
{
    CreatePersonDialogState *state = data;
    const EvidenceRecord *record =
        evidence_selection_model_get_selected(state->selection_model);
    GError *error = NULL;
    (void) button;
    if (record == NULL && state->visible_records->len == 1)
        record = g_ptr_array_index(state->visible_records, 0);
    if (record == NULL) {
        gtk_label_set_text(state->error,
            "Sélectionnez d’abord une preuve existante.");
        gtk_widget_set_visible(GTK_WIDGET(state->error), TRUE);
        return;
    }
    if (!person_evidence_selection_add_existing(
            state->person_evidence_selection, record, &error)) {
        gtk_label_set_text(state->error, error->message);
        gtk_widget_set_visible(GTK_WIDGET(state->error), TRUE);
    } else {
        gtk_widget_set_visible(GTK_WIDGET(state->error), FALSE);
        create_person_dialog_rebuild_retained(state);
    }
    g_clear_error(&error);
}

static void create_person_dialog_on_retained_changed(
    GtkDropDown *dropdown, GParamSpec *pspec, gpointer data)
{
    CreatePersonDialogState *state = data;
    guint selected = gtk_drop_down_get_selected(dropdown);
    const PersonEvidenceSelectionItem *item =
        person_evidence_selection_get(state->person_evidence_selection,
            selected);
    (void) pspec;
    if (state->updating_retained ||
        selected == GTK_INVALID_LIST_POSITION || item == NULL) return;
    if (state->ocr_start != NULL) {
        const char *mime =
            person_evidence_selection_item_get_mime_type(item);
        const char *name =
            person_evidence_selection_item_get_original_name(item);
        char *lower = name != NULL ? g_ascii_strdown(name, -1) : NULL;
        gboolean compatible =
            g_strcmp0(mime, "image/png") == 0 ||
            g_strcmp0(mime, "image/jpeg") == 0 ||
            g_strcmp0(mime, "image/heic") == 0 ||
            g_strcmp0(mime, "image/heif") == 0 ||
            g_strcmp0(mime, "application/pdf") == 0 ||
            (lower != NULL && (g_str_has_suffix(lower, ".png") ||
             g_str_has_suffix(lower, ".jpg") ||
             g_str_has_suffix(lower, ".jpeg") ||
             g_str_has_suffix(lower, ".heic") ||
             g_str_has_suffix(lower, ".heif") ||
             g_str_has_suffix(lower, ".pdf")));
        gtk_widget_set_sensitive(GTK_WIDGET(state->ocr_start),
            compatible && state->tesseract_path != NULL);
        g_free(lower);
    }
    person_evidence_selection_set_active(state->person_evidence_selection,
        person_evidence_selection_item_get_identifier(item));
    if (state->ocr_overlay != NULL)
        ocr_provenance_overlay_clear(state->ocr_overlay,
            ++state->ocr_generation);
    static const char *const types[] = {
        "screenshot", "photo", "video", "document", "email",
        "archive", "audio", "text", "other"};
    for (guint i = 0; i < G_N_ELEMENTS(types); i++)
        if (g_strcmp0(types[i],
                person_evidence_selection_item_get_type_identifier(item)) == 0)
            gtk_drop_down_set_selected(state->retained_type, i);
    if (person_evidence_selection_item_get_origin(item) ==
        PERSON_EVIDENCE_ORIGIN_EXISTING)
        create_person_dialog_select_record(state,
            person_evidence_selection_item_get_record(item),
            person_evidence_selection_item_get_type_identifier(item));
    else {
        char *size = g_format_size(
            person_evidence_selection_item_get_size_bytes(item));
        char *text = g_strdup_printf(
            "Nom : %s\nOrigine : nouvelle (staging)\nMIME : %s\n"
            "Type : %s\nTaille : %s\nSHA-256 : %.12s…",
            person_evidence_selection_item_get_original_name(item),
            person_evidence_selection_item_get_mime_type(item),
            person_evidence_selection_item_get_type_identifier(item), size,
            person_evidence_selection_item_get_sha256(item));
        create_person_dialog_clear_preview(state);
        {
            char *staging_root = g_path_get_dirname(
                person_evidence_selection_item_get_staging_path(item));
            char *staging_name = g_path_get_basename(
                person_evidence_selection_item_get_staging_path(item));
            EvidencePreviewRequest *request = evidence_preview_request_new(
                staging_root,
                person_evidence_selection_item_get_identifier(item),
                staging_name,
                person_evidence_selection_item_get_sha256(item),
                person_evidence_selection_item_get_mime_type(item),
                person_dialog_lifecycle_get_generation(state->lifecycle));
            evidence_preview_widget_show(state->preview, request, text);
            evidence_preview_request_free(request);
            g_free(staging_root); g_free(staging_name);
        }
        g_free(text); g_free(size);
    }
}

static void create_person_dialog_on_retained_type_changed(
    GtkDropDown *dropdown, GParamSpec *pspec, gpointer data)
{
    static const char *const types[] = {
        "screenshot", "photo", "video", "document", "email",
        "archive", "audio", "text", "other"};
    CreatePersonDialogState *state = data;
    guint selected = gtk_drop_down_get_selected(dropdown);
    const PersonEvidenceSelectionItem *item =
        person_evidence_selection_get_active(
            state->person_evidence_selection);
    (void) pspec;
    if (item != NULL && selected < G_N_ELEMENTS(types)) {
        person_evidence_selection_set_type(state->person_evidence_selection,
            person_evidence_selection_item_get_identifier(item),
            types[selected]);
        create_person_dialog_rebuild_retained(state);
    }
}

static void create_person_dialog_on_remove_retained(
    GtkButton *button, gpointer data)
{
    CreatePersonDialogState *state = data;
    const PersonEvidenceSelectionItem *item =
        person_evidence_selection_get_active(
            state->person_evidence_selection);
    char *identifier = item != NULL ? g_strdup(
        person_evidence_selection_item_get_identifier(item)) : NULL;
    char *staging_path = item != NULL &&
        person_evidence_selection_item_get_origin(item) ==
            PERSON_EVIDENCE_ORIGIN_STAGED
        ? g_strdup(person_evidence_selection_item_get_staging_path(item))
        : NULL;
    (void) button;
    if (identifier != NULL) {
        state->ocr_generation++;
        if (state->ocr_cancellable != NULL)
            g_cancellable_cancel(state->ocr_cancellable);
        for (guint i = state->ocr_runs != NULL ? state->ocr_runs->len : 0;
             i > 0; i--) {
            IdentityOcrRun *run = g_ptr_array_index(state->ocr_runs, i - 1);
            if (g_strcmp0(identity_ocr_run_get_evidence_id(run),
                    identifier) == 0)
                g_ptr_array_remove_index(state->ocr_runs, i - 1);
        }
        while (gtk_widget_get_first_child(GTK_WIDGET(state->ocr_fields)) !=
               NULL)
            gtk_box_remove(state->ocr_fields,
                gtk_widget_get_first_child(GTK_WIDGET(state->ocr_fields)));
        gtk_text_buffer_set_text(gtk_text_view_get_buffer(state->ocr_text),
            "", -1);
        gtk_label_set_text(state->ocr_status,
            "Aucune session OCR active pour la preuve retirée.");
        ocr_provenance_overlay_clear(state->ocr_overlay,
            state->ocr_generation);
        person_evidence_selection_remove(
            state->person_evidence_selection, identifier);
        if (staging_path != NULL)
            evidence_staging_remove(state->staging, staging_path, NULL);
        create_person_dialog_rebuild_retained(state);
        create_person_dialog_clear_preview(state);
    }
    g_free(identifier); g_free(staging_path);
}

static void staging_context_free(gpointer data)
{
    CreatePersonStagingContext *context = data;
    if (context == NULL) return;
    g_weak_ref_clear(&context->window);
    g_free(context);
}
static void create_person_dialog_staging_completed(
    BackgroundTask *task, gpointer data)
{
    CreatePersonStagingContext *context = data;
    GtkWindow *window = g_weak_ref_get(&context->window);
    CreatePersonDialogState *state = window != NULL
        ? g_object_get_data(G_OBJECT(window), "person-dialog-state") : NULL;
    GPtrArray *prepared = background_task_get_result(task);
    if (state != NULL && state->session_check != NULL &&
        !state->session_check(state->user_data)) {
        person_dialog_lifecycle_cancel(state->lifecycle);
        evidence_staging_cleanup(state->staging, NULL);
        gtk_label_set_text(state->error,
            "Préparation annulée : l’enquête active a changé.");
        gtk_widget_set_visible(GTK_WIDGET(state->error), TRUE);
    } else if (state != NULL &&
        person_dialog_lifecycle_accepts_preview(
            state->lifecycle, context->generation) && prepared != NULL) {
        for (guint i = 0; i < prepared->len; i++) {
            EvidenceStagingResult *item = g_ptr_array_index(prepared, i);
            const EvidenceRecord *existing =
                evidence_selection_model_find_by_sha256(
                    state->selection_model, item->sha256);
            GError *error = NULL;
            if (existing != NULL) {
                if (!person_evidence_selection_add_existing(
                        state->person_evidence_selection,
                        existing, &error) && error != NULL &&
                    error->code != 2)
                    gtk_label_set_text(state->error, error->message);
                evidence_staging_remove(state->staging,
                    item->staging_path, NULL);
                gtk_label_set_text(state->error,
                    "Doublon détecté : la preuve existante a été réutilisée.");
                gtk_widget_set_visible(GTK_WIDGET(state->error), TRUE);
            } else if (!person_evidence_selection_add_staged(
                    state->person_evidence_selection,
                    item->source_path, item->staging_path,
                    item->original_name, item->mime_type,
                    item->suggested_type, item->size_bytes, item->sha256,
                    NULL, item->prepared_at, &error)) {
                evidence_staging_remove(state->staging,
                    item->staging_path, NULL);
                gtk_label_set_text(state->error,
                    error != NULL ? error->message :
                    "Doublon refusé dans la sélection.");
                gtk_widget_set_visible(GTK_WIDGET(state->error), TRUE);
            }
            g_clear_error(&error);
        }
        create_person_dialog_rebuild_retained(state);
    } else if (state != NULL) {
        GError *error = background_task_dup_error(task);
        gtk_label_set_text(state->error,
            error != NULL ? error->message :
            "La préparation des preuves a échoué.");
        gtk_widget_set_visible(GTK_WIDGET(state->error), TRUE);
        g_clear_error(&error);
    }
    if (state != NULL && state->staging_task == task) {
        background_task_unref(state->staging_task);
        state->staging_task = NULL;
    }
    g_clear_object(&window);
}
static void create_person_dialog_files_selected(
    GObject *source, GAsyncResult *async_result, gpointer data)
{
    CreatePersonStagingContext *selection_context = data;
    GtkWindow *window = g_weak_ref_get(&selection_context->window);
    CreatePersonDialogState *state = window != NULL
        ? g_object_get_data(G_OBJECT(window), "person-dialog-state") : NULL;
    GListModel *files;
    GPtrArray *paths;
    GError *error = NULL;
    CreatePersonStagingContext *context;
    files = gtk_file_dialog_open_multiple_finish(
        GTK_FILE_DIALOG(source), async_result, &error);
    if (state == NULL) {
        g_clear_object(&files);
        g_clear_error(&error);
        staging_context_free(selection_context);
        g_clear_object(&window);
        return;
    }
    if (files == NULL) {
        if (!g_error_matches(error, GTK_DIALOG_ERROR,
                GTK_DIALOG_ERROR_DISMISSED)) {
            gtk_label_set_text(state->error, error->message);
            gtk_widget_set_visible(GTK_WIDGET(state->error), TRUE);
        }
        g_clear_error(&error);
        staging_context_free(selection_context);
        g_clear_object(&window);
        return;
    }
    paths = g_ptr_array_new_with_free_func(g_free);
    for (guint i = 0; i < g_list_model_get_n_items(files); i++) {
        GFile *file = g_list_model_get_item(files, i);
        char *path = g_file_get_path(file);
        if (path != NULL) g_ptr_array_add(paths, path);
        g_object_unref(file);
    }
    g_object_unref(files);
    if (paths->len == 0) {
        g_ptr_array_unref(paths);
        staging_context_free(selection_context);
        g_clear_object(&window);
        return;
    }
    context = g_new0(CreatePersonStagingContext, 1);
    g_weak_ref_init(&context->window, G_OBJECT(state->window));
    context->generation =
        person_dialog_lifecycle_begin_preview(state->lifecycle);
    state->staging_task = evidence_staging_task_start(state->task_manager,
        state->staging, paths, create_person_dialog_staging_completed,
        context, staging_context_free, &error);
    if (state->staging_task == NULL) {
        staging_context_free(context);
        gtk_label_set_text(state->error,
            error != NULL ? error->message :
            "Impossible de démarrer le staging.");
        gtk_widget_set_visible(GTK_WIDGET(state->error), TRUE);
    }
    g_clear_error(&error);
    g_ptr_array_unref(paths);
    staging_context_free(selection_context);
    g_clear_object(&window);
}
static void create_person_dialog_on_import_files(
    GtkButton *button, gpointer data)
{
    CreatePersonDialogState *state = data;
    GtkFileDialog *dialog;
    CreatePersonStagingContext *context;
    (void) button;
    if (state->staging_task != NULL) return;
    dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog,
        "Importer des fichiers dans le staging");
    context = g_new0(CreatePersonStagingContext, 1);
    g_weak_ref_init(&context->window, G_OBJECT(state->window));
    gtk_file_dialog_open_multiple(dialog, state->window, NULL,
        create_person_dialog_files_selected, context);
    g_object_unref(dialog);
}

static void create_person_dialog_rebuild_evidence(CreatePersonDialogState *state)
{
    GPtrArray *labels = g_ptr_array_new_with_free_func(g_free);
    const EvidenceRecord *selected =
        evidence_selection_model_get_selected(state->selection_model);
    const char *selected_id = selected != NULL
        ? evidence_record_get_identifier(selected) : NULL;
    guint selected_index = 0;
    g_ptr_array_add(labels, g_strdup("Aucune preuve associée"));
    g_ptr_array_set_size(state->evidence_identifiers, 0);
    g_clear_pointer(&state->visible_records, g_ptr_array_unref);
    state->visible_records =
        evidence_selection_model_list_visible(state->selection_model);
    for (guint i = 0; i < state->visible_records->len; i++) {
        EvidenceRecord *record = g_ptr_array_index(state->visible_records, i);
        g_ptr_array_add(labels,
            g_strdup(evidence_record_get_original_name(record)));
        g_ptr_array_add(state->evidence_identifiers,
            g_strdup(evidence_record_get_identifier(record)));
        if (g_strcmp0(selected_id,
                evidence_record_get_identifier(record)) == 0)
            selected_index = i + 1;
    }
    g_ptr_array_add(labels, NULL);
    state->updating_evidence = TRUE;
    if (state->evidence_selected_handler != 0)
        g_signal_handler_block(state->evidence,
            state->evidence_selected_handler);
    gtk_string_list_splice(state->evidence_labels, 0,
        g_list_model_get_n_items(G_LIST_MODEL(state->evidence_labels)),
        (const char * const *) labels->pdata);
    gtk_drop_down_set_selected(state->evidence, selected_index);
    if (state->evidence_selected_handler != 0)
        g_signal_handler_unblock(state->evidence,
            state->evidence_selected_handler);
    state->updating_evidence = FALSE;
    g_ptr_array_unref(labels);
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
    const char *identifier = NULL;
    const EvidenceRecord *record = NULL;
    (void) pspec;
    if (state->updating_evidence) return;
    if (selected != GTK_INVALID_LIST_POSITION && selected > 0 &&
        selected - 1 < state->evidence_identifiers->len)
        identifier = g_ptr_array_index(
            state->evidence_identifiers, selected - 1);
    if (identifier != NULL)
        record = evidence_selection_model_find_by_identifier(
            state->selection_model, identifier);
    create_person_dialog_select_record(state, record, NULL);
}
static void create_person_dialog_cancel(CreatePersonDialogState *state)
{
    if (state == NULL ||
        !person_dialog_lifecycle_cancel(state->lifecycle)) return;
    evidence_preview_widget_cancel(state->preview);
    if (state->callback != NULL) state->callback(NULL, state->user_data);
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
    person_dialog_lifecycle_begin_preview(state->lifecycle);
    evidence_preview_widget_clear(state->preview);
}

static void create_person_dialog_select_record(CreatePersonDialogState *state,
    const EvidenceRecord *record, const char *business_type)
{
    char *size = NULL, *sha = NULL, *text = NULL;
    const char *mime;
    const char *type_identifier;
    EvidencePreviewRequest *request;
    create_person_dialog_clear_preview(state);
    if (record == NULL) return;
    evidence_selection_model_select(state->selection_model,
        evidence_record_get_identifier(record));
    size = g_format_size(evidence_record_get_size_bytes(record));
    sha = g_strndup(evidence_record_get_sha256(record), 12);
    mime = evidence_record_get_mime_type(record);
    type_identifier = business_type != NULL ? business_type :
        evidence_record_get_type_identifier(record);
    text = g_strdup_printf(
        "Nom : %s\nType : %s (%s)\nMIME : %s\nTaille : %s\n"
        "Import : %s\nSHA-256 : %s…\nIntégrité : %s\nDescription : %s",
        evidence_record_get_original_name(record),
        type_identifier, type_identifier,
        mime != NULL ? mime : "Non renseigné", size,
        evidence_record_get_imported_at(record), sha,
        integrity_label(evidence_record_get_integrity_status(record)),
        evidence_record_get_description(record) != NULL
            ? evidence_record_get_description(record) : "Aucune");
    request = evidence_preview_request_new(state->investigation_root_path,
        evidence_record_get_identifier(record),
        evidence_record_get_relative_path(record),
        evidence_record_get_sha256(record), mime,
        person_dialog_lifecycle_get_generation(state->lifecycle));
    evidence_preview_widget_show(state->preview, request, text);
    evidence_preview_request_free(request);
    g_free(size); g_free(sha); g_free(text);
}
static gboolean create_person_dialog_on_close(GtkWindow *window, gpointer data)
{
    (void) window; create_person_dialog_cancel(data); return FALSE;
}
static void create_person_dialog_on_cancel(GtkButton *button, gpointer data)
{
    CreatePersonDialogState *state = data;
    (void) button; create_person_dialog_cancel(state); gtk_window_close(state->window);
}
static void create_person_dialog_on_create(GtkButton *button, gpointer data)
{
    CreatePersonDialogState *state = data;
    CreatePersonDialogResult *result = NULL;
    char *notes = NULL;
    guint status = gtk_drop_down_get_selected(state->status);
    const GPtrArray *statuses =
        person_vocabulary_adapter_get_statuses(state->vocabularies);
    (void) button;
    if (state->session_check != NULL &&
        !state->session_check(state->user_data)) {
        gtk_label_set_text(state->error,
            "L'enquête active a changé : création refusée.");
        gtk_widget_set_visible(GTK_WIDGET(state->error), TRUE);
        return;
    }
    if (statuses == NULL || status >= statuses->len ||
        gtk_editable_get_text(GTK_EDITABLE(state->designation))[0] == '\0')
    {
        gtk_label_set_text(state->error, "La désignation de la personne est obligatoire.");
        gtk_widget_set_visible(GTK_WIDGET(state->error), TRUE); return;
    }
    notes = create_person_dialog_get_notes(state);
    result = g_new0(CreatePersonDialogResult, 1);
    result->designation = g_strdup(gtk_editable_get_text(GTK_EDITABLE(state->designation)));
    result->declared_name = g_strdup(gtk_editable_get_text(GTK_EDITABLE(state->name)));
    result->pseudonym = g_strdup(gtk_editable_get_text(GTK_EDITABLE(state->pseudonym)));
    result->status = g_strdup(person_vocabulary_adapter_status_code(state->vocabularies, status));
    result->notes = g_strdup(notes);
    if (result->designation) { g_strstrip(result->designation); if (result->designation[0] == '\0') { g_free(result->designation); result->designation = NULL; } }
    if (result->declared_name) { g_strstrip(result->declared_name); if (result->declared_name[0] == '\0') { g_free(result->declared_name); result->declared_name = NULL; } }
    if (result->pseudonym) { g_strstrip(result->pseudonym); if (result->pseudonym[0] == '\0') { g_free(result->pseudonym); result->pseudonym = NULL; } }
    if (result->notes) { g_strstrip(result->notes); if (result->notes[0] == '\0') { g_free(result->notes); result->notes = NULL; } }
    if (person_evidence_selection_get_count(
            state->person_evidence_selection) > 0) {
        const PersonEvidenceSelectionItem *first =
            person_evidence_selection_get(
                state->person_evidence_selection, 0);
        result->evidence_identifier = g_strdup(
            person_evidence_selection_item_get_evidence_identifier(first));
    }
    result->input.designation = result->designation;
    result->input.declared_name = result->declared_name;
    result->input.pseudonym = result->pseudonym;
    result->input.identification_status = result->status;
    result->input.notes = result->notes;
    result->input.confidence = gtk_spin_button_get_value_as_int(state->confidence);
    result->input.evidence_identifier = result->evidence_identifier;
    result->role_assignments =
        person_vocabulary_adapter_build_role_assignments(
            state->vocabularies, state->role_buttons,
            result->evidence_identifier);
    result->input.role_assignments = result->role_assignments;
    GError *relation_error = NULL;
    if (!person_factual_relation_editor_collect_relations(state->factual_relation_editor,
            &result->factual_relations, &relation_error)) {
        gtk_label_set_text(state->error, relation_error->message);
        gtk_widget_set_visible(GTK_WIDGET(state->error), TRUE);
        g_clear_error(&relation_error); create_person_dialog_result_free(result);
        g_free(notes); return;
    }
    if (!person_ocr_projection_editor_collect(state->ocr_projection_editor,&result->ocr_projections,&relation_error)) {
        gtk_label_set_text(state->error, relation_error->message);
        gtk_widget_set_visible(GTK_WIDGET(state->error), TRUE);
        g_clear_error(&relation_error);create_person_dialog_result_free(result);g_free(notes);return;
    }
    result->evidence_selection = state->person_evidence_selection;
    state->person_evidence_selection = NULL;
    result->staging = state->staging;
    state->staging = NULL;
    result->ocr_runs = state->ocr_runs;
    state->ocr_runs = NULL;
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
    static const char *const pages[] = {
        "person", "roles", "evidence", "identity-ocr", "ocr-projection",
        "factual-relations", "summary"};
    static const char *const steps[] = {
        "1 Personne", "2 Rôles", "3 Preuves", "4 OCR identité",
        "5 Projection OCR", "6 Relations factuelles", "7 Confirmation"};
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
    if (state->step == 6) {
        GPtrArray *role_labels =
            person_vocabulary_adapter_selected_role_labels(
                state->role_buttons);
        char *notes = create_person_dialog_get_notes(state);
        char *text;
        guint selected_status = gtk_drop_down_get_selected(state->status);
        const char *status_label =
            person_vocabulary_adapter_status_label(
                state->vocabularies, selected_status);
        text = person_confirmation_summary_build_multiple(
            gtk_editable_get_text(GTK_EDITABLE(state->designation)),
            gtk_editable_get_text(GTK_EDITABLE(state->name)),
            gtk_editable_get_text(GTK_EDITABLE(state->pseudonym)),
            status_label,
            gtk_spin_button_get_value_as_int(state->confidence), notes,
            role_labels, state->person_evidence_selection);
        GString *confirmation = g_string_new(text);
        person_creation_confirmation_append_sections(confirmation,
            state->ocr_runs, state->ocr_projection_editor,
            state->factual_relation_editor);
        gtk_label_set_text(state->summary, confirmation->str);
        GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment(
            state->summary_scroll);
        gtk_adjustment_set_value(adjustment,
            gtk_adjustment_get_lower(adjustment));
        g_string_free(confirmation, TRUE);
        g_free(text);
        g_free(notes);
        g_ptr_array_unref(role_labels);
    }
    gtk_widget_set_sensitive(GTK_WIDGET(state->previous), state->step > 0);
    gtk_widget_set_visible(GTK_WIDGET(state->next), state->step < 6);
    gtk_widget_set_visible(GTK_WIDGET(state->create), state->step == 6);
    gtk_widget_set_sensitive(GTK_WIDGET(state->create),
        state->step == 6 &&
        gtk_editable_get_text(GTK_EDITABLE(state->designation))[0] != '\0' &&
        person_evidence_selection_is_confirmable(
            state->person_evidence_selection));
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
    if (state->step == 3) {
        create_person_dialog_capture_corrected_transcription(state);
        create_person_dialog_capture_factual_notes(state);
        person_ocr_projection_editor_set_runs(state->ocr_projection_editor,
            state->ocr_runs);
    }
    if (state->step == 4) {
        GError *error = NULL;
        GPtrArray *projections = NULL;
        if (!person_ocr_projection_editor_collect(state->ocr_projection_editor,
                &projections, &error)) {
            gtk_label_set_text(state->error, error->message);
            gtk_widget_set_visible(GTK_WIDGET(state->error), TRUE);
            g_clear_error(&error); return;
        }
        g_ptr_array_unref(projections);
    }
    if (state->step == 5) {
        GError *error = NULL;
        if (!person_factual_relation_editor_validate(state->factual_relation_editor,
                &error)) {
            gtk_label_set_text(state->error, error->message);
            gtk_widget_set_visible(GTK_WIDGET(state->error), TRUE);
            g_clear_error(&error); return;
        }
    }
    gtk_widget_set_visible(GTK_WIDGET(state->error), FALSE);
    if (state->step < 6) state->step++;
    create_person_dialog_update_navigation(state);
}
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
    Database *database, const GPtrArray *records,
    const char *investigation_root_path,
    TaskManager *task_manager, const ToolInfo *tesseract_tool,
    CreatePersonDialogSessionCheck session_check,
    CreatePersonDialogCallback callback, gpointer data,
    GDestroyNotify data_destroy)
{
    CreatePersonDialogState *state = NULL;
    GtkWidget *box = NULL, *grid = NULL, *actions = NULL, *cancel = NULL;
    GtkWidget *roles_box = NULL, *evidence_box = NULL, *ocr_box = NULL;
    GtkWidget *summary = NULL;
    if (parent == NULL || database == NULL ||
        investigation_root_path == NULL ||
        task_manager == NULL) return FALSE;
    state = g_new0(CreatePersonDialogState, 1);
    GError *vocabulary_error = NULL;
    state->vocabularies = person_vocabulary_adapter_new(
        database, &vocabulary_error);
    if (state->vocabularies == NULL) {
        g_clear_error(&vocabulary_error);
        g_free(state);
        return FALSE;
    }
    state->callback = callback; state->session_check = session_check;
    state->user_data = data; state->user_data_destroy = data_destroy;
    state->evidence_identifiers = g_ptr_array_new_with_free_func(g_free);
    state->selection_model = evidence_selection_model_new(records);
    state->person_evidence_selection = person_evidence_selection_new();
    state->staging = evidence_staging_new(NULL);
    if (state->selection_model == NULL ||
        state->person_evidence_selection == NULL || state->staging == NULL) {
        create_person_dialog_state_free(state);
        return FALSE;
    }
    state->visible_records =
        evidence_selection_model_list_visible(state->selection_model);
    state->type_codes =
        evidence_selection_model_list_type_codes(state->selection_model);
    state->investigation_root_path = g_strdup(investigation_root_path);
    state->task_manager = task_manager;
    state->ocr_runs = g_ptr_array_new_with_free_func(
        (GDestroyNotify) identity_ocr_run_free);
    if (tesseract_tool != NULL &&
        tool_info_get_availability(tesseract_tool) ==
            TOOL_AVAILABILITY_AVAILABLE) {
        state->tesseract_path = g_strdup(
            tool_info_get_resolved_path(tesseract_tool));
        state->tesseract_version = g_strdup(
            tool_info_get_detected_version(tesseract_tool));
    }
    state->lifecycle = person_dialog_lifecycle_new();
    state->window = GTK_WINDOW(gtk_window_new());
    gtk_window_set_application(state->window,
        gtk_window_get_application(parent));
    gtk_window_set_title(state->window, "Ajouter une personne");
    labfy_dialog_prepare(state->window, parent, TRUE, TRUE);
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(box, 16); gtk_widget_set_margin_end(box, 16);
    gtk_widget_set_margin_top(box, 16); gtk_widget_set_margin_bottom(box, 16);
    grid = gtk_grid_new(); gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    state->designation = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(state->designation, "Personne présumée liée aux comptes");
    state->name = GTK_ENTRY(gtk_entry_new());
    state->pseudonym = GTK_ENTRY(gtk_entry_new());
    GtkStringList *status_labels =
        person_vocabulary_adapter_create_status_labels(state->vocabularies);
    state->status = GTK_DROP_DOWN(gtk_drop_down_new(
        G_LIST_MODEL(status_labels), NULL));
    gtk_drop_down_set_selected(state->status, 1);
    state->confidence = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(0, 100, 5));
    gtk_spin_button_set_value(state->confidence, 30);
    state->evidence_labels = gtk_string_list_new(NULL);
    gtk_string_list_append(state->evidence_labels,
        "Aucune preuve associée");
    for (guint i = 0; i < state->visible_records->len; i++)
    {
        EvidenceRecord *record = g_ptr_array_index(state->visible_records, i);
        const char *id = evidence_record_get_identifier(record);
        const char *name = evidence_record_get_original_name(record);
        if (id == NULL || name == NULL) continue;
        gtk_string_list_append(state->evidence_labels, name);
        g_ptr_array_add(state->evidence_identifiers, g_strdup(id));
    }
    state->evidence = GTK_DROP_DOWN(gtk_drop_down_new(
        G_LIST_MODEL(g_object_ref(state->evidence_labels)), NULL));
    gtk_widget_set_name(GTK_WIDGET(state->evidence),
        "create-person-evidence-dropdown");
    state->search = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(state->search,
        "Rechercher par nom, description ou type");
    state->type_filter_labels = gtk_string_list_new(NULL);
    gtk_string_list_append(state->type_filter_labels, "Tous les types");
    for (guint i = 0; i < state->type_codes->len; i++)
        gtk_string_list_append(state->type_filter_labels,
            g_ptr_array_index(state->type_codes, i));
    state->type_filter = GTK_DROP_DOWN(
        gtk_drop_down_new(G_LIST_MODEL(
            g_object_ref(state->type_filter_labels)), NULL));
    gtk_widget_set_name(GTK_WIDGET(state->type_filter),
        "create-person-evidence-type-filter");
    /*
     * gtk_drop_down_new() prend la propriété complète du modèle. Une
     * libération ici laisserait les vues internes du GtkDropDown avec un
     * GListModel détruit et ferait échouer leur nettoyage à la fermeture.
     */
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
    gtk_widget_set_name(GTK_WIDGET(state->stack),
        "create-person-assistant-stack");
    gtk_stack_set_transition_type(state->stack,
        GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_add_titled(state->stack, grid, "person", "1 — Personne");
    roles_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    state->role_buttons =
        person_vocabulary_adapter_create_role_buttons(
            state->vocabularies, GTK_BOX(roles_box));
    gtk_stack_add_titled(state->stack, roles_box, "roles", "2 — Rôles");
    evidence_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_box_append(GTK_BOX(evidence_box), gtk_label_new(
        "Sélectionnez des preuves existantes et/ou préparez de nouveaux fichiers."));
    gtk_box_append(GTK_BOX(evidence_box), GTK_WIDGET(state->search));
    gtk_box_append(GTK_BOX(evidence_box), GTK_WIDGET(state->type_filter));
    gtk_box_append(GTK_BOX(evidence_box), GTK_WIDGET(state->evidence));
    {
        GtkWidget *evidence_actions =
            gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        state->add_existing = GTK_BUTTON(
            gtk_button_new_with_label("Ajouter à la sélection"));
        state->import_files = GTK_BUTTON(
            gtk_button_new_with_label("Importer des fichiers"));
        gtk_widget_set_tooltip_text(GTK_WIDGET(state->add_existing),
            "Retenir la preuve existante affichée");
        gtk_widget_set_tooltip_text(GTK_WIDGET(state->import_files),
            "Créer des copies temporaires sans import définitif");
        gtk_box_append(GTK_BOX(evidence_actions),
            GTK_WIDGET(state->add_existing));
        gtk_box_append(GTK_BOX(evidence_actions),
            GTK_WIDGET(state->import_files));
        gtk_box_append(GTK_BOX(evidence_box), evidence_actions);
    }
    gtk_box_append(GTK_BOX(evidence_box),
        gtk_label_new("Preuves retenues"));
    {
        state->retained_labels = gtk_string_list_new(NULL);
        state->retained = GTK_DROP_DOWN(gtk_drop_down_new(
            G_LIST_MODEL(g_object_ref(state->retained_labels)), NULL));
    }
    gtk_box_append(GTK_BOX(evidence_box), GTK_WIDGET(state->retained));
    {
        static const char *const type_labels_fr[] = {
            "Capture d’écran", "Photo", "Vidéo", "Document", "E-mail",
            "Archive", "Audio", "Texte", "Autre", NULL};
        GtkWidget *retained_actions =
            gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        state->retained_type = GTK_DROP_DOWN(
            gtk_drop_down_new_from_strings(type_labels_fr));
        state->remove_retained = GTK_BUTTON(
            gtk_button_new_with_label("Retirer"));
        gtk_widget_set_tooltip_text(GTK_WIDGET(state->remove_retained),
            "Retirer uniquement de la sélection");
        gtk_box_append(GTK_BOX(retained_actions),
            GTK_WIDGET(state->retained_type));
        gtk_box_append(GTK_BOX(retained_actions),
            GTK_WIDGET(state->remove_retained));
        gtk_box_append(GTK_BOX(evidence_box), retained_actions);
    }
    state->preview = evidence_preview_widget_new(state->task_manager,
        (EvidencePreviewWidgetSessionCheck) state->session_check,
        state->user_data);
    if (state->preview == NULL) {
        create_person_dialog_state_free(state);
        return FALSE;
    }
    {
        GtkWidget *evidence_paned =
            gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_widget_set_name(evidence_paned, "create-person-evidence-paned");
        gtk_widget_set_size_request(evidence_box, 320, -1);
        GtkWidget *evidence_scroll = gtk_scrolled_window_new();
        gtk_widget_set_name(evidence_scroll,
            "create-person-evidence-scroll");
        gtk_scrolled_window_set_policy(
            GTK_SCROLLED_WINDOW(evidence_scroll),
            GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_child(
            GTK_SCROLLED_WINDOW(evidence_scroll), evidence_box);
        gtk_paned_set_start_child(
            GTK_PANED(evidence_paned), evidence_scroll);
        gtk_paned_set_end_child(GTK_PANED(evidence_paned),
            evidence_preview_widget_get_widget(state->preview));
        labfy_paned_apply_initial_ratio(
            GTK_PANED(evidence_paned), 2.0 / 3.0, 480, 240);
        gtk_paned_set_resize_start_child(
            GTK_PANED(evidence_paned), FALSE);
        gtk_paned_set_resize_end_child(GTK_PANED(evidence_paned), TRUE);
        gtk_stack_add_titled(state->stack, evidence_paned,
            "evidence", "3 — Preuves");
    }
    ocr_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_name(ocr_box, "create-person-ocr-review-content");
    GtkWidget *ocr_warning = gtk_label_new(
        "L’OCR produit des propositions à vérifier. Il ne confirme ni "
        "l’identité ni l’authenticité du document.");
    gtk_label_set_wrap(GTK_LABEL(ocr_warning), TRUE);
    static const char *const document_types[] = {
        "Carte nationale d’identité", "Passeport", "Permis de conduire",
        "Titre de séjour", "Autre document d’identité", NULL};
    static const char *const document_sides[] = {
        "Recto", "Verso", "Page d’identité", "Autre page", NULL};
    static const char *const profile_labels[] = {
        "Aucun prétraitement", "Orientation normalisée",
        "Niveaux de gris et contraste modéré",
        "Agrandissement contrôlé", NULL};
    state->ocr_document_type = GTK_DROP_DOWN(
        gtk_drop_down_new_from_strings(document_types));
    state->ocr_document_side = GTK_DROP_DOWN(
        gtk_drop_down_new_from_strings(document_sides));
    {
        static const char *const pending_languages[] = {
            "Détection des langues…", NULL};
        state->ocr_languages = GTK_DROP_DOWN(
            gtk_drop_down_new_from_strings(pending_languages));
        gtk_widget_set_name(GTK_WIDGET(state->ocr_languages),
            "create-person-ocr-languages");
    }
    state->ocr_profile = GTK_DROP_DOWN(
        gtk_drop_down_new_from_strings(profile_labels));
    state->ocr_page = GTK_SPIN_BUTTON(
        gtk_spin_button_new_with_range(1, 999, 1));
    state->ocr_start = GTK_BUTTON(
        gtk_button_new_with_label("Analyser comme document d’identité"));
    state->ocr_cancel = GTK_BUTTON(
        gtk_button_new_with_label("Annuler l’OCR"));
    gtk_widget_set_sensitive(GTK_WIDGET(state->ocr_start), FALSE);
    gtk_widget_set_tooltip_text(GTK_WIDGET(state->ocr_start),
        state->tesseract_path != NULL
            ? "Lancer explicitement Tesseract sur la copie contrôlée"
            : "Tesseract est absent du registre d’outils");
    gtk_box_append(GTK_BOX(ocr_box), GTK_WIDGET(state->ocr_document_type));
    gtk_box_append(GTK_BOX(ocr_box), GTK_WIDGET(state->ocr_document_side));
    gtk_box_append(GTK_BOX(ocr_box), GTK_WIDGET(state->ocr_page));
    gtk_box_append(GTK_BOX(ocr_box), GTK_WIDGET(state->ocr_languages));
    gtk_box_append(GTK_BOX(ocr_box), GTK_WIDGET(state->ocr_profile));
    GtkWidget *ocr_actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append(GTK_BOX(ocr_actions), GTK_WIDGET(state->ocr_start));
    gtk_box_append(GTK_BOX(ocr_actions), GTK_WIDGET(state->ocr_cancel));
    gtk_box_append(GTK_BOX(ocr_box), ocr_actions);
    state->ocr_status = GTK_LABEL(gtk_label_new(
        state->tesseract_path != NULL ? "OCR facultatif, non lancé."
            : "Tesseract absent : OCR indisponible."));
    gtk_box_append(GTK_BOX(ocr_box), GTK_WIDGET(state->ocr_status));
    state->ocr_overlay = ocr_provenance_overlay_new();
    state->ocr_fields = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 6));
    GtkWidget *field_scroll = gtk_scrolled_window_new();
    gtk_widget_set_size_request(field_scroll, -1, 180);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(field_scroll),
        GTK_WIDGET(state->ocr_fields));
    gtk_box_append(GTK_BOX(ocr_box), field_scroll);
    {
        static const char *const field_codes[] = {
            "Type de document", "Pays émetteur", "Autorité émettrice",
            "Numéro du document", "Nom", "Nom de naissance", "Prénoms",
            "Sexe imprimé", "Nationalité", "Date de naissance",
            "Lieu de naissance", "Date de délivrance", "Date d’expiration",
            "Adresse imprimée", "Ligne MRZ 1", "Ligne MRZ 2",
            "Ligne MRZ 3", NULL};
        GtkWidget *manual_box =
            gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        state->ocr_manual_code = GTK_DROP_DOWN(
            gtk_drop_down_new_from_strings(field_codes));
        state->ocr_manual_value = GTK_ENTRY(gtk_entry_new());
        gtk_entry_set_placeholder_text(state->ocr_manual_value,
            "Valeur réellement visible dans la preuve");
        gtk_widget_set_hexpand(GTK_WIDGET(state->ocr_manual_value), TRUE);
        GtkWidget *add_manual =
            gtk_button_new_with_label("Ajouter un champ manquant");
        gtk_box_append(GTK_BOX(manual_box),
            GTK_WIDGET(state->ocr_manual_code));
        gtk_box_append(GTK_BOX(manual_box),
            GTK_WIDGET(state->ocr_manual_value));
        gtk_box_append(GTK_BOX(manual_box), add_manual);
        gtk_box_append(GTK_BOX(ocr_box), manual_box);
        g_signal_connect(add_manual, "clicked",
            G_CALLBACK(create_person_dialog_on_add_manual_field), state);
    }
    gtk_box_append(GTK_BOX(ocr_box), gtk_label_new(
        "Notes factuelles sur le document "
        "(tronqué, flou, masqué ou incomplet)"));
    state->ocr_factual_notes = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_widget_set_name(GTK_WIDGET(state->ocr_factual_notes),
        "create-person-ocr-factual-notes");
    gtk_text_view_set_wrap_mode(
        state->ocr_factual_notes, GTK_WRAP_WORD_CHAR);
    gtk_widget_set_size_request(
        GTK_WIDGET(state->ocr_factual_notes), -1, 90);
    gtk_box_append(GTK_BOX(ocr_box),
        GTK_WIDGET(state->ocr_factual_notes));
    state->ocr_text = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_widget_set_name(GTK_WIDGET(state->ocr_text),
        "create-person-ocr-raw-text");
    gtk_text_view_set_editable(state->ocr_text, FALSE);
    gtk_text_view_set_monospace(state->ocr_text, TRUE);
    GtkWidget *ocr_text_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ocr_text_scroll),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(ocr_text_scroll),
        GTK_WIDGET(state->ocr_text));
    state->ocr_corrected_text = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_widget_set_name(GTK_WIDGET(state->ocr_corrected_text),
        "identity-corrected-transcription");
    gtk_text_view_set_wrap_mode(
        state->ocr_corrected_text, GTK_WRAP_WORD_CHAR);
    GtkWidget *corrected_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(corrected_scroll),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(corrected_scroll),
        GTK_WIDGET(state->ocr_corrected_text));
    state->ocr_transcription_stack = GTK_STACK(gtk_stack_new());
    gtk_widget_set_name(GTK_WIDGET(state->ocr_transcription_stack),
        "create-person-ocr-transcription-stack");
    gtk_widget_set_size_request(
        GTK_WIDGET(state->ocr_transcription_stack), -1, 150);
    gtk_stack_add_titled(state->ocr_transcription_stack, ocr_text_scroll,
        "raw", "OCR brut");
    gtk_stack_add_titled(
        state->ocr_transcription_stack, corrected_scroll,
        "corrected", "Transcription corrigée");
    GtkWidget *transcription_switcher = gtk_stack_switcher_new();
    gtk_widget_set_name(transcription_switcher,
        "create-person-ocr-transcription-switcher");
    gtk_stack_switcher_set_stack(
        GTK_STACK_SWITCHER(transcription_switcher),
        state->ocr_transcription_stack);
    gtk_box_append(GTK_BOX(ocr_box), transcription_switcher);
    gtk_box_append(GTK_BOX(ocr_box),
        GTK_WIDGET(state->ocr_transcription_stack));
    GtkWidget *reset_transcription = gtk_button_new_with_label(
        "Réinitialiser depuis l’OCR brut");
    gtk_widget_set_halign(reset_transcription, GTK_ALIGN_START);
    g_signal_connect(reset_transcription, "clicked",
        G_CALLBACK(create_person_dialog_on_reset_transcription), state);
    gtk_box_append(GTK_BOX(ocr_box), reset_transcription);
    GtkWidget *ocr_left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_name(ocr_left, "create-person-ocr-left-panel");
    gtk_widget_set_size_request(ocr_left, 500, -1);
    gtk_box_append(GTK_BOX(ocr_left), ocr_warning);
    GtkWidget *ocr_scroll = gtk_scrolled_window_new();
    gtk_widget_set_name(ocr_scroll, "create-person-ocr-scroll");
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ocr_scroll),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_propagate_natural_height(
        GTK_SCROLLED_WINDOW(ocr_scroll), FALSE);
    gtk_widget_set_vexpand(ocr_scroll, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(ocr_scroll), ocr_box);
    gtk_box_append(GTK_BOX(ocr_left), ocr_scroll);
    GtkWidget *ocr_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_name(ocr_paned, "create-person-ocr-paned");
    gtk_paned_set_start_child(GTK_PANED(ocr_paned), ocr_left);
    GtkWidget *ocr_preview =
        ocr_provenance_overlay_get_widget(state->ocr_overlay);
    gtk_widget_set_name(ocr_preview, "create-person-ocr-preview");
    gtk_widget_set_size_request(ocr_preview, 180, -1);
    gtk_paned_set_end_child(GTK_PANED(ocr_paned), ocr_preview);
    gtk_paned_set_resize_start_child(GTK_PANED(ocr_paned), FALSE);
    gtk_paned_set_resize_end_child(GTK_PANED(ocr_paned), TRUE);
    gtk_stack_add_titled(state->stack, ocr_paned, "identity-ocr",
        "4 — OCR identité");
    state->summary = GTK_LABEL(gtk_label_new(""));
    summary = GTK_WIDGET(state->summary);
    gtk_label_set_wrap(state->summary, TRUE);
    gtk_label_set_wrap_mode(state->summary, PANGO_WRAP_WORD_CHAR);
    gtk_label_set_max_width_chars(state->summary, 100);
    gtk_label_set_xalign(state->summary, 0.0f);
    gtk_label_set_selectable(state->summary, TRUE);
    gtk_widget_set_hexpand(summary, TRUE);
    gtk_widget_set_valign(summary, GTK_ALIGN_START);
    state->summary_scroll = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new());
    gtk_widget_set_name(GTK_WIDGET(state->summary_scroll),
        "create-person-confirmation-scroll");
    gtk_scrolled_window_set_policy(state->summary_scroll,
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_propagate_natural_height(
        state->summary_scroll, FALSE);
    gtk_widget_set_vexpand(GTK_WIDGET(state->summary_scroll), TRUE);
    gtk_scrolled_window_set_child(state->summary_scroll, summary);

    state->factual_relation_editor = person_factual_relation_editor_new();
    state->ocr_projection_editor = person_ocr_projection_editor_new();
    person_factual_relation_editor_set_available_evidence(state->factual_relation_editor,
        state->evidence_labels, state->evidence_identifiers);
    person_factual_relation_editor_set_available_ocr_runs(state->factual_relation_editor,
        state->ocr_runs);
    GtkWidget *projection_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(projection_scroll),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(projection_scroll),
        person_ocr_projection_editor_get_widget(state->ocr_projection_editor));
    gtk_stack_add_titled(state->stack, projection_scroll, "ocr-projection",
        "5 — Projection OCR");
    GtkWidget *relation_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(relation_scroll),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(relation_scroll),
        person_factual_relation_editor_get_widget(state->factual_relation_editor));
    gtk_stack_add_titled(state->stack, relation_scroll, "factual-relations",
        "6 — Relations factuelles");

    gtk_stack_add_titled(state->stack,
        GTK_WIDGET(state->summary_scroll), "summary", "7 — Confirmation");
    state->error = GTK_LABEL(gtk_label_new(NULL)); gtk_widget_set_visible(GTK_WIDGET(state->error), FALSE);
    actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_name(actions, "create-person-actions");
    gtk_widget_set_halign(actions, GTK_ALIGN_END);
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
    gtk_widget_set_vexpand(GTK_WIDGET(state->stack), TRUE);
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
    g_signal_connect(state->ocr_start, "clicked",
        G_CALLBACK(create_person_dialog_on_ocr_start), state);
    g_signal_connect(state->ocr_cancel, "clicked",
        G_CALLBACK(create_person_dialog_on_ocr_cancel), state);
    g_signal_connect(state->search, "changed",
        G_CALLBACK(create_person_dialog_on_search_changed), state);
    g_signal_connect(state->type_filter, "notify::selected",
        G_CALLBACK(create_person_dialog_on_type_changed), state);
    state->evidence_selected_handler = g_signal_connect(
        state->evidence, "notify::selected",
        G_CALLBACK(create_person_dialog_on_evidence_changed), state);
    g_signal_connect(state->add_existing, "clicked",
        G_CALLBACK(create_person_dialog_on_add_existing), state);
    state->retained_selected_handler = g_signal_connect(
        state->retained, "notify::selected",
        G_CALLBACK(create_person_dialog_on_retained_changed), state);
    g_signal_connect(state->retained_type, "notify::selected",
        G_CALLBACK(create_person_dialog_on_retained_type_changed), state);
    g_signal_connect(state->remove_retained, "clicked",
        G_CALLBACK(create_person_dialog_on_remove_retained), state);
    g_signal_connect(state->import_files, "clicked",
        G_CALLBACK(create_person_dialog_on_import_files), state);
    create_person_dialog_update_navigation(state);
    g_object_set_data_full(G_OBJECT(state->window), "person-dialog-state", state,
        create_person_dialog_state_free);
    if (state->tesseract_path != NULL) {
        IdentityLanguageJob *job = g_new0(IdentityLanguageJob, 1);
        job->executable = g_strdup(state->tesseract_path);
        g_weak_ref_init(&job->window, G_OBJECT(state->window));
        GTask *task = g_task_new(NULL, NULL, identity_languages_completed, job);
        g_task_set_task_data(task, job, identity_language_job_free);
        g_task_run_in_thread(task, identity_languages_worker);
        g_object_unref(task);
    }
    labfy_paned_apply_initial_ratio(
        GTK_PANED(ocr_paned), 2.0 / 3.0, 520, 240);
    labfy_dialog_present(state->window);
    return TRUE;
}
void create_person_dialog_result_free(CreatePersonDialogResult *result)
{
    if (result == NULL) return;
    g_free(result->designation); g_free(result->declared_name);
    g_free(result->pseudonym); g_free(result->status); g_free(result->notes);
    g_free(result->evidence_identifier);
    g_clear_pointer(&result->role_assignments, g_ptr_array_unref);
    g_clear_pointer(&result->evidence_selection,
        person_evidence_selection_free);
    g_clear_pointer(&result->staging, evidence_staging_free);
    g_clear_pointer(&result->ocr_runs, g_ptr_array_unref);
    if (result->factual_relations) {
        g_ptr_array_unref(result->factual_relations);
    }
    g_clear_pointer(&result->ocr_projections, g_ptr_array_unref);
    g_free(result);
}
const PersonEntityInput *create_person_dialog_result_get_input(
    const CreatePersonDialogResult *result)
{
    return result != NULL ? &result->input : NULL;
}
const PersonEvidenceSelection *
create_person_dialog_result_get_evidence_selection(
    const CreatePersonDialogResult *result)
{
    return result != NULL ? result->evidence_selection : NULL;
}
EvidenceStaging *create_person_dialog_result_steal_staging(
    CreatePersonDialogResult *result)
{
    EvidenceStaging *staging = result != NULL ? result->staging : NULL;
    if (result != NULL) result->staging = NULL;
    return staging;
}
const GPtrArray *create_person_dialog_result_get_ocr_runs(
    const CreatePersonDialogResult *result)
{
    return result != NULL ? result->ocr_runs : NULL;
}
const GPtrArray *create_person_dialog_result_get_factual_relations(
    const CreatePersonDialogResult *result)
{
    return result != NULL ? result->factual_relations : NULL;
}
const GPtrArray *create_person_dialog_result_get_ocr_projections(const CreatePersonDialogResult *r){return r?r->ocr_projections:NULL;}
gboolean create_person_dialog_test_overlay_has_region(GtkWindow*w){CreatePersonDialogState*s=w?g_object_get_data(G_OBJECT(w),"person-dialog-state"):NULL;return s&&ocr_provenance_overlay_has_region(s->ocr_overlay);}
guint64 create_person_dialog_test_ocr_generation(GtkWindow*w){CreatePersonDialogState*s=w?g_object_get_data(G_OBJECT(w),"person-dialog-state"):NULL;return s?s->ocr_generation:0;}
