/******************************************************************************
 * @file entity_type.c
 * @brief Implémentation du modèle représentant un type d'entité.
 ******************************************************************************/

#include "models/entity_type.h"

#include <glib.h>

/**
 * @struct EntityType
 * @brief Représentation interne d'un type d'entité.
 */
struct EntityType
{
    gint64 identifier;
    char *code;
    char *label;
    char *description;
};

/**
 * @brief Copie une chaîne après suppression des espaces périphériques.
 *
 * @param value Chaîne à copier.
 *
 * @return Nouvelle chaîne normalisée, ou NULL lorsque value vaut NULL.
 */
static char *entity_type_duplicate_trimmed(
    const char *value
)
{
    char *copy =
        NULL;

    if (value == NULL)
    {
        return NULL;
    }

    copy =
        g_strdup(
            value
        );

    g_strstrip(
        copy
    );

    return copy;
}

/**
 * @brief Vérifie qu'une chaîne obligatoire contient du texte.
 *
 * @param value Chaîne déjà normalisée.
 *
 * @return TRUE lorsque la chaîne contient au moins un caractère.
 */
static gboolean entity_type_required_value_is_valid(
    const char *value
)
{
    return value != NULL &&
           value[0] != '\0';
}

GQuark entity_type_error_quark(void)
{
    return g_quark_from_static_string(
        "entity-type-error-quark"
    );
}

EntityType *entity_type_new(
    gint64 identifier,
    const char *code,
    const char *label,
    const char *description,
    GError **error
)
{
    EntityType *entity_type =
        NULL;

    char *normalized_code =
        NULL;

    char *normalized_label =
        NULL;

    char *normalized_description =
        NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (identifier <= 0)
    {
        g_set_error_literal(
            error,
            ENTITY_TYPE_ERROR,
            ENTITY_TYPE_ERROR_INVALID_IDENTIFIER,
            "L'identifiant du type d'entité doit être strictement positif."
        );

        return NULL;
    }

    normalized_code =
        entity_type_duplicate_trimmed(
            code
        );

    if (!entity_type_required_value_is_valid(
            normalized_code
        ))
    {
        g_set_error_literal(
            error,
            ENTITY_TYPE_ERROR,
            ENTITY_TYPE_ERROR_INVALID_CODE,
            "Le code du type d'entité est obligatoire."
        );

        g_free(
            normalized_code
        );

        return NULL;
    }

    normalized_label =
        entity_type_duplicate_trimmed(
            label
        );

    if (!entity_type_required_value_is_valid(
            normalized_label
        ))
    {
        g_set_error_literal(
            error,
            ENTITY_TYPE_ERROR,
            ENTITY_TYPE_ERROR_INVALID_LABEL,
            "Le libellé du type d'entité est obligatoire."
        );

        g_free(
            normalized_label
        );

        g_free(
            normalized_code
        );

        return NULL;
    }

    normalized_description =
        entity_type_duplicate_trimmed(
            description
        );

    /*
     * Une description vide ou composée uniquement d'espaces
     * est représentée par NULL.
     */
    if (normalized_description != NULL &&
        normalized_description[0] == '\0')
    {
        g_free(
            normalized_description
        );

        normalized_description =
            NULL;
    }

    entity_type =
        g_new0(
            EntityType,
            1
        );

    entity_type->identifier =
        identifier;

    entity_type->code =
        normalized_code;

    entity_type->label =
        normalized_label;

    entity_type->description =
        normalized_description;

    return entity_type;
}

void entity_type_free(
    EntityType *entity_type
)
{
    if (entity_type == NULL)
    {
        return;
    }

    g_free(
        entity_type->description
    );

    g_free(
        entity_type->label
    );

    g_free(
        entity_type->code
    );

    g_free(
        entity_type
    );
}

gint64 entity_type_get_identifier(
    const EntityType *entity_type
)
{
    if (entity_type == NULL)
    {
        return 0;
    }

    return entity_type->identifier;
}

const char *entity_type_get_code(
    const EntityType *entity_type
)
{
    if (entity_type == NULL)
    {
        return NULL;
    }

    return entity_type->code;
}

const char *entity_type_get_label(
    const EntityType *entity_type
)
{
    if (entity_type == NULL)
    {
        return NULL;
    }

    return entity_type->label;
}

const char *entity_type_get_description(
    const EntityType *entity_type
)
{
    if (entity_type == NULL)
    {
        return NULL;
    }

    return entity_type->description;
}
