#include "models/relation_type.h"
#include "core/relation_type_normalizer.h"

struct RelationType
{
    gint64 identifier;
    char *code;
    char *label;
    char *normalized_key;
    char *description;
    gboolean system_type;
};

static char *relation_type_optional(const char *value)
{
    char *copy = value != NULL ? g_strdup(value) : NULL;
    if (copy != NULL) g_strstrip(copy);
    if (copy != NULL && copy[0] == '\0') g_clear_pointer(&copy, g_free);
    return copy;
}

GQuark relation_type_error_quark(void)
{
    return g_quark_from_static_string("relation-type-error-quark");
}

RelationType *relation_type_new(gint64 identifier, const char *code,
    const char *label, const char *normalized_key, const char *description,
    gboolean system_type, GError **error)
{
    RelationType *type = NULL;
    char *label_copy = NULL;
    char *expected_key = NULL;
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);
    if (identifier <= 0)
    {
        g_set_error_literal(error, RELATION_TYPE_ERROR,
            RELATION_TYPE_ERROR_INVALID_IDENTIFIER,
            "L'identifiant du type de relation est invalide.");
        return NULL;
    }
    label_copy = relation_type_optional(label);
    expected_key = relation_type_normalize_key(label_copy);
    if (label_copy == NULL || expected_key == NULL)
    {
        g_set_error_literal(error, RELATION_TYPE_ERROR,
            RELATION_TYPE_ERROR_INVALID_LABEL,
            "Le libellé du type de relation est obligatoire.");
        goto cleanup;
    }
    if (normalized_key == NULL || g_strcmp0(expected_key, normalized_key) != 0)
    {
        g_set_error_literal(error, RELATION_TYPE_ERROR,
            RELATION_TYPE_ERROR_INVALID_NORMALIZED_KEY,
            "La clé normalisée du type de relation est incohérente.");
        goto cleanup;
    }
    type = g_new0(RelationType, 1);
    type->identifier = identifier;
    type->code = relation_type_optional(code);
    type->label = label_copy;
    type->normalized_key = g_strdup(normalized_key);
    type->description = relation_type_optional(description);
    type->system_type = system_type;
    label_copy = NULL;
cleanup:
    g_free(expected_key);
    g_free(label_copy);
    return type;
}

void relation_type_free(RelationType *type)
{
    if (type == NULL) return;
    g_free(type->description); g_free(type->normalized_key);
    g_free(type->label); g_free(type->code); g_free(type);
}
gint64 relation_type_get_identifier(const RelationType *type)
{ return type != NULL ? type->identifier : 0; }
const char *relation_type_get_code(const RelationType *type)
{ return type != NULL ? type->code : NULL; }
const char *relation_type_get_label(const RelationType *type)
{ return type != NULL ? type->label : NULL; }
const char *relation_type_get_normalized_key(const RelationType *type)
{ return type != NULL ? type->normalized_key : NULL; }
const char *relation_type_get_description(const RelationType *type)
{ return type != NULL ? type->description : NULL; }
gboolean relation_type_is_system(const RelationType *type)
{ return type != NULL && type->system_type; }
