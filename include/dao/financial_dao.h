#ifndef LABFY_INVESTIGATION_FINANCIAL_DAO_H
#define LABFY_INVESTIGATION_FINANCIAL_DAO_H
#include "database/database.h"
#include "models/financial_records.h"
G_BEGIN_DECLS
typedef struct BankObservationDao BankObservationDao;
typedef struct BankReviewDao BankReviewDao;
typedef struct BankAccountDao BankAccountDao;
typedef struct DeclaredBankHolderDao DeclaredBankHolderDao;
typedef struct FinancialTransactionDao FinancialTransactionDao;
BankObservationDao *bank_observation_dao_new(Database *database);
BankReviewDao *bank_review_dao_new(Database *database);
BankAccountDao *bank_account_dao_new(Database *database);
DeclaredBankHolderDao *declared_bank_holder_dao_new(Database *database);
FinancialTransactionDao *financial_transaction_dao_new(Database *database);
void bank_observation_dao_free(BankObservationDao *dao);
void bank_review_dao_free(BankReviewDao *dao);
void bank_account_dao_free(BankAccountDao *dao);
void declared_bank_holder_dao_free(DeclaredBankHolderDao *dao);
void financial_transaction_dao_free(FinancialTransactionDao *dao);
gboolean bank_observation_dao_insert_analysis(BankObservationDao*,const BankAnalysisRecord*,GError**);
gboolean bank_observation_dao_insert(BankObservationDao*,const BankObservationRecord*,GError**);
BankObservationRecord *bank_observation_dao_find(BankObservationDao*,const char*,GError**);
GPtrArray *bank_observation_dao_list_by_evidence(BankObservationDao*,const char*,GError**);
gboolean bank_review_dao_insert_proposal(BankReviewDao*,const BankProposalRecord*,GError**);
gboolean bank_review_dao_insert_revision(BankReviewDao*,const BankRevisionRecord*,GError**);
gboolean bank_review_dao_insert_decision(BankReviewDao*,const BankDecisionRecord*,GError**);
BankProposalRecord *bank_review_dao_find_proposal(BankReviewDao*,const char*,GError**);
GPtrArray *bank_review_dao_list_proposals_by_evidence(BankReviewDao*,const char*,GError**);
BankRevisionRecord *bank_review_dao_find_revision(BankReviewDao*,const char*,GError**);
BankDecisionRecord *bank_review_dao_find_decision(BankReviewDao*,const char*,GError**);
GPtrArray *bank_review_dao_list_revisions(BankReviewDao*,const char*,GError**);
GPtrArray *bank_review_dao_list_decisions(BankReviewDao*,const char*,GError**);
gboolean bank_account_dao_insert(BankAccountDao*,const BankAccountRecord*,GError**);
BankAccountRecord *bank_account_dao_find(BankAccountDao*,const char*,GError**);
GPtrArray *bank_account_dao_list_by_investigation(BankAccountDao*,const char*,GError**);
gboolean bank_account_dao_add_observation_source(BankAccountDao*,const char*,const char*,const char*,const char*,const char*,GError**);
gboolean declared_bank_holder_dao_insert(DeclaredBankHolderDao*,const DeclaredBankHolderRecord*,GError**);
DeclaredBankHolderRecord *declared_bank_holder_dao_find(DeclaredBankHolderDao*,const char*,GError**);
GPtrArray *declared_bank_holder_dao_list_by_evidence(DeclaredBankHolderDao*,const char*,GError**);
GPtrArray *declared_bank_holder_dao_list_by_account(DeclaredBankHolderDao*,const char*,GError**);
gboolean financial_transaction_dao_insert(FinancialTransactionDao*,const FinancialTransactionRecord*,GError**);
FinancialTransactionRecord *financial_transaction_dao_find(FinancialTransactionDao*,const char*,GError**);
GPtrArray *financial_transaction_dao_list_by_evidence(FinancialTransactionDao*,const char*,GError**);
GPtrArray *financial_transaction_dao_list_by_account(FinancialTransactionDao*,const char*,GError**);
gboolean financial_transaction_dao_add_observation_source(FinancialTransactionDao*,const char*,const char*,guint,GError**);
G_END_DECLS
#endif
