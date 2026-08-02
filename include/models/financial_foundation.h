#ifndef LABFY_INVESTIGATION_FINANCIAL_FOUNDATION_H
#define LABFY_INVESTIGATION_FINANCIAL_FOUNDATION_H

#include <glib.h>

G_BEGIN_DECLS

#define FINANCIAL_TEXT_MAX_BYTES (1024U * 1024U)
#define FINANCIAL_CANDIDATE_MAX_RESULTS 4096U

typedef enum { BANK_VALIDATION_VALID, BANK_VALIDATION_INVALID,
    BANK_VALIDATION_AMBIGUOUS, BANK_VALIDATION_UNVERIFIABLE,
    BANK_VALIDATION_COUNT } BankValidationState;
typedef enum { BANK_DECISION_PROPOSED, BANK_DECISION_CONFIRMED,
    BANK_DECISION_REJECTED, BANK_DECISION_COUNT } BankHumanDecision;
typedef enum { BANK_ORIGIN_AUTOMATIC, BANK_ORIGIN_HUMAN,
    BANK_ORIGIN_LEGACY, BANK_ORIGIN_COUNT } BankOrigin;
typedef enum { BANK_ACCOUNT_STATE_UNKNOWN, BANK_ACCOUNT_STATE_ACTIVE,
    BANK_ACCOUNT_STATE_CLOSED } BankAccountState;

/* Contrat d'ownership commun : tous les constructeurs copient les chaînes
 * reçues. Tous les champs pointeurs des structures retournées sont possédés
 * par la structure et libérés par sa fonction *_free(), qui accepte NULL.
 * Les champs bruts d'une observation ou d'un candidat sont immuables par
 * contrat ; une correction appartient à BankObservationRevision. */

/* Les conversions retournent "unknown" pour toute valeur hors enum. */
const char *bank_validation_state_to_code(BankValidationState state);
const char *bank_human_decision_to_code(BankHumanDecision decision);
const char *bank_origin_to_code(BankOrigin origin);

typedef struct {
    char *source_text; char *raw_iban; char *normalized_iban;
    char *raw_bic; char *normalized_bic; char *declared_holder_raw;
    char *declared_institution; char *declared_address;
    gsize start_offset; gsize end_offset; BankValidationState validation;
    GPtrArray *warnings; BankOrigin origin; BankHumanDecision decision;
    gboolean proposed_grouping; char *proposed_group_identifier;
} BankCandidate;

BankCandidate *bank_candidate_new(const char *source_text, const char *raw_iban,
    gsize start_offset, gsize end_offset, GError **error);
void bank_candidate_free(BankCandidate *candidate);

typedef struct {
    char *identifier; char *evidence_identifier; char *evidence_sha256;
    char *raw_value; char *source_type; char *extraction_method;
    char *source_page_or_image; char *ocr_run_identifier;
    gboolean has_region; double region_x; double region_y;
    double region_width; double region_height; BankOrigin origin;
    char *created_at_utc; char *factual_notes;
} BankObservation;

BankObservation *bank_observation_new(const char *identifier,
    const char *evidence_identifier, const char *evidence_sha256,
    const char *raw_value, const char *source_type,
    const char *extraction_method, BankOrigin origin,
    const char *created_at_utc, GError **error);
void bank_observation_free(BankObservation *observation);

typedef struct {
    char *observation_identifier; char *corrected_iban; char *normalized_iban;
    char *corrected_bic; char *normalized_bic; char *corrected_holder;
    BankValidationState validation; BankOrigin origin;
    char *created_at_utc; char *factual_notes;
} BankObservationRevision;

BankObservationRevision *bank_observation_revision_new(
    const char *observation_identifier, const char *corrected_iban,
    const char *normalized_iban, BankValidationState validation,
    const char *created_at_utc, GError **error);
void bank_observation_revision_free(BankObservationRevision *revision);

typedef struct {
    char *entity_identifier; char *normalized_iban; char *country_code;
    char *bic; char *declared_institution; BankAccountState state;
    char *factual_notes;
} BankAccount;
BankAccount *bank_account_new(const char *entity_identifier,
    const char *normalized_iban, const char *bic,
    const char *declared_institution, BankAccountState state, GError **error);
void bank_account_free(BankAccount *account);

typedef struct {
    char *declared_name; char *evidence_identifier;
    char *observation_identifier; BankOrigin origin; char *factual_notes;
} DeclaredBankHolder;
DeclaredBankHolder *declared_bank_holder_new(const char *declared_name,
    const char *evidence_identifier, const char *observation_identifier,
    BankOrigin origin, GError **error);
void declared_bank_holder_free(DeclaredBankHolder *holder);

typedef struct {
    char *transaction_type; char *raw_date; char *normalized_date;
    char *amount_decimal; char *currency; char *observed_status;
    char *bank_reference; char *reference_type; char *uetr;
    char *debtor_account_identifier; char *creditor_account_identifier;
    char *declared_payer; char *declared_beneficiary;
    char *source_institution; char *evidence_identifier;
    char *observation_identifier; BankOrigin origin;
    char *factual_notes; char *created_at_utc;
} FinancialTransaction;
/* amount_decimal est une chaîne ASCII validée : signe facultatif, chiffres,
 * point décimal facultatif. Elle préserve une valeur exacte sans flottant. */
FinancialTransaction *financial_transaction_new(const char *transaction_type,
    const char *raw_date, const char *amount_decimal, const char *currency,
    const char *observed_status, const char *evidence_identifier,
    const char *created_at_utc, GError **error);
void financial_transaction_free(FinancialTransaction *transaction);

typedef struct {
    GPtrArray *candidates; BankHumanDecision decision;
    gboolean create_account; gboolean reuse_account; gboolean create_person;
    gboolean create_relation; gboolean create_transaction;
} FinancialReviewModel;
/* candidates est possédé et détruit avec ses BankCandidate. Les options de
 * création restent FALSE jusqu'à une action humaine explicite. */
FinancialReviewModel *financial_review_model_new(void);
void financial_review_model_free(FinancialReviewModel *model);

G_END_DECLS
#endif
