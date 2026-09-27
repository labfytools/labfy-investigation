#ifndef LABFY_INVESTIGATION_FINANCIAL_RECORDS_H
#define LABFY_INVESTIGATION_FINANCIAL_RECORDS_H
#include "models/financial_foundation.h"
#include <glib.h>
G_BEGIN_DECLS

/* Tous les constructeurs copient les chaînes. Les tableaux sont possédés,
 * possèdent leurs char* et les détruisent avec g_free. Les getters directs
 * sur les champs retournent donc des pointeurs empruntés. */
typedef struct {
 char *id,*investigation_id,*evidence_id,*evidence_sha256,*extraction_method;
 char *source_kind,*created_at; BankOrigin origin;
} BankAnalysisRecord;
typedef struct {
 char *id,*analysis_id,*investigation_id,*evidence_id,*evidence_sha256;
 char *occurrence_type,*raw_value,*extraction_rule,*page_or_image,*ocr_run_id;
 char *region_x,*region_y,*region_width,*region_height,*source_context,*created_at;
 gsize start_offset,end_offset;
 BankValidationState validation; BankOrigin origin; GPtrArray *warnings;
} BankObservationRecord;
typedef struct {
 char *id,*investigation_id,*evidence_id,*created_at; gsize start_offset,end_offset;
 gboolean ambiguous; BankOrigin origin; GPtrArray *observation_ids,*roles,*reasons;
} BankProposalRecord;
typedef struct {
 char *id,*observation_id,*corrected_value,*normalized_value,*factual_notes;
 char *author,*created_at; BankValidationState validation;
} BankRevisionRecord;
typedef struct {
 char *id,*observation_id,*proposal_id,*revision_id,*created_at;
 BankHumanDecision decision; gboolean create_account,reuse_account,create_holder,create_transaction;
} BankDecisionRecord;
typedef struct {
 char *entity_id,*investigation_id,*iban_normalized,*country_code,*bank_code;
 char *branch_code,*account_number,*rib_key,*bic,*declared_institution;
 char *first_observed_at,*last_observed_at,*factual_notes; BankAccountState state;
} BankAccountRecord;
typedef struct {
 char *id,*investigation_id,*evidence_id,*observation_id,*revision_id;
 char *account_entity_id,*person_entity_id,*decision_id,*declared_name_raw;
 char *declared_name_corrected,*created_at,*factual_notes;
} DeclaredBankHolderRecord;
typedef struct {
 char *id,*investigation_id,*transaction_type,*observed_date_raw,*observed_time_raw;
 char *observed_at_normalized,*amount_decimal,*currency,*observed_status;
 char *bank_reference,*reference_type,*uetr,*debtor_account_id,*creditor_account_id;
 char *declared_payer,*declared_beneficiary,*source_institution,*evidence_id;
 char *source_observation_id,*source_proposal_id,*decision_id,*factual_notes,*created_at;
} FinancialTransactionRecord;

gboolean financial_record_identifier_valid(const char *value);
gboolean financial_record_utc_valid(const char *value);
gboolean financial_record_sha256_valid(const char *value);
gboolean financial_record_amount_valid(const char *value);
BankAnalysisRecord *bank_analysis_record_copy(const BankAnalysisRecord *record);
BankObservationRecord *bank_observation_record_copy(const BankObservationRecord *record);
BankProposalRecord *bank_proposal_record_copy(const BankProposalRecord *record);
BankRevisionRecord *bank_revision_record_copy(const BankRevisionRecord *record);
BankDecisionRecord *bank_decision_record_copy(const BankDecisionRecord *record);
BankAccountRecord *bank_account_record_copy(const BankAccountRecord *record);
DeclaredBankHolderRecord *declared_bank_holder_record_copy(const DeclaredBankHolderRecord *record);
FinancialTransactionRecord *financial_transaction_record_copy(const FinancialTransactionRecord *record);
void bank_analysis_record_free(BankAnalysisRecord *record);
void bank_observation_record_free(BankObservationRecord *record);
void bank_proposal_record_free(BankProposalRecord *record);
void bank_revision_record_free(BankRevisionRecord *record);
void bank_decision_record_free(BankDecisionRecord *record);
void bank_account_record_free(BankAccountRecord *record);
void declared_bank_holder_record_free(DeclaredBankHolderRecord *record);
void financial_transaction_record_free(FinancialTransactionRecord *record);
G_END_DECLS
#endif
