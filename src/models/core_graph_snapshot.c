/******************************************************************************
 * @file core_graph_snapshot.c
 * @brief Modèle propriétaire de la projection unifiée du graphe.
 ******************************************************************************/

#include "models/core_graph_snapshot.h"

struct CoreGraphNode { CoreGraphNodeView view; };
struct CoreGraphEdge { CoreGraphEdgeView view; };
struct CoreGraphCapability { CoreGraphCapabilityView view; };

struct CoreGraphSnapshot
{
    char *investigation_id;
    char *investigation_label;
    guint schema_version;
    guint transport_version;
    CoreGraphLimits limits;
    GPtrArray *nodes;
    GPtrArray *edges;
    GPtrArray *capabilities;
    GHashTable *node_ids;
    GHashTable *edge_ids;
};

static void core_graph_snapshot_set_error(
    GError **error,
    CoreGraphSnapshotError code,
    const char *message)
{
    if (error != NULL && *error == NULL)
        g_set_error_literal(error, CORE_GRAPH_SNAPSHOT_ERROR, code, message);
}

static char *core_graph_copy_required(const char *value)
{
    return value != NULL && value[0] != '\0' ? g_strdup(value) : NULL;
}

static char *core_graph_copy_optional(const char *value)
{
    return value != NULL ? g_strdup(value) : NULL;
}

static void core_graph_node_free(CoreGraphNode *node)
{
    if (node == NULL) return;
    g_free((char *) node->view.id);
    g_free((char *) node->view.object_kind);
    g_free((char *) node->view.object_id);
    g_free((char *) node->view.type);
    g_free((char *) node->view.label);
    g_free((char *) node->view.state);
    g_free((char *) node->view.group);
    g_free((char *) node->view.raw_excerpt);
    g_free((char *) node->view.value_raw);
    g_free((char *) node->view.value_normalized);
    g_free((char *) node->view.value_corrected);
    g_free((char *) node->view.review_state);
    g_free((char *) node->view.provenance_kind);
    g_free((char *) node->view.source_header);
    g_free((char *) node->view.entity_id);
    g_free((char *) node->view.promotion_kind);
    g_free((char *) node->view.tool_id);
    g_free((char *) node->view.tool_version);
    g_free((char *) node->view.started_at);
    g_free((char *) node->view.finished_at);
    g_free((char *) node->view.collected_at);
    g_free((char *) node->view.imported_at);
    g_free((char *) node->view.observed_at);
    g_free((char *) node->view.integrated_at);
    g_free((char *) node->view.missing_provenance_reason);
    g_free(node);
}

static void core_graph_edge_free(CoreGraphEdge *edge)
{
    if (edge == NULL) return;
    g_free((char *) edge->view.id);
    g_free((char *) edge->view.source);
    g_free((char *) edge->view.target);
    g_free((char *) edge->view.kind);
    g_free((char *) edge->view.semantic);
    g_free((char *) edge->view.review_state);
    g_free((char *) edge->view.disposition);
    g_free(edge);
}

static void core_graph_capability_free(CoreGraphCapability *capability)
{
    if (capability == NULL) return;
    g_free((char *) capability->view.node_id);
    g_free((char *) capability->view.object_kind);
    g_free((char *) capability->view.object_id);
    g_free((char *) capability->view.capability_id);
    g_free((char *) capability->view.reason);
    g_free(capability);
}

GQuark core_graph_snapshot_error_quark(void)
{
    return g_quark_from_static_string("core-graph-snapshot-error-quark");
}

CoreGraphSnapshot *core_graph_snapshot_new(
    const char *investigation_id,
    const char *investigation_label,
    guint schema_version,
    CoreGraphLimits limits,
    GError **error)
{
    CoreGraphSnapshot *snapshot = NULL;
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);
    if (investigation_id == NULL || investigation_id[0] == '\0' ||
        investigation_label == NULL || investigation_label[0] == '\0' ||
        schema_version == 0 || limits.max_nodes == 0 || limits.max_edges == 0 ||
        limits.max_serialized_bytes == 0)
    {
        core_graph_snapshot_set_error(error,
            CORE_GRAPH_SNAPSHOT_ERROR_INVALID_ARGUMENT,
            "Les métadonnées ou limites du snapshot sont invalides.");
        return NULL;
    }
    snapshot = g_try_new0(CoreGraphSnapshot, 1);
    if (snapshot == NULL) goto memory_failure;
    snapshot->investigation_id = g_strdup(investigation_id);
    snapshot->investigation_label = g_strdup(investigation_label);
    snapshot->schema_version = schema_version;
    snapshot->transport_version = 2U;
    snapshot->limits = limits;
    snapshot->nodes = g_ptr_array_new_with_free_func(
        (GDestroyNotify) core_graph_node_free);
    snapshot->edges = g_ptr_array_new_with_free_func(
        (GDestroyNotify) core_graph_edge_free);
    snapshot->capabilities = g_ptr_array_new_with_free_func(
        (GDestroyNotify) core_graph_capability_free);
    snapshot->node_ids = g_hash_table_new_full(g_str_hash, g_str_equal,
        g_free, NULL);
    snapshot->edge_ids = g_hash_table_new_full(g_str_hash, g_str_equal,
        g_free, NULL);
    if (snapshot->investigation_id == NULL ||
        snapshot->investigation_label == NULL || snapshot->nodes == NULL ||
        snapshot->edges == NULL || snapshot->capabilities == NULL ||
        snapshot->node_ids == NULL || snapshot->edge_ids == NULL)
        goto memory_failure;
    return snapshot;

memory_failure:
    core_graph_snapshot_free(snapshot);
    core_graph_snapshot_set_error(error, CORE_GRAPH_SNAPSHOT_ERROR_MEMORY,
        "Impossible d'allouer le snapshot du graphe cœur.");
    return NULL;
}

void core_graph_snapshot_free(CoreGraphSnapshot *snapshot)
{
    if (snapshot == NULL) return;
    g_clear_pointer(&snapshot->edge_ids, g_hash_table_unref);
    g_clear_pointer(&snapshot->node_ids, g_hash_table_unref);
    g_clear_pointer(&snapshot->capabilities, g_ptr_array_unref);
    g_clear_pointer(&snapshot->edges, g_ptr_array_unref);
    g_clear_pointer(&snapshot->nodes, g_ptr_array_unref);
    g_free(snapshot->investigation_label);
    g_free(snapshot->investigation_id);
    g_free(snapshot);
}

gboolean core_graph_snapshot_add_node(
    CoreGraphSnapshot *snapshot,
    const CoreGraphNodeView *view,
    GError **error)
{
    CoreGraphNode *node = NULL;
    char *id_key = NULL;
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    if (snapshot == NULL || view == NULL || view->id == NULL ||
        view->object_kind == NULL || view->object_id == NULL ||
        view->type == NULL || view->label == NULL || view->state == NULL)
    {
        core_graph_snapshot_set_error(error,
            CORE_GRAPH_SNAPSHOT_ERROR_INVALID_ARGUMENT,
            "Le nœud de projection est incomplet.");
        return FALSE;
    }
    if (snapshot->nodes->len >= snapshot->limits.max_nodes)
    {
        core_graph_snapshot_set_error(error, CORE_GRAPH_SNAPSHOT_ERROR_LIMIT,
            "La limite de nœuds du snapshot est atteinte.");
        return FALSE;
    }
    if (g_hash_table_contains(snapshot->node_ids, view->id))
    {
        core_graph_snapshot_set_error(error,
            CORE_GRAPH_SNAPSHOT_ERROR_DUPLICATE,
            "Un identifiant de nœud est dupliqué.");
        return FALSE;
    }
    node = g_try_new0(CoreGraphNode, 1);
    if (node == NULL) goto memory_failure;
#define COPY_REQUIRED(field) node->view.field = core_graph_copy_required(view->field)
#define COPY_OPTIONAL(field) node->view.field = core_graph_copy_optional(view->field)
    COPY_REQUIRED(id); COPY_REQUIRED(object_kind); COPY_REQUIRED(object_id);
    COPY_REQUIRED(type); COPY_REQUIRED(label); COPY_REQUIRED(state);
    COPY_OPTIONAL(group); COPY_OPTIONAL(raw_excerpt); COPY_OPTIONAL(value_raw);
    COPY_OPTIONAL(value_normalized); COPY_OPTIONAL(value_corrected);
    COPY_OPTIONAL(review_state); COPY_OPTIONAL(provenance_kind);
    COPY_OPTIONAL(source_header); COPY_OPTIONAL(entity_id);
    COPY_OPTIONAL(promotion_kind); COPY_OPTIONAL(tool_id);
    COPY_OPTIONAL(tool_version); COPY_OPTIONAL(started_at);
    COPY_OPTIONAL(finished_at); COPY_OPTIONAL(missing_provenance_reason);
    COPY_OPTIONAL(collected_at); COPY_OPTIONAL(imported_at);
    COPY_OPTIONAL(observed_at); COPY_OPTIONAL(integrated_at);
#undef COPY_REQUIRED
#undef COPY_OPTIONAL
    node->view.provenance_complete = view->provenance_complete;
    if (node->view.id == NULL || node->view.object_kind == NULL ||
        node->view.object_id == NULL || node->view.type == NULL ||
        node->view.label == NULL || node->view.state == NULL)
        goto memory_failure;
    id_key = g_strdup(view->id);
    if (id_key == NULL) goto memory_failure;
    g_hash_table_add(snapshot->node_ids, id_key);
    g_ptr_array_add(snapshot->nodes, node);
    return TRUE;

memory_failure:
    g_free(id_key);
    core_graph_node_free(node);
    core_graph_snapshot_set_error(error, CORE_GRAPH_SNAPSHOT_ERROR_MEMORY,
        "Impossible de copier le nœud de projection.");
    return FALSE;
}

gboolean core_graph_snapshot_add_edge(
    CoreGraphSnapshot *snapshot,
    const CoreGraphEdgeView *view,
    GError **error)
{
    CoreGraphEdge *edge = NULL;
    char *id_key = NULL;
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    if (snapshot == NULL || view == NULL || view->id == NULL ||
        view->source == NULL || view->target == NULL || view->kind == NULL ||
        view->semantic == NULL || view->review_state == NULL)
    {
        core_graph_snapshot_set_error(error,
            CORE_GRAPH_SNAPSHOT_ERROR_INVALID_ARGUMENT,
            "L'arête de projection est incomplète.");
        return FALSE;
    }
    if (snapshot->edges->len >= snapshot->limits.max_edges)
    {
        core_graph_snapshot_set_error(error, CORE_GRAPH_SNAPSHOT_ERROR_LIMIT,
            "La limite d'arêtes du snapshot est atteinte.");
        return FALSE;
    }
    if (!g_hash_table_contains(snapshot->node_ids, view->source) ||
        !g_hash_table_contains(snapshot->node_ids, view->target))
    {
        core_graph_snapshot_set_error(error,
            CORE_GRAPH_SNAPSHOT_ERROR_MISSING_REFERENCE,
            "Une extrémité d'arête est absente du snapshot.");
        return FALSE;
    }
    if (g_hash_table_contains(snapshot->edge_ids, view->id))
    {
        core_graph_snapshot_set_error(error, CORE_GRAPH_SNAPSHOT_ERROR_DUPLICATE,
            "Un identifiant d'arête est dupliqué.");
        return FALSE;
    }
    edge = g_try_new0(CoreGraphEdge, 1);
    if (edge == NULL) goto memory_failure;
    edge->view.id = g_strdup(view->id);
    edge->view.source = g_strdup(view->source);
    edge->view.target = g_strdup(view->target);
    edge->view.kind = g_strdup(view->kind);
    edge->view.semantic = g_strdup(view->semantic);
    edge->view.review_state = g_strdup(view->review_state);
    edge->view.disposition = core_graph_copy_optional(view->disposition);
    edge->view.directed = view->directed;
    if (edge->view.id == NULL || edge->view.source == NULL ||
        edge->view.target == NULL || edge->view.kind == NULL ||
        edge->view.semantic == NULL || edge->view.review_state == NULL)
        goto memory_failure;
    id_key = g_strdup(view->id);
    if (id_key == NULL) goto memory_failure;
    g_hash_table_add(snapshot->edge_ids, id_key);
    g_ptr_array_add(snapshot->edges, edge);
    return TRUE;

memory_failure:
    g_free(id_key);
    core_graph_edge_free(edge);
    core_graph_snapshot_set_error(error, CORE_GRAPH_SNAPSHOT_ERROR_MEMORY,
        "Impossible de copier l'arête de projection.");
    return FALSE;
}

gboolean core_graph_snapshot_add_capability(
    CoreGraphSnapshot *snapshot,
    const CoreGraphCapabilityView *view,
    GError **error)
{
    CoreGraphCapability *capability = NULL;
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    if (snapshot == NULL || view == NULL || view->node_id == NULL ||
        view->object_kind == NULL || view->object_id == NULL ||
        view->capability_id == NULL || view->reason == NULL ||
        !g_hash_table_contains(snapshot->node_ids, view->node_id))
    {
        core_graph_snapshot_set_error(error,
            CORE_GRAPH_SNAPSHOT_ERROR_INVALID_ARGUMENT,
            "La capability de projection est invalide.");
        return FALSE;
    }
    capability = g_try_new0(CoreGraphCapability, 1);
    if (capability == NULL) goto memory_failure;
    capability->view.node_id = g_strdup(view->node_id);
    capability->view.object_kind = g_strdup(view->object_kind);
    capability->view.object_id = g_strdup(view->object_id);
    capability->view.capability_id = g_strdup(view->capability_id);
    capability->view.reason = g_strdup(view->reason);
    capability->view.available = view->available;
    if (capability->view.node_id == NULL ||
        capability->view.object_kind == NULL ||
        capability->view.object_id == NULL ||
        capability->view.capability_id == NULL ||
        capability->view.reason == NULL) goto memory_failure;
    g_ptr_array_add(snapshot->capabilities, capability);
    return TRUE;

memory_failure:
    core_graph_capability_free(capability);
    core_graph_snapshot_set_error(error, CORE_GRAPH_SNAPSHOT_ERROR_MEMORY,
        "Impossible de copier la capability de projection.");
    return FALSE;
}

static gint core_graph_compare_nodes(gconstpointer left, gconstpointer right)
{
    const CoreGraphNode *const *a = left;
    const CoreGraphNode *const *b = right;
    return g_strcmp0((*a)->view.id, (*b)->view.id);
}

static gint core_graph_compare_edges(gconstpointer left, gconstpointer right)
{
    const CoreGraphEdge *const *a = left;
    const CoreGraphEdge *const *b = right;
    return g_strcmp0((*a)->view.id, (*b)->view.id);
}

static gint core_graph_compare_capabilities(gconstpointer left, gconstpointer right)
{
    const CoreGraphCapability *const *a = left;
    const CoreGraphCapability *const *b = right;
    gint result = g_strcmp0((*a)->view.node_id, (*b)->view.node_id);
    return result != 0 ? result : g_strcmp0((*a)->view.capability_id,
        (*b)->view.capability_id);
}

void core_graph_snapshot_sort(CoreGraphSnapshot *snapshot)
{
    if (snapshot == NULL) return;
    g_ptr_array_sort(snapshot->nodes, core_graph_compare_nodes);
    g_ptr_array_sort(snapshot->edges, core_graph_compare_edges);
    g_ptr_array_sort(snapshot->capabilities, core_graph_compare_capabilities);
}

gboolean core_graph_snapshot_set_transport_version(
    CoreGraphSnapshot *snapshot,
    guint version,
    GError **error)
{
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    if (snapshot == NULL || (version != 2U && version != 3U))
    {
        core_graph_snapshot_set_error(error,
            CORE_GRAPH_SNAPSHOT_ERROR_INVALID_ARGUMENT,
            "La version de transport du snapshot est invalide.");
        return FALSE;
    }
    snapshot->transport_version = version;
    return TRUE;
}

const char *core_graph_snapshot_get_investigation_id(
    const CoreGraphSnapshot *snapshot)
{ return snapshot != NULL ? snapshot->investigation_id : NULL; }
const char *core_graph_snapshot_get_investigation_label(
    const CoreGraphSnapshot *snapshot)
{ return snapshot != NULL ? snapshot->investigation_label : NULL; }
guint core_graph_snapshot_get_schema_version(const CoreGraphSnapshot *snapshot)
{ return snapshot != NULL ? snapshot->schema_version : 0; }
guint core_graph_snapshot_get_transport_version(const CoreGraphSnapshot *snapshot)
{ return snapshot != NULL ? snapshot->transport_version : 0; }
CoreGraphLimits core_graph_snapshot_get_limits(const CoreGraphSnapshot *snapshot)
{ return snapshot != NULL ? snapshot->limits : (CoreGraphLimits) {0}; }
const GPtrArray *core_graph_snapshot_get_nodes(const CoreGraphSnapshot *snapshot)
{ return snapshot != NULL ? snapshot->nodes : NULL; }
const GPtrArray *core_graph_snapshot_get_edges(const CoreGraphSnapshot *snapshot)
{ return snapshot != NULL ? snapshot->edges : NULL; }
const GPtrArray *core_graph_snapshot_get_capabilities(
    const CoreGraphSnapshot *snapshot)
{ return snapshot != NULL ? snapshot->capabilities : NULL; }
const CoreGraphNodeView *core_graph_node_get_view(const CoreGraphNode *node)
{ return node != NULL ? &node->view : NULL; }
const CoreGraphEdgeView *core_graph_edge_get_view(const CoreGraphEdge *edge)
{ return edge != NULL ? &edge->view : NULL; }
const CoreGraphCapabilityView *core_graph_capability_get_view(
    const CoreGraphCapability *capability)
{ return capability != NULL ? &capability->view : NULL; }
