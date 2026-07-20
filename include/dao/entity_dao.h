/******************************************************************************
 * @file entity_dao.h
 * @brief Persistance des entités OSINT dans SQLite.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_ENTITY_DAO_H
#define LABFY_INVESTIGATION_ENTITY_DAO_H

#include "database/database.h"
#include "models/entity_record.h"

#include <glib.h>

G_BEGIN_DECLS

/**
 * @brief Catégories d'erreurs produites par EntityDao.
 */
typedef enum
{
    ENTITY_DAO_ERROR_INVALID_ARGUMENT,
    ENTITY_DAO_ERROR_MEMORY,
    ENTITY_DAO_ERROR_PREPARE,
    ENTITY_DAO_ERROR_BIND,
    ENTITY_DAO_ERROR_EXECUTE,
    ENTITY_DAO_ERROR_CONSTRAINT,
    ENTITY_DAO_ERROR_READ,
    ENTITY_DAO_ERROR_MODEL,
    ENTITY_DAO_ERROR_SCHEMA,
    ENTITY_DAO_ERROR_RANGE
} EntityDaoError;

/**
 * @brief Domaine d'erreur du DAO des entités.
 */
#define ENTITY_DAO_ERROR \
    entity_dao_error_quark()

/**
 * @brief Retourne le domaine d'erreur du DAO.
 */
GQuark entity_dao_error_quark(void);

/**
 * @brief DAO opaque donnant accès aux entités persistées.
 *
 * Le DAO emprunte la connexion Database reçue lors de sa création.
 */
typedef struct EntityDao EntityDao;

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
EntityDao *entity_dao_new(
    Database *database,
    GError **error
);

/**
 * @brief Libère le DAO.
 *
 * La connexion Database empruntée n'est pas fermée.
 * Cette fonction accepte NULL.
 *
 * @param entity_dao DAO à libérer.
 */
void entity_dao_free(
    EntityDao *entity_dao
);

/**
 * @brief Insère une nouvelle entité.
 *
 * Aucun enregistrement existant n'est remplacé.
 *
 * @param entity_dao DAO valide.
 * @param entity_record Entité empruntée.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return TRUE si l'insertion réussit.
 */
gboolean entity_dao_insert(
    EntityDao *entity_dao,
    const EntityRecord *entity_record,
    GError **error
);

/**
 * @brief Recherche une entité par son UUID.
 *
 * Une entité absente retourne NULL sans produire d'erreur.
 *
 * @param entity_dao DAO valide.
 * @param identifier UUID recherché.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouveau modèle possédé par l'appelant, ou NULL.
 */
EntityRecord *entity_dao_find_by_identifier(
    EntityDao *entity_dao,
    const char *identifier,
    GError **error
);

/**
 * @brief Charge toutes les entités dans un ordre déterministe.
 *
 * L'ordre utilisé est :
 *
 * - date de création croissante ;
 * - UUID croissant en cas d'égalité.
 *
 * Le tableau retourné utilise entity_record_free() comme fonction
 * de destruction.
 *
 * @param entity_dao DAO valide.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouveau GPtrArray, ou NULL en cas d'échec.
 */
GPtrArray *entity_dao_list_all(
    EntityDao *entity_dao,
    GError **error
);

/**
 * @brief Compte les entités persistées.
 *
 * @param entity_dao DAO valide.
 * @param out_count Destination du nombre d'entités.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return TRUE si le comptage réussit.
 */
gboolean entity_dao_count(
    EntityDao *entity_dao,
    guint64 *out_count,
    GError **error
);

G_END_DECLS

#endif
