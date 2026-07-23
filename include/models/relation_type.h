/******************************************************************************
 * @file relation_type.h
 * @brief Type canonique d'une relation.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_RELATION_TYPE_H
#define LABFY_INVESTIGATION_RELATION_TYPE_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct RelationType RelationType;

typedef enum
{
    RELATION_TYPE_ERROR_INVALID_ARGUMENT,
    RELATION_TYPE_ERROR_INVALID_IDENTIFIER,
    RELATION_TYPE_ERROR_INVALID_LABEL,
    RELATION_TYPE_ERROR_INVALID_NORMALIZED_KEY
} RelationTypeError;

#define RELATION_TYPE_ERROR relation_type_error_quark()
GQuark relation_type_error_quark(void);

RelationType *relation_type_new(gint64 identifier, const char *code,
    const char *label, const char *normalized_key, const char *description,
    gboolean system_type, GError **error);
void relation_type_free(RelationType *relation_type);
gint64 relation_type_get_identifier(const RelationType *relation_type);
const char *relation_type_get_code(const RelationType *relation_type);
const char *relation_type_get_label(const RelationType *relation_type);
const char *relation_type_get_normalized_key(const RelationType *relation_type);
const char *relation_type_get_description(const RelationType *relation_type);
gboolean relation_type_is_system(const RelationType *relation_type);

G_END_DECLS
#endif
