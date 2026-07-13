/******************************************************************************
 * @file main_window.h
 * @brief Interface publique de la fenêtre principale.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_MAIN_WINDOW_H
#define LABFY_INVESTIGATION_MAIN_WINDOW_H

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
 * @brief Libère les ressources de la fenêtre.
 *
 * @param main_window Fenêtre à détruire.
 */
void main_window_free(MainWindow *main_window);

#endif
