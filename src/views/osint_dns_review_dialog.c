/******************************************************************************
 * @file osint_dns_review_dialog.c
 * @brief Dialogue de sélection des propositions issues d'une réponse DNS.
 ******************************************************************************/

#include "views/osint_dns_review_dialog.h"

#include "models/osint_dns_proposal.h"

typedef struct
{
    GtkWindow *window;
    GtkWidget *select_all_button;
    GtkWidget *confirm_button;
    GPtrArray *proposals;
    GPtrArray *selection_buttons;
    OsintDnsReviewDialogConfirmCallback callback;
    gpointer user_data;
} OsintDnsReviewDialogContext;

/** @brief Libère le contexte possédé par la fenêtre. */
static void osint_dns_review_dialog_context_free(gpointer data)
{
    OsintDnsReviewDialogContext *context = data;
    if (context == NULL) return;
    g_clear_pointer(&context->selection_buttons, g_ptr_array_unref);
    g_clear_pointer(&context->proposals, g_ptr_array_unref);
    g_free(context);
}

/** @brief Ferme le dialogue sans intégrer de proposition. */
static void osint_dns_review_dialog_on_cancel_clicked(
    GtkButton *button,
    gpointer user_data
)
{
    OsintDnsReviewDialogContext *context = user_data;
    (void) button;
    if (context != NULL) gtk_window_destroy(context->window);
}

/** @brief Actualise l'état du bouton de confirmation. */
static void osint_dns_review_dialog_update_confirmation(
    OsintDnsReviewDialogContext *context
)
{
    gboolean has_selection = FALSE;
    for (guint index = 0;
         context != NULL && index < context->selection_buttons->len;
         index++)
    {
        GtkCheckButton *button = g_ptr_array_index(
            context->selection_buttons, index
        );
        if (gtk_widget_get_sensitive(GTK_WIDGET(button)) &&
            gtk_check_button_get_active(button))
        {
            has_selection = TRUE;
            break;
        }
    }
    if (context != NULL)
        gtk_widget_set_sensitive(context->confirm_button, has_selection);
}

/** @brief Réagit à la sélection individuelle d'une proposition. */
static void osint_dns_review_dialog_on_proposal_toggled(
    GtkCheckButton *button,
    gpointer user_data
)
{
    (void) button;
    osint_dns_review_dialog_update_confirmation(user_data);
}

/** @brief Sélectionne ou désélectionne toutes les propositions compatibles. */
static void osint_dns_review_dialog_on_select_all_toggled(
    GtkCheckButton *button,
    gpointer user_data
)
{
    OsintDnsReviewDialogContext *context = user_data;
    const gboolean selected = gtk_check_button_get_active(button);
    for (guint index = 0;
         context != NULL && index < context->selection_buttons->len;
         index++)
    {
        GtkWidget *selection_button = g_ptr_array_index(
            context->selection_buttons, index
        );
        if (gtk_widget_get_sensitive(selection_button))
            gtk_check_button_set_active(
                GTK_CHECK_BUTTON(selection_button), selected
            );
    }
    osint_dns_review_dialog_update_confirmation(context);
}

/** @brief Transmet les propositions cochées puis ferme le dialogue. */
static void osint_dns_review_dialog_on_confirm_clicked(
    GtkButton *button,
    gpointer user_data
)
{
    OsintDnsReviewDialogContext *context = user_data;
    GPtrArray *selected_proposals = g_ptr_array_new();
    (void) button;

    for (guint index = 0;
         context != NULL && index < context->selection_buttons->len;
         index++)
    {
        GtkCheckButton *selection_button = g_ptr_array_index(
            context->selection_buttons, index
        );
        if (gtk_widget_get_sensitive(GTK_WIDGET(selection_button)) &&
            gtk_check_button_get_active(selection_button))
        {
            g_ptr_array_add(
                selected_proposals,
                g_object_get_data(
                    G_OBJECT(selection_button), "osint-dns-proposal"
                )
            );
        }
    }

    if (context != NULL && context->callback != NULL &&
        selected_proposals->len > 0U)
    {
        context->callback(selected_proposals, context->user_data);
    }
    g_ptr_array_unref(selected_proposals);
    if (context != NULL) gtk_window_destroy(context->window);
}

/** @brief Crée une ligne décrivant une proposition DNS. */
static GtkWidget *osint_dns_review_dialog_create_row(
    OsintDnsReviewDialogContext *context,
    const OsintDnsProposal *proposal
)
{
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    const char *entity_type = osint_dns_proposal_get_entity_type(proposal);
    const char *relation_type = osint_dns_proposal_get_relation_type(proposal);
    char *normalized_value = osint_dns_proposal_dup_normalized_value(proposal);
    const gboolean compatible = entity_type != NULL && relation_type != NULL &&
        normalized_value != NULL;
    char *title = g_strdup_printf(
        "%s  →  %s",
        osint_dns_proposal_get_record_type(proposal),
        osint_dns_proposal_get_value(proposal)
    );
    char *details = g_strdup_printf(
        "Cible : %s\nPropriétaire : %s\nType Labfy : %s\n"
        "Relation prévue : %s — %s → %s",
        osint_dns_proposal_get_target(proposal),
        osint_dns_proposal_get_owner(proposal),
        compatible ? entity_type : "non pris en charge",
        osint_dns_proposal_get_target(proposal),
        compatible ? relation_type : "non prise en charge",
        normalized_value != NULL ? normalized_value
                                 : osint_dns_proposal_get_value(proposal)
    );
    GtkWidget *selection_button = gtk_check_button_new_with_label(title);
    GtkWidget *details_label = gtk_label_new(details);

    gtk_widget_set_sensitive(selection_button, compatible);
    gtk_widget_set_margin_start(details_label, 28);
    gtk_widget_set_halign(details_label, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(details_label), 0.0F);
    gtk_label_set_selectable(GTK_LABEL(details_label), TRUE);
    g_object_set_data(
        G_OBJECT(selection_button), "osint-dns-proposal", (gpointer) proposal
    );
    g_signal_connect(
        selection_button, "toggled",
        G_CALLBACK(osint_dns_review_dialog_on_proposal_toggled), context
    );
    g_ptr_array_add(context->selection_buttons, selection_button);
    gtk_box_append(GTK_BOX(row), selection_button);
    gtk_box_append(GTK_BOX(row), details_label);
    g_free(normalized_value);
    g_free(title);
    g_free(details);
    return row;
}

void osint_dns_review_dialog_present(
    GtkWindow *parent_window,
    GPtrArray *proposals,
    OsintDnsReviewDialogConfirmCallback callback,
    gpointer user_data
)
{
    OsintDnsReviewDialogContext *context = NULL;
    GtkWidget *main_box = NULL;
    GtkWidget *title_label = NULL;
    GtkWidget *notice_label = NULL;
    GtkWidget *scrolled_window = NULL;
    GtkWidget *proposal_box = NULL;
    GtkWidget *button_box = NULL;
    GtkWidget *cancel_button = NULL;

    if (proposals == NULL || proposals->len == 0U) return;
    context = g_new0(OsintDnsReviewDialogContext, 1);
    context->window = GTK_WINDOW(gtk_window_new());
    context->proposals = g_ptr_array_ref(proposals);
    context->selection_buttons = g_ptr_array_new();
    context->callback = callback;
    context->user_data = user_data;

    gtk_window_set_title(context->window, "Intégrer les propositions DNS");
    gtk_window_set_default_size(context->window, 720, 560);
    gtk_window_set_modal(context->window, TRUE);
    gtk_window_set_destroy_with_parent(context->window, TRUE);
    if (parent_window != NULL)
        gtk_window_set_transient_for(context->window, parent_window);
    g_object_set_data_full(
        G_OBJECT(context->window), "osint-dns-review-context", context,
        osint_dns_review_dialog_context_free
    );

    main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(main_box, 20);
    gtk_widget_set_margin_end(main_box, 20);
    gtk_widget_set_margin_top(main_box, 20);
    gtk_widget_set_margin_bottom(main_box, 20);
    title_label = gtk_label_new("Sélectionner les propositions à intégrer");
    gtk_widget_set_halign(title_label, GTK_ALIGN_START);
    gtk_widget_add_css_class(title_label, "title-2");
    notice_label = gtk_label_new(
        "Aucune écriture ne sera effectuée avant confirmation. "
        "Les types non pris en charge restent visibles mais désactivés."
    );
    gtk_label_set_wrap(GTK_LABEL(notice_label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(notice_label), 0.0F);

    context->select_all_button = gtk_check_button_new_with_label(
        "Sélectionner toutes les propositions compatibles"
    );
    g_signal_connect(
        context->select_all_button, "toggled",
        G_CALLBACK(osint_dns_review_dialog_on_select_all_toggled), context
    );
    proposal_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    for (guint index = 0; index < proposals->len; index++)
        gtk_box_append(
            GTK_BOX(proposal_box),
            osint_dns_review_dialog_create_row(
                context, g_ptr_array_index(proposals, index)
            )
        );
    scrolled_window = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled_window, TRUE);
    gtk_scrolled_window_set_child(
        GTK_SCROLLED_WINDOW(scrolled_window), proposal_box
    );

    button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(button_box, GTK_ALIGN_END);
    cancel_button = gtk_button_new_with_label("Annuler");
    context->confirm_button = gtk_button_new_with_label("Intégrer la sélection");
    gtk_widget_add_css_class(context->confirm_button, "suggested-action");
    gtk_widget_set_sensitive(context->confirm_button, FALSE);
    g_signal_connect(
        cancel_button, "clicked",
        G_CALLBACK(osint_dns_review_dialog_on_cancel_clicked), context
    );
    g_signal_connect(
        context->confirm_button, "clicked",
        G_CALLBACK(osint_dns_review_dialog_on_confirm_clicked), context
    );
    gtk_box_append(GTK_BOX(button_box), cancel_button);
    gtk_box_append(GTK_BOX(button_box), context->confirm_button);
    gtk_box_append(GTK_BOX(main_box), title_label);
    gtk_box_append(GTK_BOX(main_box), notice_label);
    gtk_box_append(GTK_BOX(main_box), context->select_all_button);
    gtk_box_append(GTK_BOX(main_box), scrolled_window);
    gtk_box_append(GTK_BOX(main_box), button_box);
    gtk_window_set_child(context->window, main_box);
    gtk_window_present(context->window);
}
