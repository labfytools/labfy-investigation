#ifndef LABFY_PERSON_OCR_PROJECTION_H
#define LABFY_PERSON_OCR_PROJECTION_H
#include <glib.h>
G_BEGIN_DECLS
typedef enum { PERSON_OCR_KEEP_EXISTING,PERSON_OCR_FILL_EMPTY,PERSON_OCR_REPLACE_EXISTING } PersonOcrProjectionStrategy;
typedef struct PersonOcrFieldProjection PersonOcrFieldProjection;
PersonOcrFieldProjection *person_ocr_field_projection_new(const char *evidence_id,
 const char *run_id,const char *ocr_field_id,const char *ocr_code,
 const char *confirmed_value,const char *quality,const char *review_status,
 const char *person_field,const char *current_value,
 PersonOcrProjectionStrategy strategy,gboolean human_confirmed);
PersonOcrFieldProjection *person_ocr_field_projection_copy(const PersonOcrFieldProjection *projection);
void person_ocr_field_projection_free(PersonOcrFieldProjection *projection);
gboolean person_ocr_field_projection_is_valid(const PersonOcrFieldProjection *projection);
const char *person_ocr_field_projection_get_evidence_id(const PersonOcrFieldProjection *p);
const char *person_ocr_field_projection_get_run_id(const PersonOcrFieldProjection *p);
const char *person_ocr_field_projection_get_ocr_field_id(const PersonOcrFieldProjection *p);
const char *person_ocr_field_projection_get_ocr_code(const PersonOcrFieldProjection *p);
const char *person_ocr_field_projection_get_confirmed_value(const PersonOcrFieldProjection *p);
const char *person_ocr_field_projection_get_quality(const PersonOcrFieldProjection *p);
const char *person_ocr_field_projection_get_review_status(const PersonOcrFieldProjection *p);
const char *person_ocr_field_projection_get_person_field(const PersonOcrFieldProjection *p);
const char *person_ocr_field_projection_get_current_value(const PersonOcrFieldProjection *p);
PersonOcrProjectionStrategy person_ocr_field_projection_get_strategy(const PersonOcrFieldProjection *p);
gboolean person_ocr_field_projection_get_human_confirmed(const PersonOcrFieldProjection *p);
gboolean person_ocr_field_projection_replace_evidence_id(PersonOcrFieldProjection *p,const char *evidence_id);
G_END_DECLS
#endif
