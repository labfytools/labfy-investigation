/******************************************************************************
 * @file task_panel.h
 * @brief Panneau GTK affichant les tâches suivies par TaskManager.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_TASK_PANEL_H
#define LABFY_INVESTIGATION_TASK_PANEL_H

#include "core/task_manager.h"

#include <gtk/gtk.h>

/**
 * @brief Représentation opaque du panneau d’activité.
 */
typedef struct TaskPanel TaskPanel;

/**
 * @brief Crée un panneau affichant les tâches d’un gestionnaire.
 *
 * Le panneau ne devient pas propriétaire de task_manager.
 * Le gestionnaire doit rester valide pendant toute la durée de vie
 * du panneau.
 *
 * @param task_manager Gestionnaire de tâches observé.
 *
 * @return Nouveau panneau, ou NULL si le gestionnaire est invalide.
 */
TaskPanel *task_panel_new(
    TaskManager *task_manager
);

/**
 * @brief Retourne le widget racine du panneau.
 *
 * Le pointeur retourné est emprunté.
 *
 * @param task_panel Panneau concerné.
 *
 * @return Widget racine, ou NULL.
 */
GtkWidget *task_panel_get_widget(
    const TaskPanel *task_panel
);

/**
 * @brief Reconstruit l’affichage à partir du gestionnaire.
 *
 * @param task_panel Panneau à actualiser.
 */
void task_panel_refresh(
    TaskPanel *task_panel
);

/**
 * @brief Libère le panneau et arrête son rafraîchissement périodique.
 *
 * Le gestionnaire associé n’est pas libéré.
 *
 * @param task_panel Panneau à libérer.
 */
void task_panel_free(
    TaskPanel *task_panel
);

#endif
