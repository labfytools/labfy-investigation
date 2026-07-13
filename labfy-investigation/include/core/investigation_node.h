/******************************************************************************
 * @file investigation_node.h
 * @brief Interface publique d'un nœud de l'arborescence d'une enquête.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_INVESTIGATION_NODE_H
#define LABFY_INVESTIGATION_INVESTIGATION_NODE_H


#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Représentation opaque d'un nœud d'enquête.
 *
 * La structure réelle est définie dans investigation_node.c.
 * Les autres modules manipulent uniquement un pointeur vers
 * InvestigationNode.
 */
typedef struct InvestigationNode InvestigationNode;

/**
 * @brief Type d'un nœud de l'arborescence.
 */
typedef enum
{
    INVESTIGATION_NODE_DIRECTORY,
    INVESTIGATION_NODE_FILE
} InvestigationNodeType;

/**
 * @brief Crée un nouveau nœud d'enquête.
 *
 * Le nom fourni est copié. Le code appelant peut donc modifier ou libérer
 * sa chaîne après l'appel sans affecter le nœud.
 *
 * @param name Nom du fichier ou du dossier.
 * @param type Type du nœud.
 *
 * @return Un nouveau nœud, ou NULL si le nom est invalide ou si la création
 *         échoue.
 */
InvestigationNode *investigation_node_new(
    const char *name,
    InvestigationNodeType type
);

/**
 * @brief Libère les ressources associées à un nœud.
 *
 * Cette fonction accepte NULL.
 *
 * @param node Nœud à libérer.
 */
void investigation_node_free(
    InvestigationNode *node
);

/**
 * @brief Retourne le nom du nœud.
 *
 * La chaîne retournée appartient au nœud et ne doit pas être modifiée
 * ni libérée par le code appelant.
 *
 * @param node Nœud à consulter.
 *
 * @return Le nom en lecture seule, ou NULL si node vaut NULL.
 */
const char *investigation_node_get_name(
    const InvestigationNode *node
);

/**
 * @brief Retourne le type du nœud.
 *
 * @param node Nœud à consulter.
 *
 * @return Le type du nœud.
 */
InvestigationNodeType investigation_node_get_type(
    const InvestigationNode *node
);

/**
 * @brief Ajoute un enfant à un dossier.
 *
 * En cas de succès, le parent devient propriétaire de l'enfant.
 *
 * @return true si l'ajout a réussi.
 */
bool investigation_node_add_child(
    InvestigationNode *parent,
    InvestigationNode *child
);

/**
 * @brief Retourne un enfant.
 *
 * @return L'enfant ou NULL.
 */
const InvestigationNode *investigation_node_get_child(
    const InvestigationNode *node,
    size_t index
);

/**
 * @brief Retourne le nombre d'enfants.
 */
size_t investigation_node_get_children_count(
    const InvestigationNode *node
);

/**
 * @brief Retourne le parent d'un nœud.
 */
const InvestigationNode *investigation_node_get_parent(
    const InvestigationNode *node
);

#endif
