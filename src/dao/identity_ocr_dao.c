#include "dao/identity_ocr_dao.h"
#include "database/statement.h"
#include "database/transaction.h"
struct IdentityOcrDao{Database*database;};
static const char *review_status_text(IdentityReviewStatus status)
{
 if(status==IDENTITY_REVIEW_ACCEPTED)return "accepted";
 if(status==IDENTITY_REVIEW_MODIFIED)return "modified";
 if(status==IDENTITY_REVIEW_REJECTED)return "rejected";
 if(status==IDENTITY_REVIEW_CONFLICT)return "conflict";
 return "proposed";
}
static gboolean bind_text(DatabaseStatement*s,int i,const char*v)
{return v!=NULL?database_statement_bind_text(s,i,v):
 database_statement_bind_null(s,i);}
IdentityOcrDao *identity_ocr_dao_new(Database*d)
{if(!d)return NULL;IdentityOcrDao*dao=g_new0(IdentityOcrDao,1);dao->database=d;return dao;}
void identity_ocr_dao_free(IdentityOcrDao*d){g_free(d);}
gboolean identity_ocr_dao_insert(IdentityOcrDao*d,const char*person,
 const char*evidence,const IdentityOcrRun*r,const char*text_path,
 const char*text_sha,const char*tsv_path,const char*tsv_sha,
 const char*timestamp,GError**error)
{
 const char*sql="INSERT INTO identity_ocr_runs(id,evidence_id,expected_sha256,"
 "page_number,document_type,document_side,engine,engine_version,"
 "requested_languages,available_languages,parameters,preprocessing_profile,"
 "executed_at,status,text_relative_path,text_sha256,tsv_relative_path,"
 "tsv_sha256,corrected_transcription,transcription_is_human,"
 "transcription_corrected_at,transcription_origin)"
 " VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,'success',?,?,?,?,?,?,?,?);";
 DatabaseStatement*s=database_statement_prepare(d->database,sql);
 gboolean ok=s&&bind_text(s,1,identity_ocr_run_get_identifier(r))&&
  bind_text(s,2,evidence)&&bind_text(s,3,identity_ocr_run_get_expected_sha256(r))&&
  database_statement_bind_int64(s,4,identity_ocr_run_get_page(r))&&
  bind_text(s,5,identity_ocr_run_get_document_type(r))&&
  bind_text(s,6,identity_ocr_run_get_document_side(r))&&bind_text(s,7,"tesseract")&&
  bind_text(s,8,identity_ocr_run_get_version(r))&&
  bind_text(s,9,identity_ocr_run_get_languages(r))&&
  bind_text(s,10,identity_ocr_run_get_available_languages(r))&&
  bind_text(s,11,"GSubprocess sans shell; sorties texte et TSV")&&
  bind_text(s,12,identity_ocr_run_get_profile(r))&&bind_text(s,13,timestamp)&&
  bind_text(s,14,text_path)&&bind_text(s,15,text_sha)&&bind_text(s,16,tsv_path)&&
  bind_text(s,17,tsv_sha)&&
  bind_text(s,18,identity_ocr_run_get_corrected_transcription(r))&&
  database_statement_bind_int64(s,19,
    identity_ocr_run_has_human_transcription(r)?1:0)&&
  bind_text(s,20,identity_ocr_run_get_transcription_corrected_at(r))&&
  bind_text(s,21,identity_ocr_run_has_human_transcription(r)?"human":NULL)&&
  database_statement_step(s)==DATABASE_STATEMENT_STEP_DONE;
 database_statement_finalize(s);if(!ok)goto fail;
 char*obs=g_uuid_string_random();s=database_statement_prepare(d->database,
  "INSERT INTO identity_document_observations(id,person_id,evidence_id,"
  "ocr_run_id,document_type,document_side,page_number,review_state,observed_at,"
  "factual_notes) VALUES(?,?,?,?,?,?,?,'accepted',?,?);");
 ok=s&&bind_text(s,1,obs)&&bind_text(s,2,person)&&bind_text(s,3,evidence)&&
  bind_text(s,4,identity_ocr_run_get_identifier(r))&&
  bind_text(s,5,identity_ocr_run_get_document_type(r))&&
  bind_text(s,6,identity_ocr_run_get_document_side(r))&&
  database_statement_bind_int64(s,7,identity_ocr_run_get_page(r))&&
  bind_text(s,8,timestamp)&&
  bind_text(s,9,identity_ocr_run_get_factual_notes(r))&&
  database_statement_step(s)==DATABASE_STATEMENT_STEP_DONE;
 database_statement_finalize(s);if(!ok){g_free(obs);goto fail;}
 const GPtrArray*fields=identity_ocr_run_get_fields(r);
 for(guint i=0;fields&&i<fields->len;i++){IdentityFieldObservation*f=g_ptr_array_index((GPtrArray*)fields,i);
  IdentityReviewStatus status=identity_field_observation_get_status(f);
  const IdentitySourceBox*b=identity_field_observation_get_box(f);const char*id=identity_field_observation_get_identifier(f);
  s=database_statement_prepare(d->database,"INSERT INTO identity_field_observations("
   "id,observation_id,field_code,raw_value,corrected_value,normalized_value,"
   "confidence,review_status,"
   "origin,evidence_id,ocr_run_id,page_number,source_x,source_y,source_width,"
   "source_height,source_image_width,source_image_height,display_order,reviewed_at,"
   "confirmed_value,confirmation_state,value_quality)"
   " VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);");
  ok=s&&bind_text(s,1,id)&&bind_text(s,2,obs)&&bind_text(s,3,identity_field_observation_get_code(f))&&
   bind_text(s,4,identity_field_observation_get_raw_value(f))&&
   bind_text(s,5,identity_field_observation_get_corrected_value(f))&&
   bind_text(s,6,identity_field_observation_get_normalized_value(f))&&
   (identity_field_observation_get_confidence(f)>=0?
    database_statement_bind_double(s,7,identity_field_observation_get_confidence(f)):
    database_statement_bind_null(s,7))&&bind_text(s,8,review_status_text(status))&&
   bind_text(s,9,identity_field_observation_get_origin(f))&&bind_text(s,10,evidence)&&
   bind_text(s,11,identity_ocr_run_get_identifier(r))&&database_statement_bind_int64(s,12,identity_ocr_run_get_page(r));
  for(int column=13;ok&&column<=18;column++){gint value=0;if(b&&b->available){
   const gint values[]={b->x,b->y,b->width,b->height,b->image_width,b->image_height};
   value=values[column-13];ok=database_statement_bind_int64(s,column,value);
  }else ok=database_statement_bind_null(s,column);}
  ok=ok&&database_statement_bind_int64(s,19,identity_field_observation_get_order(f))&&
   bind_text(s,20,timestamp)&&
   bind_text(s,21,identity_field_observation_get_confirmed_value(f))&&
   bind_text(s,22,identity_field_observation_is_human_confirmed(f)
    ?"human_confirmed":"unconfirmed")&&
   bind_text(s,23,identity_field_observation_get_value_quality(f))&&
   database_statement_step(s)==DATABASE_STATEMENT_STEP_DONE;
  database_statement_finalize(s);if(!ok){g_free(obs);goto fail;}}
 g_free(obs);return TRUE;
fail:g_set_error_literal(error,g_quark_from_static_string("identity-ocr-dao"),1,
 "Impossible de conserver l’OCR d’identité.");return FALSE;
}

#define FREE_FIELD(record,field) g_free((record)->field)
void identity_ocr_run_record_free(IdentityOcrRunRecord*r)
{if(!r)return;FREE_FIELD(r,id);FREE_FIELD(r,evidence_id);FREE_FIELD(r,expected_sha256);
 FREE_FIELD(r,document_type);FREE_FIELD(r,document_side);FREE_FIELD(r,engine);
 FREE_FIELD(r,engine_version);FREE_FIELD(r,requested_languages);
 FREE_FIELD(r,available_languages);FREE_FIELD(r,parameters);
 FREE_FIELD(r,preprocessing_profile);FREE_FIELD(r,executed_at);FREE_FIELD(r,status);
 FREE_FIELD(r,error_message);FREE_FIELD(r,text_relative_path);FREE_FIELD(r,text_sha256);
 FREE_FIELD(r,tsv_relative_path);FREE_FIELD(r,tsv_sha256);
 FREE_FIELD(r,work_image_relative_path);FREE_FIELD(r,work_image_sha256);
 FREE_FIELD(r,corrected_transcription);
 FREE_FIELD(r,transcription_corrected_at);
 FREE_FIELD(r,transcription_origin);g_free(r);}
void identity_document_observation_record_free(IdentityDocumentObservationRecord*r)
{if(!r)return;FREE_FIELD(r,id);FREE_FIELD(r,person_id);FREE_FIELD(r,evidence_id);
 FREE_FIELD(r,ocr_run_id);FREE_FIELD(r,document_type);
 FREE_FIELD(r,issuing_country_declared);FREE_FIELD(r,document_side);
 FREE_FIELD(r,review_state);FREE_FIELD(r,observed_at);FREE_FIELD(r,factual_notes);g_free(r);}
void identity_field_observation_record_free(IdentityFieldObservationRecord*r)
{if(!r)return;FREE_FIELD(r,id);FREE_FIELD(r,observation_id);FREE_FIELD(r,field_code);
 FREE_FIELD(r,raw_value);FREE_FIELD(r,corrected_value);FREE_FIELD(r,normalized_value);
 FREE_FIELD(r,confirmed_value);FREE_FIELD(r,confirmation_state);
 FREE_FIELD(r,value_quality);
 FREE_FIELD(r,review_status);FREE_FIELD(r,origin);FREE_FIELD(r,evidence_id);
 FREE_FIELD(r,ocr_run_id);FREE_FIELD(r,reviewed_at);FREE_FIELD(r,review_note);g_free(r);}

static IdentityOcrRunRecord *read_run(DatabaseStatement*s)
{
 gint64 human=0;
 IdentityOcrRunRecord*r=g_new0(IdentityOcrRunRecord,1);gboolean ok=
  database_statement_column_text(s,0,&r->id)&&
  database_statement_column_text(s,1,&r->evidence_id)&&
  database_statement_column_text(s,2,&r->expected_sha256)&&
  database_statement_column_int64(s,3,&r->page_number)&&
  database_statement_column_text(s,4,&r->document_type)&&
  database_statement_column_text(s,5,&r->document_side)&&
  database_statement_column_text(s,6,&r->engine)&&
  database_statement_column_text(s,7,&r->engine_version)&&
  database_statement_column_text(s,8,&r->requested_languages)&&
  database_statement_column_text(s,9,&r->available_languages)&&
  database_statement_column_text(s,10,&r->parameters)&&
  database_statement_column_text(s,11,&r->preprocessing_profile)&&
  database_statement_column_text(s,12,&r->executed_at)&&
  database_statement_column_text(s,13,&r->status)&&
  database_statement_column_text(s,14,&r->error_message)&&
  database_statement_column_text(s,15,&r->text_relative_path)&&
  database_statement_column_text(s,16,&r->text_sha256)&&
  database_statement_column_text(s,17,&r->tsv_relative_path)&&
  database_statement_column_text(s,18,&r->tsv_sha256)&&
  database_statement_column_text(s,19,&r->work_image_relative_path)&&
  database_statement_column_text(s,20,&r->work_image_sha256)&&
  database_statement_column_text(s,21,&r->corrected_transcription)&&
  database_statement_column_int64(s,22,&human)&&
  database_statement_column_text(s,23,&r->transcription_corrected_at)&&
  database_statement_column_text(s,24,&r->transcription_origin);
 if(!ok){identity_ocr_run_record_free(r);return NULL;}
 r->transcription_is_human=human!=0;return r;
}
static IdentityDocumentObservationRecord *read_document(DatabaseStatement*s)
{
 IdentityDocumentObservationRecord*r=g_new0(IdentityDocumentObservationRecord,1);
 gboolean ok=database_statement_column_text(s,0,&r->id)&&
  database_statement_column_text(s,1,&r->person_id)&&
  database_statement_column_text(s,2,&r->evidence_id)&&
  database_statement_column_text(s,3,&r->ocr_run_id)&&
  database_statement_column_text(s,4,&r->document_type)&&
  database_statement_column_text(s,5,&r->issuing_country_declared)&&
  database_statement_column_text(s,6,&r->document_side)&&
  database_statement_column_int64(s,7,&r->page_number)&&
  database_statement_column_text(s,8,&r->review_state)&&
  database_statement_column_text(s,9,&r->observed_at)&&
  database_statement_column_text(s,10,&r->factual_notes);
 if(!ok){identity_document_observation_record_free(r);return NULL;}return r;
}
static IdentityFieldObservationRecord *read_field(DatabaseStatement*s)
{
 IdentityFieldObservationRecord*r=g_new0(IdentityFieldObservationRecord,1);
 gboolean ok=database_statement_column_text(s,0,&r->id)&&
  database_statement_column_text(s,1,&r->observation_id)&&
  database_statement_column_text(s,2,&r->field_code)&&
  database_statement_column_text(s,3,&r->raw_value)&&
  database_statement_column_text(s,4,&r->corrected_value)&&
  database_statement_column_text(s,5,&r->normalized_value);
 bool is_null=FALSE;
 ok=ok&&database_statement_column_is_null(s,6,&is_null);
 r->has_confidence=!is_null;
 ok=ok&&(!r->has_confidence||database_statement_column_double(s,6,&r->confidence))&&
  database_statement_column_text(s,7,&r->review_status)&&
  database_statement_column_text(s,8,&r->origin)&&
  database_statement_column_text(s,9,&r->evidence_id)&&
  database_statement_column_text(s,10,&r->ocr_run_id)&&
  database_statement_column_int64(s,11,&r->page_number);
 ok=ok&&database_statement_column_is_null(s,12,&is_null);
 r->has_source_box=!is_null;
 gint64*values[]={&r->source_x,&r->source_y,&r->source_width,&r->source_height,
  &r->source_image_width,&r->source_image_height};
 for(guint i=0;ok&&r->has_source_box&&i<G_N_ELEMENTS(values);i++)
  ok=database_statement_column_int64(s,12+(int)i,values[i]);
 ok=ok&&database_statement_column_int64(s,18,&r->display_order)&&
  database_statement_column_text(s,19,&r->reviewed_at)&&
  database_statement_column_text(s,20,&r->review_note)&&
  database_statement_column_text(s,21,&r->confirmed_value)&&
  database_statement_column_text(s,22,&r->confirmation_state)&&
  database_statement_column_text(s,23,&r->value_quality);
 if(!ok){identity_field_observation_record_free(r);return NULL;}return r;
}
static const char run_columns[]="id,evidence_id,expected_sha256,page_number,"
 "document_type,document_side,engine,engine_version,requested_languages,"
 "available_languages,parameters,preprocessing_profile,executed_at,status,"
 "error_message,text_relative_path,text_sha256,tsv_relative_path,tsv_sha256,"
 "work_image_relative_path,work_image_sha256,corrected_transcription,"
 "transcription_is_human,transcription_corrected_at,transcription_origin";
static const char document_columns[]="id,person_id,evidence_id,ocr_run_id,"
 "document_type,issuing_country_declared,document_side,page_number,"
 "review_state,observed_at,factual_notes";
static const char field_columns[]="id,observation_id,field_code,raw_value,"
 "corrected_value,normalized_value,confidence,review_status,origin,evidence_id,"
 "ocr_run_id,page_number,source_x,source_y,source_width,source_height,"
 "source_image_width,source_image_height,display_order,reviewed_at,review_note,"
 "confirmed_value,confirmation_state,value_quality";

typedef gpointer(*ReadRecord)(DatabaseStatement*);
static gpointer read_run_record(DatabaseStatement*s){return read_run(s);}
static gpointer read_document_record(DatabaseStatement*s){return read_document(s);}
static gpointer read_field_record(DatabaseStatement*s){return read_field(s);}
static gpointer find_one(IdentityOcrDao*d,const char*table,const char*columns,
 const char*id,ReadRecord read,GError**error)
{
 if(!d||!id){g_set_error_literal(error,g_quark_from_static_string("identity-ocr-dao"),2,
  "Lecture OCR invalide.");return NULL;}
 char*sql=g_strdup_printf("SELECT %s FROM %s WHERE id=?;",columns,table);
 DatabaseStatement*s=database_statement_prepare(d->database,sql);g_free(sql);
 if(!s||!database_statement_bind_text(s,1,id)){database_statement_finalize(s);
  g_set_error_literal(error,g_quark_from_static_string("identity-ocr-dao"),3,
   "Impossible de préparer la lecture OCR.");return NULL;}
 DatabaseStatementStepResult step=database_statement_step(s);
 gpointer result=step==DATABASE_STATEMENT_STEP_ROW?read(s):NULL;
 if(step==DATABASE_STATEMENT_STEP_ERROR||(step==DATABASE_STATEMENT_STEP_ROW&&!result))
  g_set_error_literal(error,g_quark_from_static_string("identity-ocr-dao"),4,
   "Impossible de lire l’enregistrement OCR.");
 database_statement_finalize(s);return result;
}
static GPtrArray *list_records(IdentityOcrDao*d,const char*table,
 const char*columns,const char*foreign_column,const char*id,ReadRecord read,
 GDestroyNotify destroy,GError**error)
{
 if(!d||!id){g_set_error_literal(error,g_quark_from_static_string("identity-ocr-dao"),2,
  "Lecture OCR invalide.");return NULL;}
 char*sql=g_strdup_printf("SELECT %s FROM %s WHERE %s=? ORDER BY rowid;",
  columns,table,foreign_column);
 DatabaseStatement*s=database_statement_prepare(d->database,sql);g_free(sql);
 GPtrArray*a=g_ptr_array_new_with_free_func(destroy);
 if(!s||!database_statement_bind_text(s,1,id))goto failed;
 for(;;){DatabaseStatementStepResult step=database_statement_step(s);
  if(step==DATABASE_STATEMENT_STEP_DONE)break;
  if(step!=DATABASE_STATEMENT_STEP_ROW)goto failed;
  gpointer record=read(s);if(!record)goto failed;g_ptr_array_add(a,record);}
 database_statement_finalize(s);return a;
failed:database_statement_finalize(s);g_ptr_array_unref(a);
 g_set_error_literal(error,g_quark_from_static_string("identity-ocr-dao"),4,
  "Impossible de lire les enregistrements OCR.");return NULL;
}
IdentityOcrRunRecord *identity_ocr_dao_find_run(IdentityOcrDao*d,const char*i,GError**e)
{return find_one(d,"identity_ocr_runs",run_columns,i,read_run_record,e);}
GPtrArray *identity_ocr_dao_list_runs_by_evidence(IdentityOcrDao*d,const char*i,GError**e)
{return list_records(d,"identity_ocr_runs",run_columns,"evidence_id",i,
 read_run_record,(GDestroyNotify)identity_ocr_run_record_free,e);}
IdentityDocumentObservationRecord *identity_ocr_dao_find_document(
 IdentityOcrDao*d,const char*i,GError**e)
{return find_one(d,"identity_document_observations",document_columns,i,
 read_document_record,e);}
GPtrArray *identity_ocr_dao_list_documents_by_person(IdentityOcrDao*d,const char*i,GError**e)
{return list_records(d,"identity_document_observations",document_columns,
 "person_id",i,read_document_record,
 (GDestroyNotify)identity_document_observation_record_free,e);}
GPtrArray *identity_ocr_dao_list_documents_by_evidence(IdentityOcrDao*d,
 const char*i,GError**e)
{return list_records(d,"identity_document_observations",document_columns,
 "evidence_id",i,read_document_record,
 (GDestroyNotify)identity_document_observation_record_free,e);}
IdentityFieldObservationRecord *identity_ocr_dao_find_field(IdentityOcrDao*d,
 const char*i,GError**e)
{return find_one(d,"identity_field_observations",field_columns,i,
 read_field_record,e);}
GPtrArray *identity_ocr_dao_list_fields_by_document(IdentityOcrDao*d,const char*i,GError**e)
{return list_records(d,"identity_field_observations",field_columns,
 "observation_id",i,read_field_record,
 (GDestroyNotify)identity_field_observation_record_free,e);}
GPtrArray *identity_ocr_dao_list_confirmed_fields(IdentityOcrDao*d,
 const char*i,GError**e)
{
 if(!d||!i){g_set_error_literal(e,g_quark_from_static_string("identity-ocr-dao"),2,
  "Lecture OCR invalide.");return NULL;}
 char*sql=g_strdup_printf("SELECT %s FROM identity_field_observations "
  "WHERE observation_id=? AND confirmation_state='human_confirmed' "
  "AND confirmed_value IS NOT NULL AND review_status IN ('accepted','modified') "
  "AND value_quality IN ('complete','partial') ORDER BY display_order,rowid;",
  field_columns);
 DatabaseStatement*s=database_statement_prepare(d->database,sql);g_free(sql);
 GPtrArray*a=g_ptr_array_new_with_free_func(
  (GDestroyNotify)identity_field_observation_record_free);
 if(!s||!database_statement_bind_text(s,1,i))goto failed;
 for(;;){DatabaseStatementStepResult step=database_statement_step(s);
  if(step==DATABASE_STATEMENT_STEP_DONE)break;
  if(step!=DATABASE_STATEMENT_STEP_ROW)goto failed;
  IdentityFieldObservationRecord*r=read_field(s);if(!r)goto failed;g_ptr_array_add(a,r);}
 database_statement_finalize(s);return a;
failed:database_statement_finalize(s);g_ptr_array_unref(a);
 g_set_error_literal(e,g_quark_from_static_string("identity-ocr-dao"),4,
  "Impossible de lire les champs OCR confirmés.");return NULL;
}

IdentityOcrRun *identity_ocr_dao_load_run(
 IdentityOcrDao*d,const char*root,const char*identifier,
 char**person_identifier,GError**error)
{
 IdentityOcrRunRecord*record=identity_ocr_dao_find_run(d,identifier,error);
 IdentityOcrRun*run=NULL;GPtrArray*documents=NULL;char*raw=NULL;char*tsv=NULL;
 if(person_identifier!=NULL)*person_identifier=NULL;
 if(record==NULL||root==NULL)goto cleanup;
 char*raw_path=g_build_filename(root,record->text_relative_path,NULL);
 char*tsv_path=g_build_filename(root,record->tsv_relative_path,NULL);
 if(!g_file_get_contents(raw_path,&raw,NULL,error)||
    !g_file_get_contents(tsv_path,&tsv,NULL,error)){
  g_free(raw_path);g_free(tsv_path);goto cleanup;}
 g_free(raw_path);g_free(tsv_path);
 run=identity_ocr_run_new(record->evidence_id,record->expected_sha256,
  record->document_type,record->document_side,(guint)record->page_number,
  record->requested_languages,record->preprocessing_profile);
 if(run==NULL||!identity_ocr_run_replace_identifier(run,record->id))
  goto cleanup;
 identity_ocr_run_set_outputs(run,record->engine_version,
  record->available_languages,record->parameters,raw,tsv);
 if(record->transcription_is_human)
  identity_ocr_run_set_corrected_transcription(run,
   record->corrected_transcription,record->transcription_corrected_at);
 documents=identity_ocr_dao_list_documents_by_evidence(
  d,record->evidence_id,error);
 for(guint i=0;documents!=NULL&&i<documents->len;i++){
  IdentityDocumentObservationRecord*document=g_ptr_array_index(documents,i);
  if(g_strcmp0(document->ocr_run_id,identifier)!=0)continue;
  identity_ocr_run_set_factual_notes(run,document->factual_notes);
  if(person_identifier!=NULL)*person_identifier=g_strdup(document->person_id);
  GPtrArray*fields=identity_ocr_dao_list_fields_by_document(
   d,document->id,error);
  for(guint j=0;fields!=NULL&&j<fields->len;j++){
   IdentityFieldObservationRecord*f=g_ptr_array_index(fields,j);
   IdentitySourceBox box={.page=(gint)f->page_number,
    .x=(gint)f->source_x,.y=(gint)f->source_y,
    .width=(gint)f->source_width,.height=(gint)f->source_height,
    .image_width=(gint)f->source_image_width,
    .image_height=(gint)f->source_image_height,
    .available=f->has_source_box};
   IdentityFieldObservation*field=f->raw_value!=NULL
    ?identity_field_observation_new(f->field_code,f->raw_value,
      f->has_confidence?f->confidence:-1.0,&box,(guint)f->display_order)
    :identity_field_observation_new_manual(f->field_code,
      f->corrected_value,(guint)f->display_order);
   if(field==NULL)continue;
   identity_field_observation_replace_identifier(field,f->id);
   if(f->corrected_value!=NULL)
    identity_field_observation_modify(field,f->corrected_value,f->review_note);
   if(g_strcmp0(f->review_status,"accepted")==0)
    identity_field_observation_accept(field);
   else if(g_strcmp0(f->review_status,"rejected")==0)
    identity_field_observation_reject(field);
   else if(g_strcmp0(f->review_status,"conflict")==0)
    identity_field_observation_mark_conflict(field);
   identity_field_observation_set_origin(field,f->origin);
   if(f->normalized_value!=NULL)
    identity_field_observation_set_normalized_value(field,f->normalized_value);
   identity_field_observation_set_value_quality(field,f->value_quality);
   if(g_strcmp0(f->confirmation_state,"human_confirmed")==0)
    identity_field_observation_confirm(field,f->confirmed_value);
   identity_ocr_run_add_field(run,field);
  }
  g_clear_pointer(&fields,g_ptr_array_unref);
  break;
 }
cleanup:
 if(record==NULL||run==NULL||documents==NULL){
  identity_ocr_run_free(run);run=NULL;
  if(error!=NULL&&*error==NULL)g_set_error_literal(error,
   g_quark_from_static_string("identity-ocr-dao"),5,
   "Impossible de recharger l’analyse OCR persistée.");}
 g_free(raw);g_free(tsv);g_clear_pointer(&documents,g_ptr_array_unref);
 identity_ocr_run_record_free(record);return run;
}

static gboolean update_review_fields(IdentityOcrDao*d,const char*observation,
 const IdentityOcrRun*run,const char*timestamp)
{
 const GPtrArray*fields=identity_ocr_run_get_fields(run);
 for(guint i=0;fields!=NULL&&i<fields->len;i++){
  IdentityFieldObservation*f=g_ptr_array_index((GPtrArray*)fields,i);
  const IdentitySourceBox*b=identity_field_observation_get_box(f);
  const char*id=identity_field_observation_get_identifier(f);
  DatabaseStatement*s=database_statement_prepare(d->database,
   "INSERT INTO identity_field_observations(id,observation_id,field_code,"
   "raw_value,corrected_value,normalized_value,confidence,review_status,origin,evidence_id,"
   "ocr_run_id,page_number,source_x,source_y,source_width,source_height,"
   "source_image_width,source_image_height,display_order,reviewed_at,"
   "confirmed_value,confirmation_state,value_quality)"
   " VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);");
  gboolean ok=s&&bind_text(s,1,id)&&bind_text(s,2,observation)&&
   bind_text(s,3,identity_field_observation_get_code(f))&&
   bind_text(s,4,identity_field_observation_get_raw_value(f))&&
   bind_text(s,5,identity_field_observation_get_corrected_value(f))&&
   bind_text(s,6,identity_field_observation_get_normalized_value(f))&&
   (identity_field_observation_get_confidence(f)>=0
    ?database_statement_bind_double(s,7,
      identity_field_observation_get_confidence(f))
    :database_statement_bind_null(s,7))&&
   bind_text(s,8,review_status_text(
      identity_field_observation_get_status(f)))&&
   bind_text(s,9,identity_field_observation_get_origin(f))&&
   bind_text(s,10,identity_ocr_run_get_evidence_id(run))&&
   bind_text(s,11,identity_ocr_run_get_identifier(run))&&
   database_statement_bind_int64(s,12,identity_ocr_run_get_page(run));
  const gint values[]={b!=NULL?b->x:0,b!=NULL?b->y:0,
   b!=NULL?b->width:0,b!=NULL?b->height:0,
   b!=NULL?b->image_width:0,b!=NULL?b->image_height:0};
  for(gint column=13;ok&&column<=18;column++)
   ok=b!=NULL&&b->available
    ?database_statement_bind_int64(s,column,values[column-13])
    :database_statement_bind_null(s,column);
  ok=ok&&database_statement_bind_int64(s,19,
    identity_field_observation_get_order(f))&&
   bind_text(s,20,timestamp)&&
   bind_text(s,21,identity_field_observation_get_confirmed_value(f))&&
   bind_text(s,22,identity_field_observation_is_human_confirmed(f)
    ?"human_confirmed":"unconfirmed")&&
   bind_text(s,23,identity_field_observation_get_value_quality(f))&&
   database_statement_step(s)==DATABASE_STATEMENT_STEP_DONE;
  database_statement_finalize(s);if(!ok)return FALSE;
 }
 return TRUE;
}

gboolean identity_ocr_dao_update_review(IdentityOcrDao*d,
 const IdentityOcrRun*run,const char*timestamp,GError**error)
{
 if(d==NULL||run==NULL||timestamp==NULL||
    !database_transaction_begin(d->database))goto fail;
 DatabaseStatement*s=database_statement_prepare(d->database,
  "UPDATE identity_ocr_runs SET corrected_transcription=?,"
  "transcription_is_human=?,transcription_corrected_at=?,"
  "transcription_origin=? WHERE id=? AND evidence_id=?;");
 gboolean ok=s&&bind_text(s,1,
   identity_ocr_run_get_corrected_transcription(run))&&
  database_statement_bind_int64(s,2,
   identity_ocr_run_has_human_transcription(run)?1:0)&&
  bind_text(s,3,identity_ocr_run_get_transcription_corrected_at(run))&&
  bind_text(s,4,identity_ocr_run_has_human_transcription(run)?"human":NULL)&&
  bind_text(s,5,identity_ocr_run_get_identifier(run))&&
  bind_text(s,6,identity_ocr_run_get_evidence_id(run))&&
  database_statement_step(s)==DATABASE_STATEMENT_STEP_DONE;
 database_statement_finalize(s);
 IdentityDocumentObservationRecord*document=NULL;
 GPtrArray*documents=ok?identity_ocr_dao_list_documents_by_evidence(
  d,identity_ocr_run_get_evidence_id(run),error):NULL;
 for(guint i=0;documents!=NULL&&i<documents->len;i++){
  IdentityDocumentObservationRecord*candidate=g_ptr_array_index(documents,i);
  if(g_strcmp0(candidate->ocr_run_id,
      identity_ocr_run_get_identifier(run))==0){document=candidate;break;}}
 if(document==NULL)ok=FALSE;
 if(ok){s=database_statement_prepare(d->database,
   "UPDATE identity_document_observations SET factual_notes=?,"
   "review_state='accepted',observed_at=? WHERE id=?;");
  ok=s&&bind_text(s,1,identity_ocr_run_get_factual_notes(run))&&
   bind_text(s,2,timestamp)&&bind_text(s,3,document->id)&&
   database_statement_step(s)==DATABASE_STATEMENT_STEP_DONE;
  database_statement_finalize(s);}
 if(ok){s=database_statement_prepare(d->database,
   "DELETE FROM identity_field_observations WHERE observation_id=?;");
  ok=s&&bind_text(s,1,document->id)&&
   database_statement_step(s)==DATABASE_STATEMENT_STEP_DONE;
  database_statement_finalize(s);}
 if(ok)ok=update_review_fields(d,document->id,run,timestamp);
 g_clear_pointer(&documents,g_ptr_array_unref);
 if(ok&&database_transaction_commit(d->database))return TRUE;
 database_transaction_rollback(d->database);
fail:
 g_set_error_literal(error,g_quark_from_static_string("identity-ocr-dao"),6,
  "Impossible d’enregistrer la révision OCR.");return FALSE;
}
