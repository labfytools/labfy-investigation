/******************************************************************************
 * @file graph_node_position.h
 * @brief Modèle représentant la position persistée d'un nœud du graphe.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_GRAPH_NODE_POSITION_H
#define LABFY_INVESTIGATION_GRAPH_NODE_POSITION_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * @brief Modèle opaque représentant une position de nœud.
 */
typedef struct GraphNodePosition GraphNodePosition;

/**
 * @brief Codes d'erreur produits par GraphNodePosition.
 */
typedef enum
{
    GRAPH_NODE_POSITION_ERROR_INVALID_ARGUMENT,
    GRAPH_NODE_POSITION_ERROR_INVALID_IDENTIFIER,
    GRAPH_NODE_POSITION_ERROR_INVALID_COORDINATE,
    GRAPH_NODE_POSITION_ERROR_INVALID_DATE
} GraphNodePositionError;

/**
 * @brief Domaine d'erreur du modèle GraphNodePosition.
 */
#define GRAPH_NODE_POSITION_ERROR \
    graph_node_position_error_quark()

/**
 * @brief Retourne le domaine d'erreur du modèle.
 */
GQuark graph_node_position_error_quark(void);

/**
 * @brief Crée une position persistée de nœud.
 *
 * Toutes les chaînes sont copiées.
 *
 * entity_identifier doit être un UUID valide.
 * x et y doivent être des nombres finis.
 * updated_at doit respecter le format UTC YYYY-MM-DDTHH:MM:SSZ.
 *
 * @param entity_identifier UUID de l'entité.
 * @param x Coordonnée horizontale logique.
 * @param y Coordonnée verticale logique.
 * @param updated_at Date UTC de dernière modification.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouvelle position, ou NULL lorsque les données sont invalides.
 */
GraphNodePosition *graph_node_position_new(
    const char *entity_identifier,
    double x,
    double y,
    const char *updated_at,
    GError **error
);

/**
 * @brief Libère une position.
 *
 * Cette fonction accepte NULL.
 *
 * @param position Position à libérer.
 */
void graph_node_position_free(
    GraphNodePosition *position
);

/**
 * @brief Retourne l'UUID de l'entité.
 *
 * La chaîne retournée appartient au modèle.
 *
 * @param position Position à consulter.
 *
 * @return UUID emprunté, ou NULL.
 */
const char *graph_node_position_get_entity_identifier(
    const GraphNodePosition *position
);

/**
 * @brief Retourne la coordonnée horizontale.
 *
 * @param position Position à consulter.
 *
 * @return Coordonnée horizontale, ou 0.0 si position est NULL.
 */
double graph_node_position_get_x(
    const GraphNodePosition *position
);

/**
 * @brief Retourne la coordonnée verticale.
 *
 * @param position Position à consulter.
 *
 * @return Coordonnée verticale, ou 0.0 si position est NULL.
 */
double graph_node_position_get_y(
    const GraphNodePosition *position
);

/**
 * @brief Retourne la date UTC de dernière modification.
 *
 * La chaîne retournée appartient au modèle.
 *
 * @param position Position à consulter.
 *
 * @return Date empruntée, ou NULL.
 */
const char *graph_node_position_get_updated_at(
    const GraphNodePosition *position
);

G_END_DECLS

#endif
