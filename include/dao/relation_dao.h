/******************************************************************************
 * @file relation_dao.h
 * @brief Persistance des relations entre entités dans SQLite.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_RELATION_DAO_H
#define LABFY_INVESTIGATION_RELATION_DAO_H

#include "database/database.h"
#include "models/relation_record.h"

#include <glib.h>

G_BEGIN_DECLS

/**
 * @brief Catégories d'erreurs produites par RelationDao.
 */
typedef enum
{
    RELATION_DAO_ERROR_INVALID_ARGUMENT,
    RELATION_DAO_ERROR_MEMORY,
    RELATION_DAO_ERROR_PREPARE,
    RELATION_DAO_ERROR_BIND,
    RELATION_DAO_ERROR_EXECUTE,
    RELATION_DAO_ERROR_CONSTRAINT,
    RELATION_DAO_ERROR_READ,
    RELATION_DAO_ERROR_MODEL,
    RELATION_DAO_ERROR_SCHEMA,
    RELATION_DAO_ERROR_RANGE
} RelationDaoError;

/**
 * @brief Domaine d'erreur du DAO des relations.
 */
#define RELATION_DAO_ERROR \
    relation_dao_error_quark()

/**
 * @brief Retourne le domaine d'erreur du DAO.
 */
GQuark relation_dao_error_quark(void);

/**
 * @brief DAO opaque donnant accès aux relations persistées.
 *
 * Le DAO emprunte la connexion Database reçue lors de sa création.
 */
typedef struct RelationDao RelationDao;

/**
 * @brief Crée un DAO utilisant une connexion Database existante.
 *
 * La connexion est empruntée et doit rester valide pendant toute
 * la durée de vie du DAO.
 *
 * @param database Connexion SQLite ouverte.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouveau DAO, ou NULL en cas d'échec.
 */
RelationDao *relation_dao_new(
    Database *database,
    GError **error
);

/**
 * @brief Libère le DAO.
 *
 * La connexion Database empruntée n'est pas fermée.
 * Cette fonction accepte NULL.
 *
 * @param relation_dao DAO à libérer.
 */
void relation_dao_free(
    RelationDao *relation_dao
);

/**
 * @brief Insère une nouvelle relation.
 *
 * Aucun enregistrement existant n'est remplacé.
 *
 * @param relation_dao DAO valide.
 * @param relation_record Relation empruntée.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return TRUE si l'insertion réussit.
 */
gboolean relation_dao_insert(
    RelationDao *relation_dao,
    const RelationRecord *relation_record,
    GError **error
);

/**
 * @brief Met à jour les champs modifiables d'une relation existante.
 *
 * L'identifiant, les extrémités et la date de création sont conservés.
 */
gboolean relation_dao_update(
    RelationDao *relation_dao,
    const RelationRecord *relation_record,
    GError **error
);

/**
 * @brief Supprime une relation par son UUID.
 *
 * Les associations dépendantes sont supprimées par les contraintes SQLite.
 * Les entités et les preuves ne sont jamais supprimées.
 *
 * @param relation_dao DAO valide.
 * @param identifier UUID de la relation existante.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return TRUE si la suppression a été exécutée.
 */
gboolean relation_dao_delete(
    RelationDao *relation_dao,
    const char *identifier,
    GError **error
);

/**
 * @brief Recherche une relation par son UUID.
 *
 * Une relation absente retourne NULL sans produire d'erreur.
 *
 * @param relation_dao DAO valide.
 * @param identifier UUID recherché.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouveau modèle possédé par l'appelant, ou NULL.
 */
RelationRecord *relation_dao_find_by_identifier(
    RelationDao *relation_dao,
    const char *identifier,
    GError **error
);

/**
 * @brief Charge toutes les relations dans un ordre déterministe.
 *
 * L'ordre utilisé est :
 *
 * - date de création croissante ;
 * - UUID croissant en cas d'égalité.
 *
 * Le tableau retourné utilise relation_record_free() comme fonction
 * de destruction.
 *
 * @param relation_dao DAO valide.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouveau GPtrArray, ou NULL en cas d'échec.
 */
GPtrArray *relation_dao_list_all(
    RelationDao *relation_dao,
    GError **error
);

/**
 * @brief Compte les relations persistées.
 *
 * @param relation_dao DAO valide.
 * @param out_count Destination du nombre de relations.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return TRUE si le comptage réussit.
 */
gboolean relation_dao_count(
    RelationDao *relation_dao,
    guint64 *out_count,
    GError **error
);

G_END_DECLS

#endif
