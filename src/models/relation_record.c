/******************************************************************************
 * @file relation_record.c
 * @brief Implémentation du modèle représentant une relation persistée.
 ******************************************************************************/

#include "models/relation_record.h"

#include <string.h>

/**
 * @brief Représentation privée d'une relation.
 */
struct RelationRecord
{
    char *identifier;
    char *source_entity_identifier;
    char *target_entity_identifier;
    char *relation_type;
    char *label;
    char *justification;

    gint confidence;

    char *created_at;
    char *updated_at;

    RelationStatus status;
};

/**
 * @brief Copie et nettoie une chaîne.
 */
static char *relation_record_duplicate_trimmed(
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
static char *relation_record_duplicate_optional(
    const char *value
)
{
    char *copy =
        NULL;

    copy =
        relation_record_duplicate_trimmed(
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
 * @brief Vérifie qu'une chaîne obligatoire est présente.
 */
static gboolean relation_record_required_value_is_valid(
    const char *value
)
{
    return value != NULL &&
           value[0] != '\0';
}

/**
 * @brief Vérifie une date UTC stricte au format YYYY-MM-DDTHH:MM:SSZ.
 */
static gboolean relation_record_timestamp_is_valid(
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
static gboolean relation_record_status_is_valid(
    RelationStatus status
)
{
    return status == RELATION_STATUS_ACTIVE ||
           status == RELATION_STATUS_ARCHIVED ||
           status == RELATION_STATUS_DELETED ||
           status == RELATION_STATUS_DISPUTED;
}

GQuark relation_record_error_quark(void)
{
    return g_quark_from_static_string(
        "relation-record-error-quark"
    );
}

RelationRecord *relation_record_new(
    const char *identifier,
    const char *source_entity_identifier,
    const char *target_entity_identifier,
    const char *relation_type,
    const char *label,
    const char *justification,
    gint confidence,
    const char *created_at,
    const char *updated_at,
    RelationStatus status,
    GError **error
)
{
    RelationRecord *relation_record =
        NULL;

    char *identifier_copy =
        NULL;

    char *source_entity_identifier_copy =
        NULL;

    char *target_entity_identifier_copy =
        NULL;

    char *relation_type_copy =
        NULL;

    char *label_copy =
        NULL;

    char *justification_copy =
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
        relation_record_duplicate_trimmed(
            identifier
        );

    if (!relation_record_required_value_is_valid(
            identifier_copy
        ) ||
        !g_uuid_string_is_valid(
            identifier_copy
        ))
    {
        g_set_error_literal(
            error,
            RELATION_RECORD_ERROR,
            RELATION_RECORD_ERROR_INVALID_IDENTIFIER,
            "L'identifiant de la relation n'est pas un UUID valide."
        );

        goto cleanup;
    }

    source_entity_identifier_copy =
        relation_record_duplicate_trimmed(
            source_entity_identifier
        );

    if (!relation_record_required_value_is_valid(
            source_entity_identifier_copy
        ) ||
        !g_uuid_string_is_valid(
            source_entity_identifier_copy
        ))
    {
        g_set_error_literal(
            error,
            RELATION_RECORD_ERROR,
            RELATION_RECORD_ERROR_INVALID_SOURCE_IDENTIFIER,
            "L'identifiant de l'entité source n'est pas un UUID valide."
        );

        goto cleanup;
    }

    target_entity_identifier_copy =
        relation_record_duplicate_trimmed(
            target_entity_identifier
        );

    if (!relation_record_required_value_is_valid(
            target_entity_identifier_copy
        ) ||
        !g_uuid_string_is_valid(
            target_entity_identifier_copy
        ))
    {
        g_set_error_literal(
            error,
            RELATION_RECORD_ERROR,
            RELATION_RECORD_ERROR_INVALID_TARGET_IDENTIFIER,
            "L'identifiant de l'entité cible n'est pas un UUID valide."
        );

        goto cleanup;
    }

    if (g_strcmp0(
            source_entity_identifier_copy,
            target_entity_identifier_copy
        ) == 0)
    {
        g_set_error_literal(
            error,
            RELATION_RECORD_ERROR,
            RELATION_RECORD_ERROR_IDENTICAL_ENTITIES,
            "Une relation ne peut pas relier une entité à elle-même."
        );

        goto cleanup;
    }

    relation_type_copy =
        relation_record_duplicate_trimmed(
            relation_type
        );

    if (!relation_record_required_value_is_valid(
            relation_type_copy
        ))
    {
        g_set_error_literal(
            error,
            RELATION_RECORD_ERROR,
            RELATION_RECORD_ERROR_INVALID_TYPE,
            "Le type de relation est obligatoire."
        );

        goto cleanup;
    }

    if (confidence < 0 ||
        confidence > 100)
    {
        g_set_error_literal(
            error,
            RELATION_RECORD_ERROR,
            RELATION_RECORD_ERROR_INVALID_CONFIDENCE,
            "Le niveau de confiance doit être compris entre 0 et 100."
        );

        goto cleanup;
    }

    created_at_copy =
        relation_record_duplicate_trimmed(
            created_at
        );

    if (!relation_record_timestamp_is_valid(
            created_at_copy
        ))
    {
        g_set_error_literal(
            error,
            RELATION_RECORD_ERROR,
            RELATION_RECORD_ERROR_INVALID_DATE,
            "La date de création de la relation est invalide."
        );

        goto cleanup;
    }

    updated_at_copy =
        relation_record_duplicate_trimmed(
            updated_at
        );

    if (!relation_record_timestamp_is_valid(
            updated_at_copy
        ))
    {
        g_set_error_literal(
            error,
            RELATION_RECORD_ERROR,
            RELATION_RECORD_ERROR_INVALID_DATE,
            "La date de modification de la relation est invalide."
        );

        goto cleanup;
    }

    if (!relation_record_status_is_valid(
            status
        ))
    {
        g_set_error_literal(
            error,
            RELATION_RECORD_ERROR,
            RELATION_RECORD_ERROR_INVALID_STATUS,
            "Le statut de la relation est invalide."
        );

        goto cleanup;
    }

    label_copy =
        relation_record_duplicate_optional(
            label
        );

    justification_copy =
        relation_record_duplicate_optional(
            justification
        );

    relation_record =
        g_new0(
            RelationRecord,
            1
        );

    relation_record->identifier =
        identifier_copy;

    relation_record->source_entity_identifier =
        source_entity_identifier_copy;

    relation_record->target_entity_identifier =
        target_entity_identifier_copy;

    relation_record->relation_type =
        relation_type_copy;

    relation_record->label =
        label_copy;

    relation_record->justification =
        justification_copy;

    relation_record->confidence =
        confidence;

    relation_record->created_at =
        created_at_copy;

    relation_record->updated_at =
        updated_at_copy;

    relation_record->status =
        status;

    identifier_copy = NULL;
    source_entity_identifier_copy = NULL;
    target_entity_identifier_copy = NULL;
    relation_type_copy = NULL;
    label_copy = NULL;
    justification_copy = NULL;
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
        justification_copy
    );

    g_free(
        label_copy
    );

    g_free(
        relation_type_copy
    );

    g_free(
        target_entity_identifier_copy
    );

    g_free(
        source_entity_identifier_copy
    );

    g_free(
        identifier_copy
    );

    return relation_record;
}

void relation_record_free(
    RelationRecord *relation_record
)
{
    if (relation_record == NULL)
    {
        return;
    }

    g_free(
        relation_record->updated_at
    );

    g_free(
        relation_record->created_at
    );

    g_free(
        relation_record->justification
    );

    g_free(
        relation_record->label
    );

    g_free(
        relation_record->relation_type
    );

    g_free(
        relation_record->target_entity_identifier
    );

    g_free(
        relation_record->source_entity_identifier
    );

    g_free(
        relation_record->identifier
    );

    g_free(
        relation_record
    );
}

const char *relation_record_get_identifier(
    const RelationRecord *relation_record
)
{
    return relation_record != NULL
        ? relation_record->identifier
        : NULL;
}

const char *relation_record_get_source_entity_identifier(
    const RelationRecord *relation_record
)
{
    return relation_record != NULL
        ? relation_record->source_entity_identifier
        : NULL;
}

const char *relation_record_get_target_entity_identifier(
    const RelationRecord *relation_record
)
{
    return relation_record != NULL
        ? relation_record->target_entity_identifier
        : NULL;
}

const char *relation_record_get_relation_type(
    const RelationRecord *relation_record
)
{
    return relation_record != NULL
        ? relation_record->relation_type
        : NULL;
}

const char *relation_record_get_label(
    const RelationRecord *relation_record
)
{
    return relation_record != NULL
        ? relation_record->label
        : NULL;
}

const char *relation_record_get_justification(
    const RelationRecord *relation_record
)
{
    return relation_record != NULL
        ? relation_record->justification
        : NULL;
}

gint relation_record_get_confidence(
    const RelationRecord *relation_record
)
{
    return relation_record != NULL
        ? relation_record->confidence
        : 0;
}

const char *relation_record_get_created_at(
    const RelationRecord *relation_record
)
{
    return relation_record != NULL
        ? relation_record->created_at
        : NULL;
}

const char *relation_record_get_updated_at(
    const RelationRecord *relation_record
)
{
    return relation_record != NULL
        ? relation_record->updated_at
        : NULL;
}

RelationStatus relation_record_get_status(
    const RelationRecord *relation_record
)
{
    return relation_record != NULL
        ? relation_record->status
        : RELATION_STATUS_UNKNOWN;
}
