/******************************************************************************
 * @file osint_selection_context.c
 * @brief Implémentation du contexte métier OSINT.
 ******************************************************************************/

#include "models/osint_selection_context.h"

#include <gio/gio.h>

struct OsintSelectionContext
{
    OsintSelectionContextKind kind;
    char *identifier;
    char *type;
    char *value;
    char *source_value;
    char *target_value;
};

/**
 * @brief Alloue et copie les champs communs d'un contexte.
 */
static OsintSelectionContext *osint_selection_context_new(
    OsintSelectionContextKind kind,
    const char *identifier,
    const char *type,
    const char *value,
    GError **error
)
{
    OsintSelectionContext *context = NULL;

    if ((error != NULL && *error != NULL) ||
        (kind != OSINT_SELECTION_CONTEXT_KIND_ENTITY &&
         kind != OSINT_SELECTION_CONTEXT_KIND_RELATION) ||
        identifier == NULL || !g_uuid_string_is_valid(identifier) ||
        type == NULL || type[0] == '\0' ||
        value == NULL || value[0] == '\0')
    {
        g_set_error_literal(
            error,
            G_IO_ERROR,
            G_IO_ERROR_INVALID_ARGUMENT,
            "Le contexte de sélection OSINT est invalide."
        );
        return NULL;
    }

    context = g_try_new0(OsintSelectionContext, 1);
    if (context != NULL)
    {
        context->kind = kind;
        context->identifier = g_strdup(identifier);
        context->type = g_strdup(type);
        context->value = g_strdup(value);
    }

    if (context == NULL || context->identifier == NULL ||
        context->type == NULL || context->value == NULL)
    {
        osint_selection_context_free(context);
        g_set_error_literal(
            error,
            G_IO_ERROR,
            G_IO_ERROR_NO_SPACE,
            "Impossible d'allouer le contexte de sélection OSINT."
        );
        return NULL;
    }

    return context;
}

OsintSelectionContext *osint_selection_context_new_entity(
    const EntityRecord *entity_record,
    GError **error
)
{
    return osint_selection_context_new(
        OSINT_SELECTION_CONTEXT_KIND_ENTITY,
        entity_record_get_identifier(entity_record),
        entity_record_get_type_identifier(entity_record),
        entity_record_get_value(entity_record),
        error
    );
}

OsintSelectionContext *osint_selection_context_new_relation(
    const RelationRecord *relation_record,
    const EntityRecord *source_entity,
    const EntityRecord *target_entity,
    GError **error
)
{
    const char *label = relation_record_get_label(relation_record);
    OsintSelectionContext *context = osint_selection_context_new(
        OSINT_SELECTION_CONTEXT_KIND_RELATION,
        relation_record_get_identifier(relation_record),
        relation_record_get_relation_type(relation_record),
        label != NULL && label[0] != '\0'
            ? label
            : relation_record_get_relation_type(relation_record),
        error
    );

    if (context == NULL || source_entity == NULL || target_entity == NULL)
    {
        osint_selection_context_free(context);
        if (context != NULL)
        {
            g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                "Les extrémités de la relation OSINT sont invalides.");
        }
        return NULL;
    }

    context->source_value = g_strdup(entity_record_get_value(source_entity));
    context->target_value = g_strdup(entity_record_get_value(target_entity));
    if (context->source_value == NULL || context->target_value == NULL)
    {
        osint_selection_context_free(context);
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NO_SPACE,
            "Impossible de copier les extrémités de la relation OSINT.");
        return NULL;
    }

    return context;
}

void osint_selection_context_free(OsintSelectionContext *context)
{
    if (context == NULL) return;
    g_free(context->target_value);
    g_free(context->source_value);
    g_free(context->value);
    g_free(context->type);
    g_free(context->identifier);
    g_free(context);
}

OsintSelectionContextKind osint_selection_context_get_kind(const OsintSelectionContext *context)
{ return context != NULL ? context->kind : OSINT_SELECTION_CONTEXT_KIND_UNKNOWN; }
const char *osint_selection_context_get_identifier(const OsintSelectionContext *context)
{ return context != NULL ? context->identifier : NULL; }
const char *osint_selection_context_get_type(const OsintSelectionContext *context)
{ return context != NULL ? context->type : NULL; }
const char *osint_selection_context_get_value(const OsintSelectionContext *context)
{ return context != NULL ? context->value : NULL; }
const char *osint_selection_context_get_source_value(const OsintSelectionContext *context)
{ return context != NULL ? context->source_value : NULL; }
const char *osint_selection_context_get_target_value(const OsintSelectionContext *context)
{ return context != NULL ? context->target_value : NULL; }
