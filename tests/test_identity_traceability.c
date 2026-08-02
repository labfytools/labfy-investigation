#include "dao/identity_traceability_dao.h"
#include "views/person_vocabulary_adapter.h"
#include "dao/identity_ocr_dao.h"
#include "database/database.h"
#include "models/identity_traceability.h"
#include "core/document_authenticity_service.h"
#include "core/document_identity_misuse_service.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <sqlite3.h>

#define EVIDENCE "10000000-0000-4000-8000-000000000018"
#define PERSON "20000000-0000-4000-8000-000000000018"
#define RUN "30000000-0000-4000-8000-000000000018"
#define DOCUMENT "40000000-0000-4000-8000-000000000018"
#define FIELD "50000000-0000-4000-8000-000000000018"
#define ASSESSMENT1 "60000000-0000-4000-8000-000000000018"
#define ASSESSMENT2 "60000000-0000-4000-8000-000000000019"
#define FACT "70000000-0000-4000-8000-000000000018"
#define MISUSE1 "90000000-0000-4000-8000-000000000018"
#define MISUSE2 "90000000-0000-4000-8000-000000000019"
#define AT "2026-07-30T10:00:00Z"

typedef struct{char*dir,*path;Database*database;}Fixture;
static void exec_ok(sqlite3*d,const char*sql)
{char*message=NULL;g_assert_cmpint(sqlite3_exec(d,sql,NULL,NULL,&message),==,SQLITE_OK);
 sqlite3_free(message);}
static char *scalar(sqlite3*d,const char*sql)
{sqlite3_stmt*s=NULL;g_assert_cmpint(sqlite3_prepare_v2(d,sql,-1,&s,NULL),==,SQLITE_OK);
 g_assert_cmpint(sqlite3_step(s),==,SQLITE_ROW);char*r=g_strdup(
 (const char*)sqlite3_column_text(s,0));sqlite3_finalize(s);return r;}
static Fixture fixture_new(void)
{
 Fixture f={0};GError*e=NULL;f.dir=g_dir_make_tmp("labfy-v18-XXXXXX",&e);
 g_assert_no_error(e);f.path=g_build_filename(f.dir,"Enquete.sqlite",NULL);
 g_assert_true(database_initialize(f.path,"SPECIMEN V18",f.dir));
 sqlite3*d=NULL;g_assert_cmpint(sqlite3_open(f.path,&d),==,SQLITE_OK);
 exec_ok(d,"PRAGMA foreign_keys=ON;"
 "INSERT INTO preuves(id,name,relative_path,type_id,size_bytes,sha256,"
 "imported_at,updated_at,status,locked,original_name) VALUES('" EVIDENCE
 "','specimen.png','specimen.png',2,8,"
 "'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','" AT
 "','" AT "','active',0,'specimen.png');"
 "INSERT INTO entites(id,type_id,valeur,label,confiance,created_at,updated_at,status)"
 " VALUES('" PERSON "',(SELECT id FROM types_entite WHERE code='person'),"
 "'Personne SPECIMEN','Personne SPECIMEN',0,'" AT "','" AT "','active');"
 "INSERT INTO identity_ocr_runs(id,evidence_id,expected_sha256,page_number,"
 "document_type,document_side,engine,requested_languages,available_languages,"
 "parameters,preprocessing_profile,executed_at,status,text_relative_path,"
 "text_sha256,tsv_relative_path,tsv_sha256) VALUES('" RUN "','" EVIDENCE "',"
 "'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',1,"
 "'identity_card','front','tesseract','fra','fra','SPECIMEN','none','" AT
 "','success','raw.txt','bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb',"
 "'raw.tsv','cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc');"
 "INSERT INTO identity_document_observations(id,person_id,evidence_id,ocr_run_id,"
 "document_type,document_side,page_number,review_state,observed_at) VALUES('"
 DOCUMENT "','" PERSON "','" EVIDENCE "','" RUN "','identity_card','front',1,"
 "'accepted','" AT "');"
 "INSERT INTO identity_field_observations(id,observation_id,field_code,raw_value,"
 "normalized_value,review_status,origin,evidence_id,ocr_run_id,page_number,"
 "display_order,reviewed_at) VALUES('" FIELD "','" DOCUMENT "','surname',"
 "'BRUT SPECIMEN','BRUT SPECIMEN','accepted','ocr','" EVIDENCE "','" RUN
 "',1,0,'" AT "');");
 sqlite3_close(d);f.database=database_open(f.path);g_assert_nonnull(f.database);
 g_assert_true(database_migrate_to_latest(f.database));return f;
}
static void fixture_free(Fixture*f)
{database_close(f->database);g_remove(f->path);g_rmdir(f->dir);
 g_free(f->path);g_free(f->dir);}

static void test_models(void)
{
 gsize status_count=0;const DocumentAuthenticityStatus*auth_statuses=
  document_authenticity_service_statuses(&status_count);
 static const char*auth_codes[]={"indeterminate","presumed_authentic","suspicious","presumed_forged","confirmed_forged"};
 static const char*auth_labels[]={"Authenticité indéterminée","Document présumé authentique","Document présentant des éléments suspects","Document présumé falsifié","Falsification confirmée"};
 g_assert_cmpuint(status_count,==,5);for(guint i=0;i<status_count;i++){g_assert_cmpstr(auth_statuses[i].code,==,auth_codes[i]);g_assert_cmpstr(auth_statuses[i].label,==,auth_labels[i]);g_assert_nonnull(auth_statuses[i].description);}
 static const char *codes[]={"identity_observed_in",
  "document_presented_in_name_of","declared_holder_in","data_extracted_from"};
 static const char *labels[]={"Identité observée dans la preuve",
  "Document présenté au nom de la personne","Titulaire déclaré dans la preuve",
  "Donnée extraite de la preuve"};
 for(guint i=0;i<G_N_ELEMENTS(codes);i++){
  g_assert_cmpstr(person_vocabulary_adapter_relation_code(i),==,codes[i]);
  g_assert_cmpstr(person_vocabulary_adapter_relation_label(codes[i]),==,labels[i]);
  g_assert_nonnull(person_vocabulary_adapter_relation_description(codes[i]));
 }
 PersonRoleVocabularyEntry justified={.requires_justification=TRUE};
 g_assert_false(person_vocabulary_adapter_justification_valid(
  &justified,"   "));
 g_assert_true(person_vocabulary_adapter_justification_valid(
  &justified,"Justification SPECIMEN"));
 g_assert_true(identity_traceability_identification_status_valid("disputed"));
 g_assert_false(identity_traceability_identification_status_valid("identified"));
 g_assert_false(identity_traceability_relation_type_valid("is_author"));
 g_assert_false(identity_traceability_field_is_projectable(
  "accepted","uncertain","human_confirmed","SPECIMEN"));
 g_assert_true(identity_traceability_field_is_projectable(
  "modified","partial","human_confirmed","SPECIMEN"));
 g_assert_null(document_authenticity_assessment_new(ASSESSMENT1,EVIDENCE,NULL,
  "confirmed_forged",NULL,AT,NULL,NULL));
 DocumentAuthenticityAssessment*a=document_authenticity_assessment_new(
  ASSESSMENT1,EVIDENCE,NULL,"confirmed_forged","Justification SPECIMEN",AT,NULL,NULL);
 g_assert_nonnull(a);DocumentAuthenticityAssessment*c=
  document_authenticity_assessment_copy(a);g_assert_cmpstr(c->status,==,a->status);
 document_authenticity_assessment_free(c);document_authenticity_assessment_free(a);
}
static void test_authenticity_service(void)
{
 Fixture f=fixture_new();GError*error=NULL;DocumentAuthenticityAssessment*created=NULL;
 g_assert_false(document_authenticity_service_add(f.database,EVIDENCE,"suspicious","   ",NULL,NULL,&created,&error));g_assert_error(error,g_quark_from_static_string("document-authenticity-service"),1);g_clear_error(&error);
 g_assert_true(document_authenticity_service_add(f.database,EVIDENCE,"indeterminate",NULL,"Note SPECIMEN",NULL,&created,&error));g_assert_no_error(error);g_assert_null(document_authenticity_assessment_get_previous_identifier(created));document_authenticity_assessment_free(created);
 g_assert_true(document_authenticity_service_add(f.database,EVIDENCE,"suspicious","Élément SPECIMEN",NULL,RUN,&created,&error));g_assert_no_error(error);g_assert_nonnull(document_authenticity_assessment_get_previous_identifier(created));document_authenticity_assessment_free(created);
 GPtrArray*h=document_authenticity_service_history(f.database,EVIDENCE,&error);g_assert_no_error(error);g_assert_cmpuint(h->len,==,2);g_ptr_array_unref(h);
 g_assert_false(document_authenticity_service_add(f.database,EVIDENCE,
  "indeterminate",NULL,NULL,"30000000-0000-4000-8000-000000000099",
  NULL,&error));g_assert_nonnull(error);g_clear_error(&error);
 h=document_authenticity_service_history(f.database,EVIDENCE,&error);g_assert_cmpuint(h->len,==,2);g_ptr_array_unref(h);fixture_free(&f);
}
static void test_identity_misuse_service(void)
{
 gsize count=0;const DocumentIdentityMisuseStatus*statuses=
  document_identity_misuse_service_statuses(&count);
 static const char*codes[]={"indeterminate","presumed","confirmed"};
 static const char*labels[]={"Hypothèse d’usurpation indéterminée",
  "Document présumé utilisé dans une usurpation d’identité",
  "Utilisation dans une usurpation d’identité confirmée"};
 g_assert_cmpuint(count,==,3);for(guint i=0;i<count;i++){
  g_assert_cmpstr(statuses[i].code,==,codes[i]);g_assert_cmpstr(statuses[i].label,==,labels[i]);}
 g_assert_null(document_identity_misuse_assessment_new(MISUSE1,EVIDENCE,NULL,
  "presumed",NULL,AT,NULL));
 Fixture f=fixture_new();GError*error=NULL;DocumentIdentityMisuseAssessment*created=NULL;
 g_assert_true(document_identity_misuse_service_add(f.database,EVIDENCE,
  "indeterminate",NULL,NULL,&created,&error));g_assert_no_error(error);
 g_assert_cmpstr(document_identity_misuse_assessment_get_origin(created),==,"human");
 document_identity_misuse_assessment_free(created);
 g_assert_true(document_identity_misuse_service_add(f.database,EVIDENCE,
  "presumed","Indices SPECIMEN",RUN,&created,&error));g_assert_no_error(error);
 g_assert_nonnull(document_identity_misuse_assessment_get_previous_identifier(created));
 document_identity_misuse_assessment_free(created);
 g_assert_false(document_identity_misuse_service_add(f.database,EVIDENCE,
  "confirmed","Conclusion SPECIMEN","30000000-0000-4000-8000-000000000099",
  NULL,&error));g_assert_nonnull(error);g_clear_error(&error);
 GPtrArray*h=document_identity_misuse_service_history(f.database,EVIDENCE,&error);
 g_assert_no_error(error);g_assert_cmpuint(h->len,==,2);g_ptr_array_unref(h);
 database_close(f.database);f.database=database_open(f.path);g_assert_nonnull(f.database);
 h=document_identity_misuse_service_history(f.database,EVIDENCE,&error);
 g_assert_cmpuint(h->len,==,2);g_ptr_array_unref(h);
 database_close(f.database);f.database=NULL;sqlite3*d=NULL;
 g_assert_cmpint(sqlite3_open(f.path,&d),==,SQLITE_OK);
 char*automatic=scalar(d,"SELECT (SELECT COUNT(*) FROM person_role_assignments)"
  "||(SELECT COUNT(*) FROM person_evidence_factual_relations)"
  "||(SELECT COUNT(*) FROM person_identification_assessments)"
  "||(SELECT COUNT(*) FROM document_authenticity_assessments);");
 g_assert_cmpstr(automatic,==,"0000");g_free(automatic);char*message=NULL;
 g_assert_cmpint(sqlite3_exec(d,"UPDATE document_identity_misuse_assessments "
  "SET status='confirmed';",NULL,NULL,&message),!=,SQLITE_OK);
 sqlite3_free(message);message=NULL;g_assert_cmpint(sqlite3_exec(d,
  "DELETE FROM document_identity_misuse_assessments;",NULL,NULL,&message),!=,SQLITE_OK);
 sqlite3_free(message);sqlite3_close(d);f.database=database_open(f.path);
 g_assert_nonnull(f.database);fixture_free(&f);
}
static void test_migrate_v19_to_v20(void)
{
 Fixture f=fixture_new();database_close(f.database);f.database=NULL;sqlite3*d=NULL;
 g_assert_cmpint(sqlite3_open(f.path,&d),==,SQLITE_OK);
 exec_ok(d,"DROP TABLE document_identity_misuse_assessments;"
  "UPDATE metadata SET value='19' WHERE key='schema_version';");sqlite3_close(d);
 f.database=database_open(f.path);g_assert_nonnull(f.database);
 g_assert_true(database_migrate_to_latest(f.database));database_close(f.database);f.database=NULL;
 g_assert_cmpint(sqlite3_open(f.path,&d),==,SQLITE_OK);char*version=scalar(d,
  "SELECT value FROM metadata WHERE key='schema_version';");char*table=scalar(d,
  "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='document_identity_misuse_assessments';");
 g_assert_cmpstr(version,==,"20");g_assert_cmpstr(table,==,"1");g_free(version);g_free(table);
 exec_ok(d,"DROP TABLE document_identity_misuse_assessments;"
  "UPDATE metadata SET value='19' WHERE key='schema_version';"
  "CREATE TABLE document_identity_misuse_assessments(dummy TEXT);");sqlite3_close(d);
 f.database=database_open(f.path);g_test_expect_message(NULL,G_LOG_LEVEL_WARNING,
  "*Impossible d’installer la migration SQLite V20*");
 g_assert_false(database_migrate_to_latest(f.database));g_test_assert_expected_messages();
 database_close(f.database);f.database=NULL;g_assert_cmpint(sqlite3_open(f.path,&d),==,SQLITE_OK);
 version=scalar(d,"SELECT value FROM metadata WHERE key='schema_version';");
 g_assert_cmpstr(version,==,"19");g_free(version);exec_ok(d,
  "DROP TABLE document_identity_misuse_assessments;");sqlite3_close(d);
 f.database=database_open(f.path);g_assert_true(database_migrate_to_latest(f.database));fixture_free(&f);
}
static void test_dao_history_and_relations(void)
{
 Fixture f=fixture_new();GError*e=NULL;
 IdentityTraceabilityDao*d=identity_traceability_dao_new(f.database);
 DocumentAuthenticityAssessment*a1=document_authenticity_assessment_new(
  ASSESSMENT1,EVIDENCE,RUN,"indeterminate",NULL,AT,NULL,"Note SPECIMEN");
 DocumentAuthenticityAssessment*a2=document_authenticity_assessment_new(
  ASSESSMENT2,EVIDENCE,RUN,"suspicious","Anomalie SPECIMEN",
  "2026-07-30T11:00:00Z",ASSESSMENT1,NULL);
 g_assert_true(identity_traceability_dao_insert_authenticity(d,a1,&e));
 g_assert_no_error(e);g_assert_true(identity_traceability_dao_insert_authenticity(d,a2,&e));
 GPtrArray*h=identity_traceability_dao_list_authenticity(d,EVIDENCE,&e);
 g_assert_cmpuint(h->len,==,2);g_ptr_array_unref(h);
 DocumentAuthenticityAssessment*current=
  identity_traceability_dao_current_authenticity(d,EVIDENCE,&e);
 g_assert_cmpstr(current->identifier,==,ASSESSMENT2);
 PersonEvidenceFactualRelation*r=person_evidence_factual_relation_new(
  FACT,PERSON,EVIDENCE,RUN,"identity_observed_in","Observation SPECIMEN",AT,TRUE);
 g_assert_true(identity_traceability_dao_insert_factual_relation(d,r,&e));
 GPtrArray*relations=identity_traceability_dao_list_factual_relations(d,EVIDENCE,&e);
 g_assert_cmpuint(relations->len,==,1);g_ptr_array_unref(relations);
 relations=identity_traceability_dao_list_factual_relations_by_person(d,PERSON,&e);
 g_assert_cmpuint(relations->len,==,1);g_ptr_array_unref(relations);
 PersonEvidenceFactualRelation*foreign=person_evidence_factual_relation_new(
  "70000000-0000-4000-8000-000000000019",PERSON,
  "10000000-0000-4000-8000-000000000019",RUN,"data_extracted_from",
  "Run étranger SPECIMEN",AT,TRUE);
 g_assert_false(identity_traceability_dao_insert_factual_relation(d,foreign,&e));
 g_assert_error(e,g_quark_from_static_string("identity-traceability-dao"),1);
 g_assert_nonnull(strstr(e->message,"ne correspond pas"));g_clear_error(&e);
 person_evidence_factual_relation_free(foreign);
 GPtrArray*roles=identity_traceability_dao_list_roles(d,TRUE,&e);
 g_assert_cmpuint(roles->len,>=,14);g_ptr_array_unref(roles);
 roles=identity_traceability_dao_list_roles(d,FALSE,&e);
 g_assert_cmpuint(roles->len,==,10);
 for(guint i=0;i<roles->len;i++){
  PersonRoleVocabularyEntry*role=g_ptr_array_index(roles,i);
  g_assert_true(role->active);g_assert_nonnull(role->code);
  g_assert_nonnull(role->label);g_assert_nonnull(role->description);
  if(i>0)g_assert_cmpint(((PersonRoleVocabularyEntry*)
   g_ptr_array_index(roles,i-1))->display_order,<,role->display_order);
 }
 g_ptr_array_unref(roles);
 GPtrArray*statuses=identity_traceability_dao_list_identification_statuses(
  d,FALSE,&e);g_assert_no_error(e);g_assert_cmpuint(statuses->len,==,6);
 for(guint i=0;i<statuses->len;i++){
  IdentificationStatusVocabularyEntry*status=g_ptr_array_index(statuses,i);
  g_assert_true(status->active);g_assert_nonnull(status->description);
  if(i>0)g_assert_cmpint(((IdentificationStatusVocabularyEntry*)
   g_ptr_array_index(statuses,i-1))->display_order,<,status->display_order);
 }
 g_ptr_array_unref(statuses);
 person_evidence_factual_relation_free(r);
 document_authenticity_assessment_free(current);
 document_authenticity_assessment_free(a2);document_authenticity_assessment_free(a1);
 identity_traceability_dao_free(d);fixture_free(&f);
}
static void test_sqlite_negative_guards(void)
{
 Fixture f=fixture_new();database_close(f.database);f.database=NULL;
 sqlite3*d=NULL;g_assert_cmpint(sqlite3_open(f.path,&d),==,SQLITE_OK);
 exec_ok(d,"PRAGMA foreign_keys=ON;");
 char*before=scalar(d,"SELECT raw_value FROM identity_field_observations WHERE id='" FIELD "';");
 char*message=NULL;
 g_assert_cmpint(sqlite3_exec(d,"INSERT INTO document_authenticity_assessments"
  "(id,evidence_id,status,assessed_at,origin) VALUES("
  "'80000000-0000-4000-8000-000000000018','" EVIDENCE "',"
  "'confirmed_forged','" AT "','human');",NULL,NULL,&message),!=,SQLITE_OK);
 sqlite3_free(message);message=NULL;
 g_assert_cmpint(sqlite3_exec(d,"INSERT INTO person_evidence_factual_relations"
  "(id,person_id,evidence_id,relation_type,observed_at,origin) VALUES("
  "'80000000-0000-4000-8000-000000000019','" PERSON "','" EVIDENCE "',"
  "'is_author','" AT "','human');",NULL,NULL,&message),!=,SQLITE_OK);
 sqlite3_free(message);message=NULL;
 g_assert_cmpint(sqlite3_exec(d,"UPDATE identity_field_observations SET "
  "confirmed_value='SPECIMEN',confirmation_state='human_confirmed',"
  "value_quality='uncertain' WHERE id='" FIELD "';",NULL,NULL,&message),!=,SQLITE_OK);
 sqlite3_free(message);
 char*raw=scalar(d,"SELECT raw_value FROM identity_field_observations WHERE id='" FIELD "';");
 char*confirmed=scalar(d,"SELECT COALESCE(confirmed_value,'NULL') FROM "
  "identity_field_observations WHERE id='" FIELD "';");
 char*auth=scalar(d,"SELECT COUNT(*) FROM document_authenticity_assessments;");
 char*relations=scalar(d,"SELECT COUNT(*) FROM person_evidence_factual_relations;");
 g_assert_cmpstr(raw,==,before);g_assert_cmpstr(confirmed,==,"NULL");
 g_assert_cmpstr(auth,==,"0");g_assert_cmpstr(relations,==,"0");
 exec_ok(d,"UPDATE identity_field_observations SET "
  "confirmed_value='CONFIRMÉ SPECIMEN',confirmation_state='human_confirmed',"
  "value_quality='partial' WHERE id='" FIELD "';");
 g_free(before);g_free(raw);g_free(confirmed);g_free(auth);g_free(relations);
 sqlite3_close(d);f.database=database_open(f.path);g_assert_nonnull(f.database);
 g_assert_true(database_migrate_to_latest(f.database));
 IdentityOcrDao*ocr=identity_ocr_dao_new(f.database);GError*error=NULL;
 GPtrArray*confirmed_fields=identity_ocr_dao_list_confirmed_fields(
  ocr,DOCUMENT,&error);g_assert_no_error(error);
 g_assert_cmpuint(confirmed_fields->len,==,1);
 IdentityFieldObservationRecord*record=g_ptr_array_index(confirmed_fields,0);
 g_assert_cmpstr(record->confirmed_value,==,"CONFIRMÉ SPECIMEN");
 g_ptr_array_unref(confirmed_fields);identity_ocr_dao_free(ocr);fixture_free(&f);
}
static void test_migrate_v17_preserves_ocr(void)
{
 Fixture f=fixture_new();database_close(f.database);f.database=NULL;
 sqlite3*d=NULL;g_assert_cmpint(sqlite3_open(f.path,&d),==,SQLITE_OK);
 exec_ok(d,"PRAGMA foreign_keys=OFF;"
 "DROP TABLE document_identity_misuse_assessments;"
 "DROP TABLE person_ocr_field_projections;DROP TABLE person_profile_fields;"
 "DROP TABLE person_identification_assessments;"
 "DROP TABLE person_evidence_factual_relations;"
 "DROP TABLE document_authenticity_assessments;"
 "DROP TABLE person_role_vocabulary;DROP TABLE identification_status_vocabulary;"
 "ALTER TABLE identity_field_observations RENAME TO identity_fields_v18_old;"
 "CREATE TABLE identity_field_observations(id TEXT PRIMARY KEY,"
 "observation_id TEXT NOT NULL,field_code TEXT NOT NULL,raw_value TEXT,"
 "corrected_value TEXT,normalized_value TEXT,confidence REAL,"
 "review_status TEXT NOT NULL,origin TEXT NOT NULL,evidence_id TEXT NOT NULL,"
 "ocr_run_id TEXT NOT NULL,page_number INTEGER NOT NULL,source_x INTEGER,"
 "source_y INTEGER,source_width INTEGER,source_height INTEGER,"
 "source_image_width INTEGER,source_image_height INTEGER,display_order INTEGER,"
 "reviewed_at TEXT NOT NULL,review_note TEXT);"
 "INSERT INTO identity_field_observations SELECT id,observation_id,field_code,"
 "raw_value,'CORRIGÉ SPECIMEN',normalized_value,confidence,review_status,origin,"
 "evidence_id,ocr_run_id,page_number,source_x,source_y,source_width,source_height,"
 "source_image_width,source_image_height,display_order,reviewed_at,review_note "
 "FROM identity_fields_v18_old;DROP TABLE identity_fields_v18_old;"
 "UPDATE metadata SET value='17' WHERE key='schema_version';"
 "CREATE TABLE identification_status_vocabulary(dummy TEXT);");
 sqlite3_close(d);f.database=database_open(f.path);g_assert_nonnull(f.database);
 g_test_expect_message(NULL,G_LOG_LEVEL_WARNING,
  "*Impossible d’installer la migration SQLite V18*");
 g_assert_false(database_migrate_to_latest(f.database));
 g_test_assert_expected_messages();database_close(f.database);
 f.database=NULL;g_assert_cmpint(sqlite3_open(f.path,&d),==,SQLITE_OK);
 char*rolled_back=scalar(d,
  "SELECT value FROM metadata WHERE key='schema_version';");
 char*partial=scalar(d,"SELECT COUNT(*) FROM sqlite_master WHERE type='table' "
  "AND name='document_authenticity_assessments';");
 g_assert_cmpstr(rolled_back,==,"17");g_assert_cmpstr(partial,==,"0");
 g_free(rolled_back);g_free(partial);
 exec_ok(d,"DROP TABLE identification_status_vocabulary;");
 sqlite3_close(d);f.database=database_open(f.path);g_assert_nonnull(f.database);
 g_assert_true(database_migrate_to_latest(f.database));database_close(f.database);
 f.database=NULL;g_assert_cmpint(sqlite3_open(f.path,&d),==,SQLITE_OK);
 char*version=scalar(d,"SELECT value FROM metadata WHERE key='schema_version';");
 char*values=scalar(d,"SELECT raw_value||':'||normalized_value||':'||corrected_value"
 "||':'||confirmation_state FROM identity_field_observations WHERE id='" FIELD "';");
 g_assert_cmpstr(version,==,"20");
 g_assert_cmpstr(values,==,"BRUT SPECIMEN:BRUT SPECIMEN:CORRIGÉ SPECIMEN:unconfirmed");
 sqlite3_close(d);g_free(version);g_free(values);
 f.database=database_open(f.path);g_assert_true(database_migrate_to_latest(f.database));
 fixture_free(&f);
}
int main(int argc,char**argv)
{g_test_init(&argc,&argv,NULL);g_test_add_func("/v18/models",test_models);
 g_test_add_func("/v18/dao-history-relations",test_dao_history_and_relations);
 g_test_add_func("/v18/authenticity-service",test_authenticity_service);
 g_test_add_func("/v20/identity-misuse-service",test_identity_misuse_service);
 g_test_add_func("/v20/migrate-v19",test_migrate_v19_to_v20);
 g_test_add_func("/v18/sqlite-negative-guards",test_sqlite_negative_guards);
 g_test_add_func("/v18/migrate-v17",test_migrate_v17_preserves_ocr);
 return g_test_run();}
