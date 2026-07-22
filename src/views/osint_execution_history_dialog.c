/******************************************************************************
 * @file osint_execution_history_dialog.c
 * @brief Consultation en lecture seule de l'historique OSINT.
 ******************************************************************************/

#include "views/osint_execution_history_dialog.h"

#include "models/osint_execution_record.h"

typedef struct
{
    GtkWindow *window;
    GtkTextBuffer *details_buffer;
    GPtrArray *records;
    GHashTable *linked_objects;
} OsintExecutionHistoryDialogContext;

/** @brief Libère les données conservées par la fenêtre. */
static void osint_execution_history_dialog_context_free(gpointer data)
{
    OsintExecutionHistoryDialogContext *context = data;
    if (context == NULL) return;
    g_clear_pointer(&context->records, g_ptr_array_unref);
    g_clear_pointer(&context->linked_objects, g_hash_table_unref);
    g_free(context);
}

/** @brief Convertit une sortie brute en texte UTF-8 affichable. */
static char *osint_execution_history_dialog_output_to_text(GBytes *bytes)
{
    gconstpointer data = NULL;
    gsize size = 0U;
    if (bytes == NULL) return g_strdup("");
    data = g_bytes_get_data(bytes, &size);
    return data != NULL && size > 0U
        ? g_utf8_make_valid(data, (gssize) size) : g_strdup("");
}

/** @brief Construit le détail textuel complet d'une exécution. */
static char *osint_execution_history_dialog_build_details(
    const OsintExecutionRecord *record, GPtrArray *linked_objects
)
{
    GBytes *stdout_raw = osint_execution_record_ref_stdout(record);
    GBytes *stderr_raw = osint_execution_record_ref_stderr(record);
    char *stdout_text = osint_execution_history_dialog_output_to_text(stdout_raw);
    char *stderr_text = osint_execution_history_dialog_output_to_text(stderr_raw);
    GString *details = g_string_new(NULL);
    g_string_append_printf(details,
        "Exécution : %s\nOutil : %s%s%s\nAction : %s\nÉtat : %s\n"
        "Sélection : %s %s\nCible : %s\nDébut : %s\nFin : %s\n"
        "Code de sortie : ",
        osint_execution_record_get_identifier(record),
        osint_execution_record_get_tool_identifier(record),
        osint_execution_record_get_tool_version(record) != NULL ? " — " : "",
        osint_execution_record_get_tool_version(record) != NULL
            ? osint_execution_record_get_tool_version(record) : "",
        osint_execution_record_get_action_identifier(record),
        osint_execution_record_get_final_state(record),
        osint_execution_record_get_selection_kind(record),
        osint_execution_record_get_selection_identifier(record),
        osint_execution_record_get_target_value(record),
        osint_execution_record_get_started_at(record),
        osint_execution_record_get_finished_at(record));
    if (osint_execution_record_has_exit_code(record))
        g_string_append_printf(details, "%d", osint_execution_record_get_exit_code(record));
    else g_string_append(details, "indisponible");
    g_string_append_printf(details,
        "\nArguments : %s\nSHA-256 : %s\n\nObjets liés :\n",
        osint_execution_record_get_arguments(record),
        osint_execution_record_get_output_sha256(record));
    if (linked_objects == NULL || linked_objects->len == 0U)
        g_string_append(details, "Aucun objet intégré.\n");
    else for (guint index = 0U; index < linked_objects->len; index++)
        g_string_append_printf(details, "- %s\n",
            (const char *) g_ptr_array_index(linked_objects, index));
    g_string_append_printf(details, "\nSortie standard :\n%s\n\nSortie d'erreur :\n%s",
        stdout_text[0] != '\0' ? stdout_text : "(vide)",
        stderr_text[0] != '\0' ? stderr_text : "(vide)");
    g_bytes_unref(stdout_raw); g_bytes_unref(stderr_raw);
    g_free(stdout_text); g_free(stderr_text);
    return g_string_free(details, FALSE);
}

/** @brief Affiche le détail correspondant au bouton d'historique activé. */
static void osint_execution_history_dialog_on_record_clicked(
    GtkButton *button, gpointer user_data
)
{
    OsintExecutionHistoryDialogContext *context = user_data;
    guint encoded_index = GPOINTER_TO_UINT(g_object_get_data(
        G_OBJECT(button), "osint-execution-index"));
    OsintExecutionRecord *record = NULL;
    GPtrArray *linked_objects = NULL;
    char *details = NULL;
    if (context == NULL || encoded_index == 0U ||
        encoded_index > context->records->len) return;
    record = g_ptr_array_index(context->records, encoded_index - 1U);
    linked_objects = g_hash_table_lookup(context->linked_objects,
        osint_execution_record_get_identifier(record));
    details = osint_execution_history_dialog_build_details(record, linked_objects);
    gtk_text_buffer_set_text(context->details_buffer, details, -1);
    g_free(details);
}

void osint_execution_history_dialog_present(
    GtkWindow *parent_window, GPtrArray *records, GHashTable *linked_objects
)
{
    OsintExecutionHistoryDialogContext *context = NULL;
    GtkWidget *main_box = NULL; GtkWidget *content_box = NULL;
    GtkWidget *history_scroll = NULL; GtkWidget *history_box = NULL;
    GtkWidget *details_scroll = NULL; GtkWidget *details_view = NULL;
    GtkWidget *close_button = NULL;
    if (records == NULL || records->len == 0U || linked_objects == NULL) return;
    context = g_new0(OsintExecutionHistoryDialogContext, 1);
    context->window = GTK_WINDOW(gtk_window_new());
    context->records = g_ptr_array_ref(records);
    context->linked_objects = g_hash_table_ref(linked_objects);
    gtk_window_set_title(context->window, "Historique OSINT");
    gtk_window_set_default_size(context->window, 980, 650);
    gtk_window_set_modal(context->window, TRUE);
    gtk_window_set_destroy_with_parent(context->window, TRUE);
    if (parent_window != NULL) gtk_window_set_transient_for(context->window, parent_window);
    g_object_set_data_full(G_OBJECT(context->window), "osint-history-context",
        context, osint_execution_history_dialog_context_free);
    main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(main_box, 16); gtk_widget_set_margin_end(main_box, 16);
    gtk_widget_set_margin_top(main_box, 16); gtk_widget_set_margin_bottom(main_box, 16);
    content_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_vexpand(content_box, TRUE);
    history_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    history_scroll = gtk_scrolled_window_new();
    gtk_widget_set_size_request(history_scroll, 300, -1);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(history_scroll), history_box);
    for (guint index = 0U; index < records->len; index++)
    {
        OsintExecutionRecord *record = g_ptr_array_index(records, index);
        char *label = g_strdup_printf("%s\n%s — %s",
            osint_execution_record_get_finished_at(record),
            osint_execution_record_get_tool_identifier(record),
            osint_execution_record_get_final_state(record));
        GtkWidget *button = gtk_button_new_with_label(label);
        gtk_widget_set_halign(button, GTK_ALIGN_FILL);
        g_object_set_data(G_OBJECT(button), "osint-execution-index",
            GUINT_TO_POINTER(index + 1U));
        g_signal_connect(button, "clicked",
            G_CALLBACK(osint_execution_history_dialog_on_record_clicked), context);
        gtk_box_append(GTK_BOX(history_box), button);
        g_free(label);
    }
    details_view = gtk_text_view_new();
    context->details_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(details_view));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(details_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(details_view), TRUE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(details_view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(details_view), GTK_WRAP_WORD_CHAR);
    details_scroll = gtk_scrolled_window_new();
    gtk_widget_set_hexpand(details_scroll, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(details_scroll), details_view);
    gtk_box_append(GTK_BOX(content_box), history_scroll);
    gtk_box_append(GTK_BOX(content_box), details_scroll);
    close_button = gtk_button_new_with_label("Fermer");
    gtk_widget_set_halign(close_button, GTK_ALIGN_END);
    g_signal_connect_swapped(close_button, "clicked",
        G_CALLBACK(gtk_window_destroy), context->window);
    gtk_box_append(GTK_BOX(main_box), content_box);
    gtk_box_append(GTK_BOX(main_box), close_button);
    gtk_window_set_child(context->window, main_box);
    osint_execution_history_dialog_on_record_clicked(
        GTK_BUTTON(gtk_widget_get_first_child(history_box)), context);
    gtk_window_present(context->window);
}
