#include "models/identity_traceability.h"
#include <string.h>

static gboolean in_values(const char *value, const char *const *values,
    gsize count)
{
    if (value == NULL) return FALSE;
    for (gsize i = 0; i < count; i++)
        if (g_str_equal(value, values[i])) return TRUE;
    return FALSE;
}
static gboolean text(const char *value)
{ return value != NULL && value[0] != '\0' && g_utf8_validate(value,-1,NULL); }
static gboolean optional_text(const char *value)
{ return value == NULL || (text(value) && strlen(value) <= 65536); }
static gboolean uuid_or_null(const char *value)
{ return value == NULL || g_uuid_string_is_valid(value); }
static gboolean timestamp(const char *value)
{ return value != NULL && strlen(value) == 20; }

gboolean identity_traceability_authenticity_status_valid(const char *value)
{
    static const char *const values[]={"indeterminate","presumed_authentic",
        "suspicious","presumed_forged","confirmed_forged"};
    return in_values(value,values,G_N_ELEMENTS(values));
}
gboolean identity_traceability_identity_misuse_status_valid(const char *value)
{
    static const char *const values[]={"indeterminate","presumed","confirmed"};
    return in_values(value,values,G_N_ELEMENTS(values));
}
gboolean identity_traceability_relation_type_valid(const char *value)
{
    static const char *const values[]={"identity_observed_in",
        "document_presented_in_name_of","declared_holder_in",
        "data_extracted_from"};
    return in_values(value,values,G_N_ELEMENTS(values));
}
gboolean identity_traceability_identification_status_valid(const char *value)
{
    static const char *const values[]={"unknown","unverified","presumed",
        "partially_identified","confirmed","disputed"};
    return in_values(value,values,G_N_ELEMENTS(values));
}
gboolean identity_traceability_value_quality_valid(const char *value)
{
    static const char *const values[]={"complete","partial","uncertain","invalid"};
    return in_values(value,values,G_N_ELEMENTS(values));
}
gboolean identity_traceability_field_is_projectable(const char *review,
    const char *quality,const char *confirmation,const char *confirmed)
{
    return confirmed != NULL && confirmed[0] != '\0' &&
        g_strcmp0(confirmation,"human_confirmed")==0 &&
        (g_strcmp0(review,"accepted")==0||g_strcmp0(review,"modified")==0) &&
        (g_strcmp0(quality,"complete")==0||g_strcmp0(quality,"partial")==0);
}

DocumentAuthenticityAssessment *document_authenticity_assessment_new(
 const char *id,const char *evidence,const char *run,const char *status,
 const char *justification,const char *at,const char *previous,const char *note)
{
    if(!g_uuid_string_is_valid(id)||!g_uuid_string_is_valid(evidence)||
       !uuid_or_null(run)||!uuid_or_null(previous)||
       !identity_traceability_authenticity_status_valid(status)||
       !timestamp(at)||!optional_text(justification)||!optional_text(note)||
       (g_strcmp0(status,"indeterminate")!=0&&!text(justification)))return NULL;
    DocumentAuthenticityAssessment*a=g_new0(DocumentAuthenticityAssessment,1);
    a->identifier=g_strdup(id);a->evidence_identifier=g_strdup(evidence);
    a->ocr_run_identifier=g_strdup(run);a->status=g_strdup(status);
    a->justification=g_strdup(justification);a->assessed_at=g_strdup(at);
    a->previous_identifier=g_strdup(previous);a->technical_note=g_strdup(note);
    a->origin=g_strdup("human");return a;
}
DocumentAuthenticityAssessment *document_authenticity_assessment_copy(
 const DocumentAuthenticityAssessment*a)
{return a?document_authenticity_assessment_new(a->identifier,
 a->evidence_identifier,a->ocr_run_identifier,a->status,a->justification,
 a->assessed_at,a->previous_identifier,a->technical_note):NULL;}
void document_authenticity_assessment_free(DocumentAuthenticityAssessment*a)
{if(!a)return;g_free(a->identifier);g_free(a->evidence_identifier);
 g_free(a->ocr_run_identifier);g_free(a->status);g_free(a->justification);
 g_free(a->assessed_at);g_free(a->previous_identifier);
 g_free(a->technical_note);g_free(a->origin);g_free(a);}
#define AUTH_GETTER(name,field) \
const char *document_authenticity_assessment_get_##name( \
 const DocumentAuthenticityAssessment*a){return a?a->field:NULL;}
AUTH_GETTER(identifier,identifier)
AUTH_GETTER(evidence_identifier,evidence_identifier)
AUTH_GETTER(ocr_run_identifier,ocr_run_identifier)
AUTH_GETTER(status,status)
AUTH_GETTER(justification,justification)
AUTH_GETTER(assessed_at,assessed_at)
AUTH_GETTER(previous_identifier,previous_identifier)
AUTH_GETTER(technical_note,technical_note)
AUTH_GETTER(origin,origin)

DocumentIdentityMisuseAssessment *document_identity_misuse_assessment_new(
 const char *id,const char *evidence,const char *run,const char *status,
 const char *justification,const char *at,const char *previous)
{
 if(!g_uuid_string_is_valid(id)||!g_uuid_string_is_valid(evidence)||
    !uuid_or_null(run)||!uuid_or_null(previous)||
    !identity_traceability_identity_misuse_status_valid(status)||
    !timestamp(at)||!optional_text(justification)||
    (g_strcmp0(status,"indeterminate")!=0&&!text(justification)))return NULL;
 DocumentIdentityMisuseAssessment*a=g_new0(DocumentIdentityMisuseAssessment,1);
 a->identifier=g_strdup(id);a->evidence_identifier=g_strdup(evidence);
 a->ocr_run_identifier=g_strdup(run);a->status=g_strdup(status);
 a->justification=g_strdup(justification);a->assessed_at=g_strdup(at);
 a->previous_identifier=g_strdup(previous);a->origin=g_strdup("human");return a;
}
DocumentIdentityMisuseAssessment *document_identity_misuse_assessment_copy(
 const DocumentIdentityMisuseAssessment*a)
{return a?document_identity_misuse_assessment_new(a->identifier,
 a->evidence_identifier,a->ocr_run_identifier,a->status,a->justification,
 a->assessed_at,a->previous_identifier):NULL;}
void document_identity_misuse_assessment_free(DocumentIdentityMisuseAssessment*a)
{if(!a)return;g_free(a->identifier);g_free(a->evidence_identifier);
 g_free(a->ocr_run_identifier);g_free(a->status);g_free(a->justification);
 g_free(a->assessed_at);g_free(a->previous_identifier);g_free(a->origin);g_free(a);}
#define MISUSE_GETTER(name,field) \
const char *document_identity_misuse_assessment_get_##name( \
 const DocumentIdentityMisuseAssessment*a){return a?a->field:NULL;}
MISUSE_GETTER(identifier,identifier)
MISUSE_GETTER(evidence_identifier,evidence_identifier)
MISUSE_GETTER(ocr_run_identifier,ocr_run_identifier)
MISUSE_GETTER(status,status)
MISUSE_GETTER(justification,justification)
MISUSE_GETTER(assessed_at,assessed_at)
MISUSE_GETTER(previous_identifier,previous_identifier)
MISUSE_GETTER(origin,origin)

PersonEvidenceFactualRelation *person_evidence_factual_relation_new(
 const char *id,const char *person,const char *evidence,const char *run,
 const char *type,const char *note,const char *at,gboolean active)
{
 if(!g_uuid_string_is_valid(id)||!g_uuid_string_is_valid(person)||
    !g_uuid_string_is_valid(evidence)||!uuid_or_null(run)||
    !identity_traceability_relation_type_valid(type)||!optional_text(note)||
    !timestamp(at))return NULL;
 PersonEvidenceFactualRelation*r=g_new0(PersonEvidenceFactualRelation,1);
 r->identifier=g_strdup(id);r->person_identifier=g_strdup(person);
 r->evidence_identifier=g_strdup(evidence);r->ocr_run_identifier=g_strdup(run);
 r->relation_type=g_strdup(type);r->factual_note=g_strdup(note);
 r->observed_at=g_strdup(at);r->origin=g_strdup("human");r->active=active;return r;
}
PersonEvidenceFactualRelation *person_evidence_factual_relation_copy(
 const PersonEvidenceFactualRelation*r)
{return r?person_evidence_factual_relation_new(r->identifier,
 r->person_identifier,r->evidence_identifier,r->ocr_run_identifier,
 r->relation_type,r->factual_note,r->observed_at,r->active):NULL;}
void person_evidence_factual_relation_free(PersonEvidenceFactualRelation*r)
{if(!r)return;g_free(r->identifier);g_free(r->person_identifier);
 g_free(r->evidence_identifier);g_free(r->ocr_run_identifier);
 g_free(r->relation_type);g_free(r->factual_note);g_free(r->observed_at);
 g_free(r->origin);g_free(r);}
#define FACTUAL_GETTER(name,field) \
const char *person_evidence_factual_relation_get_##name( \
 const PersonEvidenceFactualRelation*r){return r?r->field:NULL;}
FACTUAL_GETTER(identifier,identifier)
FACTUAL_GETTER(person_identifier,person_identifier)
FACTUAL_GETTER(evidence_identifier,evidence_identifier)
FACTUAL_GETTER(ocr_run_identifier,ocr_run_identifier)
FACTUAL_GETTER(relation_type,relation_type)
FACTUAL_GETTER(factual_note,factual_note)
FACTUAL_GETTER(observed_at,observed_at)
FACTUAL_GETTER(origin,origin)
gboolean person_evidence_factual_relation_get_active(
 const PersonEvidenceFactualRelation*r){return r?r->active:FALSE;}
void person_role_vocabulary_entry_free(PersonRoleVocabularyEntry*e)
{if(!e)return;g_free(e->code);g_free(e->label);g_free(e->description);g_free(e);}
void identification_status_vocabulary_entry_free(
 IdentificationStatusVocabularyEntry*e)
{person_role_vocabulary_entry_free(e);}
