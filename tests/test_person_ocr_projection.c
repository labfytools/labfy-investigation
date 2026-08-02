#include "core/person_ocr_projection_mapping.h"
#include "core/person_ocr_projection_service.h"
#include "dao/person_ocr_projection_dao.h"
#include "database/database.h"
#include <glib/gstdio.h>
#include <sqlite3.h>
#define PERSON "20000000-0000-4000-8000-000000000019"
#define EVIDENCE "10000000-0000-4000-8000-000000000019"
#define RUN "30000000-0000-4000-8000-000000000019"
#define DOC "40000000-0000-4000-8000-000000000019"
#define FIELD "50000000-0000-4000-8000-000000000019"
#define FIELD2 "50000000-0000-4000-8000-000000000020"
#define AT "2026-08-01T10:00:00Z"
typedef struct {
  char *dir, *path;
  Database *d;
} Fixture;
static void exec_ok(sqlite3 *d, const char *s) {
  char *m = NULL;
  int rc = sqlite3_exec(d, s, NULL, NULL, &m);
  if (rc != SQLITE_OK)
    g_test_message("SQLite: %s", m);
  g_assert_cmpint(rc, ==, SQLITE_OK);
  sqlite3_free(m);
}
static Fixture fixture(void) {
  Fixture f = {0};
  f.dir = g_dir_make_tmp("labfy-projection-XXXXXX", NULL);
  f.path = g_build_filename(f.dir, "SPECIMEN.sqlite", NULL);
  g_assert_true(database_initialize(f.path, "SPECIMEN", f.dir));
  sqlite3 *s = NULL;
  sqlite3_open(f.path, &s);
  exec_ok(
      s,
      "INSERT INTO "
      "preuves(id,name,relative_path,type_id,size_bytes,sha256,imported_at,"
      "updated_at,status,locked,original_name) VALUES('" EVIDENCE
      "','SPECIMEN','SPECIMEN',2,8,'"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','" AT
      "','" AT "','active',0,'SPECIMEN');INSERT INTO "
               "entites(id,type_id,valeur,label,confiance,created_at,updated_"
               "at,status) VALUES('" PERSON
      "',(SELECT id FROM types_entite WHERE code='person'),'PERSONNE "
      "SPECIMEN','PERSONNE SPECIMEN',0,'" AT "','" AT
      "','active');INSERT INTO "
      "identity_ocr_runs(id,evidence_id,expected_sha256,page_number,document_"
      "type,document_side,engine,requested_languages,available_languages,"
      "parameters,preprocessing_profile,executed_at,status,text_relative_path,"
      "text_sha256,tsv_relative_path,tsv_sha256) VALUES('" RUN "','" EVIDENCE
      "','aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',1,'"
      "identity_card','front','SPECIMEN','fra','fra','SPECIMEN','none','" AT
      "','success','SPECIMEN.txt','"
      "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb','"
      "SPECIMEN.tsv','"
      "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc');"
      "INSERT INTO "
      "identity_document_observations(id,person_id,evidence_id,ocr_run_id,"
      "document_type,document_side,page_number,review_state,observed_at) "
      "VALUES('" DOC "','" PERSON "','" EVIDENCE "','" RUN
      "','identity_card','front',1,'accepted','" AT
      "');INSERT INTO "
      "identity_field_observations(id,observation_id,field_code,raw_value,"
      "normalized_value,review_status,origin,evidence_id,ocr_run_id,page_"
      "number,display_order,reviewed_at,confirmed_value,confirmation_state,"
      "value_quality) VALUES('" FIELD "','" DOC
      "','surname','BRUT','NORMALISÉ','accepted','ocr','" EVIDENCE "','" RUN
      "',1,0,'" AT "','NOM SPECIMEN','human_confirmed','complete'),('" FIELD2
      "','" DOC "','given_names','BRUT "
                "PARTIEL','PARTIEL','modified','manual_override','" EVIDENCE
      "','" RUN "',1,1,'" AT
      "','PRÉNOM SPECIMEN','human_confirmed','partial');");
  sqlite3_close(s);
  f.d = database_open(f.path);
  g_assert_true(database_migrate_to_latest(f.d));
  return f;
}
static void done(Fixture *f) {
  database_close(f->d);
  g_remove(f->path);
  g_rmdir(f->dir);
  g_free(f->path);
  g_free(f->dir);
}
static PersonOcrFieldProjection *
projection(const char *field, const char *code, const char *value,
           const char *q, const char *target, const char *current,
           PersonOcrProjectionStrategy strategy) {
  return person_ocr_field_projection_new(
      EVIDENCE, RUN, field, code, value, q,
      g_strcmp0(q, "partial") == 0 ? "modified" : "accepted", target, current,
      strategy, TRUE);
}
static void test_model_mapping(void) {
  PersonOcrFieldProjection *p =
      projection(FIELD, "surname", "NOM SPECIMEN", "complete", "surname", NULL,
                 PERSON_OCR_FILL_EMPTY);
  g_assert_nonnull(p);
  PersonOcrFieldProjection *c = person_ocr_field_projection_copy(p);
  g_assert_cmpstr(person_ocr_field_projection_get_confirmed_value(c), ==,
                  "NOM SPECIMEN");
  g_assert_true(
      person_ocr_projection_mapping_is_compatible("surname", "surname"));
  g_assert_false(person_ocr_projection_mapping_is_compatible("document_number",
                                                             "surname"));
  g_assert_null(person_ocr_field_projection_new(
      EVIDENCE, RUN, FIELD, "surname", "", "complete", "accepted", "surname",
      NULL, PERSON_OCR_FILL_EMPTY, TRUE));
  g_assert_null(person_ocr_field_projection_new(
      EVIDENCE, RUN, FIELD, "surname", "NOM", NULL, "accepted", "surname",
      NULL, PERSON_OCR_FILL_EMPTY, TRUE));
  g_assert_null(person_ocr_field_projection_new(
      EVIDENCE, RUN, FIELD, "surname", "NOM", "uncertain", "accepted",
      "surname", NULL, PERSON_OCR_FILL_EMPTY, TRUE));
  g_assert_null(person_ocr_field_projection_new(
      EVIDENCE, RUN, FIELD, "surname", "NOM", "complete", "rejected",
      "surname", NULL, PERSON_OCR_FILL_EMPTY, TRUE));
  char invalid_utf8[] = {(char)0xff, 0};
  g_assert_null(person_ocr_field_projection_new(
      EVIDENCE, RUN, FIELD, "surname", invalid_utf8, "complete", "accepted",
      "surname", NULL, PERSON_OCR_FILL_EMPTY, TRUE));
  person_ocr_field_projection_free(c);
  person_ocr_field_projection_free(p);
}
static void test_conflicts_and_foreign_sources(void) {
  Fixture f = fixture();
  GError *e = NULL;
  sqlite3 *s = NULL;
  database_close(f.d);
  f.d = NULL;
  sqlite3_open(f.path, &s);
  exec_ok(s, "INSERT INTO person_profile_fields VALUES('" PERSON
             "','surname','ANCIEN SPECIMEN','" AT "');");
  sqlite3_close(s);
  f.d = database_open(f.path);
  GPtrArray *a = g_ptr_array_new_with_free_func(
      (GDestroyNotify)person_ocr_field_projection_free);
  g_ptr_array_add(a, projection(FIELD, "surname", "NOM SPECIMEN", "complete",
                                "surname", "ANCIEN SPECIMEN",
                                PERSON_OCR_KEEP_EXISTING));
  g_assert_true(person_ocr_projection_service_apply(f.d, PERSON, a, &e));
  PersonOcrProjectionDao *dao = person_ocr_projection_dao_new(f.d);
  char *value = person_ocr_projection_dao_get_value(dao, PERSON, "surname", &e);
  g_assert_cmpstr(value, ==, "ANCIEN SPECIMEN");
  g_free(value);
  g_ptr_array_set_size(a, 0);
  g_ptr_array_add(a, projection(FIELD, "surname", "NOM SPECIMEN", "complete",
                                "surname", "ANCIEN SPECIMEN",
                                PERSON_OCR_REPLACE_EXISTING));
  g_assert_true(person_ocr_projection_service_apply(f.d, PERSON, a, &e));
  GHashTable *fields = person_ocr_projection_dao_list_profile_fields(
      dao, PERSON, &e);
  g_assert_no_error(e);
  g_assert_cmpstr(g_hash_table_lookup(fields, "surname"), ==, "NOM SPECIMEN");
  g_hash_table_unref(fields);
  g_ptr_array_set_size(a, 0);
  g_ptr_array_add(a, person_ocr_field_projection_new(
                         EVIDENCE, "30000000-0000-4000-8000-000000000099",
                         FIELD, "surname", "NOM SPECIMEN", "complete",
                         "accepted", "surname", "NOM SPECIMEN",
                         PERSON_OCR_REPLACE_EXISTING, TRUE));
  g_assert_false(person_ocr_projection_service_apply(f.d, PERSON, a, &e));
  g_assert_nonnull(e);
  g_clear_error(&e);
  person_ocr_projection_dao_free(dao);
  g_ptr_array_unref(a);
  done(&f);
}
static void test_mid_series_rollback(void) {
  Fixture f = fixture();
  GError *e = NULL;
  GPtrArray *a = g_ptr_array_new_with_free_func(
      (GDestroyNotify)person_ocr_field_projection_free);
  g_ptr_array_add(a, projection(FIELD2, "given_names", "PRÉNOM SPECIMEN",
                                "partial", "given_names", NULL,
                                PERSON_OCR_FILL_EMPTY));
  g_ptr_array_add(a, person_ocr_field_projection_new(
                         EVIDENCE, RUN, FIELD, "surname", "VALEUR OBSOLÈTE",
                         "complete", "accepted", "surname", NULL,
                         PERSON_OCR_FILL_EMPTY, TRUE));
  g_assert_false(person_ocr_projection_service_apply(f.d, PERSON, a, &e));
  g_assert_nonnull(e);
  g_clear_error(&e);
  PersonOcrProjectionDao *dao = person_ocr_projection_dao_new(f.d);
  char *value = person_ocr_projection_dao_get_value(
      dao, PERSON, "given_names", &e);
  g_assert_null(value);
  GPtrArray *history = person_ocr_projection_dao_list(dao, PERSON, &e);
  g_assert_cmpuint(history->len, ==, 0);
  g_ptr_array_unref(history);
  person_ocr_projection_dao_free(dao);
  g_ptr_array_unref(a);
  done(&f);
}
static void test_apply_stale_rollback(void) {
  Fixture f = fixture();
  GError *e = NULL;
  GPtrArray *candidates =
      person_ocr_projection_service_candidates(f.d, EVIDENCE, RUN, &e);
  g_assert_no_error(e);
  g_assert_cmpuint(candidates->len, ==, 2);
  g_ptr_array_unref(candidates);
  GPtrArray *a = g_ptr_array_new_with_free_func(
      (GDestroyNotify)person_ocr_field_projection_free);
  g_ptr_array_add(a, projection(FIELD, "surname", "NOM SPECIMEN", "complete",
                                "surname", NULL, PERSON_OCR_FILL_EMPTY));
  g_ptr_array_add(a, projection(FIELD2, "given_names", "PRÉNOM SPECIMEN",
                                "partial", "given_names", NULL,
                                PERSON_OCR_FILL_EMPTY));
  g_assert_true(person_ocr_projection_service_apply(f.d, PERSON, a, &e));
  PersonOcrProjectionDao *dao = person_ocr_projection_dao_new(f.d);
  char *v = person_ocr_projection_dao_get_value(dao, PERSON, "surname", &e);
  g_assert_cmpstr(v, ==, "NOM SPECIMEN");
  g_free(v);
  GPtrArray *h = person_ocr_projection_dao_list(dao, PERSON, &e);
  g_assert_cmpuint(h->len, ==, 2);
  g_ptr_array_unref(h);
  sqlite3 *s = NULL;
  database_close(f.d);
  f.d = NULL;
  sqlite3_open(f.path, &s);
  exec_ok(s, "UPDATE identity_field_observations SET "
             "review_status='rejected',confirmed_value=NULL,confirmation_state="
             "'unconfirmed' WHERE id='" FIELD "';");
  sqlite3_close(s);
  f.d = database_open(f.path);
  g_ptr_array_set_size(a, 0);
  g_ptr_array_add(a, projection(FIELD, "surname", "NOM SPECIMEN", "complete",
                                "surname", "NOM SPECIMEN",
                                PERSON_OCR_REPLACE_EXISTING));
  g_assert_false(person_ocr_projection_service_apply(f.d, PERSON, a, &e));
  g_assert_nonnull(e);
  g_clear_error(&e);
  person_ocr_projection_dao_free(dao);
  dao = person_ocr_projection_dao_new(f.d);
  h = person_ocr_projection_dao_list(dao, PERSON, &e);
  g_assert_cmpuint(h->len, ==, 2);
  g_ptr_array_unref(h);
  person_ocr_projection_dao_free(dao);
  g_ptr_array_unref(a);
  done(&f);
}
int main(int argc, char **argv) {
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/projection/model-mapping", test_model_mapping);
  g_test_add_func("/projection/apply-stale-rollback",
                  test_apply_stale_rollback);
  g_test_add_func("/projection/conflicts-foreign-sources",
                  test_conflicts_and_foreign_sources);
  g_test_add_func("/projection/mid-series-rollback", test_mid_series_rollback);
  return g_test_run();
}
