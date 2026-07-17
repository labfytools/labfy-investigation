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
