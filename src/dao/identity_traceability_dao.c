#include "dao/identity_traceability_dao.h"
#include "database/statement.h"
struct IdentityTraceabilityDao{Database*database;};
static GQuark domain(void){return g_quark_from_static_string("identity-traceability-dao");}
static void fail(GError**e,const char*m){if(e&&*e==NULL)g_set_error_literal(e,domain(),1,m);}
static gboolean bind(DatabaseStatement*s,int i,const char*v)
{return v?database_statement_bind_text(s,i,v):database_statement_bind_null(s,i);}
IdentityTraceabilityDao *identity_traceability_dao_new(Database*d)
{if(!d)return NULL;IdentityTraceabilityDao*dao=g_new0(IdentityTraceabilityDao,1);
 dao->database=d;return dao;}
void identity_traceability_dao_free(IdentityTraceabilityDao*d){g_free(d);}

gboolean identity_traceability_dao_insert_authenticity(
 IdentityTraceabilityDao*d,const DocumentAuthenticityAssessment*a,GError**e)
{
 DocumentAuthenticityAssessment*valid=a?document_authenticity_assessment_copy(a):NULL;
 if(!d||!valid){fail(e,"Évaluation d’authenticité invalide.");return FALSE;}
 const char*run=document_authenticity_assessment_get_ocr_run_identifier(valid);
 if(run){
  DatabaseStatement*check=database_statement_prepare(d->database,
   "SELECT 1 FROM identity_ocr_runs WHERE id=? AND evidence_id=?;");
  gboolean compatible=check&&bind(check,1,run)&&bind(check,2,
   document_authenticity_assessment_get_evidence_identifier(valid))&&
   database_statement_step(check)==DATABASE_STATEMENT_STEP_ROW;
  database_statement_finalize(check);
  if(!compatible){document_authenticity_assessment_free(valid);
   fail(e,"L’exécution OCR ne correspond pas à la preuve évaluée.");return FALSE;}
 }
 const char*previous=document_authenticity_assessment_get_previous_identifier(valid);
 if(previous){
  DatabaseStatement*check=database_statement_prepare(d->database,
   "SELECT 1 FROM document_authenticity_assessments WHERE id=? AND evidence_id=?;");
  gboolean compatible=check&&bind(check,1,previous)&&bind(check,2,
   document_authenticity_assessment_get_evidence_identifier(valid))&&
   database_statement_step(check)==DATABASE_STATEMENT_STEP_ROW;
  database_statement_finalize(check);
  if(!compatible){document_authenticity_assessment_free(valid);
   fail(e,"L’appréciation précédente ne correspond pas à la preuve évaluée.");return FALSE;}
 }
 DatabaseStatement*s=database_statement_prepare(d->database,
  "INSERT INTO document_authenticity_assessments VALUES(?,?,?,?,?,?,?,?,?);");
 gboolean ok=s&&bind(s,1,document_authenticity_assessment_get_identifier(valid))&&
  bind(s,2,document_authenticity_assessment_get_evidence_identifier(valid))&&
  bind(s,3,run)&&bind(s,4,document_authenticity_assessment_get_status(valid))&&
  bind(s,5,document_authenticity_assessment_get_justification(valid))&&
  bind(s,6,document_authenticity_assessment_get_assessed_at(valid))&&
  bind(s,7,document_authenticity_assessment_get_previous_identifier(valid))&&
  bind(s,8,document_authenticity_assessment_get_technical_note(valid))&&bind(s,9,"human")&&
  database_statement_step(s)==DATABASE_STATEMENT_STEP_DONE;
 database_statement_finalize(s);document_authenticity_assessment_free(valid);
 if(!ok)fail(e,"Impossible de conserver l’évaluation d’authenticité.");
 return ok;
}
static DocumentAuthenticityAssessment *read_auth(DatabaseStatement*s)
{
 char *v[9]={0};gboolean ok=TRUE;
 for(int i=0;i<9;i++)ok=ok&&database_statement_column_text(s,i,&v[i]);
 DocumentAuthenticityAssessment*a=ok?document_authenticity_assessment_new(
  v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7]):NULL;
 for(int i=0;i<9;i++)g_free(v[i]);
 return a;
}
static GPtrArray *auth_query(IdentityTraceabilityDao*d,const char*sql,
 const char*value,GError**e)
{
 if(!d||!value){fail(e,"Lecture d’authenticité invalide.");return NULL;}
 DatabaseStatement*s=database_statement_prepare(d->database,sql);
 GPtrArray*a=g_ptr_array_new_with_free_func(
  (GDestroyNotify)document_authenticity_assessment_free);
 if(!s||!bind(s,1,value))goto bad;
 for(;;){DatabaseStatementStepResult step=database_statement_step(s);
  if(step==DATABASE_STATEMENT_STEP_DONE)break;
  if(step!=DATABASE_STATEMENT_STEP_ROW)goto bad;
  DocumentAuthenticityAssessment*r=read_auth(s);if(!r)goto bad;g_ptr_array_add(a,r);}
 database_statement_finalize(s);return a;
bad:database_statement_finalize(s);g_ptr_array_unref(a);
 fail(e,"Impossible de lire l’historique d’authenticité.");return NULL;
}
DocumentAuthenticityAssessment *identity_traceability_dao_find_authenticity(
 IdentityTraceabilityDao*d,const char*id,GError**e)
{
 GPtrArray*a=auth_query(d,"SELECT * FROM document_authenticity_assessments "
  "WHERE id=? ORDER BY assessed_at,id;",id,e);if(!a)return NULL;
 DocumentAuthenticityAssessment*r=a->len?document_authenticity_assessment_copy(
  g_ptr_array_index(a,0)):NULL;g_ptr_array_unref(a);return r;
}
GPtrArray *identity_traceability_dao_list_authenticity(
 IdentityTraceabilityDao*d,const char*id,GError**e)
{return auth_query(d,"SELECT * FROM document_authenticity_assessments "
 "WHERE evidence_id=? ORDER BY assessed_at,id;",id,e);}
DocumentAuthenticityAssessment *identity_traceability_dao_current_authenticity(
 IdentityTraceabilityDao*d,const char*id,GError**e)
{
 GPtrArray*a=auth_query(d,"SELECT a.* FROM document_authenticity_assessments a "
  "WHERE a.evidence_id=? AND NOT EXISTS(SELECT 1 FROM "
  "document_authenticity_assessments n WHERE n.previous_assessment_id=a.id) "
  "ORDER BY a.assessed_at DESC,a.id DESC LIMIT 1;",id,e);
 if(!a)return NULL;
 DocumentAuthenticityAssessment*r=a->len?
  document_authenticity_assessment_copy(g_ptr_array_index(a,0)):NULL;
 g_ptr_array_unref(a);return r;
}
gboolean identity_traceability_dao_insert_identity_misuse(
 IdentityTraceabilityDao*d,const DocumentIdentityMisuseAssessment*a,GError**e)
{
 DocumentIdentityMisuseAssessment*valid=a?document_identity_misuse_assessment_copy(a):NULL;
 if(!d||!valid){fail(e,"Évaluation d’usage d’identité invalide.");return FALSE;}
 const char*run=document_identity_misuse_assessment_get_ocr_run_identifier(valid);
 if(run){DatabaseStatement*check=database_statement_prepare(d->database,
  "SELECT 1 FROM identity_ocr_runs WHERE id=? AND evidence_id=?;");
  gboolean compatible=check&&bind(check,1,run)&&bind(check,2,
   document_identity_misuse_assessment_get_evidence_identifier(valid))&&
   database_statement_step(check)==DATABASE_STATEMENT_STEP_ROW;
  database_statement_finalize(check);if(!compatible){
   document_identity_misuse_assessment_free(valid);
   fail(e,"L’exécution OCR ne correspond pas à la preuve évaluée.");return FALSE;}}
 const char*previous=document_identity_misuse_assessment_get_previous_identifier(valid);
 if(previous){DatabaseStatement*check=database_statement_prepare(d->database,
  "SELECT 1 FROM document_identity_misuse_assessments WHERE id=? AND evidence_id=?;");
  gboolean compatible=check&&bind(check,1,previous)&&bind(check,2,
   document_identity_misuse_assessment_get_evidence_identifier(valid))&&
   database_statement_step(check)==DATABASE_STATEMENT_STEP_ROW;
  database_statement_finalize(check);if(!compatible){
   document_identity_misuse_assessment_free(valid);
   fail(e,"L’évaluation précédente ne correspond pas à la preuve.");return FALSE;}}
 DatabaseStatement*s=database_statement_prepare(d->database,
  "INSERT INTO document_identity_misuse_assessments VALUES(?,?,?,?,?,?,?,?);");
 gboolean ok=s&&bind(s,1,document_identity_misuse_assessment_get_identifier(valid))&&
  bind(s,2,document_identity_misuse_assessment_get_evidence_identifier(valid))&&
  bind(s,3,run)&&bind(s,4,document_identity_misuse_assessment_get_status(valid))&&
  bind(s,5,document_identity_misuse_assessment_get_justification(valid))&&
  bind(s,6,document_identity_misuse_assessment_get_assessed_at(valid))&&
  bind(s,7,previous)&&bind(s,8,"human")&&
  database_statement_step(s)==DATABASE_STATEMENT_STEP_DONE;
 database_statement_finalize(s);document_identity_misuse_assessment_free(valid);
 if(!ok)fail(e,"Impossible de conserver l’évaluation d’usage d’identité.");
 return ok;
}
static DocumentIdentityMisuseAssessment *read_misuse(DatabaseStatement*s)
{
 char*v[8]={0};gboolean ok=TRUE;for(int i=0;i<8;i++)
  ok=ok&&database_statement_column_text(s,i,&v[i]);
 DocumentIdentityMisuseAssessment*a=ok?document_identity_misuse_assessment_new(
  v[0],v[1],v[2],v[3],v[4],v[5],v[6]):NULL;
 for(int i=0;i<8;i++)g_free(v[i]);
 return a;
}
GPtrArray *identity_traceability_dao_list_identity_misuse(
 IdentityTraceabilityDao*d,const char*evidence,GError**e)
{
 if(!d||!evidence){fail(e,"Lecture de l’usage d’identité invalide.");return NULL;}
 DatabaseStatement*s=database_statement_prepare(d->database,
  "SELECT id,evidence_id,ocr_run_id,status,justification,assessed_at,"
  "previous_assessment_id,origin FROM document_identity_misuse_assessments "
  "WHERE evidence_id=? ORDER BY assessed_at,id;");
 GPtrArray*a=g_ptr_array_new_with_free_func((GDestroyNotify)
  document_identity_misuse_assessment_free);if(!s||!bind(s,1,evidence))goto bad_misuse;
 for(;;){DatabaseStatementStepResult step=database_statement_step(s);
  if(step==DATABASE_STATEMENT_STEP_DONE)break;
  if(step!=DATABASE_STATEMENT_STEP_ROW)goto bad_misuse;
  DocumentIdentityMisuseAssessment*r=read_misuse(s);if(!r)goto bad_misuse;g_ptr_array_add(a,r);}
 database_statement_finalize(s);return a;
bad_misuse:database_statement_finalize(s);g_ptr_array_unref(a);
 fail(e,"Impossible de lire l’historique d’usage d’identité.");return NULL;
}
DocumentIdentityMisuseAssessment *identity_traceability_dao_current_identity_misuse(
 IdentityTraceabilityDao*d,const char*evidence,GError**e)
{
 GPtrArray*a=identity_traceability_dao_list_identity_misuse(d,evidence,e);if(!a)return NULL;
 DocumentIdentityMisuseAssessment*current=NULL;
 for(guint i=0;i<a->len;i++){DocumentIdentityMisuseAssessment*c=g_ptr_array_index(a,i);
  gboolean has_next=FALSE;for(guint j=0;j<a->len;j++){
   DocumentIdentityMisuseAssessment*n=g_ptr_array_index(a,j);
   if(g_strcmp0(document_identity_misuse_assessment_get_previous_identifier(n),
    document_identity_misuse_assessment_get_identifier(c))==0){has_next=TRUE;break;}}
  if(!has_next)current=c;}
 DocumentIdentityMisuseAssessment*r=document_identity_misuse_assessment_copy(current);
 g_ptr_array_unref(a);return r;
}
gboolean identity_traceability_dao_insert_factual_relation(
 IdentityTraceabilityDao*d,const PersonEvidenceFactualRelation*r,GError**e)
{
 PersonEvidenceFactualRelation*valid=r?person_evidence_factual_relation_copy(r):NULL;
 if(!d||!valid){fail(e,"Relation factuelle invalide.");return FALSE;}
 const char*run=person_evidence_factual_relation_get_ocr_run_identifier(valid);
 if(run){
  DatabaseStatement*check=database_statement_prepare(d->database,
   "SELECT 1 FROM identity_ocr_runs WHERE id=? AND evidence_id=?;");
  gboolean compatible=check&&bind(check,1,run)&&bind(check,2,
   person_evidence_factual_relation_get_evidence_identifier(valid))&&
   database_statement_step(check)==DATABASE_STATEMENT_STEP_ROW;
  database_statement_finalize(check);
  if(!compatible){person_evidence_factual_relation_free(valid);
   fail(e,"L’exécution OCR ne correspond pas à la preuve choisie.");return FALSE;}
 }
 DatabaseStatement*s=database_statement_prepare(d->database,
  "INSERT INTO person_evidence_factual_relations VALUES(?,?,?,?,?,?,?,?,?);");
 gboolean ok=s&&bind(s,1,person_evidence_factual_relation_get_identifier(valid))&&
  bind(s,2,person_evidence_factual_relation_get_person_identifier(valid))&&
  bind(s,3,person_evidence_factual_relation_get_evidence_identifier(valid))&&
  bind(s,4,run)&&bind(s,5,person_evidence_factual_relation_get_relation_type(valid))&&
  bind(s,6,person_evidence_factual_relation_get_factual_note(valid))&&
  bind(s,7,person_evidence_factual_relation_get_observed_at(valid))&&
  bind(s,8,"human")&&database_statement_bind_int64(s,9,
   person_evidence_factual_relation_get_active(valid)?1:0)&&
  database_statement_step(s)==DATABASE_STATEMENT_STEP_DONE;
 database_statement_finalize(s);person_evidence_factual_relation_free(valid);
 if(!ok)fail(e,"Impossible de conserver la relation factuelle.");
 return ok;
}
GPtrArray *identity_traceability_dao_list_factual_relations(
 IdentityTraceabilityDao*d,const char*id,GError**e)
{
 if(!d||!id){fail(e,"Lecture des relations factuelles invalide.");return NULL;}
 DatabaseStatement*s=database_statement_prepare(d->database,
  "SELECT id,person_id,evidence_id,ocr_run_id,relation_type,factual_note,"
  "observed_at,origin,active FROM person_evidence_factual_relations "
  "WHERE evidence_id=? ORDER BY observed_at,id;");
 GPtrArray*a=g_ptr_array_new_with_free_func(
  (GDestroyNotify)person_evidence_factual_relation_free);
 if(!s||!bind(s,1,id))goto bad;
 for(;;){DatabaseStatementStepResult step=database_statement_step(s);
  if(step==DATABASE_STATEMENT_STEP_DONE)break;
  if(step!=DATABASE_STATEMENT_STEP_ROW)goto bad;
  char*v[8]={0};gint64 active=0;gboolean ok=TRUE;
  for(int i=0;i<8;i++)ok=ok&&database_statement_column_text(s,i,&v[i]);
  ok=ok&&database_statement_column_int64(s,8,&active);
  PersonEvidenceFactualRelation*r=ok?person_evidence_factual_relation_new(
   v[0],v[1],v[2],v[3],v[4],v[5],v[6],active!=0):NULL;
  for(int i=0;i<8;i++)g_free(v[i]);
  if(!r)goto bad;
  g_ptr_array_add(a,r);}
 database_statement_finalize(s);return a;
bad:database_statement_finalize(s);g_ptr_array_unref(a);
 fail(e,"Impossible de lire les relations factuelles.");return NULL;
}
GPtrArray *identity_traceability_dao_list_factual_relations_by_evidence(
 IdentityTraceabilityDao*d,const char*id,GError**e)
{return identity_traceability_dao_list_factual_relations(d,id,e);}
GPtrArray *identity_traceability_dao_list_factual_relations_by_person(
 IdentityTraceabilityDao*d,const char*id,GError**e)
{
 if(!d||!id){fail(e,"Lecture des relations factuelles invalide.");return NULL;}
 DatabaseStatement*s=database_statement_prepare(d->database,
  "SELECT id,person_id,evidence_id,ocr_run_id,relation_type,factual_note,"
  "observed_at,origin,active FROM person_evidence_factual_relations "
  "WHERE person_id=? ORDER BY observed_at,id;");
 GPtrArray*a=g_ptr_array_new_with_free_func(
  (GDestroyNotify)person_evidence_factual_relation_free);
 if(!s||!bind(s,1,id))goto bad2;
 for(;;){DatabaseStatementStepResult step=database_statement_step(s);
  if(step==DATABASE_STATEMENT_STEP_DONE)break;
  if(step!=DATABASE_STATEMENT_STEP_ROW)goto bad2;
  char*v[8]={0};gint64 active=0;gboolean ok=TRUE;
  for(int i=0;i<8;i++)ok=ok&&database_statement_column_text(s,i,&v[i]);
  ok=ok&&database_statement_column_int64(s,8,&active);
  PersonEvidenceFactualRelation*r=ok?person_evidence_factual_relation_new(
   v[0],v[1],v[2],v[3],v[4],v[5],v[6],active!=0):NULL;
  for(int i=0;i<8;i++)g_free(v[i]);
  if(!r)goto bad2;
  g_ptr_array_add(a,r);}
 database_statement_finalize(s);return a;
bad2:database_statement_finalize(s);g_ptr_array_unref(a);
 fail(e,"Impossible de lire les relations factuelles.");return NULL;
}
GPtrArray *identity_traceability_dao_list_roles(
 IdentityTraceabilityDao*d,gboolean inactive,GError**e)
{
 if(!d){fail(e,"Lecture du vocabulaire invalide.");return NULL;}
 DatabaseStatement*s=database_statement_prepare(d->database,
  inactive?"SELECT code,label,description,display_order,active,"
  "requires_justification,sensitive FROM person_role_vocabulary "
  "ORDER BY display_order,code;":"SELECT code,label,description,display_order,"
  "active,requires_justification,sensitive FROM person_role_vocabulary "
  "WHERE active=1 ORDER BY display_order,code;");
 GPtrArray*a=g_ptr_array_new_with_free_func(
  (GDestroyNotify)person_role_vocabulary_entry_free);
 if(!s)goto bad;
 for(;;){DatabaseStatementStepResult step=database_statement_step(s);
  if(step==DATABASE_STATEMENT_STEP_DONE)break;
  if(step!=DATABASE_STATEMENT_STEP_ROW)goto bad;
  PersonRoleVocabularyEntry*r=g_new0(PersonRoleVocabularyEntry,1);
  gint64 order=0,active=0,required=0,sensitive=0;
  gboolean ok=database_statement_column_text(s,0,&r->code)&&
   database_statement_column_text(s,1,&r->label)&&
   database_statement_column_text(s,2,&r->description)&&
   database_statement_column_int64(s,3,&order)&&
   database_statement_column_int64(s,4,&active)&&
   database_statement_column_int64(s,5,&required)&&
   database_statement_column_int64(s,6,&sensitive);
  if(!ok){person_role_vocabulary_entry_free(r);goto bad;}
  r->display_order=(gint)order;r->active=active!=0;
  r->requires_justification=required!=0;r->sensitive=sensitive!=0;
  g_ptr_array_add(a,r);}database_statement_finalize(s);return a;
bad:database_statement_finalize(s);g_ptr_array_unref(a);
 fail(e,"Impossible de lire le vocabulaire des rôles.");return NULL;
}
GPtrArray *identity_traceability_dao_list_identification_statuses(
 IdentityTraceabilityDao*d,gboolean inactive,GError**e)
{
 if(!d){fail(e,"Lecture du vocabulaire invalide.");return NULL;}
 DatabaseStatement*s=database_statement_prepare(d->database,
  inactive?"SELECT code,label,description,display_order,active,"
  "requires_justification,sensitive FROM identification_status_vocabulary "
  "ORDER BY display_order,code;":"SELECT code,label,description,display_order,"
  "active,requires_justification,sensitive FROM identification_status_vocabulary "
  "WHERE active=1 ORDER BY display_order,code;");
 GPtrArray*a=g_ptr_array_new_with_free_func(
  (GDestroyNotify)identification_status_vocabulary_entry_free);
 if(!s)goto bad;
 for(;;){DatabaseStatementStepResult step=database_statement_step(s);
  if(step==DATABASE_STATEMENT_STEP_DONE)break;
  if(step!=DATABASE_STATEMENT_STEP_ROW)goto bad;
  IdentificationStatusVocabularyEntry*r=g_new0(
   IdentificationStatusVocabularyEntry,1);
  gint64 order=0,active=0,required=0,sensitive=0;
  gboolean ok=database_statement_column_text(s,0,&r->code)&&
   database_statement_column_text(s,1,&r->label)&&
   database_statement_column_text(s,2,&r->description)&&
   database_statement_column_int64(s,3,&order)&&
   database_statement_column_int64(s,4,&active)&&
   database_statement_column_int64(s,5,&required)&&
   database_statement_column_int64(s,6,&sensitive);
  if(!ok){identification_status_vocabulary_entry_free(r);goto bad;}
  r->display_order=(gint)order;r->active=active!=0;
  r->requires_justification=required!=0;r->sensitive=sensitive!=0;
  g_ptr_array_add(a,r);}
 database_statement_finalize(s);return a;
bad:database_statement_finalize(s);g_ptr_array_unref(a);
 fail(e,"Impossible de lire le vocabulaire des états d’identification.");
 return NULL;
}
