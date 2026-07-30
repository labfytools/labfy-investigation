/******************************************************************************
 * @file manage_entity_evidence_dialog.c
 * @brief Dialogue de sélection des preuves associées à une entité.
 ******************************************************************************/
#include "views/manage_entity_evidence_dialog.h"
#include "views/dialog_geometry.h"
#include "widgets/evidence_preview_widget.h"
#include <gio/gio.h>
typedef struct
{
    char *identifier;
    char *path;
    char *relative_path;
    char *sha256;
    char *mime_type;
    GtkCheckButton *check;
} EvidenceChoice;
typedef struct
{
    GtkWindow *window;
    EvidencePreviewWidget *preview;
    char *root_path;
    GPtrArray *choices;
    ManageEntityEvidenceDialogCallback callback;
    ManageEntityEvidenceImportCallback import_callback;
    gpointer callback_data;
    gboolean completed;
} ManageEvidenceState;
/** @brief Ferme le gestionnaire puis lance un import pour cette personne. */
static void manage_evidence_on_import(GtkButton *button, gpointer data)
{
    ManageEvidenceState *state = data;
    (void) button;
    if (state == NULL || state->completed) return;
    state->completed = TRUE;
    if (state->import_callback != NULL)
        state->import_callback(state->callback_data);
    gtk_window_close(state->window);
}
/** @brief Libère une option privée. */
static void evidence_choice_free(gpointer data)
{
    EvidenceChoice *choice = data;
    if (choice == NULL) return;
    g_free(choice->identifier); g_free(choice->path);
    g_free(choice->relative_path); g_free(choice->sha256);
    g_free(choice->mime_type); g_free(choice);
}
/** @brief Arrête et efface l'aperçu courant. */
static void manage_evidence_clear_preview(ManageEvidenceState *state)
{
    if (state == NULL || state->preview == NULL) return;
    evidence_preview_widget_clear(state->preview);
}
/** @brief Libère l'état du dialogue. */
static void manage_evidence_state_free(gpointer data)
{
    ManageEvidenceState *state = data;
    if (state == NULL) return;
    manage_evidence_clear_preview(state);
    g_clear_pointer(&state->preview, evidence_preview_widget_free);
    g_ptr_array_unref(state->choices); g_free(state->root_path); g_free(state);
}
/** @brief Affiche l'aperçu de l'option manipulée. */
static void manage_evidence_on_toggled(GtkCheckButton *button, gpointer data)
{
    ManageEvidenceState *state = data;
    EvidenceChoice *choice = g_object_get_data(G_OBJECT(button), "choice");
    if (state == NULL || choice == NULL || choice->path == NULL) return;
    EvidencePreviewRequest *request = evidence_preview_request_new(
        state->root_path, choice->identifier, choice->relative_path,
        choice->sha256, choice->mime_type, 1U);
    evidence_preview_widget_show(state->preview, request,
        gtk_check_button_get_label(choice->check));
    evidence_preview_request_free(request);
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
    const char *root_path, TaskManager *task_manager,
    ManageEntityEvidenceDialogCallback callback,
    gpointer callback_data, ManageEntityEvidenceImportCallback import_callback,
    GError **error)
{
    ManageEvidenceState *state = NULL; GtkWidget *root = NULL;
    GtkWidget *content = NULL; GtkWidget *list = NULL; GtkWidget *scroll = NULL;
    GtkWidget *save = NULL; GtkWidget *import = NULL; GtkWidget *actions = NULL;
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    if (!GTK_IS_WINDOW(parent) || records == NULL || root_path == NULL ||
        task_manager == NULL ||
        callback == NULL) return FALSE;
    state = g_new0(ManageEvidenceState, 1);
    state->choices = g_ptr_array_new_with_free_func(evidence_choice_free);
    state->callback = callback; state->callback_data = callback_data;
    state->root_path = g_strdup(root_path);
    state->import_callback = import_callback;
    state->window = GTK_WINDOW(gtk_window_new());
    gtk_window_set_title(state->window, "Pièces jointes de la personne");
    labfy_dialog_prepare(state->window, parent, TRUE, TRUE);
    root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(root, 16); gtk_widget_set_margin_bottom(root, 16);
    gtk_widget_set_margin_start(root, 16); gtk_widget_set_margin_end(root, 16);
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
        choice->relative_path = g_strdup(
            evidence_record_get_relative_path(record));
        choice->sha256 = g_strdup(evidence_record_get_sha256(record));
        {
            gboolean uncertain = FALSE;
            char *content_type = g_content_type_guess(
                choice->path != NULL ? choice->path : candidate,
                NULL, 0U, &uncertain);
            choice->mime_type = content_type != NULL
                ? g_content_type_get_mime_type(content_type)
                : g_strdup("application/octet-stream");
            g_free(content_type);
            (void) uncertain;
        }
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
    state->preview = evidence_preview_widget_new(task_manager, NULL, NULL);
    if (state->preview == NULL) {
        gtk_window_destroy(state->window);
        manage_evidence_state_free(state);
        return FALSE;
    }
    content = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_name(content, "manage-evidence-paned");
    gtk_paned_set_start_child(GTK_PANED(content), scroll);
    gtk_paned_set_end_child(GTK_PANED(content),
        evidence_preview_widget_get_widget(state->preview));
    labfy_paned_apply_initial_ratio(
        GTK_PANED(content), 2.0 / 3.0, 480, 240);
    gtk_paned_set_resize_start_child(GTK_PANED(content), FALSE);
    gtk_paned_set_resize_end_child(GTK_PANED(content), TRUE);
    save = gtk_button_new_with_label("Enregistrer les pièces jointes");
    import = gtk_button_new_with_label("Importer une nouvelle preuve");
    actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(save, "suggested-action"); gtk_widget_set_halign(save, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(actions), import);
    gtk_box_append(GTK_BOX(actions), save);
    gtk_widget_set_halign(actions, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(root), content); gtk_box_append(GTK_BOX(root), actions);
    gtk_window_set_child(state->window, root);
    g_object_set_data_full(G_OBJECT(state->window), "state", state, manage_evidence_state_free);
    g_signal_connect(save, "clicked", G_CALLBACK(manage_evidence_on_save), state);
    g_signal_connect(import, "clicked", G_CALLBACK(manage_evidence_on_import), state);
    g_signal_connect(state->window, "close-request", G_CALLBACK(manage_evidence_on_close), state);
    labfy_dialog_present(state->window); return TRUE;
}
