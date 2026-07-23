/******************************************************************************
 * @file relation_type_service.h
 * @brief Gestion métier des types canoniques de relations.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_RELATION_TYPE_SERVICE_H
#define LABFY_INVESTIGATION_RELATION_TYPE_SERVICE_H
#include "database/database.h"
#include "models/relation_type.h"
#include <glib.h>
G_BEGIN_DECLS
typedef struct RelationTypeService RelationTypeService;
typedef enum
{
    RELATION_TYPE_SERVICE_ERROR_INVALID_ARGUMENT,
    RELATION_TYPE_SERVICE_ERROR_DUPLICATE,
    RELATION_TYPE_SERVICE_ERROR_IN_USE,
    RELATION_TYPE_SERVICE_ERROR_CONFLICT,
    RELATION_TYPE_SERVICE_ERROR_DATABASE
} RelationTypeServiceError;
#define RELATION_TYPE_SERVICE_ERROR relation_type_service_error_quark()
GQuark relation_type_service_error_quark(void);
RelationTypeService *relation_type_service_new(Database *database,
    GError **error);
void relation_type_service_free(RelationTypeService *service);
GPtrArray *relation_type_service_list_all(RelationTypeService *service,
    GError **error);
gboolean relation_type_service_create_custom(RelationTypeService *service,
    const char *label, const char *description, gint64 *identifier,
    GError **error);
gboolean relation_type_service_rename(RelationTypeService *service,
    gint64 identifier, const char *label, GError **error);
gboolean relation_type_service_count_usage(RelationTypeService *service,
    gint64 identifier, guint64 *count, GError **error);
gboolean relation_type_service_delete(RelationTypeService *service,
    gint64 identifier, GError **error);
gboolean relation_type_service_merge(RelationTypeService *service,
    gint64 source_identifier, gint64 target_identifier, GError **error);
G_END_DECLS
#endif
