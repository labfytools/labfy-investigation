#include "dao/relation_type_dao.h"
#include "database/error.h"
#include "database/statement.h"

struct RelationTypeDao { Database *database; };

static void set_db_error(RelationTypeDao *dao, GError **error,
    RelationTypeDaoError code, const char *context)
{
    if (error != NULL && *error == NULL)
        g_set_error(error, RELATION_TYPE_DAO_ERROR, code, "%s : %s", context,
            dao != NULL && database_error_get_message(dao->database) != NULL
                ? database_error_get_message(dao->database) : "erreur inconnue");
}

static RelationType *read_type(DatabaseStatement *statement, GError **error)
{
    int64_t id = 0; int64_t system = 0;
    char *code = NULL; char *label = NULL; char *key = NULL; char *description = NULL;
    RelationType *type = NULL; GError *model_error = NULL;
    if (!database_statement_column_int64(statement, 0, &id) ||
        !database_statement_column_text(statement, 1, &code) ||
        !database_statement_column_text(statement, 2, &label) ||
        !database_statement_column_text(statement, 3, &key) ||
        !database_statement_column_text(statement, 4, &description) ||
        !database_statement_column_int64(statement, 5, &system))
    {
        g_set_error_literal(error, RELATION_TYPE_DAO_ERROR,
            RELATION_TYPE_DAO_ERROR_READ, "Le type de relation est illisible.");
        goto cleanup;
    }
    type = relation_type_new((gint64) id, code, label, key, description,
        system != 0, &model_error);
    if (type == NULL)
        g_set_error(error, RELATION_TYPE_DAO_ERROR,
            RELATION_TYPE_DAO_ERROR_MODEL, "Type de relation invalide : %s",
            model_error != NULL ? model_error->message : "erreur inconnue");
cleanup:
    g_clear_error(&model_error); g_free(description); g_free(key);
    g_free(label); g_free(code); return type;
}

static RelationType *find_one(RelationTypeDao *dao, const char *sql,
    gboolean bind_integer, gint64 integer, const char *text, GError **error)
{
    DatabaseStatement *statement = NULL; RelationType *type = NULL;
    if (dao == NULL || dao->database == NULL) return NULL;
    statement = database_statement_prepare(dao->database, sql);
    if (statement == NULL) { set_db_error(dao, error,
        RELATION_TYPE_DAO_ERROR_PREPARE, "Lecture du type impossible"); return NULL; }
    if ((bind_integer && !database_statement_bind_int64(statement, 1, integer)) ||
        (!bind_integer && !database_statement_bind_text(statement, 1, text)))
    { set_db_error(dao, error, RELATION_TYPE_DAO_ERROR_BIND,
        "Paramètre du type invalide"); goto cleanup; }
    DatabaseStatementStepResult step = database_statement_step(statement);
    if (step == DATABASE_STATEMENT_STEP_ROW) type = read_type(statement, error);
    else if (step == DATABASE_STATEMENT_STEP_ERROR) set_db_error(dao, error,
        RELATION_TYPE_DAO_ERROR_EXECUTE, "Recherche du type impossible");
cleanup:
    database_statement_finalize(statement); return type;
}

GQuark relation_type_dao_error_quark(void)
{ return g_quark_from_static_string("relation-type-dao-error-quark"); }
RelationTypeDao *relation_type_dao_new(Database *database, GError **error)
{
    if (database == NULL) { g_set_error_literal(error, RELATION_TYPE_DAO_ERROR,
        RELATION_TYPE_DAO_ERROR_INVALID_ARGUMENT, "La base est obligatoire.");
        return NULL; }
    RelationTypeDao *dao = g_new0(RelationTypeDao, 1); dao->database = database;
    return dao;
}
void relation_type_dao_free(RelationTypeDao *dao) { g_free(dao); }

GPtrArray *relation_type_dao_list_all(RelationTypeDao *dao, GError **error)
{
    static const char *sql = "SELECT id,code,label,normalized_key,description,"
        "is_system FROM relation_types ORDER BY label COLLATE NOCASE,id;";
    DatabaseStatement *statement = NULL;
    GPtrArray *items = g_ptr_array_new_with_free_func(
        (GDestroyNotify) relation_type_free);
    if (dao == NULL || dao->database == NULL) goto fail;
    statement = database_statement_prepare(dao->database, sql);
    if (statement == NULL) { set_db_error(dao, error,
        RELATION_TYPE_DAO_ERROR_PREPARE, "Liste des types impossible"); goto fail; }
    while (TRUE)
    {
        DatabaseStatementStepResult step = database_statement_step(statement);
        if (step == DATABASE_STATEMENT_STEP_DONE) break;
        if (step == DATABASE_STATEMENT_STEP_ERROR) { set_db_error(dao, error,
            RELATION_TYPE_DAO_ERROR_EXECUTE, "Liste des types impossible"); goto fail; }
        RelationType *type = read_type(statement, error);
        if (type == NULL) goto fail;
        g_ptr_array_add(items, type);
    }
    database_statement_finalize(statement); return items;
fail:
    database_statement_finalize(statement); g_ptr_array_unref(items); return NULL;
}

#define TYPE_COLUMNS "SELECT id,code,label,normalized_key,description,is_system FROM relation_types WHERE "
RelationType *relation_type_dao_find_by_identifier(RelationTypeDao *dao,
    gint64 id, GError **error)
{ return find_one(dao, TYPE_COLUMNS "id=?;", TRUE, id, NULL, error); }
RelationType *relation_type_dao_find_by_code(RelationTypeDao *dao,
    const char *code, GError **error)
{ return find_one(dao, TYPE_COLUMNS "code=?;", FALSE, 0, code, error); }
RelationType *relation_type_dao_find_by_normalized_key(RelationTypeDao *dao,
    const char *key, GError **error)
{ return find_one(dao, TYPE_COLUMNS "normalized_key=?;", FALSE, 0, key, error); }

gboolean relation_type_dao_insert_custom(RelationTypeDao *dao,
    const char *label, const char *key, const char *description,
    gint64 *out_identifier, GError **error)
{
    const char *sql = "INSERT INTO relation_types(code,label,normalized_key,"
        "description,is_system) VALUES(NULL,?,?,?,0);";
    DatabaseStatement *s = database_statement_prepare(dao->database, sql);
    if (s == NULL) { set_db_error(dao,error,RELATION_TYPE_DAO_ERROR_PREPARE,
        "Création du type impossible"); return FALSE; }
    if (!database_statement_bind_text(s,1,label) ||
        !database_statement_bind_text(s,2,key) ||
        (description != NULL ? !database_statement_bind_text(s,3,description)
                             : !database_statement_bind_null(s,3)) ||
        database_statement_step(s) != DATABASE_STATEMENT_STEP_DONE)
    { set_db_error(dao,error,RELATION_TYPE_DAO_ERROR_CONSTRAINT,
        "Ce libellé de type existe déjà"); database_statement_finalize(s); return FALSE; }
    database_statement_finalize(s);
    if (out_identifier != NULL)
    {
        int64_t identifier = 0;
        s = database_statement_prepare(dao->database,
            "SELECT last_insert_rowid();");
        if (s == NULL ||
            database_statement_step(s) != DATABASE_STATEMENT_STEP_ROW ||
            !database_statement_column_int64(s, 0, &identifier))
        {
            set_db_error(dao, error, RELATION_TYPE_DAO_ERROR_READ,
                "Identifiant du type indisponible");
            database_statement_finalize(s);
            return FALSE;
        }
        *out_identifier = (gint64) identifier;
        database_statement_finalize(s);
    }
    return TRUE;
}

gboolean relation_type_dao_update_label(RelationTypeDao *dao, gint64 id,
    const char *label, const char *key, GError **error)
{
    DatabaseStatement *s = database_statement_prepare(dao->database,
        "UPDATE relation_types SET label=?,normalized_key=? WHERE id=?;");
    if (s == NULL || !database_statement_bind_text(s,1,label) ||
        !database_statement_bind_text(s,2,key) ||
        !database_statement_bind_int64(s,3,id) ||
        database_statement_step(s) != DATABASE_STATEMENT_STEP_DONE)
    { set_db_error(dao,error,RELATION_TYPE_DAO_ERROR_CONSTRAINT,
        "Renommage du type impossible"); database_statement_finalize(s); return FALSE; }
    database_statement_finalize(s); return TRUE;
}

gboolean relation_type_dao_count_relations(RelationTypeDao *dao, gint64 id,
    guint64 *count, GError **error)
{
    DatabaseStatement *s = database_statement_prepare(dao->database,
        "SELECT COUNT(*) FROM relations WHERE relation_type_id=?;");
    int64_t value = 0;
    if (s == NULL || !database_statement_bind_int64(s,1,id) ||
        database_statement_step(s) != DATABASE_STATEMENT_STEP_ROW ||
        !database_statement_column_int64(s,0,&value))
    { set_db_error(dao,error,RELATION_TYPE_DAO_ERROR_EXECUTE,
        "Comptage des relations impossible"); database_statement_finalize(s); return FALSE; }
    *count = (guint64) value; database_statement_finalize(s); return TRUE;
}

gboolean relation_type_dao_delete_unused(RelationTypeDao *dao, gint64 id,
    GError **error)
{
    DatabaseStatement *s = database_statement_prepare(dao->database,
        "DELETE FROM relation_types WHERE id=? AND is_system=0 AND NOT EXISTS"
        "(SELECT 1 FROM relations WHERE relation_type_id=?);");
    if (s == NULL || !database_statement_bind_int64(s,1,id) ||
        !database_statement_bind_int64(s,2,id) ||
        database_statement_step(s) != DATABASE_STATEMENT_STEP_DONE)
    { set_db_error(dao,error,RELATION_TYPE_DAO_ERROR_CONSTRAINT,
        "Suppression du type refusée"); database_statement_finalize(s); return FALSE; }
    database_statement_finalize(s); return TRUE;
}
