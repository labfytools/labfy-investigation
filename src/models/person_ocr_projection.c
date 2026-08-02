#include "models/person_ocr_projection.h"
struct PersonOcrFieldProjection {
  char *evidence, *run, *field, *ocr_code, *value, *quality, *status, *target,
      *current;
  PersonOcrProjectionStrategy strategy;
  gboolean confirmed;
};
static gboolean text(const char *v) {
  return v && *v && g_utf8_validate(v, -1, NULL);
}
PersonOcrFieldProjection *person_ocr_field_projection_new(
    const char *e, const char *r, const char *f, const char *c, const char *v,
    const char *q, const char *s, const char *t, const char *current,
    PersonOcrProjectionStrategy strategy, gboolean confirmed) {
  PersonOcrFieldProjection *p = g_new0(PersonOcrFieldProjection, 1);
  p->evidence = g_strdup(e);
  p->run = g_strdup(r);
  p->field = g_strdup(f);
  p->ocr_code = g_strdup(c);
  p->value = g_strdup(v);
  p->quality = g_strdup(q);
  p->status = g_strdup(s);
  p->target = g_strdup(t);
  p->current = g_strdup(current);
  p->strategy = strategy;
  p->confirmed = confirmed;
  if (!person_ocr_field_projection_is_valid(p)) {
    person_ocr_field_projection_free(p);
    return NULL;
  }
  return p;
}
gboolean
person_ocr_field_projection_is_valid(const PersonOcrFieldProjection *p) {
  return p && g_uuid_string_is_valid(p->evidence) &&
         g_uuid_string_is_valid(p->run) && g_uuid_string_is_valid(p->field) &&
         text(p->ocr_code) && text(p->value) && text(p->quality) &&
         text(p->status) && text(p->target) &&
         (p->current == NULL || g_utf8_validate(p->current, -1, NULL)) &&
         p->confirmed &&
         (g_str_equal(p->quality, "complete") ||
          g_str_equal(p->quality, "partial")) &&
         (g_str_equal(p->status, "accepted") ||
          g_str_equal(p->status, "modified")) &&
         (p->strategy == PERSON_OCR_KEEP_EXISTING ||
          p->strategy == PERSON_OCR_FILL_EMPTY ||
          p->strategy == PERSON_OCR_REPLACE_EXISTING);
}
PersonOcrFieldProjection *
person_ocr_field_projection_copy(const PersonOcrFieldProjection *p) {
  return p ? person_ocr_field_projection_new(p->evidence, p->run, p->field,
                                             p->ocr_code, p->value, p->quality,
                                             p->status, p->target, p->current,
                                             p->strategy, p->confirmed)
           : NULL;
}
void person_ocr_field_projection_free(PersonOcrFieldProjection *p) {
  if (!p)
    return;
  g_free(p->evidence);
  g_free(p->run);
  g_free(p->field);
  g_free(p->ocr_code);
  g_free(p->value);
  g_free(p->quality);
  g_free(p->status);
  g_free(p->target);
  g_free(p->current);
  g_free(p);
}
#define GET(name, field)                                                       \
  const char *person_ocr_field_projection_get_##name(                          \
      const PersonOcrFieldProjection *p) {                                     \
    return p ? p->field : NULL;                                                \
  }
GET(evidence_id, evidence)
GET(run_id, run) GET(ocr_field_id, field) GET(ocr_code, ocr_code)
    GET(confirmed_value, value) GET(quality, quality) GET(review_status, status)
        GET(person_field, target)
            GET(current_value, current) PersonOcrProjectionStrategy
    person_ocr_field_projection_get_strategy(
        const PersonOcrFieldProjection *p) {
  return p ? p->strategy : PERSON_OCR_KEEP_EXISTING;
}
gboolean person_ocr_field_projection_get_human_confirmed(
    const PersonOcrFieldProjection *p) {
  return p && p->confirmed;
}
gboolean
person_ocr_field_projection_replace_evidence_id(PersonOcrFieldProjection *p,
                                                const char *e) {
  if (!p || !g_uuid_string_is_valid(e))
    return FALSE;
  g_free(p->evidence);
  p->evidence = g_strdup(e);
  return TRUE;
}
