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
} TestContext;

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
    g_assert_nonnull(dialog);
    designation = find_entry(GTK_WIDGET(dialog),
        "Personne présumée liée aux comptes");
    g_assert_nonnull(designation);
    gtk_editable_set_text(GTK_EDITABLE(designation), "SPECIMEN");
    button = find_button(GTK_WIDGET(dialog), "Suivant");
    g_assert_nonnull(button);
    g_signal_emit_by_name(button, "clicked");
    g_signal_emit_by_name(button, "clicked");
    search = find_entry(GTK_WIDGET(dialog),
        "Rechercher par nom, description ou type");
    g_assert_nonnull(search);
    gtk_editable_set_text(GTK_EDITABLE(search), "SPECIMEN");
    button = find_button(GTK_WIDGET(dialog), "Suivant");
    g_signal_emit_by_name(button, "clicked");
    g_assert_nonnull(find_label_containing(GTK_WIDGET(dialog),
        "Désignation : SPECIMEN"));
    g_assert_nonnull(find_label_containing(GTK_WIDGET(dialog),
        "SPECIMEN-recherche.png"));
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
    context->main_window = GTK_WINDOW(
        gtk_application_window_new(application));
    gtk_window_present(context->main_window);
    g_assert_true(create_person_dialog_present(context->main_window,
        records, "/tmp", context->task_manager, NULL, completed,
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
