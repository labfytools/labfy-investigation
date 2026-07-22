/******************************************************************************
 * @file rib_ocr_review_dialog.c
 * @brief Révision humaine des résultats OCR d'un RIB.
 ******************************************************************************/
#include "views/rib_ocr_review_dialog.h"
#include "core/iban_analyzer.h"
typedef struct { GtkWindow *window; GtkEntry *iban; GtkLabel *error;
    RibOcrReviewCallback callback; gpointer data; gboolean completed; } State;
/** @brief Libère l'état du dialogue. */
static void state_free(gpointer data) { g_free(data); }
/** @brief Valide et transmet l'IBAN corrigé. */
static void on_confirm(GtkButton *button, gpointer data)
{
    State *state = data; const char *text = gtk_editable_get_text(
        GTK_EDITABLE(state->iban)); char *normalized = NULL; (void) button;
    normalized = iban_analyzer_normalize(text);
    if (!iban_analyzer_validate(normalized))
    { gtk_label_set_text(state->error, "IBAN invalide : vérifiez les caractères OCR."); g_free(normalized); return; }
    state->completed = TRUE; state->callback(normalized, state->data);
    g_free(normalized); gtk_window_close(state->window);
}
/** @brief Signale l'annulation au contrôleur. */
static gboolean on_close(GtkWindow *window, gpointer data)
{
    State *state = data; (void) window;
    if (!state->completed) state->callback(NULL, state->data);
    return FALSE;
}
void rib_ocr_review_dialog_present(GtkWindow *parent, const char *ocr_text,
    const char *suggested, RibOcrReviewCallback callback, gpointer user_data)
{
    State *state = g_new0(State, 1); GtkWidget *root = gtk_box_new(
        GTK_ORIENTATION_VERTICAL, 10); GtkWidget *scroll = gtk_scrolled_window_new();
    GtkWidget *view = gtk_text_view_new(); GtkWidget *confirm = NULL;
    state->window = GTK_WINDOW(gtk_window_new()); state->callback = callback;
    state->data = user_data; state->iban = GTK_ENTRY(gtk_entry_new());
    state->error = GTK_LABEL(gtk_label_new(NULL));
    gtk_window_set_title(state->window, "Réviser l’analyse OCR du RIB");
    gtk_window_set_transient_for(state->window, parent); gtk_window_set_modal(state->window, TRUE);
    gtk_window_set_default_size(state->window, 760, 620);
    gtk_widget_set_margin_top(root, 16); gtk_widget_set_margin_bottom(root, 16);
    gtk_widget_set_margin_start(root, 16); gtk_widget_set_margin_end(root, 16);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(view)),
        ocr_text != NULL ? ocr_text : "", -1);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), view);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_editable_set_text(GTK_EDITABLE(state->iban), suggested != NULL ? suggested : "");
    confirm = gtk_button_new_with_label("Créer l’entité IBAN");
    gtk_widget_add_css_class(confirm, "suggested-action");
    gtk_box_append(GTK_BOX(root), gtk_label_new("Texte OCR brut"));
    gtk_box_append(GTK_BOX(root), scroll); gtk_box_append(GTK_BOX(root), gtk_label_new("IBAN détecté ou corrigé"));
    gtk_box_append(GTK_BOX(root), GTK_WIDGET(state->iban)); gtk_box_append(GTK_BOX(root), GTK_WIDGET(state->error));
    gtk_box_append(GTK_BOX(root), confirm); gtk_window_set_child(state->window, root);
    g_object_set_data_full(G_OBJECT(state->window), "state", state, state_free);
    g_signal_connect(confirm, "clicked", G_CALLBACK(on_confirm), state);
    g_signal_connect(state->window, "close-request", G_CALLBACK(on_close), state);
    gtk_window_present(state->window);
}
