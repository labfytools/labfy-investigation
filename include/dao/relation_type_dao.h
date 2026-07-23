/******************************************************************************
 * @file relation_type_dao.h
 * @brief Persistance du référentiel des types de relations.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_RELATION_TYPE_DAO_H
#define LABFY_INVESTIGATION_RELATION_TYPE_DAO_H

#include "database/database.h"
#include "models/relation_type.h"
#include <glib.h>

G_BEGIN_DECLS
typedef struct RelationTypeDao RelationTypeDao;
typedef enum
{
    RELATION_TYPE_DAO_ERROR_INVALID_ARGUMENT,
    RELATION_TYPE_DAO_ERROR_PREPARE,
    RELATION_TYPE_DAO_ERROR_BIND,
    RELATION_TYPE_DAO_ERROR_EXECUTE,
    RELATION_TYPE_DAO_ERROR_CONSTRAINT,
    RELATION_TYPE_DAO_ERROR_READ,
    RELATION_TYPE_DAO_ERROR_MODEL
} RelationTypeDaoError;
#define RELATION_TYPE_DAO_ERROR relation_type_dao_error_quark()
GQuark relation_type_dao_error_quark(void);
RelationTypeDao *relation_type_dao_new(Database *database, GError **error);
void relation_type_dao_free(RelationTypeDao *dao);
GPtrArray *relation_type_dao_list_all(RelationTypeDao *dao, GError **error);
RelationType *relation_type_dao_find_by_identifier(RelationTypeDao *dao,
    gint64 identifier, GError **error);
RelationType *relation_type_dao_find_by_code(RelationTypeDao *dao,
    const char *code, GError **error);
RelationType *relation_type_dao_find_by_normalized_key(RelationTypeDao *dao,
    const char *key, GError **error);
gboolean relation_type_dao_insert_custom(RelationTypeDao *dao,
    const char *label, const char *normalized_key, const char *description,
    gint64 *out_identifier, GError **error);
gboolean relation_type_dao_update_label(RelationTypeDao *dao,
    gint64 identifier, const char *label, const char *normalized_key,
    GError **error);
gboolean relation_type_dao_count_relations(RelationTypeDao *dao,
    gint64 identifier, guint64 *count, GError **error);
gboolean relation_type_dao_delete_unused(RelationTypeDao *dao,
    gint64 identifier, GError **error);
G_END_DECLS
#endif
