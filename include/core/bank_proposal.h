/******************************************************************************
 * @file bank_proposal.h
 * @brief Détection, normalisation et modèle de proposition bancaire (IBAN, RIB, BIC).
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_BANK_PROPOSAL_H
#define LABFY_INVESTIGATION_BANK_PROPOSAL_H

#include "core/controlled_vocab.h"
#include <glib.h>

G_BEGIN_DECLS

/** @brief Objet métier représentant une proposition bancaire complète. */
typedef struct BankProposal
{
    char *id;                   /**< UUID de la proposition */
    char *raw_iban;             /**< Graphie IBAN brute lue/OCR */
    char *normalized_iban;      /**< IBAN nettoyé et majuscule */
    char *bic;                  /**< BIC / SWIFT (8 ou 11 car) */
    char *holder_name;          /**< Titulaire du compte */
    char *bank_name;            /**< Nom de la banque */
    char *bank_address;         /**< Adresse de la banque */
    char *country_code;         /**< Code pays (ex: "FR") */
    char *bank_code;            /**< Code banque (5 ch pour RIB FR) */
    char *branch_code;          /**< Code guichet (5 ch pour RIB FR) */
    char *account_number;       /**< Numéro de compte (11 car) */
    char *rib_key;              /**< Clé RIB (2 ch) */
    gboolean is_iban_valid;     /**< VRAI si MOD-97 et format valides */
    gboolean is_derived_bban;   /**< VRAI si composants dérivés de l'IBAN */
    char *suggested_ocr_fix;    /**< Proposition de correction OCR (ex: "O->0") */
    char *verification_status;  /**< Code contrôlé: proposed, confirmed, rejected, etc. */
    char *provenance_kind;      /**< Code contrôlé: ocr, observed, derived, etc. */
    char *evidence_id;          /**< UUID de la preuve source */
    char *extraction_id;        /**< UUID de l'extraction source */
    char *created_at;           /**< Horodatage ISO 8601 UTC */
    char *updated_at;           /**< Horodatage ISO 8601 UTC */
} BankProposal;

/** @brief Libère une structure BankProposal. */
void bank_proposal_free(BankProposal *proposal);

/**
 * @brief Crée une proposition bancaire depuis une chaîne IBAN ou un texte OCR.
 * @param raw_text Texte brut observé / OCR.
 * @param evidence_id UUID de la preuve source (facultatif).
 * @return Nouvelle BankProposal, ou NULL.
 */
BankProposal *bank_proposal_analyze_text(const char *raw_text, const char *evidence_id);

/**
 * @brief Valide la structure MOD-97 d'un IBAN.
 * @param iban IBAN nettoyé (sans espaces).
 * @return VRAI si valide, FALSE sinon.
 */
gboolean bank_proposal_validate_iban(const char *iban);

/**
 * @brief Tente de dériver les composants BBAN (RIB) pour un IBAN français valide.
 * @param proposal Proposition bancaire à enrichir.
 * @return VRAI si dérivation réussie, FALSE sinon.
 */
gboolean bank_proposal_derive_french_rib(BankProposal *proposal);

/**
 * @brief Valide la structure d'un code BIC/SWIFT (8 ou 11 caractères).
 * @param bic Code BIC à vérifier.
 * @return VRAI si valide, FALSE sinon.
 */
gboolean bank_proposal_validate_bic(const char *bic);

G_END_DECLS

#endif /* LABFY_INVESTIGATION_BANK_PROPOSAL_H */
