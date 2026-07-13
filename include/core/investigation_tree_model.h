/******************************************************************************
 * @file investigation_tree_model.h
 * @brief Interface publique du modèle d'arborescence d'une enquête.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_INVESTIGATION_TREE_MODEL_H
#define LABFY_INVESTIGATION_INVESTIGATION_TREE_MODEL_H

#include "core/investigation_node.h"

/**
 * @brief Représentation opaque du modèle d'arborescence.
 *
 * La structure réelle est définie dans investigation_tree_model.c.
 * Les autres modules manipulent uniquement un pointeur vers
 * InvestigationTreeModel.
 */
typedef struct InvestigationTreeModel InvestigationTreeModel;

/**
 * @brief Crée un nouveau modèle d'arborescence.
 *
 * Le modèle devient propriétaire de root_node après cet appel.
 * Le code appelant ne doit donc plus libérer root_node directement.
 *
 * @param root_node Nœud racine transféré au modèle.
 *
 * @return Un nouveau modèle, ou NULL si root_node vaut NULL
 *         ou si la création échoue.
 */
InvestigationTreeModel *investigation_tree_model_new(
    InvestigationNode *root_node
);

/**
 * @brief Libère le modèle et le nœud racine qu'il possède.
 *
 * Cette fonction accepte NULL.
 *
 * @param tree_model Modèle à libérer.
 */
void investigation_tree_model_free(
    InvestigationTreeModel *tree_model
);

/**
 * @brief Retourne le nœud racine du modèle.
 *
 * Le pointeur retourné appartient toujours au modèle.
 * Il ne doit être ni modifié ni libéré par le code appelant.
 *
 * @param tree_model Modèle à consulter.
 *
 * @return Nœud racine en lecture seule, ou NULL si tree_model vaut NULL.
 */
const InvestigationNode *investigation_tree_model_get_root(
    const InvestigationTreeModel *tree_model
);

#endif
