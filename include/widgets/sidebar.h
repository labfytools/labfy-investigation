/******************************************************************************
 * @file sidebar.h
 * @brief Interface publique du panneau de navigation latéral.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_SIDEBAR_H
#define LABFY_INVESTIGATION_SIDEBAR_H

#include "core/investigation_tree_model.h"
#include "widgets/investigation_tree_view.h"
#include "widgets/evidence_category_model.h"
#include "models/investigation_graph_model.h"

#include <gtk/gtk.h>

/**
 * @brief Représentation opaque du panneau latéral.
 *
 * La structure réelle est définie dans sidebar.c.
 * Les autres modules manipulent uniquement un pointeur vers Sidebar.
 */
typedef struct Sidebar Sidebar;

/**
 * @brief Callback appelé lorsqu'une preuve est sélectionnée.
 *
 * L'identifiant est emprunté et uniquement valide pendant l'appel.
 * Une sélection vide ou une catégorie transmet NULL.
 *
 * @param evidence_identifier UUID de la preuve, ou NULL.
 * @param user_data Données privées fournies par l'appelant.
 */
typedef void (*SidebarEvidenceSelectionCallback)(
    const char *evidence_identifier,
    gpointer user_data
);

/**
 * @brief Callback appelé lorsqu'une entité est sélectionnée.
 *
 * L'identifiant est emprunté et valide uniquement pendant l'appel.
 * Une sélection vide transmet NULL.
 *
 * @param entity_identifier UUID de l'entité, ou NULL.
 * @param user_data Données privées fournies par l'appelant.
 */
typedef void (*SidebarEntitySelectionCallback)(
    const char *entity_identifier,
    gpointer user_data
);

/**
 * @brief Callback appelé lorsqu'une relation est sélectionnée.
 *
 * @param relation_identifier UUID emprunté de la relation, ou NULL.
 * @param user_data Données privées fournies par l'appelant.
 */
typedef void (*SidebarRelationSelectionCallback)(
    const char *relation_identifier,
    gpointer user_data
);

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
 * @brief Associe un modèle d'arborescence à la barre latérale.
 *
 * La barre latérale ne devient pas propriétaire du modèle.
 * Le modèle doit rester valide pendant toute la durée de son utilisation.
 *
 * Pour ce ticket, seul le nom du nœud racine est affiché dans le titre.
 *
 * @param sidebar    Barre latérale à mettre à jour.
 * @param tree_model Modèle d'arborescence en lecture seule.
 */
void sidebar_set_tree_model(
    Sidebar *sidebar,
    const InvestigationTreeModel *tree_model
);

/**
 * @brief Définit le callback appelé lors de la sélection d'un nœud.
 *
 * La Sidebar transmet simplement ce callback à InvestigationTreeView.
 *
 * @param sidebar   Barre latérale à configurer.
 * @param callback  Fonction appelée lors d'un changement de sélection.
 * @param user_data Données privées transmises au callback.
 */
void sidebar_set_selection_callback(
    Sidebar *sidebar,
    InvestigationTreeViewSelectionCallback callback,
    gpointer user_data
);

/**
 * @brief Installe le modèle des catégories de preuves.
 *
 * La Sidebar ne devient pas propriétaire du modèle.
 * Passer NULL rétablit l'état vide.
 *
 * @param sidebar Barre latérale à actualiser.
 * @param evidence_category_model Modèle de catégories, ou NULL.
 */
void sidebar_set_evidence_model(
    Sidebar *sidebar,
    EvidenceCategoryModel *evidence_category_model
);

/**
 * @brief Définit le callback de sélection d'une preuve.
 *
 * @param sidebar Barre latérale à configurer.
 * @param callback Callback facultatif.
 * @param user_data Données privées transmises au callback.
 */
void sidebar_set_evidence_selection_callback(
    Sidebar *sidebar,
    SidebarEvidenceSelectionCallback callback,
    gpointer user_data
);

/**
 * @brief Installe les entités et relations du graphe dans la barre latérale.
 *
 * La Sidebar emprunte graph_model. Passer NULL vide la liste.
 *
 * @param sidebar Barre latérale à actualiser.
 * @param graph_model Graphe emprunté, ou NULL.
 */
void sidebar_set_graph_model(
    Sidebar *sidebar,
    const InvestigationGraphModel *graph_model
);

/**
 * @brief Définit le callback de sélection d'une entité.
 *
 * @param sidebar Barre latérale à configurer.
 * @param callback Callback facultatif.
 * @param user_data Données privées transmises au callback.
 */
void sidebar_set_entity_selection_callback(
    Sidebar *sidebar,
    SidebarEntitySelectionCallback callback,
    gpointer user_data
);

/**
 * @brief Définit le callback de sélection d'une relation.
 *
 * @param sidebar Barre latérale à configurer.
 * @param callback Callback facultatif.
 * @param user_data Données privées transmises au callback.
 */
void sidebar_set_relation_selection_callback(
    Sidebar *sidebar,
    SidebarRelationSelectionCallback callback,
    gpointer user_data
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
