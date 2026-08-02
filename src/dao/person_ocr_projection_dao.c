#include "dao/person_ocr_projection_dao.h"
#include "database/statement.h"
struct PersonOcrProjectionDao {
  Database *database;
};
static GQuark domain(void) {
  return g_quark_from_static_string("person-ocr-projection-dao");
}
PersonOcrProjectionDao *person_ocr_projection_dao_new(Database *d) {
  if (!d)
    return NULL;
  PersonOcrProjectionDao *x = g_new0(PersonOcrProjectionDao, 1);
  x->database = d;
  return x;
}
void person_ocr_projection_dao_free(PersonOcrProjectionDao *d) { g_free(d); }
char *person_ocr_projection_dao_get_value(PersonOcrProjectionDao *d,
                                          const char *p, const char *f,
                                          GError **e) {
  if (!d || !p || !f)
    return NULL;
  DatabaseStatement *s = database_statement_prepare(
      d->database, "SELECT value FROM person_profile_fields WHERE person_id=? "
                   "AND field_code=?;");
  char *v = NULL;
  if (!s || !database_statement_bind_text(s, 1, p) ||
      !database_statement_bind_text(s, 2, f))
    goto fail;
  DatabaseStatementStepResult step = database_statement_step(s);
  if (step == DATABASE_STATEMENT_STEP_ROW)
    database_statement_column_text(s, 0, &v);
  else if (step != DATABASE_STATEMENT_STEP_DONE)
    goto fail;
  database_statement_finalize(s);
  return v;
fail:
  database_statement_finalize(s);
  g_set_error_literal(e, domain(), 1,
                      "Impossible de lire le champ de la personne.");
  return NULL;
}
gboolean person_ocr_projection_dao_apply(PersonOcrProjectionDao *d,
                                         const char *person,
                                         const PersonOcrFieldProjection *p,
                                         const char *at, GError **e) {
  if (!d || !person_ocr_field_projection_is_valid(p) || !at)
    return FALSE;
  const char *target = person_ocr_field_projection_get_person_field(p),
             *value = person_ocr_field_projection_get_confirmed_value(p);
  DatabaseStatement *s = database_statement_prepare(
      d->database,
      "INSERT INTO "
      "person_profile_fields(person_id,field_code,value,updated_at) "
      "VALUES(?,?,?,?) ON CONFLICT(person_id,field_code) DO UPDATE SET "
      "value=excluded.value,updated_at=excluded.updated_at;");
  gboolean ok = s && database_statement_bind_text(s, 1, person) &&
                database_statement_bind_text(s, 2, target) &&
                database_statement_bind_text(s, 3, value) &&
                database_statement_bind_text(s, 4, at) &&
                database_statement_step(s) == DATABASE_STATEMENT_STEP_DONE;
  database_statement_finalize(s);
  if (!ok)
    goto fail;
  char *id = g_uuid_string_random();
  s = database_statement_prepare(d->database,
                                 "INSERT INTO person_ocr_field_projections "
                                 "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?);");
  const char *previous = person_ocr_field_projection_get_current_value(p);
  const char *strategy =
      person_ocr_field_projection_get_strategy(p) == PERSON_OCR_REPLACE_EXISTING
          ? "replace_existing"
          : "fill_empty";
  ok = s && database_statement_bind_text(s, 1, id) &&
       database_statement_bind_text(s, 2, person) &&
       database_statement_bind_text(s, 3, target) &&
       (previous ? database_statement_bind_text(s, 4, previous)
                 : database_statement_bind_null(s, 4)) &&
       database_statement_bind_text(s, 5, value) &&
       database_statement_bind_text(
           s, 6, person_ocr_field_projection_get_evidence_id(p)) &&
       database_statement_bind_text(
           s, 7, person_ocr_field_projection_get_run_id(p)) &&
       database_statement_bind_text(
           s, 8, person_ocr_field_projection_get_ocr_field_id(p)) &&
       database_statement_bind_text(
           s, 9, person_ocr_field_projection_get_ocr_code(p)) &&
       database_statement_bind_text(
           s, 10, person_ocr_field_projection_get_quality(p)) &&
       database_statement_bind_text(
           s, 11, person_ocr_field_projection_get_review_status(p)) &&
       database_statement_bind_text(s, 12, strategy) &&
       database_statement_bind_text(s, 13, at) &&
       database_statement_bind_text(s, 14, "human") &&
       database_statement_step(s) == DATABASE_STATEMENT_STEP_DONE;
  database_statement_finalize(s);
  g_free(id);
  if (ok)
    return TRUE;
fail:
  g_set_error_literal(e, domain(), 2,
                      "Impossible de conserver la projection OCR.");
  return FALSE;
}
GPtrArray *person_ocr_projection_dao_list(PersonOcrProjectionDao *d,
                                          const char *p, GError **e) {
  GPtrArray *a = g_ptr_array_new_with_free_func(g_free);
  DatabaseStatement *s =
      d ? database_statement_prepare(
              d->database, "SELECT id FROM person_ocr_field_projections WHERE "
                           "person_id=? ORDER BY projected_at,id;")
        : NULL;
  if (!s || !database_statement_bind_text(s, 1, p))
    goto fail;
  for (;;) {
    DatabaseStatementStepResult step = database_statement_step(s);
    if (step == DATABASE_STATEMENT_STEP_DONE)
      break;
    if (step != DATABASE_STATEMENT_STEP_ROW)
      goto fail;
    char *id = NULL;
    if (!database_statement_column_text(s, 0, &id))
      goto fail;
    g_ptr_array_add(a, id);
  }
  database_statement_finalize(s);
  return a;
fail:
  database_statement_finalize(s);
  g_ptr_array_unref(a);
  g_set_error_literal(e, domain(), 3,
                      "Impossible de lire les projections OCR.");
  return NULL;
}
GHashTable *person_ocr_projection_dao_list_profile_fields(
    PersonOcrProjectionDao *d, const char *person, GError **e) {
  GHashTable *fields = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                             g_free);
  DatabaseStatement *s = d ? database_statement_prepare(
                                 d->database,
                                 "SELECT field_code,value FROM "
                                 "person_profile_fields WHERE person_id=? "
                                 "ORDER BY field_code;")
                           : NULL;
  if (!s || !database_statement_bind_text(s, 1, person))
    goto fail;
  for (;;) {
    DatabaseStatementStepResult step = database_statement_step(s);
    if (step == DATABASE_STATEMENT_STEP_DONE)
      break;
    char *field = NULL, *value = NULL;
    if (step != DATABASE_STATEMENT_STEP_ROW ||
        !database_statement_column_text(s, 0, &field) ||
        !database_statement_column_text(s, 1, &value)) {
      g_free(field);
      g_free(value);
      goto fail;
    }
    g_hash_table_insert(fields, field, value);
  }
  database_statement_finalize(s);
  return fields;
fail:
  database_statement_finalize(s);
  g_hash_table_unref(fields);
  g_set_error_literal(e, domain(), 4,
                      "Impossible de lire le profil structuré.");
  return NULL;
}
