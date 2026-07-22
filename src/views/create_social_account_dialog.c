/******************************************************************************
 * @file create_social_account_dialog.c
 * @brief Dialogue GTK de création d'un compte social observé.
 ******************************************************************************/

#include "views/create_social_account_dialog.h"

struct CreateSocialAccountDialogResult
{
    char *platform;
    char *profile_url;
    char *username;
    char *platform_identifier;
    char *first_observed_at;
    char *account_state;
    char *notes;
    char *evidence_identifier;
};

typedef struct
{
    GtkWindow *window;
    GtkDropDown *platform;
    GtkEntry *url;
    GtkEntry *username;
    GtkEntry *platform_identifier;
    GtkEntry *observed_at;
    GtkDropDown *state;
    GtkTextView *notes;
    GtkDropDown *evidence;
    GtkLabel *error;
    GPtrArray *evidence_identifiers;
    CreateSocialAccountDialogCallback callback;
    gpointer user_data;
    gboolean completed;
} CreateSocialAccountDialogState;

static const char *const platform_codes[] = {
    "tiktok", "instagram", "facebook", "x", "telegram", "other"
};
static const char *const state_codes[] = {
    "unknown", "active", "private", "suspended", "deleted"
};

/** @brief Retourne une copie nettoyée, ou NULL pour un texte vide. */
static char *create_social_account_dialog_copy(const char *text)
{
    char *copy = text != NULL ? g_strdup(text) : NULL;
    if (copy == NULL) return NULL;
    g_strstrip(copy);
    if (copy[0] == '\0') { g_free(copy); return NULL; }
    return copy;
}

/** @brief Extrait le texte du champ de notes. */
static char *create_social_account_dialog_notes(CreateSocialAccountDialogState *state)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(state->notes);
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    return gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
}

/** @brief Libère l'état privé attaché à la fenêtre. */
static void create_social_account_dialog_state_free(gpointer user_data)
{
    CreateSocialAccountDialogState *state = user_data;
    if (state == NULL) return;
    g_clear_pointer(&state->evidence_identifiers, g_ptr_array_unref);
    g_free(state);
}

/** @brief Signale une annulation au callback une seule fois. */
static void create_social_account_dialog_cancel(CreateSocialAccountDialogState *state)
{
    if (state == NULL || state->completed) return;
    state->completed = TRUE;
    if (state->callback != NULL) state->callback(NULL, state->user_data);
}

/** @brief Traite la fermeture native du dialogue. */
static gboolean create_social_account_dialog_on_close(GtkWindow *window, gpointer user_data)
{
    (void) window;
    create_social_account_dialog_cancel(user_data);
    return FALSE;
}

/** @brief Traite le bouton d'annulation. */
static void create_social_account_dialog_on_cancel(GtkButton *button, gpointer user_data)
{
    CreateSocialAccountDialogState *state = user_data;
    (void) button;
    create_social_account_dialog_cancel(state);
    gtk_window_close(state->window);
}

/** @brief Valide le formulaire et transmet un résultat possédé. */
static void create_social_account_dialog_on_create(GtkButton *button, gpointer user_data)
{
    CreateSocialAccountDialogState *state = user_data;
    CreateSocialAccountDialogResult *result = NULL;
    GDateTime *date = NULL;
    char *notes = NULL;
    guint platform = gtk_drop_down_get_selected(state->platform);
    guint account_state = gtk_drop_down_get_selected(state->state);
    guint evidence = gtk_drop_down_get_selected(state->evidence);
    const char *url = gtk_editable_get_text(GTK_EDITABLE(state->url));
    const char *username = gtk_editable_get_text(GTK_EDITABLE(state->username));
    const char *observed_at = gtk_editable_get_text(GTK_EDITABLE(state->observed_at));
    (void) button;
    date = g_date_time_new_from_iso8601(observed_at, NULL);
    if (platform >= G_N_ELEMENTS(platform_codes) ||
        account_state >= G_N_ELEMENTS(state_codes) ||
        url == NULL ||
        (!g_str_has_prefix(url, "https://") && !g_str_has_prefix(url, "http://")) ||
        username == NULL || username[0] == '\0' || date == NULL)
    {
        gtk_label_set_text(state->error,
            "Renseignez une URL http(s), un pseudonyme et une date ISO 8601 valide.");
        gtk_widget_set_visible(GTK_WIDGET(state->error), TRUE);
        g_clear_pointer(&date, g_date_time_unref);
        return;
    }
    notes = create_social_account_dialog_notes(state);
    result = g_new0(CreateSocialAccountDialogResult, 1);
    result->platform = g_strdup(platform_codes[platform]);
    result->profile_url = create_social_account_dialog_copy(url);
    result->username = create_social_account_dialog_copy(username);
    result->platform_identifier = create_social_account_dialog_copy(
        gtk_editable_get_text(GTK_EDITABLE(state->platform_identifier)));
    result->first_observed_at = create_social_account_dialog_copy(observed_at);
    result->account_state = g_strdup(state_codes[account_state]);
    result->notes = create_social_account_dialog_copy(notes);
    if (evidence > 0 && evidence - 1 < state->evidence_identifiers->len)
        result->evidence_identifier = g_strdup(g_ptr_array_index(
            state->evidence_identifiers, evidence - 1));
    state->completed = TRUE;
    if (state->callback != NULL) state->callback(result, state->user_data);
    else create_social_account_dialog_result_free(result);
    g_free(notes);
    g_date_time_unref(date);
    gtk_window_close(state->window);
}

/** @brief Ajoute une ligne libellée au formulaire. */
static void create_social_account_dialog_add_row(
    GtkGrid *grid, int row, const char *label, GtkWidget *field)
{
    GtkWidget *caption = gtk_label_new(label);
    gtk_label_set_xalign(GTK_LABEL(caption), 0.0f);
    gtk_widget_set_halign(caption, GTK_ALIGN_START);
    gtk_widget_set_hexpand(field, TRUE);
    gtk_grid_attach(grid, caption, 0, row, 1, 1);
    gtk_grid_attach(grid, field, 1, row, 1, 1);
}

gboolean create_social_account_dialog_present(
    GtkWindow *parent, const GPtrArray *evidence_records,
    CreateSocialAccountDialogCallback callback, gpointer user_data)
{
    static const char *const platform_labels[] = {
        "TikTok", "Instagram", "Facebook", "X", "Telegram", "Autre", NULL
    };
    static const char *const state_labels[] = {
        "Inconnu", "Actif", "Privé", "Suspendu", "Supprimé", NULL
    };
    CreateSocialAccountDialogState *state = NULL;
    GtkWidget *box = NULL;
    GtkWidget *grid = NULL;
    GtkWidget *actions = NULL;
    GtkWidget *cancel = NULL;
    GtkWidget *create = NULL;
    GtkStringList *evidence_labels = NULL;
    GDateTime *now = NULL;
    char *now_text = NULL;
    guint index = 0;
    if (parent == NULL) return FALSE;
    state = g_new0(CreateSocialAccountDialogState, 1);
    state->callback = callback;
    state->user_data = user_data;
    state->evidence_identifiers = g_ptr_array_new_with_free_func(g_free);
    state->window = GTK_WINDOW(gtk_window_new());
    gtk_window_set_title(state->window, "Ajouter un compte social");
    gtk_window_set_transient_for(state->window, parent);
    gtk_window_set_modal(state->window, TRUE);
    gtk_window_set_default_size(state->window, 620, 560);
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(box, 16); gtk_widget_set_margin_end(box, 16);
    gtk_widget_set_margin_top(box, 16); gtk_widget_set_margin_bottom(box, 16);
    grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    state->platform = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(platform_labels));
    state->url = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(state->url, "https://www.tiktok.com/@compte");
    state->username = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(state->username, "@pseudonyme affiché");
    state->platform_identifier = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(state->platform_identifier, "Identifiant stable, si visible");
    state->observed_at = GTK_ENTRY(gtk_entry_new());
    now = g_date_time_new_now_utc();
    now_text = g_date_time_format(now, "%Y-%m-%dT%H:%M:%SZ");
    gtk_editable_set_text(GTK_EDITABLE(state->observed_at), now_text);
    state->state = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(state_labels));
    evidence_labels = gtk_string_list_new(NULL);
    gtk_string_list_append(evidence_labels, "Aucune preuve associée");
    for (index = 0; evidence_records != NULL && index < evidence_records->len; index++)
    {
        EvidenceRecord *record = g_ptr_array_index((GPtrArray *) evidence_records, index);
        const char *identifier = evidence_record_get_identifier(record);
        const char *name = evidence_record_get_original_name(record);
        if (identifier == NULL || name == NULL) continue;
        gtk_string_list_append(evidence_labels, name);
        g_ptr_array_add(state->evidence_identifiers, g_strdup(identifier));
    }
    state->evidence = GTK_DROP_DOWN(gtk_drop_down_new(
        G_LIST_MODEL(evidence_labels), NULL));
    g_object_unref(evidence_labels);
    state->notes = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_wrap_mode(state->notes, GTK_WRAP_WORD_CHAR);
    gtk_widget_set_size_request(GTK_WIDGET(state->notes), -1, 100);
    create_social_account_dialog_add_row(GTK_GRID(grid), 0, "Plateforme", GTK_WIDGET(state->platform));
    create_social_account_dialog_add_row(GTK_GRID(grid), 1, "URL complète", GTK_WIDGET(state->url));
    create_social_account_dialog_add_row(GTK_GRID(grid), 2, "Pseudonyme", GTK_WIDGET(state->username));
    create_social_account_dialog_add_row(GTK_GRID(grid), 3, "ID plateforme", GTK_WIDGET(state->platform_identifier));
    create_social_account_dialog_add_row(GTK_GRID(grid), 4, "Première observation (UTC)", GTK_WIDGET(state->observed_at));
    create_social_account_dialog_add_row(GTK_GRID(grid), 5, "État observé", GTK_WIDGET(state->state));
    create_social_account_dialog_add_row(GTK_GRID(grid), 6, "Preuve associée", GTK_WIDGET(state->evidence));
    create_social_account_dialog_add_row(GTK_GRID(grid), 7, "Notes factuelles", GTK_WIDGET(state->notes));
    state->error = GTK_LABEL(gtk_label_new(NULL));
    gtk_label_set_wrap(state->error, TRUE);
    gtk_widget_add_css_class(GTK_WIDGET(state->error), "error");
    gtk_widget_set_visible(GTK_WIDGET(state->error), FALSE);
    actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(actions, GTK_ALIGN_END);
    cancel = gtk_button_new_with_label("Annuler");
    create = gtk_button_new_with_label("Ajouter au graphe");
    gtk_widget_add_css_class(create, "suggested-action");
    gtk_box_append(GTK_BOX(actions), cancel); gtk_box_append(GTK_BOX(actions), create);
    gtk_box_append(GTK_BOX(box), grid);
    gtk_box_append(GTK_BOX(box), GTK_WIDGET(state->error));
    gtk_box_append(GTK_BOX(box), actions);
    gtk_window_set_child(state->window, box);
    g_signal_connect(state->window, "close-request", G_CALLBACK(create_social_account_dialog_on_close), state);
    g_signal_connect(cancel, "clicked", G_CALLBACK(create_social_account_dialog_on_cancel), state);
    g_signal_connect(create, "clicked", G_CALLBACK(create_social_account_dialog_on_create), state);
    g_object_set_data_full(G_OBJECT(state->window), "social-account-state", state,
        create_social_account_dialog_state_free);
    gtk_window_present(state->window);
    g_free(now_text); g_date_time_unref(now);
    return TRUE;
}

void create_social_account_dialog_result_free(CreateSocialAccountDialogResult *result)
{
    if (result == NULL) return;
    g_free(result->platform); g_free(result->profile_url); g_free(result->username);
    g_free(result->platform_identifier); g_free(result->first_observed_at);
    g_free(result->account_state); g_free(result->notes);
    g_free(result->evidence_identifier); g_free(result);
}

#define SOCIAL_RESULT_GETTER(name, field) \
const char *create_social_account_dialog_result_get_##name( \
    const CreateSocialAccountDialogResult *result) \
{ return result != NULL ? result->field : NULL; }

SOCIAL_RESULT_GETTER(platform, platform)
SOCIAL_RESULT_GETTER(profile_url, profile_url)
SOCIAL_RESULT_GETTER(username, username)
SOCIAL_RESULT_GETTER(platform_identifier, platform_identifier)
SOCIAL_RESULT_GETTER(first_observed_at, first_observed_at)
SOCIAL_RESULT_GETTER(account_state, account_state)
SOCIAL_RESULT_GETTER(notes, notes)
SOCIAL_RESULT_GETTER(evidence_identifier, evidence_identifier)
