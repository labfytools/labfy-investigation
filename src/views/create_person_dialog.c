/******************************************************************************
 * @file create_person_dialog.c
 * @brief Formulaire GTK de création d'une personne observée.
 ******************************************************************************/
#include "views/create_person_dialog.h"
#include "core/evidence_staging.h"
#include "core/evidence_staging_task.h"
#include "core/person_confirmation_summary.h"
#include "core/person_dialog_lifecycle.h"
#include "models/evidence_selection_model.h"
#include "models/person_evidence_selection.h"
#include "widgets/evidence_preview_widget.h"

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
    GtkStack *stack;
    GtkButton *previous;
    GtkButton *next;
    GtkButton *create;
    GtkButton *add_existing;
    GtkButton *remove_retained;
    GtkButton *import_files;
    GtkStringList *evidence_labels;
    GtkStringList *retained_labels;
    GtkStringList *type_filter_labels;
    GtkCheckButton *roles[9];
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
    PersonDialogLifecycle *lifecycle;
    CreatePersonDialogCallback callback;
    CreatePersonDialogSessionCheck session_check;
    gpointer user_data;
    GDestroyNotify user_data_destroy;
    gulong evidence_selected_handler;
    gulong retained_selected_handler;
    gboolean updating_evidence;
    gboolean updating_retained;
} CreatePersonDialogState;

typedef struct {
    GWeakRef window;
    guint64 generation;
} CreatePersonStagingContext;
static const char *const status_codes[] = {"unknown", "suspected", "confirmed"};
static void create_person_dialog_clear_preview(CreatePersonDialogState *state);
static void create_person_dialog_select_record(CreatePersonDialogState *state,
    const EvidenceRecord *record, const char *business_type);
static void create_person_dialog_on_retained_changed(
    GtkDropDown *dropdown, GParamSpec *pspec, gpointer data);

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
    g_clear_pointer(&state->preview, evidence_preview_widget_free);
    if (state->staging_task != NULL) {
        background_task_cancel(state->staging_task);
        background_task_unref(state->staging_task);
    }
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
    person_evidence_selection_set_active(state->person_evidence_selection,
        person_evidence_selection_item_get_identifier(item));
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
/** @brief Termine le dialogue comme une annulation. */
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
    result->evidence_selection = state->person_evidence_selection;
    state->person_evidence_selection = NULL;
    result->staging = state->staging;
    state->staging = NULL;
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
        "1 Personne", "2 Rôles", "3 Preuves", "4 Confirmation"};
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
        char *notes = create_person_dialog_get_notes(state);
        char *text;
        for (guint i = 0; i < G_N_ELEMENTS(state->roles); i++)
            if (gtk_check_button_get_active(state->roles[i]))
                g_ptr_array_add(role_labels, (gpointer)
                    gtk_check_button_get_label(state->roles[i]));
        text = person_confirmation_summary_build_multiple(
            gtk_editable_get_text(GTK_EDITABLE(state->designation)),
            gtk_editable_get_text(GTK_EDITABLE(state->name)),
            gtk_editable_get_text(GTK_EDITABLE(state->pseudonym)),
            gtk_drop_down_get_selected(state->status) <
                G_N_ELEMENTS(status_labels)
                ? status_labels[gtk_drop_down_get_selected(state->status)]
                : "Non renseigné",
            gtk_spin_button_get_value_as_int(state->confidence), notes,
            role_labels, state->person_evidence_selection);
        gtk_label_set_text(state->summary, text);
        g_free(text);
        g_free(notes);
        g_ptr_array_unref(role_labels);
    }
    gtk_widget_set_sensitive(GTK_WIDGET(state->previous), state->step > 0);
    gtk_widget_set_visible(GTK_WIDGET(state->next), state->step < 3);
    gtk_widget_set_visible(GTK_WIDGET(state->create), state->step == 3);
    gtk_widget_set_sensitive(GTK_WIDGET(state->create),
        state->step == 3 &&
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
    if (parent == NULL || investigation_root_path == NULL ||
        task_manager == NULL) return FALSE;
    state = g_new0(CreatePersonDialogState, 1);
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
    state->lifecycle = person_dialog_lifecycle_new();
    state->window = GTK_WINDOW(gtk_window_new());
    gtk_window_set_application(state->window,
        gtk_window_get_application(parent));
    gtk_window_set_title(state->window, "Ajouter une personne");
    gtk_window_set_transient_for(state->window, parent);
    gtk_window_set_modal(state->window, TRUE);
    gtk_window_set_default_size(state->window, 820, 680);
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
    gtk_box_append(GTK_BOX(evidence_box),
        evidence_preview_widget_get_widget(state->preview));
    gtk_stack_add_titled(state->stack, evidence_box, "evidence", "3 — Preuves");
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
    gtk_window_present(state->window); return TRUE;
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
