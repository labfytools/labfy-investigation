#include "core/person_ocr_projection_service.h"
#include "core/person_ocr_projection_mapping.h"
#include "dao/entity_dao.h"
#include "dao/identity_ocr_dao.h"
#include "dao/person_ocr_projection_dao.h"
#include "database/transaction.h"
#include "models/identity_traceability.h"
static GQuark domain(void) {
  return g_quark_from_static_string("person-ocr-projection-service");
}
GPtrArray *person_ocr_projection_service_candidates(Database *d,
                                                    const char *evidence,
                                                    const char *run,
                                                    GError **error) {
  if (!d || !g_uuid_string_is_valid(evidence) ||
      !g_uuid_string_is_valid(run)) {
    g_set_error_literal(error, domain(), 1,
                        "La preuve ou l’exécution OCR est invalide.");
    return NULL;
  }
  IdentityOcrDao *dao = identity_ocr_dao_new(d);
  IdentityOcrRunRecord *r =
      dao ? identity_ocr_dao_find_run(dao, run, error) : NULL;
  if (!r || g_strcmp0(r->evidence_id, evidence) != 0) {
    if (error && !*error)
      g_set_error_literal(error, domain(), 1,
                          "L’exécution OCR ne correspond pas à la preuve.");
    identity_ocr_run_record_free(r);
    identity_ocr_dao_free(dao);
    return NULL;
  }
  GPtrArray *docs = identity_ocr_dao_list_documents_by_evidence(dao, evidence,
                                                                error),
            *out = g_ptr_array_new_with_free_func(
                (GDestroyNotify)identity_field_observation_record_free);
  if (docs == NULL) {
    g_ptr_array_unref(out);
    identity_ocr_run_record_free(r);
    identity_ocr_dao_free(dao);
    return NULL;
  }
  for (guint i = 0; docs && i < docs->len; i++) {
    IdentityDocumentObservationRecord *doc = g_ptr_array_index(docs, i);
    if (g_strcmp0(doc->ocr_run_id, run))
      continue;
    GPtrArray *f = identity_ocr_dao_list_confirmed_fields(dao, doc->id, error);
    if (f == NULL) {
      g_ptr_array_unref(out);
      g_ptr_array_unref(docs);
      identity_ocr_run_record_free(r);
      identity_ocr_dao_free(dao);
      return NULL;
    }
    for (guint j = 0; f && j < f->len; j++) {
      IdentityFieldObservationRecord *v = g_ptr_array_index(f, j);
      if (person_ocr_projection_mapping_for(v->field_code))
        g_ptr_array_add(out, g_ptr_array_steal_index(f, j--));
    }
    g_clear_pointer(&f, g_ptr_array_unref);
  }
  g_clear_pointer(&docs, g_ptr_array_unref);
  identity_ocr_run_record_free(r);
  identity_ocr_dao_free(dao);
  return out;
}
gboolean person_ocr_projection_service_apply(Database *d, const char *person,
                                             const GPtrArray *items,
                                             GError **error) {
  if (!d || !g_uuid_string_is_valid(person) || !items) {
    g_set_error_literal(error, domain(), 2, "Projection invalide.");
    return FALSE;
  }
  EntityDao *entities = entity_dao_new(d, error);
  EntityRecord *entity =
      entities ? entity_dao_find_by_identifier(entities, person, error) : NULL;
  if (!entity ||
      g_strcmp0(entity_record_get_type_identifier(entity), "person")) {
    if (error && !*error)
      g_set_error_literal(error, domain(), 2,
                          "La personne cible est invalide.");
    entity_record_free(entity);
    entity_dao_free(entities);
    return FALSE;
  }
  entity_record_free(entity);
  entity_dao_free(entities);
  PersonOcrProjectionDao *dao = person_ocr_projection_dao_new(d);
  IdentityOcrDao *ocr = identity_ocr_dao_new(d);
  gboolean owns = !database_transaction_is_active(d);
  if (owns && !database_transaction_begin(d))
    goto fail;
  GDateTime *now = g_date_time_new_now_utc();
  char *at = g_date_time_format(now, "%Y-%m-%dT%H:%M:%SZ");
  for (guint i = 0; i < items->len; i++) {
    PersonOcrFieldProjection *p = g_ptr_array_index((GPtrArray *)items, i);
    if (person_ocr_field_projection_get_strategy(p) == PERSON_OCR_KEEP_EXISTING)
      continue;
    IdentityFieldObservationRecord *f = identity_ocr_dao_find_field(
        ocr, person_ocr_field_projection_get_ocr_field_id(p), error);
    char *current = person_ocr_projection_dao_get_value(
        dao, person, person_ocr_field_projection_get_person_field(p), error);
    if (error && *error) {
      identity_field_observation_record_free(f);
      g_free(current);
      g_free(at);
      g_date_time_unref(now);
      goto fail;
    }
    gboolean valid =
        f &&
        identity_traceability_field_is_projectable(
            f->review_status, f->value_quality, f->confirmation_state,
            f->confirmed_value) &&
        g_strcmp0(f->evidence_id,
                  person_ocr_field_projection_get_evidence_id(p)) == 0 &&
        g_strcmp0(f->ocr_run_id, person_ocr_field_projection_get_run_id(p)) ==
            0 &&
        g_strcmp0(f->field_code, person_ocr_field_projection_get_ocr_code(p)) ==
            0 &&
        g_strcmp0(f->confirmed_value,
                  person_ocr_field_projection_get_confirmed_value(p)) == 0 &&
        person_ocr_projection_mapping_is_compatible(
            f->field_code, person_ocr_field_projection_get_person_field(p)) &&
        g_strcmp0(current, person_ocr_field_projection_get_current_value(p)) ==
            0 &&
        ((!current && person_ocr_field_projection_get_strategy(p) ==
                          PERSON_OCR_FILL_EMPTY) ||
         (current && person_ocr_field_projection_get_strategy(p) ==
                         PERSON_OCR_REPLACE_EXISTING));
    identity_field_observation_record_free(f);
    g_free(current);
    if (!valid || !person_ocr_projection_dao_apply(dao, person, p, at, error)) {
      g_free(at);
      g_date_time_unref(now);
      goto fail;
    }
  }
  g_free(at);
  g_date_time_unref(now);
  if (owns && !database_transaction_commit(d))
    goto fail;
  person_ocr_projection_dao_free(dao);
  identity_ocr_dao_free(ocr);
  return TRUE;
fail:
  if (owns && database_transaction_is_active(d))
    database_transaction_rollback(d);
  if (error && !*error)
    g_set_error_literal(error, domain(), 3,
                        "La valeur OCR a changé ou n’est plus projetable.");
  person_ocr_projection_dao_free(dao);
  identity_ocr_dao_free(ocr);
  return FALSE;
}
