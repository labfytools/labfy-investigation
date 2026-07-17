/******************************************************************************
 * @file application_message_dialog.h
 * @brief Fenêtre modale affichant un message à l'utilisateur.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_APPLICATION_MESSAGE_DIALOG_H
#define LABFY_INVESTIGATION_APPLICATION_MESSAGE_DIALOG_H

#include <gtk/gtk.h>

/**
 * @brief Nature du message affiché.
 */
typedef enum
{
    APPLICATION_MESSAGE_DIALOG_ERROR,
    APPLICATION_MESSAGE_DIALOG_WARNING,
    APPLICATION_MESSAGE_DIALOG_INFORMATION
} ApplicationMessageDialogType;

/**
 * @brief Affiche une fenêtre modale contenant un message.
 *
 * Les chaînes sont copiées par les widgets GTK.
 *
 * @param parent_window Fenêtre parente, ou NULL.
 * @param message_type Type de message.
 * @param title Titre de la fenêtre, ou NULL.
 * @param message Message principal, ou NULL.
 */
void application_message_dialog_present(
    GtkWindow *parent_window,
    ApplicationMessageDialogType message_type,
    const char *title,
    const char *message
);

#endif
