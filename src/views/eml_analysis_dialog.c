/******************************************************************************
 * @file eml_analysis_dialog.c
 * @brief Présentation en lecture seule d'une analyse EML.
 ******************************************************************************/
#include "views/eml_analysis_dialog.h"
#include "widgets/controlled_vocab_dropdown.h"
typedef struct { GtkWindow *window; GtkWidget *proposals_box;
    EmlAnalysisDialogCallback callback; gpointer user_data; gboolean completed;
} EmlAnalysisDialogState;
/** @brief Libère l'état de révision. */
static void eml_analysis_dialog_state_free(gpointer data) { g_free(data); }
/** @brief Signale une annulation une seule fois. */
static void eml_analysis_dialog_cancel(EmlAnalysisDialogState *state)
{ if (state == NULL || state->completed) return; state->completed = TRUE;
  if (state->callback != NULL) state->callback(NULL, state->user_data); }
/** @brief Traite la fermeture native. */
static gboolean eml_analysis_dialog_on_window_close(GtkWindow *window, gpointer data)
{ (void) window; eml_analysis_dialog_cancel(data); return FALSE; }
/** @brief Ferme la fenêtre de résultat. */
static void eml_analysis_dialog_on_close(GtkButton *button, gpointer data)
{ EmlAnalysisDialogState *state = data; (void) button;
  eml_analysis_dialog_cancel(state); gtk_window_close(state->window); }
/** @brief Transmet uniquement les propositions explicitement cochées. */
static void eml_analysis_dialog_on_integrate(GtkButton *button, gpointer data)
{
    EmlAnalysisDialogState *state = data;
    GPtrArray *selected = g_ptr_array_new_with_free_func(
        (GDestroyNotify) eml_entity_proposal_free);
    (void) button;
    for (GtkWidget *child = gtk_widget_get_first_child(state->proposals_box);
         child != NULL; child = gtk_widget_get_next_sibling(child))
    {
        GtkWidget *check = GTK_IS_BOX(child) ? gtk_widget_get_first_child(child) : NULL;
        if (check != NULL && GTK_IS_CHECK_BUTTON(check) &&
            gtk_check_button_get_active(GTK_CHECK_BUTTON(check)))
        {
            const char *type = g_object_get_data(G_OBJECT(child), "eml-type");
            const char *value = g_object_get_data(G_OBJECT(child), "eml-value");
            GtkWidget *status = g_object_get_data(G_OBJECT(child), "eml-status");
            GtkWidget *provenance = g_object_get_data(G_OBJECT(child),
                "eml-provenance");
            g_ptr_array_add(selected, eml_entity_proposal_new_with_metadata(
                type, value,
                controlled_vocab_dropdown_get_selected_code(status),
                controlled_vocab_dropdown_get_selected_code(provenance)));
        }
    }
    if (selected->len == 0)
    { g_ptr_array_unref(selected); return; }
    state->completed = TRUE;
    if (state->callback != NULL) state->callback(selected, state->user_data);
    else g_ptr_array_unref(selected);
    gtk_window_close(state->window);
}
/** @brief Ajoute les propositions d'un type sous forme de cases décochées. */
static void eml_analysis_dialog_add_proposals(GtkWidget *box,
    const char *type, const char *label, const GPtrArray *values)
{
    for (guint i = 0; values != NULL && i < values->len; i++)
    {
        const char *value = g_ptr_array_index((GPtrArray *) values, i);
        char *text = g_strdup_printf("%s : %s", label, value);
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        GtkWidget *check = gtk_check_button_new_with_label(text);
        GtkWidget *status = controlled_vocab_dropdown_new(
            CONTROLLED_VOCAB_VERIFICATION_STATUS, "proposed");
        GtkWidget *provenance = controlled_vocab_dropdown_new(
            CONTROLLED_VOCAB_PROVENANCE_KIND,
            g_strcmp0(type, "ip_address") == 0 ? "header" : "header");
        g_object_set_data_full(G_OBJECT(row), "eml-type", g_strdup(type), g_free);
        g_object_set_data_full(G_OBJECT(row), "eml-value", g_strdup(value), g_free);
        g_object_set_data(G_OBJECT(row), "eml-status", status);
        g_object_set_data(G_OBJECT(row), "eml-provenance", provenance);
        gtk_widget_set_hexpand(check, TRUE);
        gtk_box_append(GTK_BOX(row), check);
        gtk_box_append(GTK_BOX(row), status);
        gtk_box_append(GTK_BOX(row), provenance);
        gtk_box_append(GTK_BOX(box), row); g_free(text);
    }
}
/** @brief Ajoute une ligne de métadonnée sélectionnable. */
static void eml_analysis_dialog_add_field(GtkGrid *grid, int row,
    const char *title, const char *value)
{
    GtkWidget *name = gtk_label_new(title);
    GtkWidget *content = gtk_label_new(value != NULL ? value : "Non présent");
    gtk_label_set_xalign(GTK_LABEL(name), 0.0f);
    gtk_label_set_xalign(GTK_LABEL(content), 0.0f);
    gtk_label_set_selectable(GTK_LABEL(content), TRUE);
    gtk_label_set_wrap(GTK_LABEL(content), TRUE);
    gtk_widget_set_valign(name, GTK_ALIGN_START);
    gtk_grid_attach(grid, name, 0, row, 1, 1);
    gtk_grid_attach(grid, content, 1, row, 1, 1);
}
/** @brief Concatène un tableau de chaînes pour son affichage. */
static char *eml_analysis_dialog_join(const GPtrArray *values)
{
    GString *text = g_string_new(NULL);
    for (guint i = 0; values != NULL && i < values->len; i++)
        g_string_append_printf(text, "%s%s", i > 0 ? "\n" : "",
            (const char *) g_ptr_array_index((GPtrArray *) values, i));
    return g_string_free(text, FALSE);
}
void eml_analysis_dialog_present(GtkWindow *parent,
    const EmlProcessingResult *result, EmlAnalysisDialogCallback callback,
    gpointer user_data)
{
    const EmlAnalysis *analysis = eml_processing_result_get_analysis(result);
    GtkWindow *window = NULL;
    GtkWidget *box = NULL, *content = NULL, *scroll = NULL;
    GtkWidget *grid = NULL, *raw_scroll = NULL, *raw = NULL;
    GtkWidget *close = NULL, *integrate = NULL, *actions = NULL;
    EmlAnalysisDialogState *state = NULL;
    char *received = NULL, *emails = NULL, *domains = NULL;
    char *sender_ips = NULL, *destination_ips = NULL;
    if (parent == NULL || analysis == NULL) return;
    state = g_new0(EmlAnalysisDialogState, 1);
    state->callback = callback; state->user_data = user_data;
    window = GTK_WINDOW(gtk_window_new());
    gtk_window_set_title(window, "Analyse locale du courriel EML");
    gtk_window_set_transient_for(window, parent); gtk_window_set_modal(window, TRUE);
    gtk_window_set_default_size(window, 720, 560);
    state->window = window;
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(box, 14); gtk_widget_set_margin_end(box, 14);
    gtk_widget_set_margin_top(box, 14); gtk_widget_set_margin_bottom(box, 14);
    content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_append(GTK_BOX(content), gtk_label_new(
        "Analyse effectuée sur la copie vérifiée de 02_Preuves_Traitees/Extractions."));
    grid = gtk_grid_new(); gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    eml_analysis_dialog_add_field(GTK_GRID(grid), 0, "Copie analysée",
        eml_processing_result_get_copy_path(result));
    eml_analysis_dialog_add_field(GTK_GRID(grid), 1, "From",
        eml_analysis_get_first_header(analysis, "from"));
    eml_analysis_dialog_add_field(GTK_GRID(grid), 2, "Reply-To",
        eml_analysis_get_first_header(analysis, "reply-to"));
    eml_analysis_dialog_add_field(GTK_GRID(grid), 3, "To",
        eml_analysis_get_first_header(analysis, "to"));
    eml_analysis_dialog_add_field(GTK_GRID(grid), 4, "Sujet",
        eml_analysis_get_first_header(analysis, "subject"));
    eml_analysis_dialog_add_field(GTK_GRID(grid), 5, "Date",
        eml_analysis_get_first_header(analysis, "date"));
    eml_analysis_dialog_add_field(GTK_GRID(grid), 6, "Message-ID",
        eml_analysis_get_first_header(analysis, "message-id"));
    eml_analysis_dialog_add_field(GTK_GRID(grid), 7, "Authentification",
        eml_analysis_get_first_header(analysis, "authentication-results"));
    received = eml_analysis_dialog_join(
        eml_analysis_get_header_values(analysis, "received"));
    emails = eml_analysis_dialog_join(eml_analysis_get_email_addresses(analysis));
    domains = eml_analysis_dialog_join(eml_analysis_get_domains(analysis));
    sender_ips = eml_analysis_dialog_join(
        eml_analysis_get_sender_ip_addresses(analysis));
    destination_ips = eml_analysis_dialog_join(
        eml_analysis_get_destination_ip_addresses(analysis));
    eml_analysis_dialog_add_field(GTK_GRID(grid), 8, "Received (ordre original)", received);
    eml_analysis_dialog_add_field(GTK_GRID(grid), 9, "Emails proposés", emails);
    eml_analysis_dialog_add_field(GTK_GRID(grid), 10, "Domaines proposés", domains);
    eml_analysis_dialog_add_field(GTK_GRID(grid), 11, "IP expéditeur",
        sender_ips);
    eml_analysis_dialog_add_field(GTK_GRID(grid), 12, "IP destinataire",
        destination_ips);
    gtk_box_append(GTK_BOX(content), grid);
    gtk_box_append(GTK_BOX(content), gtk_label_new(
        "Sélection explicite des entités à intégrer"));
    state->proposals_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    eml_analysis_dialog_add_proposals(state->proposals_box, "email_address",
        "Email", eml_analysis_get_email_addresses(analysis));
    eml_analysis_dialog_add_proposals(state->proposals_box, "domain_name",
        "Domaine", eml_analysis_get_domains(analysis));
    eml_analysis_dialog_add_proposals(state->proposals_box, "ip_address",
        "IP expéditeur", eml_analysis_get_sender_ip_addresses(analysis));
    eml_analysis_dialog_add_proposals(state->proposals_box, "ip_address",
        "IP destinataire", eml_analysis_get_destination_ip_addresses(analysis));
    gtk_box_append(GTK_BOX(content), state->proposals_box);
    gtk_box_append(GTK_BOX(content), gtk_label_new("En-têtes bruts (lecture seule)"));
    raw = gtk_text_view_new(); gtk_text_view_set_editable(GTK_TEXT_VIEW(raw), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(raw), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(raw), GTK_WRAP_WORD_CHAR);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(raw)),
        eml_analysis_get_raw_headers(analysis), -1);
    raw_scroll = gtk_scrolled_window_new();
    gtk_widget_set_size_request(raw_scroll, -1, 180);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(raw_scroll), raw);
    gtk_box_append(GTK_BOX(content), raw_scroll);
    scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), content);
    gtk_box_append(GTK_BOX(box), scroll);
    actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(actions, GTK_ALIGN_END);
    close = gtk_button_new_with_label("Fermer");
    integrate = gtk_button_new_with_label("Intégrer la sélection");
    gtk_widget_add_css_class(integrate, "suggested-action");
    g_signal_connect(close, "clicked", G_CALLBACK(eml_analysis_dialog_on_close), state);
    g_signal_connect(integrate, "clicked", G_CALLBACK(eml_analysis_dialog_on_integrate), state);
    gtk_box_append(GTK_BOX(actions), close); gtk_box_append(GTK_BOX(actions), integrate);
    gtk_box_append(GTK_BOX(box), actions); gtk_window_set_child(window, box);
    g_signal_connect(window, "close-request",
        G_CALLBACK(eml_analysis_dialog_on_window_close), state);
    g_object_set_data_full(G_OBJECT(window), "eml-dialog-state", state,
        eml_analysis_dialog_state_free);
    gtk_window_present(window);
    g_free(received); g_free(emails); g_free(domains);
    g_free(sender_ips); g_free(destination_ips);
}
