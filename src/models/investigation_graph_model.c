/******************************************************************************
 * @file investigation_graph_model.c
 * @brief Graphe métier en mémoire d'une enquête.
 ******************************************************************************/

#include "models/investigation_graph_model.h"

#include <glib.h>

/**
 * @brief Représentation privée du graphe métier.
 *
 * Les tables principales possèdent les modèles.
 * Les index entrants et sortants ne possèdent que leurs tableaux.
 * Les relations contenues dans ces tableaux sont empruntées à
 * relations_by_identifier.
 */
struct InvestigationGraphModel
{
    GHashTable *entities_by_identifier;
    GHashTable *relations_by_identifier;

    GHashTable *outgoing_relations_by_entity;
    GHashTable *incoming_relations_by_entity;
};

/**
 * @brief Enregistre une erreur littérale du modèle.
 */
static void investigation_graph_model_set_error_literal(
    GError **error,
    InvestigationGraphModelError error_code,
    const char *message
)
{
    if (error == NULL ||
        *error != NULL)
    {
        return;
    }

    g_set_error_literal(
        error,
        INVESTIGATION_GRAPH_MODEL_ERROR,
        error_code,
        message
    );
}

/**
 * @brief Libère une entité possédée par une GHashTable.
 */
static void investigation_graph_model_destroy_entity(
    gpointer data
)
{
    entity_record_free(
        data
    );
}

/**
 * @brief Libère une relation possédée par une GHashTable.
 */
static void investigation_graph_model_destroy_relation(
    gpointer data
)
{
    relation_record_free(
        data
    );
}

/**
 * @brief Libère un tableau d'index.
 *
 * Les éléments du tableau sont empruntés et ne sont pas libérés.
 */
static void investigation_graph_model_destroy_relation_array(
    gpointer data
)
{
    if (data == NULL)
    {
        return;
    }

    g_ptr_array_unref(
        data
    );
}

/**
 * @brief Compare deux pointeurs vers EntityRecord par UUID.
 */
static gint investigation_graph_model_compare_entities(
    gconstpointer first_data,
    gconstpointer second_data
)
{
    const EntityRecord *const *first_entity =
        first_data;

    const EntityRecord *const *second_entity =
        second_data;

    return g_strcmp0(
        entity_record_get_identifier(
            *first_entity
        ),
        entity_record_get_identifier(
            *second_entity
        )
    );
}

/**
 * @brief Compare deux pointeurs vers RelationRecord par UUID.
 */
static gint investigation_graph_model_compare_relations(
    gconstpointer first_data,
    gconstpointer second_data
)
{
    const RelationRecord *const *first_relation =
        first_data;

    const RelationRecord *const *second_relation =
        second_data;

    return g_strcmp0(
        relation_record_get_identifier(
            *first_relation
        ),
        relation_record_get_identifier(
            *second_relation
        )
    );
}

/**
 * @brief Vérifie qu'un UUID d'entité est exploitable.
 */
static gboolean investigation_graph_model_entity_identifier_is_valid(
    const char *entity_identifier
)
{
    return entity_identifier != NULL &&
           g_uuid_string_is_valid(
               entity_identifier
           );
}

/**
 * @brief Vérifie qu'un UUID de relation est exploitable.
 */
static gboolean investigation_graph_model_relation_identifier_is_valid(
    const char *relation_identifier
)
{
    return relation_identifier != NULL &&
           g_uuid_string_is_valid(
               relation_identifier
           );
}

/**
 * @brief Valide une demande de navigation depuis une entité.
 */
static gboolean investigation_graph_model_validate_entity_request(
    const InvestigationGraphModel *graph_model,
    const char *entity_identifier,
    GError **error
)
{
    if (graph_model == NULL)
    {
        investigation_graph_model_set_error_literal(
            error,
            INVESTIGATION_GRAPH_MODEL_ERROR_INVALID_ARGUMENT,
            "Le graphe d'enquête est absent."
        );

        return FALSE;
    }

    if (!investigation_graph_model_entity_identifier_is_valid(
            entity_identifier
        ))
    {
        investigation_graph_model_set_error_literal(
            error,
            INVESTIGATION_GRAPH_MODEL_ERROR_INVALID_ARGUMENT,
            "L'identifiant d'entité est invalide."
        );

        return FALSE;
    }

    if (!g_hash_table_contains(
            graph_model->entities_by_identifier,
            entity_identifier
        ))
    {
        investigation_graph_model_set_error_literal(
            error,
            INVESTIGATION_GRAPH_MODEL_ERROR_ENTITY_NOT_FOUND,
            "L'entité demandée n'existe pas dans le graphe."
        );

        return FALSE;
    }

    return TRUE;
}

/**
 * @brief Copie les pointeurs empruntés d'un tableau d'index.
 */
static GPtrArray *investigation_graph_model_copy_relation_array(
    const GPtrArray *source_array,
    GError **error
)
{
    GPtrArray *copied_array =
        NULL;

    guint relation_index =
        0;

    copied_array =
        g_ptr_array_new();

    if (copied_array == NULL)
    {
        investigation_graph_model_set_error_literal(
            error,
            INVESTIGATION_GRAPH_MODEL_ERROR_MEMORY,
            "Impossible d'allouer la liste des relations."
        );

        return NULL;
    }

    if (source_array != NULL)
    {
        for (relation_index = 0;
             relation_index < source_array->len;
             relation_index++)
        {
            g_ptr_array_add(
                copied_array,
                g_ptr_array_index(
                    source_array,
                    relation_index
                )
            );
        }
    }

    g_ptr_array_sort(
        copied_array,
        investigation_graph_model_compare_relations
    );

    return copied_array;
}

GQuark investigation_graph_model_error_quark(void)
{
    return g_quark_from_static_string(
        "investigation-graph-model-error-quark"
    );
}

InvestigationGraphModel *investigation_graph_model_new(
    GError **error
)
{
    InvestigationGraphModel *graph_model =
        NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    graph_model =
        g_try_new0(
            InvestigationGraphModel,
            1
        );

    if (graph_model == NULL)
    {
        investigation_graph_model_set_error_literal(
            error,
            INVESTIGATION_GRAPH_MODEL_ERROR_MEMORY,
            "Impossible d'allouer le graphe d'enquête."
        );

        return NULL;
    }

    graph_model->entities_by_identifier =
        g_hash_table_new_full(
            g_str_hash,
            g_str_equal,
            g_free,
            investigation_graph_model_destroy_entity
        );

    graph_model->relations_by_identifier =
        g_hash_table_new_full(
            g_str_hash,
            g_str_equal,
            g_free,
            investigation_graph_model_destroy_relation
        );

    graph_model->outgoing_relations_by_entity =
        g_hash_table_new_full(
            g_str_hash,
            g_str_equal,
            g_free,
            investigation_graph_model_destroy_relation_array
        );

    graph_model->incoming_relations_by_entity =
        g_hash_table_new_full(
            g_str_hash,
            g_str_equal,
            g_free,
            investigation_graph_model_destroy_relation_array
        );

    if (graph_model->entities_by_identifier == NULL ||
        graph_model->relations_by_identifier == NULL ||
        graph_model->outgoing_relations_by_entity == NULL ||
        graph_model->incoming_relations_by_entity == NULL)
    {
        investigation_graph_model_free(
            graph_model
        );

        investigation_graph_model_set_error_literal(
            error,
            INVESTIGATION_GRAPH_MODEL_ERROR_MEMORY,
            "Impossible d'allouer les index du graphe d'enquête."
        );

        return NULL;
    }

    return graph_model;
}

void investigation_graph_model_free(
    InvestigationGraphModel *graph_model
)
{
    if (graph_model == NULL)
    {
        return;
    }

    /*
     * Les index ne possèdent pas les RelationRecord.
     * Ils sont donc détruits avant la table propriétaire des relations.
     */
    g_clear_pointer(
        &graph_model->incoming_relations_by_entity,
        g_hash_table_unref
    );

    g_clear_pointer(
        &graph_model->outgoing_relations_by_entity,
        g_hash_table_unref
    );

    g_clear_pointer(
        &graph_model->relations_by_identifier,
        g_hash_table_unref
    );

    g_clear_pointer(
        &graph_model->entities_by_identifier,
        g_hash_table_unref
    );

    g_free(
        graph_model
    );
}

gboolean investigation_graph_model_add_entity(
    InvestigationGraphModel *graph_model,
    EntityRecord *entity_record,
    GError **error
)
{
    const char *entity_identifier =
        NULL;

    char *entity_key =
        NULL;

    char *outgoing_key =
        NULL;

    char *incoming_key =
        NULL;

    GPtrArray *outgoing_relations =
        NULL;

    GPtrArray *incoming_relations =
        NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (graph_model == NULL ||
        entity_record == NULL)
    {
        investigation_graph_model_set_error_literal(
            error,
            INVESTIGATION_GRAPH_MODEL_ERROR_INVALID_ARGUMENT,
            "Le graphe ou l'entité est absent."
        );

        return FALSE;
    }

    entity_identifier =
        entity_record_get_identifier(
            entity_record
        );

    if (!investigation_graph_model_entity_identifier_is_valid(
            entity_identifier
        ))
    {
        investigation_graph_model_set_error_literal(
            error,
            INVESTIGATION_GRAPH_MODEL_ERROR_INVALID_ARGUMENT,
            "L'entité possède un identifiant invalide."
        );

        return FALSE;
    }

    if (g_hash_table_contains(
            graph_model->entities_by_identifier,
            entity_identifier
        ))
    {
        investigation_graph_model_set_error_literal(
            error,
            INVESTIGATION_GRAPH_MODEL_ERROR_DUPLICATE_ENTITY,
            "Une entité possède déjà cet identifiant."
        );

        return FALSE;
    }

    entity_key =
        g_strdup(
            entity_identifier
        );

    outgoing_key =
        g_strdup(
            entity_identifier
        );

    incoming_key =
        g_strdup(
            entity_identifier
        );

    outgoing_relations =
        g_ptr_array_new();

    incoming_relations =
        g_ptr_array_new();

    if (entity_key == NULL ||
        outgoing_key == NULL ||
        incoming_key == NULL ||
        outgoing_relations == NULL ||
        incoming_relations == NULL)
    {
        g_free(
            incoming_key
        );

        g_free(
            outgoing_key
        );

        g_free(
            entity_key
        );

        if (incoming_relations != NULL)
        {
            g_ptr_array_unref(
                incoming_relations
            );
        }

        if (outgoing_relations != NULL)
        {
            g_ptr_array_unref(
                outgoing_relations
            );
        }

        investigation_graph_model_set_error_literal(
            error,
            INVESTIGATION_GRAPH_MODEL_ERROR_MEMORY,
            "Impossible d'allouer les index de l'entité."
        );

        return FALSE;
    }

    g_hash_table_insert(
        graph_model->entities_by_identifier,
        entity_key,
        entity_record
    );

    g_hash_table_insert(
        graph_model->outgoing_relations_by_entity,
        outgoing_key,
        outgoing_relations
    );

    g_hash_table_insert(
        graph_model->incoming_relations_by_entity,
        incoming_key,
        incoming_relations
    );

    return TRUE;
}

gboolean investigation_graph_model_add_relation(
    InvestigationGraphModel *graph_model,
    RelationRecord *relation_record,
    GError **error
)
{
    const char *relation_identifier =
        NULL;

    const char *source_identifier =
        NULL;

    const char *target_identifier =
        NULL;

    char *relation_key =
        NULL;

    GPtrArray *outgoing_relations =
        NULL;

    GPtrArray *incoming_relations =
        NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (graph_model == NULL ||
        relation_record == NULL)
    {
        investigation_graph_model_set_error_literal(
            error,
            INVESTIGATION_GRAPH_MODEL_ERROR_INVALID_ARGUMENT,
            "Le graphe ou la relation est absent."
        );

        return FALSE;
    }

    relation_identifier =
        relation_record_get_identifier(
            relation_record
        );

    source_identifier =
        relation_record_get_source_entity_identifier(
            relation_record
        );

    target_identifier =
        relation_record_get_target_entity_identifier(
            relation_record
        );

    if (!investigation_graph_model_relation_identifier_is_valid(
            relation_identifier
        ) ||
        !investigation_graph_model_entity_identifier_is_valid(
            source_identifier
        ) ||
        !investigation_graph_model_entity_identifier_is_valid(
            target_identifier
        ))
    {
        investigation_graph_model_set_error_literal(
            error,
            INVESTIGATION_GRAPH_MODEL_ERROR_INVALID_ARGUMENT,
            "La relation contient un identifiant invalide."
        );

        return FALSE;
    }

    if (g_hash_table_contains(
            graph_model->relations_by_identifier,
            relation_identifier
        ))
    {
        investigation_graph_model_set_error_literal(
            error,
            INVESTIGATION_GRAPH_MODEL_ERROR_DUPLICATE_RELATION,
            "Une relation possède déjà cet identifiant."
        );

        return FALSE;
    }

    if (!g_hash_table_contains(
            graph_model->entities_by_identifier,
            source_identifier
        ))
    {
        investigation_graph_model_set_error_literal(
            error,
            INVESTIGATION_GRAPH_MODEL_ERROR_SOURCE_NOT_FOUND,
            "L'entité source de la relation est absente du graphe."
        );

        return FALSE;
    }

    if (!g_hash_table_contains(
            graph_model->entities_by_identifier,
            target_identifier
        ))
    {
        investigation_graph_model_set_error_literal(
            error,
            INVESTIGATION_GRAPH_MODEL_ERROR_TARGET_NOT_FOUND,
            "L'entité cible de la relation est absente du graphe."
        );

        return FALSE;
    }

    outgoing_relations =
        g_hash_table_lookup(
            graph_model->outgoing_relations_by_entity,
            source_identifier
        );

    incoming_relations =
        g_hash_table_lookup(
            graph_model->incoming_relations_by_entity,
            target_identifier
        );

    if (outgoing_relations == NULL ||
        incoming_relations == NULL)
    {
        investigation_graph_model_set_error_literal(
            error,
            INVESTIGATION_GRAPH_MODEL_ERROR_INVALID_ARGUMENT,
            "Les index internes du graphe sont incohérents."
        );

        return FALSE;
    }

    relation_key =
        g_strdup(
            relation_identifier
        );

    if (relation_key == NULL)
    {
        investigation_graph_model_set_error_literal(
            error,
            INVESTIGATION_GRAPH_MODEL_ERROR_MEMORY,
            "Impossible d'allouer l'index de la relation."
        );

        return FALSE;
    }

    g_hash_table_insert(
        graph_model->relations_by_identifier,
        relation_key,
        relation_record
    );

    g_ptr_array_add(
        outgoing_relations,
        relation_record
    );

    g_ptr_array_add(
        incoming_relations,
        relation_record
    );

    return TRUE;
}

const EntityRecord *investigation_graph_model_find_entity(
    const InvestigationGraphModel *graph_model,
    const char *entity_identifier
)
{
    if (graph_model == NULL ||
        !investigation_graph_model_entity_identifier_is_valid(
            entity_identifier
        ))
    {
        return NULL;
    }

    return g_hash_table_lookup(
        graph_model->entities_by_identifier,
        entity_identifier
    );
}

const RelationRecord *investigation_graph_model_find_relation(
    const InvestigationGraphModel *graph_model,
    const char *relation_identifier
)
{
    if (graph_model == NULL ||
        !investigation_graph_model_relation_identifier_is_valid(
            relation_identifier
        ))
    {
        return NULL;
    }

    return g_hash_table_lookup(
        graph_model->relations_by_identifier,
        relation_identifier
    );
}

guint64 investigation_graph_model_get_entity_count(
    const InvestigationGraphModel *graph_model
)
{
    if (graph_model == NULL)
    {
        return 0;
    }

    return (guint64) g_hash_table_size(
        graph_model->entities_by_identifier
    );
}

guint64 investigation_graph_model_get_relation_count(
    const InvestigationGraphModel *graph_model
)
{
    if (graph_model == NULL)
    {
        return 0;
    }

    return (guint64) g_hash_table_size(
        graph_model->relations_by_identifier
    );
}

GPtrArray *investigation_graph_model_list_entities(
    const InvestigationGraphModel *graph_model,
    GError **error
)
{
    GPtrArray *entities =
        NULL;

    GHashTableIter iterator;

    gpointer entity_record =
        NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (graph_model == NULL)
    {
        investigation_graph_model_set_error_literal(
            error,
            INVESTIGATION_GRAPH_MODEL_ERROR_INVALID_ARGUMENT,
            "Le graphe d'enquête est absent."
        );

        return NULL;
    }

    entities =
        g_ptr_array_new();

    if (entities == NULL)
    {
        investigation_graph_model_set_error_literal(
            error,
            INVESTIGATION_GRAPH_MODEL_ERROR_MEMORY,
            "Impossible d'allouer la liste des entités."
        );

        return NULL;
    }

    g_hash_table_iter_init(
        &iterator,
        graph_model->entities_by_identifier
    );

    while (g_hash_table_iter_next(
               &iterator,
               NULL,
               &entity_record
           ))
    {
        g_ptr_array_add(
            entities,
            entity_record
        );
    }

    g_ptr_array_sort(
        entities,
        investigation_graph_model_compare_entities
    );

    return entities;
}

GPtrArray *investigation_graph_model_list_relations(
    const InvestigationGraphModel *graph_model,
    GError **error
)
{
    GPtrArray *relations =
        NULL;

    GHashTableIter iterator;

    gpointer relation_record =
        NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (graph_model == NULL)
    {
        investigation_graph_model_set_error_literal(
            error,
            INVESTIGATION_GRAPH_MODEL_ERROR_INVALID_ARGUMENT,
            "Le graphe d'enquête est absent."
        );

        return NULL;
    }

    relations =
        g_ptr_array_new();

    if (relations == NULL)
    {
        investigation_graph_model_set_error_literal(
            error,
            INVESTIGATION_GRAPH_MODEL_ERROR_MEMORY,
            "Impossible d'allouer la liste des relations."
        );

        return NULL;
    }

    g_hash_table_iter_init(
        &iterator,
        graph_model->relations_by_identifier
    );

    while (g_hash_table_iter_next(
               &iterator,
               NULL,
               &relation_record
           ))
    {
        g_ptr_array_add(
            relations,
            relation_record
        );
    }

    g_ptr_array_sort(
        relations,
        investigation_graph_model_compare_relations
    );

    return relations;
}

GPtrArray *investigation_graph_model_list_outgoing_relations(
    const InvestigationGraphModel *graph_model,
    const char *entity_identifier,
    GError **error
)
{
    const GPtrArray *outgoing_relations =
        NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (!investigation_graph_model_validate_entity_request(
            graph_model,
            entity_identifier,
            error
        ))
    {
        return NULL;
    }

    outgoing_relations =
        g_hash_table_lookup(
            graph_model->outgoing_relations_by_entity,
            entity_identifier
        );

    return investigation_graph_model_copy_relation_array(
        outgoing_relations,
        error
    );
}

GPtrArray *investigation_graph_model_list_incoming_relations(
    const InvestigationGraphModel *graph_model,
    const char *entity_identifier,
    GError **error
)
{
    const GPtrArray *incoming_relations =
        NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (!investigation_graph_model_validate_entity_request(
            graph_model,
            entity_identifier,
            error
        ))
    {
        return NULL;
    }

    incoming_relations =
        g_hash_table_lookup(
            graph_model->incoming_relations_by_entity,
            entity_identifier
        );

    return investigation_graph_model_copy_relation_array(
        incoming_relations,
        error
    );
}

GPtrArray *investigation_graph_model_list_incident_relations(
    const InvestigationGraphModel *graph_model,
    const char *entity_identifier,
    GError **error
)
{
    const GPtrArray *outgoing_relations =
        NULL;

    const GPtrArray *incoming_relations =
        NULL;

    GPtrArray *incident_relations =
        NULL;

    GHashTable *seen_relations =
        NULL;

    guint relation_index =
        0;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (!investigation_graph_model_validate_entity_request(
            graph_model,
            entity_identifier,
            error
        ))
    {
        return NULL;
    }

    outgoing_relations =
        g_hash_table_lookup(
            graph_model->outgoing_relations_by_entity,
            entity_identifier
        );

    incoming_relations =
        g_hash_table_lookup(
            graph_model->incoming_relations_by_entity,
            entity_identifier
        );

    incident_relations =
        g_ptr_array_new();

    seen_relations =
        g_hash_table_new(
            g_direct_hash,
            g_direct_equal
        );

    if (incident_relations == NULL ||
        seen_relations == NULL)
    {
        if (incident_relations != NULL)
        {
            g_ptr_array_unref(
                incident_relations
            );
        }

        if (seen_relations != NULL)
        {
            g_hash_table_unref(
                seen_relations
            );
        }

        investigation_graph_model_set_error_literal(
            error,
            INVESTIGATION_GRAPH_MODEL_ERROR_MEMORY,
            "Impossible d'allouer la liste des relations incidentes."
        );

        return NULL;
    }

    if (outgoing_relations != NULL)
    {
        for (relation_index = 0;
             relation_index < outgoing_relations->len;
             relation_index++)
        {
            gpointer relation_record =
                g_ptr_array_index(
                    outgoing_relations,
                    relation_index
                );

            if (g_hash_table_add(
                    seen_relations,
                    relation_record
                ))
            {
                g_ptr_array_add(
                    incident_relations,
                    relation_record
                );
            }
        }
    }

    if (incoming_relations != NULL)
    {
        for (relation_index = 0;
             relation_index < incoming_relations->len;
             relation_index++)
        {
            gpointer relation_record =
                g_ptr_array_index(
                    incoming_relations,
                    relation_index
                );

            if (g_hash_table_add(
                    seen_relations,
                    relation_record
                ))
            {
                g_ptr_array_add(
                    incident_relations,
                    relation_record
                );
            }
        }
    }

    g_hash_table_unref(
        seen_relations
    );

    g_ptr_array_sort(
        incident_relations,
        investigation_graph_model_compare_relations
    );

    return incident_relations;
}
