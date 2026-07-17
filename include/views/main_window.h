/******************************************************************************
 * @file main_window.h
 * @brief Interface publique de la fenêtre principale.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_MAIN_WINDOW_H
#define LABFY_INVESTIGATION_MAIN_WINDOW_H

#include "core/investigation_tree_model.h"
#include "widgets/investigation_tree_view.h"
#include "core/investigation_node.h"

#include <gtk/gtk.h>

/**
 * @brief Représentation opaque de la fenêtre principale.
 */
typedef struct MainWindow MainWindow;

/**
 * @brief Callback appelé lorsque l'utilisateur demande une nouvelle enquête.
 *
 * @param user_data Données utilisateur associées au callback.
 */
typedef void (*MainWindowNewInvestigationCallback)(
    gpointer user_data
);

/**
 * @brief Définit le callback du bouton « Nouvelle enquête ».
 *
 * La fenêtre ne crée pas directement l'enquête. Elle transmet uniquement
 * la demande au contrôleur.
 *
 * @param main_window Fenêtre principale.
 * @param callback Fonction appelée lors du clic.
 * @param user_data Données transmises au callback.
 */
void main_window_set_new_investigation_callback(
    MainWindow *main_window,
    MainWindowNewInvestigationCallback callback,
    gpointer user_data
);

/**
 * @brief Crée une nouvelle fenêtre principale.
 *
 * @param application Application GTK.
 *
 * @return Une nouvelle fenêtre ou NULL en cas d'échec.
 */
MainWindow *main_window_new(GtkApplication *application);

/**
 * @brief Affiche la fenêtre principale.
 *
 * @param main_window Fenêtre à afficher.
 */
void main_window_present(MainWindow *main_window);

/**
 * @brief Retourne la fenêtre GTK associée.
 *
 * @param main_window Fenêtre principale.
 *
 * @return Fenêtre GTK.
 */
GtkWindow *main_window_get_window(
    const MainWindow *main_window
);

/**
 * @brief Transmet un modèle d'arborescence à la fenêtre principale.
 *
 * La fenêtre principale ne devient pas propriétaire du modèle.
 * Elle le transmet uniquement à la Sidebar.
 *
 * @param main_window Fenêtre principale à mettre à jour.
 * @param tree_model  Modèle d'arborescence en lecture seule.
 */
void main_window_set_tree_model(
    MainWindow *main_window,
    const InvestigationTreeModel *tree_model
);

/**
 * @brief Met à jour l'identité de l'enquête affichée.
 *
 * La fonction met à jour :
 *
 * - le titre de la fenêtre ;
 * - le texte de la barre d'état.
 *
 * Les chaînes sont copiées par GTK et restent la propriété de l'appelant.
 *
 * Cette fonction accepte des chaînes NULL ou vides.
 *
 * @param main_window Fenêtre principale à mettre à jour.
 * @param investigation_name Nom persistant de l'enquête.
 * @param investigation_root_path Chemin racine de l'enquête.
 */
void main_window_set_investigation(
    MainWindow *main_window,
    const char *investigation_name,
    const char *investigation_root_path
);

void main_window_set_tree_selection_callback(
    MainWindow *main_window,
    InvestigationTreeViewSelectionCallback callback,
    gpointer user_data
);

/**
 * @brief Affiche dans le Workspace les informations du nœud sélectionné.
 *
 * La fenêtre principale ne devient pas propriétaire du nœud.
 *
 * @param main_window Fenêtre principale à mettre à jour.
 * @param node        Nœud sélectionné en lecture seule, ou NULL.
 */
void main_window_set_selected_node(
    MainWindow *main_window,
    const InvestigationNode *node
);

/**
 * @brief Met à jour le texte de la barre d'état.
 *
 * Si status_text vaut NULL, le texte par défaut est affiché.
 *
 * @param main_window Fenêtre principale.
 * @param status_text Nouveau texte de statut, ou NULL.
 */
void main_window_set_status(
    MainWindow *main_window,
    const char *status_text
);

/**
 * @brief Callback appelé lorsque l'utilisateur demande l'ouverture
 *        d'une enquête existante.
 *
 * @param user_data Données utilisateur associées au callback.
 */
typedef void (*MainWindowOpenInvestigationCallback)(
    gpointer user_data
);

/**
 * @brief Callback appelé lorsque l'utilisateur demande à quitter.
 *
 * @param user_data Données utilisateur associées au callback.
 */
typedef void (*MainWindowQuitCallback)(
    gpointer user_data
);

/**
 * @brief Définit le callback du bouton « Ouvrir une enquête ».
 *
 * La fenêtre transmet uniquement la demande au contrôleur.
 *
 * @param main_window Fenêtre principale.
 * @param callback Fonction appelée lors du clic.
 * @param user_data Données transmises au callback.
 */
void main_window_set_open_investigation_callback(
    MainWindow *main_window,
    MainWindowOpenInvestigationCallback callback,
    gpointer user_data
);

/**
 * @brief Définit le callback du bouton « Quitter ».
 *
 * MainWindow transmet uniquement la demande au contrôleur.
 *
 * @param main_window Fenêtre principale.
 * @param callback Fonction appelée lors du clic.
 * @param user_data Données transmises au callback.
 */
void main_window_set_quit_callback(
    MainWindow *main_window,
    MainWindowQuitCallback callback,
    gpointer user_data
);

/**
 * @brief Libère les ressources de la fenêtre.
 *
 * @param main_window Fenêtre à détruire.
 */
void main_window_free(MainWindow *main_window);

#endif
