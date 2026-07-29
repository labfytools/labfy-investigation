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

static GtkDropDown *find_evidence_drop_down(GtkWidget *widget)
{
    if (GTK_IS_DROP_DOWN(widget)) {
        GListModel *model = gtk_drop_down_get_model(GTK_DROP_DOWN(widget));
        GtkStringObject *first = model != NULL &&
            g_list_model_get_n_items(model) > 0
            ? g_list_model_get_item(model, 0) : NULL;
        gboolean matches = first != NULL && g_strcmp0(
            gtk_string_object_get_string(first),
            "Aucune preuve associée") == 0;
        g_clear_object(&first);
        if (matches) return GTK_DROP_DOWN(widget);
    }
    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL; child = gtk_widget_get_next_sibling(child)) {
        GtkDropDown *match = find_evidence_drop_down(child);
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

static GtkWidget *find_first_button(GtkWidget *widget)
{
    if (GTK_IS_BUTTON(widget)) return widget;
    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL; child = gtk_widget_get_next_sibling(child)) {
        GtkWidget *match = find_first_button(child);
        if (match != NULL) return match;
    }
    return NULL;
}

static GtkListView *find_list_view(GtkWidget *widget, gboolean mapped_only)
{
    if (GTK_IS_LIST_VIEW(widget) &&
        (!mapped_only || gtk_widget_get_mapped(widget)))
        return GTK_LIST_VIEW(widget);
    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL; child = gtk_widget_get_next_sibling(child)) {
        GtkListView *match = find_list_view(child, mapped_only);
        if (match != NULL) return match;
    }
    return NULL;
}

static GtkListView *find_popup_list_view(GtkDropDown *dropdown)
{
    GtkWidget *root = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(dropdown)));
    GtkListView *list_view;
    GtkWidget *button = find_first_button(GTK_WIDGET(dropdown));
    g_assert_nonnull(button);
    if (GTK_IS_TOGGLE_BUTTON(button))
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
    else
        gtk_widget_activate(button);
    while (g_main_context_iteration(NULL, FALSE)) {}
    list_view = find_list_view(GTK_WIDGET(dropdown), TRUE);
    if (list_view == NULL && root != NULL)
        list_view = find_list_view(root, TRUE);
    return list_view;
}

static void activate_popup_item(GtkListView *list_view, guint position)
{
    GtkSelectionModel *selection = gtk_list_view_get_model(list_view);
    g_assert_nonnull(selection);
    g_assert_true(gtk_selection_model_select_item(
        selection, position, TRUE));
    g_signal_emit_by_name(list_view, "activate", position);
    while (g_main_context_iteration(NULL, FALSE)) {}
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
    evidence = find_evidence_drop_down(GTK_WIDGET(dialog));
    g_assert_nonnull(evidence);
    context->evidence_model = gtk_drop_down_get_model(evidence);
    g_assert_nonnull(context->evidence_model);
    g_object_weak_ref(G_OBJECT(context->evidence_model),
        evidence_model_destroyed, context);
    GtkListView *popup_list = find_popup_list_view(evidence);
    g_assert_nonnull(popup_list);
    activate_popup_item(popup_list, 1);
    popup_list = find_popup_list_view(evidence);
    g_assert_nonnull(popup_list);
    activate_popup_item(popup_list, 2);
    popup_list = find_popup_list_view(evidence);
    g_assert_nonnull(popup_list);
    activate_popup_item(popup_list, 0);
    popup_list = find_popup_list_view(evidence);
    g_assert_nonnull(popup_list);
    activate_popup_item(popup_list, 1);
    gtk_editable_set_text(GTK_EDITABLE(search), "recherche");
    g_assert_true(gtk_drop_down_get_model(evidence) ==
        context->evidence_model);
    g_assert_false(context->evidence_model_destroyed);
    type_filter = find_drop_down_starting_with(
        GTK_WIDGET(dialog), "Tous les types");
    g_assert_nonnull(type_filter);
    popup_list = find_popup_list_view(type_filter);
    g_assert_nonnull(popup_list);
    activate_popup_item(popup_list, 2);
    popup_list = find_popup_list_view(evidence);
    g_assert_nonnull(popup_list);
    activate_popup_item(popup_list, 1);
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
    popup_list = find_popup_list_view(type_filter);
    g_assert_nonnull(popup_list);
    activate_popup_item(popup_list, 0);
    gtk_editable_set_text(GTK_EDITABLE(search), "AUTRE");
    g_assert_true(gtk_drop_down_get_model(evidence) ==
        context->evidence_model);
    popup_list = find_popup_list_view(evidence);
    g_assert_nonnull(popup_list);
    activate_popup_item(popup_list, 1);
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
