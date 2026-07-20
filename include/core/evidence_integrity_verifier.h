/******************************************************************************
 * @file evidence_integrity_verifier.h
 * @brief Vérification de l'intégrité d'une preuve numérique.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_EVIDENCE_INTEGRITY_VERIFIER_H
#define LABFY_INVESTIGATION_EVIDENCE_INTEGRITY_VERIFIER_H

#include "models/evidence_record.h"

#include <gio/gio.h>
#include <glib.h>

G_BEGIN_DECLS

/**
 * @brief Erreurs techniques du vérificateur.
 */
typedef enum
{
    EVIDENCE_INTEGRITY_VERIFIER_ERROR_INVALID_ARGUMENT,
    EVIDENCE_INTEGRITY_VERIFIER_ERROR_ROOT,
    EVIDENCE_INTEGRITY_VERIFIER_ERROR_CANCELLED,
    EVIDENCE_INTEGRITY_VERIFIER_ERROR_MEMORY
} EvidenceIntegrityVerifierError;

#define EVIDENCE_INTEGRITY_VERIFIER_ERROR \
    evidence_integrity_verifier_error_quark()

GQuark evidence_integrity_verifier_error_quark(void);

/**
 * @brief Résultat opaque d'une vérification.
 */
typedef struct EvidenceIntegrityVerificationResult
    EvidenceIntegrityVerificationResult;

/**
 * @brief Vérifie une preuve à partir de son chemin relatif.
 *
 * Le chemin est résolu sous investigation_root_path.
 * Les chemins absolus, les composants "." et ".." et les liens
 * symboliques sont refusés.
 *
 * Les résultats MISSING et ERROR sont des résultats métier normaux :
 * la fonction retourne alors un objet résultat sans remplir GError.
 *
 * Une erreur technique d'argument, de racine, de mémoire ou une
 * annulation retourne NULL.
 *
 * @param investigation_root_path Racine de l'enquête.
 * @param relative_path Chemin relatif enregistré dans SQLite.
 * @param expected_sha256 Empreinte enregistrée.
 * @param cancellable Objet d'annulation facultatif.
 * @param error Adresse recevant une erreur technique.
 *
 * @return Nouveau résultat possédé par l'appelant, ou NULL.
 */
EvidenceIntegrityVerificationResult *
evidence_integrity_verifier_verify(
    const char *investigation_root_path,
    const char *relative_path,
    const char *expected_sha256,
    GCancellable *cancellable,
    GError **error
);

/**
 * @brief Retourne le statut calculé.
 */
EvidenceIntegrityStatus
evidence_integrity_verification_result_get_status(
    const EvidenceIntegrityVerificationResult *result
);

/**
 * @brief Retourne l'empreinte recalculée.
 *
 * La chaîne est empruntée. Elle vaut NULL lorsque le fichier
 * n'a pas pu être lu.
 */
const char *
evidence_integrity_verification_result_get_computed_sha256(
    const EvidenceIntegrityVerificationResult *result
);

/**
 * @brief Retourne la taille effectivement lue.
 */
guint64 evidence_integrity_verification_result_get_size_bytes(
    const EvidenceIntegrityVerificationResult *result
);

/**
 * @brief Retourne un diagnostic technique facultatif.
 *
 * La chaîne est empruntée.
 */
const char *
evidence_integrity_verification_result_get_diagnostic(
    const EvidenceIntegrityVerificationResult *result
);

/**
 * @brief Libère un résultat.
 *
 * Cette fonction accepte NULL.
 */
void evidence_integrity_verification_result_free(
    EvidenceIntegrityVerificationResult *result
);

G_END_DECLS

#endif
