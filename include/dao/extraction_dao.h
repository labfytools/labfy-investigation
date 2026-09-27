#ifndef LABFY_INVESTIGATION_EXTRACTION_DAO_H
#define LABFY_INVESTIGATION_EXTRACTION_DAO_H

#include "database/database.h"
#include <glib.h>

typedef struct ExtractionDao ExtractionDao;

/**
 * Vue possédée d'une extraction persistée. `evidence_identifier` désigne la
 * preuve dérivée éventuelle ; `source_kind`/`source_identifier` désignent
 * l'entrée de l'extraction. Toutes les chaînes appartiennent au record.
 */
typedef struct
{
    char *identifier;
    char *evidence_identifier;
    char *source_kind;
    char *source_identifier;
    char *tool_identifier;
    char *created_at;
} ExtractionRecord;

ExtractionDao *extraction_dao_new(Database *database, GError **error);
void extraction_dao_free(ExtractionDao *dao);
gboolean extraction_dao_insert(ExtractionDao *dao, const char *id,
    const char *evidence_id, const char *source_kind, const char *source_id,
    const char *tool_id, const char *created_at, GError **error);

void extraction_record_free(ExtractionRecord *record);

/** Retourne un record possédé, ou NULL sans erreur lorsque l'UUID est absent. */
ExtractionRecord *extraction_dao_find_by_identifier(
    ExtractionDao *dao,
    const char *identifier,
    GError **error);

/** Retourne un tableau possédé, trié par identifiant persistant. */
GPtrArray *extraction_dao_list_all(ExtractionDao *dao, GError **error);

#endif
