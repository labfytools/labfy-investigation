/******************************************************************************
 * @file metadata_analysis_dialog.c
 * @brief Fenêtre de consultation des métadonnées extraites.
 ******************************************************************************/

#include "views/metadata_analysis_dialog.h"
#include "views/dialog_geometry.h"

static gboolean metadata_analysis_dialog_on_escape(
    GtkWidget *widget, GVariant *arguments, gpointer user_data)
{
    (void) widget;
    (void) arguments;
    gtk_window_destroy(GTK_WINDOW(user_data));
    return TRUE;
}

void metadata_analysis_dialog_present(GtkWindow *parent, const char *summary,
    const char *json_path, const char *version)
{
    GtkWidget *window = NULL;
    GtkWidget *content = NULL;
    GtkWidget *heading = NULL;
    GtkWidget *scrolled = NULL;
    GtkWidget *text_view = NULL;
    GtkTextBuffer *buffer = NULL;
    GtkWidget *path_label = NULL;
    GtkWidget *close_button = NULL;
    char *heading_text = NULL;
    char *path_text = NULL;

    window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(window), "Métadonnées de la preuve");
    labfy_dialog_prepare(GTK_WINDOW(window), parent, TRUE, TRUE);
    content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(content, 16);
    gtk_widget_set_margin_bottom(content, 16);
    gtk_widget_set_margin_start(content, 16);
    gtk_widget_set_margin_end(content, 16);
    heading_text = g_strdup_printf("Extraction locale — ExifTool %s",
        version != NULL ? version : "version inconnue");
    heading = gtk_label_new(heading_text);
    gtk_widget_set_halign(heading, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(content), heading);
    scrolled = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(text_view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD_CHAR);
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_text_buffer_set_text(buffer, summary != NULL ? summary :
        "Aucune métadonnée exploitable.", -1);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), text_view);
    gtk_box_append(GTK_BOX(content), scrolled);
    path_text = g_strdup_printf("Sortie JSON conservée : %s",
        json_path != NULL ? json_path : "indisponible");
    path_label = gtk_label_new(path_text);
    gtk_label_set_wrap(GTK_LABEL(path_label), TRUE);
    gtk_label_set_selectable(GTK_LABEL(path_label), TRUE);
    gtk_widget_set_halign(path_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(content), path_label);
    close_button = gtk_button_new_with_label("Fermer");
    gtk_widget_set_halign(close_button, GTK_ALIGN_END);
    g_signal_connect_swapped(close_button, "clicked",
        G_CALLBACK(gtk_window_destroy), window);
    {
        GtkShortcutController *shortcuts = GTK_SHORTCUT_CONTROLLER(
            gtk_shortcut_controller_new());
        gtk_shortcut_controller_set_scope(
            shortcuts, GTK_SHORTCUT_SCOPE_LOCAL);
        gtk_shortcut_controller_add_shortcut(shortcuts,
            gtk_shortcut_new(
                gtk_keyval_trigger_new(GDK_KEY_Escape, 0),
                gtk_callback_action_new(
                    metadata_analysis_dialog_on_escape, window, NULL)));
        gtk_widget_add_controller(window, GTK_EVENT_CONTROLLER(shortcuts));
    }
    gtk_box_append(GTK_BOX(content), close_button);
    gtk_window_set_child(GTK_WINDOW(window), content);
    labfy_dialog_present(GTK_WINDOW(window));
    g_free(heading_text);
    g_free(path_text);
}
