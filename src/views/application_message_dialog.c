/******************************************************************************
 * @file application_message_dialog.c
 * @brief Implémentation de la fenêtre modale de message.
 ******************************************************************************/

#include "views/application_message_dialog.h"

/**
 * @brief Largeur minimale du dialogue.
 */
#define APPLICATION_MESSAGE_DIALOG_DEFAULT_WIDTH 480

/**
 * @brief Message utilisé lorsqu'aucun détail n'est disponible.
 */
#define APPLICATION_MESSAGE_DIALOG_DEFAULT_MESSAGE \
    "Aucun détail supplémentaire n'est disponible."

/**
 * @brief Libellé par défaut du bouton de confirmation.
 */
#define APPLICATION_MESSAGE_DIALOG_DEFAULT_CONFIRM_LABEL \
    "Confirmer"

/**
 * @brief État privé d'un dialogue de confirmation.
 */
typedef struct
{
    GtkWindow *dialog_window;

    ApplicationMessageDialogConfirmationCallback callback;
    gpointer user_data;

    gboolean completed;
} ApplicationMessageDialogConfirmationContext;

/**
 * @brief Retourne le libellé correspondant au type de message.
 *
 * @param message_type Type du message.
 *
 * @return Libellé statique du type.
 */
static const char *application_message_dialog_get_type_label(
    ApplicationMessageDialogType message_type
)
{
    switch (message_type)
    {
        case APPLICATION_MESSAGE_DIALOG_ERROR:
            return "Erreur";

        case APPLICATION_MESSAGE_DIALOG_WARNING:
            return "Avertissement";

        case APPLICATION_MESSAGE_DIALOG_INFORMATION:
            return "Information";

        default:
            return "Information";
    }
}

/**
 * @brief Retourne le titre à utiliser pour le dialogue.
 *
 * @param message_type Type du message.
 * @param title Titre fourni par l'appelant.
 *
 * @return Titre fourni ou titre par défaut.
 */
static const char *application_message_dialog_get_safe_title(
    ApplicationMessageDialogType message_type,
    const char *title
)
{
    if (title != NULL &&
        title[0] != '\0')
    {
        return title;
    }

    return application_message_dialog_get_type_label(
        message_type
    );
}

/**
 * @brief Retourne un message toujours valide.
 *
 * @param message Message fourni par l'appelant.
 *
 * @return Message fourni ou message par défaut.
 */
static const char *application_message_dialog_get_safe_message(
    const char *message
)
{
    if (message != NULL &&
        message[0] != '\0')
    {
        return message;
    }

    return APPLICATION_MESSAGE_DIALOG_DEFAULT_MESSAGE;
}

/**
 * @brief Ferme le dialogue lorsque l'utilisateur clique sur le bouton.
 *
 * @param button Bouton ayant reçu le clic.
 * @param user_data Pointeur vers la fenêtre du dialogue.
 */
static void application_message_dialog_on_close_clicked(
    GtkButton *button,
    gpointer user_data
)
{
    GtkWindow *dialog_window = user_data;

    (void) button;

    if (dialog_window == NULL)
    {
        return;
    }

    gtk_window_destroy(
        dialog_window
    );
}

void application_message_dialog_present(
    GtkWindow *parent_window,
    ApplicationMessageDialogType message_type,
    const char *title,
    const char *message
)
{
    GtkWindow *dialog_window = NULL;

    GtkWidget *main_box = NULL;
    GtkWidget *type_label = NULL;
    GtkWidget *title_label = NULL;
    GtkWidget *message_label = NULL;
    GtkWidget *button_box = NULL;
    GtkWidget *close_button = NULL;

    const char *safe_title = NULL;
    const char *safe_message = NULL;
    const char *type_text = NULL;

    safe_title =
        application_message_dialog_get_safe_title(
            message_type,
            title
        );

    safe_message =
        application_message_dialog_get_safe_message(
            message
        );

    type_text =
        application_message_dialog_get_type_label(
            message_type
        );

    dialog_window = GTK_WINDOW(
        gtk_window_new()
    );

    gtk_window_set_title(
        dialog_window,
        safe_title
    );

    gtk_window_set_default_size(
        dialog_window,
        APPLICATION_MESSAGE_DIALOG_DEFAULT_WIDTH,
        -1
    );

    gtk_window_set_modal(
        dialog_window,
        TRUE
    );

    gtk_window_set_destroy_with_parent(
        dialog_window,
        TRUE
    );

    gtk_window_set_resizable(
        dialog_window,
        FALSE
    );

    if (parent_window != NULL)
    {
        gtk_window_set_transient_for(
            dialog_window,
            parent_window
        );
    }

    main_box = gtk_box_new(
        GTK_ORIENTATION_VERTICAL,
        12
    );

    gtk_widget_set_margin_start(
        main_box,
        20
    );

    gtk_widget_set_margin_end(
        main_box,
        20
    );

    gtk_widget_set_margin_top(
        main_box,
        20
    );

    gtk_widget_set_margin_bottom(
        main_box,
        20
    );

    type_label = gtk_label_new(
        type_text
    );

    gtk_widget_set_halign(
        type_label,
        GTK_ALIGN_START
    );

    gtk_label_set_xalign(
        GTK_LABEL(type_label),
        0.0F
    );

    title_label = gtk_label_new(
        safe_title
    );

    gtk_widget_set_halign(
        title_label,
        GTK_ALIGN_START
    );

    gtk_label_set_xalign(
        GTK_LABEL(title_label),
        0.0F
    );

    gtk_label_set_wrap(
        GTK_LABEL(title_label),
        TRUE
    );

    message_label = gtk_label_new(
        safe_message
    );

    gtk_widget_set_halign(
        message_label,
        GTK_ALIGN_FILL
    );

    gtk_label_set_xalign(
        GTK_LABEL(message_label),
        0.0F
    );

    gtk_label_set_wrap(
        GTK_LABEL(message_label),
        TRUE
    );

    gtk_label_set_wrap_mode(
        GTK_LABEL(message_label),
        PANGO_WRAP_WORD_CHAR
    );

    gtk_label_set_selectable(
        GTK_LABEL(message_label),
        TRUE
    );

    gtk_label_set_max_width_chars(
        GTK_LABEL(message_label),
        70
    );

    button_box = gtk_box_new(
        GTK_ORIENTATION_HORIZONTAL,
        0
    );

    gtk_widget_set_halign(
        button_box,
        GTK_ALIGN_END
    );

    close_button = gtk_button_new_with_label(
        "Fermer"
    );

    g_signal_connect(
        close_button,
        "clicked",
        G_CALLBACK(
            application_message_dialog_on_close_clicked
        ),
        dialog_window
    );

    gtk_box_append(
        GTK_BOX(button_box),
        close_button
    );

    gtk_box_append(
        GTK_BOX(main_box),
        type_label
    );

    gtk_box_append(
        GTK_BOX(main_box),
        title_label
    );

    gtk_box_append(
        GTK_BOX(main_box),
        message_label
    );

    gtk_box_append(
        GTK_BOX(main_box),
        button_box
    );

    gtk_window_set_child(
        dialog_window,
        main_box
    );

    gtk_window_present(
        dialog_window
    );
}

void application_message_dialog_present_details(
    GtkWindow *parent_window,
    ApplicationMessageDialogType message_type,
    const char *title,
    const char *message,
    const char *details
)
{
    GtkWindow *dialog_window = GTK_WINDOW(gtk_window_new());
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *title_label = gtk_label_new(
        application_message_dialog_get_safe_title(message_type, title)
    );
    GtkWidget *message_label = gtk_label_new(
        application_message_dialog_get_safe_message(message)
    );
    GtkWidget *scrolled_window = gtk_scrolled_window_new();
    GtkWidget *details_view = gtk_text_view_new();
    GtkWidget *close_button = gtk_button_new_with_label("Fermer");
    GtkTextBuffer *details_buffer = gtk_text_view_get_buffer(
        GTK_TEXT_VIEW(details_view)
    );

    gtk_window_set_title(
        dialog_window,
        application_message_dialog_get_safe_title(message_type, title)
    );
    gtk_window_set_default_size(dialog_window, 760, 520);
    gtk_window_set_modal(dialog_window, TRUE);
    gtk_window_set_destroy_with_parent(dialog_window, TRUE);
    if (parent_window != NULL)
    {
        gtk_window_set_transient_for(dialog_window, parent_window);
    }

    gtk_widget_set_margin_start(main_box, 20);
    gtk_widget_set_margin_end(main_box, 20);
    gtk_widget_set_margin_top(main_box, 20);
    gtk_widget_set_margin_bottom(main_box, 20);

    gtk_widget_set_halign(title_label, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(title_label), 0.0F);
    gtk_widget_add_css_class(title_label, "title-2");

    gtk_widget_set_halign(message_label, GTK_ALIGN_FILL);
    gtk_label_set_xalign(GTK_LABEL(message_label), 0.0F);
    gtk_label_set_wrap(GTK_LABEL(message_label), TRUE);

    gtk_text_view_set_editable(GTK_TEXT_VIEW(details_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(details_view), TRUE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(details_view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(details_view), GTK_WRAP_NONE);
    gtk_text_buffer_set_text(
        details_buffer,
        details != NULL && details[0] != '\0'
            ? details
            : APPLICATION_MESSAGE_DIALOG_DEFAULT_MESSAGE,
        -1
    );

    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(scrolled_window),
        GTK_POLICY_AUTOMATIC,
        GTK_POLICY_AUTOMATIC
    );
    gtk_widget_set_vexpand(scrolled_window, TRUE);
    gtk_scrolled_window_set_child(
        GTK_SCROLLED_WINDOW(scrolled_window),
        details_view
    );

    gtk_widget_set_halign(close_button, GTK_ALIGN_END);
    g_signal_connect(
        close_button,
        "clicked",
        G_CALLBACK(application_message_dialog_on_close_clicked),
        dialog_window
    );

    gtk_box_append(GTK_BOX(main_box), title_label);
    gtk_box_append(GTK_BOX(main_box), message_label);
    gtk_box_append(GTK_BOX(main_box), scrolled_window);
    gtk_box_append(GTK_BOX(main_box), close_button);
    gtk_window_set_child(dialog_window, main_box);
    gtk_window_present(dialog_window);
}

/**
 * @brief Libère le contexte associé à une fenêtre de confirmation.
 */
static void application_message_dialog_confirmation_context_free(
    gpointer data
)
{
    ApplicationMessageDialogConfirmationContext *context =
        data;

    if (context == NULL)
    {
        return;
    }

    context->dialog_window =
        NULL;

    context->callback =
        NULL;

    context->user_data =
        NULL;

    g_free(
        context
    );
}

/**
 * @brief Termine une confirmation et transmet son résultat une seule fois.
 *
 * Lorsque destroy_window vaut TRUE, la fenêtre est détruite avant l'appel
 * du callback. Les valeurs du callback sont donc copiées localement avant
 * la destruction du contexte attaché à la fenêtre.
 */
static void application_message_dialog_complete_confirmation(
    ApplicationMessageDialogConfirmationContext *context,
    gboolean confirmed,
    gboolean destroy_window
)
{
    ApplicationMessageDialogConfirmationCallback callback =
        NULL;

    gpointer user_data =
        NULL;

    GtkWindow *dialog_window =
        NULL;

    if (context == NULL ||
        context->completed)
    {
        return;
    }

    context->completed =
        TRUE;

    callback =
        context->callback;

    user_data =
        context->user_data;

    dialog_window =
        context->dialog_window;

    context->callback =
        NULL;

    context->user_data =
        NULL;

    if (destroy_window &&
        dialog_window != NULL)
    {
        gtk_window_destroy(
            dialog_window
        );
    }

    if (callback != NULL)
    {
        callback(
            confirmed,
            user_data
        );
    }
}

/**
 * @brief Annule le dialogue depuis son bouton.
 */
static void application_message_dialog_on_cancel_clicked(
    GtkButton *button,
    gpointer user_data
)
{
    ApplicationMessageDialogConfirmationContext *context =
        user_data;

    (void) button;

    application_message_dialog_complete_confirmation(
        context,
        FALSE,
        TRUE
    );
}

/**
 * @brief Confirme le dialogue depuis son bouton principal.
 */
static void application_message_dialog_on_confirm_clicked(
    GtkButton *button,
    gpointer user_data
)
{
    ApplicationMessageDialogConfirmationContext *context =
        user_data;

    (void) button;

    application_message_dialog_complete_confirmation(
        context,
        TRUE,
        TRUE
    );
}

/**
 * @brief Traite la fermeture manuelle comme une annulation.
 */
static gboolean application_message_dialog_on_confirmation_close_requested(
    GtkWindow *dialog_window,
    gpointer user_data
)
{
    ApplicationMessageDialogConfirmationContext *context =
        user_data;

    (void) dialog_window;

    application_message_dialog_complete_confirmation(
        context,
        FALSE,
        FALSE
    );

    /*
     * FALSE laisse GTK poursuivre la fermeture normale de la fenêtre.
     */
    return FALSE;
}

void application_message_dialog_present_confirmation(
    GtkWindow *parent_window,
    ApplicationMessageDialogType message_type,
    const char *title,
    const char *message,
    const char *confirm_label,
    ApplicationMessageDialogConfirmationCallback callback,
    gpointer user_data
)
{
    GtkWindow *dialog_window =
        NULL;

    GtkWidget *main_box =
        NULL;

    GtkWidget *type_label =
        NULL;

    GtkWidget *title_label =
        NULL;

    GtkWidget *message_label =
        NULL;

    GtkWidget *button_box =
        NULL;

    GtkWidget *cancel_button =
        NULL;

    GtkWidget *confirm_button =
        NULL;

    ApplicationMessageDialogConfirmationContext *context =
        NULL;

    const char *safe_title =
        NULL;

    const char *safe_message =
        NULL;

    const char *safe_confirm_label =
        NULL;

    const char *type_text =
        NULL;

    safe_title =
        application_message_dialog_get_safe_title(
            message_type,
            title
        );

    safe_message =
        application_message_dialog_get_safe_message(
            message
        );

    safe_confirm_label =
        confirm_label != NULL &&
        confirm_label[0] != '\0'
            ? confirm_label
            : APPLICATION_MESSAGE_DIALOG_DEFAULT_CONFIRM_LABEL;

    type_text =
        application_message_dialog_get_type_label(
            message_type
        );

    context =
        g_try_new0(
            ApplicationMessageDialogConfirmationContext,
            1
        );

    if (context == NULL)
    {
        return;
    }

    dialog_window =
        GTK_WINDOW(
            gtk_window_new()
        );

    context->dialog_window =
        dialog_window;

    context->callback =
        callback;

    context->user_data =
        user_data;

    context->completed =
        FALSE;

    g_object_set_data_full(
        G_OBJECT(
            dialog_window
        ),
        "application-message-dialog-confirmation-context",
        context,
        application_message_dialog_confirmation_context_free
    );

    gtk_window_set_title(
        dialog_window,
        safe_title
    );

    gtk_window_set_default_size(
        dialog_window,
        APPLICATION_MESSAGE_DIALOG_DEFAULT_WIDTH,
        -1
    );

    gtk_window_set_modal(
        dialog_window,
        TRUE
    );

    gtk_window_set_destroy_with_parent(
        dialog_window,
        TRUE
    );

    gtk_window_set_resizable(
        dialog_window,
        FALSE
    );

    if (parent_window != NULL)
    {
        gtk_window_set_transient_for(
            dialog_window,
            parent_window
        );
    }

    main_box =
        gtk_box_new(
            GTK_ORIENTATION_VERTICAL,
            12
        );

    gtk_widget_set_margin_start(
        main_box,
        20
    );

    gtk_widget_set_margin_end(
        main_box,
        20
    );

    gtk_widget_set_margin_top(
        main_box,
        20
    );

    gtk_widget_set_margin_bottom(
        main_box,
        20
    );

    type_label =
        gtk_label_new(
            type_text
        );

    gtk_widget_set_halign(
        type_label,
        GTK_ALIGN_START
    );

    gtk_label_set_xalign(
        GTK_LABEL(
            type_label
        ),
        0.0F
    );

    title_label =
        gtk_label_new(
            safe_title
        );

    gtk_widget_set_halign(
        title_label,
        GTK_ALIGN_START
    );

    gtk_label_set_xalign(
        GTK_LABEL(
            title_label
        ),
        0.0F
    );

    gtk_label_set_wrap(
        GTK_LABEL(
            title_label
        ),
        TRUE
    );

    gtk_widget_add_css_class(
        title_label,
        "title-2"
    );

    message_label =
        gtk_label_new(
            safe_message
        );

    gtk_widget_set_halign(
        message_label,
        GTK_ALIGN_FILL
    );

    gtk_label_set_xalign(
        GTK_LABEL(
            message_label
        ),
        0.0F
    );

    gtk_label_set_wrap(
        GTK_LABEL(
            message_label
        ),
        TRUE
    );

    gtk_label_set_wrap_mode(
        GTK_LABEL(
            message_label
        ),
        PANGO_WRAP_WORD_CHAR
    );

    gtk_label_set_selectable(
        GTK_LABEL(
            message_label
        ),
        TRUE
    );

    gtk_label_set_max_width_chars(
        GTK_LABEL(
            message_label
        ),
        70
    );

    button_box =
        gtk_box_new(
            GTK_ORIENTATION_HORIZONTAL,
            8
        );

    gtk_widget_set_halign(
        button_box,
        GTK_ALIGN_END
    );

    cancel_button =
        gtk_button_new_with_label(
            "Annuler"
        );

    confirm_button =
        gtk_button_new_with_label(
            safe_confirm_label
        );

    gtk_widget_add_css_class(
        confirm_button,
        "destructive-action"
    );

    g_signal_connect(
        cancel_button,
        "clicked",
        G_CALLBACK(
            application_message_dialog_on_cancel_clicked
        ),
        context
    );

    g_signal_connect(
        confirm_button,
        "clicked",
        G_CALLBACK(
            application_message_dialog_on_confirm_clicked
        ),
        context
    );

    g_signal_connect(
        dialog_window,
        "close-request",
        G_CALLBACK(
            application_message_dialog_on_confirmation_close_requested
        ),
        context
    );

    gtk_box_append(
        GTK_BOX(
            button_box
        ),
        cancel_button
    );

    gtk_box_append(
        GTK_BOX(
            button_box
        ),
        confirm_button
    );

    gtk_box_append(
        GTK_BOX(
            main_box
        ),
        type_label
    );

    gtk_box_append(
        GTK_BOX(
            main_box
        ),
        title_label
    );

    gtk_box_append(
        GTK_BOX(
            main_box
        ),
        message_label
    );

    gtk_box_append(
        GTK_BOX(
            main_box
        ),
        button_box
    );

    gtk_window_set_child(
        dialog_window,
        main_box
    );

    gtk_widget_grab_focus(
        cancel_button
    );

    gtk_window_present(
        dialog_window
    );
}
