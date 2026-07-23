/******************************************************************************
 * @file graph_node_position_dao.h
 * @brief Persistance des positions des nœuds du graphe dans SQLite.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_GRAPH_NODE_POSITION_DAO_H
#define LABFY_INVESTIGATION_GRAPH_NODE_POSITION_DAO_H

#include "database/database.h"
#include "models/graph_node_position.h"

#include <glib.h>

G_BEGIN_DECLS

/**
 * @brief Catégories d'erreurs du DAO des positions de nœuds.
 */
typedef enum
{
    GRAPH_NODE_POSITION_DAO_ERROR_INVALID_ARGUMENT,
    GRAPH_NODE_POSITION_DAO_ERROR_MEMORY,
    GRAPH_NODE_POSITION_DAO_ERROR_PREPARE,
    GRAPH_NODE_POSITION_DAO_ERROR_BIND,
    GRAPH_NODE_POSITION_DAO_ERROR_EXECUTE,
    GRAPH_NODE_POSITION_DAO_ERROR_CONSTRAINT,
    GRAPH_NODE_POSITION_DAO_ERROR_READ,
    GRAPH_NODE_POSITION_DAO_ERROR_MODEL
} GraphNodePositionDaoError;

/**
 * @brief Domaine d'erreurs du DAO des positions de nœuds.
 */
#define GRAPH_NODE_POSITION_DAO_ERROR \
    graph_node_position_dao_error_quark()

/**
 * @brief Retourne le domaine d'erreurs du DAO.
 */
GQuark graph_node_position_dao_error_quark(void);

/**
 * @brief DAO opaque donnant accès aux positions persistées.
 *
 * L'objet emprunte la connexion Database reçue lors de sa création.
 */
typedef struct GraphNodePositionDao GraphNodePositionDao;

/**
 * @brief Crée un DAO utilisant une connexion Database existante.
 *
 * La connexion est empruntée et doit rester valide pendant toute la durée
 * de vie du DAO.
 *
 * @param database Connexion ouverte.
 * @param error Adresse recevant une éventuelle erreur.
 *
 * @return Nouveau DAO, ou NULL en cas d'échec.
 */
GraphNodePositionDao *graph_node_position_dao_new(
    Database *database,
    GError **error
);

/**
 * @brief Libère le DAO.
 *
 * La connexion Database empruntée n'est pas fermée.
 * Cette fonction accepte NULL.
 *
 * @param position_dao DAO à libérer.
 */
void graph_node_position_dao_free(
    GraphNodePositionDao *position_dao
);

/**
 * @brief Charge toutes les positions dans un ordre déterministe.
 *
 * Le tableau retourné utilise graph_node_position_free() comme fonction
 * de destruction.
 *
 * @param position_dao DAO valide.
 * @param error Adresse recevant une éventuelle erreur.
 *
 * @return Nouveau GPtrArray possédé par l'appelant, ou NULL.
 */
GPtrArray *graph_node_position_dao_list_all(
    GraphNodePositionDao *position_dao,
    GError **error
);

/**
 * @brief Insère ou met à jour la position d'un nœud.
 *
 * La date updated_at est générée en UTC par le DAO.
 *
 * @param position_dao DAO valide.
 * @param node_identifier UUID du nœud (entité ou relation).
 * @param x Coordonnée horizontale logique.
 * @param y Coordonnée verticale logique.
 * @param error Adresse recevant une éventuelle erreur.
 *
 * @return TRUE si l'enregistrement réussit, sinon FALSE.
 */
gboolean graph_node_position_dao_upsert(
    GraphNodePositionDao *position_dao,
    const char *node_identifier,
    double x,
    double y,
    GError **error
);

/**
 * @brief Supprime la position persistée d'un nœud.
 *
 * L'absence de position n'est pas une erreur.
 *
 * @param position_dao DAO valide.
 * @param node_identifier UUID du nœud (entité ou relation).
 * @param error Adresse recevant une éventuelle erreur.
 *
 * @return TRUE si la requête réussit, sinon FALSE.
 */
gboolean graph_node_position_dao_delete(
    GraphNodePositionDao *position_dao,
    const char *node_identifier,
    GError **error
);

/**
 * @brief Supprime toutes les positions persistées du graphe.
 *
 * Une table déjà vide n'est pas une erreur.
 *
 * @param position_dao DAO valide.
 * @param error Adresse recevant une éventuelle erreur.
 *
 * @return TRUE si la requête réussit, sinon FALSE.
 */
gboolean graph_node_position_dao_delete_all(
    GraphNodePositionDao *position_dao,
    GError **error
);

gboolean graph_node_position_dao_load_viewport(
    GraphNodePositionDao *position_dao,
    double *zoom,
    double *offset_x,
    double *offset_y,
    gboolean *found,
    GError **error
);

gboolean graph_node_position_dao_upsert_viewport(
    GraphNodePositionDao *position_dao,
    double zoom,
    double offset_x,
    double offset_y,
    GError **error
);

G_END_DECLS

#endif
