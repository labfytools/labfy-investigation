/******************************************************************************
 * @file investigation_graph_layout.c
 * @brief Implémentation du modèle en mémoire de disposition du graphe.
 ******************************************************************************/

#include "models/investigation_graph_layout.h"

#include <math.h>

/**
 * @brief Coordonnées privées associées à une entité.
 */
typedef struct
{
    double x;
    double y;
} InvestigationGraphLayoutPosition;

/**
 * @struct InvestigationGraphLayout
 * @brief Représentation privée de la disposition.
 */
struct InvestigationGraphLayout
{
    GHashTable *positions_by_entity_identifier;
};

/**
 * @brief Enregistre une erreur littérale.
 */
static void investigation_graph_layout_set_error_literal(
    GError **error,
    InvestigationGraphLayoutError error_code,
    const char *error_message
)
{
    if (error == NULL)
    {
        return;
    }

    g_set_error_literal(
        error,
        INVESTIGATION_GRAPH_LAYOUT_ERROR,
        error_code,
        error_message
    );
}

/**
 * @brief Vérifie un identifiant d'entité.
 */
static gboolean investigation_graph_layout_identifier_is_valid(
    const char *entity_identifier
)
{
    return entity_identifier != NULL &&
           entity_identifier[0] != '\0' &&
           g_uuid_string_is_valid(
               entity_identifier
           );
}

GQuark investigation_graph_layout_error_quark(void)
{
    return g_quark_from_static_string(
        "investigation-graph-layout-error-quark"
    );
}

InvestigationGraphLayout *investigation_graph_layout_new(void)
{
    InvestigationGraphLayout *layout =
        NULL;

    layout =
        g_try_new0(
            InvestigationGraphLayout,
            1
        );

    if (layout == NULL)
    {
        return NULL;
    }

    layout->positions_by_entity_identifier =
        g_hash_table_new_full(
            g_str_hash,
            g_str_equal,
            g_free,
            g_free
        );

    if (layout->positions_by_entity_identifier == NULL)
    {
        investigation_graph_layout_free(
            layout
        );

        return NULL;
    }

    return layout;
}

gboolean investigation_graph_layout_set_position(
    InvestigationGraphLayout *layout,
    const char *entity_identifier,
    double x,
    double y,
    GError **error
)
{
    InvestigationGraphLayoutPosition *position =
        NULL;

    char *entity_identifier_copy =
        NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (layout == NULL ||
        layout->positions_by_entity_identifier == NULL)
    {
        investigation_graph_layout_set_error_literal(
            error,
            INVESTIGATION_GRAPH_LAYOUT_ERROR_INVALID_ARGUMENT,
            "La disposition du graphe est absente."
        );

        return FALSE;
    }

    if (!investigation_graph_layout_identifier_is_valid(
            entity_identifier
        ))
    {
        investigation_graph_layout_set_error_literal(
            error,
            INVESTIGATION_GRAPH_LAYOUT_ERROR_INVALID_IDENTIFIER,
            "L'identifiant de l'entité n'est pas un UUID valide."
        );

        return FALSE;
    }

    if (!isfinite(x) ||
        !isfinite(y))
    {
        investigation_graph_layout_set_error_literal(
            error,
            INVESTIGATION_GRAPH_LAYOUT_ERROR_INVALID_COORDINATE,
            "Les coordonnées du nœud doivent être des nombres finis."
        );

        return FALSE;
    }

    entity_identifier_copy =
        g_strdup(
            entity_identifier
        );

    position =
        g_try_new0(
            InvestigationGraphLayoutPosition,
            1
        );

    if (entity_identifier_copy == NULL ||
        position == NULL)
    {
        g_free(
            position
        );

        g_free(
            entity_identifier_copy
        );

        investigation_graph_layout_set_error_literal(
            error,
            INVESTIGATION_GRAPH_LAYOUT_ERROR_MEMORY,
            "Impossible d'allouer la position du nœud."
        );

        return FALSE;
    }

    position->x =
        x;

    position->y =
        y;

    g_hash_table_replace(
        layout->positions_by_entity_identifier,
        entity_identifier_copy,
        position
    );

    return TRUE;
}

gboolean investigation_graph_layout_get_position(
    const InvestigationGraphLayout *layout,
    const char *entity_identifier,
    double *x,
    double *y
)
{
    const InvestigationGraphLayoutPosition *position =
        NULL;

    if (layout == NULL ||
        layout->positions_by_entity_identifier == NULL ||
        !investigation_graph_layout_identifier_is_valid(
            entity_identifier
        ))
    {
        return FALSE;
    }

    position =
        g_hash_table_lookup(
            layout->positions_by_entity_identifier,
            entity_identifier
        );

    if (position == NULL)
    {
        return FALSE;
    }

    if (x != NULL)
    {
        *x =
            position->x;
    }

    if (y != NULL)
    {
        *y =
            position->y;
    }

    return TRUE;
}

gboolean investigation_graph_layout_remove_position(
    InvestigationGraphLayout *layout,
    const char *entity_identifier
)
{
    if (layout == NULL ||
        layout->positions_by_entity_identifier == NULL ||
        !investigation_graph_layout_identifier_is_valid(
            entity_identifier
        ))
    {
        return FALSE;
    }

    return g_hash_table_remove(
        layout->positions_by_entity_identifier,
        entity_identifier
    );
}

void investigation_graph_layout_clear(
    InvestigationGraphLayout *layout
)
{
    if (layout == NULL ||
        layout->positions_by_entity_identifier == NULL)
    {
        return;
    }

    g_hash_table_remove_all(
        layout->positions_by_entity_identifier
    );
}

guint investigation_graph_layout_get_count(
    const InvestigationGraphLayout *layout
)
{
    if (layout == NULL ||
        layout->positions_by_entity_identifier == NULL)
    {
        return 0U;
    }

    return g_hash_table_size(
        layout->positions_by_entity_identifier
    );
}

void investigation_graph_layout_free(
    InvestigationGraphLayout *layout
)
{
    if (layout == NULL)
    {
        return;
    }

    g_clear_pointer(
        &layout->positions_by_entity_identifier,
        g_hash_table_unref
    );

    g_free(
        layout
    );
}
