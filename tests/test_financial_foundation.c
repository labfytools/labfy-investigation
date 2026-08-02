#include "core/bank_proposal.h"
#include "core/bank_text_extractor.h"
#include "core/bic_validator.h"
#include "core/french_bban.h"
#include "core/iban_analyzer.h"
#include "models/financial_foundation.h"
#include <glib.h>
#include <string.h>

static const char *valid_fr="FR4830002005500000000000052";
static const char *valid_de="DE89370400440532013000";
static const char *valid_gb="GB82WEST12345698765432";
static const char *stamp="2026-01-02T03:04:05Z";

static char *test_make_mod97_iban(const char *country,gsize length)
{
 char *value=g_malloc0(length+1);memcpy(value,country,2);memset(value+2,'0',length-2);
 for(guint key=0;key<100;key++){value[2]=(char)('0'+key/10);value[3]=(char)('0'+key%10);
  if(iban_analyzer_validate(value))return value;}g_free(value);return NULL;
}

static void test_states_independent(void)
{
 g_assert_cmpstr(bank_validation_state_to_code(BANK_VALIDATION_VALID),==,"valid");
 g_assert_cmpstr(bank_human_decision_to_code(BANK_DECISION_PROPOSED),==,"proposed");
 g_assert_cmpstr(bank_origin_to_code(BANK_ORIGIN_HUMAN),==,"human");
 FinancialReviewModel *m=financial_review_model_new();g_assert_nonnull(m);
 g_assert_false(m->create_account);g_assert_false(m->reuse_account);
 g_assert_false(m->create_person);g_assert_false(m->create_relation);
 g_assert_false(m->create_transaction);BankCandidate *c=bank_candidate_new("RAW","RAW",0,3,NULL);
 g_ptr_array_add(m->candidates,c);c->decision=BANK_DECISION_REJECTED;
 g_assert_cmpstr(c->raw_iban,==,"RAW");financial_review_model_free(m);
}
static void test_enum_conversion_bounds(void)
{
 const char *validation_codes[]={"valid","invalid","ambiguous","unverifiable"};
 for(gint value=0;value<BANK_VALIDATION_COUNT;value++)
  g_assert_cmpstr(bank_validation_state_to_code((BankValidationState)value),==,validation_codes[value]);
 const char *decision_codes[]={"proposed","confirmed","rejected"};
 for(gint value=0;value<BANK_DECISION_COUNT;value++)
  g_assert_cmpstr(bank_human_decision_to_code((BankHumanDecision)value),==,decision_codes[value]);
 const char *origin_codes[]={"automatic","human","legacy"};
 for(gint value=0;value<BANK_ORIGIN_COUNT;value++)
  g_assert_cmpstr(bank_origin_to_code((BankOrigin)value),==,origin_codes[value]);
 const gint invalid_values[]={-1,-2,G_MAXINT};
 for(guint i=0;i<G_N_ELEMENTS(invalid_values);i++) {
  g_assert_cmpstr(bank_validation_state_to_code((BankValidationState)invalid_values[i]),==,"unknown");
  g_assert_cmpstr(bank_human_decision_to_code((BankHumanDecision)invalid_values[i]),==,"unknown");
  g_assert_cmpstr(bank_origin_to_code((BankOrigin)invalid_values[i]),==,"unknown");
 }
 g_assert_cmpstr(bank_validation_state_to_code(BANK_VALIDATION_COUNT),==,"unknown");
 g_assert_cmpstr(bank_validation_state_to_code((BankValidationState)(BANK_VALIDATION_COUNT+1)),==,"unknown");
 g_assert_cmpstr(bank_human_decision_to_code(BANK_DECISION_COUNT),==,"unknown");
 g_assert_cmpstr(bank_human_decision_to_code((BankHumanDecision)(BANK_DECISION_COUNT+1)),==,"unknown");
 g_assert_cmpstr(bank_origin_to_code(BANK_ORIGIN_COUNT),==,"unknown");
 g_assert_cmpstr(bank_origin_to_code((BankOrigin)(BANK_ORIGIN_COUNT+1)),==,"unknown");
}
static void test_candidate_offset_bounds(void)
{
 const char *source="SPECIMEN\xC3\xA9";
 const gsize length=strlen(source);
 BankCandidate *candidate=bank_candidate_new(source,"SPECIMEN",0,length,NULL);
 g_assert_nonnull(candidate);bank_candidate_free(candidate);
 candidate=bank_candidate_new(source,"SPECIMEN",length,length,NULL);
 g_assert_nonnull(candidate);bank_candidate_free(candidate);
 candidate=bank_candidate_new(source,"SPECIMEN",8,10,NULL);
 g_assert_nonnull(candidate);g_assert_cmpuint(candidate->start_offset,==,8);
 g_assert_cmpuint(candidate->end_offset,==,10);bank_candidate_free(candidate);
 g_assert_null(bank_candidate_new("0123456789","SPECIMEN",100,101,NULL));
 g_assert_null(bank_candidate_new("0123456789","SPECIMEN",8,7,NULL));
 g_assert_null(bank_candidate_new("0123456789","SPECIMEN",11,11,NULL));
 g_assert_null(bank_candidate_new("0123456789","SPECIMEN",10,11,NULL));
 g_assert_null(bank_candidate_new("0123456789","SPECIMEN",G_MAXSIZE,G_MAXSIZE,NULL));
 g_assert_null(bank_candidate_new("","SPECIMEN",1,1,NULL));
}
static void test_iban_canonical_and_legacy_parity(void)
{
 const char *values[]={valid_fr,valid_de,valid_gb,"FR0030002005500000000000021","FR763000",NULL};
 for(guint i=0;values[i]!=NULL;i++)g_assert_cmpint(iban_analyzer_validate(values[i]),==,
   bank_proposal_validate_iban(values[i]));
 char *n=iban_analyzer_normalize_canonical("fr48 3000-2005\n5000 0000 0000 052");
 g_assert_cmpstr(n,==,valid_fr);g_free(n);
 g_assert_null(iban_analyzer_normalize_canonical("FR48.30002005500000000000052"));
 g_assert_null(iban_analyzer_normalize_canonical("FR48３0002005500000000000052"));
 char *wrong_length=test_make_mod97_iban("FR",26);g_assert_nonnull(wrong_length);
 g_assert_false(bank_proposal_validate_iban(wrong_length));
 g_assert_cmpint(iban_analyzer_validate_canonical_result(wrong_length),==,IBAN_ANALYZER_RESULT_INVALID);
 char *unsupported=test_make_mod97_iban("AD",24);g_assert_nonnull(unsupported);
 g_assert_cmpint(iban_analyzer_validate_canonical_result(unsupported),==,IBAN_ANALYZER_RESULT_UNSUPPORTED_COUNTRY);
 g_assert_false(iban_analyzer_validate_canonical(unsupported));
 char *unknown=test_make_mod97_iban("ZZ",24);g_assert_nonnull(unknown);
 g_assert_cmpint(iban_analyzer_validate_canonical_result(unknown),==,IBAN_ANALYZER_RESULT_UNSUPPORTED_COUNTRY);
 g_assert_cmpint(iban_analyzer_validate_canonical_result("F100"),==,IBAN_ANALYZER_RESULT_INVALID);
 g_assert_cmpint(iban_analyzer_validate_canonical_result("FR00"),==,IBAN_ANALYZER_RESULT_INVALID);
 g_assert_cmpint(iban_analyzer_validate_canonical_result(valid_fr),==,IBAN_ANALYZER_RESULT_VALID);
 g_assert_cmpint(iban_analyzer_validate_canonical_result("FR0030002005500000000000021"),==,IBAN_ANALYZER_RESULT_INVALID);
 g_free(unknown);g_free(unsupported);g_free(wrong_length);
}
static void test_bic_and_bban(void)
{
 g_assert_cmpint(bic_validator_check(NULL),==,BIC_VALIDATOR_ABSENT);
 g_assert_cmpint(bic_validator_check(""),==,BIC_VALIDATOR_ABSENT);
 g_assert_false(bic_validator_validate(NULL));g_assert_true(bic_validator_validate("BNPAFRPP"));
 g_assert_cmpint(bic_validator_check("BNPAFRPP"),==,BIC_VALIDATOR_VALID);
 g_assert_cmpint(bic_validator_check("BNPAFR"),==,BIC_VALIDATOR_INVALID);
 g_assert_true(bic_validator_validate("BNPAFRPPXXX"));g_assert_false(bic_validator_validate("BNPAFR"));
 g_assert_false(bic_validator_validate("BNPAFRP!"));char *bic=bic_validator_normalize("bnpafrppxxx");
 g_assert_cmpstr(bic,==,"BNPAFRPPXXX");g_free(bic);FrenchBban *b=french_bban_derive(valid_fr);
 g_assert_nonnull(b);g_assert_cmpstr(b->bank_code,==,"30002");g_assert_cmpstr(b->branch_code,==,"00550");
 g_assert_cmpstr(b->account_number,==,"00000000000");g_assert_cmpstr(b->rib_key,==,"52");
 french_bban_free(b);g_assert_null(french_bban_derive("FR0030002005500000000000021"));
}
static void test_punctuation_and_utf8_offsets(void)
{
 const char *formats[]={"(%s)","IBAN: %s","%s,","[%s]",NULL};
 for(guint i=0;formats[i]!=NULL;i++){char *text=g_strdup_printf(formats[i],valid_fr);
  GPtrArray *items=bank_text_extractor_extract(text,NULL);g_assert_cmpuint(items->len,==,1);
  BankCandidate *candidate=g_ptr_array_index(items,0);g_assert_cmpstr(candidate->raw_iban,==,valid_fr);
  g_assert_cmpmem(text+candidate->start_offset,candidate->end_offset-candidate->start_offset,
   valid_fr,strlen(valid_fr));g_assert_nonnull(strstr(candidate->source_text,valid_fr));
  g_ptr_array_unref(items);g_free(text);}
 const char *utf8="Préfixe SPECIMEN : [FR4830002005500000000000052]";
 GPtrArray *items=bank_text_extractor_extract(utf8,NULL);g_assert_cmpuint(items->len,==,1);
 BankCandidate *candidate=g_ptr_array_index(items,0);g_assert_cmpuint(candidate->start_offset,==,
  (gsize)(strstr(utf8,valid_fr)-utf8));g_assert_cmpuint(candidate->end_offset,==,
  candidate->start_offset+strlen(valid_fr));g_ptr_array_unref(items);
}
static void test_multi_occurrences_and_grouping(void)
{
 const char *text="Titulaire : SPECIMEN ALPHA\nBIC : bnpafrpp\n"
  "IBAN : FR48 3000 2005 5000 0000 0000 052\n"
  "IBAN : DE89-3704-0044-0532-0130-00\n\n"
  "Titulaire : SPECIMEN BETA\nIBAN : GB82 WEST 1234 5698 7654 32";
 GPtrArray *items=bank_text_extractor_extract(text,NULL);g_assert_cmpuint(items->len,==,3);
 BankCandidate *a=g_ptr_array_index(items,0),*b=g_ptr_array_index(items,1),*c=g_ptr_array_index(items,2);
 g_assert_cmpint(a->validation,==,BANK_VALIDATION_VALID);g_assert_cmpint(b->validation,==,BANK_VALIDATION_VALID);
 g_assert_cmpint(c->validation,==,BANK_VALIDATION_VALID);g_assert_cmpstr(a->declared_holder_raw,==,"SPECIMEN ALPHA");
 g_assert_cmpstr(b->declared_holder_raw,==,"SPECIMEN ALPHA");g_assert_cmpstr(c->declared_holder_raw,==,"SPECIMEN BETA");
 g_assert_cmpstr(a->normalized_bic,==,"BNPAFRPP");g_assert_true(a->proposed_grouping);
 a->proposed_group_identifier=g_strdup("group-specimen");b->proposed_group_identifier=g_strdup("group-specimen");
 c->proposed_group_identifier=g_strdup("separate-specimen");g_assert_cmpstr(a->proposed_group_identifier,==,b->proposed_group_identifier);
 g_assert_cmpstr(a->proposed_group_identifier,!=,c->proposed_group_identifier);
 g_assert_cmpmem(text+a->start_offset,a->end_offset-a->start_offset,a->raw_iban,strlen(a->raw_iban));
 g_ptr_array_unref(items);
}
static void test_duplicate_offsets_and_invalid(void)
{
 char *text=g_strdup_printf("%s puis %s",valid_fr,valid_fr);GPtrArray *items=bank_text_extractor_extract(text,NULL);
 g_assert_cmpuint(items->len,==,2);BankCandidate *a=g_ptr_array_index(items,0),*b=g_ptr_array_index(items,1);
 g_assert_cmpuint(a->start_offset,<,b->start_offset);g_assert_cmpstr(a->raw_iban,==,b->raw_iban);g_ptr_array_unref(items);g_free(text);
 items=bank_text_extractor_extract("IBAN : FR00 3000 2005 5000 0000 0000 021",NULL);
 g_assert_cmpuint(items->len,==,1);a=g_ptr_array_index(items,0);g_assert_cmpint(a->validation,==,BANK_VALIDATION_INVALID);
 g_assert_nonnull(a->raw_iban);g_ptr_array_unref(items);
 items=bank_text_extractor_extract("IBAN : FR48 3000",NULL);g_assert_cmpuint(items->len,==,1);
 g_assert_cmpint(((BankCandidate*)g_ptr_array_index(items,0))->validation,==,BANK_VALIDATION_INVALID);g_ptr_array_unref(items);
}
static void test_ambiguities_no_silent_fix(void)
{
 const char *texts[]={"IBAN : FRO8 3000 2005 5000 0000 0000 052",
  "IBAN : FRI8 3000 2005 5000 0000 0000 052","IBAN : FR48 ３000 2005 5000 0000 0000 052",NULL};
 for(guint i=0;texts[i];i++){GPtrArray *items=bank_text_extractor_extract(texts[i],NULL);
  g_assert_cmpuint(items->len,==,1);BankCandidate *c=g_ptr_array_index(items,0);
  g_assert_cmpint(c->validation,==,BANK_VALIDATION_AMBIGUOUS);g_assert_null(c->normalized_iban);
  g_assert_true(strstr(texts[i],c->raw_iban)!=NULL);g_ptr_array_unref(items);}
}
static void test_models_and_transaction(void)
{
 const char *sha="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";GError *error=NULL;
 BankObservation *o=bank_observation_new("obs-specimen","proof-specimen",sha,"FRO8 raw","image","manual",BANK_ORIGIN_AUTOMATIC,stamp,&error);
 g_assert_no_error(error);g_assert_nonnull(o);BankObservationRevision *r=bank_observation_revision_new(
  "obs-specimen",valid_fr,valid_fr,BANK_VALIDATION_VALID,stamp,&error);g_assert_nonnull(r);
 g_assert_cmpstr(o->raw_value,==,"FRO8 raw");g_assert_cmpint(r->origin,==,BANK_ORIGIN_HUMAN);
 BankAccount *a=bank_account_new("entity-specimen",valid_fr,"BNPAFRPP",NULL,BANK_ACCOUNT_STATE_UNKNOWN,&error);g_assert_nonnull(a);
 DeclaredBankHolder *h=declared_bank_holder_new("SPECIMEN HOLDER","proof-specimen","obs-specimen",BANK_ORIGIN_HUMAN,&error);g_assert_nonnull(h);
 FinancialTransaction *t=financial_transaction_new("transfer","02/01/2026","1234.56","eur","envoyée","proof-specimen",stamp,&error);
 g_assert_nonnull(t);g_assert_cmpstr(t->amount_decimal,==,"1234.56");g_assert_cmpstr(t->observed_status,==,"envoyée");
 t->uetr=g_strdup("11111111-2222-4333-8444-555555555555");g_assert_cmpstr(t->uetr,==,"11111111-2222-4333-8444-555555555555");
 financial_transaction_free(t);declared_bank_holder_free(h);bank_account_free(a);
 bank_observation_revision_free(r);bank_observation_free(o);
 g_assert_null(financial_transaction_new("transfer","date","1.2.3","EUR","vu","proof",stamp,&error));g_clear_error(&error);
}
static void test_null_empty_and_limits(void)
{
 GPtrArray *items=bank_text_extractor_extract(NULL,NULL);g_assert_cmpuint(items->len,==,0);g_ptr_array_unref(items);
 items=bank_text_extractor_extract("",NULL);g_assert_cmpuint(items->len,==,0);g_ptr_array_unref(items);
 char *large=g_malloc0(FINANCIAL_TEXT_MAX_BYTES+2);memset(large,'A',FINANCIAL_TEXT_MAX_BYTES+1);
 GError *error=NULL;items=bank_text_extractor_extract(large,&error);g_assert_error(error,
  g_quark_from_static_string("bank-text-extractor-error"),1);g_assert_cmpuint(items->len,==,0);
 g_clear_error(&error);g_ptr_array_unref(items);g_free(large);
}
int main(int argc,char **argv)
{g_test_init(&argc,&argv,NULL);g_test_add_func("/financial/states",test_states_independent);
 g_test_add_func("/financial/enum-bounds",test_enum_conversion_bounds);
 g_test_add_func("/financial/candidate-offset-bounds",test_candidate_offset_bounds);
 g_test_add_func("/financial/iban-parity",test_iban_canonical_and_legacy_parity);
 g_test_add_func("/financial/bic-bban",test_bic_and_bban);
 g_test_add_func("/financial/multi-grouping",test_multi_occurrences_and_grouping);
 g_test_add_func("/financial/duplicates-invalid",test_duplicate_offsets_and_invalid);
 g_test_add_func("/financial/punctuation-offsets",test_punctuation_and_utf8_offsets);
 g_test_add_func("/financial/ambiguities",test_ambiguities_no_silent_fix);
 g_test_add_func("/financial/models-transaction",test_models_and_transaction);
 g_test_add_func("/financial/null-empty-limits",test_null_empty_and_limits);return g_test_run();}
