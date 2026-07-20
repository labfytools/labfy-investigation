/******************************************************************************
 * @file entity_record.c
 * @brief Implémentation du modèle représentant une entité persistée.
 ******************************************************************************/

#include "models/entity_record.h"

#include <string.h>

/**
 * @brief Représentation privée d'une entité.
 */
struct EntityRecord
{
    char *identifier;
    char *type_identifier;
    char *value;
    char *label;
    char *description;

    gint confidence;

    char *created_at;
    char *updated_at;

    EntityStatus status;
};

/**
 * @brief Copie et nettoie une chaîne.
 */
static char *entity_record_duplicate_trimmed(
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
 * @brief Normalise un champ facultatif.
 *
 * Une chaîne vide après nettoyage devient NULL.
 */
static char *entity_record_duplicate_optional(
    const char *value
)
{
    char *copy =
        NULL;

    copy =
        entity_record_duplicate_trimmed(
            value
        );

    if (copy != NULL &&
        copy[0] == '\0')
    {
        g_free(
            copy
        );

        copy =
            NULL;
    }

    return copy;
}

/**
 * @brief Vérifie qu'une valeur obligatoire est présente.
 */
static gboolean entity_record_required_value_is_valid(
    const char *value
)
{
    return value != NULL &&
           value[0] != '\0';
}

/**
 * @brief Vérifie une date UTC stricte YYYY-MM-DDTHH:MM:SSZ.
 */
static gboolean entity_record_timestamp_is_valid(
    const char *timestamp
)
{
    GDateTime *date_time =
        NULL;

    char *normalized_timestamp =
        NULL;

    gboolean valid =
        FALSE;

    gsize character_index =
        0;

    if (timestamp == NULL ||
        strlen(timestamp) != 20U)
    {
        return FALSE;
    }

    if (timestamp[4] != '-' ||
        timestamp[7] != '-' ||
        timestamp[10] != 'T' ||
        timestamp[13] != ':' ||
        timestamp[16] != ':' ||
        timestamp[19] != 'Z')
    {
        return FALSE;
    }

    for (character_index = 0;
         character_index < 20U;
         character_index++)
    {
        if (character_index == 4U ||
            character_index == 7U ||
            character_index == 10U ||
            character_index == 13U ||
            character_index == 16U ||
            character_index == 19U)
        {
            continue;
        }

        if (!g_ascii_isdigit(
                timestamp[character_index]
            ))
        {
            return FALSE;
        }
    }

    date_time =
        g_date_time_new_from_iso8601(
            timestamp,
            NULL
        );

    if (date_time == NULL)
    {
        return FALSE;
    }

    normalized_timestamp =
        g_date_time_format(
            date_time,
            "%Y-%m-%dT%H:%M:%SZ"
        );

    valid =
        normalized_timestamp != NULL &&
        g_strcmp0(
            normalized_timestamp,
            timestamp
        ) == 0;

    g_free(
        normalized_timestamp
    );

    g_date_time_unref(
        date_time
    );

    return valid;
}

/**
 * @brief Vérifie qu'un statut peut être persisté.
 */
static gboolean entity_record_status_is_valid(
    EntityStatus status
)
{
    return status == ENTITY_STATUS_ACTIVE ||
           status == ENTITY_STATUS_ARCHIVED ||
           status == ENTITY_STATUS_DELETED;
}

GQuark entity_record_error_quark(void)
{
    return g_quark_from_static_string(
        "entity-record-error-quark"
    );
}

EntityRecord *entity_record_new(
    const char *identifier,
    const char *type_identifier,
    const char *value,
    const char *label,
    const char *description,
    gint confidence,
    const char *created_at,
    const char *updated_at,
    EntityStatus status,
    GError **error
)
{
    EntityRecord *entity_record =
        NULL;

    char *identifier_copy =
        NULL;

    char *type_identifier_copy =
        NULL;

    char *value_copy =
        NULL;

    char *label_copy =
        NULL;

    char *description_copy =
        NULL;

    char *created_at_copy =
        NULL;

    char *updated_at_copy =
        NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    identifier_copy =
        entity_record_duplicate_trimmed(
            identifier
        );

    if (!entity_record_required_value_is_valid(
            identifier_copy
        ) ||
        !g_uuid_string_is_valid(
            identifier_copy
        ))
    {
        g_set_error_literal(
            error,
            ENTITY_RECORD_ERROR,
            ENTITY_RECORD_ERROR_INVALID_IDENTIFIER,
            "L'identifiant de l'entité n'est pas un UUID valide."
        );

        goto cleanup;
    }

    type_identifier_copy =
        entity_record_duplicate_trimmed(
            type_identifier
        );

    if (!entity_record_required_value_is_valid(
            type_identifier_copy
        ))
    {
        g_set_error_literal(
            error,
            ENTITY_RECORD_ERROR,
            ENTITY_RECORD_ERROR_INVALID_TYPE,
            "Le type de l'entité est obligatoire."
        );

        goto cleanup;
    }

    value_copy =
        entity_record_duplicate_trimmed(
            value
        );

    if (!entity_record_required_value_is_valid(
            value_copy
        ))
    {
        g_set_error_literal(
            error,
            ENTITY_RECORD_ERROR,
            ENTITY_RECORD_ERROR_INVALID_VALUE,
            "La valeur de l'entité est obligatoire."
        );

        goto cleanup;
    }

    if (confidence < 0 ||
        confidence > 100)
    {
        g_set_error_literal(
            error,
            ENTITY_RECORD_ERROR,
            ENTITY_RECORD_ERROR_INVALID_CONFIDENCE,
            "Le niveau de confiance doit être compris entre 0 et 100."
        );

        goto cleanup;
    }

    created_at_copy =
        entity_record_duplicate_trimmed(
            created_at
        );

    if (!entity_record_timestamp_is_valid(
            created_at_copy
        ))
    {
        g_set_error_literal(
            error,
            ENTITY_RECORD_ERROR,
            ENTITY_RECORD_ERROR_INVALID_DATE,
            "La date de création de l'entité est invalide."
        );

        goto cleanup;
    }

    updated_at_copy =
        entity_record_duplicate_trimmed(
            updated_at
        );

    if (!entity_record_timestamp_is_valid(
            updated_at_copy
        ))
    {
        g_set_error_literal(
            error,
            ENTITY_RECORD_ERROR,
            ENTITY_RECORD_ERROR_INVALID_DATE,
            "La date de modification de l'entité est invalide."
        );

        goto cleanup;
    }

    if (!entity_record_status_is_valid(
            status
        ))
    {
        g_set_error_literal(
            error,
            ENTITY_RECORD_ERROR,
            ENTITY_RECORD_ERROR_INVALID_STATUS,
            "Le statut de l'entité est invalide."
        );

        goto cleanup;
    }

    label_copy =
        entity_record_duplicate_optional(
            label
        );

    description_copy =
        entity_record_duplicate_optional(
            description
        );

    entity_record =
        g_new0(
            EntityRecord,
            1
        );

    entity_record->identifier =
        identifier_copy;

    entity_record->type_identifier =
        type_identifier_copy;

    entity_record->value =
        value_copy;

    entity_record->label =
        label_copy;

    entity_record->description =
        description_copy;

    entity_record->confidence =
        confidence;

    entity_record->created_at =
        created_at_copy;

    entity_record->updated_at =
        updated_at_copy;

    entity_record->status =
        status;

    /*
     * La propriété a été transférée au modèle.
     */
    identifier_copy = NULL;
    type_identifier_copy = NULL;
    value_copy = NULL;
    label_copy = NULL;
    description_copy = NULL;
    created_at_copy = NULL;
    updated_at_copy = NULL;

cleanup:

    g_free(
        updated_at_copy
    );

    g_free(
        created_at_copy
    );

    g_free(
        description_copy
    );

    g_free(
        label_copy
    );

    g_free(
        value_copy
    );

    g_free(
        type_identifier_copy
    );

    g_free(
        identifier_copy
    );

    return entity_record;
}

void entity_record_free(
    EntityRecord *entity_record
)
{
    if (entity_record == NULL)
    {
        return;
    }

    g_free(
        entity_record->updated_at
    );

    g_free(
        entity_record->created_at
    );

    g_free(
        entity_record->description
    );

    g_free(
        entity_record->label
    );

    g_free(
        entity_record->value
    );

    g_free(
        entity_record->type_identifier
    );

    g_free(
        entity_record->identifier
    );

    g_free(
        entity_record
    );
}

const char *entity_record_get_identifier(
    const EntityRecord *entity_record
)
{
    return entity_record != NULL
        ? entity_record->identifier
        : NULL;
}

const char *entity_record_get_type_identifier(
    const EntityRecord *entity_record
)
{
    return entity_record != NULL
        ? entity_record->type_identifier
        : NULL;
}

const char *entity_record_get_value(
    const EntityRecord *entity_record
)
{
    return entity_record != NULL
        ? entity_record->value
        : NULL;
}

const char *entity_record_get_label(
    const EntityRecord *entity_record
)
{
    return entity_record != NULL
        ? entity_record->label
        : NULL;
}

const char *entity_record_get_description(
    const EntityRecord *entity_record
)
{
    return entity_record != NULL
        ? entity_record->description
        : NULL;
}

gint entity_record_get_confidence(
    const EntityRecord *entity_record
)
{
    return entity_record != NULL
        ? entity_record->confidence
        : 0;
}

const char *entity_record_get_created_at(
    const EntityRecord *entity_record
)
{
    return entity_record != NULL
        ? entity_record->created_at
        : NULL;
}

const char *entity_record_get_updated_at(
    const EntityRecord *entity_record
)
{
    return entity_record != NULL
        ? entity_record->updated_at
        : NULL;
}

EntityStatus entity_record_get_status(
    const EntityRecord *entity_record
)
{
    return entity_record != NULL
        ? entity_record->status
        : ENTITY_STATUS_UNKNOWN;
}
