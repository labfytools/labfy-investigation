/******************************************************************************
 * @file investigation_tree_model.c
 * @brief Implémentation du modèle d'arborescence d'une enquête.
 ******************************************************************************/

#include "core/investigation_tree_model.h"

#include <glib.h>

/**
 * @struct InvestigationTreeModel
 * @brief Représentation interne du modèle.
 */
struct InvestigationTreeModel
{
    InvestigationNode *root_node;
};

InvestigationTreeModel *investigation_tree_model_new(
    InvestigationNode *root_node
)
{
    InvestigationTreeModel *tree_model = NULL;

    if (root_node == NULL)
    {
        return NULL;
    }

    tree_model = g_new0(
        InvestigationTreeModel,
        1
    );

    if (tree_model == NULL)
    {
        return NULL;
    }

    /*
     * Ownership transferred.
     *
     * Le modèle devient propriétaire du nœud racine.
     */
    tree_model->root_node = root_node;

    return tree_model;
}

void investigation_tree_model_free(
    InvestigationTreeModel *tree_model
)
{
    if (tree_model == NULL)
    {
        return;
    }

    investigation_node_free(
        tree_model->root_node
    );

    g_free(tree_model);
}

const InvestigationNode *investigation_tree_model_get_root(
    const InvestigationTreeModel *tree_model
)
{
    if (tree_model == NULL)
    {
        return NULL;
    }

    return tree_model->root_node;
}
