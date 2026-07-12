/******************************************************************************
 * @file investigation_node.c
 * @brief Implémentation d'un nœud d'enquête.
 ******************************************************************************/

#include "core/investigation_node.h"

#include <glib.h>

/**
 * @struct InvestigationNode
 * @brief Représentation interne d'un nœud.
 */
struct InvestigationNode
{
    char *name;
    InvestigationNodeType type;
};

InvestigationNode *investigation_node_new(
    const char *name,
    InvestigationNodeType type
)
{
    InvestigationNode *node = NULL;

    if (name == NULL || name[0] == '\0')
    {
        return NULL;
    }

    node = g_new0(
        InvestigationNode,
        1
    );

    if (node == NULL)
    {
        return NULL;
    }

    node->name = g_strdup(name);

    if (node->name == NULL)
    {
        investigation_node_free(node);
        return NULL;
    }

    node->type = type;

    return node;
}

void investigation_node_free(
    InvestigationNode *node
)
{
    if (node == NULL)
    {
        return;
    }

    g_free(node->name);

    g_free(node);
}

const char *investigation_node_get_name(
    const InvestigationNode *node
)
{
    if (node == NULL)
    {
        return NULL;
    }

    return node->name;
}

InvestigationNodeType investigation_node_get_type(
    const InvestigationNode *node
)
{
    if (node == NULL)
    {
        return INVESTIGATION_NODE_FILE;
    }

    return node->type;
}
