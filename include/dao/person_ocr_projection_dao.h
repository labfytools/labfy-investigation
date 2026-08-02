#ifndef LABFY_PERSON_OCR_PROJECTION_DAO_H
#define LABFY_PERSON_OCR_PROJECTION_DAO_H
#include "database/database.h"
#include "models/person_ocr_projection.h"
#include <glib.h>
G_BEGIN_DECLS
typedef struct PersonOcrProjectionDao PersonOcrProjectionDao;
PersonOcrProjectionDao *person_ocr_projection_dao_new(Database *database);
void person_ocr_projection_dao_free(PersonOcrProjectionDao *dao);
char *person_ocr_projection_dao_get_value(PersonOcrProjectionDao *dao,const char *person_id,const char *field,GError **error);
gboolean person_ocr_projection_dao_apply(PersonOcrProjectionDao *dao,const char *person_id,const PersonOcrFieldProjection *projection,const char *timestamp,GError **error);
GPtrArray *person_ocr_projection_dao_list(PersonOcrProjectionDao *dao,const char *person_id,GError **error);
GHashTable *person_ocr_projection_dao_list_profile_fields(PersonOcrProjectionDao *dao,const char *person_id,GError **error);
G_END_DECLS
#endif
