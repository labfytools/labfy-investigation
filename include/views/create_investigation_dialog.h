/******************************************************************************
 * @file create_investigation_dialog.h
 * @brief Interface publique du dialogue de création d'une enquête.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_CREATE_INVESTIGATION_DIALOG_H
#define LABFY_INVESTIGATION_CREATE_INVESTIGATION_DIALOG_H

#include <gtk/gtk.h>

/**
 * @brief Callback appelé lorsque le dialogue est validé ou annulé.
 *
 * Lors d'une validation réussie :
 *
 * - parent_directory contient le dossier parent sélectionné ;
 * - investigation_name contient le nom nettoyé de l'enquête.
 *
 * Lors d'une annulation :
 *
 * - parent_directory vaut NULL ;
 * - investigation_name vaut NULL.
 *
 * Les chaînes transmises restent valides uniquement pendant l'appel du
 * callback. Le code appelant doit les copier s'il souhaite les conserver.
 *
 * @param parent_directory Dossier parent sélectionné, ou NULL.
 * @param investigation_name Nom de l'enquête, ou NULL.
 * @param user_data Données utilisateur fournies lors de l'ouverture.
 */
typedef void (*CreateInvestigationDialogCallback)(
    const char *parent_directory,
    const char *investigation_name,
    gpointer user_data
);

/**
 * @brief Présente le dialogue de création d'une nouvelle enquête.
 *
 * Le dialogue permet :
 *
 * - de sélectionner un dossier parent existant ;
 * - de saisir le nom de l'enquête ;
 * - de valider ou d'annuler la création.
 *
 * Cette fonction ne crée aucun fichier et n'accède pas à SQLite.
 *
 * @param parent_window Fenêtre GTK parente, ou NULL.
 * @param callback Fonction appelée lors de la validation ou de l'annulation.
 * @param user_data Données transmises au callback.
 */
void create_investigation_dialog_present(
    GtkWindow *parent_window,
    CreateInvestigationDialogCallback callback,
    gpointer user_data
);

#endif
