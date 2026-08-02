#ifndef LABFY_PERSON_OCR_PROJECTION_SERVICE_H
#define LABFY_PERSON_OCR_PROJECTION_SERVICE_H
#include "database/database.h"
#include "models/person_ocr_projection.h"
#include <glib.h>
G_BEGIN_DECLS
GPtrArray *person_ocr_projection_service_candidates(Database *database,const char *evidence_id,const char *run_id,GError **error);
gboolean person_ocr_projection_service_apply(Database *database,const char *person_id,const GPtrArray *projections,GError **error);
G_END_DECLS
#endif
