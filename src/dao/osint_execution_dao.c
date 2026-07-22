/******************************************************************************
 * @file osint_execution_dao.c
 * @brief Persistance de la provenance des exécutions OSINT.
 ******************************************************************************/

#include "dao/osint_execution_dao.h"

#include "database/error.h"
#include "database/statement.h"

struct OsintExecutionDao { Database *database; };

static const char *const insert_sql =
    "INSERT INTO osint_executions(id,tool_identifier,tool_version,"
    "action_identifier,selection_id,selection_kind,target_value,arguments,"
    "started_at,finished_at,exit_code,final_state,stdout_raw,stderr_raw,"
    "output_sha256) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";
static const char *const find_sql =
    "SELECT id,tool_identifier,tool_version,action_identifier,selection_id,"
    "selection_kind,target_value,arguments,started_at,finished_at,exit_code,"
    "final_state,stdout_raw,stderr_raw,output_sha256 FROM osint_executions "
    "WHERE id=?;";

static void osint_execution_dao_set_error(
    OsintExecutionDao *dao, GError **error, const char *context
)
{
    if (error != NULL && *error == NULL)
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "%s : %s", context,
            dao != NULL && database_error_get_message(dao->database) != NULL
                ? database_error_get_message(dao->database) : "erreur SQLite");
}

OsintExecutionDao *osint_execution_dao_new(Database *database, GError **error)
{
    if (database == NULL)
    {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
            "La connexion de provenance OSINT est absente.");
        return NULL;
    }
    OsintExecutionDao *dao = g_try_new0(OsintExecutionDao, 1);
    if (dao != NULL) dao->database = database;
    return dao;
}

void osint_execution_dao_free(OsintExecutionDao *dao) { g_free(dao); }

gboolean osint_execution_dao_insert(
    OsintExecutionDao *dao, const OsintExecutionRecord *record, GError **error
)
{
    DatabaseStatement *statement = NULL;
    GBytes *stdout_raw = NULL;
    GBytes *stderr_raw = NULL;
    gboolean success = FALSE;
    if (dao == NULL || record == NULL) return FALSE;
    statement = database_statement_prepare(dao->database, insert_sql);
    if (statement == NULL) goto cleanup;
    stdout_raw = osint_execution_record_ref_stdout(record);
    stderr_raw = osint_execution_record_ref_stderr(record);
    success = database_statement_bind_text(statement, 1,
            osint_execution_record_get_identifier(record)) &&
        database_statement_bind_text(statement, 2,
            osint_execution_record_get_tool_identifier(record)) &&
        (osint_execution_record_get_tool_version(record) != NULL
            ? database_statement_bind_text(statement, 3,
                osint_execution_record_get_tool_version(record))
            : database_statement_bind_null(statement, 3)) &&
        database_statement_bind_text(statement, 4,
            osint_execution_record_get_action_identifier(record)) &&
        database_statement_bind_text(statement, 5,
            osint_execution_record_get_selection_identifier(record)) &&
        database_statement_bind_text(statement, 6,
            osint_execution_record_get_selection_kind(record)) &&
        database_statement_bind_text(statement, 7,
            osint_execution_record_get_target_value(record)) &&
        database_statement_bind_text(statement, 8,
            osint_execution_record_get_arguments(record)) &&
        database_statement_bind_text(statement, 9,
            osint_execution_record_get_started_at(record)) &&
        database_statement_bind_text(statement, 10,
            osint_execution_record_get_finished_at(record)) &&
        (osint_execution_record_has_exit_code(record)
            ? database_statement_bind_int64(statement, 11,
                osint_execution_record_get_exit_code(record))
            : database_statement_bind_null(statement, 11)) &&
        database_statement_bind_text(statement, 12,
            osint_execution_record_get_final_state(record)) &&
        database_statement_bind_blob(statement, 13, stdout_raw) &&
        database_statement_bind_blob(statement, 14, stderr_raw) &&
        database_statement_bind_text(statement, 15,
            osint_execution_record_get_output_sha256(record)) &&
        database_statement_step(statement) == DATABASE_STATEMENT_STEP_DONE;
cleanup:
    if (!success) osint_execution_dao_set_error(dao, error,
        "Impossible d'insérer l'exécution OSINT");
    g_clear_pointer(&stdout_raw, g_bytes_unref);
    g_clear_pointer(&stderr_raw, g_bytes_unref);
    database_statement_finalize(statement);
    return success;
}

OsintExecutionRecord *osint_execution_dao_find_by_identifier(
    OsintExecutionDao *dao, const char *identifier, GError **error
)
{
    DatabaseStatement *statement = NULL;
    OsintExecutionRecord *record = NULL;
    char *values[12] = {0};
    GBytes *stdout_raw = NULL;
    GBytes *stderr_raw = NULL;
    int64_t exit_code = 0;
    bool exit_is_null = true;
    if (dao == NULL || identifier == NULL) return NULL;
    statement = database_statement_prepare(dao->database, find_sql);
    if (statement == NULL || !database_statement_bind_text(statement, 1, identifier) ||
        database_statement_step(statement) != DATABASE_STATEMENT_STEP_ROW)
        goto cleanup;
    if (!database_statement_column_text(statement, 0, &values[0]) ||
        !database_statement_column_text(statement, 1, &values[1]) ||
        !database_statement_column_text(statement, 2, &values[2]) ||
        !database_statement_column_text(statement, 3, &values[3]) ||
        !database_statement_column_text(statement, 4, &values[4]) ||
        !database_statement_column_text(statement, 5, &values[5]) ||
        !database_statement_column_text(statement, 6, &values[6]) ||
        !database_statement_column_text(statement, 7, &values[7]) ||
        !database_statement_column_text(statement, 8, &values[8]) ||
        !database_statement_column_text(statement, 9, &values[9]) ||
        !database_statement_column_is_null(statement, 10, &exit_is_null) ||
        (!exit_is_null && !database_statement_column_int64(statement, 10, &exit_code)) ||
        !database_statement_column_text(statement, 11, &values[10]) ||
        !database_statement_column_blob(statement, 12, &stdout_raw) ||
        !database_statement_column_blob(statement, 13, &stderr_raw) ||
        !database_statement_column_text(statement, 14, &values[11])) goto cleanup;
    record = osint_execution_record_new(
        values[0], values[1], values[2], values[3], values[4], values[5],
        values[6], values[7], values[8], values[9], !exit_is_null,
        (gint) exit_code, values[10], stdout_raw, stderr_raw, values[11], error);
cleanup:
    if (record == NULL && error != NULL && *error == NULL)
        osint_execution_dao_set_error(dao, error,
            "Impossible de lire l'exécution OSINT");
    for (guint index = 0; index < G_N_ELEMENTS(values); index++) g_free(values[index]);
    g_clear_pointer(&stdout_raw, g_bytes_unref);
    g_clear_pointer(&stderr_raw, g_bytes_unref);
    database_statement_finalize(statement);
    return record;
}

static gboolean osint_execution_dao_link(
    OsintExecutionDao *dao, const char *table, const char *object_column,
    const char *execution_identifier, const char *object_identifier,
    const char *disposition, GError **error
)
{
    char *sql = NULL;
    DatabaseStatement *statement = NULL;
    gboolean success = FALSE;
    if (dao == NULL || execution_identifier == NULL || object_identifier == NULL ||
        (g_strcmp0(disposition, "created") != 0 &&
         g_strcmp0(disposition, "reused") != 0)) return FALSE;
    sql = g_strdup_printf(
        "INSERT OR IGNORE INTO %s(execution_id,%s,disposition) VALUES(?,?,?);",
        table, object_column);
    statement = database_statement_prepare(dao->database, sql);
    success = statement != NULL &&
        database_statement_bind_text(statement, 1, execution_identifier) &&
        database_statement_bind_text(statement, 2, object_identifier) &&
        database_statement_bind_text(statement, 3, disposition) &&
        database_statement_step(statement) == DATABASE_STATEMENT_STEP_DONE;
    if (!success) osint_execution_dao_set_error(dao, error,
        "Impossible de lier la provenance OSINT");
    database_statement_finalize(statement); g_free(sql); return success;
}

gboolean osint_execution_dao_link_entity(
    OsintExecutionDao *dao, const char *execution_identifier,
    const char *entity_identifier, const char *disposition, GError **error)
{ return osint_execution_dao_link(dao, "osint_execution_entities", "entity_id",
    execution_identifier, entity_identifier, disposition, error); }
gboolean osint_execution_dao_link_relation(
    OsintExecutionDao *dao, const char *execution_identifier,
    const char *relation_identifier, const char *disposition, GError **error)
{ return osint_execution_dao_link(dao, "osint_execution_relations", "relation_id",
    execution_identifier, relation_identifier, disposition, error); }
