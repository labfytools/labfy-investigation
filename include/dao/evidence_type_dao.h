/******************************************************************************
 * @file evidence_type_dao.h
 * @brief Lecture des types de preuve depuis SQLite.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_EVIDENCE_TYPE_DAO_H
#define LABFY_INVESTIGATION_EVIDENCE_TYPE_DAO_H

#include "database/database.h"

#include <glib.h>

G_BEGIN_DECLS

/**
 * @brief DAO opaque donnant accès aux types de preuve.
 *
 * Le DAO emprunte la connexion Database reçue lors de sa création.
 */
typedef struct EvidenceTypeDao EvidenceTypeDao;

/**
 * @brief Catégories d’erreurs produites par EvidenceTypeDao.
 */
typedef enum
{
    /**
     * Un argument public est invalide.
     */
    EVIDENCE_TYPE_DAO_ERROR_INVALID_ARGUMENT,

    /**
     * L’allocation d’une ressource a échoué.
     */
    EVIDENCE_TYPE_DAO_ERROR_MEMORY,

    /**
     * La requête SQLite n’a pas pu être préparée.
     */
    EVIDENCE_TYPE_DAO_ERROR_PREPARE,

    /**
     * Une ligne SQLite n’a pas pu être lue.
     */
    EVIDENCE_TYPE_DAO_ERROR_READ,

    /**
     * Une ligne SQLite ne peut pas produire un EvidenceType valide.
     */
    EVIDENCE_TYPE_DAO_ERROR_MODEL,

    /**
     * La table ou le schéma attendu est absent ou invalide.
     */
    EVIDENCE_TYPE_DAO_ERROR_SCHEMA
} EvidenceTypeDaoError;

/**
 * @brief Domaine d’erreur du DAO des types de preuve.
 */
#define EVIDENCE_TYPE_DAO_ERROR \
    evidence_type_dao_error_quark()

/**
 * @brief Retourne le domaine d’erreur du DAO.
 *
 * @return Quark GLib associé à EvidenceTypeDao.
 */
GQuark evidence_type_dao_error_quark(void);

/**
 * @brief Crée un DAO utilisant une connexion Database existante.
 *
 * La connexion est empruntée. Elle doit rester valide pendant toute
 * la durée de vie du DAO.
 *
 * @param database Connexion SQLite ouverte.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouveau DAO, ou NULL en cas d’échec.
 */
EvidenceTypeDao *evidence_type_dao_new(
    Database *database,
    GError **error
);

/**
 * @brief Libère le DAO.
 *
 * La connexion Database empruntée n’est pas fermée.
 * Cette fonction accepte NULL.
 *
 * @param evidence_type_dao DAO à libérer.
 */
void evidence_type_dao_free(
    EvidenceTypeDao *evidence_type_dao
);

/**
 * @brief Charge tous les types de preuve.
 *
 * Les types sont triés par identifiant numérique croissant.
 *
 * Le tableau retourné appartient à l’appelant et utilise
 * evidence_type_free() comme fonction de destruction.
 *
 * Une table vide produit un tableau vide, sans erreur.
 *
 * @param evidence_type_dao DAO valide.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouveau GPtrArray, ou NULL en cas d’échec.
 */
GPtrArray *evidence_type_dao_list_all(
    EvidenceTypeDao *evidence_type_dao,
    GError **error
);

G_END_DECLS

#endif
