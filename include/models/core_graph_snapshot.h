/******************************************************************************
 * @file core_graph_snapshot.h
 * @brief Projection unifiée et indépendante de SQLite/GTK/Web.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_CORE_GRAPH_SNAPSHOT_H
#define LABFY_INVESTIGATION_CORE_GRAPH_SNAPSHOT_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct CoreGraphSnapshot CoreGraphSnapshot;
typedef struct CoreGraphNode CoreGraphNode;
typedef struct CoreGraphEdge CoreGraphEdge;
typedef struct CoreGraphCapability CoreGraphCapability;

/** Bornes locales d'une collecte. Une limite atteinte produit une erreur. */
typedef struct
{
    guint max_nodes;
    guint max_edges;
    gsize max_serialized_bytes;
} CoreGraphLimits;

/**
 * Vue d'entrée/sortie d'un nœud. Les entrées sont copiées par le modèle ; les
 * vues retournées par les getters restent empruntées au snapshot.
 */
typedef struct
{
    const char *id;
    const char *object_kind;
    const char *object_id;
    const char *type;
    const char *label;
    const char *state;
    const char *group;
    const char *raw_excerpt;
    const char *value_raw;
    const char *value_normalized;
    const char *value_corrected;
    const char *review_state;
    const char *provenance_kind;
    const char *source_header;
    const char *entity_id;
    const char *promotion_kind;
    const char *tool_id;
    const char *tool_version;
    const char *started_at;
    const char *finished_at;
    const char *collected_at;
    const char *imported_at;
    const char *observed_at;
    const char *integrated_at;
    const char *missing_provenance_reason;
    gboolean provenance_complete;
} CoreGraphNodeView;

/** Vue d'entrée/sortie d'une arête stable et orientée. */
typedef struct
{
    const char *id;
    const char *source;
    const char *target;
    const char *kind;
    const char *semantic;
    const char *review_state;
    const char *disposition;
    gboolean directed;
} CoreGraphEdgeView;

/** Vue d'une capability de navigation, jamais d'exécution réseau. */
typedef struct
{
    const char *node_id;
    const char *object_kind;
    const char *object_id;
    const char *capability_id;
    const char *reason;
    gboolean available;
} CoreGraphCapabilityView;

typedef enum
{
    CORE_GRAPH_SNAPSHOT_ERROR_INVALID_ARGUMENT,
    CORE_GRAPH_SNAPSHOT_ERROR_MEMORY,
    CORE_GRAPH_SNAPSHOT_ERROR_DUPLICATE,
    CORE_GRAPH_SNAPSHOT_ERROR_MISSING_REFERENCE,
    CORE_GRAPH_SNAPSHOT_ERROR_LIMIT
} CoreGraphSnapshotError;

#define CORE_GRAPH_SNAPSHOT_ERROR core_graph_snapshot_error_quark()
GQuark core_graph_snapshot_error_quark(void);

/**
 * Crée un snapshot possédant toutes les chaînes et collections ajoutées.
 * investigation_id est le contexte persistant des object_ref ; label est une
 * présentation et ne participe jamais à leur identité.
 */
CoreGraphSnapshot *core_graph_snapshot_new(
    const char *investigation_id,
    const char *investigation_label,
    guint schema_version,
    CoreGraphLimits limits,
    GError **error
);
void core_graph_snapshot_free(CoreGraphSnapshot *snapshot);

gboolean core_graph_snapshot_add_node(
    CoreGraphSnapshot *snapshot,
    const CoreGraphNodeView *view,
    GError **error
);
gboolean core_graph_snapshot_add_edge(
    CoreGraphSnapshot *snapshot,
    const CoreGraphEdgeView *view,
    GError **error
);
gboolean core_graph_snapshot_add_capability(
    CoreGraphSnapshot *snapshot,
    const CoreGraphCapabilityView *view,
    GError **error
);

/** Trie nœuds, arêtes et capabilities par identités stables. */
void core_graph_snapshot_sort(CoreGraphSnapshot *snapshot);

/**
 * Sélectionne le contrat de transport expérimental (2 ou 3). La v3 est requise
 * uniquement lorsque la projection contient des objets `extraction`.
 */
gboolean core_graph_snapshot_set_transport_version(
    CoreGraphSnapshot *snapshot,
    guint version,
    GError **error);

const char *core_graph_snapshot_get_investigation_id(
    const CoreGraphSnapshot *snapshot);
const char *core_graph_snapshot_get_investigation_label(
    const CoreGraphSnapshot *snapshot);
guint core_graph_snapshot_get_schema_version(const CoreGraphSnapshot *snapshot);
guint core_graph_snapshot_get_transport_version(
    const CoreGraphSnapshot *snapshot);
CoreGraphLimits core_graph_snapshot_get_limits(const CoreGraphSnapshot *snapshot);
const GPtrArray *core_graph_snapshot_get_nodes(const CoreGraphSnapshot *snapshot);
const GPtrArray *core_graph_snapshot_get_edges(const CoreGraphSnapshot *snapshot);
const GPtrArray *core_graph_snapshot_get_capabilities(
    const CoreGraphSnapshot *snapshot);
const CoreGraphNodeView *core_graph_node_get_view(const CoreGraphNode *node);
const CoreGraphEdgeView *core_graph_edge_get_view(const CoreGraphEdge *edge);
const CoreGraphCapabilityView *core_graph_capability_get_view(
    const CoreGraphCapability *capability);

G_END_DECLS

#endif
