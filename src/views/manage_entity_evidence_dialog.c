/******************************************************************************
 * @file manage_entity_evidence_dialog.c
 * @brief Dialogue de sélection des preuves associées à une entité.
 ******************************************************************************/
#include "views/manage_entity_evidence_dialog.h"
#include <gio/gio.h>
typedef struct
{
    char *identifier;
    char *path;
    GtkCheckButton *check;
} EvidenceChoice;
typedef struct
{
    GtkWindow *window;
    GtkStack *preview;
    GtkPicture *picture;
    GtkVideo *video;
    GPtrArray *choices;
    ManageEntityEvidenceDialogCallback callback;
    gpointer callback_data;
    gboolean completed;
} ManageEvidenceState;
/** @brief Libère une option privée. */
static void evidence_choice_free(gpointer data)
{
    EvidenceChoice *choice = data;
    if (choice == NULL) return;
    g_free(choice->identifier); g_free(choice->path); g_free(choice);
}
/** @brief Arrête et efface l'aperçu courant. */
static void manage_evidence_clear_preview(ManageEvidenceState *state)
{
    GtkMediaStream *stream = NULL;
    if (state == NULL || state->video == NULL) return;
    stream = gtk_video_get_media_stream(state->video);
    if (stream != NULL) gtk_media_stream_pause(stream);
    gtk_video_set_media_stream(state->video, NULL);
    gtk_picture_set_paintable(state->picture, NULL);
}
/** @brief Libère l'état du dialogue. */
static void manage_evidence_state_free(gpointer data)
{
    ManageEvidenceState *state = data;
    if (state == NULL) return;
    manage_evidence_clear_preview(state);
    g_ptr_array_unref(state->choices); g_free(state);
}
/** @brief Affiche l'aperçu de l'option manipulée. */
static void manage_evidence_on_toggled(GtkCheckButton *button, gpointer data)
{
    ManageEvidenceState *state = data;
    EvidenceChoice *choice = g_object_get_data(G_OBJECT(button), "choice");
    GFile *file = NULL; GFileInfo *info = NULL; char *mime = NULL;
    const char *content_type = NULL;
    if (state == NULL || choice == NULL || choice->path == NULL) return;
    manage_evidence_clear_preview(state);
    file = g_file_new_for_path(choice->path);
    info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
        G_FILE_QUERY_INFO_NONE, NULL, NULL);
    if (info != NULL) content_type = g_file_info_get_content_type(info);
    if (content_type != NULL) mime = g_content_type_get_mime_type(content_type);
    if (mime != NULL && g_str_has_prefix(mime, "image/"))
    {
        gtk_picture_set_filename(state->picture, choice->path);
        gtk_stack_set_visible_child_name(state->preview, "image");
    }
    else if (mime != NULL && g_str_has_prefix(mime, "video/"))
    {
        GtkMediaStream *stream = gtk_media_file_new_for_filename(choice->path);
        gtk_video_set_media_stream(state->video, stream); g_object_unref(stream);
        gtk_stack_set_visible_child_name(state->preview, "video");
    }
    else gtk_stack_set_visible_child_name(state->preview, "unsupported");
    g_free(mime); g_clear_object(&info); g_clear_object(&file);
}
/** @brief Termine le dialogue en renvoyant la sélection. */
static void manage_evidence_on_save(GtkButton *button, gpointer data)
{
    ManageEvidenceState *state = data;
    GPtrArray *selected = g_ptr_array_new_with_free_func(g_free);
    (void) button;
    for (guint index = 0; index < state->choices->len; index++)
    {
        EvidenceChoice *choice = g_ptr_array_index(state->choices, index);
        if (gtk_check_button_get_active(choice->check))
            g_ptr_array_add(selected, g_strdup(choice->identifier));
    }
    state->completed = TRUE; state->callback(selected, state->callback_data);
    gtk_window_close(state->window);
}
/** @brief Termine le dialogue par une annulation. */
static gboolean manage_evidence_on_close(GtkWindow *window, gpointer data)
{
    ManageEvidenceState *state = data; (void) window;
    if (!state->completed) state->callback(NULL, state->callback_data);
    return FALSE;
}
gboolean manage_entity_evidence_dialog_present(GtkWindow *parent,
    const GPtrArray *records, const GPtrArray *selected_ids,
    const char *root_path, ManageEntityEvidenceDialogCallback callback,
    gpointer callback_data, GError **error)
{
    ManageEvidenceState *state = NULL; GtkWidget *root = NULL;
    GtkWidget *content = NULL; GtkWidget *list = NULL; GtkWidget *scroll = NULL;
    GtkWidget *save = NULL;
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    if (!GTK_IS_WINDOW(parent) || records == NULL || root_path == NULL ||
        callback == NULL) return FALSE;
    state = g_new0(ManageEvidenceState, 1);
    state->choices = g_ptr_array_new_with_free_func(evidence_choice_free);
    state->callback = callback; state->callback_data = callback_data;
    state->window = GTK_WINDOW(gtk_window_new());
    gtk_window_set_title(state->window, "Pièces jointes de la personne");
    gtk_window_set_transient_for(state->window, parent);
    gtk_window_set_modal(state->window, TRUE);
    gtk_window_set_default_size(state->window, 900, 620);
    root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(root, 16); gtk_widget_set_margin_bottom(root, 16);
    gtk_widget_set_margin_start(root, 16); gtk_widget_set_margin_end(root, 16);
    content = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    scroll = gtk_scrolled_window_new(); gtk_widget_set_size_request(scroll, 380, 480);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), list);
    for (guint index = 0; index < records->len; index++)
    {
        const EvidenceRecord *record = g_ptr_array_index(records, index);
        EvidenceChoice *choice = g_new0(EvidenceChoice, 1); gboolean active = FALSE;
        char *label = g_strdup_printf("%s — %s",
            evidence_record_get_original_name(record),
            evidence_record_get_type_identifier(record));
        char *candidate = g_build_filename(root_path,
            evidence_record_get_relative_path(record), NULL);
        char *canonical_root = g_canonicalize_filename(root_path, NULL);
        char *canonical_path = g_canonicalize_filename(candidate, NULL);
        choice->identifier = g_strdup(evidence_record_get_identifier(record));
        if (canonical_root != NULL && canonical_path != NULL &&
            g_str_has_prefix(canonical_path, canonical_root) &&
            (canonical_path[strlen(canonical_root)] == G_DIR_SEPARATOR ||
             canonical_path[strlen(canonical_root)] == '\0'))
            choice->path = g_strdup(canonical_path);
        g_free(canonical_path); g_free(canonical_root); g_free(candidate);
        choice->check = GTK_CHECK_BUTTON(gtk_check_button_new_with_label(label));
        g_free(label);
        for (guint selected = 0; selected_ids != NULL && selected < selected_ids->len; selected++)
            if (g_strcmp0(choice->identifier, g_ptr_array_index(selected_ids, selected)) == 0) active = TRUE;
        gtk_check_button_set_active(choice->check, active);
        g_object_set_data(G_OBJECT(choice->check), "choice", choice);
        g_signal_connect(choice->check, "toggled", G_CALLBACK(manage_evidence_on_toggled), state);
        gtk_box_append(GTK_BOX(list), GTK_WIDGET(choice->check));
        g_ptr_array_add(state->choices, choice);
    }
    state->preview = GTK_STACK(gtk_stack_new());
    state->picture = GTK_PICTURE(gtk_picture_new());
    gtk_picture_set_content_fit(state->picture, GTK_CONTENT_FIT_CONTAIN);
    state->video = GTK_VIDEO(gtk_video_new());
    gtk_stack_add_named(state->preview, gtk_label_new("Sélectionnez une preuve."), "unsupported");
    gtk_stack_add_named(state->preview, GTK_WIDGET(state->picture), "image");
    gtk_stack_add_named(state->preview, GTK_WIDGET(state->video), "video");
    gtk_widget_set_hexpand(GTK_WIDGET(state->preview), TRUE);
    gtk_box_append(GTK_BOX(content), scroll); gtk_box_append(GTK_BOX(content), GTK_WIDGET(state->preview));
    save = gtk_button_new_with_label("Enregistrer les pièces jointes");
    gtk_widget_add_css_class(save, "suggested-action"); gtk_widget_set_halign(save, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(root), content); gtk_box_append(GTK_BOX(root), save);
    gtk_window_set_child(state->window, root);
    g_object_set_data_full(G_OBJECT(state->window), "state", state, manage_evidence_state_free);
    g_signal_connect(save, "clicked", G_CALLBACK(manage_evidence_on_save), state);
    g_signal_connect(state->window, "close-request", G_CALLBACK(manage_evidence_on_close), state);
    gtk_window_present(state->window); return TRUE;
}
