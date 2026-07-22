/******************************************************************************
 * @file osint_execution_history_dialog.h
 * @brief Consultation en lecture seule de l'historique OSINT.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_OSINT_EXECUTION_HISTORY_DIALOG_H
#define LABFY_INVESTIGATION_OSINT_EXECUTION_HISTORY_DIALOG_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

/**
 * @brief Affiche les exécutions et leurs objets liés dans une fenêtre modale.
 *
 * La fonction conserve une référence sur les deux conteneurs jusqu'à la
 * fermeture. records contient des OsintExecutionRecord. linked_objects associe
 * chaque UUID d'exécution à un GPtrArray de chaînes.
 *
 * @param parent_window Fenêtre parente, ou NULL.
 * @param records Historique non vide, de la plus récente à l'ancienne.
 * @param linked_objects Table des objets liés par UUID d'exécution.
 */
void osint_execution_history_dialog_present(
    GtkWindow *parent_window,
    GPtrArray *records,
    GHashTable *linked_objects
);

G_END_DECLS

#endif
