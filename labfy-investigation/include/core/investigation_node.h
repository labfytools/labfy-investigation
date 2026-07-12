/******************************************************************************
 * @file investigation_node.h
 * @brief Interface publique d'un nœud de l'arborescence d'une enquête.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_INVESTIGATION_NODE_H
#define LABFY_INVESTIGATION_INVESTIGATION_NODE_H

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

#endif
