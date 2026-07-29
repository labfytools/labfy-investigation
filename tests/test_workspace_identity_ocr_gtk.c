#include "views/main_window.h"
#include "views/evidence_identity_ocr_dialog.h"
#include "widgets/workspace.h"
#include "models/evidence_record.h"
#include "models/entity_record.h"
#include "core/task_manager.h"
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <string.h>

#define EVIDENCE_ID "11111111-1111-4111-8111-111111111111"
#define SHA "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"

typedef struct {
    GtkApplication *application;
    MainWindow *main_window;
    TaskManager *manager;
    EvidenceRecord *evidence;
    char *run_a;
    char *run_b;
    char *fixture_path;
    IdentityOcrRun *review_run;
    GtkWindow *review_dialog;
    guint revisions;
    guint reruns;
    gboolean passed;
} TestContext;

static gboolean session_current(gpointer data)
{
    return data != NULL;
}

static void review_saved(
    EvidenceIdentityOcrDialogResult *result, gpointer data);

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

static char *text_view_text(GtkWidget *widget)
{
    GtkTextBuffer *buffer =
        gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget));
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    return gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
}

static IdentityOcrRun *make_run(
    const char *raw, const char *corrected, guint page)
{
    IdentityOcrRun *run = identity_ocr_run_new(
        EVIDENCE_ID, SHA, page == 1 ? "identity_card" : "passport",
        page == 1 ? "front" : "identity_page", page, "fra", "none");
    identity_ocr_run_set_outputs(run, "tesseract SPECIMEN", "fra",
        "SPECIMEN", raw, "TSV SPECIMEN");
    identity_ocr_run_set_corrected_transcription(
        run, corrected, "2026-07-29T10:00:00Z");
    identity_ocr_run_set_factual_notes(run,
        page == 1 ? "NOTE WORKSPACE A" : "NOTE WORKSPACE B");
    IdentitySourceBox box = {
        .page = (gint) page, .x = 10, .y = 20,
        .width = 100, .height = 30,
        .image_width = 640, .image_height = 400, .available = TRUE
    };
    IdentityFieldObservation *field = identity_field_observation_new(
        "surname", page == 1 ? "BRUT A" : "BRUT B", 90, &box, 0);
    identity_field_observation_accept(field);
    identity_ocr_run_add_field(run, field);
    return run;
}

static void identity_requested(
    const char *evidence_identifier, gboolean revise,
    const char *run_identifier, gpointer data)
{
    TestContext *context = data;
    g_assert_cmpstr(evidence_identifier, ==, EVIDENCE_ID);
    if (revise) {
        g_assert_cmpstr(run_identifier, ==, context->run_a);
        context->revisions++;
        GPtrArray *persons = g_ptr_array_new_with_free_func(
            (GDestroyNotify) entity_record_free);
        GError *error = NULL;
        EntityRecord *person = entity_record_new(
            "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa", "person",
            "PERSONNE SPECIMEN", "PERSONNE SPECIMEN", NULL, 0,
            "2026-07-29T10:00:00Z", "2026-07-29T10:00:00Z",
            ENTITY_STATUS_ACTIVE, &error);
        g_assert_no_error(error);
        g_ptr_array_add(persons, person);
        g_assert_true(evidence_identity_ocr_dialog_present_review(
            main_window_get_window(context->main_window),
            context->fixture_path, persons,
            "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",
            context->review_run, session_current,
            review_saved,
            context, NULL, &error));
        g_assert_no_error(error);
        g_ptr_array_unref(persons);
    } else {
        g_assert_null(run_identifier);
        context->reruns++;
    }
}

static GtkWindow *find_review_dialog(void)
{
    GListModel *windows = gtk_window_get_toplevels();
    for (guint index = 0; index < g_list_model_get_n_items(windows);
         index++) {
        GtkWindow *window = g_list_model_get_item(windows, index);
        gboolean matches = g_strcmp0(gtk_window_get_title(window),
            "Réviser l’analyse OCR") == 0;
        if (matches) return window;
        g_object_unref(window);
    }
    return NULL;
}

static gboolean finish_review(gpointer data)
{
    TestContext *context = data;
    evidence_identity_ocr_dialog_finish_import(
        context->review_dialog, NULL);
    g_clear_object(&context->review_dialog);
    return G_SOURCE_REMOVE;
}

static void review_saved(
    EvidenceIdentityOcrDialogResult *result, gpointer data)
{
    TestContext *context = data;
    GtkWindow *dialog =
        evidence_identity_ocr_dialog_result_get_dialog(result);
    IdentityOcrRun *run =
        evidence_identity_ocr_dialog_result_steal_run(result);
    GtkWidget *save = find_named(GTK_WIDGET(dialog),
        "evidence-identity-continue-with-ocr");
    GtkWidget *status = find_named(GTK_WIDGET(dialog),
        "evidence-identity-ocr-status");
    g_assert_false(gtk_widget_get_sensitive(save));
    g_assert_nonnull(g_strstr_len(
        gtk_label_get_text(GTK_LABEL(status)), -1,
        "Enregistrement de la révision"));
    g_assert_cmpstr(identity_ocr_run_get_identifier(run),
        ==, context->run_a);
    context->review_dialog = g_object_ref(dialog);
    identity_ocr_run_free(run);
    evidence_identity_ocr_dialog_result_free(result);
    g_idle_add(finish_review, context);
}

static gboolean drive(gpointer data)
{
    TestContext *context = data;
    GtkWidget *root = GTK_WIDGET(
        main_window_get_window(context->main_window));
    GtkWidget *analyze = find_named(root,
        "workspace-analyze-identity");
    GtkWidget *section = find_named(root,
        "workspace-identity-ocr-section");
    GtkDropDown *selector = GTK_DROP_DOWN(find_named(root,
        "workspace-identity-ocr-selector"));
    g_assert_nonnull(analyze);
    g_assert_true(gtk_widget_get_sensitive(analyze));
    g_assert_true(gtk_widget_get_visible(section));
    g_assert_cmpuint(g_list_model_get_n_items(
        gtk_drop_down_get_model(selector)), ==, 2);
    if (context->revisions == 0) {
        g_assert_cmpuint(gtk_drop_down_get_selected(selector), ==, 1);
        char *raw = text_view_text(find_named(root,
            "workspace-identity-ocr-raw"));
        g_assert_cmpstr(raw, ==, "RUN B OCR BRUT");
        g_free(raw);
        gtk_drop_down_set_selected(selector, 0);
        while (g_main_context_iteration(NULL, FALSE)) {}
        raw = text_view_text(find_named(root,
            "workspace-identity-ocr-raw"));
        g_assert_cmpstr(raw, ==, "RUN A OCR BRUT");
        g_free(raw);
        g_signal_emit_by_name(find_named(root,
            "workspace-revise-identity-ocr"), "clicked");
        GtkWindow *dialog = find_review_dialog();
        g_assert_nonnull(dialog);
        GtkWidget *save = find_named(GTK_WIDGET(dialog),
            "evidence-identity-continue-with-ocr");
        GtkWidget *corrected = find_named(GTK_WIDGET(dialog),
            "identity-corrected-transcription");
        gtk_text_buffer_set_text(gtk_text_view_get_buffer(
            GTK_TEXT_VIEW(corrected)), "RUN A RÉVISION SPECIMEN", -1);
        g_signal_emit_by_name(save, "clicked");
        g_signal_emit_by_name(save, "clicked");
        g_object_unref(dialog);
        g_timeout_add(20, drive, context);
        return G_SOURCE_REMOVE;
    }
    GtkWindow *pending_dialog = find_review_dialog();
    if (pending_dialog != NULL) {
        g_object_unref(pending_dialog);
        g_timeout_add(20, drive, context);
        return G_SOURCE_REMOVE;
    }
    g_signal_emit_by_name(find_named(root,
        "workspace-rerun-identity-ocr"), "clicked");
    g_assert_cmpuint(context->revisions, ==, 1);
    g_assert_cmpuint(context->reruns, ==, 1);
    context->passed = TRUE;
    g_application_quit(G_APPLICATION(context->application));
    return G_SOURCE_REMOVE;
}

static void activate(GtkApplication *application, gpointer data)
{
    TestContext *context = data;
    GError *error = NULL;
    context->main_window = main_window_new(application, context->manager);
    main_window_set_identity_ocr_callback(
        context->main_window, identity_requested, context);
    context->evidence = evidence_record_new(
        EVIDENCE_ID, "IDENTITE-SPECIMEN.jpeg",
        "IDENTITE-SPECIMEN.jpeg", "01_Preuves_Originales/specimen.jpeg",
        "document", 32, SHA, "2026-07-29T10:00:00Z", NULL,
        NULL, NULL, EVIDENCE_INTEGRITY_STATUS_VALID, &error);
    g_assert_no_error(error);
    main_window_set_selected_evidence(
        context->main_window, context->evidence);
    GPtrArray *records = g_ptr_array_new_with_free_func(
        (GDestroyNotify) workspace_identity_ocr_record_free);
    IdentityOcrRun *run_a = make_run(
        "RUN A OCR BRUT", "RUN A CORRIGÉ", 1);
    IdentityOcrRun *run_b = make_run(
        "RUN B OCR BRUT", "RUN B CORRIGÉ", 2);
    context->run_a = g_strdup(identity_ocr_run_get_identifier(run_a));
    context->review_run = identity_ocr_run_copy(run_a);
    context->run_b = g_strdup(identity_ocr_run_get_identifier(run_b));
    g_ptr_array_add(records, workspace_identity_ocr_record_new(
        run_a, "PERSONNE A SPECIMEN", "2026-07-29T10:00:00Z",
        "OCR/A.txt", SHA, "OCR/A.tsv", SHA));
    g_ptr_array_add(records, workspace_identity_ocr_record_new(
        run_b, "PERSONNE B SPECIMEN", "2026-07-29T11:00:00Z",
        "OCR/B.txt", SHA, "OCR/B.tsv", SHA));
    main_window_set_identity_ocr_runs(context->main_window, records);
    main_window_present(context->main_window);
    g_idle_add(drive, context);
}

int main(int argc, char **argv)
{
    if (!gtk_init_check()) {
        g_print("SKIP: aucun affichage GTK disponible.\n");
        return 0;
    }
    TestContext context = {0};
    context.fixture_path = g_strdup_printf(
        "%s/labfy-workspace-identity-%u.jpeg",
        g_get_tmp_dir(), g_random_int());
    g_assert_true(g_file_set_contents(context.fixture_path,
        "SPECIMEN JPEG", -1, NULL));
    context.manager = task_manager_new();
    context.application = gtk_application_new(
        "com.labfytools.test.workspace.identity",
        G_APPLICATION_NON_UNIQUE);
    g_signal_connect(context.application, "activate",
        G_CALLBACK(activate), &context);
    int status = g_application_run(
        G_APPLICATION(context.application), argc, argv);
    g_assert_true(context.passed);
    evidence_record_free(context.evidence);
    main_window_free(context.main_window);
    task_manager_free(context.manager);
    g_object_unref(context.application);
    g_free(context.run_a);
    g_free(context.run_b);
    identity_ocr_run_free(context.review_run);
    g_remove(context.fixture_path);
    g_free(context.fixture_path);
    return status;
}
