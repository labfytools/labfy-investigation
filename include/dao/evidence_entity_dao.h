/******************************************************************************
 * @file evidence_entity_dao.h
 * @brief Persistance des associations entre preuves et entités.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_EVIDENCE_ENTITY_DAO_H
#define LABFY_INVESTIGATION_EVIDENCE_ENTITY_DAO_H

#include "database/database.h"

#include <glib.h>

G_BEGIN_DECLS

/**
 * @brief Catégories d'erreurs produites par EvidenceEntityDao.
 */
typedef enum
{
    /**
     * Un argument public est invalide.
     */
    EVIDENCE_ENTITY_DAO_ERROR_INVALID_ARGUMENT,

    /**
     * L'allocation d'une ressource a échoué.
     */
    EVIDENCE_ENTITY_DAO_ERROR_MEMORY,

    /**
     * La requête SQLite n'a pas pu être préparée.
     */
    EVIDENCE_ENTITY_DAO_ERROR_PREPARE,

    /**
     * Un paramètre n'a pas pu être lié à la requête.
     */
    EVIDENCE_ENTITY_DAO_ERROR_BIND,

    /**
     * La requête SQLite n'a pas pu être exécutée.
     */
    EVIDENCE_ENTITY_DAO_ERROR_EXECUTE,

    /**
     * Une contrainte SQLite a été violée.
     */
    EVIDENCE_ENTITY_DAO_ERROR_CONSTRAINT,

    /**
     * Un enregistrement ou une association demandée n'existe pas.
     */
    EVIDENCE_ENTITY_DAO_ERROR_NOT_FOUND,

    /**
     * Une ligne SQLite n'a pas pu être lue.
     */
    EVIDENCE_ENTITY_DAO_ERROR_READ,

    /**
     * La table ou une colonne attendue est absente.
     */
    EVIDENCE_ENTITY_DAO_ERROR_SCHEMA
} EvidenceEntityDaoError;

/**
 * @brief Domaine d'erreur du DAO.
 */
#define EVIDENCE_ENTITY_DAO_ERROR \
    evidence_entity_dao_error_quark()

/**
 * @brief DAO opaque des associations entre preuves et entités.
 *
 * Le DAO emprunte la connexion Database reçue lors de sa création.
 */
typedef struct EvidenceEntityDao EvidenceEntityDao;

/**
 * @brief Retourne le domaine d'erreur du DAO.
 */
GQuark evidence_entity_dao_error_quark(void);

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
EvidenceEntityDao *evidence_entity_dao_new(
    Database *database,
    GError **error
);

/**
 * @brief Libère le DAO.
 *
 * La connexion Database empruntée n'est pas fermée.
 * Cette fonction accepte NULL.
 *
 * @param evidence_entity_dao DAO à libérer.
 */
void evidence_entity_dao_free(
    EvidenceEntityDao *evidence_entity_dao
);

/**
 * @brief Associe une preuve à une entité.
 *
 * Les deux UUID doivent correspondre à des enregistrements existants.
 * Une association déjà existante produit une erreur de contrainte.
 *
 * @param evidence_entity_dao DAO valide.
 * @param evidence_identifier UUID de la preuve.
 * @param entity_identifier UUID de l'entité.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return TRUE si l'association est créée.
 */
gboolean evidence_entity_dao_link(
    EvidenceEntityDao *evidence_entity_dao,
    const char *evidence_identifier,
    const char *entity_identifier,
    GError **error
);

/**
 * @brief Supprime une association entre une preuve et une entité.
 *
 * Une association absente produit EVIDENCE_ENTITY_DAO_ERROR_NOT_FOUND.
 *
 * Les enregistrements preuve et entité ne sont jamais supprimés.
 *
 * @param evidence_entity_dao DAO valide.
 * @param evidence_identifier UUID de la preuve.
 * @param entity_identifier UUID de l'entité.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return TRUE si l'association est supprimée.
 */
gboolean evidence_entity_dao_unlink(
    EvidenceEntityDao *evidence_entity_dao,
    const char *evidence_identifier,
    const char *entity_identifier,
    GError **error
);

/**
 * @brief Vérifie si une association existe.
 *
 * @param evidence_entity_dao DAO valide.
 * @param evidence_identifier UUID de la preuve.
 * @param entity_identifier UUID de l'entité.
 * @param out_exists Destination du résultat.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return TRUE si la recherche a été exécutée correctement.
 */
gboolean evidence_entity_dao_exists(
    EvidenceEntityDao *evidence_entity_dao,
    const char *evidence_identifier,
    const char *entity_identifier,
    gboolean *out_exists,
    GError **error
);

/**
 * @brief Liste les UUID des entités liées à une preuve.
 *
 * Les identifiants sont triés par ordre lexicographique croissant.
 *
 * Le tableau et les chaînes qu'il contient appartiennent à l'appelant.
 * Le tableau utilise g_free() comme fonction de destruction.
 *
 * Une preuve sans association produit un tableau vide.
 *
 * @param evidence_entity_dao DAO valide.
 * @param evidence_identifier UUID de la preuve.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouveau GPtrArray de chaînes, ou NULL en cas d'échec.
 */
GPtrArray *evidence_entity_dao_list_entity_identifiers(
    EvidenceEntityDao *evidence_entity_dao,
    const char *evidence_identifier,
    GError **error
);

/**
 * @brief Liste les UUID des preuves liées à une entité.
 *
 * Les identifiants sont triés par ordre lexicographique croissant.
 *
 * Le tableau et les chaînes qu'il contient appartiennent à l'appelant.
 * Le tableau utilise g_free() comme fonction de destruction.
 *
 * Une entité sans association produit un tableau vide.
 *
 * @param evidence_entity_dao DAO valide.
 * @param entity_identifier UUID de l'entité.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouveau GPtrArray de chaînes, ou NULL en cas d'échec.
 */
GPtrArray *evidence_entity_dao_list_evidence_identifiers(
    EvidenceEntityDao *evidence_entity_dao,
    const char *entity_identifier,
    GError **error
);

G_END_DECLS

#endif
