/******************************************************************************
 * @file relation_evidence_dao.h
 * @brief Persistance des associations entre relations et preuves.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_RELATION_EVIDENCE_DAO_H
#define LABFY_INVESTIGATION_RELATION_EVIDENCE_DAO_H

#include "database/database.h"

#include <glib.h>

G_BEGIN_DECLS

/**
 * @brief DAO opaque des associations entre relations et preuves.
 *
 * Le DAO emprunte la connexion Database reçue lors de sa création.
 */
typedef struct RelationEvidenceDao RelationEvidenceDao;

/**
 * @brief Catégories d'erreurs produites par RelationEvidenceDao.
 */
typedef enum
{
    /**
     * Un argument public est invalide.
     */
    RELATION_EVIDENCE_DAO_ERROR_INVALID_ARGUMENT,

    /**
     * L'allocation d'une ressource a échoué.
     */
    RELATION_EVIDENCE_DAO_ERROR_MEMORY,

    /**
     * La requête SQLite n'a pas pu être préparée.
     */
    RELATION_EVIDENCE_DAO_ERROR_PREPARE,

    /**
     * Un paramètre n'a pas pu être lié à la requête.
     */
    RELATION_EVIDENCE_DAO_ERROR_BIND,

    /**
     * La requête SQLite n'a pas pu être exécutée.
     */
    RELATION_EVIDENCE_DAO_ERROR_EXECUTE,

    /**
     * Une contrainte SQLite a été violée.
     */
    RELATION_EVIDENCE_DAO_ERROR_CONSTRAINT,

    /**
     * Un enregistrement ou une association demandée n'existe pas.
     */
    RELATION_EVIDENCE_DAO_ERROR_NOT_FOUND,

    /**
     * Une ligne SQLite n'a pas pu être lue.
     */
    RELATION_EVIDENCE_DAO_ERROR_READ,

    /**
     * La table ou une colonne attendue est absente.
     */
    RELATION_EVIDENCE_DAO_ERROR_SCHEMA
} RelationEvidenceDaoError;

/**
 * @brief Domaine d'erreur du DAO.
 */
#define RELATION_EVIDENCE_DAO_ERROR \
    relation_evidence_dao_error_quark()

/**
 * @brief Retourne le domaine d'erreur du DAO.
 */
GQuark relation_evidence_dao_error_quark(void);

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
RelationEvidenceDao *relation_evidence_dao_new(
    Database *database,
    GError **error
);

/**
 * @brief Libère le DAO.
 *
 * La connexion Database empruntée n'est pas fermée.
 * Cette fonction accepte NULL.
 *
 * @param relation_evidence_dao DAO à libérer.
 */
void relation_evidence_dao_free(
    RelationEvidenceDao *relation_evidence_dao
);

/**
 * @brief Associe une relation à une preuve.
 *
 * Les deux UUID doivent correspondre à des enregistrements existants.
 * Une association déjà existante produit une erreur de contrainte.
 *
 * @param relation_evidence_dao DAO valide.
 * @param relation_identifier UUID de la relation.
 * @param evidence_identifier UUID de la preuve.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return TRUE si l'association est créée.
 */
gboolean relation_evidence_dao_link(
    RelationEvidenceDao *relation_evidence_dao,
    const char *relation_identifier,
    const char *evidence_identifier,
    GError **error
);

/**
 * @brief Supprime une association entre une relation et une preuve.
 *
 * Une association absente produit RELATION_EVIDENCE_DAO_ERROR_NOT_FOUND.
 *
 * La relation et la preuve ne sont jamais supprimées.
 *
 * @param relation_evidence_dao DAO valide.
 * @param relation_identifier UUID de la relation.
 * @param evidence_identifier UUID de la preuve.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return TRUE si l'association est supprimée.
 */
gboolean relation_evidence_dao_unlink(
    RelationEvidenceDao *relation_evidence_dao,
    const char *relation_identifier,
    const char *evidence_identifier,
    GError **error
);

/**
 * @brief Vérifie si une association existe.
 *
 * @param relation_evidence_dao DAO valide.
 * @param relation_identifier UUID de la relation.
 * @param evidence_identifier UUID de la preuve.
 * @param out_exists Destination du résultat.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return TRUE si la recherche a été exécutée correctement.
 */
gboolean relation_evidence_dao_exists(
    RelationEvidenceDao *relation_evidence_dao,
    const char *relation_identifier,
    const char *evidence_identifier,
    gboolean *out_exists,
    GError **error
);

/**
 * @brief Liste les UUID des preuves liées à une relation.
 *
 * Les identifiants sont triés par ordre lexicographique croissant.
 *
 * Le tableau et les chaînes qu'il contient appartiennent à l'appelant.
 * Le tableau utilise g_free() comme fonction de destruction.
 *
 * Une relation sans association produit un tableau vide.
 *
 * @param relation_evidence_dao DAO valide.
 * @param relation_identifier UUID de la relation.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouveau GPtrArray de chaînes, ou NULL en cas d'échec.
 */
GPtrArray *relation_evidence_dao_list_evidence_identifiers(
    RelationEvidenceDao *relation_evidence_dao,
    const char *relation_identifier,
    GError **error
);

/**
 * @brief Liste les UUID des relations liées à une preuve.
 *
 * Les identifiants sont triés par ordre lexicographique croissant.
 *
 * Le tableau et les chaînes qu'il contient appartiennent à l'appelant.
 * Le tableau utilise g_free() comme fonction de destruction.
 *
 * Une preuve sans association produit un tableau vide.
 *
 * @param relation_evidence_dao DAO valide.
 * @param evidence_identifier UUID de la preuve.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouveau GPtrArray de chaînes, ou NULL en cas d'échec.
 */
GPtrArray *relation_evidence_dao_list_relation_identifiers(
    RelationEvidenceDao *relation_evidence_dao,
    const char *evidence_identifier,
    GError **error
);

G_END_DECLS

#endif
