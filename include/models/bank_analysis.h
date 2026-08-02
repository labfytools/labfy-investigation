#ifndef LABFY_INVESTIGATION_BANK_ANALYSIS_H
#define LABFY_INVESTIGATION_BANK_ANALYSIS_H

#include "models/financial_foundation.h"

G_BEGIN_DECLS

#define BANK_ANALYSIS_MAX_OCCURRENCES 4096U
#define BANK_ANALYSIS_MAX_BLOCKS 1024U
#define BANK_ANALYSIS_MAX_TRANSACTIONS 256U
#define BANK_ANALYSIS_MAX_VALUE_BYTES 512U
#define BANK_ANALYSIS_MAX_CONTEXT_BYTES 1024U
#define BANK_ANALYSIS_MAX_ITEMS_PER_BLOCK 256U
#define BANK_ANALYSIS_MAX_WARNINGS 256U
#define BANK_ANALYSIS_MAX_ID_BYTES 128U
#define BANK_ANALYSIS_MAX_AMOUNT_BYTES 128U
#define BANK_ANALYSIS_MAX_AMOUNT_DIGITS 64U
#define BANK_ANALYSIS_MAX_AMOUNT_GROUPS 16U
#define BANK_ANALYSIS_MAX_AMOUNT_SEPARATORS 16U
#define BANK_ANALYSIS_MAX_AMOUNT_PRECISION 8U

typedef enum {
    BANK_OCCURRENCE_IBAN, BANK_OCCURRENCE_BIC,
    BANK_OCCURRENCE_DECLARED_HOLDER, BANK_OCCURRENCE_DECLARED_INSTITUTION,
    BANK_OCCURRENCE_DECLARED_ADDRESS, BANK_OCCURRENCE_AMOUNT,
    BANK_OCCURRENCE_CURRENCY, BANK_OCCURRENCE_OBSERVED_DATE,
    BANK_OCCURRENCE_OBSERVED_TIME, BANK_OCCURRENCE_OBSERVED_STATUS,
    BANK_OCCURRENCE_BANK_REFERENCE, BANK_OCCURRENCE_UETR,
    BANK_OCCURRENCE_DECLARED_PAYER, BANK_OCCURRENCE_DECLARED_BENEFICIARY,
    BANK_OCCURRENCE_DEBTOR_ACCOUNT, BANK_OCCURRENCE_CREDITOR_ACCOUNT,
    BANK_OCCURRENCE_COUNT
} BankOccurrenceType;

/* Toutes les chaînes sont possédées. warnings est possédé par l'occurrence,
 * possède ses éléments char* et les détruit avec g_free. Les offsets sont
 * des octets dans source_text ; end_offset est exclusif et désigne exactement
 * raw_value. bank_occurrence_free() détruit l'ensemble. */
typedef struct {
    char *identifier;
    BankOccurrenceType type;
    char *raw_value;
    char *normalized_value;
    gsize start_offset;
    gsize end_offset;
    BankValidationState validation;
    char *extraction_rule;
    GPtrArray *warnings;
    char *source_context;
} BankOccurrence;

typedef struct {
    char *identifier;
    /* Tableau possédé de char* possédés, détruits avec g_free. Tout élément
     * lu directement est emprunté et invalidé par retrait ou destruction. */
    GPtrArray *occurrence_identifiers;
    gsize start_offset;
    gsize end_offset;
    /* Tableau possédé de char* possédés, détruits avec g_free. Tout élément
     * lu directement est emprunté et invalidé par retrait ou destruction. */
    GPtrArray *reasons;
    gboolean ambiguous;
    BankOrigin origin;
    BankHumanDecision decision;
} BankProposedBlock;

typedef struct {
    char *identifier;
    /* Chaque tableau ci-dessous est possédé, possède ses éléments char* et
     * les détruit avec g_free. Un élément lu directement est emprunté et
     * invalidé par retrait ou destruction du tableau. */
    GPtrArray *source_occurrence_identifiers;
    GPtrArray *missing_fields;
    GPtrArray *ambiguities;
    GPtrArray *reasons;
    BankHumanDecision decision;
    gboolean create_transaction;
    gboolean create_account;
    gboolean create_person;
    gboolean create_relation;
} BankTransactionProposal;

typedef struct {
    char *source_text;
    /* Tableaux possédés. occurrences, blocks et transaction_proposals
     * possèdent leurs objets et appellent respectivement bank_occurrence_free,
     * bank_proposed_block_free et bank_transaction_proposal_free. warnings
     * possède ses char* et les détruit avec g_free. Un élément lu directement
     * est emprunté ; suppression/fusion ou destruction peut l'invalider. */
    GPtrArray *occurrences;
    GPtrArray *blocks;
    GPtrArray *transaction_proposals;
    GPtrArray *warnings;
    gboolean occurrence_limit_reached;
    gboolean block_limit_reached;
    gboolean transaction_limit_reached;
    gboolean amount_limit_reached;
} BankAnalysisResult;

const char *bank_occurrence_type_to_code(BankOccurrenceType type);
void bank_occurrence_free(BankOccurrence *occurrence);
void bank_proposed_block_free(BankProposedBlock *block);
void bank_transaction_proposal_free(BankTransactionProposal *proposal);
void bank_analysis_result_free(BankAnalysisResult *result);
/* Ces fonctions retournent des pointeurs empruntés appartenant à result ;
 * l'appelant ne doit jamais les libérer. Une occurrence reste valide pendant
 * la vie de result. Un bloc reste valide jusqu'à sa suppression éventuelle
 * par une fusion de révision, ou jusqu'à la libération de result. */
BankOccurrence *bank_analysis_find_occurrence(const BankAnalysisResult *result,
    const char *identifier);
BankProposedBlock *bank_analysis_find_block(const BankAnalysisResult *result,
    const char *identifier);

G_END_DECLS
#endif
