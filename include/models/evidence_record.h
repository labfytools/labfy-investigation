/******************************************************************************
 * @file evidence_record.h
 * @brief Modèle représentant une preuve numérique persistée.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_EVIDENCE_RECORD_H
#define LABFY_INVESTIGATION_EVIDENCE_RECORD_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * @brief Modèle opaque représentant une preuve numérique.
 */
typedef struct EvidenceRecord EvidenceRecord;

/**
 * @brief État connu de l’intégrité d’une preuve.
 */
typedef enum
{
    /**
     * Aucune vérification d’intégrité n’a encore été effectuée.
     */
    EVIDENCE_INTEGRITY_STATUS_UNKNOWN,

    /**
     * Le fichier existe et son empreinte est conforme.
     */
    EVIDENCE_INTEGRITY_STATUS_VALID,

    /**
     * Le fichier attendu est absent.
     */
    EVIDENCE_INTEGRITY_STATUS_MISSING,

    /**
     * Le fichier existe, mais son contenu a changé.
     */
    EVIDENCE_INTEGRITY_STATUS_MODIFIED,

    /**
     * La vérification d’intégrité n’a pas pu être réalisée.
     */
    EVIDENCE_INTEGRITY_STATUS_ERROR
} EvidenceIntegrityStatus;

/**
 * @brief Codes d’erreur produits par EvidenceRecord.
 */
typedef enum
{
    /**
     * Un argument général est invalide.
     */
    EVIDENCE_RECORD_ERROR_INVALID_ARGUMENT,

    /**
     * L’identifiant UUID est invalide.
     */
    EVIDENCE_RECORD_ERROR_INVALID_IDENTIFIER,

    /**
     * Un nom de fichier est invalide.
     */
    EVIDENCE_RECORD_ERROR_INVALID_NAME,

    /**
     * Le chemin relatif est invalide.
     */
    EVIDENCE_RECORD_ERROR_INVALID_PATH,

    /**
     * L’identifiant du type de preuve est invalide.
     */
    EVIDENCE_RECORD_ERROR_INVALID_TYPE,

    /**
     * L’empreinte SHA-256 est invalide.
     */
    EVIDENCE_RECORD_ERROR_INVALID_SHA256,

    /**
     * Une date est invalide.
     */
    EVIDENCE_RECORD_ERROR_INVALID_DATE,

    /**
     * Le statut d’intégrité est invalide.
     */
    EVIDENCE_RECORD_ERROR_INVALID_STATUS
} EvidenceRecordError;

/**
 * @brief Domaine d’erreur du modèle EvidenceRecord.
 */
#define EVIDENCE_RECORD_ERROR \
    evidence_record_error_quark()

/**
 * @brief Retourne le domaine d’erreur du modèle.
 *
 * @return Quark GLib associé à EvidenceRecord.
 */
GQuark evidence_record_error_quark(void);

/**
 * @brief Crée un modèle représentant une preuve numérique.
 *
 * Toutes les chaînes sont copiées et appartiennent au nouvel objet.
 *
 * Les champs identifier, original_name, internal_name, relative_path,
 * type_identifier, sha256 et imported_at sont obligatoires.
 *
 * collected_at, source et description peuvent être NULL.
 *
 * @param identifier Identifiant UUID de la preuve.
 * @param original_name Nom du fichier au moment de la collecte.
 * @param internal_name Nom utilisé pour le stockage dans l’enquête.
 * @param relative_path Chemin relatif depuis la racine de l’enquête.
 * @param type_identifier Identifiant métier du type de preuve.
 * @param size_bytes Taille observée du fichier.
 * @param sha256 Empreinte SHA-256 du fichier.
 * @param imported_at Date UTC de l’importation.
 * @param collected_at Date UTC de collecte, ou NULL.
 * @param source Description de la source, ou NULL.
 * @param description Description facultative, ou NULL.
 * @param integrity_status État d’intégrité de la preuve.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouveau modèle, ou NULL lorsque les données sont invalides.
 */
EvidenceRecord *evidence_record_new(
    const char *identifier,
    const char *original_name,
    const char *internal_name,
    const char *relative_path,
    const char *type_identifier,
    guint64 size_bytes,
    const char *sha256,
    const char *imported_at,
    const char *collected_at,
    const char *source,
    const char *description,
    EvidenceIntegrityStatus integrity_status,
    GError **error
);

/**
 * @brief Libère un modèle EvidenceRecord.
 *
 * Cette fonction accepte evidence_record == NULL.
 *
 * @param evidence_record Modèle à libérer.
 */
void evidence_record_free(
    EvidenceRecord *evidence_record
);

/**
 * @brief Retourne l’identifiant UUID de la preuve.
 *
 * @return Chaîne empruntée, ou NULL.
 */
const char *evidence_record_get_identifier(
    const EvidenceRecord *evidence_record
);

/**
 * @brief Retourne le nom original du fichier.
 *
 * @return Chaîne empruntée, ou NULL.
 */
const char *evidence_record_get_original_name(
    const EvidenceRecord *evidence_record
);

/**
 * @brief Retourne le nom interne du fichier.
 *
 * @return Chaîne empruntée, ou NULL.
 */
const char *evidence_record_get_internal_name(
    const EvidenceRecord *evidence_record
);

/**
 * @brief Retourne le chemin relatif de la preuve.
 *
 * @return Chaîne empruntée, ou NULL.
 */
const char *evidence_record_get_relative_path(
    const EvidenceRecord *evidence_record
);

/**
 * @brief Retourne l’identifiant du type de preuve.
 *
 * @return Chaîne empruntée, ou NULL.
 */
const char *evidence_record_get_type_identifier(
    const EvidenceRecord *evidence_record
);

/**
 * @brief Retourne la taille observée de la preuve.
 *
 * @return Taille en octets, ou 0 lorsque l’objet est NULL.
 */
guint64 evidence_record_get_size_bytes(
    const EvidenceRecord *evidence_record
);

/**
 * @brief Retourne l’empreinte SHA-256.
 *
 * L’empreinte est stockée en minuscules.
 *
 * @return Chaîne empruntée, ou NULL.
 */
const char *evidence_record_get_sha256(
    const EvidenceRecord *evidence_record
);

/**
 * @brief Retourne la date UTC d’importation.
 *
 * @return Chaîne empruntée, ou NULL.
 */
const char *evidence_record_get_imported_at(
    const EvidenceRecord *evidence_record
);

/**
 * @brief Retourne la date UTC de collecte.
 *
 * @return Chaîne empruntée, ou NULL.
 */
const char *evidence_record_get_collected_at(
    const EvidenceRecord *evidence_record
);

/**
 * @brief Retourne la source déclarée de la preuve.
 *
 * @return Chaîne empruntée, ou NULL.
 */
const char *evidence_record_get_source(
    const EvidenceRecord *evidence_record
);

/**
 * @brief Retourne la description de la preuve.
 *
 * @return Chaîne empruntée, ou NULL.
 */
const char *evidence_record_get_description(
    const EvidenceRecord *evidence_record
);

/**
 * @brief Retourne l’état d’intégrité de la preuve.
 *
 * @return État enregistré, ou EVIDENCE_INTEGRITY_STATUS_UNKNOWN
 *         lorsque l’objet est NULL.
 */
EvidenceIntegrityStatus evidence_record_get_integrity_status(
    const EvidenceRecord *evidence_record
);

G_END_DECLS

#endif
