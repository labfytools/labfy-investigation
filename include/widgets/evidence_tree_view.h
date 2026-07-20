/******************************************************************************
 * @file evidence_tree_view.h
 * @brief Affichage GTK hiérarchique des catégories et preuves importées.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_EVIDENCE_TREE_VIEW_H
#define LABFY_INVESTIGATION_EVIDENCE_TREE_VIEW_H

#include "widgets/evidence_category_model.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

/**
 * @brief Vue opaque affichant les preuves regroupées par catégorie.
 */
typedef struct EvidenceTreeView EvidenceTreeView;

/**
 * @brief Callback appelé lorsqu'une preuve est sélectionnée.
 *
 * L'identifiant est emprunté et uniquement valide pendant l'appel.
 *
 * Lorsque la sélection est vide ou correspond à une catégorie,
 * evidence_identifier vaut NULL.
 *
 * @param evidence_identifier UUID de la preuve sélectionnée, ou NULL.
 * @param user_data Données privées fournies par l'appelant.
 */
typedef void (*EvidenceTreeViewSelectionCallback)(
    const char *evidence_identifier,
    gpointer user_data
);

/**
 * @brief Crée une vue hiérarchique vide.
 *
 * @return Nouvelle vue, ou NULL.
 */
EvidenceTreeView *evidence_tree_view_new(void);

/**
 * @brief Retourne le widget GTK racine.
 *
 * Le widget retourné est emprunté.
 *
 * @param evidence_tree_view Vue à consulter.
 *
 * @return GtkScrolledWindow racine, ou NULL.
 */
GtkWidget *evidence_tree_view_get_widget(
    const EvidenceTreeView *evidence_tree_view
);

/**
 * @brief Installe un modèle de catégories.
 *
 * La vue ne devient pas propriétaire de EvidenceCategoryModel.
 * Le modèle doit rester valide pendant son utilisation.
 *
 * Passer NULL vide la vue.
 *
 * @param evidence_tree_view Vue à actualiser.
 * @param evidence_category_model Modèle de catégories, ou NULL.
 */
void evidence_tree_view_set_model(
    EvidenceTreeView *evidence_tree_view,
    EvidenceCategoryModel *evidence_category_model
);

/**
 * @brief Définit le callback de sélection.
 *
 * @param evidence_tree_view Vue à configurer.
 * @param callback Callback facultatif.
 * @param user_data Données privées transmises au callback.
 */
void evidence_tree_view_set_selection_callback(
    EvidenceTreeView *evidence_tree_view,
    EvidenceTreeViewSelectionCallback callback,
    gpointer user_data
);

/**
 * @brief Libère la structure d'encapsulation.
 *
 * Les widgets GTK intégrés dans la fenêtre restent gérés par l'arbre GTK.
 * Cette fonction accepte NULL.
 *
 * @param evidence_tree_view Vue à libérer.
 */
void evidence_tree_view_free(
    EvidenceTreeView *evidence_tree_view
);

G_END_DECLS

#endif
