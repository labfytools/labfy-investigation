#include "views/create_person_dialog.h"
#include <gtk/gtk.h>
#include <string.h>

typedef struct
{
    GtkApplication *application;
    GtkWindow *main_window;
    TaskManager *task_manager;
    guint completion_count;
    gboolean passed;
    gboolean evidence_model_destroyed;
    GListModel *evidence_model;
} TestContext;

static void evidence_model_destroyed(gpointer data, GObject *object)
{
    TestContext *context = data;
    (void) object;
    context->evidence_model_destroyed = TRUE;
}

static GtkWidget *find_button(GtkWidget *widget, const char *label)
{
    if (GTK_IS_BUTTON(widget) &&
        g_strcmp0(gtk_button_get_label(GTK_BUTTON(widget)), label) == 0)
        return widget;
    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL; child = gtk_widget_get_next_sibling(child)) {
        GtkWidget *match = find_button(child, label);
        if (match != NULL) return match;
    }
    return NULL;
}

static GtkWidget *find_entry(GtkWidget *widget, const char *placeholder)
{
    if (GTK_IS_ENTRY(widget) && g_strcmp0(
            gtk_entry_get_placeholder_text(GTK_ENTRY(widget)),
            placeholder) == 0)
        return widget;
    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL; child = gtk_widget_get_next_sibling(child)) {
        GtkWidget *match = find_entry(child, placeholder);
        if (match != NULL) return match;
    }
    return NULL;
}

static GtkDropDown *find_drop_down_starting_with(GtkWidget *widget,
    const char *first_label)
{
    if (GTK_IS_DROP_DOWN(widget)) {
        GListModel *model = gtk_drop_down_get_model(GTK_DROP_DOWN(widget));
        GtkStringObject *first = model != NULL &&
            g_list_model_get_n_items(model) > 0
            ? g_list_model_get_item(model, 0) : NULL;
        gboolean matches = first != NULL && g_strcmp0(
            gtk_string_object_get_string(first), first_label) == 0;
        g_clear_object(&first);
        if (matches) return GTK_DROP_DOWN(widget);
    }
    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL; child = gtk_widget_get_next_sibling(child)) {
        GtkDropDown *match = find_drop_down_starting_with(child, first_label);
        if (match != NULL) return match;
    }
    return NULL;
}

static GtkWidget *find_named(GtkWidget *widget, const char *name)
{
    if (g_strcmp0(gtk_widget_get_name(widget), name) == 0)
        return widget;
    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL; child = gtk_widget_get_next_sibling(child)) {
        GtkWidget *match = find_named(child, name);
        if (match != NULL) return match;
    }
    return NULL;
}

static gboolean frame_seen(GtkWidget *widget, GdkFrameClock *clock,
    gpointer data)
{
    gboolean *seen = data;
    (void) widget;
    (void) clock;
    *seen = TRUE;
    return G_SOURCE_REMOVE;
}

static void wait_for_frame(GtkWidget *widget)
{
    gboolean seen = FALSE;
    gtk_widget_add_tick_callback(widget, frame_seen, &seen, NULL);
    while (!seen)
        g_main_context_iteration(NULL, TRUE);
}

static void select_drop_down(GtkDropDown *dropdown, guint position)
{
    g_test_message("sélecteur=%s position=%u mapped=%d visible=%d",
        gtk_widget_get_name(GTK_WIDGET(dropdown)), position,
        gtk_widget_get_mapped(GTK_WIDGET(dropdown)),
        gtk_widget_get_visible(GTK_WIDGET(dropdown)));
    g_assert_true(gtk_widget_get_mapped(GTK_WIDGET(dropdown)));
    g_assert_true(gtk_widget_activate(GTK_WIDGET(dropdown)));
    wait_for_frame(GTK_WIDGET(dropdown));
    g_assert_true(gtk_widget_activate(GTK_WIDGET(dropdown)));
    wait_for_frame(GTK_WIDGET(dropdown));
    gtk_drop_down_set_selected(dropdown, position);
    g_main_context_iteration(NULL, FALSE);
    g_assert_cmpuint(gtk_drop_down_get_selected(dropdown), ==, position);
}

static GtkWidget *find_label_containing(GtkWidget *widget, const char *text)
{
    if (GTK_IS_LABEL(widget) &&
        strstr(gtk_label_get_text(GTK_LABEL(widget)), text) != NULL)
        return widget;
    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL; child = gtk_widget_get_next_sibling(child)) {
        GtkWidget *match = find_label_containing(child, text);
        if (match != NULL) return match;
    }
    return NULL;
}

static void completed(CreatePersonDialogResult *result, gpointer data)
{
    TestContext *context = data;
    g_assert_null(result);
    context->completion_count++;
}

static gboolean check_after_cancel(gpointer data)
{
    TestContext *context = data;
    GList *windows = gtk_application_get_windows(context->application);
    g_assert_cmpuint(context->completion_count, ==, 1);
    g_assert_true(context->evidence_model_destroyed);
    g_assert_true(gtk_widget_get_visible(GTK_WIDGET(context->main_window)));
    g_assert_cmpuint(g_list_length(windows), ==, 1);
    g_assert_true(windows->data == context->main_window);
    context->passed = TRUE;
    g_application_quit(G_APPLICATION(context->application));
    return G_SOURCE_REMOVE;
}

static gboolean click_cancel(gpointer data)
{
    TestContext *context = data;
    GList *windows = gtk_application_get_windows(context->application);
    GtkWindow *dialog = windows != NULL && windows->data != context->main_window
        ? windows->data : windows != NULL ? windows->next->data : NULL;
    GtkWidget *button, *designation, *search;
    GtkDropDown *evidence, *retained_type, *type_filter;
    g_assert_nonnull(dialog);
    g_assert_true(gtk_window_get_transient_for(dialog) ==
        context->main_window);
    g_assert_true(gtk_window_get_modal(dialog));
    g_assert_true(gtk_window_get_destroy_with_parent(dialog));
    gtk_window_set_default_size(dialog, 760, 560);
    wait_for_frame(GTK_WIDGET(dialog));
    designation = find_entry(GTK_WIDGET(dialog),
        "Personne présumée liée aux comptes");
    g_assert_nonnull(designation);
    gtk_editable_set_text(GTK_EDITABLE(designation), "SPECIMEN");
    button = find_button(GTK_WIDGET(dialog), "Suivant");
    g_assert_nonnull(button);
    g_signal_emit_by_name(button, "clicked");
    wait_for_frame(GTK_WIDGET(button));
    g_signal_emit_by_name(button, "clicked");
    wait_for_frame(GTK_WIDGET(button));
    search = find_entry(GTK_WIDGET(dialog),
        "Rechercher par nom, description ou type");
    g_assert_nonnull(search);
    evidence = GTK_DROP_DOWN(find_named(GTK_WIDGET(dialog),
        "create-person-evidence-dropdown"));
    g_assert_nonnull(evidence);
    context->evidence_model = gtk_drop_down_get_model(evidence);
    g_assert_nonnull(context->evidence_model);
    g_object_weak_ref(G_OBJECT(context->evidence_model),
        evidence_model_destroyed, context);
    select_drop_down(evidence, 1);
    select_drop_down(evidence, 2);
    select_drop_down(evidence, 0);
    select_drop_down(evidence, 1);
    gtk_editable_set_text(GTK_EDITABLE(search), "recherche");
    g_assert_true(gtk_drop_down_get_model(evidence) ==
        context->evidence_model);
    g_assert_false(context->evidence_model_destroyed);
    type_filter = GTK_DROP_DOWN(find_named(GTK_WIDGET(dialog),
        "create-person-evidence-type-filter"));
    g_assert_nonnull(type_filter);
    select_drop_down(type_filter, 2);
    select_drop_down(evidence, 1);
    g_assert_cmpuint(gtk_drop_down_get_selected(evidence), ==, 1);
    button = find_button(GTK_WIDGET(dialog), "Ajouter à la sélection");
    g_assert_nonnull(button);
    g_signal_emit_by_name(button, "clicked");
    retained_type = find_drop_down_starting_with(
        GTK_WIDGET(dialog), "Capture d’écran");
    g_assert_nonnull(retained_type);
    gtk_drop_down_set_selected(retained_type, 4);
    g_assert_nonnull(find_label_containing(GTK_WIDGET(dialog),
        "Type : email (email)"));
    g_assert_nonnull(find_label_containing(GTK_WIDGET(dialog), "— email"));
    select_drop_down(type_filter, 0);
    gtk_editable_set_text(GTK_EDITABLE(search), "AUTRE");
    g_assert_true(gtk_drop_down_get_model(evidence) ==
        context->evidence_model);
    select_drop_down(evidence, 1);
    button = find_button(GTK_WIDGET(dialog), "Ajouter à la sélection");
    g_assert_nonnull(button);
    g_signal_emit_by_name(button, "clicked");
    button = find_button(GTK_WIDGET(dialog), "Suivant");
    g_signal_emit_by_name(button, "clicked");
    g_assert_nonnull(find_label_containing(GTK_WIDGET(dialog),
        "L’OCR produit des propositions à vérifier"));
    g_signal_emit_by_name(button, "clicked");
    g_assert_nonnull(find_label_containing(GTK_WIDGET(dialog),
        "Désignation : SPECIMEN"));
    g_assert_nonnull(find_label_containing(GTK_WIDGET(dialog),
        "SPECIMEN-recherche.png"));
    g_assert_nonnull(find_label_containing(GTK_WIDGET(dialog),
        "Type métier : email"));
    g_assert_nonnull(find_label_containing(GTK_WIDGET(dialog),
        "AUTRE-SPECIMEN.jpg"));
    g_assert_nonnull(find_label_containing(GTK_WIDGET(dialog),
        "Aucune écriture n’a encore été effectuée"));
    button = find_button(GTK_WIDGET(dialog), "Annuler");
    g_assert_nonnull(button);
    g_signal_emit_by_name(button, "clicked");
    g_idle_add(check_after_cancel, context);
    return G_SOURCE_REMOVE;
}

static void activate(GtkApplication *application, gpointer data)
{
    TestContext *context = data;
    GError *error = NULL;
    GPtrArray *records = g_ptr_array_new_with_free_func(
        (GDestroyNotify) evidence_record_free);
    EvidenceRecord *record = evidence_record_new(
        "10000000-0000-4000-8000-000000000001",
        "SPECIMEN-recherche.png", "specimen.png",
        "01_Preuves_Originales/specimen.png", "screenshot", 12,
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "2026-07-28T10:00:00Z", NULL, NULL,
        "Description synthétique", EVIDENCE_INTEGRITY_STATUS_UNKNOWN,
        &error);
    g_assert_no_error(error);
    evidence_record_set_display_metadata(
        record, "Capture d’écran", NULL);
    g_ptr_array_add(records, record);
    record = evidence_record_new(
        "10000000-0000-4000-8000-000000000002",
        "AUTRE-SPECIMEN.jpg", "autre.jpg",
        "01_Preuves_Originales/autre.jpg", "photo", 24,
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
        "2026-07-28T10:00:00Z", NULL, NULL,
        "Deuxième preuve synthétique", EVIDENCE_INTEGRITY_STATUS_UNKNOWN,
        &error);
    g_assert_no_error(error);
    evidence_record_set_display_metadata(record, "Photo", "image/jpeg");
    g_ptr_array_add(records, record);
    context->main_window = GTK_WINDOW(
        gtk_application_window_new(application));
    gtk_window_present(context->main_window);
    g_assert_true(create_person_dialog_present(context->main_window,
        records, "/tmp", context->task_manager, NULL, NULL, completed,
        context, NULL));
    g_ptr_array_unref(records);
    g_idle_add(click_cancel, context);
}

int main(int argc, char **argv)
{
    TestContext context = {0};
    int status;
    if (!gtk_init_check()) {
        g_print("SKIP: aucun affichage GTK disponible.\n");
        return 0;
    }
    g_object_set(gtk_settings_get_default(),
        "gtk-enable-animations", FALSE, NULL);
    context.application = gtk_application_new(
        "org.labfy.Investigation.DialogTest",
        G_APPLICATION_NON_UNIQUE);
    context.task_manager = task_manager_new();
    g_signal_connect(context.application, "activate",
        G_CALLBACK(activate), &context);
    status = g_application_run(
        G_APPLICATION(context.application), argc, argv);
    g_assert_true(context.passed);
    task_manager_free(context.task_manager);
    g_object_unref(context.application);
    return status;
}
