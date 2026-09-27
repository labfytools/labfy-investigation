#include "dao/extraction_dao.h"
#include "database/statement.h"
#include <glib.h>
#include <gio/gio.h>

struct ExtractionDao { Database *database; };

static void extraction_dao_set_error(GError **error, GIOErrorEnum code,
    const char *message)
{
    if (error != NULL && *error == NULL)
        g_set_error_literal(error, G_IO_ERROR, code, message);
}

ExtractionDao *extraction_dao_new(Database *database, GError **error)
{
    if (database == NULL)
    {
        extraction_dao_set_error(error, G_IO_ERROR_INVALID_ARGUMENT,
            "Connexion SQLite invalide.");
        return NULL;
    }
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
    if (dao == NULL || id == NULL || evidence_id == NULL || source_kind == NULL ||
        source_id == NULL || tool_id == NULL || created_at == NULL)
    {
        extraction_dao_set_error(error, G_IO_ERROR_INVALID_ARGUMENT,
            "Les champs obligatoires de l'extraction sont invalides.");
        return FALSE;
    }
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

void extraction_record_free(ExtractionRecord *record)
{
    if (record == NULL) return;
    g_free(record->identifier);
    g_free(record->evidence_identifier);
    g_free(record->source_kind);
    g_free(record->source_identifier);
    g_free(record->tool_identifier);
    g_free(record->created_at);
    g_free(record);
}

static ExtractionRecord *extraction_dao_read_record(
    DatabaseStatement *statement,
    GError **error)
{
    ExtractionRecord *record = g_new0(ExtractionRecord, 1);
    if (record == NULL)
    {
        extraction_dao_set_error(error, G_IO_ERROR_NO_SPACE,
            "Impossible d'allouer l'extraction persistée.");
        return NULL;
    }
    if (!database_statement_column_text(statement, 0, &record->identifier) ||
        !database_statement_column_text(statement, 1,
            &record->evidence_identifier) ||
        !database_statement_column_text(statement, 2, &record->source_kind) ||
        !database_statement_column_text(statement, 3,
            &record->source_identifier) ||
        !database_statement_column_text(statement, 4,
            &record->tool_identifier) ||
        !database_statement_column_text(statement, 5, &record->created_at) ||
        record->identifier == NULL || record->source_kind == NULL ||
        record->source_identifier == NULL || record->tool_identifier == NULL ||
        record->created_at == NULL)
    {
        extraction_record_free(record);
        extraction_dao_set_error(error, G_IO_ERROR_INVALID_DATA,
            "Une extraction persistée est incomplète ou illisible.");
        return NULL;
    }
    return record;
}

ExtractionRecord *extraction_dao_find_by_identifier(
    ExtractionDao *dao,
    const char *identifier,
    GError **error)
{
    DatabaseStatement *statement = NULL;
    ExtractionRecord *record = NULL;
    DatabaseStatementStepResult step = DATABASE_STATEMENT_STEP_ERROR;
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);
    if (dao == NULL || identifier == NULL || identifier[0] == '\0')
    {
        extraction_dao_set_error(error, G_IO_ERROR_INVALID_ARGUMENT,
            "L'identifiant d'extraction est invalide.");
        return NULL;
    }
    statement = database_statement_prepare(dao->database,
        "SELECT id,evidence_id,source_kind,source_id,tool_id,created_at "
        "FROM extractions WHERE id=?;");
    if (statement == NULL ||
        !database_statement_bind_text(statement, 1, identifier))
    {
        extraction_dao_set_error(error, G_IO_ERROR_FAILED,
            "Impossible de préparer la lecture de l'extraction.");
        goto cleanup;
    }
    step = database_statement_step(statement);
    if (step == DATABASE_STATEMENT_STEP_ROW)
        record = extraction_dao_read_record(statement, error);
    else if (step == DATABASE_STATEMENT_STEP_ERROR)
        extraction_dao_set_error(error, G_IO_ERROR_FAILED,
            "Impossible de lire l'extraction persistée.");
cleanup:
    database_statement_finalize(statement);
    return record;
}

GPtrArray *extraction_dao_list_all(ExtractionDao *dao, GError **error)
{
    DatabaseStatement *statement = NULL;
    GPtrArray *records = NULL;
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);
    if (dao == NULL)
    {
        extraction_dao_set_error(error, G_IO_ERROR_INVALID_ARGUMENT,
            "Le DAO des extractions est invalide.");
        return NULL;
    }
    records = g_ptr_array_new_with_free_func(
        (GDestroyNotify) extraction_record_free);
    statement = database_statement_prepare(dao->database,
        "SELECT id,evidence_id,source_kind,source_id,tool_id,created_at "
        "FROM extractions ORDER BY id;");
    if (records == NULL || statement == NULL)
    {
        extraction_dao_set_error(error, G_IO_ERROR_FAILED,
            "Impossible de préparer la liste des extractions.");
        goto failure;
    }
    for (;;)
    {
        DatabaseStatementStepResult step = database_statement_step(statement);
        if (step == DATABASE_STATEMENT_STEP_DONE) break;
        if (step != DATABASE_STATEMENT_STEP_ROW)
        {
            extraction_dao_set_error(error, G_IO_ERROR_FAILED,
                "Impossible de parcourir les extractions.");
            goto failure;
        }
        ExtractionRecord *record = extraction_dao_read_record(statement, error);
        if (record == NULL) goto failure;
        g_ptr_array_add(records, record);
    }
    database_statement_finalize(statement);
    return records;
failure:
    database_statement_finalize(statement);
    g_clear_pointer(&records, g_ptr_array_unref);
    return NULL;
}
