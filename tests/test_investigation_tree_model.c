/******************************************************************************
 * @file test_investigation_tree_model.c
 * @brief Tests du module InvestigationTreeModel.
 ******************************************************************************/

#include "core/investigation_node.h"
#include "core/investigation_tree_model.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Vérifie la création d'un modèle valide.
 */
static void test_tree_model_creation(void)
{
    InvestigationNode *root_node = NULL;
    InvestigationTreeModel *tree_model = NULL;
    const InvestigationNode *returned_node = NULL;

    root_node = investigation_node_new(
        "Template",
        "/test/Template",
        INVESTIGATION_NODE_DIRECTORY
    );

    assert(root_node != NULL);

    tree_model = investigation_tree_model_new(root_node);

    assert(tree_model != NULL);

    returned_node = investigation_tree_model_get_root(tree_model);

    assert(returned_node != NULL);

    assert(
        strcmp(
            investigation_node_get_name(returned_node),
            "Template"
        ) == 0
    );

    assert(
        strcmp(
            investigation_node_get_path(returned_node),
            "/test/Template"
        ) == 0
    );

    assert(
        investigation_node_get_type(returned_node) ==
        INVESTIGATION_NODE_DIRECTORY
    );

    /*
     * tree_model est propriétaire de root_node.
     */
    investigation_tree_model_free(tree_model);
}

/**
 * @brief Vérifie les paramètres invalides.
 */
static void test_invalid_parameters(void)
{
    assert(
        investigation_tree_model_new(NULL) == NULL
    );

    assert(
        investigation_tree_model_get_root(NULL) == NULL
    );
}

/**
 * @brief Vérifie que free(NULL) est accepté.
 */
static void test_null_free(void)
{
    investigation_tree_model_free(NULL);
}

int main(void)
{
    test_tree_model_creation();

    test_invalid_parameters();

    test_null_free();

    printf(
        "InvestigationTreeModel : tous les tests sont valides.\n"
    );

    return 0;
}
