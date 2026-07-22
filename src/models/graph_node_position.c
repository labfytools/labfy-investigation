/******************************************************************************
 * @file graph_node_position.c
 * @brief Implémentation du modèle de position persistée d'un nœud.
 ******************************************************************************/

#include "models/graph_node_position.h"

#include <math.h>
#include <string.h>

/**
 * @struct GraphNodePosition
 * @brief Représentation privée d'une position de nœud.
 */
struct GraphNodePosition
{
    char *node_identifier;

    double x;
    double y;

    char *updated_at;
};

/**
 * @brief Copie et nettoie une chaîne.
 */
static char *graph_node_position_duplicate_trimmed(
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

    if (copy == NULL)
    {
        return NULL;
    }

    g_strstrip(
        copy
    );

    return copy;
}

/**
 * @brief Vérifie une date UTC stricte YYYY-MM-DDTHH:MM:SSZ.
 */
static gboolean graph_node_position_timestamp_is_valid(
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

GQuark graph_node_position_error_quark(void)
{
    return g_quark_from_static_string(
        "graph-node-position-error-quark"
    );
}

GraphNodePosition *graph_node_position_new(
    const char *node_identifier,
    double x,
    double y,
    const char *updated_at,
    GError **error
)
{
    GraphNodePosition *position =
        NULL;

    char *node_identifier_copy =
        NULL;

    char *updated_at_copy =
        NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    node_identifier_copy =
        graph_node_position_duplicate_trimmed(
            node_identifier
        );

    if (node_identifier_copy == NULL ||
        node_identifier_copy[0] == '\0' ||
        !g_uuid_string_is_valid(
            node_identifier_copy
        ))
    {
        g_set_error_literal(
            error,
            GRAPH_NODE_POSITION_ERROR,
            GRAPH_NODE_POSITION_ERROR_INVALID_IDENTIFIER,
            "L'identifiant du nœud n'est pas un UUID valide."
        );

        goto cleanup;
    }

    if (!isfinite(x) ||
        !isfinite(y))
    {
        g_set_error_literal(
            error,
            GRAPH_NODE_POSITION_ERROR,
            GRAPH_NODE_POSITION_ERROR_INVALID_COORDINATE,
            "Les coordonnées du nœud doivent être des nombres finis."
        );

        goto cleanup;
    }

    updated_at_copy =
        graph_node_position_duplicate_trimmed(
            updated_at
        );

    if (!graph_node_position_timestamp_is_valid(
            updated_at_copy
        ))
    {
        g_set_error_literal(
            error,
            GRAPH_NODE_POSITION_ERROR,
            GRAPH_NODE_POSITION_ERROR_INVALID_DATE,
            "La date de modification de la position est invalide."
        );

        goto cleanup;
    }

    position =
        g_try_new0(
            GraphNodePosition,
            1
        );

    if (position == NULL)
    {
        g_set_error_literal(
            error,
            GRAPH_NODE_POSITION_ERROR,
            GRAPH_NODE_POSITION_ERROR_INVALID_ARGUMENT,
            "Impossible d'allouer la position du nœud."
        );

        goto cleanup;
    }

    position->node_identifier =
        node_identifier_copy;

    position->x =
        x;

    position->y =
        y;

    position->updated_at =
        updated_at_copy;

    node_identifier_copy =
        NULL;

    updated_at_copy =
        NULL;

cleanup:

    g_free(
        updated_at_copy
    );

    g_free(
        node_identifier_copy
    );

    return position;
}

void graph_node_position_free(
    GraphNodePosition *position
)
{
    if (position == NULL)
    {
        return;
    }

    g_free(
        position->updated_at
    );

    g_free(
        position->node_identifier
    );

    g_free(
        position
    );
}

const char *graph_node_position_get_node_identifier(
    const GraphNodePosition *position
)
{
    return position != NULL
        ? position->node_identifier
        : NULL;
}

double graph_node_position_get_x(
    const GraphNodePosition *position
)
{
    return position != NULL
        ? position->x
        : 0.0;
}

double graph_node_position_get_y(
    const GraphNodePosition *position
)
{
    return position != NULL
        ? position->y
        : 0.0;
}

const char *graph_node_position_get_updated_at(
    const GraphNodePosition *position
)
{
    return position != NULL
        ? position->updated_at
        : NULL;
}
