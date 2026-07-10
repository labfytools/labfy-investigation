/******************************************************************************
 * @file folder_dialog.h
 * @brief Interface du dialogue de sélection d'un dossier d'enquête.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_FOLDER_DIALOG_H
#define LABFY_INVESTIGATION_FOLDER_DIALOG_H

#include <gtk/gtk.h>

/**
 * @brief Fonction appelée après la fermeture du dialogue de sélection.
 *
 * @param folder_path Chemin du dossier sélectionné, ou NULL si l'utilisateur
 *                    a annulé la sélection.
 * @param user_data   Données transmises lors de l'ouverture du dialogue.
 *
 * Le chemin reçu reste valide uniquement pendant l'appel de cette fonction.
 * Il doit être copié si le code appelant souhaite le conserver.
 *
 * @param callback
 *        Fonction appelée lorsque le dialogue est fermé.
 *
 *        Si l'utilisateur annule la sélection,
 *        folder_path vaut NULL.
 */
typedef void (*FolderDialogCallback)(
    const char *folder_path,
    gpointer user_data
);

/**
 * @brief Ouvre un dialogue permettant de sélectionner un dossier.
 *
 * @param parent    Fenêtre parente du dialogue.
 *                  Peut être NULL.
 * @param callback  Fonction appelée lorsque le dialogue est terminé.
 * @param user_data Données transmises à la fonction de rappel.
 */
void folder_dialog_select_folder(
    GtkWindow *parent,
    FolderDialogCallback callback,
    gpointer user_data
);

#endif
