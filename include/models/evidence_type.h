/******************************************************************************
 * @file evidence_type.h
 * @brief Modèle représentant un type de preuve.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_EVIDENCE_TYPE_H
#define LABFY_INVESTIGATION_EVIDENCE_TYPE_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * @brief Modèle opaque représentant un type de preuve.
 */
typedef struct EvidenceType EvidenceType;

/**
 * @brief Codes d’erreur produits par EvidenceType.
 */
typedef enum
{
    /**
     * Un argument général est invalide.
     */
    EVIDENCE_TYPE_ERROR_INVALID_ARGUMENT,

    /**
     * L’identifiant numérique est invalide.
     */
    EVIDENCE_TYPE_ERROR_INVALID_IDENTIFIER,

    /**
     * Le code métier est invalide.
     */
    EVIDENCE_TYPE_ERROR_INVALID_CODE,

    /**
     * Le libellé utilisateur est invalide.
     */
    EVIDENCE_TYPE_ERROR_INVALID_LABEL
} EvidenceTypeError;

/**
 * @brief Domaine d’erreur du modèle EvidenceType.
 */
#define EVIDENCE_TYPE_ERROR \
    evidence_type_error_quark()

/**
 * @brief Retourne le domaine d’erreur du modèle.
 *
 * @return Quark GLib associé à EvidenceType.
 */
GQuark evidence_type_error_quark(void);

/**
 * @brief Crée un type de preuve.
 *
 * Les chaînes sont copiées et appartiennent au nouvel objet.
 *
 * L’identifiant doit être strictement positif.
 *
 * Le code et le libellé sont obligatoires.
 * La description peut être NULL ou vide.
 *
 * @param identifier Identifiant numérique SQLite.
 * @param code Code métier du type.
 * @param label Libellé destiné à l’utilisateur.
 * @param description Description facultative.
 * @param error Emplacement facultatif recevant une erreur.
 *
 * @return Nouveau modèle, ou NULL lorsque les données sont invalides.
 */
EvidenceType *evidence_type_new(
    gint64 identifier,
    const char *code,
    const char *label,
    const char *description,
    GError **error
);

/**
 * @brief Libère un modèle EvidenceType.
 *
 * Cette fonction accepte evidence_type == NULL.
 *
 * @param evidence_type Modèle à libérer.
 */
void evidence_type_free(
    EvidenceType *evidence_type
);

/**
 * @brief Retourne l’identifiant numérique du type.
 *
 * @return Identifiant, ou 0 lorsque evidence_type est NULL.
 */
gint64 evidence_type_get_identifier(
    const EvidenceType *evidence_type
);

/**
 * @brief Retourne le code métier du type.
 *
 * @return Chaîne empruntée, ou NULL.
 */
const char *evidence_type_get_code(
    const EvidenceType *evidence_type
);

/**
 * @brief Retourne le libellé utilisateur du type.
 *
 * @return Chaîne empruntée, ou NULL.
 */
const char *evidence_type_get_label(
    const EvidenceType *evidence_type
);

/**
 * @brief Retourne la description du type.
 *
 * @return Chaîne empruntée, ou NULL.
 */
const char *evidence_type_get_description(
    const EvidenceType *evidence_type
);

G_END_DECLS

#endif
