/******************************************************************************
 * @file eml_analysis_dialog.c
 * @brief Présentation en lecture seule d'une analyse EML.
 ******************************************************************************/
#include "views/eml_analysis_dialog.h"
#include "views/dialog_geometry.h"
#include "widgets/controlled_vocab_dropdown.h"
typedef struct { GtkWindow *window; GtkWidget *proposals_box;
    GtkWidget *summary_label;
    EmlAnalysisDialogCallback callback; gpointer user_data; gboolean completed;
} EmlAnalysisDialogState;
static void eml_analysis_dialog_update_summary(GtkCheckButton *button,
    gpointer data)
{
    EmlAnalysisDialogState *state = data; guint kept = 0, promoted = 0;
    (void) button;
    for (GtkWidget *row = gtk_widget_get_first_child(state->proposals_box);
         row != NULL; row = gtk_widget_get_next_sibling(row))
    {
        GtkWidget *keep = g_object_get_data(G_OBJECT(row), "eml-keep");
        GtkWidget *promote = g_object_get_data(G_OBJECT(row), "eml-promote");
        gboolean active = keep != NULL &&
            gtk_check_button_get_active(GTK_CHECK_BUTTON(keep));
        if (promote != NULL)
        {
            gtk_widget_set_sensitive(promote, active);
            if (!active)
                gtk_check_button_set_active(GTK_CHECK_BUTTON(promote), FALSE);
        }
        if (active) kept++;
        if (promote != NULL &&
            gtk_check_button_get_active(GTK_CHECK_BUTTON(promote))) promoted++;
    }
    char *summary = g_strdup_printf(
        "%u observation(s) seront conservée(s) dans la fiche ; "
        "%u seront promue(s) en entité(s).", kept, promoted);
    gtk_label_set_text(GTK_LABEL(state->summary_label), summary);
    g_free(summary);
}
static void eml_analysis_dialog_bind_summary(EmlAnalysisDialogState *state,
    GtkWidget *content)
{
    state->summary_label = gtk_label_new(
        "0 observation sera conservée dans la fiche ; "
        "0 sera promue en entité.");
    gtk_label_set_xalign(GTK_LABEL(state->summary_label), 0.0f);
    gtk_box_append(GTK_BOX(content), state->summary_label);
    for (GtkWidget *row = gtk_widget_get_first_child(state->proposals_box);
         row != NULL; row = gtk_widget_get_next_sibling(row))
    {
        g_signal_connect(g_object_get_data(G_OBJECT(row), "eml-keep"),
            "toggled", G_CALLBACK(eml_analysis_dialog_update_summary), state);
        g_signal_connect(g_object_get_data(G_OBJECT(row), "eml-promote"),
            "toggled", G_CALLBACK(eml_analysis_dialog_update_summary), state);
    }
}
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
        GtkWidget *check = g_object_get_data(G_OBJECT(child), "eml-keep");
        if (check != NULL && GTK_IS_CHECK_BUTTON(check) &&
            gtk_check_button_get_active(GTK_CHECK_BUTTON(check)))
        {
            const char *type = g_object_get_data(G_OBJECT(child), "eml-type");
            const char *value = g_object_get_data(G_OBJECT(child), "eml-value");
            GtkWidget *status = g_object_get_data(G_OBJECT(child), "eml-status");
            GtkWidget *provenance = g_object_get_data(G_OBJECT(child),
                "eml-provenance");
            const char *raw = g_object_get_data(G_OBJECT(child), "eml-raw");
            const char *role = g_object_get_data(G_OBJECT(child), "eml-role");
            const char *header = g_object_get_data(G_OBJECT(child), "eml-header");
            guint occurrence = GPOINTER_TO_UINT(g_object_get_data(
                G_OBJECT(child), "eml-occurrence"));
            EmlEntityProposal *proposal = eml_entity_proposal_new_observation(
                type, raw != NULL ? raw : value, value,
                role != NULL ? role : "other",
                header != NULL ? header : "manual",
                occurrence > 0 ? occurrence : 1,
                controlled_vocab_dropdown_get_selected_code(status),
                controlled_vocab_dropdown_get_selected_code(provenance));
            GtkWidget *promote = g_object_get_data(G_OBJECT(child),
                "eml-promote");
            proposal->promote_to_entity = promote != NULL &&
                gtk_check_button_get_active(GTK_CHECK_BUTTON(promote));
            g_ptr_array_add(selected, proposal);
        }
    }
    if (selected->len == 0)
    { g_ptr_array_unref(selected); return; }
    state->completed = TRUE;
    if (state->callback != NULL) state->callback(selected, state->user_data);
    else g_ptr_array_unref(selected);
    gtk_window_close(state->window);
}
static void eml_analysis_dialog_add_observations(GtkWidget *box,
    const GPtrArray *observations)
{
    for (guint i = 0; observations != NULL && i < observations->len; i++)
    {
        const EmlObservation *observation = g_ptr_array_index(
            (GPtrArray *) observations, i);
        char *text = g_strdup_printf("%s — %s — rôle : %s — origine : %s #%u",
            observation->type_identifier, observation->value_normalized,
            observation->role, observation->source_header,
            observation->occurrence);
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        GtkWidget *check = gtk_check_button_new_with_label(
            "Conserver dans la fiche");
        GtkWidget *value_label = gtk_label_new(text);
        GtkWidget *promote = gtk_check_button_new_with_label(
            "Promouvoir en entité");
        gtk_widget_set_sensitive(promote, FALSE);
        GtkWidget *status = controlled_vocab_dropdown_new(
            CONTROLLED_VOCAB_VERIFICATION_STATUS, "proposed");
        GtkWidget *provenance = controlled_vocab_dropdown_new(
            CONTROLLED_VOCAB_PROVENANCE_KIND, observation->provenance_kind);
        g_object_set_data_full(G_OBJECT(row), "eml-type",
            g_strdup(observation->type_identifier), g_free);
        g_object_set_data_full(G_OBJECT(row), "eml-value",
            g_strdup(observation->value_normalized), g_free);
        g_object_set_data_full(G_OBJECT(row), "eml-raw",
            g_strdup(observation->value_raw), g_free);
        g_object_set_data_full(G_OBJECT(row), "eml-role",
            g_strdup(observation->role), g_free);
        g_object_set_data_full(G_OBJECT(row), "eml-header",
            g_strdup(observation->source_header), g_free);
        g_object_set_data(G_OBJECT(row), "eml-occurrence",
            GUINT_TO_POINTER(observation->occurrence));
        g_object_set_data(G_OBJECT(row), "eml-status", status);
        g_object_set_data(G_OBJECT(row), "eml-provenance", provenance);
        g_object_set_data(G_OBJECT(row), "eml-keep", check);
        g_object_set_data(G_OBJECT(row), "eml-promote", promote);
        gtk_widget_set_hexpand(check, TRUE);
        gtk_box_append(GTK_BOX(row), check);
        gtk_box_append(GTK_BOX(row), value_label);
        gtk_box_append(GTK_BOX(row), promote);
        gtk_box_append(GTK_BOX(row), status);
        gtk_box_append(GTK_BOX(row), provenance);
        gtk_box_append(GTK_BOX(box), row);
        g_free(text);
    }
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
        GtkWidget *check = gtk_check_button_new_with_label(
            "Conserver dans la fiche");
        GtkWidget *value_label = gtk_label_new(text);
        GtkWidget *promote = gtk_check_button_new_with_label(
            "Promouvoir en entité");
        gtk_widget_set_sensitive(promote, FALSE);
        GtkWidget *status = controlled_vocab_dropdown_new(
            CONTROLLED_VOCAB_VERIFICATION_STATUS, "proposed");
        GtkWidget *provenance = controlled_vocab_dropdown_new(
            CONTROLLED_VOCAB_PROVENANCE_KIND,
            g_strcmp0(type, "ip_address") == 0 ? "header" : "header");
        g_object_set_data_full(G_OBJECT(row), "eml-type", g_strdup(type), g_free);
        g_object_set_data_full(G_OBJECT(row), "eml-value", g_strdup(value), g_free);
        g_object_set_data(G_OBJECT(row), "eml-status", status);
        g_object_set_data(G_OBJECT(row), "eml-provenance", provenance);
        g_object_set_data(G_OBJECT(row), "eml-keep", check);
        g_object_set_data(G_OBJECT(row), "eml-promote", promote);
        gtk_widget_set_hexpand(check, TRUE);
        gtk_box_append(GTK_BOX(row), check);
        gtk_box_append(GTK_BOX(row), value_label);
        gtk_box_append(GTK_BOX(row), promote);
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
    labfy_dialog_prepare(window, parent, TRUE, TRUE);
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
        "Choisir les observations à conserver ; la promotion vers le graphe "
        "est une action séparée."));
    state->proposals_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    eml_analysis_dialog_add_observations(state->proposals_box,
        eml_analysis_get_observations(analysis));
    gtk_box_append(GTK_BOX(content), state->proposals_box);
    eml_analysis_dialog_bind_summary(state, content);
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
    labfy_dialog_present(window);
    g_free(received); g_free(emails); g_free(domains);
    g_free(sender_ips); g_free(destination_ips);
}

static void eml_analysis_dialog_append_attachment_summary(
    GString *text,
    const EmlPipelineResult *result)
{
    for (guint index = 0; result->mime_result != NULL &&
         result->mime_result->attachments != NULL &&
         index < result->mime_result->attachments->len; index++)
    {
        EmlAttachment *attachment = g_ptr_array_index(
            result->mime_result->attachments, index);
        g_string_append_printf(text,
            "%s → %s | MIME %s / %s | partie %s | %"
            G_GSIZE_FORMAT " octets | SHA-256 %s%s\n",
            attachment->declared_filename != NULL
                ? attachment->declared_filename : "(sans nom)",
            attachment->sanitized_filename != NULL
                ? attachment->sanitized_filename : "(sans nom)",
            attachment->content_type != NULL
                ? attachment->content_type : "inconnu",
            attachment->detected_mime != NULL
                ? attachment->detected_mime : "inconnu",
            attachment->part_index != NULL ? attachment->part_index : "?",
            attachment->decoded_size,
            attachment->sha256 != NULL ? attachment->sha256 : "indisponible",
            attachment->is_truncated ? " | TRONQUÉ" : "");
    }
}

static void eml_analysis_dialog_append_document_summary(
    GString *text,
    GString *metadata,
    GString *warnings,
    const EmlPipelineResult *result)
{
    for (guint index = 0; result->document_analyses != NULL &&
         index < result->document_analyses->len; index++)
    {
        DocumentFileAnalysis *document = g_ptr_array_index(
            result->document_analyses, index);
        if (document->ocr != NULL && document->ocr->text != NULL)
            g_string_append_printf(text, "\nOCR (%s)%s\n%s\n",
                document->ocr->requested_languages,
                document->ocr->execution->stdout_truncated
                    ? " — texte tronqué/partiel" : "",
                document->ocr->text);
        if (document->pdf != NULL)
            for (guint page_index = 0;
                 page_index < document->pdf->pages->len; page_index++)
            {
                PdfPageAnalysis *page = g_ptr_array_index(
                    document->pdf->pages, page_index);
                g_string_append_printf(text,
                    "\nPDF page %u — méthode %s — état %s\n%s\n",
                    page->page_number,
                    page->method == PDF_PAGE_METHOD_NATIVE
                        ? "texte natif" : "OCR",
                    document_analysis_state_code(page->state),
                    page->text != NULL ? page->text : "(aucun texte)");
            }
        if (document->metadata != NULL)
        {
            DocumentToolExecution *execution =
                document->metadata->execution;
            if (execution != NULL &&
                execution->state == DOCUMENT_ANALYSIS_STATE_UNAVAILABLE)
                g_string_append(warnings, "ExifTool indisponible.\n");
            for (guint metadata_index = 0;
                 metadata_index < document->metadata->metadata->len;
                 metadata_index++)
            {
                DocumentMetadataEntry *entry = g_ptr_array_index(
                    document->metadata->metadata, metadata_index);
                g_string_append_printf(metadata,
                    "%s | %s:%s | %s | %s%s\n",
                    entry->code, entry->original_group,
                    entry->original_tag, entry->raw_value,
                    execution != NULL && execution->version != NULL
                        ? execution->version : "version inconnue",
                    entry->sensitive
                        ? " | SENSIBLE — confirmation explicite requise" : "");
            }
        }
    }
}

void eml_analysis_dialog_present_pipeline(
    GtkWindow *parent,
    const EmlPipelineResult *result,
    const char *evidence_name,
    const char *relative_path,
    const char *source_sha256,
    EmlAnalysisDialogCallback callback,
    gpointer user_data)
{
    if (parent == NULL || result == NULL || result->analysis == NULL) return;
    EmlAnalysisDialogState *state = g_new0(EmlAnalysisDialogState, 1);
    state->callback = callback; state->user_data = user_data;
    state->window = GTK_WINDOW(gtk_window_new());
    gtk_window_set_title(state->window, "Révision de l’analyse EML");
    labfy_dialog_prepare(state->window, parent, TRUE, TRUE);
    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(outer, 14);
    gtk_widget_set_margin_end(outer, 14);
    gtk_widget_set_margin_top(outer, 14);
    gtk_widget_set_margin_bottom(outer, 14);
    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    eml_analysis_dialog_add_field(GTK_GRID(grid), 0, "Preuve", evidence_name);
    eml_analysis_dialog_add_field(GTK_GRID(grid), 1, "Chemin relatif", relative_path);
    eml_analysis_dialog_add_field(GTK_GRID(grid), 2, "SHA-256", source_sha256);
    eml_analysis_dialog_add_field(GTK_GRID(grid), 3, "État global",
        document_analysis_state_code(result->state));
    eml_analysis_dialog_add_field(GTK_GRID(grid), 4, "From",
        eml_analysis_get_first_header(result->analysis, "from"));
    eml_analysis_dialog_add_field(GTK_GRID(grid), 5, "Sender",
        eml_analysis_get_first_header(result->analysis, "sender"));
    eml_analysis_dialog_add_field(GTK_GRID(grid), 6, "Reply-To",
        eml_analysis_get_first_header(result->analysis, "reply-to"));
    eml_analysis_dialog_add_field(GTK_GRID(grid), 7, "Return-Path",
        eml_analysis_get_first_header(result->analysis, "return-path"));
    eml_analysis_dialog_add_field(GTK_GRID(grid), 8, "To",
        eml_analysis_get_first_header(result->analysis, "to"));
    eml_analysis_dialog_add_field(GTK_GRID(grid), 9, "Cc",
        eml_analysis_get_first_header(result->analysis, "cc"));
    eml_analysis_dialog_add_field(GTK_GRID(grid), 10, "Subject",
        eml_analysis_get_first_header(result->analysis, "subject"));
    eml_analysis_dialog_add_field(GTK_GRID(grid), 11, "Date brute",
        eml_analysis_get_first_header(result->analysis, "date"));
    eml_analysis_dialog_add_field(GTK_GRID(grid), 12, "Message-ID",
        eml_analysis_get_first_header(result->analysis, "message-id"));
    char *received = eml_analysis_dialog_join(
        eml_analysis_get_header_values(result->analysis, "received"));
    eml_analysis_dialog_add_field(GTK_GRID(grid), 13,
        "Received (ordre original)", received);
    gtk_box_append(GTK_BOX(content), grid);

    GString *attachments = g_string_new(NULL);
    GString *texts = g_string_new(NULL);
    GString *metadata = g_string_new(NULL);
    GString *warnings = g_string_new(NULL);
    eml_analysis_dialog_append_attachment_summary(attachments, result);
    eml_analysis_dialog_append_document_summary(
        texts, metadata, warnings, result);
    for (guint index = 0; result->warnings != NULL &&
         index < result->warnings->len; index++)
        g_string_append_printf(warnings, "%s\n",
            (char *) g_ptr_array_index(result->warnings, index));
    for (guint index = 0; result->bank_proposals != NULL &&
         index < result->bank_proposals->len; index++)
    {
        BankProposal *proposal = g_ptr_array_index(
            result->bank_proposals, index);
        if (!proposal->is_iban_valid ||
            proposal->suggested_ocr_fix != NULL)
            g_string_append_printf(warnings,
                "Proposition bancaire invalide ou corrigée, "
                "désélectionnée : %s\n",
                proposal->raw_iban != NULL ? proposal->raw_iban : "(vide)");
    }
    GtkWidget *details = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(details), 6);
    gtk_grid_set_column_spacing(GTK_GRID(details), 12);
    eml_analysis_dialog_add_field(GTK_GRID(details), 0,
        "Pièces jointes", attachments->str);
    eml_analysis_dialog_add_field(GTK_GRID(details), 1,
        "Texte PDF et OCR", texts->str);
    eml_analysis_dialog_add_field(GTK_GRID(details), 2,
        "Métadonnées", metadata->str);
    eml_analysis_dialog_add_field(GTK_GRID(details), 3,
        "Avertissements", warnings->str);
    gtk_box_append(GTK_BOX(content), details);

    state->proposals_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    eml_analysis_dialog_add_observations(state->proposals_box,
        eml_analysis_get_observations(result->analysis));
    GPtrArray *valid_ibans = g_ptr_array_new();
    for (guint index = 0; result->bank_proposals != NULL &&
         index < result->bank_proposals->len; index++)
    {
        BankProposal *proposal = g_ptr_array_index(
            result->bank_proposals, index);
        if (proposal->is_iban_valid && proposal->normalized_iban != NULL &&
            proposal->suggested_ocr_fix == NULL)
            g_ptr_array_add(valid_ibans, proposal->normalized_iban);
    }
    eml_analysis_dialog_add_proposals(state->proposals_box, "iban",
        "IBAN valide", valid_ibans);
    g_ptr_array_unref(valid_ibans);
    gtk_box_append(GTK_BOX(content), gtk_label_new(
        "Propositions à intégrer explicitement"));
    gtk_box_append(GTK_BOX(content), state->proposals_box);
    eml_analysis_dialog_bind_summary(state, content);

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), content);
    gtk_box_append(GTK_BOX(outer), scroll);
    GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(actions, GTK_ALIGN_END);
    GtkWidget *close = gtk_button_new_with_label("Rejeter et fermer");
    GtkWidget *integrate = gtk_button_new_with_label(
        "Intégrer les éléments sélectionnés");
    gtk_widget_add_css_class(integrate, "suggested-action");
    g_signal_connect(close, "clicked",
        G_CALLBACK(eml_analysis_dialog_on_close), state);
    g_signal_connect(integrate, "clicked",
        G_CALLBACK(eml_analysis_dialog_on_integrate), state);
    gtk_box_append(GTK_BOX(actions), close);
    gtk_box_append(GTK_BOX(actions), integrate);
    gtk_box_append(GTK_BOX(outer), actions);
    gtk_window_set_child(state->window, outer);
    g_signal_connect(state->window, "close-request",
        G_CALLBACK(eml_analysis_dialog_on_window_close), state);
    g_object_set_data_full(G_OBJECT(state->window), "eml-dialog-state",
        state, eml_analysis_dialog_state_free);
    labfy_dialog_present(state->window);
    g_free(received);
    g_string_free(attachments, TRUE);
    g_string_free(texts, TRUE);
    g_string_free(metadata, TRUE);
    g_string_free(warnings, TRUE);
}
