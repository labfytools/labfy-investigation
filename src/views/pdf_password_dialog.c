/******************************************************************************
 * @file pdf_password_dialog.c
 * @brief Paramétrage d'une récupération de mot de passe PDF.
 ******************************************************************************/

#include "views/pdf_password_dialog.h"

typedef struct
{
    GtkWidget *window;
    GtkWidget *method;
    GtkWidget *parameter;
    GtkWidget *status;
    PdfPasswordDialogCallback callback;
    gpointer user_data;
} PdfPasswordDialog;

/** @brief Libère le contexte à la destruction de la fenêtre. */
static void pdf_password_dialog_destroyed(GtkWidget *widget, gpointer data)
{
    (void) widget;
    g_free(data);
}

/** @brief Actualise l'aide selon la méthode choisie. */
static void pdf_password_dialog_method_changed(GObject *object,
    GParamSpec *specification, gpointer data)
{
    PdfPasswordDialog *dialog = data;
    guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(object));
    (void) specification;
    gtk_entry_set_placeholder_text(GTK_ENTRY(dialog->parameter), selected == 0 ?
        "/chemin/vers/dictionnaire.txt" : selected == 1 ?
        "?d?d?d?d?d?d" : "minimum:maximum (ex. 0:999999)");
}

/** @brief Valide le formulaire et transmet une copie éphémère du paramètre. */
static void pdf_password_dialog_started(GtkButton *button, gpointer data)
{
    PdfPasswordDialog *dialog = data;
    const char *parameter = gtk_editable_get_text(
        GTK_EDITABLE(dialog->parameter));
    guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(dialog->method));
    (void) button;
    if (parameter == NULL || parameter[0] == '\0')
    {
        gtk_label_set_text(GTK_LABEL(dialog->status),
            selected == 0 ? "Choisissez un dictionnaire." :
            "Saisissez un masque John.");
        return;
    }
    if (selected == 0 && !g_file_test(parameter, G_FILE_TEST_IS_REGULAR))
    {
        gtk_label_set_text(GTK_LABEL(dialog->status),
            "Le dictionnaire indiqué est introuvable.");
        return;
    }
    if (selected == 2)
    {
        gchar **bounds = g_strsplit(parameter, ":", 2);
        guint64 minimum = bounds[0] != NULL ? g_ascii_strtoull(bounds[0], NULL, 10) : 0;
        guint64 maximum = bounds[1] != NULL ? g_ascii_strtoull(bounds[1], NULL, 10) : 0;
        gboolean valid = bounds[0] != NULL && bounds[1] != NULL &&
            bounds[0][0] != '\0' && bounds[1][0] != '\0' && minimum <= maximum &&
            maximum <= 9999999999999ULL;
        g_strfreev(bounds);
        if (!valid)
        {
            gtk_label_set_text(GTK_LABEL(dialog->status),
                "Plage invalide : utilisez minimum:maximum, jusqu'à 9999999999999.");
            return;
        }
    }
    if (dialog->callback != NULL)
        dialog->callback(selected == 0 ? PDF_PASSWORD_RECOVERY_DICTIONARY :
            selected == 1 ? PDF_PASSWORD_RECOVERY_MASK :
            PDF_PASSWORD_RECOVERY_NUMERIC_RANGE, parameter, dialog->user_data);
    gtk_window_destroy(GTK_WINDOW(dialog->window));
}

void pdf_password_dialog_present(GtkWindow *parent,
    PdfPasswordDialogCallback callback, gpointer user_data)
{
    PdfPasswordDialog *dialog = g_new0(PdfPasswordDialog, 1);
    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *description = gtk_label_new(
        "La recherche s'effectue localement sur une copie. Commencez par un "
        "dictionnaire ciblé ; un masque trop large peut durer très longtemps.");
    GtkStringList *methods = gtk_string_list_new((const char *[])
        { "Dictionnaire", "Masque John", "Plage numérique", NULL });
    GtkWidget *start = NULL;
    GtkWidget *cancel = NULL;
    GtkWidget *buttons = NULL;

    dialog->window = gtk_window_new();
    dialog->callback = callback;
    dialog->user_data = user_data;
    gtk_window_set_title(GTK_WINDOW(dialog->window),
        "Récupérer le mot de passe PDF");
    gtk_window_set_default_size(GTK_WINDOW(dialog->window), 560, 300);
    gtk_window_set_modal(GTK_WINDOW(dialog->window), TRUE);
    if (parent != NULL)
        gtk_window_set_transient_for(GTK_WINDOW(dialog->window), parent);
    gtk_widget_set_margin_top(content, 18);
    gtk_widget_set_margin_bottom(content, 18);
    gtk_widget_set_margin_start(content, 18);
    gtk_widget_set_margin_end(content, 18);
    gtk_label_set_wrap(GTK_LABEL(description), TRUE);
    gtk_widget_set_halign(description, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(content), description);
    dialog->method = gtk_drop_down_new(G_LIST_MODEL(methods), NULL);
    gtk_box_append(GTK_BOX(content), dialog->method);
    dialog->parameter = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(dialog->parameter),
        "/chemin/vers/dictionnaire.txt");
    gtk_box_append(GTK_BOX(content), dialog->parameter);
    dialog->status = gtk_label_new("");
    gtk_widget_set_halign(dialog->status, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(content), dialog->status);
    buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(buttons, GTK_ALIGN_END);
    cancel = gtk_button_new_with_label("Annuler");
    start = gtk_button_new_with_label("Lancer la récupération");
    gtk_box_append(GTK_BOX(buttons), cancel);
    gtk_box_append(GTK_BOX(buttons), start);
    gtk_box_append(GTK_BOX(content), buttons);
    gtk_window_set_child(GTK_WINDOW(dialog->window), content);
    g_signal_connect(dialog->method, "notify::selected",
        G_CALLBACK(pdf_password_dialog_method_changed), dialog);
    g_signal_connect(start, "clicked",
        G_CALLBACK(pdf_password_dialog_started), dialog);
    g_signal_connect_swapped(cancel, "clicked",
        G_CALLBACK(gtk_window_destroy), dialog->window);
    g_signal_connect(dialog->window, "destroy",
        G_CALLBACK(pdf_password_dialog_destroyed), dialog);
    gtk_window_present(GTK_WINDOW(dialog->window));
    /* GtkDropDown conserve le modèle pendant toute la durée de la fenêtre. */
}
