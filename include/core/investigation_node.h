/******************************************************************************
 * @file investigation_node.h
 * @brief Interface publique d'un nœud de l'arborescence d'une enquête.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_INVESTIGATION_NODE_H
#define LABFY_INVESTIGATION_INVESTIGATION_NODE_H

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Représentation opaque d'un nœud.
 */
typedef struct InvestigationNode InvestigationNode;

/**
 * @brief Type d'un nœud.
 */
typedef enum
{
    INVESTIGATION_NODE_DIRECTORY,
    INVESTIGATION_NODE_FILE
} InvestigationNodeType;

/**
 * @brief Crée un nouveau nœud.
 *
 * Le nom et le chemin sont copiés.
 * Le code appelant peut donc modifier ou libérer ses chaînes après l'appel.
 *
 * @param name Nom du fichier ou du dossier.
 * @param path Chemin complet du fichier ou du dossier.
 * @param type Type du nœud.
 *
 * @return Un nouveau nœud, ou NULL si le nom ou le chemin est invalide,
 *         ou si une allocation échoue.
 */
InvestigationNode *investigation_node_new(
    const char *name,
    const char *path,
    InvestigationNodeType type
);

/**
 * @brief Libère un nœud et tous ses enfants.
 *
 * Cette fonction accepte NULL.
 *
 * @param node Nœud à libérer.
 */
void investigation_node_free(
    InvestigationNode *node
);

/**
 * @brief Retourne le nom d'un nœud.
 *
 * La chaîne retournée appartient au nœud.
 * Elle ne doit être ni modifiée ni libérée.
 *
 * @param node Nœud à consulter.
 *
 * @return Le nom du nœud, ou NULL si node vaut NULL.
 */
const char *investigation_node_get_name(
    const InvestigationNode *node
);

/**
 * @brief Retourne le chemin complet d'un nœud.
 *
 * La chaîne retournée appartient au nœud.
 * Elle ne doit être ni modifiée ni libérée.
 *
 * @param node Nœud à consulter.
 *
 * @return Le chemin complet, ou NULL si node vaut NULL.
 */
const char *investigation_node_get_path(
    const InvestigationNode *node
);

/**
 * @brief Retourne le type d'un nœud.
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
 * @param parent Nœud parent.
 * @param child  Nœud enfant.
 *
 * @return true si l'ajout a réussi, sinon false.
 */
bool investigation_node_add_child(
    InvestigationNode *parent,
    InvestigationNode *child
);

/**
 * @brief Retourne un enfant par son index.
 *
 * @param node  Nœud parent.
 * @param index Index de l'enfant.
 *
 * @return L'enfant demandé, ou NULL si l'index est invalide.
 */
const InvestigationNode *investigation_node_get_child(
    const InvestigationNode *node,
    size_t index
);

/**
 * @brief Retourne le nombre d'enfants.
 *
 * @param node Nœud à consulter.
 *
 * @return Le nombre d'enfants, ou 0 si node vaut NULL.
 */
size_t investigation_node_get_children_count(
    const InvestigationNode *node
);

/**
 * @brief Retourne le parent d'un nœud.
 *
 * @param node Nœud à consulter.
 *
 * @return Le parent, ou NULL si le nœud n'a pas de parent
 *         ou si node vaut NULL.
 */
const InvestigationNode *investigation_node_get_parent(
    const InvestigationNode *node
);

#endif
