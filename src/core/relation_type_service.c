#include "core/relation_type_service.h"
#include "core/relation_type_normalizer.h"
#include "dao/relation_type_dao.h"
#include "database/error.h"
#include "database/statement.h"
#include "database/transaction.h"

struct RelationTypeService
{
    Database *database;
    RelationTypeDao *dao;
};

static void set_error(GError **error, RelationTypeServiceError code,
    const char *message)
{
    if (error != NULL && *error == NULL)
        g_set_error_literal(error, RELATION_TYPE_SERVICE_ERROR, code, message);
}
GQuark relation_type_service_error_quark(void)
{ return g_quark_from_static_string("relation-type-service-error-quark"); }
RelationTypeService *relation_type_service_new(Database *database,
    GError **error)
{
    RelationTypeService *service = NULL;
    if (database == NULL) { set_error(error,
        RELATION_TYPE_SERVICE_ERROR_INVALID_ARGUMENT,
        "La base est obligatoire."); return NULL; }
    service = g_new0(RelationTypeService, 1);
    service->database = database;
    service->dao = relation_type_dao_new(database, error);
    if (service->dao == NULL) { g_free(service); return NULL; }
    return service;
}
void relation_type_service_free(RelationTypeService *service)
{
    if (service == NULL) return;
    relation_type_dao_free(service->dao); g_free(service);
}
GPtrArray *relation_type_service_list_all(RelationTypeService *service,
    GError **error)
{ return service != NULL ? relation_type_dao_list_all(service->dao, error) : NULL; }

gboolean relation_type_service_create_custom(RelationTypeService *service,
    const char *label, const char *description, gint64 *identifier,
    GError **error)
{
    char *key = relation_type_normalize_key(label);
    RelationType *existing = NULL; gboolean result = FALSE;
    if (service == NULL || key == NULL) { set_error(error,
        RELATION_TYPE_SERVICE_ERROR_INVALID_ARGUMENT,
        "Le libellé du type est invalide."); goto cleanup; }
    existing = relation_type_dao_find_by_normalized_key(service->dao,key,error);
    if (existing != NULL) { set_error(error, RELATION_TYPE_SERVICE_ERROR_DUPLICATE,
        "Un type de relation équivalent existe déjà."); goto cleanup; }
    if (error != NULL && *error != NULL) goto cleanup;
    result = relation_type_dao_insert_custom(service->dao,label,key,description,
        identifier,error);
cleanup:
    relation_type_free(existing); g_free(key); return result;
}

gboolean relation_type_service_rename(RelationTypeService *service,
    gint64 identifier, const char *label, GError **error)
{
    char *key = relation_type_normalize_key(label);
    RelationType *existing = NULL; gboolean result = FALSE;
    if (service == NULL || identifier <= 0 || key == NULL) { set_error(error,
        RELATION_TYPE_SERVICE_ERROR_INVALID_ARGUMENT,
        "Le renommage demandé est invalide."); goto cleanup; }
    existing = relation_type_dao_find_by_normalized_key(service->dao,key,error);
    if (existing != NULL &&
        relation_type_get_identifier(existing) != identifier)
    { set_error(error, RELATION_TYPE_SERVICE_ERROR_DUPLICATE,
        "Un type de relation équivalent existe déjà."); goto cleanup; }
    if (error != NULL && *error != NULL) goto cleanup;
    result = relation_type_dao_update_label(service->dao,identifier,label,key,error);
cleanup:
    relation_type_free(existing); g_free(key); return result;
}

gboolean relation_type_service_count_usage(RelationTypeService *service,
    gint64 identifier, guint64 *count, GError **error)
{
    if (service == NULL || identifier <= 0 || count == NULL) { set_error(error,
        RELATION_TYPE_SERVICE_ERROR_INVALID_ARGUMENT,
        "Le comptage demandé est invalide."); return FALSE; }
    return relation_type_dao_count_relations(service->dao,identifier,count,error);
}

gboolean relation_type_service_delete(RelationTypeService *service,
    gint64 identifier, GError **error)
{
    guint64 count = 0;
    if (!relation_type_service_count_usage(service,identifier,&count,error))
        return FALSE;
    if (count > 0U) { set_error(error, RELATION_TYPE_SERVICE_ERROR_IN_USE,
        "Ce type est utilisé par des relations et ne peut pas être supprimé.");
        return FALSE; }
    return relation_type_dao_delete_unused(service->dao,identifier,error);
}

gboolean relation_type_service_merge(RelationTypeService *service,
    gint64 source_identifier, gint64 target_identifier, GError **error)
{
    DatabaseStatement *statement = NULL; gboolean active = FALSE;
    gboolean success = FALSE;
    if (service == NULL || source_identifier <= 0 || target_identifier <= 0 ||
        source_identifier == target_identifier)
    { set_error(error, RELATION_TYPE_SERVICE_ERROR_INVALID_ARGUMENT,
        "La fusion demandée est invalide."); return FALSE; }
    statement = database_statement_prepare(service->database,
        "SELECT 1 FROM relations source JOIN relations target ON "
        "target.entite_source_id=source.entite_source_id AND "
        "target.entite_cible_id=source.entite_cible_id AND "
        "target.relation_type_id=? WHERE source.relation_type_id=? LIMIT 1;");
    if (statement == NULL ||
        !database_statement_bind_int64(statement,1,target_identifier) ||
        !database_statement_bind_int64(statement,2,source_identifier))
        goto cleanup;
    if (database_statement_step(statement) == DATABASE_STATEMENT_STEP_ROW)
    { set_error(error, RELATION_TYPE_SERVICE_ERROR_CONFLICT,
        "Fusion refusée : des relations source et cible entreraient en conflit.");
        goto cleanup; }
    database_statement_finalize(statement); statement = NULL;
    if (!database_transaction_begin(service->database)) goto cleanup;
    active = TRUE;
    statement = database_statement_prepare(service->database,
        "UPDATE relations SET relation_type_id=?,type_relation=("
        "SELECT COALESCE(code,label) FROM relation_types WHERE id=?) "
        "WHERE relation_type_id=?;");
    if (statement == NULL ||
        !database_statement_bind_int64(statement,1,target_identifier) ||
        !database_statement_bind_int64(statement,2,target_identifier) ||
        !database_statement_bind_int64(statement,3,source_identifier) ||
        database_statement_step(statement) != DATABASE_STATEMENT_STEP_DONE)
        goto cleanup;
    database_statement_finalize(statement); statement = NULL;
    if (!relation_type_dao_delete_unused(service->dao,source_identifier,error))
        goto cleanup;
    if (!database_transaction_commit(service->database)) goto cleanup;
    active = FALSE; success = TRUE;
cleanup:
    database_statement_finalize(statement);
    if (!success && active) (void) database_transaction_rollback(service->database);
    if (!success && error != NULL && *error == NULL)
        g_set_error(error, RELATION_TYPE_SERVICE_ERROR,
            RELATION_TYPE_SERVICE_ERROR_DATABASE, "Fusion impossible : %s",
            database_error_get_message(service->database) != NULL
                ? database_error_get_message(service->database) : "erreur SQLite");
    return success;
}
