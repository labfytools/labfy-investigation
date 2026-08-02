#include "views/evidence_metadata_dialog.h"
#include "dao/entity_dao.h"
#include "dao/evidence_dao.h"
#include "dao/identity_ocr_dao.h"
#include "database/database.h"
#include "models/entity_record.h"
#include "models/evidence_type.h"
#include "widgets/ocr_provenance_overlay.h"
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#define EVIDENCE_ID "11111111-1111-4111-8111-111111111111"
#define PERSON_A "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa"
#define PERSON_B "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb"
#define SHA_EVIDENCE "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
#define SHA_A "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
#define SHA_B "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
#define SHA_C "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"

typedef struct {
    GtkApplication *application;
    GtkWindow *main_window;
    EvidenceRecord *record;
    GPtrArray *types;
    char *root;
    char *database_path;
    Database *database;
    char *run_a;
    char *run_b;
    char *run_c;
    guint phase;
    guint close_cycles;
    guint callbacks;
    gboolean revision_requested;
    gboolean relaunch_requested;
    gboolean passed;
} TestContext;

static gboolean drive(gpointer data);

static void remove_tree(const char *path)
{
    GDir *directory = g_dir_open(path, 0, NULL);
    if (directory != NULL) {
        const char *name = NULL;
        while ((name = g_dir_read_name(directory)) != NULL) {
            char *child = g_build_filename(path, name, NULL);
            if (g_file_test(child, G_FILE_TEST_IS_DIR)) remove_tree(child);
            else g_remove(child);
            g_free(child);
        }
        g_dir_close(directory);
    }
    g_rmdir(path);
}

static GtkWindow *find_metadata_window(TestContext *context)
{
    for (GList *item = gtk_application_get_windows(context->application);
         item != NULL; item = item->next) {
        GtkWindow *window = item->data;
        if (g_strcmp0(gtk_window_get_title(window),
                "Modifier les métadonnées") == 0) return window;
    }
    return NULL;
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

static char *text_view_text(GtkWidget *widget)
{
    GtkTextBuffer *buffer =
        gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget));
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    return gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
}

static char *widget_text(GtkWidget *widget)
{
    GString *text = g_string_new(NULL);
    if (GTK_IS_LABEL(widget))
        g_string_append(text, gtk_label_get_text(GTK_LABEL(widget)));
    else if (GTK_IS_TEXT_VIEW(widget)) {
        char *value = text_view_text(widget);
        g_string_append(text, value);
        g_free(value);
    }
    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL; child = gtk_widget_get_next_sibling(child)) {
        char *value = widget_text(child);
        g_string_append_c(text, '\n');
        g_string_append(text, value);
        g_free(value);
    }
    return g_string_free(text, FALSE);
}

static IdentityOcrRun *new_run(
    const char *type, const char *side, guint page,
    const char *raw, const char *corrected, const char *note,
    gboolean second)
{
    IdentityOcrRun *run = identity_ocr_run_new(
        EVIDENCE_ID, SHA_EVIDENCE, type, side, page,
        second ? "eng" : "fra", "none");
    identity_ocr_run_set_outputs(run,
        second ? "tesseract 5 B" : "tesseract 5 A",
        second ? "eng" : "fra", "SPECIMEN", raw,
        second ? "TSV RUN B" : "TSV RUN A");
    g_assert_true(identity_ocr_run_set_corrected_transcription(
        run, corrected, "2026-07-29T10:10:00Z"));
    identity_ocr_run_set_factual_notes(run, note);
    IdentitySourceBox box = {
        .page = (gint) page,
        .x = second ? 51 : 11, .y = second ? 52 : 12,
        .width = 80, .height = 20,
        .image_width = 640, .image_height = 400, .available = TRUE
    };
    IdentityFieldObservation *field = identity_field_observation_new(
        second ? "surname" : "document_number",
        second ? "RUN B RAW FIELD" : "RUN A RAW FIELD",
        second ? 82.0 : 91.0, &box, 0);
    if (second)
        g_assert_true(identity_field_observation_modify(
            field, "RUN B MODIFIÉ", "SPECIMEN"));
    else
        g_assert_true(identity_field_observation_accept(field));
    identity_ocr_run_add_field(run, field);
    IdentityFieldObservation *human = second
        ? identity_field_observation_new_manual(
            "given_names", "RUN B MANUEL", 1)
        : identity_field_observation_new(
            "surname", "RUN A OCR NOM", 70.0, &box, 1);
    if (!second)
        g_assert_true(identity_field_observation_modify(
            human, "RUN A SURCHARGE", "SPECIMEN"));
    identity_ocr_run_add_field(run, human);
    return run;
}

static void insert_run(TestContext *context, gboolean second)
{
    GError *error = NULL;
    IdentityOcrRun *run = new_run(
        second ? "passport" : "identity_card",
        second ? "identity_page" : "front", second ? 2 : 1,
        second ? "RUN B OCR BRUT" : "RUN A OCR BRUT",
        second ? "RUN B CORRIGÉ" : "RUN A CORRIGÉ",
        second ? "NOTE RUN B" : "NOTE RUN A", second);
    const char *directory = second
        ? "02_Preuves_Traitees/OCR/run-b"
        : "02_Preuves_Traitees/OCR/run-a";
    char *absolute_directory =
        g_build_filename(context->root, directory, NULL);
    g_assert_cmpint(g_mkdir_with_parents(
        absolute_directory, 0700), ==, 0);
    char *text_relative = g_build_filename(directory, "ocr.txt", NULL);
    char *tsv_relative = g_build_filename(directory, "ocr.tsv", NULL);
    char *text_path = g_build_filename(context->root,
        text_relative, NULL);
    char *tsv_path = g_build_filename(context->root,
        tsv_relative, NULL);
    g_assert_true(g_file_set_contents(text_path,
        second ? "RUN B OCR BRUT" : "RUN A OCR BRUT", -1, &error));
    g_assert_no_error(error);
    g_assert_true(g_file_set_contents(tsv_path,
        second ? "TSV RUN B" : "TSV RUN A", -1, &error));
    g_assert_no_error(error);
    IdentityOcrDao *dao = identity_ocr_dao_new(context->database);
    g_assert_true(identity_ocr_dao_insert(dao,
        second ? PERSON_B : PERSON_A, EVIDENCE_ID, run,
        text_relative, second ? SHA_B : SHA_A,
        tsv_relative, second ? SHA_B : SHA_A,
        second ? "2026-07-29T11:00:00Z"
            : "2026-07-29T10:00:00Z", &error));
    g_assert_no_error(error);
    if (second) context->run_b =
        g_strdup(identity_ocr_run_get_identifier(run));
    else context->run_a =
        g_strdup(identity_ocr_run_get_identifier(run));
    identity_ocr_dao_free(dao);
    identity_ocr_run_free(run);
    g_free(absolute_directory);
    g_free(text_relative);
    g_free(tsv_relative);
    g_free(text_path);
    g_free(tsv_path);
}

static void insert_run_c(TestContext *context)
{
    GError *error = NULL;
    IdentityOcrRun *run = new_run(
        "passport", "identity_page", 3, "RUN C OCR BRUT",
        "RUN C CORRIGÉ", "NOTE RUN C", TRUE);
    char *directory = g_build_filename(context->root,
        "02_Preuves_Traitees/OCR/run-c", NULL);
    g_mkdir_with_parents(directory, 0700);
    char *text_path = g_build_filename(directory, "ocr.txt", NULL);
    char *tsv_path = g_build_filename(directory, "ocr.tsv", NULL);
    g_file_set_contents(text_path, "RUN C OCR BRUT", -1, NULL);
    g_file_set_contents(tsv_path, "TSV RUN C", -1, NULL);
    IdentityOcrDao *dao = identity_ocr_dao_new(context->database);
    g_assert_true(identity_ocr_dao_insert(dao, PERSON_B, EVIDENCE_ID,
        run, "02_Preuves_Traitees/OCR/run-c/ocr.txt", SHA_C,
        "02_Preuves_Traitees/OCR/run-c/ocr.tsv", SHA_C,
        "2026-07-29T12:00:00Z", &error));
    g_assert_no_error(error);
    context->run_c = g_strdup(identity_ocr_run_get_identifier(run));
    identity_ocr_dao_free(dao);
    identity_ocr_run_free(run);
    g_free(directory);
    g_free(text_path);
    g_free(tsv_path);
}

static void completed(EvidenceMetadataDialogResult *result, gpointer data)
{
    TestContext *context = data;
    g_assert_null(result);
    context->callbacks++;
    g_test_message("fiche fermée phase=%u callbacks=%u",
        context->phase, context->callbacks);
    g_idle_add(drive, context);
}

static void analyze_requested(
    GtkWindow *dialog, const char *evidence_identifier,
    gboolean revise_existing, const char *ocr_run_identifier,
    gpointer data)
{
    TestContext *context = data;
    g_assert_true(GTK_IS_WINDOW(dialog));
    g_assert_cmpstr(evidence_identifier, ==, EVIDENCE_ID);
    if (revise_existing) {
        g_assert_cmpstr(ocr_run_identifier, ==, context->run_a);
        context->revision_requested = TRUE;
        IdentityOcrDao *dao = identity_ocr_dao_new(context->database);
        GError *error = NULL;
        char *person = NULL;
        IdentityOcrRun *run = identity_ocr_dao_load_run(
            dao, context->root, context->run_a, &person, &error);
        g_assert_no_error(error);
        g_assert_cmpstr(person, ==, PERSON_A);
        g_assert_true(identity_ocr_run_set_corrected_transcription(
            run, "RUN A RÉVISÉ", "2026-07-29T11:30:00Z"));
        g_assert_true(identity_ocr_dao_update_review(
            dao, run, "2026-07-29T11:30:00Z", &error));
        g_assert_no_error(error);
        identity_ocr_run_free(run);
        identity_ocr_dao_free(dao);
        g_free(person);
    } else {
        g_assert_null(ocr_run_identifier);
        context->relaunch_requested = TRUE;
        insert_run_c(context);
    }
}

static void assert_run_content(
    GtkWindow *dialog, guint selected, const char *present,
    const char *absent, const char *run_identifier,
    const char *person, const char *note, const char *origin,
    const char *artifact_sha)
{
    GtkDropDown *selector = GTK_DROP_DOWN(find_named(
        GTK_WIDGET(dialog), "evidence-ocr-run-selector"));
    g_assert_nonnull(selector);
    g_assert_cmpuint(g_list_model_get_n_items(
        gtk_drop_down_get_model(selector)), >, selected);
    gtk_drop_down_set_selected(selector, selected);
    while (g_main_context_iteration(NULL, FALSE)) {}
    GtkWidget *section = find_named(
        GTK_WIDGET(dialog), "evidence-ocr-readonly");
    char *all = widget_text(section);
    g_assert_nonnull(g_strstr_len(all, -1, present));
    g_assert_nonnull(g_strstr_len(all, -1, run_identifier));
    g_assert_nonnull(g_strstr_len(all, -1, person));
    g_assert_nonnull(g_strstr_len(all, -1, note));
    g_assert_nonnull(g_strstr_len(all, -1, origin));
    g_assert_nonnull(g_strstr_len(all, -1, artifact_sha));
    g_assert_null(g_strstr_len(all, -1, absent));
    g_free(all);
}

static gboolean present_dialog(TestContext *context)
{
    return evidence_metadata_dialog_present_with_ocr(
        context->main_window, context->record, context->types,
        context->database, context->root,
        analyze_requested, completed, context);
}

static gboolean drive(gpointer data)
{
    TestContext *context = data;
    GtkWindow *dialog = find_metadata_window(context);
    g_test_message("drive phase=%u fenêtre=%s cycles=%u",
        context->phase, dialog != NULL ? "oui" : "non",
        context->close_cycles);
    if (dialog != NULL) {
        g_assert_true(gtk_window_get_transient_for(dialog) ==
            context->main_window);
        g_assert_true(gtk_window_get_modal(dialog));
        g_assert_true(gtk_window_get_destroy_with_parent(dialog));
        g_assert_nonnull(find_named(GTK_WIDGET(dialog),
            "document-authenticity-editor"));
        g_assert_nonnull(find_named(GTK_WIDGET(dialog),
            "document-identity-misuse-editor"));
    }
    if (dialog == NULL) {
        if (context->phase == 1) {
            insert_run(context, FALSE);
            context->phase = 2;
        } else if (context->phase == 3) {
            insert_run(context, TRUE);
            context->phase = 4;
        } else if (context->phase == 6) {
            database_close(context->database);
            context->database = database_open(context->database_path);
            g_assert_nonnull(context->database);
        }
        g_assert_true(present_dialog(context));
        g_idle_add(drive, context);
        return G_SOURCE_REMOVE;
    }
    if (!gtk_widget_get_visible(GTK_WIDGET(dialog)))
        return G_SOURCE_REMOVE;
    GtkDropDown *selector = GTK_DROP_DOWN(find_named(
        GTK_WIDGET(dialog), "evidence-ocr-run-selector"));
    if (context->phase == 0) {
        g_assert_null(selector);
        g_assert_null(find_named(GTK_WIDGET(dialog),
            "revise-existing-identity-ocr"));
        g_assert_nonnull(find_named(GTK_WIDGET(dialog),
            "analyze-existing-identity"));
        context->phase = 1;
        gtk_window_close(dialog);
    } else if (context->phase == 2) {
        g_assert_nonnull(selector);
        g_assert_cmpuint(g_list_model_get_n_items(
            gtk_drop_down_get_model(selector)), ==, 1);
        assert_run_content(dialog, 0, "RUN A OCR BRUT",
            "RUN B OCR BRUT", context->run_a, PERSON_A,
            "NOTE RUN A", "manual_override", SHA_A);
        context->phase = 3;
        gtk_window_close(dialog);
    } else if (context->phase == 4) {
        g_assert_nonnull(selector);
        g_assert_cmpuint(g_list_model_get_n_items(
            gtk_drop_down_get_model(selector)), ==, 2);
        assert_run_content(dialog, 0, "RUN A OCR BRUT",
            "RUN B OCR BRUT", context->run_a, PERSON_A,
            "NOTE RUN A", "manual_override", SHA_A);
        GtkWidget *show = find_button(
            GTK_WIDGET(dialog), "Voir la zone");
        g_assert_nonnull(show);
        g_signal_emit_by_name(show, "clicked");
        GtkWidget *section = find_named(
            GTK_WIDGET(dialog), "evidence-ocr-readonly");
        OcrProvenanceOverlay *overlay = g_object_get_data(
            G_OBJECT(section), "ocr-readonly-overlay");
        g_assert_true(ocr_provenance_overlay_has_region(overlay));
        for (guint index = 0; index < 20; index++)
            gtk_drop_down_set_selected(selector, index % 2);
        assert_run_content(dialog, 1, "RUN B OCR BRUT",
            "RUN A OCR BRUT", context->run_b, PERSON_B,
            "NOTE RUN B", "manual_entry", SHA_B);
        assert_run_content(dialog, 0, "RUN A CORRIGÉ",
            "RUN B CORRIGÉ", context->run_a, PERSON_A,
            "NOTE RUN A", "manual_override", SHA_A);
        context->phase = 5;
        g_signal_emit_by_name(find_named(GTK_WIDGET(dialog),
            "revise-existing-identity-ocr"), "clicked");
    } else if (context->phase == 5) {
        g_assert_nonnull(selector);
        g_assert_true(context->revision_requested);
        assert_run_content(dialog, 0, "RUN A RÉVISÉ",
            "RUN B CORRIGÉ", context->run_a, PERSON_A,
            "NOTE RUN A", "manual_override", SHA_A);
        assert_run_content(dialog, 1, "RUN B CORRIGÉ",
            "RUN A RÉVISÉ", context->run_b, PERSON_B,
            "NOTE RUN B", "manual_entry", SHA_B);
        context->phase = 6;
        g_signal_emit_by_name(find_named(GTK_WIDGET(dialog),
            "analyze-existing-identity"), "clicked");
    } else if (context->phase == 6) {
        g_assert_nonnull(selector);
        g_assert_true(context->relaunch_requested);
        g_assert_cmpuint(g_list_model_get_n_items(
            gtk_drop_down_get_model(selector)), ==, 3);
        g_assert_cmpuint(gtk_drop_down_get_selected(selector), ==, 2);
        assert_run_content(dialog, 2, "RUN C OCR BRUT",
            "RUN A OCR BRUT", context->run_c, PERSON_B,
            "NOTE RUN C", "manual_entry", SHA_C);
        assert_run_content(dialog, 0, "RUN A RÉVISÉ",
            "RUN C OCR BRUT", context->run_a, PERSON_A,
            "NOTE RUN A", "manual_override", SHA_A);
        context->phase = 7;
        gtk_window_close(dialog);
    } else {
        context->close_cycles++;
        if (context->close_cycles < 20) {
            gtk_window_close(dialog);
        } else {
            context->passed = TRUE;
            gtk_window_close(dialog);
            g_application_quit(G_APPLICATION(context->application));
        }
    }
    return G_SOURCE_REMOVE;
}

static void activate(GtkApplication *application, gpointer data)
{
    TestContext *context = data;
    GError *error = NULL;
    char *database_directory =
        g_build_filename(context->root, "00_BaseDeDonnees", NULL);
    g_mkdir_with_parents(database_directory, 0700);
    context->database_path =
        g_build_filename(database_directory, "Enquete.sqlite", NULL);
    g_assert_true(database_initialize(context->database_path,
        "Enquête fiche OCR SPECIMEN", context->root));
    context->database = database_open(context->database_path);
    g_assert_nonnull(context->database);
    EntityDao *entity_dao = entity_dao_new(context->database, &error);
    EntityRecord *person_a = entity_record_new(
        PERSON_A, "person", "PERSONNE A SPECIMEN", "PERSONNE A SPECIMEN",
        NULL, 0, "2026-07-29T09:00:00Z", "2026-07-29T09:00:00Z",
        ENTITY_STATUS_ACTIVE, &error);
    EntityRecord *person_b = entity_record_new(
        PERSON_B, "person", "PERSONNE B SPECIMEN", "PERSONNE B SPECIMEN",
        NULL, 0, "2026-07-29T09:00:00Z", "2026-07-29T09:00:00Z",
        ENTITY_STATUS_ACTIVE, &error);
    g_assert_true(entity_dao_insert(entity_dao, person_a, &error));
    g_assert_true(entity_dao_insert(entity_dao, person_b, &error));
    g_assert_no_error(error);
    entity_record_free(person_a);
    entity_record_free(person_b);
    entity_dao_free(entity_dao);
    context->record = evidence_record_new(
        EVIDENCE_ID, "SPECIMEN.png", "SPECIMEN.png",
        "01_Preuves_Originales/SPECIMEN.png", "document", 8,
        SHA_EVIDENCE, "2026-07-29T09:00:00Z",
        "2026-07-29T09:00:00Z", "SPECIMEN",
        "Preuve multi-run SPECIMEN",
        EVIDENCE_INTEGRITY_STATUS_VALID, &error);
    EvidenceDao *evidence_dao = evidence_dao_new(context->database, &error);
    gboolean evidence_inserted = evidence_dao_insert(
        evidence_dao, context->record, &error);
    if (!evidence_inserted)
        g_test_message("Insertion preuve SPECIMEN: %s",
            error != NULL ? error->message : "erreur inconnue");
    g_assert_no_error(error);
    g_assert_true(evidence_inserted);
    evidence_dao_free(evidence_dao);
    context->main_window =
        GTK_WINDOW(gtk_application_window_new(application));
    gtk_window_present(context->main_window);
    g_assert_true(present_dialog(context));
    g_idle_add(drive, context);
    g_free(database_directory);
}

int main(int argc, char **argv)
{
    if (!gtk_init_check()) {
        g_print("SKIP: aucun affichage GTK disponible.\n");
        return 0;
    }
    TestContext context = {0};
    context.root = g_dir_make_tmp("labfy-metadata-ocr-gtk-XXXXXX", NULL);
    context.types = g_ptr_array_new_with_free_func(
        (GDestroyNotify) evidence_type_free);
    GError *error = NULL;
    g_ptr_array_add(context.types,
        evidence_type_new(1, "document", "Document SPECIMEN", NULL, &error));
    g_assert_no_error(error);
    context.application = gtk_application_new(
        "com.labfytools.test.metadata.ocr",
        G_APPLICATION_NON_UNIQUE);
    g_signal_connect(context.application, "activate",
        G_CALLBACK(activate), &context);
    int status = g_application_run(
        G_APPLICATION(context.application), argc, argv);
    g_assert_true(context.passed);
    database_close(context.database);
    g_ptr_array_unref(context.types);
    evidence_record_free(context.record);
    g_object_unref(context.application);
    g_free(context.database_path);
    g_free(context.run_a);
    g_free(context.run_b);
    g_free(context.run_c);
    remove_tree(context.root);
    g_free(context.root);
    return status;
}
