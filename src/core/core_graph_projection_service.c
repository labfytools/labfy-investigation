/******************************************************************************
 * @file core_graph_projection_service.c
 * @brief Projection typée de l'état V20 vers un graphe unifié.
 ******************************************************************************/

#include "core/core_graph_projection_service.h"

#include "core/eml_analysis_persistence_service.h"
#include "core/investigation_graph_loader.h"
#include "core/local_capability_registry.h"
#include "dao/evidence_dao.h"
#include "dao/evidence_entity_dao.h"
#include "dao/extraction_dao.h"
#include "dao/investigation_dao.h"
#include "dao/osint_execution_dao.h"
#include "database/error.h"
#include "database/transaction.h"
#include "models/entity_record.h"
#include "models/evidence_observation.h"
#include "models/evidence_record.h"
#include "models/investigation_graph_model.h"
#include "models/investigation_record.h"
#include "models/osint_execution_record.h"
#include "models/relation_record.h"

#define CORE_GRAPH_RUNTIME_SCHEMA_VERSION 20U
#define CORE_GRAPH_RAW_EXCERPT_LIMIT 512U

GQuark core_graph_projection_error_quark(void) {
  return g_quark_from_static_string("core-graph-projection-error-quark");
}

static void core_graph_projection_set_error(GError **error,
                                            CoreGraphProjectionError code,
                                            const char *context,
                                            const GError *nested) {
  if (error == NULL || *error != NULL)
    return;
  g_set_error(error, CORE_GRAPH_PROJECTION_ERROR, code, "%s%s%s", context,
              nested != NULL ? " : " : "",
              nested != NULL ? nested->message : "");
}

static char *core_graph_projection_ref(const char *kind,
                                       const char *identifier) {
  return g_strdup_printf("%s:%s", kind, identifier);
}

static const char *core_graph_entity_state(EntityStatus status) {
  switch (status) {
  case ENTITY_STATUS_ACTIVE:
    return "active";
  case ENTITY_STATUS_ARCHIVED:
    return "archived";
  case ENTITY_STATUS_DELETED:
    return "deleted";
  default:
    return "unknown";
  }
}

static const char *core_graph_relation_state(RelationStatus status) {
  switch (status) {
  case RELATION_STATUS_ACTIVE:
    return "active";
  case RELATION_STATUS_ARCHIVED:
    return "archived";
  case RELATION_STATUS_DELETED:
    return "deleted";
  case RELATION_STATUS_DISPUTED:
    return "disputed";
  default:
    return "unknown";
  }
}

static const char *core_graph_evidence_state(EvidenceIntegrityStatus status) {
  switch (status) {
  case EVIDENCE_INTEGRITY_STATUS_VALID:
    return "valid";
  case EVIDENCE_INTEGRITY_STATUS_MISSING:
    return "missing";
  case EVIDENCE_INTEGRITY_STATUS_MODIFIED:
    return "modified";
  case EVIDENCE_INTEGRITY_STATUS_ERROR:
    return "error";
  default:
    return "unknown";
  }
}

static gboolean core_graph_projection_add_navigation(
    CoreGraphSnapshot *snapshot, const char *node_id, const char *object_kind,
    const char *object_id, gboolean provenance, GError **error) {
  CoreGraphCapabilityView focus = {
      .node_id = node_id,
      .object_kind = object_kind,
      .object_id = object_id,
      .capability_id = "focus-neighborhood",
      .reason = "Navigation locale dans le snapshot du cœur",
      .available = TRUE};
  CoreGraphCapabilityView why = {
      .node_id = node_id,
      .object_kind = object_kind,
      .object_id = object_id,
      .capability_id = "show-provenance",
      .reason = "Provenance persistée disponible, avec lacunes signalées",
      .available = TRUE};
  if (!core_graph_snapshot_add_capability(snapshot, &focus, error))
    return FALSE;
  return !provenance ||
         core_graph_snapshot_add_capability(snapshot, &why, error);
}

static gboolean
core_graph_projection_add_entities(CoreGraphSnapshot *snapshot,
                                   const InvestigationGraphModel *graph,
                                   GError **error) {
  GPtrArray *entities = investigation_graph_model_list_entities(graph, error);
  if (entities == NULL)
    return FALSE;
  for (guint index = 0; index < entities->len; index++) {
    const EntityRecord *record = g_ptr_array_index(entities, index);
    const char *identifier = entity_record_get_identifier(record);
    const char *type = entity_record_get_type_identifier(record);
    const char *label = entity_record_get_label(record);
    char *node_id = core_graph_projection_ref("entity", identifier);
    CoreGraphNodeView node = {
        .id = node_id,
        .object_kind = "entity",
        .object_id = identifier,
        .type = type,
        .label = label != NULL ? label : entity_record_get_value(record),
        .state = core_graph_entity_state(entity_record_get_status(record)),
        .group = "identity",
        .provenance_complete = FALSE,
        .missing_provenance_reason = "Le runtime V20 ne relie pas uniformément "
                                     "chaque entité à une source."};
    gboolean success =
        node_id != NULL &&
        core_graph_snapshot_add_node(snapshot, &node, error) &&
        core_graph_projection_add_navigation(snapshot, node_id, "entity",
                                             identifier, FALSE, error);
    if (success && (g_strcmp0(type, "domain") == 0 ||
                    g_strcmp0(type, "domain_name") == 0)) {
      CoreGraphCapabilityView rdap = {
          .node_id = node_id,
          .object_kind = "entity",
          .object_id = identifier,
          .capability_id = "rdap",
          .reason = "Non raccordé dans ce lot ; aucun contact réseau",
          .available = FALSE};
      success = core_graph_snapshot_add_capability(snapshot, &rdap, error);
    }
    g_free(node_id);
    if (!success) {
      g_ptr_array_unref(entities);
      return FALSE;
    }
  }
  g_ptr_array_unref(entities);
  return TRUE;
}

static gboolean
core_graph_projection_add_relations(CoreGraphSnapshot *snapshot,
                                    const InvestigationGraphModel *graph,
                                    GError **error) {
  GPtrArray *relations = investigation_graph_model_list_relations(graph, error);
  if (relations == NULL)
    return FALSE;
  for (guint index = 0; index < relations->len; index++) {
    const RelationRecord *record = g_ptr_array_index(relations, index);
    char *source = core_graph_projection_ref(
        "entity", relation_record_get_source_entity_identifier(record));
    char *target = core_graph_projection_ref(
        "entity", relation_record_get_target_entity_identifier(record));
    char *edge_id = core_graph_projection_ref(
        "relation", relation_record_get_identifier(record));
    CoreGraphEdgeView edge = {
        .id = edge_id,
        .source = source,
        .target = target,
        .kind = "business",
        .semantic = relation_record_get_relation_type(record),
        .review_state =
            core_graph_relation_state(relation_record_get_status(record)),
        .directed = TRUE};
    gboolean success = source != NULL && target != NULL && edge_id != NULL &&
                       core_graph_snapshot_add_edge(snapshot, &edge, error);
    g_free(edge_id);
    g_free(target);
    g_free(source);
    if (!success) {
      g_ptr_array_unref(relations);
      return FALSE;
    }
  }
  g_ptr_array_unref(relations);
  return TRUE;
}

static gboolean core_graph_projection_add_observation(
    CoreGraphSnapshot *snapshot, const EvidenceObservation *observation,
    const char *evidence_node_id, const GHashTable *extraction_ids,
    const GHashTable *coherent_extraction_ids, GError **error) {
  char *node_id =
      core_graph_projection_ref("observation", observation->identifier);
  char *edge_id =
      core_graph_projection_ref("observation-source", observation->identifier);
  const gboolean promoted = observation->entity_identifier != NULL;
  const gboolean has_extraction =
      observation->extraction_identifier != NULL &&
      g_hash_table_contains((GHashTable *)extraction_ids,
                            observation->extraction_identifier);
  const gboolean coherent_extraction =
      has_extraction &&
      g_hash_table_contains((GHashTable *)coherent_extraction_ids,
                            observation->extraction_identifier);
  CoreGraphNodeView node = {
      .id = node_id,
      .object_kind = "observation",
      .object_id = observation->identifier,
      .type = observation->type_identifier,
      .label = observation->value,
      .state = promoted ? "promoted" : "unpromoted",
      .group = "evidence",
      .raw_excerpt = observation->value_raw,
      .value_raw = observation->value_raw,
      .value_normalized = observation->value_normalized,
      .value_corrected = observation->value_corrected,
      .review_state = observation->verification_status,
      .provenance_kind = observation->provenance_kind,
      .source_header = observation->source_header,
      .entity_id = observation->entity_identifier,
      .promotion_kind = observation->promotion_kind,
      .observed_at = observation->observed_at,
      .integrated_at = observation->integrated_at,
      .provenance_complete = coherent_extraction,
      .missing_provenance_reason =
          observation->extraction_identifier == NULL
              ? "Aucune transformation identifiée n'est persistée pour cette "
                "observation."
              : (!has_extraction
                     ? "La référence d'extraction persistée est absente."
                     : (!coherent_extraction ? "L'extraction ne relie pas une "
                                               "source et un dérivé cohérents."
                                             : NULL))};
  CoreGraphEdgeView source_edge = {.id = edge_id,
                                   .source = evidence_node_id,
                                   .target = node_id,
                                   .kind = "provenance",
                                   .semantic = "evidence_observation",
                                   .review_state =
                                       observation->verification_status,
                                   .directed = TRUE};
  gboolean success =
      node_id != NULL && edge_id != NULL &&
      core_graph_snapshot_add_node(snapshot, &node, error) &&
      core_graph_projection_add_navigation(snapshot, node_id, "observation",
                                           observation->identifier, TRUE,
                                           error) &&
      core_graph_snapshot_add_edge(snapshot, &source_edge, error);
  if (success && promoted) {
    char *target =
        core_graph_projection_ref("entity", observation->entity_identifier);
    char *promotion_id = core_graph_projection_ref("observation-promotion",
                                                   observation->identifier);
    CoreGraphEdgeView promotion = {.id = promotion_id,
                                   .source = node_id,
                                   .target = target,
                                   .kind = "provenance",
                                   .semantic = "explicit_promotion",
                                   .review_state =
                                       observation->verification_status,
                                   .disposition = observation->promotion_kind,
                                   .directed = TRUE};
    success = target != NULL && promotion_id != NULL &&
              core_graph_snapshot_add_edge(snapshot, &promotion, error);
    g_free(promotion_id);
    g_free(target);
  }
  if (success && has_extraction) {
    char *source = core_graph_projection_ref(
        "extraction", observation->extraction_identifier);
    char *provenance_id = g_strdup_printf("extraction-observation:%s:%s",
                                          observation->extraction_identifier,
                                          observation->identifier);
    CoreGraphEdgeView provenance = {.id = provenance_id,
                                    .source = source,
                                    .target = node_id,
                                    .kind = "provenance",
                                    .semantic = "analysis_observation",
                                    .review_state =
                                        observation->verification_status,
                                    .disposition = "calculated",
                                    .directed = TRUE};
    success = source != NULL && provenance_id != NULL &&
              core_graph_snapshot_add_edge(snapshot, &provenance, error);
    g_free(provenance_id);
    g_free(source);
  }
  g_free(edge_id);
  g_free(node_id);
  return success;
}

static gboolean core_graph_projection_add_evidence_nodes(
    CoreGraphSnapshot *snapshot, EvidenceDao *evidence_dao,
    EvidenceEntityDao *association_dao, const GHashTable *derived_evidence_ids,
    GHashTable *evidence_ids, const LocalCapabilityRegistry *registry,
    gboolean operational, GError **error) {
  GPtrArray *records = evidence_dao_list_all(evidence_dao, error);
  if (records == NULL)
    return FALSE;
  for (guint index = 0; index < records->len; index++) {
    const EvidenceRecord *record = g_ptr_array_index(records, index);
    const char *identifier = evidence_record_get_identifier(record);
    char *node_id = core_graph_projection_ref("evidence", identifier);
    const gboolean derived =
        g_hash_table_contains((GHashTable *)derived_evidence_ids, identifier);
    CoreGraphNodeView node = {
        .id = node_id,
        .object_kind = "evidence",
        .object_id = identifier,
        .type = evidence_record_get_type_identifier(record),
        .label = evidence_record_get_original_name(record),
        .state = derived ? "derived"
                         : core_graph_evidence_state(
                               evidence_record_get_integrity_status(record)),
        .group = "evidence",
        .raw_excerpt = evidence_record_get_description(record),
        .collected_at = evidence_record_get_collected_at(record),
        .imported_at = evidence_record_get_imported_at(record),
        .provenance_complete = evidence_record_get_source(record) != NULL,
        .missing_provenance_reason =
            evidence_record_get_source(record) == NULL
                ? "La source déclarée de la preuve est absente."
                : NULL};
    gboolean success =
        node_id != NULL &&
        core_graph_snapshot_add_node(snapshot, &node, error) &&
        core_graph_projection_add_navigation(snapshot, node_id, "evidence",
                                             identifier, TRUE, error);
    if (success && !derived && registry != NULL) {
      const char *type = evidence_record_get_type_identifier(record);
      const char *mime = g_strcmp0(type, "email") == 0 ? "message/rfc822"
                         : g_strcmp0(type, "photo") == 0
                             ? "image/unknown"
                             : "application/octet-stream";
      const char *capability_id =
          g_strcmp0(type, "email") == 0   ? LOCAL_CAPABILITY_EML_HEADERS
          : g_strcmp0(type, "photo") == 0 ? LOCAL_CAPABILITY_EXIF_METADATA
                                          : NULL;
      const LocalCapabilityStatus *status =
          capability_id != NULL
              ? local_capability_registry_lookup(registry, capability_id)
              : NULL;
      char *application_reason = NULL;
      gboolean applies =
          status != NULL &&
          local_capability_status_applies(
              status, mime, evidence_record_get_relative_path(record),
              &application_reason);
      if (applies) {
        char *reason = g_strdup_printf(
            operational ? "%s Commande locale J6 autorisée ; état outil : %s."
                        : "%s Exécution par le lanceur local ; client HTTP en "
                          "lecture seule. État outil : %s.",
            application_reason,
            local_capability_availability_code(status->availability));
        CoreGraphCapabilityView capability = {
            .node_id = node_id,
            .object_kind = "evidence",
            .object_id = identifier,
            .capability_id = capability_id,
            .reason = reason,
            .available =
                operational && status->availability == LOCAL_CAPABILITY_READY};
        success =
            core_graph_snapshot_add_capability(snapshot, &capability, error);
        g_free(reason);
      }
      g_free(application_reason);
    }
    if (success)
      g_hash_table_add(evidence_ids, g_strdup(identifier));
    GPtrArray *entities = success ? evidence_entity_dao_list_entity_identifiers(
                                        association_dao, identifier, error)
                                  : NULL;
    if (!success || entities == NULL) {
      g_free(node_id);
      g_ptr_array_unref(records);
      return FALSE;
    }
    for (guint link = 0; link < entities->len && success; link++) {
      const char *entity_id = g_ptr_array_index(entities, link);
      char *target = core_graph_projection_ref("entity", entity_id);
      char *edge_id =
          g_strdup_printf("evidence-attachment:%s:%s", identifier, entity_id);
      CoreGraphEdgeView attachment = {.id = edge_id,
                                      .source = node_id,
                                      .target = target,
                                      .kind = "support",
                                      .semantic = "evidence_attachment",
                                      .review_state = "persisted",
                                      .directed = TRUE};
      success = target != NULL && edge_id != NULL &&
                core_graph_snapshot_add_edge(snapshot, &attachment, error);
      g_free(edge_id);
      g_free(target);
    }
    g_ptr_array_unref(entities);
    g_free(node_id);
    if (!success) {
      g_ptr_array_unref(records);
      return FALSE;
    }
  }
  g_ptr_array_unref(records);
  return TRUE;
}

static gboolean core_graph_projection_add_extractions(
    CoreGraphSnapshot *snapshot, const GPtrArray *records,
    const GHashTable *evidence_ids, GHashTable *extraction_ids,
    GHashTable *coherent_extraction_ids, GError **error) {
  for (guint index = 0U; index < records->len; index++) {
    const ExtractionRecord *record =
        g_ptr_array_index((GPtrArray *)records, index);
    char *node_id = core_graph_projection_ref("extraction", record->identifier);
    gboolean source_exists = FALSE;
    gboolean output_exists = record->evidence_identifier != NULL &&
                             g_hash_table_contains((GHashTable *)evidence_ids,
                                                   record->evidence_identifier);
    if (g_strcmp0(record->source_kind, "evidence") == 0)
      source_exists = g_hash_table_contains((GHashTable *)evidence_ids,
                                            record->source_identifier);
    const gboolean eml =
        g_strcmp0(record->tool_identifier, EML_ANALYSIS_TOOL_ID) == 0;
    const gboolean exiftool =
        g_str_has_prefix(record->tool_identifier, "exiftool@");
    const char *tool_version =
        exiftool ? strchr(record->tool_identifier, '@') + 1 : NULL;
    CoreGraphNodeView node = {
        .id = node_id,
        .object_kind = "extraction",
        .object_id = record->identifier,
        .type = eml        ? "EML_EXTRACTION"
                : exiftool ? "METADATA_EXTRACTION"
                           : "EXTRACTION",
        .label = eml        ? "Analyse EML locale persistée"
                 : exiftool ? "Métadonnées ExifTool persistées"
                            : record->tool_identifier,
        .state = "persisted",
        .group = "evidence",
        .tool_id = exiftool ? "exiftool" : record->tool_identifier,
        .tool_version = eml ? EML_ANALYSIS_TOOL_VERSION : tool_version,
        .started_at = record->created_at,
        .provenance_complete = source_exists && output_exists,
        .missing_provenance_reason = source_exists && output_exists
                                         ? NULL
                                         : "La source ou la preuve dérivée de "
                                           "l'extraction est absente."};
    gboolean success =
        node_id != NULL &&
        core_graph_snapshot_add_node(snapshot, &node, error) &&
        core_graph_projection_add_navigation(snapshot, node_id, "extraction",
                                             record->identifier, TRUE, error);
    if (success)
      g_hash_table_add(extraction_ids, g_strdup(record->identifier));
    if (success && source_exists) {
      char *source = core_graph_projection_ref(record->source_kind,
                                               record->source_identifier);
      char *edge_id =
          core_graph_projection_ref("extraction-input", record->identifier);
      CoreGraphEdgeView edge = {.id = edge_id,
                                .source = source,
                                .target = node_id,
                                .kind = "provenance",
                                .semantic = "analysis_input",
                                .review_state = "persisted",
                                .directed = TRUE};
      success = source != NULL && edge_id != NULL &&
                core_graph_snapshot_add_edge(snapshot, &edge, error);
      g_free(edge_id);
      g_free(source);
    }
    if (success && output_exists) {
      char *target =
          core_graph_projection_ref("evidence", record->evidence_identifier);
      char *edge_id =
          core_graph_projection_ref("extraction-output", record->identifier);
      CoreGraphEdgeView edge = {.id = edge_id,
                                .source = node_id,
                                .target = target,
                                .kind = "provenance",
                                .semantic = "analysis_derivative",
                                .review_state = "persisted",
                                .directed = TRUE};
      success = target != NULL && edge_id != NULL &&
                core_graph_snapshot_add_edge(snapshot, &edge, error);
      g_free(edge_id);
      g_free(target);
    }
    if (success && source_exists && output_exists)
      g_hash_table_add(coherent_extraction_ids, g_strdup(record->identifier));
    g_free(node_id);
    if (!success)
      return FALSE;
  }
  if (records->len > 0U &&
      !core_graph_snapshot_set_transport_version(snapshot, 3U, error))
    return FALSE;
  return TRUE;
}

static gboolean core_graph_projection_add_observations(
    CoreGraphSnapshot *snapshot, EvidenceDao *evidence_dao,
    EvidenceEntityDao *association_dao, const GHashTable *extraction_ids,
    const GHashTable *coherent_extraction_ids, GError **error) {
  GPtrArray *records = evidence_dao_list_all(evidence_dao, error);
  if (records == NULL)
    return FALSE;
  for (guint index = 0U; index < records->len; index++) {
    const EvidenceRecord *record = g_ptr_array_index(records, index);
    const char *identifier = evidence_record_get_identifier(record);
    char *node_id = core_graph_projection_ref("evidence", identifier);
    GPtrArray *observations = evidence_entity_dao_list_observations(
        association_dao, identifier, error);
    gboolean success = node_id != NULL && observations != NULL;
    for (guint item = 0U; success && item < observations->len; item++)
      success = core_graph_projection_add_observation(
          snapshot, g_ptr_array_index(observations, item), node_id,
          extraction_ids, coherent_extraction_ids, error);
    g_clear_pointer(&observations, g_ptr_array_unref);
    g_free(node_id);
    if (!success) {
      g_ptr_array_unref(records);
      return FALSE;
    }
  }
  g_ptr_array_unref(records);
  return TRUE;
}

static char *
core_graph_projection_stdout_excerpt(const OsintExecutionRecord *record) {
  GBytes *bytes = osint_execution_record_ref_stdout(record);
  gsize length = 0;
  const guint8 *data = bytes != NULL ? g_bytes_get_data(bytes, &length) : NULL;
  char *excerpt = NULL;
  if (data != NULL && length <= CORE_GRAPH_RAW_EXCERPT_LIMIT &&
      g_utf8_validate((const char *)data, length, NULL))
    excerpt = g_strndup((const char *)data, length);
  g_clear_pointer(&bytes, g_bytes_unref);
  return excerpt;
}

static gboolean
core_graph_projection_add_executions(CoreGraphSnapshot *snapshot,
                                     OsintExecutionDao *dao, GError **error) {
  GPtrArray *records = osint_execution_dao_list_all(dao, error);
  if (records == NULL)
    return FALSE;
  for (guint index = 0; index < records->len; index++) {
    const OsintExecutionRecord *record = g_ptr_array_index(records, index);
    const char *identifier = osint_execution_record_get_identifier(record);
    char *node_id = core_graph_projection_ref("execution", identifier);
    char *excerpt = core_graph_projection_stdout_excerpt(record);
    char *label = g_strdup_printf(
        "%s · %s", osint_execution_record_get_tool_identifier(record),
        osint_execution_record_get_action_identifier(record));
    CoreGraphNodeView node = {
        .id = node_id,
        .object_kind = "execution",
        .object_id = identifier,
        .type = "OSINT_EXECUTION",
        .label = label,
        .state = osint_execution_record_get_final_state(record),
        .group = "evidence",
        .raw_excerpt = excerpt,
        .tool_id = osint_execution_record_get_tool_identifier(record),
        .tool_version = osint_execution_record_get_tool_version(record),
        .started_at = osint_execution_record_get_started_at(record),
        .finished_at = osint_execution_record_get_finished_at(record),
        .provenance_complete = FALSE,
        .missing_provenance_reason =
            "Le runtime V20 n'identifie pas un artefact brut et une "
            "transformation séparés."};
    gboolean success =
        node_id != NULL && label != NULL &&
        core_graph_snapshot_add_node(snapshot, &node, error) &&
        core_graph_projection_add_navigation(snapshot, node_id, "execution",
                                             identifier, TRUE, error);
    const char *selection_kind =
        osint_execution_record_get_selection_kind(record);
    if (success && g_strcmp0(selection_kind, "entity") == 0) {
      const char *selection_id =
          osint_execution_record_get_selection_identifier(record);
      char *source = core_graph_projection_ref("entity", selection_id);
      char *edge_id = core_graph_projection_ref("execution-input", identifier);
      CoreGraphEdgeView edge = {.id = edge_id,
                                .source = source,
                                .target = node_id,
                                .kind = "provenance",
                                .semantic = "execution_input",
                                .review_state = "historical",
                                .directed = TRUE};
      success = source != NULL && edge_id != NULL &&
                core_graph_snapshot_add_edge(snapshot, &edge, error);
      g_free(edge_id);
      g_free(source);
    }
    GPtrArray *links =
        success ? osint_execution_dao_list_links(dao, identifier, error) : NULL;
    if (!success || links == NULL) {
      g_free(label);
      g_free(excerpt);
      g_free(node_id);
      g_ptr_array_unref(records);
      return FALSE;
    }
    for (guint link_index = 0; link_index < links->len && success;
         link_index++) {
      const OsintExecutionLink *link = g_ptr_array_index(links, link_index);
      if (g_strcmp0(link->object_kind, "entity") != 0)
        continue;
      char *target =
          core_graph_projection_ref("entity", link->object_identifier);
      char *edge_id =
          g_strdup_printf("execution-output:%s:%s:%s", identifier,
                          link->object_kind, link->object_identifier);
      CoreGraphEdgeView edge = {.id = edge_id,
                                .source = node_id,
                                .target = target,
                                .kind = "provenance",
                                .semantic = "execution_output",
                                .review_state = "historical",
                                .disposition = link->disposition,
                                .directed = TRUE};
      success = target != NULL && edge_id != NULL &&
                core_graph_snapshot_add_edge(snapshot, &edge, error);
      g_free(edge_id);
      g_free(target);
    }
    g_ptr_array_unref(links);
    g_free(label);
    g_free(excerpt);
    g_free(node_id);
    if (!success) {
      g_ptr_array_unref(records);
      return FALSE;
    }
  }
  g_ptr_array_unref(records);
  return TRUE;
}

static CoreGraphSnapshot *core_graph_projection_service_collect_internal(
    Database *database, CoreGraphLimits limits,
    const LocalCapabilityRegistry *registry, gboolean operational,
    GError **error) {
  InvestigationRecord *investigation = NULL;
  InvestigationGraphLoader *loader = NULL;
  InvestigationGraphModel *graph = NULL;
  EvidenceDao *evidence_dao = NULL;
  EvidenceEntityDao *association_dao = NULL;
  OsintExecutionDao *execution_dao = NULL;
  ExtractionDao *extraction_dao = NULL;
  GPtrArray *extractions = NULL;
  GHashTable *derived_evidence_ids = NULL;
  GHashTable *evidence_ids = NULL;
  GHashTable *extraction_ids = NULL;
  GHashTable *coherent_extraction_ids = NULL;
  CoreGraphSnapshot *snapshot = NULL;
  GError *nested = NULL;
  gboolean transaction_active = FALSE;
  const gboolean owns_transaction =
      !database_transaction_is_active(database);
  gboolean completed = FALSE;

  g_return_val_if_fail(error == NULL || *error == NULL, NULL);
  if (database == NULL) {
    core_graph_projection_set_error(
        error, CORE_GRAPH_PROJECTION_ERROR_INVALID_ARGUMENT,
        "La connexion Database est obligatoire", NULL);
    return NULL;
  }
  if (owns_transaction && !database_transaction_begin_read_only(database)) {
    core_graph_projection_set_error(
        error, CORE_GRAPH_PROJECTION_ERROR_TRANSACTION,
        database_error_get_message(database) != NULL
            ? database_error_get_message(database)
            : "Impossible de démarrer le snapshot de lecture",
        NULL);
    return NULL;
  }
  transaction_active = owns_transaction;
  investigation = investigation_dao_load(database);
  if (investigation == NULL) {
    core_graph_projection_set_error(error,
                                    CORE_GRAPH_PROJECTION_ERROR_INVESTIGATION,
                                    database_error_get_message(database) != NULL
                                        ? database_error_get_message(database)
                                        : "Impossible de charger l'enquête",
                                    NULL);
    goto cleanup;
  }
  snapshot = core_graph_snapshot_new(
      investigation_record_get_id(investigation),
      investigation_record_get_name(investigation),
      CORE_GRAPH_RUNTIME_SCHEMA_VERSION, limits, &nested);
  if (snapshot == NULL)
    goto model_failure;
  extraction_dao = extraction_dao_new(database, &nested);
  extractions = extraction_dao != NULL
                    ? extraction_dao_list_all(extraction_dao, &nested)
                    : NULL;
  derived_evidence_ids =
      g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  evidence_ids = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  extraction_ids = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  coherent_extraction_ids =
      g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  if (extractions == NULL || derived_evidence_ids == NULL ||
      evidence_ids == NULL || extraction_ids == NULL ||
      coherent_extraction_ids == NULL)
    goto extraction_failure;
  for (guint index = 0U; index < extractions->len; index++) {
    const ExtractionRecord *record = g_ptr_array_index(extractions, index);
    if (record->evidence_identifier != NULL)
      g_hash_table_add(derived_evidence_ids,
                       g_strdup(record->evidence_identifier));
  }
  loader = investigation_graph_loader_new(database, &nested);
  if (loader == NULL)
    goto graph_failure;
  graph = investigation_graph_loader_load(loader, &nested);
  if (graph == NULL ||
      !core_graph_projection_add_entities(snapshot, graph, &nested) ||
      !core_graph_projection_add_relations(snapshot, graph, &nested))
    goto graph_failure;
  evidence_dao = evidence_dao_new(database, &nested);
  association_dao = evidence_entity_dao_new(database, &nested);
  if (evidence_dao == NULL || association_dao == NULL ||
      !core_graph_projection_add_evidence_nodes(
          snapshot, evidence_dao, association_dao, derived_evidence_ids,
          evidence_ids, registry, operational, &nested) ||
      !core_graph_projection_add_extractions(
          snapshot, extractions, evidence_ids, extraction_ids,
          coherent_extraction_ids, &nested) ||
      !core_graph_projection_add_observations(snapshot, evidence_dao,
                                              association_dao, extraction_ids,
                                              coherent_extraction_ids, &nested))
    goto evidence_failure;
  execution_dao = osint_execution_dao_new(database, &nested);
  if (execution_dao == NULL ||
      !core_graph_projection_add_executions(snapshot, execution_dao, &nested))
    goto execution_failure;
  core_graph_snapshot_sort(snapshot);
  if (owns_transaction && !database_transaction_commit(database)) {
    core_graph_projection_set_error(
        error, CORE_GRAPH_PROJECTION_ERROR_TRANSACTION,
        "Impossible de terminer le snapshot cohérent", NULL);
    goto cleanup;
  }
  transaction_active = FALSE;
  completed = TRUE;
  goto cleanup;

execution_failure:
  core_graph_projection_set_error(error, CORE_GRAPH_PROJECTION_ERROR_EXECUTION,
                                  "Impossible de projeter les exécutions OSINT",
                                  nested);
  goto cleanup;
evidence_failure:
  core_graph_projection_set_error(
      error, CORE_GRAPH_PROJECTION_ERROR_EVIDENCE,
      "Impossible de projeter preuves et observations", nested);
  goto cleanup;
extraction_failure:
  core_graph_projection_set_error(
      error, CORE_GRAPH_PROJECTION_ERROR_EVIDENCE,
      "Impossible de charger les extractions persistées", nested);
  goto cleanup;
graph_failure:
  core_graph_projection_set_error(
      error, CORE_GRAPH_PROJECTION_ERROR_ENTITY_RELATION,
      "Impossible de projeter entités et relations", nested);
  goto cleanup;
model_failure:
  core_graph_projection_set_error(
      error, CORE_GRAPH_PROJECTION_ERROR_MODEL,
      "Impossible de construire le modèle de projection", nested);

cleanup:
  g_clear_error(&nested);
  if (transaction_active)
    database_transaction_rollback(database);
  g_clear_pointer(&coherent_extraction_ids, g_hash_table_unref);
  g_clear_pointer(&extraction_ids, g_hash_table_unref);
  g_clear_pointer(&evidence_ids, g_hash_table_unref);
  g_clear_pointer(&derived_evidence_ids, g_hash_table_unref);
  g_clear_pointer(&extractions, g_ptr_array_unref);
  extraction_dao_free(extraction_dao);
  osint_execution_dao_free(execution_dao);
  evidence_entity_dao_free(association_dao);
  evidence_dao_free(evidence_dao);
  investigation_graph_model_free(graph);
  investigation_graph_loader_free(loader);
  investigation_record_free(investigation);
  if (!completed)
    g_clear_pointer(&snapshot, core_graph_snapshot_free);
  return snapshot;
}

CoreGraphSnapshot *core_graph_projection_service_collect_with_registry(
    Database *database, CoreGraphLimits limits,
    const LocalCapabilityRegistry *registry, GError **error) {
  return core_graph_projection_service_collect_internal(database, limits,
                                                        registry, FALSE, error);
}

CoreGraphSnapshot *core_graph_projection_service_collect_operational(
    Database *database, CoreGraphLimits limits,
    const LocalCapabilityRegistry *registry, GError **error) {
  return core_graph_projection_service_collect_internal(database, limits,
                                                        registry, TRUE, error);
}

CoreGraphSnapshot *core_graph_projection_service_collect(Database *database,
                                                         CoreGraphLimits limits,
                                                         GError **error) {
  return core_graph_projection_service_collect_with_registry(database, limits,
                                                             NULL, error);
}
