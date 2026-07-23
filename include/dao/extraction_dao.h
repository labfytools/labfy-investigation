#ifndef LABFY_INVESTIGATION_EXTRACTION_DAO_H
#define LABFY_INVESTIGATION_EXTRACTION_DAO_H

#include "database/database.h"
#include <glib.h>

typedef struct ExtractionDao ExtractionDao;

ExtractionDao *extraction_dao_new(Database *database, GError **error);
void extraction_dao_free(ExtractionDao *dao);
gboolean extraction_dao_insert(ExtractionDao *dao, const char *id,
    const char *evidence_id, const char *source_kind, const char *source_id,
    const char *tool_id, const char *created_at, GError **error);

#endif
