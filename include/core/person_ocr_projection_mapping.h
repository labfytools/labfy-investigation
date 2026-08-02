#ifndef LABFY_PERSON_OCR_PROJECTION_MAPPING_H
#define LABFY_PERSON_OCR_PROJECTION_MAPPING_H
#include <glib.h>
G_BEGIN_DECLS
typedef struct{const char*ocr_code;const char*person_field;const char*label;}PersonOcrProjectionMapping;
const PersonOcrProjectionMapping *person_ocr_projection_mapping_for(const char *ocr_code);
gboolean person_ocr_projection_mapping_is_compatible(const char *ocr_code,const char *person_field);
G_END_DECLS
#endif
