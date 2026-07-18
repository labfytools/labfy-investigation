/******************************************************************************
 * @file evidence_dao.h
 * @brief Persistance des preuves numériques dans SQLite.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_EVIDENCE_DAO_H
#define LABFY_INVESTIGATION_EVIDENCE_DAO_H

#include "database/database.h"
#include "models/evidence_record.h"

#include <glib.h>

/**
 * @brief Catégories d’erreurs du DAO des preuves.
 */
typedef enum
{
    EVIDENCE_DAO_ERROR_INVALID_ARGUMENT,
    EVIDENCE_DAO_ERROR_MEMORY,
    EVIDENCE_DAO_ERROR_PREPARE,
    EVIDENCE_DAO_ERROR_BIND,
    EVIDENCE_DAO_ERROR_EXECUTE,
    EVIDENCE_DAO_ERROR_CONSTRAINT,
    EVIDENCE_DAO_ERROR_READ,
    EVIDENCE_DAO_ERROR_MODEL,
    EVIDENCE_DAO_ERROR_SCHEMA,
    EVIDENCE_DAO_ERROR_RANGE
} EvidenceDaoError;

/**
 * @brief Domaine d’erreurs du DAO des preuves.
 */
#define EVIDENCE_DAO_ERROR \
    evidence_dao_error_quark()

/**
 * @brief Retourne le domaine d’erreurs du DAO des preuves.
 */
GQuark evidence_dao_error_quark(void);

/**
 * @brief DAO opaque donnant accès aux preuves persistées.
 *
 * L’objet emprunte la connexion Database reçue lors de sa création.
 */
typedef struct EvidenceDao EvidenceDao;

/**
 * @brief Crée un DAO utilisant une connexion Database existante.
 *
 * La connexion est empruntée et doit rester valide pendant toute la durée
 * de vie du DAO.
 *
 * @param database Connexion ouverte.
 * @param error Adresse recevant une éventuelle erreur.
 *
 * @return Nouveau DAO, ou NULL en cas d’échec.
 */
EvidenceDao *evidence_dao_new(
    Database *database,
    GError **error
);

/**
 * @brief Libère le DAO.
 *
 * La connexion Database empruntée n’est pas fermée.
 * Cette fonction accepte NULL.
 *
 * @param evidence_dao DAO à libérer.
 */
void evidence_dao_free(
    EvidenceDao *evidence_dao
);

/**
 * @brief Insère une nouvelle preuve.
 *
 * Aucun enregistrement existant n’est remplacé ou modifié.
 *
 * @param evidence_dao DAO valide.
 * @param evidence_record Preuve empruntée.
 * @param error Adresse recevant une éventuelle erreur.
 *
 * @return TRUE si l’insertion réussit, sinon FALSE.
 */
gboolean evidence_dao_insert(
    EvidenceDao *evidence_dao,
    const EvidenceRecord *evidence_record,
    GError **error
);

/**
 * @brief Recherche une preuve par son identifiant.
 *
 * Une preuve absente retourne NULL sans produire d’erreur.
 *
 * @param evidence_dao DAO valide.
 * @param identifier Identifiant UUID recherché.
 * @param error Adresse recevant une éventuelle erreur.
 *
 * @return Nouveau modèle possédé par l’appelant, ou NULL.
 */
EvidenceRecord *evidence_dao_find_by_identifier(
    EvidenceDao *evidence_dao,
    const char *identifier,
    GError **error
);

/**
 * @brief Charge toutes les preuves dans un ordre déterministe.
 *
 * Le tableau retourné utilise evidence_record_free() comme fonction
 * de destruction.
 *
 * @param evidence_dao DAO valide.
 * @param error Adresse recevant une éventuelle erreur.
 *
 * @return Nouveau GPtrArray, ou NULL en cas d’échec.
 */
GPtrArray *evidence_dao_list_all(
    EvidenceDao *evidence_dao,
    GError **error
);

/**
 * @brief Compte les preuves persistées.
 *
 * @param evidence_dao DAO valide.
 * @param out_count Destination du nombre de preuves.
 * @param error Adresse recevant une éventuelle erreur.
 *
 * @return TRUE si le comptage réussit, sinon FALSE.
 */
gboolean evidence_dao_count(
    EvidenceDao *evidence_dao,
    guint64 *out_count,
    GError **error
);

#endif
