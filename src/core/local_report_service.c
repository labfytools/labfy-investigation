#include "core/local_report_service.h"
#include "core/core_graph_projection_service.h"
#include "core/local_capability_registry.h"
#include "core/local_correlation_service.h"
#include "database/transaction.h"
#include "models/core_graph_snapshot.h"
#include <json-glib/json-glib.h>
#include <string.h>

static void report_error(GError **error, GIOErrorEnum code, const char *text) {
  if (error != NULL && *error == NULL)
    g_set_error_literal(error, G_IO_ERROR, code, text);
}

static void add_string(JsonBuilder *builder, const char *name,
                       const char *value) {
  json_builder_set_member_name(builder, name);
  if (value != NULL) json_builder_add_string_value(builder, value);
  else json_builder_add_null_value(builder);
}

static void checksum_field(GChecksum *sum, const char *value) {
  guint64 length = value != NULL ? strlen(value) : G_MAXUINT64;
  guint8 encoded[8];
  for (guint i = 0; i < 8U; i++) encoded[i]=(guint8)(length>>((7U-i)*8U));
  g_checksum_update(sum, encoded, sizeof(encoded));
  if (value != NULL) g_checksum_update(sum,(const guchar *)value,(gssize)length);
}

static void checksum_uint(GChecksum *sum, guint64 value) {
  char encoded[32];
  g_snprintf(encoded,sizeof(encoded),"%" G_GUINT64_FORMAT,value);
  checksum_field(sum,encoded);
}

static gboolean safe_text(const char *value, gsize limit) {
  return value != NULL && g_utf8_validate(value, -1, NULL) &&
         strlen(value) <= limit;
}

static const CoreGraphNodeView *find_node(const CoreGraphSnapshot *snapshot,
                                          const char *id) {
  const GPtrArray *nodes = core_graph_snapshot_get_nodes(snapshot);
  for (guint i=0;i<nodes->len;i++) {
    const CoreGraphNodeView *node=core_graph_node_get_view(g_ptr_array_index((GPtrArray *)nodes,i));
    if (g_strcmp0(node->id,id)==0) return node;
  }
  return NULL;
}

static gboolean provenance_kind(const char *kind) {
  return g_strcmp0(kind,"provenance")==0 || g_strcmp0(kind,"support")==0;
}

static gboolean close_provenance(const CoreGraphSnapshot *snapshot,
    GHashTable *included, guint max_dependencies, GError **error) {
  const GPtrArray *edges=core_graph_snapshot_get_edges(snapshot);
  guint initial=g_hash_table_size(included); gboolean changed=TRUE;
  while(changed) {
    changed=FALSE;
    for(guint i=0;i<edges->len;i++) {
      const CoreGraphEdgeView *edge=core_graph_edge_get_view(g_ptr_array_index((GPtrArray *)edges,i));
      if(!provenance_kind(edge->kind)) continue;
      gboolean target=g_hash_table_contains(included,edge->target);
      if(!target || g_hash_table_contains(included,edge->source)) continue;
      /* CONTRACT: les arêtes de provenance vont du justificatif vers l'objet
       * produit. Remonter vers source est nécessaire; descendre exporterait
       * des observations sœurs non sélectionnées. */
      const char *candidate=edge->source;
      if(g_hash_table_size(included)-initial>=max_dependencies) {
        report_error(error,G_IO_ERROR_NO_SPACE,"Fermeture de provenance trop grande.");
        return FALSE;
      }
      g_hash_table_add(included,g_strdup(candidate)); changed=TRUE;
    }
  }
  return TRUE;
}

static void add_node(JsonBuilder *builder, const CoreGraphNodeView *node,
                     gboolean selected, gboolean full) {
  json_builder_begin_object(builder); add_string(builder,"id",node->id);
  add_string(builder,"object_kind",node->object_kind);
  add_string(builder,"object_id",node->object_id); add_string(builder,"type",node->type);
  add_string(builder,"label",full?node->label:NULL); add_string(builder,"state",full?node->state:NULL);
  add_string(builder,"group",full?node->group:NULL); add_string(builder,"value_raw",full?node->value_raw:NULL);
  add_string(builder,"value_normalized",full?node->value_normalized:NULL);
  add_string(builder,"value_corrected",full?node->value_corrected:NULL);
  add_string(builder,"review_state",full?node->review_state:NULL);
  add_string(builder,"provenance_kind",full?node->provenance_kind:NULL);
  add_string(builder,"source_header",full?node->source_header:NULL);
  add_string(builder,"tool_id",full?node->tool_id:NULL); add_string(builder,"tool_version",full?node->tool_version:NULL);
  add_string(builder,"collected_at",node->collected_at);
  add_string(builder,"imported_at",node->imported_at);
  add_string(builder,"observed_at",node->observed_at);
  add_string(builder,"integrated_at",node->integrated_at);
  add_string(builder,"missing_provenance_reason",node->missing_provenance_reason);
  json_builder_set_member_name(builder,"provenance_complete");
  json_builder_add_boolean_value(builder,node->provenance_complete);
  json_builder_set_member_name(builder,"selection_role");
  json_builder_add_string_value(builder,selected?"selected":"provenance_dependency");
  json_builder_set_member_name(builder,"reference_only");
  json_builder_add_boolean_value(builder,!full);
  json_builder_end_object(builder);
}

static GDateTime *parse_report_instant(const char *value,
                                       gboolean *minute_precision,
                                       gboolean *utc,
                                       gboolean *offset) {
  const char *time = strchr(value, 'T');
  gsize length = strlen(value);
  *minute_precision = FALSE;
  *utc = g_str_has_suffix(value, "Z");
  *offset = length >= 6U &&
      (value[length - 6U] == '+' || value[length - 6U] == '-') &&
      value[length - 3U] == ':';
  if (time == NULL || (!*utc && !*offset)) return NULL;

  const char *zone = *utc ? value + length - 1U : value + length - 6U;
  gsize clock_length = (gsize)(zone - time - 1);
  if (clock_length == 5U && time[3] == ':') {
    /* CONTRACT: une date ISO 8601 HH:MM avec fuseau est positionnable. Le
     * rapport conserve sa valeur brute et matérialise le début de la minute
     * avec :00, sans inventer une précision à la seconde. */
    char *normalized = g_strdup_printf("%.*s:00%s", (int)(zone - value),
                                       value, zone);
    GDateTime *parsed = g_date_time_new_from_iso8601(normalized, NULL);
    g_free(normalized);
    *minute_precision = parsed != NULL;
    return parsed;
  }
  if (clock_length < 8U || time[3] != ':' || time[6] != ':') return NULL;
  return g_date_time_new_from_iso8601(value, NULL);
}

static void add_event(JsonBuilder *builder, const CoreGraphNodeView *node,
                      const char *category, const char *value, guint *count) {
  if(value==NULL) return;
  char *seed=g_strdup_printf("%s\x1f%s\x1f%s",node->id,category,value);
  char *hash=g_compute_checksum_for_string(G_CHECKSUM_SHA256,seed,-1);
  json_builder_begin_object(builder); add_string(builder,"id",hash);
  gboolean minute_precision=FALSE;
  gboolean utc=FALSE;
  gboolean offset=FALSE;
  GDateTime *parsed=parse_report_instant(value,&minute_precision,&utc,&offset);
  char *instant=parsed!=NULL?g_date_time_format_iso8601(parsed):NULL;
  add_string(builder,"object_id",node->id); add_string(builder,"category",category);
  add_string(builder,"raw_value",value); add_string(builder,"instant",instant);
  add_string(builder,"timezone",utc?"UTC":(offset?"explicit_offset":"unknown"));
  add_string(builder,"precision",parsed==NULL?"unknown":
             (minute_precision?"minute":"second"));
  json_builder_set_member_name(builder,"positionable");
  json_builder_add_boolean_value(builder,parsed!=NULL);
  json_builder_end_object(builder); (*count)++; g_free(instant);g_clear_pointer(&parsed,g_date_time_unref);g_free(hash); g_free(seed);
}

static guint count_events(const CoreGraphSnapshot *snapshot,GHashTable *included) {
  guint count=0U;const GPtrArray *nodes=core_graph_snapshot_get_nodes(snapshot);
  for(guint i=0;i<nodes->len;i++) {
    const CoreGraphNodeView *n=core_graph_node_get_view(g_ptr_array_index((GPtrArray *)nodes,i));
    if(!g_hash_table_contains(included,n->id)) continue;
    const char *values[]={n->started_at,n->finished_at,n->collected_at,n->imported_at,n->observed_at,n->integrated_at};
    for(guint j=0;j<G_N_ELEMENTS(values);j++) if(values[j]!=NULL) count++;
  }
  return count;
}

GBytes *local_report_service_build(Database *database,
    const char *investigation_identifier, const LocalReportRequest *request,
    LocalReportLimits limits, GError **error) {
  g_return_val_if_fail(error==NULL||*error==NULL,NULL);
  if(database==NULL||!g_uuid_string_is_valid(investigation_identifier)||request==NULL||
     !safe_text(request->title,160U)||(request->human_comment!=NULL&&!safe_text(request->human_comment,1000U))||
     g_strcmp0(request->profile,"MINIMAL")!=0||request->selected_count==0U||
     request->selected_count>limits.max_selected||limits.max_dependencies==0U||
     limits.max_events==0U||limits.max_json_bytes==0U) {
    report_error(error,G_IO_ERROR_INVALID_ARGUMENT,"Demande de rapport invalide ou hors limites."); return NULL;
  }
  gboolean owned=!database_transaction_is_active(database);
  if(owned&&!database_transaction_begin_read_only(database)) {
    report_error(error,G_IO_ERROR_FAILED,"Snapshot de rapport indisponible."); return NULL;
  }
  LocalCapabilityRegistry *registry=local_capability_registry_new(error);
  CoreGraphSnapshot *snapshot=registry!=NULL?core_graph_projection_service_collect_operational(
      database,(CoreGraphLimits){2000U,4000U,4U*1024U*1024U},registry,error):NULL;
  GHashTable *selected=g_hash_table_new_full(g_str_hash,g_str_equal,g_free,NULL);
  GHashTable *included=g_hash_table_new_full(g_str_hash,g_str_equal,g_free,NULL);
  gboolean ok=snapshot!=NULL;
  if(ok && g_strcmp0(core_graph_snapshot_get_investigation_id(snapshot),
                     investigation_identifier)!=0) {
    report_error(error,G_IO_ERROR_INVALID_DATA,"La sélection appartient à une autre enquête.");
    ok=FALSE;
  }
  for(gsize i=0;ok&&i<request->selected_count;i++) {
    const char *id=request->selected_node_ids[i];
    if(id==NULL||find_node(snapshot,id)==NULL) { report_error(error,G_IO_ERROR_NOT_FOUND,"Objet sélectionné inconnu ou étranger.");ok=FALSE;break; }
    g_hash_table_add(selected,g_strdup(id));g_hash_table_add(included,g_strdup(id));
  }
  if(ok) ok=close_provenance(snapshot,included,limits.max_dependencies,error);
  if(ok && request->include_timeline && count_events(snapshot,included)>limits.max_events) {
    report_error(error,G_IO_ERROR_NO_SPACE,"La limite d'événements tronquerait le rapport MINIMAL.");ok=FALSE;
  }
  JsonBuilder *builder=ok?json_builder_new():NULL; GChecksum *sum=ok?g_checksum_new(G_CHECKSUM_SHA256):NULL;
  if(ok) {
    checksum_field(sum,LOCAL_REPORT_CONTRACT);checksum_field(sum,LOCAL_REPORT_RULE_VERSION);
    checksum_field(sum,investigation_identifier);checksum_field(sum,request->title);
    checksum_field(sum,request->human_comment);checksum_field(sum,request->profile);
    checksum_uint(sum,limits.max_selected);checksum_uint(sum,limits.max_dependencies);
    checksum_uint(sum,limits.max_events);checksum_uint(sum,limits.max_json_bytes);
    checksum_uint(sum,request->include_evidence);checksum_uint(sum,request->include_timeline);
    checksum_uint(sum,request->include_infrastructure);
    const GPtrArray *nodes=core_graph_snapshot_get_nodes(snapshot);
    for(guint i=0;i<nodes->len;i++){const CoreGraphNodeView *n=core_graph_node_get_view(g_ptr_array_index((GPtrArray *)nodes,i));if(g_hash_table_contains(included,n->id)){checksum_field(sum,n->id);checksum_field(sum,g_hash_table_contains(selected,n->id)?"selected":"provenance_dependency");checksum_field(sum,n->type);checksum_field(sum,n->label);checksum_field(sum,n->state);checksum_field(sum,n->raw_excerpt);checksum_field(sum,n->value_raw);checksum_field(sum,n->value_normalized);checksum_field(sum,n->value_corrected);checksum_field(sum,n->review_state);checksum_field(sum,n->provenance_kind);checksum_field(sum,n->source_header);checksum_field(sum,n->tool_id);checksum_field(sum,n->tool_version);checksum_field(sum,n->started_at);checksum_field(sum,n->finished_at);checksum_field(sum,n->collected_at);checksum_field(sum,n->imported_at);checksum_field(sum,n->observed_at);checksum_field(sum,n->integrated_at);}}
    const GPtrArray *edges=core_graph_snapshot_get_edges(snapshot);
    for(guint i=0;i<edges->len;i++){const CoreGraphEdgeView *e=core_graph_edge_get_view(g_ptr_array_index((GPtrArray *)edges,i));if(request->include_infrastructure&&g_hash_table_contains(included,e->source)&&g_hash_table_contains(included,e->target)){checksum_field(sum,e->id);checksum_field(sum,e->source);checksum_field(sum,e->target);checksum_field(sum,e->kind);checksum_field(sum,e->semantic);checksum_field(sum,e->review_state);checksum_field(sum,e->disposition);checksum_uint(sum,e->directed);}}
    json_builder_begin_object(builder);add_string(builder,"contract",LOCAL_REPORT_CONTRACT);
    add_string(builder,"rule_version",LOCAL_REPORT_RULE_VERSION);add_string(builder,"investigation_id",investigation_identifier);
    add_string(builder,"revision",g_checksum_get_string(sum));add_string(builder,"generated_at",request->generated_at);
    add_string(builder,"title",request->title);add_string(builder,"human_comment",request->human_comment);
    add_string(builder,"human_comment_kind","human_authored");add_string(builder,"profile",request->profile);
    json_builder_set_member_name(builder,"sections");json_builder_begin_object(builder);
    json_builder_set_member_name(builder,"evidence");json_builder_add_boolean_value(builder,request->include_evidence);
    json_builder_set_member_name(builder,"timeline");json_builder_add_boolean_value(builder,request->include_timeline);
    json_builder_set_member_name(builder,"infrastructure");json_builder_add_boolean_value(builder,request->include_infrastructure);json_builder_end_object(builder);
    json_builder_set_member_name(builder,"objects");json_builder_begin_array(builder);
    for(guint i=0;i<nodes->len;i++){const CoreGraphNodeView *n=core_graph_node_get_view(g_ptr_array_index((GPtrArray *)nodes,i));if(g_hash_table_contains(included,n->id))add_node(builder,n,g_hash_table_contains(selected,n->id),request->include_evidence);}json_builder_end_array(builder);
    json_builder_set_member_name(builder,"network");json_builder_begin_array(builder);
    for(guint i=0;i<edges->len;i++){const CoreGraphEdgeView *e=core_graph_edge_get_view(g_ptr_array_index((GPtrArray *)edges,i));if(request->include_infrastructure&&g_hash_table_contains(included,e->source)&&g_hash_table_contains(included,e->target)){json_builder_begin_object(builder);add_string(builder,"id",e->id);add_string(builder,"source",e->source);add_string(builder,"target",e->target);add_string(builder,"kind",e->kind);add_string(builder,"semantic",e->semantic);add_string(builder,"review_state",e->review_state);add_string(builder,"disposition",e->disposition);json_builder_set_member_name(builder,"directed");json_builder_add_boolean_value(builder,e->directed);json_builder_end_object(builder);}}json_builder_end_array(builder);
    json_builder_set_member_name(builder,"timeline");json_builder_begin_array(builder);guint events=0U;
    if(request->include_timeline) {
      for(guint i=0;i<nodes->len;i++) {
        const CoreGraphNodeView *n=core_graph_node_get_view(
            g_ptr_array_index((GPtrArray *)nodes,i));
        if(!g_hash_table_contains(included,n->id)) continue;
        add_event(builder,n,"processing_started",n->started_at,&events);
        if(events<limits.max_events)
          add_event(builder,n,"processing_finished",n->finished_at,&events);
        if(events<limits.max_events) add_event(builder,n,"evidence_collection",n->collected_at,&events);
        if(events<limits.max_events) add_event(builder,n,"evidence_import",n->imported_at,&events);
        if(events<limits.max_events) add_event(builder,n,"observation",n->observed_at,&events);
        if(events<limits.max_events) add_event(builder,n,"integration",n->integrated_at,&events);
      }
    }
    json_builder_end_array(builder);
    json_builder_set_member_name(builder,"limitations");json_builder_begin_array(builder);
    json_builder_add_string_value(builder,"Une empreinte enregistrée ne constitue pas une vérification physique récente.");
    json_builder_add_string_value(builder,"Les rapprochements sont exploratoires et ne confirment aucune identité.");
    json_builder_add_string_value(builder,"Les dates absentes ou sans fuseau ne sont pas inventées.");json_builder_end_array(builder);
    json_builder_set_member_name(builder,"limits");json_builder_begin_object(builder);
    json_builder_set_member_name(builder,"max_selected");json_builder_add_int_value(builder,limits.max_selected);
    json_builder_set_member_name(builder,"max_dependencies");json_builder_add_int_value(builder,limits.max_dependencies);
    json_builder_set_member_name(builder,"max_events");json_builder_add_int_value(builder,limits.max_events);
    json_builder_set_member_name(builder,"max_json_bytes");json_builder_add_int_value(builder,limits.max_json_bytes);json_builder_end_object(builder);json_builder_end_object(builder);
  }
  GBytes *result=NULL;
  if(ok){JsonGenerator *generator=json_generator_new();JsonNode *root=json_builder_get_root(builder);json_generator_set_root(generator,root);gsize size=0U;char *data=json_generator_to_data(generator,&size);if(data==NULL||size>limits.max_json_bytes){g_free(data);report_error(error,G_IO_ERROR_NO_SPACE,"Document de rapport trop volumineux.");}else result=g_bytes_new_take(data,size);json_node_free(root);g_object_unref(generator);}
  if(builder) g_object_unref(builder);
  if(sum) g_checksum_free(sum);
  g_hash_table_unref(included);g_hash_table_unref(selected);
  core_graph_snapshot_free(snapshot);local_capability_registry_free(registry);
  if(owned){if(result!=NULL&&!database_transaction_commit(database)){g_clear_pointer(&result,g_bytes_unref);report_error(error,G_IO_ERROR_FAILED,"Fermeture du snapshot impossible.");}else if(result==NULL)(void)database_transaction_rollback(database);}
  return result;
}
