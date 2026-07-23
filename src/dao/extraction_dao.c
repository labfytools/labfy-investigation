#include "dao/extraction_dao.h"
#include "database/statement.h"
#include <glib.h>
#include <gio/gio.h>

struct ExtractionDao { Database *database; };

ExtractionDao *extraction_dao_new(Database *database, GError **error)
{
    if (database == NULL) { g_set_error_literal(error, G_IO_ERROR,
        G_IO_ERROR_INVALID_ARGUMENT, "Connexion SQLite invalide."); return NULL; }
    ExtractionDao *dao = g_new0(ExtractionDao, 1);
    dao->database = database;
    return dao;
}

void extraction_dao_free(ExtractionDao *dao) { g_free(dao); }

gboolean extraction_dao_insert(ExtractionDao *dao, const char *id,
    const char *evidence_id, const char *source_kind, const char *source_id,
    const char *tool_id, const char *created_at, GError **error)
{
    DatabaseStatement *statement = NULL;
    gboolean ok = FALSE;
    (void) error;
    if (dao == NULL || id == NULL || evidence_id == NULL || source_kind == NULL ||
        source_id == NULL || tool_id == NULL || created_at == NULL) return FALSE;
    statement = database_statement_prepare(dao->database,
        "INSERT INTO extractions(id,evidence_id,source_kind,source_id,tool_id,created_at) VALUES(?,?,?,?,?,?);");
    if (statement == NULL) return FALSE;
    ok = database_statement_bind_text(statement, 1, id) &&
        (evidence_id != NULL ? database_statement_bind_text(statement, 2, evidence_id) :
            database_statement_bind_null(statement, 2)) &&
        database_statement_bind_text(statement, 3, source_kind) &&
        database_statement_bind_text(statement, 4, source_id) &&
        database_statement_bind_text(statement, 5, tool_id) &&
        database_statement_bind_text(statement, 6, created_at) &&
        database_statement_step(statement) == DATABASE_STATEMENT_STEP_DONE;
    if (!ok && error != NULL && *error == NULL)
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED,
            "Impossible d'insérer la traçabilité de l'extraction.");
    database_statement_finalize(statement);
    return ok;
}
