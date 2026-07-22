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
 * @brief Callback appelé à la fermeture d'un dialogue de confirmation.
 *
 * Le callback est appelé une seule fois après un clic sur l'un des boutons
 * ou après une fermeture manuelle de la fenêtre.
 *
 * @param confirmed TRUE si l'utilisateur confirme, sinon FALSE.
 * @param user_data Données empruntées fournies par l'appelant.
 */
typedef void (*ApplicationMessageDialogConfirmationCallback)(
    gboolean confirmed,
    gpointer user_data
);

/**
 * @brief Callback appelé par l'action facultative d'un dialogue détaillé.
 *
 * @param user_data Données associées à l'action.
 */
typedef void (*ApplicationMessageDialogActionCallback)(
    gpointer user_data
);

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

/**
 * @brief Affiche un message accompagné d'un contenu textuel défilable.
 *
 * Le contenu détaillé est affiché dans une zone monospace sélectionnable.
 * Les chaînes sont copiées par les widgets GTK.
 *
 * @param parent_window Fenêtre parente, ou NULL.
 * @param message_type Type de message.
 * @param title Titre de la fenêtre, ou NULL.
 * @param message Message principal, ou NULL.
 * @param details Contenu détaillé, ou NULL.
 */
void application_message_dialog_present_details(
    GtkWindow *parent_window,
    ApplicationMessageDialogType message_type,
    const char *title,
    const char *message,
    const char *details
);

/**
 * @brief Affiche un message détaillé avec une action facultative.
 *
 * Le dialogue devient propriétaire de action_data lorsque l'affichage est
 * créé. Les données sont libérées avec action_data_destroy à sa fermeture.
 *
 * @param parent_window Fenêtre parente, ou NULL.
 * @param message_type Type de message.
 * @param title Titre de la fenêtre, ou NULL.
 * @param message Message principal, ou NULL.
 * @param details Contenu détaillé, ou NULL.
 * @param action_label Libellé de l'action, ou NULL.
 * @param action_callback Callback de l'action, ou NULL.
 * @param action_data Données transmises au callback.
 * @param action_data_destroy Destructeur des données, ou NULL.
 */
void application_message_dialog_present_details_action(
    GtkWindow *parent_window,
    ApplicationMessageDialogType message_type,
    const char *title,
    const char *message,
    const char *details,
    const char *action_label,
    ApplicationMessageDialogActionCallback action_callback,
    gpointer action_data,
    GDestroyNotify action_data_destroy
);

/**
 * @brief Affiche un dialogue modal demandant une confirmation.
 *
 * Le bouton d'annulation transmet FALSE. Le bouton de confirmation transmet
 * TRUE. Fermer la fenêtre manuellement équivaut à une annulation.
 *
 * callback et user_data sont empruntés jusqu'à la fermeture du dialogue.
 * confirm_label peut être NULL ou vide pour utiliser « Confirmer ».
 *
 * @param parent_window Fenêtre parente, ou NULL.
 * @param message_type Type de message.
 * @param title Titre de la fenêtre, ou NULL.
 * @param message Message principal, ou NULL.
 * @param confirm_label Libellé du bouton de confirmation, ou NULL.
 * @param callback Callback facultatif recevant la réponse.
 * @param user_data Données empruntées transmises au callback.
 */
void application_message_dialog_present_confirmation(
    GtkWindow *parent_window,
    ApplicationMessageDialogType message_type,
    const char *title,
    const char *message,
    const char *confirm_label,
    ApplicationMessageDialogConfirmationCallback callback,
    gpointer user_data
);

#endif
