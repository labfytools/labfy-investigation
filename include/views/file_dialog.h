/******************************************************************************
 * @file file_dialog.h
 * @brief Interface du dialogue de sélection d’un fichier de preuve.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_FILE_DIALOG_H
#define LABFY_INVESTIGATION_FILE_DIALOG_H

#include <gtk/gtk.h>

/**
 * @brief Fonction appelée après la fermeture du sélecteur.
 *
 * Cas possibles :
 *
 * - succès : file_path non NULL et error NULL ;
 * - annulation : file_path NULL et error NULL ;
 * - erreur : file_path NULL et error non NULL.
 *
 * Le chemin et l’erreur restent valides uniquement pendant l’appel.
 *
 * @param file_path Chemin local sélectionné, ou NULL.
 * @param error Erreur de sélection, ou NULL.
 * @param user_data Données transmises lors de l’ouverture.
 */
typedef void (*FileDialogCallback)(
    const char *file_path,
    const GError *error,
    gpointer user_data
);

/**
 * @brief Fonction appelée après une sélection multiple.
 *
 * paths est un tableau emprunté de chemins locaux possédés par le dialogue.
 * Il n'est valide que pendant l'appel. Une annulation transmet NULL sans
 * erreur.
 *
 * @param paths Chemins sélectionnés, ou NULL.
 * @param error Erreur de sélection, ou NULL.
 * @param user_data Données fournies lors de l'ouverture.
 */
typedef void (*FileDialogMultipleCallback)(
    const GPtrArray *paths,
    const GError *error,
    gpointer user_data
);

/**
 * @brief Ouvre un dialogue permettant de sélectionner un fichier.
 *
 * @param parent Fenêtre parente, éventuellement NULL.
 * @param callback Fonction appelée lorsque le dialogue est terminé.
 * @param user_data Données transmises au callback.
 */
void file_dialog_select_file(
    GtkWindow *parent,
    FileDialogCallback callback,
    gpointer user_data
);

/**
 * @brief Ouvre un dialogue permettant de sélectionner plusieurs fichiers.
 * @param parent Fenêtre parente, éventuellement NULL.
 * @param callback Fonction appelée à la fermeture du dialogue.
 * @param user_data Données transmises au callback.
 */
void file_dialog_select_files(
    GtkWindow *parent,
    FileDialogMultipleCallback callback,
    gpointer user_data
);

#endif
