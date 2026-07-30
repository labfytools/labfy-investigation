#include "models/identity_ocr.h"
struct IdentityFieldObservation {
    char *code, *raw_value, *corrected_value, *normalized_value, *note;
    char *confirmed_value, *value_quality;
    char *origin;
    double confidence;
    IdentityReviewStatus status;
    IdentitySourceBox box;
    guint order;
};
struct IdentityOcrRun {
    char *id, *evidence_id, *expected_sha256, *document_type, *document_side;
    char *languages, *profile, *version, *available_languages, *parameters;
    char *raw_text, *tsv, *factual_notes;
    char *corrected_transcription, *transcription_corrected_at;
    gboolean human_transcription;
    guint page;
    GPtrArray *fields;
    GBytes *preview;
};
static const char *const document_types[] = {
    "identity_card","passport","driving_licence","residence_permit","other"};
static const char *const document_sides[] = {
    "front","back","identity_page","other_page"};
static const char *const field_codes[] = {
    "document_type","issuing_country","issuing_authority","document_number",
    "surname","birth_name","given_names","sex_as_printed","nationality",
    "birth_date","birth_place","issue_date","expiry_date",
    "address_as_printed","mrz_line_1","mrz_line_2","mrz_line_3"};
static gboolean in_vocab(const char *value, const char *const *values,
    guint count)
{
    for (guint i=0; value != NULL && i<count; i++)
        if (g_str_equal(value, values[i])) return TRUE;
    return FALSE;
}
gboolean identity_ocr_document_type_is_valid(const char *v)
{ return in_vocab(v, document_types, G_N_ELEMENTS(document_types)); }
gboolean identity_ocr_document_side_is_valid(const char *v)
{ return in_vocab(v, document_sides, G_N_ELEMENTS(document_sides)); }
gboolean identity_ocr_field_code_is_valid(const char *v)
{ return in_vocab(v, field_codes, G_N_ELEMENTS(field_codes)); }
IdentityFieldObservation *identity_field_observation_new(
    const char *code, const char *raw, double confidence,
    const IdentitySourceBox *box, guint order)
{
    if (!identity_ocr_field_code_is_valid(code) || raw == NULL ||
        raw[0]=='\0' || confidence < -1.0 || confidence > 100.0) return NULL;
    IdentityFieldObservation *f=g_new0(IdentityFieldObservation,1);
    f->code=g_strdup(code); f->raw_value=g_strdup(raw);
    f->confidence=confidence; f->status=IDENTITY_REVIEW_PROPOSED;
    f->origin=g_strdup("ocr"); f->value_quality=g_strdup("complete"); f->order=order;
    if (box != NULL) f->box=*box;
    return f;
}
IdentityFieldObservation *identity_field_observation_new_manual(
    const char *code, const char *value, guint order)
{
    if (!identity_ocr_field_code_is_valid(code) || value == NULL ||
        value[0] == '\0') return NULL;
    IdentityFieldObservation *field =
        g_new0(IdentityFieldObservation, 1);
    field->code = g_strdup(code);
    field->corrected_value = g_strdup(value);
    field->confidence = -1.0;
    field->status = IDENTITY_REVIEW_PROPOSED;
    field->origin = g_strdup("manual_entry");
    field->value_quality = g_strdup("complete");
    field->order = order;
    return field;
}
IdentityFieldObservation *identity_field_observation_copy(
    const IdentityFieldObservation *f)
{
    if (f==NULL) return NULL;
    IdentityFieldObservation *c=f->raw_value != NULL
        ? identity_field_observation_new(
            f->code,f->raw_value,f->confidence,&f->box,f->order)
        : identity_field_observation_new_manual(
            f->code,f->corrected_value,f->order);
    c->corrected_value=g_strdup(f->corrected_value);
    c->normalized_value=g_strdup(f->normalized_value);
    c->confirmed_value=g_strdup(f->confirmed_value);
    g_free(c->value_quality);c->value_quality=g_strdup(f->value_quality);
    c->note=g_strdup(f->note); c->status=f->status;
    g_free(c->origin); c->origin=g_strdup(f->origin); return c;
}
void identity_field_observation_free(IdentityFieldObservation *f)
{
    if(f==NULL)return;
    g_free(f->code);g_free(f->raw_value);
    g_free(f->corrected_value);g_free(f->normalized_value);g_free(f->note);
    g_free(f->origin);g_free(f->confirmed_value);g_free(f->value_quality);g_free(f);
}
gboolean identity_field_observation_accept(IdentityFieldObservation *f)
{ if(f==NULL)return FALSE;f->status=IDENTITY_REVIEW_ACCEPTED;return TRUE; }
gboolean identity_field_observation_modify(IdentityFieldObservation *f,
    const char *value,const char *note)
{
    if(f==NULL||value==NULL||value[0]=='\0')return FALSE;
    g_free(f->corrected_value);f->corrected_value=g_strdup(value);
    g_free(f->note);f->note=g_strdup(note);g_free(f->origin);
    f->origin=g_strdup(f->raw_value == NULL
        ? "manual_entry" : "manual_override");
    f->status=IDENTITY_REVIEW_MODIFIED;
    return TRUE;
}
gboolean identity_field_observation_restore_raw(IdentityFieldObservation *f)
{
    if (f == NULL || f->raw_value == NULL) return FALSE;
    g_clear_pointer(&f->corrected_value, g_free);
    g_clear_pointer(&f->note, g_free);
    g_free(f->origin);
    f->origin = g_strdup("ocr");
    f->status = IDENTITY_REVIEW_PROPOSED;
    return TRUE;
}
void identity_field_observation_reject(IdentityFieldObservation *f)
{if(f!=NULL){f->status=IDENTITY_REVIEW_REJECTED;
 g_clear_pointer(&f->confirmed_value,g_free);}}
gboolean identity_field_observation_set_origin(IdentityFieldObservation*f,
 const char*origin)
{if(f==NULL||(!g_str_equal(origin,"ocr")&&!g_str_equal(origin,"mrz")&&
 !g_str_equal(origin,"manual_override")&&!g_str_equal(origin,"manual_entry")))
 return FALSE;
 g_free(f->origin);f->origin=g_strdup(origin);return TRUE;}
void identity_field_observation_mark_conflict(IdentityFieldObservation*f)
{if(f!=NULL){f->status=IDENTITY_REVIEW_CONFLICT;
 g_clear_pointer(&f->confirmed_value,g_free);}}
#define FG(name,type,field,zero) type identity_field_observation_get_##name(\
 const IdentityFieldObservation*f){return f!=NULL?f->field:zero;}
FG(code,const char*,code,NULL) FG(raw_value,const char*,raw_value,NULL)
FG(corrected_value,const char*,corrected_value,NULL)
FG(normalized_value,const char*,normalized_value,NULL)
FG(confirmed_value,const char*,confirmed_value,NULL)
FG(value_quality,const char*,value_quality,NULL)
FG(origin,const char*,origin,NULL)
FG(status,IdentityReviewStatus,status,IDENTITY_REVIEW_PROPOSED)
FG(confidence,double,confidence,-1.0)
FG(order,guint,order,0)
const IdentitySourceBox *identity_field_observation_get_box(
 const IdentityFieldObservation*f){return f!=NULL?&f->box:NULL;}
gboolean identity_field_observation_set_normalized_value(
 IdentityFieldObservation*f,const char*v)
{if(!f||!v||!v[0]||!g_utf8_validate(v,-1,NULL))return FALSE;
 g_free(f->normalized_value);f->normalized_value=g_strdup(v);return TRUE;}
gboolean identity_field_observation_set_value_quality(
 IdentityFieldObservation*f,const char*q)
{if(!f||(!g_str_equal(q,"complete")&&!g_str_equal(q,"partial")&&
 !g_str_equal(q,"uncertain")&&!g_str_equal(q,"invalid")))return FALSE;
 g_free(f->value_quality);f->value_quality=g_strdup(q);
 if(g_str_equal(q,"uncertain")||g_str_equal(q,"invalid"))
  g_clear_pointer(&f->confirmed_value,g_free);
 return TRUE;}
gboolean identity_field_observation_confirm(IdentityFieldObservation*f,
 const char*v)
{if(!f||!v||!v[0]||f->status==IDENTITY_REVIEW_REJECTED||
 f->status==IDENTITY_REVIEW_CONFLICT||
 g_strcmp0(f->value_quality,"uncertain")==0||
 g_strcmp0(f->value_quality,"invalid")==0)return FALSE;
 g_free(f->confirmed_value);f->confirmed_value=g_strdup(v);return TRUE;}
void identity_field_observation_clear_confirmation(IdentityFieldObservation*f)
{if(f)g_clear_pointer(&f->confirmed_value,g_free);}
gboolean identity_field_observation_is_human_confirmed(
 const IdentityFieldObservation*f){return f&&f->confirmed_value!=NULL;}
IdentityOcrRun *identity_ocr_run_new(const char *evidence_id,
 const char *sha,const char *type,const char *side,guint page,
 const char *languages,const char *profile)
{
    if(evidence_id==NULL||sha==NULL||strlen(sha)!=64||
       !identity_ocr_document_type_is_valid(type)||
       !identity_ocr_document_side_is_valid(side)||page==0||
       languages==NULL||profile==NULL)return NULL;
    IdentityOcrRun*r=g_new0(IdentityOcrRun,1);r->id=g_uuid_string_random();
    r->evidence_id=g_strdup(evidence_id);r->expected_sha256=g_strdup(sha);
    r->document_type=g_strdup(type);r->document_side=g_strdup(side);
    r->page=page;r->languages=g_strdup(languages);r->profile=g_strdup(profile);
    r->fields=g_ptr_array_new_with_free_func(
        (GDestroyNotify)identity_field_observation_free);return r;
}
IdentityOcrRun *identity_ocr_run_copy(const IdentityOcrRun*r)
{
    if(r==NULL)return NULL;
    IdentityOcrRun*c=identity_ocr_run_new(r->evidence_id,
      r->expected_sha256,r->document_type,r->document_side,r->page,
      r->languages,r->profile);g_free(c->id);c->id=g_strdup(r->id);
    identity_ocr_run_set_outputs(c,r->version,r->available_languages,
      r->parameters,r->raw_text,r->tsv);
    identity_ocr_run_set_preview(c,r->preview);
    identity_ocr_run_set_factual_notes(c,r->factual_notes);
    if (r->human_transcription)
      identity_ocr_run_set_corrected_transcription(c,
        r->corrected_transcription, r->transcription_corrected_at);
    for(guint i=0;i<r->fields->len;i++)identity_ocr_run_add_field(c,
      identity_field_observation_copy(g_ptr_array_index(r->fields,i)));
    return c;
}
void identity_ocr_run_free(IdentityOcrRun*r)
{
 if(r==NULL)return;
 g_free(r->id);g_free(r->evidence_id);g_free(r->expected_sha256);
 g_free(r->document_type);g_free(r->document_side);g_free(r->languages);
 g_free(r->profile);g_free(r->version);g_free(r->available_languages);
 g_free(r->parameters);g_free(r->raw_text);g_free(r->tsv);
 g_free(r->factual_notes);
 g_free(r->corrected_transcription);g_free(r->transcription_corrected_at);
 g_clear_pointer(&r->preview,g_bytes_unref);
 g_ptr_array_unref(r->fields);g_free(r);
}
void identity_ocr_run_set_outputs(IdentityOcrRun*r,const char*v,
 const char*a,const char*p,const char*t,const char*tsv)
{if(r==NULL)return;r->version=g_strdup(v);r->available_languages=g_strdup(a);
 r->parameters=g_strdup(p);r->raw_text=g_strdup(t);r->tsv=g_strdup(tsv);}
void identity_ocr_run_add_field(IdentityOcrRun*r,IdentityFieldObservation*f)
{if(r!=NULL&&f!=NULL)g_ptr_array_add(r->fields,f);}
gboolean identity_ocr_run_replace_evidence_id(
    IdentityOcrRun *run, const char *evidence_identifier)
{
    if (run == NULL || evidence_identifier == NULL ||
        evidence_identifier[0] == '\0') return FALSE;
    g_free(run->evidence_id);
    run->evidence_id = g_strdup(evidence_identifier);
    return run->evidence_id != NULL;
}
gboolean identity_ocr_run_replace_identifier(
    IdentityOcrRun *run, const char *identifier)
{
    if (run == NULL || identifier == NULL ||
        !g_uuid_string_is_valid(identifier)) return FALSE;
    g_free(run->id);
    run->id = g_strdup(identifier);
    return run->id != NULL;
}
void identity_ocr_run_set_factual_notes(IdentityOcrRun*r,const char*notes)
{if(r==NULL)return;g_free(r->factual_notes);r->factual_notes=g_strdup(notes);}
const char *identity_ocr_run_get_factual_notes(const IdentityOcrRun*r)
{return r!=NULL?r->factual_notes:NULL;}
gboolean identity_ocr_run_set_corrected_transcription(IdentityOcrRun*r,
 const char*text,const char*at)
{
 if(r==NULL||text==NULL||!g_utf8_validate(text,-1,NULL)||
    strlen(text)>1048576||(r->raw_text!=NULL&&r->raw_text[0]!='\0'&&
    text[0]=='\0'))return FALSE;
 g_free(r->corrected_transcription);r->corrected_transcription=g_strdup(text);
 g_free(r->transcription_corrected_at);
 r->transcription_corrected_at=g_strdup(at);
 r->human_transcription=TRUE;return TRUE;
}
void identity_ocr_run_reset_corrected_transcription(IdentityOcrRun*r)
{if(r==NULL)return;g_clear_pointer(&r->corrected_transcription,g_free);
 g_clear_pointer(&r->transcription_corrected_at,g_free);
 r->human_transcription=FALSE;}
const char *identity_ocr_run_get_corrected_transcription(const IdentityOcrRun*r)
{return r!=NULL?r->corrected_transcription:NULL;}
const char *identity_ocr_run_get_transcription_corrected_at(const IdentityOcrRun*r)
{return r!=NULL?r->transcription_corrected_at:NULL;}
gboolean identity_ocr_run_has_human_transcription(const IdentityOcrRun*r)
{return r!=NULL&&r->human_transcription;}
void identity_ocr_run_set_preview(IdentityOcrRun*r,GBytes*b)
{if(r==NULL)return;g_clear_pointer(&r->preview,g_bytes_unref);r->preview=b!=NULL?g_bytes_ref(b):NULL;}
GBytes *identity_ocr_run_get_preview(const IdentityOcrRun*r)
{return r!=NULL?r->preview:NULL;}
#define RG(name,type,field,zero) type identity_ocr_run_get_##name(\
 const IdentityOcrRun*r){return r!=NULL?r->field:zero;}
RG(identifier,const char*,id,NULL) RG(evidence_id,const char*,evidence_id,NULL)
RG(raw_text,const char*,raw_text,NULL) RG(tsv,const char*,tsv,NULL)
RG(document_type,const char*,document_type,NULL)
RG(document_side,const char*,document_side,NULL)
RG(languages,const char*,languages,NULL) RG(page,guint,page,0)
RG(expected_sha256,const char*,expected_sha256,NULL)
RG(profile,const char*,profile,NULL) RG(version,const char*,version,NULL)
RG(available_languages,const char*,available_languages,NULL)
const GPtrArray *identity_ocr_run_get_fields(const IdentityOcrRun*r)
{return r!=NULL?r->fields:NULL;}
