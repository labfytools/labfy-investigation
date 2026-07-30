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
 DatabaseStatement*s=database_statement_prepare(d->database,
  "INSERT INTO document_authenticity_assessments VALUES(?,?,?,?,?,?,?,?,?);");
 gboolean ok=s&&bind(s,1,a->identifier)&&bind(s,2,a->evidence_identifier)&&
  bind(s,3,a->ocr_run_identifier)&&bind(s,4,a->status)&&bind(s,5,a->justification)&&
  bind(s,6,a->assessed_at)&&bind(s,7,a->previous_identifier)&&
  bind(s,8,a->technical_note)&&bind(s,9,"human")&&
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
 GPtrArray*a=auth_query(d,"SELECT * FROM document_authenticity_assessments "
  "WHERE evidence_id=? ORDER BY assessed_at DESC,id DESC LIMIT 1;",id,e);
 if(!a)return NULL;
 DocumentAuthenticityAssessment*r=a->len?
  document_authenticity_assessment_copy(g_ptr_array_index(a,0)):NULL;
 g_ptr_array_unref(a);return r;
}
gboolean identity_traceability_dao_insert_factual_relation(
 IdentityTraceabilityDao*d,const PersonEvidenceFactualRelation*r,GError**e)
{
 PersonEvidenceFactualRelation*valid=r?person_evidence_factual_relation_copy(r):NULL;
 if(!d||!valid){fail(e,"Relation factuelle invalide.");return FALSE;}
 DatabaseStatement*s=database_statement_prepare(d->database,
  "INSERT INTO person_evidence_factual_relations VALUES(?,?,?,?,?,?,?,?,?);");
 gboolean ok=s&&bind(s,1,r->identifier)&&bind(s,2,r->person_identifier)&&
  bind(s,3,r->evidence_identifier)&&bind(s,4,r->ocr_run_identifier)&&
  bind(s,5,r->relation_type)&&bind(s,6,r->factual_note)&&bind(s,7,r->observed_at)&&
  bind(s,8,"human")&&database_statement_bind_int64(s,9,r->active?1:0)&&
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
