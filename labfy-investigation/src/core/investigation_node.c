/******************************************************************************
 * @file investigation_node.c
 * @brief Implémentation d'un nœud d'enquête.
 ******************************************************************************/

#include "core/investigation_node.h"

#include <glib.h>

/**
 * @struct InvestigationNode
 * @brief Représentation interne d'un nœud.
 *
 * Le nœud possède son nom et ses enfants.
 * Le pointeur parent est une simple référence non propriétaire.
 */
struct InvestigationNode
{
    char *name;
    InvestigationNodeType type;
    InvestigationNode *parent;
    GPtrArray *children;
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

    node = g_new0(InvestigationNode, 1);

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

    node->children = g_ptr_array_new_with_free_func(
        (GDestroyNotify) investigation_node_free
    );

    if (node->children == NULL)
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

    /*
     * Le tableau possède les enfants.
     * TRUE demande à GLib de libérer aussi le contenu du tableau.
     */
    if (node->children != NULL)
    {
        g_ptr_array_free(node->children, TRUE);
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

bool investigation_node_add_child(
    InvestigationNode *parent,
    InvestigationNode *child
)
{
    if (parent == NULL || child == NULL)
    {
        return false;
    }

    if (parent->type != INVESTIGATION_NODE_DIRECTORY)
    {
        return false;
    }

    if (parent == child)
    {
        return false;
    }

    if (child->parent != NULL)
    {
        return false;
    }

    child->parent = parent;

    g_ptr_array_add(
        parent->children,
        child
    );

    return true;
}

const InvestigationNode *investigation_node_get_child(
    const InvestigationNode *node,
    size_t index
)
{
    if (node == NULL || node->children == NULL)
    {
        return NULL;
    }

    if (index >= node->children->len)
    {
        return NULL;
    }

    return g_ptr_array_index(
        node->children,
        index
    );
}

size_t investigation_node_get_children_count(
    const InvestigationNode *node
)
{
    if (node == NULL || node->children == NULL)
    {
        return 0;
    }

    return node->children->len;
}

const InvestigationNode *investigation_node_get_parent(
    const InvestigationNode *node
)
{
    if (node == NULL)
    {
        return NULL;
    }

    return node->parent;
}
