/******************************************************************************
 * @file main_window.h
 * @brief Interface publique de la fenêtre principale.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_MAIN_WINDOW_H
#define LABFY_INVESTIGATION_MAIN_WINDOW_H

#include "core/investigation_tree_model.h"
#include "widgets/investigation_tree_view.h"

#include <gtk/gtk.h>

/**
 * @brief Représentation opaque de la fenêtre principale.
 */
typedef struct MainWindow MainWindow;

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

void main_window_set_tree_selection_callback(
    MainWindow *main_window,
    InvestigationTreeViewSelectionCallback callback,
    gpointer user_data
);

/**
 * @brief Libère les ressources de la fenêtre.
 *
 * @param main_window Fenêtre à détruire.
 */
void main_window_free(MainWindow *main_window);

#endif
