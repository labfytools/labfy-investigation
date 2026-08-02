#include "models/financial_foundation.h"
#include "core/bic_validator.h"
#include "core/iban_analyzer.h"
#include <string.h>

static GQuark financial_error_quark(void)
{ return g_quark_from_static_string("financial-foundation-error"); }
static gboolean required(const char *value)
{ return value != NULL && value[0] != '\0' && strlen(value) <= FINANCIAL_TEXT_MAX_BYTES; }
static gboolean timestamp_valid(const char *value)
{ return value != NULL && strlen(value) == 20 && value[4] == '-' &&
    value[7] == '-' && value[10] == 'T' && value[13] == ':' &&
    value[16] == ':' && value[19] == 'Z'; }
static void invalid(GError **error, const char *message)
{ if (error != NULL && *error == NULL) g_set_error_literal(error,
    financial_error_quark(), 1, message); }

const char *bank_validation_state_to_code(BankValidationState state)
{ switch(state) { case BANK_VALIDATION_VALID:return "valid";
  case BANK_VALIDATION_INVALID:return "invalid";
  case BANK_VALIDATION_AMBIGUOUS:return "ambiguous";
  case BANK_VALIDATION_UNVERIFIABLE:return "unverifiable";
  case BANK_VALIDATION_COUNT:break; } return "unknown"; }
const char *bank_human_decision_to_code(BankHumanDecision decision)
{ switch(decision) { case BANK_DECISION_PROPOSED:return "proposed";
  case BANK_DECISION_CONFIRMED:return "confirmed";
  case BANK_DECISION_REJECTED:return "rejected";
  case BANK_DECISION_COUNT:break; } return "unknown"; }
const char *bank_origin_to_code(BankOrigin origin)
{ switch(origin) { case BANK_ORIGIN_AUTOMATIC:return "automatic";
  case BANK_ORIGIN_HUMAN:return "human";
  case BANK_ORIGIN_LEGACY:return "legacy";
  case BANK_ORIGIN_COUNT:break; } return "unknown"; }

BankCandidate *bank_candidate_new(const char *source_text, const char *raw_iban,
    gsize start, gsize end, GError **error)
{
    if (!required(source_text) || !required(raw_iban)) {
        invalid(error, "Le candidat bancaire est invalide."); return NULL;
    }
    gsize source_length = strlen(source_text);
    if (start > end || start > source_length || end > source_length) {
        invalid(error, "Le candidat bancaire est invalide."); return NULL;
    }
    BankCandidate *item = g_new0(BankCandidate, 1);
    item->source_text = g_strdup(source_text); item->raw_iban = g_strdup(raw_iban);
    item->start_offset = start; item->end_offset = end;
    item->validation = BANK_VALIDATION_UNVERIFIABLE;
    item->warnings = g_ptr_array_new_with_free_func(g_free);
    item->origin = BANK_ORIGIN_AUTOMATIC; item->decision=BANK_DECISION_PROPOSED;
    item->proposed_grouping = TRUE;
    return item;
}
void bank_candidate_free(BankCandidate *c)
{ if (!c) return; g_free(c->source_text); g_free(c->raw_iban);
  g_free(c->normalized_iban); g_free(c->raw_bic); g_free(c->normalized_bic);
  g_free(c->declared_holder_raw); g_free(c->declared_institution);
  g_free(c->declared_address);g_free(c->proposed_group_identifier);
  g_clear_pointer(&c->warnings,g_ptr_array_unref); g_free(c); }

BankObservation *bank_observation_new(const char *id, const char *evidence,
    const char *sha256, const char *raw, const char *source,
    const char *method, BankOrigin origin, const char *created, GError **error)
{
    if (!required(id)||!required(evidence)||!required(sha256)||strlen(sha256)!=64||
        !required(raw)||!required(source)||!required(method)||!timestamp_valid(created))
    { invalid(error,"L’observation bancaire est invalide."); return NULL; }
    BankObservation *o=g_new0(BankObservation,1); o->identifier=g_strdup(id);
    o->evidence_identifier=g_strdup(evidence); o->evidence_sha256=g_strdup(sha256);
    o->raw_value=g_strdup(raw); o->source_type=g_strdup(source);
    o->extraction_method=g_strdup(method); o->origin=origin;
    o->created_at_utc=g_strdup(created); return o;
}
void bank_observation_free(BankObservation *o)
{ if(!o)return; g_free(o->identifier);g_free(o->evidence_identifier);g_free(o->evidence_sha256);
  g_free(o->raw_value);g_free(o->source_type);g_free(o->extraction_method);
  g_free(o->source_page_or_image);g_free(o->ocr_run_identifier);
  g_free(o->created_at_utc);g_free(o->factual_notes);g_free(o); }

BankObservationRevision *bank_observation_revision_new(const char *id,
    const char *corrected, const char *normalized, BankValidationState validation,
    const char *created, GError **error)
{
    if(!required(id)||!required(corrected)||!required(normalized)||!timestamp_valid(created)||
       (validation==BANK_VALIDATION_VALID&&!iban_analyzer_validate_canonical(normalized)))
    { invalid(error,"La révision bancaire est invalide."); return NULL; }
    BankObservationRevision *r=g_new0(BankObservationRevision,1);
    r->observation_identifier=g_strdup(id);r->corrected_iban=g_strdup(corrected);
    r->normalized_iban=g_strdup(normalized);r->validation=validation;
    r->origin=BANK_ORIGIN_HUMAN;r->created_at_utc=g_strdup(created);return r;
}
void bank_observation_revision_free(BankObservationRevision *r)
{ if(!r)return;g_free(r->observation_identifier);g_free(r->corrected_iban);
  g_free(r->normalized_iban);g_free(r->corrected_bic);g_free(r->normalized_bic);
  g_free(r->corrected_holder);g_free(r->created_at_utc);g_free(r->factual_notes);g_free(r); }

BankAccount *bank_account_new(const char *id,const char *iban,const char *bic,
    const char *institution,BankAccountState state,GError **error)
{
    char *canonical=iban_analyzer_normalize_canonical(iban);
    if(!required(id)||canonical==NULL||g_strcmp0(canonical,iban)!=0||
       !iban_analyzer_validate_canonical(iban)||
       bic_validator_check(bic)==BIC_VALIDATOR_INVALID)
    { g_free(canonical);invalid(error,"Le compte bancaire est invalide.");return NULL; }
    BankAccount *a=g_new0(BankAccount,1);a->entity_identifier=g_strdup(id);
    a->normalized_iban=canonical;a->country_code=g_strndup(iban,2);
    a->bic=bic_validator_normalize(bic);a->declared_institution=g_strdup(institution);
    a->state=state;return a;
}
void bank_account_free(BankAccount *a)
{if(!a)return;g_free(a->entity_identifier);g_free(a->normalized_iban);g_free(a->country_code);
 g_free(a->bic);g_free(a->declared_institution);g_free(a->factual_notes);g_free(a);}

DeclaredBankHolder *declared_bank_holder_new(const char *name,const char *evidence,
    const char *observation,BankOrigin origin,GError **error)
{if(!required(name)||!required(evidence)){invalid(error,"Le titulaire déclaré est invalide.");return NULL;}
 DeclaredBankHolder *h=g_new0(DeclaredBankHolder,1);h->declared_name=g_strdup(name);
 h->evidence_identifier=g_strdup(evidence);h->observation_identifier=g_strdup(observation);
 h->origin=origin;return h;}
void declared_bank_holder_free(DeclaredBankHolder *h)
{if(!h)return;g_free(h->declared_name);g_free(h->evidence_identifier);
 g_free(h->observation_identifier);g_free(h->factual_notes);g_free(h);}

static gboolean decimal_valid(const char *v)
{if(!required(v))return FALSE;gsize i=v[0]=='-'?1:0;gboolean digit=FALSE,dot=FALSE;
 for(;v[i];i++){if(g_ascii_isdigit(v[i]))digit=TRUE;else if(v[i]=='.'&&!dot)dot=TRUE;else return FALSE;}
 return digit&&v[strlen(v)-1]!='.';}
FinancialTransaction *financial_transaction_new(const char *type,const char *raw_date,
    const char *amount,const char *currency,const char *status,const char *evidence,
    const char *created,GError **error)
{if(!required(type)||!required(raw_date)||!decimal_valid(amount)||!required(currency)||
 !required(status)||!required(evidence)||!timestamp_valid(created))
 {invalid(error,"La transaction financière est invalide.");return NULL;}
 FinancialTransaction *t=g_new0(FinancialTransaction,1);t->transaction_type=g_strdup(type);
 t->raw_date=g_strdup(raw_date);t->amount_decimal=g_strdup(amount);t->currency=g_ascii_strup(currency,-1);
 t->observed_status=g_strdup(status);t->evidence_identifier=g_strdup(evidence);
 t->origin=BANK_ORIGIN_HUMAN;t->created_at_utc=g_strdup(created);return t;}
void financial_transaction_free(FinancialTransaction *t)
{if(!t)return;g_free(t->transaction_type);g_free(t->raw_date);g_free(t->normalized_date);
 g_free(t->amount_decimal);g_free(t->currency);g_free(t->observed_status);g_free(t->bank_reference);
 g_free(t->reference_type);g_free(t->uetr);g_free(t->debtor_account_identifier);
 g_free(t->creditor_account_identifier);g_free(t->declared_payer);g_free(t->declared_beneficiary);
 g_free(t->source_institution);g_free(t->evidence_identifier);g_free(t->observation_identifier);
 g_free(t->factual_notes);g_free(t->created_at_utc);g_free(t);}
FinancialReviewModel *financial_review_model_new(void)
{FinancialReviewModel *m=g_new0(FinancialReviewModel,1);m->candidates=g_ptr_array_new_with_free_func(
 (GDestroyNotify)bank_candidate_free);m->decision=BANK_DECISION_PROPOSED;return m;}
void financial_review_model_free(FinancialReviewModel *m)
{if(!m)return;g_clear_pointer(&m->candidates,g_ptr_array_unref);g_free(m);}
