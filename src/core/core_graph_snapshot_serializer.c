/******************************************************************************
 * @file core_graph_snapshot_serializer.c
 * @brief Transport JSON fiable et borné pour le prototype Web Graph.
 ******************************************************************************/

#include "core/core_graph_snapshot_serializer.h"
#include "core/local_capability_registry.h"

#include <glib/gstdio.h>
#include <json-glib/json-glib.h>
#include <errno.h>
#include <string.h>

GQuark core_graph_serializer_error_quark(void)
{
    return g_quark_from_static_string("core-graph-serializer-error-quark");
}

static void core_graph_serializer_set_error(
    GError **error,
    CoreGraphSerializerError code,
    const char *message)
{
    if (error != NULL && *error == NULL)
        g_set_error_literal(error, CORE_GRAPH_SERIALIZER_ERROR, code, message);
}

static gboolean core_graph_serializer_validate_text(
    const char *value,
    GError **error)
{
    if (value == NULL || g_utf8_validate(value, -1, NULL)) return TRUE;
    core_graph_serializer_set_error(error,
        CORE_GRAPH_SERIALIZER_ERROR_INVALID_UTF8,
        "Le snapshot contient une chaîne qui n'est pas un UTF-8 valide.");
    return FALSE;
}

static gboolean core_graph_serializer_validate(
    const CoreGraphSnapshot *snapshot,
    GError **error)
{
    const GPtrArray *nodes = core_graph_snapshot_get_nodes(snapshot);
    const GPtrArray *edges = core_graph_snapshot_get_edges(snapshot);
    const GPtrArray *capabilities = core_graph_snapshot_get_capabilities(snapshot);
    if (!core_graph_serializer_validate_text(
            core_graph_snapshot_get_investigation_id(snapshot), error) ||
        !core_graph_serializer_validate_text(
            core_graph_snapshot_get_investigation_label(snapshot), error))
        return FALSE;
    for (guint index = 0; index < nodes->len; index++)
    {
        const CoreGraphNodeView *view = core_graph_node_get_view(
            g_ptr_array_index((GPtrArray *) nodes, index));
        const char *values[] = { view->id, view->object_kind, view->object_id,
            view->type, view->label, view->state, view->group,
            view->raw_excerpt, view->value_raw, view->value_normalized,
            view->value_corrected, view->review_state, view->provenance_kind,
            view->source_header, view->entity_id, view->promotion_kind,
            view->tool_id, view->tool_version, view->started_at,
            view->finished_at, view->collected_at, view->imported_at,
            view->observed_at, view->integrated_at,
            view->missing_provenance_reason };
        for (guint value = 0; value < G_N_ELEMENTS(values); value++)
            if (!core_graph_serializer_validate_text(values[value], error))
                return FALSE;
    }
    for (guint index = 0; index < edges->len; index++)
    {
        const CoreGraphEdgeView *view = core_graph_edge_get_view(
            g_ptr_array_index((GPtrArray *) edges, index));
        const char *values[] = { view->id, view->source, view->target,
            view->kind, view->semantic, view->review_state, view->disposition };
        for (guint value = 0; value < G_N_ELEMENTS(values); value++)
            if (!core_graph_serializer_validate_text(values[value], error))
                return FALSE;
    }
    for (guint index = 0; index < capabilities->len; index++)
    {
        const CoreGraphCapabilityView *view = core_graph_capability_get_view(
            g_ptr_array_index((GPtrArray *) capabilities, index));
        const char *values[] = { view->node_id, view->object_kind,
            view->object_id, view->capability_id, view->reason };
        for (guint value = 0; value < G_N_ELEMENTS(values); value++)
            if (!core_graph_serializer_validate_text(values[value], error))
                return FALSE;
    }
    return TRUE;
}

static void core_graph_json_string_member(
    JsonBuilder *builder,
    const char *name,
    const char *value)
{
    json_builder_set_member_name(builder, name);
    if (value != NULL) json_builder_add_string_value(builder, value);
    else json_builder_add_null_value(builder);
}

static void core_graph_serializer_add_node(
    JsonBuilder *builder,
    const char *investigation_id,
    const CoreGraphNodeView *view)
{
    json_builder_begin_object(builder);
    core_graph_json_string_member(builder, "id", view->id);
    core_graph_json_string_member(builder, "object_kind", view->object_kind);
    core_graph_json_string_member(builder, "object_id", view->object_id);
    core_graph_json_string_member(builder, "type", view->type);
    core_graph_json_string_member(builder, "label", view->label);
    core_graph_json_string_member(builder, "state", view->state);
    core_graph_json_string_member(builder, "group", view->group);
    core_graph_json_string_member(builder, "raw", view->raw_excerpt);
    json_builder_set_member_name(builder, "object_ref");
    json_builder_begin_object(builder);
    core_graph_json_string_member(builder, "investigation_id", investigation_id);
    core_graph_json_string_member(builder, "object_kind", view->object_kind);
    core_graph_json_string_member(builder, "object_id", view->object_id);
    json_builder_end_object(builder);
    json_builder_set_member_name(builder, "details");
    json_builder_begin_object(builder);
    core_graph_json_string_member(builder, "value_raw", view->value_raw);
    core_graph_json_string_member(builder, "value_normalized",
        view->value_normalized);
    core_graph_json_string_member(builder, "value_corrected",
        view->value_corrected);
    core_graph_json_string_member(builder, "review_state", view->review_state);
    core_graph_json_string_member(builder, "provenance_kind",
        view->provenance_kind);
    core_graph_json_string_member(builder, "source_header", view->source_header);
    core_graph_json_string_member(builder, "promoted_entity_id", view->entity_id);
    core_graph_json_string_member(builder, "promotion_kind", view->promotion_kind);
    core_graph_json_string_member(builder, "tool_id", view->tool_id);
    core_graph_json_string_member(builder, "tool_version", view->tool_version);
    core_graph_json_string_member(builder, "started_at", view->started_at);
    core_graph_json_string_member(builder, "finished_at", view->finished_at);
    core_graph_json_string_member(builder, "collected_at", view->collected_at);
    core_graph_json_string_member(builder, "imported_at", view->imported_at);
    core_graph_json_string_member(builder, "observed_at", view->observed_at);
    core_graph_json_string_member(builder, "integrated_at", view->integrated_at);
    json_builder_set_member_name(builder, "provenance_complete");
    json_builder_add_boolean_value(builder, view->provenance_complete);
    core_graph_json_string_member(builder, "missing_provenance_reason",
        view->missing_provenance_reason);
    json_builder_end_object(builder);
    json_builder_end_object(builder);
}

static void core_graph_serializer_add_edge(
    JsonBuilder *builder,
    const CoreGraphEdgeView *view)
{
    json_builder_begin_object(builder);
    core_graph_json_string_member(builder, "id", view->id);
    core_graph_json_string_member(builder, "source", view->source);
    core_graph_json_string_member(builder, "target", view->target);
    core_graph_json_string_member(builder, "kind", view->kind);
    core_graph_json_string_member(builder, "semantic", view->semantic);
    core_graph_json_string_member(builder, "review_state", view->review_state);
    core_graph_json_string_member(builder, "disposition", view->disposition);
    json_builder_set_member_name(builder, "directed");
    json_builder_add_boolean_value(builder, view->directed);
    json_builder_end_object(builder);
}

static void core_graph_serializer_add_catalog(JsonBuilder *builder)
{
    static const struct
    {
        const char *id;
        const char *intent;
        const char *contact;
    } catalog[] = {
        {"focus-neighborhood", "Explorer le voisinage", "NONE"},
        {"show-provenance", "Remonter aux sources", "NONE"},
        {"rdap", "Interroger RDAP", "THIRD_PARTY"},
        {LOCAL_CAPABILITY_EML_HEADERS, "Analyser les en-têtes EML", "NONE"},
        {LOCAL_CAPABILITY_EXIF_METADATA, "Examiner les métadonnées", "NONE"}
    };
    json_builder_set_member_name(builder, "capability_catalog");
    json_builder_begin_array(builder);
    for (guint index = 0; index < G_N_ELEMENTS(catalog); index++)
    {
        json_builder_begin_object(builder);
        core_graph_json_string_member(builder, "id", catalog[index].id);
        core_graph_json_string_member(builder, "intent", catalog[index].intent);
        core_graph_json_string_member(builder, "network_contact",
            catalog[index].contact);
        json_builder_set_member_name(builder, "experimental");
        json_builder_add_boolean_value(builder, TRUE);
        json_builder_end_object(builder);
    }
    json_builder_end_array(builder);
}

char *core_graph_snapshot_serialize(
    const CoreGraphSnapshot *snapshot,
    gboolean synthetic_fixture,
    gsize *out_length,
    GError **error)
{
    JsonBuilder *builder = NULL;
    JsonGenerator *generator = NULL;
    JsonNode *root = NULL;
    char *data = NULL;
    const GPtrArray *nodes = NULL;
    const GPtrArray *edges = NULL;
    const GPtrArray *capabilities = NULL;
    CoreGraphLimits limits = {0};
    guint transport_version = 0U;
    char *contract = NULL;
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);
    if (out_length != NULL) *out_length = 0;
    if (snapshot == NULL)
    {
        core_graph_serializer_set_error(error,
            CORE_GRAPH_SERIALIZER_ERROR_INVALID_ARGUMENT,
            "Le snapshot du graphe est absent.");
        return NULL;
    }
    if (!core_graph_serializer_validate(snapshot, error)) return NULL;
    nodes = core_graph_snapshot_get_nodes(snapshot);
    edges = core_graph_snapshot_get_edges(snapshot);
    capabilities = core_graph_snapshot_get_capabilities(snapshot);
    limits = core_graph_snapshot_get_limits(snapshot);
    transport_version = core_graph_snapshot_get_transport_version(snapshot);
    if (transport_version != 2U && transport_version != 3U)
    {
        core_graph_serializer_set_error(error,
            CORE_GRAPH_SERIALIZER_ERROR_INVALID_ARGUMENT,
            "La version de transport du snapshot n'est pas prise en charge.");
        return NULL;
    }
    contract = g_strdup_printf("labfy.web_graph.snapshot.v%u",
        transport_version);
    if (contract == NULL) goto generation_failure;
    builder = json_builder_new();
    generator = json_generator_new();
    if (builder == NULL || generator == NULL) goto generation_failure;
    json_builder_begin_object(builder);
    core_graph_json_string_member(builder, "contract", contract);
    core_graph_json_string_member(builder, "origin", "core");
    json_builder_set_member_name(builder, "transport_revision");
    json_builder_add_int_value(builder, 1);
    json_builder_set_member_name(builder, "revision");
    json_builder_add_int_value(builder, 1);
    json_builder_set_member_name(builder, "schema_version");
    json_builder_add_int_value(builder,
        core_graph_snapshot_get_schema_version(snapshot));
    json_builder_set_member_name(builder, "complete");
    json_builder_add_boolean_value(builder, TRUE);
    json_builder_set_member_name(builder, "investigation");
    json_builder_begin_object(builder);
    core_graph_json_string_member(builder, "id",
        core_graph_snapshot_get_investigation_id(snapshot));
    core_graph_json_string_member(builder, "label",
        core_graph_snapshot_get_investigation_label(snapshot));
    json_builder_set_member_name(builder, "synthetic");
    json_builder_add_boolean_value(builder, synthetic_fixture);
    json_builder_end_object(builder);
    json_builder_set_member_name(builder, "limits");
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "max_nodes");
    json_builder_add_int_value(builder, limits.max_nodes);
    json_builder_set_member_name(builder, "max_edges");
    json_builder_add_int_value(builder, limits.max_edges);
    json_builder_set_member_name(builder, "max_serialized_bytes");
    json_builder_add_int_value(builder, (gint64) limits.max_serialized_bytes);
    json_builder_set_member_name(builder, "truncated");
    json_builder_add_boolean_value(builder, FALSE);
    json_builder_end_object(builder);
    json_builder_set_member_name(builder, "presentation");
    json_builder_begin_object(builder);
    core_graph_json_string_member(builder, "coordinates_source", "web_bridge");
    json_builder_end_object(builder);
    json_builder_set_member_name(builder, "nodes");
    json_builder_begin_array(builder);
    for (guint index = 0; index < nodes->len; index++)
        core_graph_serializer_add_node(builder,
            core_graph_snapshot_get_investigation_id(snapshot),
            core_graph_node_get_view(g_ptr_array_index((GPtrArray *) nodes, index)));
    json_builder_end_array(builder);
    json_builder_set_member_name(builder, "edges");
    json_builder_begin_array(builder);
    for (guint index = 0; index < edges->len; index++)
        core_graph_serializer_add_edge(builder,
            core_graph_edge_get_view(g_ptr_array_index((GPtrArray *) edges, index)));
    json_builder_end_array(builder);
    core_graph_serializer_add_catalog(builder);
    json_builder_set_member_name(builder, "capabilities");
    json_builder_begin_array(builder);
    for (guint index = 0; index < capabilities->len; index++)
    {
        const CoreGraphCapabilityView *view = core_graph_capability_get_view(
            g_ptr_array_index((GPtrArray *) capabilities, index));
        json_builder_begin_object(builder);
        core_graph_json_string_member(builder, "node_id", view->node_id);
        json_builder_set_member_name(builder, "object_ref");
        json_builder_begin_object(builder);
        core_graph_json_string_member(builder, "investigation_id",
            core_graph_snapshot_get_investigation_id(snapshot));
        core_graph_json_string_member(builder, "object_kind", view->object_kind);
        core_graph_json_string_member(builder, "object_id", view->object_id);
        json_builder_end_object(builder);
        core_graph_json_string_member(builder, "capability_id",
            view->capability_id);
        json_builder_set_member_name(builder, "available");
        json_builder_add_boolean_value(builder, view->available);
        core_graph_json_string_member(builder, "reason", view->reason);
        json_builder_end_object(builder);
    }
    json_builder_end_array(builder);
    json_builder_end_object(builder);
    root = json_builder_get_root(builder);
    if (root == NULL) goto generation_failure;
    json_generator_set_root(generator, root);
    data = json_generator_to_data(generator, out_length);
    if (data == NULL) goto generation_failure;
    if ((out_length != NULL ? *out_length : strlen(data)) >
        limits.max_serialized_bytes)
    {
        g_clear_pointer(&data, g_free);
        core_graph_serializer_set_error(error, CORE_GRAPH_SERIALIZER_ERROR_LIMIT,
            "Le snapshot dépasse la limite d'octets sérialisés.");
    }
    json_node_unref(root);
    g_object_unref(generator);
    g_object_unref(builder);
    g_free(contract);
    return data;

generation_failure:
    if (root != NULL) json_node_unref(root);
    g_clear_object(&generator);
    g_clear_object(&builder);
    g_free(contract);
    core_graph_serializer_set_error(error,
        CORE_GRAPH_SERIALIZER_ERROR_GENERATION,
        "JSON-GLib n'a pas pu générer le snapshot v2.");
    return NULL;
}

gboolean core_graph_snapshot_write_atomic(
    const CoreGraphSnapshot *snapshot,
    gboolean synthetic_fixture,
    const char *output_path,
    GError **error)
{
    char *data = NULL;
    char *temporary_path = NULL;
    char *suffix = NULL;
    gsize length = 0;
    gboolean success = FALSE;
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    if (output_path == NULL || output_path[0] == '\0')
    {
        core_graph_serializer_set_error(error,
            CORE_GRAPH_SERIALIZER_ERROR_INVALID_ARGUMENT,
            "Le chemin de publication du snapshot est obligatoire.");
        return FALSE;
    }
    data = core_graph_snapshot_serialize(snapshot, synthetic_fixture, &length,
        error);
    if (data == NULL) return FALSE;
    suffix = g_uuid_string_random();
    temporary_path = g_strdup_printf("%s.stage-%s", output_path, suffix);
    if (suffix == NULL || temporary_path == NULL ||
        !g_file_set_contents(temporary_path, data, (gssize) length, error) ||
        g_chmod(temporary_path, 0600) != 0 ||
        g_rename(temporary_path, output_path) != 0)
    {
        if (error != NULL && *error == NULL)
            g_set_error(error, CORE_GRAPH_SERIALIZER_ERROR,
                CORE_GRAPH_SERIALIZER_ERROR_PUBLICATION,
                "Impossible de publier atomiquement le snapshot : %s",
                g_strerror(errno));
        if (temporary_path != NULL) g_remove(temporary_path);
    }
    else success = TRUE;
    g_free(temporary_path);
    g_free(suffix);
    g_free(data);
    return success;
}
