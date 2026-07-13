/******************************************************************************
 * @file sidebar.h
 * @brief Interface publique du panneau de navigation latéral.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_SIDEBAR_H
#define LABFY_INVESTIGATION_SIDEBAR_H

#include <gtk/gtk.h>

/**
 * @brief Représentation opaque du panneau latéral.
 *
 * La structure réelle est définie dans sidebar.c.
 * Les autres modules manipulent uniquement un pointeur vers Sidebar.
 */
typedef struct Sidebar Sidebar;

/**
 * @brief Crée un nouveau panneau de navigation latéral.
 *
 * @return Un nouveau panneau latéral, ou NULL en cas d'échec.
 */
Sidebar *sidebar_new(void);

/**
 * @brief Retourne le widget GTK racine du panneau latéral.
 *
 * Le widget retourné appartient au module Sidebar et ne doit pas être libéré
 * directement par le code appelant.
 *
 * @param sidebar Panneau latéral à consulter.
 *
 * @return Le widget GTK racine, ou NULL si sidebar est NULL.
 */
GtkWidget *sidebar_get_widget(
    const Sidebar *sidebar
);

/**
 * @brief Libère la structure d'encapsulation du panneau latéral.
 *
 * Cette fonction accepte NULL.
 *
 * @param sidebar Panneau latéral à libérer.
 */
void sidebar_free(Sidebar *sidebar);

#endif
