/******************************************************************************
 * @file investigation_tree_view.h
 * @brief Interface publique de la vue arborescente d'une enquête.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_INVESTIGATION_TREE_VIEW_H
#define LABFY_INVESTIGATION_INVESTIGATION_TREE_VIEW_H

#include "core/investigation_tree_model.h"

#include <gtk/gtk.h>

/**
 * @brief Représentation opaque de la vue arborescente d'une enquête.
 *
 * La structure réelle est définie dans investigation_tree_view.c.
 * Les autres modules manipulent uniquement un pointeur vers
 * InvestigationTreeView.
 */
typedef struct InvestigationTreeView InvestigationTreeView;

/**
 * @brief Callback appelé lorsqu'un nœud est sélectionné.
 *
 * @param node      Nœud sélectionné, ou NULL si aucune sélection n'est active.
 * @param user_data Données privées fournies lors de l'enregistrement
 *                  du callback.
 */
typedef void (*InvestigationTreeViewSelectionCallback)(
    const InvestigationNode *node,
    gpointer user_data
);

/**
 * @brief Définit le callback de sélection.
 *
 * Le callback est appelé à chaque changement de sélection.
 *
 * Le composant ne devient pas propriétaire du callback ni de user_data.
 *
 * @param tree_view Vue arborescente.
 * @param callback  Fonction appelée lors d'une sélection.
 * @param user_data Données privées transmises au callback.
 */
void investigation_tree_view_set_selection_callback(
    InvestigationTreeView *tree_view,
    InvestigationTreeViewSelectionCallback callback,
    gpointer user_data
);

/**
 * @brief Crée une nouvelle vue arborescente vide.
 *
 * Aucun modèle métier n'est associé au composant lors de sa création.
 *
 * @return Une nouvelle vue, ou NULL si sa création échoue.
 */
InvestigationTreeView *investigation_tree_view_new(void);

/**
 * @brief Retourne le widget GTK racine de la vue arborescente.
 *
 * Le widget retourné appartient au module InvestigationTreeView.
 * Le code appelant ne doit ni le détruire ni en libérer la référence.
 *
 * @param tree_view Vue arborescente à consulter.
 *
 * @return Le widget racine, ou NULL si tree_view vaut NULL.
 */
GtkWidget *investigation_tree_view_get_widget(
    const InvestigationTreeView *tree_view
);

/**
 * @brief Associe un modèle métier à la vue arborescente.
 *
 * La vue ne devient pas propriétaire du modèle. Le modèle doit rester valide
 * pendant toute la durée de son affichage.
 *
 * Passer NULL retire le modèle actuel et vide la vue.
 *
 * @param tree_view  Vue arborescente à mettre à jour.
 * @param tree_model Modèle métier en lecture seule, ou NULL.
 */
void investigation_tree_view_set_model(
    InvestigationTreeView *tree_view,
    const InvestigationTreeModel *tree_model
);

/**
 * @brief Libère la structure d'encapsulation de la vue.
 *
 * Cette fonction accepte NULL.
 *
 * Les objets GTK intégrés à l'arbre de widgets restent gérés par GTK.
 *
 * @param tree_view Vue arborescente à libérer.
 */
void investigation_tree_view_free(
    InvestigationTreeView *tree_view
);

#endif
