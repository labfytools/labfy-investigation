#include "core/evidence_identity_import_task.h"
#include "core/file_hash.h"
#include "core/tool_registry.h"
#include "dao/entity_dao.h"
#include "dao/evidence_dao.h"
#include "dao/evidence_entity_dao.h"
#include "dao/identity_ocr_dao.h"
#include "database/database.h"
#include "database/schema.h"
#include "models/evidence_type.h"
#include "views/evidence_identity_ocr_dialog.h"
#include "views/evidence_import_dialog.h"
#include "views/main_window.h"
#include "widgets/workspace.h"
#include <cairo.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <string.h>

typedef struct {
    GtkApplication *application;
    GtkWindow *main_window;
    MainWindow *production_main_window;
    TaskManager *manager;
    ToolRegistry *registry;
    char *root;
    char *database_path;
    char *file_path;
    GPtrArray *persons;
    EvidenceImportDialogResult *metadata;
    BackgroundTask *import_task;
    GtkWindow *active_ocr_dialog;
    guint phase;
    guint guard;
    gboolean metadata_ready;
    gboolean import_done;
    gboolean cancellation_done;
    gboolean layout_resize_pending;
    gboolean passed;
    gboolean revision_done;
    char *final_evidence_identifier;
    char *run_identifier;
    guint revision_requests;
    guint rerun_requests;
    guint import_start_count;
} TestContext;

static void remove_tree(const char *root)
{
    GDir *directory = g_dir_open(root, 0, NULL);
    const char *name;
    if (directory == NULL) return;
    while ((name = g_dir_read_name(directory)) != NULL) {
        char *path = g_build_filename(root, name, NULL);
        if (g_file_test(path, G_FILE_TEST_IS_DIR)) remove_tree(path);
        else g_unlink(path);
        g_free(path);
    }
    g_dir_close(directory);
    g_rmdir(root);
}

static GtkWidget *find_named(GtkWidget *widget, const char *name)
{
    if (g_strcmp0(gtk_widget_get_name(widget), name) == 0) return widget;
    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL; child = gtk_widget_get_next_sibling(child)) {
        GtkWidget *found = find_named(child, name);
        if (found != NULL) return found;
    }
    return NULL;
}

static gboolean is_descendant_of(GtkWidget *widget, GtkWidget *ancestor)
{
    for (GtkWidget *parent = widget; parent != NULL;
         parent = gtk_widget_get_parent(parent))
        if (parent == ancestor) return TRUE;
    return FALSE;
}

static GtkWidget *find_button(GtkWidget *widget, const char *label)
{
    if (GTK_IS_BUTTON(widget) &&
        g_strcmp0(gtk_button_get_label(GTK_BUTTON(widget)), label) == 0)
        return widget;
    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL; child = gtk_widget_get_next_sibling(child)) {
        GtkWidget *found = find_button(child, label);
        if (found != NULL) return found;
    }
    return NULL;
}

static GtkWidget *find_label(GtkWidget *widget, const char *text)
{
    if (GTK_IS_LABEL(widget) &&
        strstr(gtk_label_get_text(GTK_LABEL(widget)), text) != NULL)
        return widget;
    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL; child = gtk_widget_get_next_sibling(child)) {
        GtkWidget *found = find_label(child, text);
        if (found != NULL) return found;
    }
    return NULL;
}

static GtkWindow *find_window(TestContext *context, const char *title)
{
    for (GList *item = gtk_application_get_windows(context->application);
         item != NULL; item = item->next)
        if (g_strcmp0(gtk_window_get_title(item->data), title) == 0)
            return item->data;
    return NULL;
}

static GtkWidget *find_field_row(GtkWidget *widget, guint ordinal)
{
    if (g_object_get_data(G_OBJECT(widget), "identity-field-code") != NULL &&
        GPOINTER_TO_UINT(g_object_get_data(
            G_OBJECT(widget), "identity-field-order")) == ordinal + 1)
        return widget;
    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL; child = gtk_widget_get_next_sibling(child)) {
        GtkWidget *found = find_field_row(child, ordinal);
        if (found != NULL) return found;
    }
    return NULL;
}

static guint count_field_rows(GtkWidget *widget)
{
    guint count = g_object_get_data(
        G_OBJECT(widget), "identity-field-code") != NULL ? 1 : 0;
    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL; child = gtk_widget_get_next_sibling(child))
        count += count_field_rows(child);
    return count;
}

static GtkWidget *find_entry_placeholder(GtkWidget *widget,
    const char *placeholder)
{
    if (GTK_IS_ENTRY(widget) && g_strcmp0(
            gtk_entry_get_placeholder_text(GTK_ENTRY(widget)),
            placeholder) == 0)
        return widget;
    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL; child = gtk_widget_get_next_sibling(child)) {
        GtkWidget *found = find_entry_placeholder(child, placeholder);
        if (found != NULL) return found;
    }
    return NULL;
}

static char *text_view_contents(GtkTextView *view)
{
    GtkTextIter start;
    GtkTextIter end;
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(view);
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    return gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
}

static void import_completed(BackgroundTask *task, gpointer data)
{
    TestContext *context = data;
    context->import_done = TRUE;
    g_assert_cmpint(background_task_get_state(task), ==,
        BACKGROUND_TASK_STATE_COMPLETED);
    evidence_identity_ocr_dialog_finish_import(
        context->active_ocr_dialog, NULL);
    g_clear_object(&context->active_ocr_dialog);
}

static void ocr_completed(EvidenceIdentityOcrDialogResult *result,
    gpointer data)
{
    TestContext *context = data;
    GError *error = NULL;
    g_assert_nonnull(result);
    g_assert_true(evidence_identity_ocr_dialog_result_has_ocr(result));
    context->import_start_count++;
    g_assert_cmpuint(context->import_start_count, ==, 1);
    context->active_ocr_dialog = g_object_ref(
        evidence_identity_ocr_dialog_result_get_dialog(result));
    IdentityOcrRun *run =
        evidence_identity_ocr_dialog_result_steal_run(result);
    EvidenceIdentityImportTaskRequest *request =
        evidence_identity_import_task_request_new(
            context->database_path, context->root, context->file_path,
            evidence_identity_ocr_dialog_result_get_person_identifier(result),
            evidence_import_dialog_result_get_type_identifier(
                context->metadata),
            evidence_import_dialog_result_get_collected_at(context->metadata),
            evidence_import_dialog_result_get_source(context->metadata),
            evidence_import_dialog_result_get_description(context->metadata),
            run);
    g_assert_nonnull(request);
    context->import_task = evidence_identity_import_task_start(
        context->manager, request, import_completed, context, NULL, &error);
    g_assert_no_error(error);
    g_assert_nonnull(context->import_task);
    evidence_identity_import_task_request_free(request);
    identity_ocr_run_free(run);
    evidence_identity_ocr_dialog_result_free(result);
}

static void cancellation_completed(EvidenceIdentityOcrDialogResult *result,
    gpointer data)
{
    TestContext *context = data;
    g_assert_null(result);
    context->cancellation_done = TRUE;
}

static void metadata_completed(EvidenceImportDialogResult *result,
    gpointer data)
{
    TestContext *context = data;
    GError *error = NULL;
    g_assert_nonnull(result);
    context->metadata = result;
    context->metadata_ready = TRUE;
    g_assert_true(evidence_identity_ocr_dialog_present(
        context->main_window, context->file_path, context->persons,
        NULL, tool_registry_find(context->registry, "tesseract"),
        NULL, ocr_completed, context, NULL, &error));
    g_assert_no_error(error);
}

static void verify_persistence(TestContext *context)
{
    GError *error = NULL;
    Database *database = database_open(context->database_path);
    g_assert_nonnull(database);
    EvidenceDao *evidence_dao = evidence_dao_new(database, &error);
    EvidenceEntityDao *link_dao =
        evidence_entity_dao_new(database, &error);
    IdentityOcrDao *ocr_dao = identity_ocr_dao_new(database);
    g_assert_no_error(error);
    GPtrArray *evidences = evidence_dao_list_all(evidence_dao, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(evidences->len, ==, 1);
    EvidenceRecord *evidence = g_ptr_array_index(evidences, 0);
    const char *evidence_id = evidence_record_get_identifier(evidence);
    gboolean link_exists = FALSE;
    g_assert_nonnull(evidence_id);
    if (context->final_evidence_identifier == NULL)
        context->final_evidence_identifier = g_strdup(evidence_id);
    g_assert_cmpstr(evidence_id, ==, context->final_evidence_identifier);
    g_assert_true(evidence_entity_dao_exists(
        link_dao, evidence_id,
        "33333333-3333-4333-8333-333333333333", &link_exists, &error));
    g_assert_true(link_exists);
    GPtrArray *runs =
        identity_ocr_dao_list_runs_by_evidence(ocr_dao, evidence_id, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(runs->len, ==, 1);
    IdentityOcrRunRecord *run = g_ptr_array_index(runs, 0);
    char *run_identifier = g_strdup(run->id);
    if (context->run_identifier == NULL)
        context->run_identifier = g_strdup(run->id);
    g_assert_cmpstr(run->evidence_id, ==,
        context->final_evidence_identifier);
    g_assert_cmpstr(run->id, ==, context->run_identifier);
    g_assert_nonnull(run->text_relative_path);
    g_assert_nonnull(run->text_sha256);
    g_assert_nonnull(run->tsv_relative_path);
    g_assert_nonnull(run->tsv_sha256);
    g_assert_true(run->transcription_is_human);
    g_assert_cmpstr(run->transcription_origin, ==, "human");
    g_assert_cmpstr(run->corrected_transcription, ==,
        context->revision_done ? "TRANSCRIPTION SPECIMEN RÉVISÉE" :
        "ÉTAT CIVIL\nNée à Besançon — Côte-d’Or");
    GPtrArray *documents = identity_ocr_dao_list_documents_by_person(
        ocr_dao, "33333333-3333-4333-8333-333333333333", &error);
    g_assert_no_error(error);
    g_assert_cmpuint(documents->len, ==, 1);
    IdentityDocumentObservationRecord *document =
        g_ptr_array_index(documents, 0);
    g_assert_nonnull(strstr(document->factual_notes,
        context->revision_done ? "révision" : "tronqué"));
    GPtrArray *fields = identity_ocr_dao_list_fields_by_document(
        ocr_dao, document->id, &error);
    g_assert_no_error(error);
    gboolean accepted = FALSE;
    gboolean modified = FALSE;
    gboolean manual = FALSE;
    for (guint index = 0; index < fields->len; index++) {
        IdentityFieldObservationRecord *field =
            g_ptr_array_index(fields, index);
        accepted |= g_strcmp0(field->review_status, "accepted") == 0;
        modified |= g_strcmp0(field->review_status, "modified") == 0 &&
            g_strcmp0(field->origin, "manual_override") == 0;
        manual |= g_strcmp0(field->origin, "manual_entry") == 0;
    }
    g_assert_true(accepted);
    g_assert_true(modified);
    g_assert_true(manual);
    char *person_identifier = NULL;
    IdentityOcrRun *loaded = identity_ocr_dao_load_run(
        ocr_dao, context->root, run_identifier, &person_identifier, &error);
    g_assert_no_error(error);
    g_assert_nonnull(loaded);
    g_assert_cmpstr(person_identifier, ==,
        "33333333-3333-4333-8333-333333333333");
    char *raw_before = g_strdup(identity_ocr_run_get_raw_text(loaded));
    if (!context->revision_done) {
        g_assert_true(identity_ocr_run_set_corrected_transcription(
            loaded, "TRANSCRIPTION SPECIMEN RÉVISÉE",
            "2026-07-29T12:00:00Z"));
        identity_ocr_run_set_factual_notes(
            loaded, "Document SPECIMEN incomplet — révision.");
        g_assert_true(identity_ocr_dao_update_review(
            ocr_dao, loaded, "2026-07-29T12:00:00Z", &error));
        g_assert_no_error(error);
        context->revision_done = TRUE;
    }
    identity_ocr_run_free(loaded);
    g_free(person_identifier);
    g_ptr_array_unref(fields);
    g_ptr_array_unref(documents);
    g_ptr_array_unref(runs);
    g_ptr_array_unref(evidences);
    identity_ocr_dao_free(ocr_dao);
    evidence_entity_dao_free(link_dao);
    evidence_dao_free(evidence_dao);
    database_close(database);
    database = database_open(context->database_path);
    g_assert_nonnull(database);
    ocr_dao = identity_ocr_dao_new(database);
    loaded = identity_ocr_dao_load_run(
        ocr_dao, context->root, run_identifier, NULL, &error);
    g_assert_no_error(error);
    g_assert_nonnull(loaded);
    g_assert_cmpstr(identity_ocr_run_get_raw_text(loaded), ==, raw_before);
    g_assert_cmpstr(identity_ocr_run_get_corrected_transcription(loaded),
        ==, "TRANSCRIPTION SPECIMEN RÉVISÉE");
    g_assert_nonnull(strstr(identity_ocr_run_get_factual_notes(loaded),
        "révision"));
    identity_ocr_run_free(loaded);
    identity_ocr_dao_free(ocr_dao);
    database_close(database);
    g_free(raw_before);
    g_free(run_identifier);
}

static void workspace_identity_requested(const char *evidence_identifier,
    gboolean revise_existing, const char *ocr_run_identifier, gpointer data)
{
    TestContext *context = data;
    g_assert_cmpstr(evidence_identifier, ==,
        context->final_evidence_identifier);
    if (revise_existing) {
        g_assert_cmpstr(ocr_run_identifier, ==, context->run_identifier);
        context->revision_requests++;
    } else {
        g_assert_null(ocr_run_identifier);
        context->rerun_requests++;
    }
}

static void load_workspace_from_reopened_database(
    TestContext *context, gboolean trigger_actions)
{
    GError *error = NULL;
    Database *database = database_open(context->database_path);
    g_assert_nonnull(database);
    EvidenceDao *evidence_dao = evidence_dao_new(database, &error);
    g_assert_no_error(error);
    GPtrArray *evidences = evidence_dao_list_all(evidence_dao, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(evidences->len, ==, 1);
    EvidenceRecord *evidence = g_ptr_array_index(evidences, 0);
    g_assert_cmpstr(evidence_record_get_identifier(evidence), ==,
        context->final_evidence_identifier);

    main_window_set_selected_evidence(
        context->production_main_window, evidence);
    IdentityOcrDao *ocr_dao = identity_ocr_dao_new(database);
    GPtrArray *runs = identity_ocr_dao_list_runs_by_evidence(
        ocr_dao, context->final_evidence_identifier, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(runs->len, ==, 1);
    GPtrArray *workspace_records = g_ptr_array_new_with_free_func(
        (GDestroyNotify) workspace_identity_ocr_record_free);
    for (guint index = 0; index < runs->len; index++) {
        IdentityOcrRunRecord *record = g_ptr_array_index(runs, index);
        char *person_identifier = NULL;
        IdentityOcrRun *loaded = identity_ocr_dao_load_run(
            ocr_dao, context->root, record->id,
            &person_identifier, &error);
        g_assert_no_error(error);
        g_assert_nonnull(loaded);
        g_assert_cmpstr(identity_ocr_run_get_evidence_id(loaded),
            ==, context->final_evidence_identifier);
        WorkspaceIdentityOcrRecord *workspace_record =
            workspace_identity_ocr_record_new(
                loaded, person_identifier, record->executed_at,
                record->text_relative_path, record->text_sha256,
                record->tsv_relative_path, record->tsv_sha256);
        g_assert_nonnull(workspace_record);
        g_ptr_array_add(workspace_records, workspace_record);
        g_free(person_identifier);
    }
    main_window_set_identity_ocr_runs(
        context->production_main_window, workspace_records);
    g_ptr_array_unref(runs);
    identity_ocr_dao_free(ocr_dao);
    g_ptr_array_unref(evidences);
    evidence_dao_free(evidence_dao);
    database_close(database);

    GtkWidget *root = GTK_WIDGET(context->main_window);
    GtkWidget *section = find_named(
        root, "workspace-identity-ocr-section");
    GtkWidget *analyze = find_named(root, "workspace-analyze-identity");
    GtkDropDown *selector = GTK_DROP_DOWN(find_named(
        root, "workspace-identity-ocr-selector"));
    g_assert_nonnull(section);
    g_assert_true(gtk_widget_get_visible(section));
    g_assert_nonnull(analyze);
    g_assert_true(gtk_widget_get_sensitive(analyze));
    g_assert_nonnull(selector);
    g_assert_cmpuint(g_list_model_get_n_items(
        gtk_drop_down_get_model(selector)), ==, 1);
    char *raw = text_view_contents(GTK_TEXT_VIEW(find_named(
        root, "workspace-identity-ocr-raw")));
    char *corrected = text_view_contents(GTK_TEXT_VIEW(find_named(
        root, "workspace-identity-ocr-corrected")));
    g_assert_nonnull(strstr(raw, "NOM : SPECIMEN"));
    g_assert_cmpstr(corrected, ==, "TRANSCRIPTION SPECIMEN RÉVISÉE");
    g_free(raw);
    g_free(corrected);
    if (trigger_actions) {
        g_signal_emit_by_name(find_named(
            root, "workspace-revise-identity-ocr"), "clicked");
        g_signal_emit_by_name(find_named(
            root, "workspace-rerun-identity-ocr"), "clicked");
        g_assert_cmpuint(context->revision_requests, ==, 1);
        g_assert_cmpuint(context->rerun_requests, ==, 1);
    }
}

static gboolean drive(gpointer data)
{
    TestContext *context = data;
    if (++context->guard > 1500)
        g_error("Timeout du test GTK d’import OCR complet (phase %u)",
            context->phase);
    if (context->phase == 0) {
        GtkWindow *dialog = find_window(context, "Importer une preuve");
        if (dialog == NULL) return G_SOURCE_CONTINUE;
        GtkWidget *paned =
            find_named(GTK_WIDGET(dialog), "evidence-import-paned");
        g_assert_true(GTK_IS_PANED(paned));
        g_assert_nonnull(gtk_paned_get_start_child(GTK_PANED(paned)));
        g_assert_nonnull(gtk_paned_get_end_child(GTK_PANED(paned)));
        g_signal_emit_by_name(
            find_button(GTK_WIDGET(dialog), "Importer"), "clicked");
        context->phase++;
    } else if (context->phase == 1) {
        if (!context->metadata_ready) return G_SOURCE_CONTINUE;
        GtkWindow *dialog = find_window(
            context, "Import — OCR d’identité facultatif");
        if (dialog == NULL) return G_SOURCE_CONTINUE;
        GtkWidget *root = GTK_WIDGET(dialog);
        int default_width = 0;
        int default_height = 0;
        if (!context->layout_resize_pending)
            gtk_window_set_default_size(dialog, 1200, 800);
        gtk_window_get_default_size(
            dialog, &default_width, &default_height);
        g_assert_cmpint(default_width, ==,
            context->layout_resize_pending ? 1000 : 1200);
        g_assert_cmpint(default_height, ==,
            context->layout_resize_pending ? 700 : 800);
        GtkWidget *paned = find_named(
            root, "evidence-identity-ocr-paned");
        GtkWidget *left = find_named(root, "identity-left-panel");
        GtkWidget *form_scroll =
            find_named(root, "identity-form-scroll");
        GtkWidget *actions =
            find_named(root, "identity-final-actions");
        g_assert_true(GTK_IS_PANED(paned));
        g_assert_nonnull(left);
        g_assert_true(GTK_IS_SCROLLED_WINDOW(form_scroll));
        g_assert_nonnull(actions);
        g_assert_true(gtk_paned_get_start_child(GTK_PANED(paned)) == left);
        g_assert_nonnull(gtk_paned_get_end_child(GTK_PANED(paned)));
        g_assert_false(is_descendant_of(actions, form_scroll));
        if (g_object_get_data(
                G_OBJECT(paned), "identity-initial-position-set") == NULL)
            return G_SOURCE_CONTINUE;
        if (!context->layout_resize_pending) {
            int initial_position =
                gtk_paned_get_position(GTK_PANED(paned));
            g_assert_cmpint(initial_position, >=, 780);
            g_assert_cmpint(initial_position, <=, 820);
            gtk_paned_set_position(GTK_PANED(paned), 650);
            g_assert_cmpint(
                gtk_paned_get_position(GTK_PANED(paned)), ==, 650);
            gtk_window_set_default_size(dialog, 1000, 700);
            context->layout_resize_pending = TRUE;
            return G_SOURCE_CONTINUE;
        }
        g_assert_cmpint(
            gtk_paned_get_position(GTK_PANED(paned)), ==, 650);
        GtkDropDown *person = GTK_DROP_DOWN(find_named(root,
            "identity-person"));
        g_assert_nonnull(person);
        g_assert_cmpuint(gtk_drop_down_get_selected(person), ==, 0);
        gtk_drop_down_set_selected(person, 1);
        gtk_drop_down_set_selected(GTK_DROP_DOWN(find_named(
            root, "identity-document-type")), 0);
        gtk_drop_down_set_selected(GTK_DROP_DOWN(find_named(
            root, "identity-document-side")), 0);
        GtkWidget *start = find_button(
            root, "Analyser comme document d’identité");
        g_assert_nonnull(find_named(root, "identity-notes"));
        g_assert_nonnull(find_entry_placeholder(
            root, "Valeur visible mais omise par l’OCR"));
        g_assert_true(gtk_widget_get_mapped(
            find_button(root, "Annuler")));
        g_assert_true(gtk_widget_get_mapped(
            find_button(root, "Importer sans OCR")));
        gtk_window_set_default_size(dialog, 760, 560);
        gtk_window_get_default_size(
            dialog, &default_width, &default_height);
        g_assert_cmpint(default_width, ==, 760);
        g_assert_cmpint(default_height, ==, 560);
        g_assert_true(gtk_widget_get_sensitive(start));
        g_assert_cmpuint(count_field_rows(root), ==, 0);
        g_signal_emit_by_name(start, "clicked");
        g_signal_emit_by_name(start, "clicked");
        context->phase++;
    } else if (context->phase == 2) {
        GtkWindow *dialog = find_window(
            context, "Import — OCR d’identité facultatif");
        if (dialog == NULL) return G_SOURCE_CONTINUE;
        GtkWidget *root = GTK_WIDGET(dialog);
        int default_width = 0;
        int default_height = 0;
        gtk_window_get_default_size(
            dialog, &default_width, &default_height);
        g_assert_cmpint(default_width, ==, 760);
        g_assert_cmpint(default_height, ==, 560);
        GtkWidget *continue_button =
            find_button(root, "Continuer l’import avec OCR");
        if (!gtk_widget_get_sensitive(continue_button))
            return G_SOURCE_CONTINUE;
        GtkTextView *raw = GTK_TEXT_VIEW(find_named(
            root, "identity-raw-text"));
        g_assert_false(gtk_text_view_get_editable(raw));
        char *raw_text = text_view_contents(raw);
        g_assert_nonnull(strstr(raw_text, "NOM : SPECIMEN"));
        GtkTextView *corrected = GTK_TEXT_VIEW(find_named(
            root, "identity-corrected-transcription"));
        g_assert_true(gtk_text_view_get_editable(corrected));
        char *initial_corrected = text_view_contents(corrected);
        g_assert_cmpstr(initial_corrected, ==, raw_text);
        g_free(initial_corrected);
        gtk_text_buffer_set_text(gtk_text_view_get_buffer(corrected),
            "ÉTAT CIVIL\nNée à Besançon — Côte-d'Or", -1);
        g_signal_emit_by_name(find_button(
            root, "Enregistrer la correction"), "clicked");
        g_assert_false(gtk_text_view_get_editable(corrected));
        g_signal_emit_by_name(find_named(
            root, "identity-corrected-transcription-reedit"), "clicked");
        g_assert_true(gtk_text_view_get_editable(corrected));
        gtk_text_buffer_set_text(gtk_text_view_get_buffer(corrected),
            "ÉTAT CIVIL\nNée à Besançon — Côte-d’Or", -1);
        g_signal_emit_by_name(find_button(
            root, "Enregistrer la correction"), "clicked");
        g_free(raw_text);
        g_assert_cmpuint(count_field_rows(root), >=, 3);
        GtkWidget *form_scroll =
            find_named(root, "identity-form-scroll");
        GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment(
            GTK_SCROLLED_WINDOW(form_scroll));
        double bottom = gtk_adjustment_get_upper(adjustment) -
            gtk_adjustment_get_page_size(adjustment);
        g_assert_cmpfloat(bottom, >, 0.0);
        gtk_adjustment_set_value(adjustment, bottom);
        g_assert_true(gtk_widget_get_mapped(
            find_button(root, "Annuler")));
        g_assert_true(gtk_widget_get_mapped(
            find_button(root, "Importer sans OCR")));
        g_assert_true(gtk_widget_get_mapped(continue_button));
        GtkWidget *left = find_named(root, "identity-left-panel");
        GtkWidget *actions =
            find_named(root, "identity-final-actions");
        graphene_rect_t action_bounds;
        g_assert_true(gtk_widget_compute_bounds(
            actions, left, &action_bounds));
        g_assert_cmpfloat(
            action_bounds.origin.y + action_bounds.size.height, <=,
            (float) gtk_widget_get_height(left));
        GtkWidget *modified = find_field_row(root, 1);
        g_assert_nonnull(modified);
        GtkWidget *entry = g_object_get_data(
            G_OBJECT(modified), "identity-value-entry");
        GtkWidget *modify = g_object_get_data(
            G_OBJECT(modified), "identity-modify-button");
        gtk_editable_set_text(GTK_EDITABLE(entry), "SPECIMEN CORRIGÉ");
        g_signal_emit_by_name(modify, "clicked");
        modified = find_field_row(root, 1);
        g_signal_emit_by_name(g_object_get_data(
            G_OBJECT(modified), "identity-reedit-button"), "clicked");
        modified = find_field_row(root, 1);
        entry = g_object_get_data(
            G_OBJECT(modified), "identity-value-entry");
        modify = g_object_get_data(
            G_OBJECT(modified), "identity-modify-button");
        g_assert_true(gtk_editable_get_editable(GTK_EDITABLE(entry)));
        gtk_editable_set_text(GTK_EDITABLE(entry), "SPECIMEN FINAL");
        g_signal_emit_by_name(modify, "clicked");
        GtkWidget *accepted = find_field_row(root, 0);
        g_assert_nonnull(accepted);
        g_signal_emit_by_name(
            find_button(accepted, "Accepter"), "clicked");
        GtkWidget *manual = find_entry_placeholder(
            root, "Valeur visible mais omise par l’OCR");
        gtk_editable_set_text(GTK_EDITABLE(manual), "VISIBLE SPECIMEN");
        g_signal_emit_by_name(
            find_button(root, "Ajouter un champ manquant"), "clicked");
        GtkWidget *manual_row =
            find_field_row(root, count_field_rows(root) - 1);
        g_assert_nonnull(manual_row);
        g_signal_emit_by_name(g_object_get_data(
            G_OBJECT(manual_row), "identity-modify-button"), "clicked");
        GtkTextView *notes =
            GTK_TEXT_VIEW(find_named(root, "identity-notes"));
        gtk_text_buffer_set_text(gtk_text_view_get_buffer(notes),
            "Document SPECIMEN tronqué sur le bord droit.", -1);
        gtk_adjustment_set_value(adjustment, 0.0);
        GtkDropDown *person = GTK_DROP_DOWN(
            find_named(root, "identity-person"));
        g_assert_cmpuint(gtk_drop_down_get_selected(person), ==, 1);
        g_signal_emit_by_name(
            find_button(root, "Voir la zone"), "clicked");
        gtk_drop_down_set_selected(person, 0);
        g_signal_emit_by_name(continue_button, "clicked");
        g_assert_nonnull(find_label(
            root, "Sélectionnez une personne avant de poursuivre."));
        g_assert_true(gtk_widget_get_sensitive(continue_button));
        g_assert_cmpuint(context->import_start_count, ==, 0);
        gtk_drop_down_set_selected(person, 1);
        g_signal_emit_by_name(continue_button, "clicked");
        g_assert_false(gtk_widget_get_sensitive(continue_button));
        g_assert_nonnull(find_label(root, "Import OCR en cours"));
        g_signal_emit_by_name(continue_button, "clicked");
        g_assert_cmpuint(context->import_start_count, ==, 1);
        context->phase++;
    } else if (context->phase == 3) {
        if (!context->import_done) return G_SOURCE_CONTINUE;
        g_assert_null(find_window(
            context, "Import — OCR d’identité facultatif"));
        verify_persistence(context);
        load_workspace_from_reopened_database(context, TRUE);
        GError *error = NULL;
        g_assert_true(evidence_identity_ocr_dialog_present(
            context->main_window, context->file_path, context->persons,
            "33333333-3333-4333-8333-333333333333",
            tool_registry_find(context->registry, "tesseract"), NULL,
            cancellation_completed, context, NULL, &error));
        g_assert_no_error(error);
        context->phase++;
    } else if (context->phase == 4) {
        GtkWindow *dialog = find_window(
            context, "Import — OCR d’identité facultatif");
        if (dialog == NULL) return G_SOURCE_CONTINUE;
        GtkWidget *start = find_button(
            GTK_WIDGET(dialog), "Analyser comme document d’identité");
        if (!gtk_widget_get_sensitive(start)) return G_SOURCE_CONTINUE;
        g_signal_emit_by_name(start, "clicked");
        g_signal_emit_by_name(
            find_button(GTK_WIDGET(dialog), "Annuler"), "clicked");
        context->phase++;
    } else if (context->phase == 5) {
        if (!context->cancellation_done) return G_SOURCE_CONTINUE;
        verify_persistence(context);
        load_workspace_from_reopened_database(context, FALSE);
        g_assert_true(gtk_widget_get_visible(
            GTK_WIDGET(context->main_window)));
        context->passed = TRUE;
        g_application_quit(G_APPLICATION(context->application));
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

static void write_png(const char *path)
{
    cairo_surface_t *surface = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, 480, 280);
    cairo_t *cr = cairo_create(surface);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_move_to(cr, 24, 60);
    cairo_show_text(cr, "SPECIMEN IDENTITE");
    g_assert_cmpint(cairo_surface_write_to_png(surface, path), ==,
        CAIRO_STATUS_SUCCESS);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

static void activate(GtkApplication *application, gpointer data)
{
    TestContext *context = data;
    GError *error = NULL;
    char *database_directory =
        g_build_filename(context->root, "00_BaseDeDonnees", NULL);
    g_assert_cmpint(g_mkdir_with_parents(database_directory, 0700), ==, 0);
    context->database_path =
        g_build_filename(database_directory, "Enquete.sqlite", NULL);
    g_assert_true(database_initialize(context->database_path,
        "Enquête SPECIMEN import OCR", context->root));
    write_png(context->file_path);
    Database *database = database_open(context->database_path);
    EntityDao *entity_dao = entity_dao_new(database, &error);
    EntityRecord *person = entity_record_new(
        "33333333-3333-4333-8333-333333333333", "person",
        "PERSONNE SPECIMEN", "PERSONNE SPECIMEN", NULL, 0,
        "2026-07-29T10:00:00Z", "2026-07-29T10:00:00Z",
        ENTITY_STATUS_ACTIVE, &error);
    g_assert_true(entity_dao_insert(entity_dao, person, &error));
    g_assert_no_error(error);
    g_ptr_array_add(context->persons, person);
    entity_dao_free(entity_dao);
    database_close(database);
    context->registry = tool_registry_new();
    char *tool = g_canonicalize_filename("tests/fake_document_tool", NULL);
    g_assert_true(tool_registry_register(context->registry, "tesseract",
        "Tesseract SPECIMEN", tool, TOOL_REQUIREMENT_OPTIONAL, &error));
    g_assert_true(tool_registry_refresh(context->registry, &error));
    g_assert_true(tool_registry_set_version(context->registry, "tesseract",
        "5.0.0 SPECIMEN", &error));
    g_assert_no_error(error);
    context->production_main_window =
        main_window_new(application, context->manager);
    g_assert_nonnull(context->production_main_window);
    main_window_set_identity_ocr_callback(
        context->production_main_window,
        workspace_identity_requested, context);
    context->main_window =
        main_window_get_window(context->production_main_window);
    main_window_present(context->production_main_window);
    GPtrArray *types = g_ptr_array_new_with_free_func(
        (GDestroyNotify) evidence_type_free);
    g_ptr_array_add(types, evidence_type_new(
        1, "document", "Document SPECIMEN", NULL, &error));
    g_assert_no_error(error);
    g_assert_true(evidence_import_dialog_present(
        context->main_window, context->file_path, types,
        metadata_completed, context, &error));
    g_assert_no_error(error);
    g_ptr_array_unref(types);
    g_free(tool);
    g_free(database_directory);
    g_timeout_add(10, drive, context);
}

int main(int argc, char **argv)
{
    if (!gtk_init_check()) {
        g_print("SKIP: aucun affichage GTK disponible.\n");
        return 0;
    }
    g_setenv("LABFY_FAKE_IDENTITY_OCR", "1", TRUE);
    TestContext context = {0};
    context.root =
        g_dir_make_tmp("labfy-identity-import-gtk-XXXXXX", NULL);
    context.file_path =
        g_build_filename(context.root, "SPECIMEN-identite.png", NULL);
    context.persons = g_ptr_array_new_with_free_func(
        (GDestroyNotify) entity_record_free);
    context.manager = task_manager_new();
    context.application = gtk_application_new(
        "org.labfy.Investigation.IdentityImportTest",
        G_APPLICATION_NON_UNIQUE);
    g_signal_connect(context.application, "activate",
        G_CALLBACK(activate), &context);
    int status = g_application_run(
        G_APPLICATION(context.application), argc, argv);
    g_assert_true(context.passed);
    background_task_unref(context.import_task);
    evidence_import_dialog_result_free(context.metadata);
    tool_registry_free(context.registry);
    main_window_free(context.production_main_window);
    task_manager_free(context.manager);
    g_ptr_array_unref(context.persons);
    g_object_unref(context.application);
    g_free(context.database_path);
    g_free(context.file_path);
    g_free(context.final_evidence_identifier);
    g_free(context.run_identifier);
    remove_tree(context.root);
    g_free(context.root);
    return status;
}
