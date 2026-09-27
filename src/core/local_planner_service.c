#define _POSIX_C_SOURCE 200809L
#include "core/local_planner_service.h"
#include "dao/evidence_dao.h"
#include "dao/extraction_dao.h"
#include "models/evidence_record.h"
#include <json-glib/json-glib.h>
#include <glib/gstdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

static void add_string(JsonBuilder *b, const char *name, const char *value) {
  json_builder_set_member_name(b, name);
  if (value) json_builder_add_string_value(b, value); else json_builder_add_null_value(b);
}
static void hash_field(GChecksum *c, const char *v) {
  guint64 n = v ? strlen(v) : G_MAXUINT64; guint8 p[8];
  for (guint i=0;i<8;i++) p[i]=(guint8)(n >> ((7U-i)*8U));
  g_checksum_update(c,p,8); if(v) g_checksum_update(c,(const guchar*)v,(gssize)n);
}
static const char *capability_for(const EvidenceRecord *e) {
  const char *type=evidence_record_get_type_identifier(e);
  if (g_strcmp0(type,"email")==0) return LOCAL_CAPABILITY_EML_HEADERS;
  if (g_strcmp0(type,"photo")==0 || g_strcmp0(type,"image")==0)
    return LOCAL_CAPABILITY_EXIF_METADATA;
  return NULL;
}
static gboolean extraction_covers(GPtrArray *items, const char *source,
                                   const char *capability) {
  const char *tool=g_strcmp0(capability,LOCAL_CAPABILITY_EML_HEADERS)==0
      ? "labfy.eml_analyzer" : "exiftool-json";
  for(guint i=0;i<items->len;i++) { ExtractionRecord *r=g_ptr_array_index(items,i);
    if(g_strcmp0(r->source_identifier,source)==0 && g_strcmp0(r->tool_identifier,tool)==0)
      return TRUE;
  } return FALSE;
}
static gboolean is_derivative(GPtrArray *items,const char *id) {
  for(guint i=0;i<items->len;i++) if(g_strcmp0(((ExtractionRecord*)g_ptr_array_index(items,i))->evidence_identifier,id)==0) return TRUE;
  return FALSE;
}
static gboolean active_job(GPtrArray *jobs,const char *source,const char *cap) {
  for(guint i=0;i<jobs->len;i++){LocalJobRecord *j=g_ptr_array_index(jobs,i);
    if(g_strcmp0(j->source_evidence_id,source)==0 && g_strcmp0(j->capability_id,cap)==0 &&
       (j->state==LOCAL_JOB_QUEUED||j->state==LOCAL_JOB_RUNNING||j->state==LOCAL_JOB_RETRY_WAIT)) return TRUE;
  } return FALSE;
}
GBytes *local_planner_service_build(Database *db, LocalJobStore *store,
    const LocalCapabilityRegistry *registry, const char *investigation_id,
    gsize max_bytes, GError **error) {
  if(!db||!store||!registry||!g_uuid_string_is_valid(investigation_id)||max_bytes==0){
    g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_INVALID_ARGUMENT,"Contrat planner invalide."); return NULL; }
  EvidenceDao *ed=evidence_dao_new(db,error); ExtractionDao *xd=ed?extraction_dao_new(db,error):NULL;
  GPtrArray *e=xd?evidence_dao_list_all(ed,error):NULL;
  GPtrArray *x=e?extraction_dao_list_all(xd,error):NULL;
  GPtrArray *jobs=x?local_job_store_list(store,error):NULL;
  if(!jobs){g_clear_pointer(&x,g_ptr_array_unref);g_clear_pointer(&e,g_ptr_array_unref);extraction_dao_free(xd);evidence_dao_free(ed);return NULL;}
  GChecksum *rev=g_checksum_new(G_CHECKSUM_SHA256); hash_field(rev,LOCAL_PLANNER_RULE_VERSION);hash_field(rev,investigation_id);
  for(guint i=0;i<e->len;i++){EvidenceRecord *r=g_ptr_array_index(e,i);hash_field(rev,evidence_record_get_identifier(r));hash_field(rev,evidence_record_get_sha256(r));hash_field(rev,evidence_record_get_type_identifier(r));}
  for(guint i=0;i<x->len;i++){ExtractionRecord *r=g_ptr_array_index(x,i);hash_field(rev,r->identifier);hash_field(rev,r->source_identifier);hash_field(rev,r->tool_identifier);}
  for(guint i=0;i<jobs->len;i++){LocalJobRecord *j=g_ptr_array_index(jobs,i);hash_field(rev,j->job_id);hash_field(rev,local_job_state_code(j->state));}
  const char *revision=g_checksum_get_string(rev); JsonBuilder *b=json_builder_new();json_builder_begin_object(b);
  add_string(b,"contract",LOCAL_PLANNER_CONTRACT);add_string(b,"investigation_id",investigation_id);add_string(b,"input_revision",revision);add_string(b,"rule_version",LOCAL_PLANNER_RULE_VERSION);
  json_builder_set_member_name(b,"complete");json_builder_add_boolean_value(b,TRUE);
  json_builder_set_member_name(b,"profiles");json_builder_begin_array(b);
  const guint limits[2][4]={{8,16,256*1024*1024U,240000},{2,3,8*1024*1024U,30000}};
  const char *profiles[2]={"LOCAL_PRUDENT","SPECIMEN_SMALL"};
  for(guint i=0;i<2;i++){json_builder_begin_object(b);add_string(b,"id",profiles[i]);json_builder_set_member_name(b,"max_analyses");json_builder_add_int_value(b,limits[i][0]);json_builder_set_member_name(b,"max_attempts_total");json_builder_add_int_value(b,limits[i][1]);json_builder_set_member_name(b,"max_source_bytes");json_builder_add_int_value(b,limits[i][2]);json_builder_set_member_name(b,"max_active_ms");json_builder_add_int_value(b,limits[i][3]);add_string(b,"network_contact","NONE");json_builder_end_object(b);}
  json_builder_end_array(b);json_builder_set_member_name(b,"recommendations");json_builder_begin_array(b);
  for(guint i=0;i<e->len;i++){EvidenceRecord *r=g_ptr_array_index(e,i);const char *id=evidence_record_get_identifier(r);const char *cap=capability_for(r);if(!cap||is_derivative(x,id))continue;
    const LocalCapabilityStatus *s=local_capability_registry_lookup(registry,cap);gboolean covered=extraction_covers(x,id,cap), active=active_job(jobs,id,cap);GString *key=g_string_new(investigation_id);g_string_append_printf(key,"\x1f%s\x1f%s\x1f%s\x1f%"G_GUINT64_FORMAT,id,evidence_record_get_sha256(r),cap,evidence_record_get_size_bytes(r));char *hash=g_compute_checksum_for_string(G_CHECKSUM_SHA256,key->str,-1);char *rid=g_strdup_printf("recommendation:%s",hash);
    json_builder_begin_object(b);add_string(b,"id",rid);add_string(b,"kind","ANALYSIS");add_string(b,"object_id",id);add_string(b,"capability_id",cap);add_string(b,"capability_version",s?s->descriptor->capability_version:NULL);add_string(b,"adapter_id",s?s->descriptor->adapter_id:NULL);add_string(b,"adapter_version",s?s->descriptor->adapter_version:NULL);add_string(b,"priority",covered||active?"LOW":"HIGH");add_string(b,"reason_code",covered?"RESULT_ALREADY_PRESENT":active?"JOB_ALREADY_ACTIVE":s&&s->availability==LOCAL_CAPABILITY_READY?"UNANALYZED_COMPATIBLE_EVIDENCE":"CAPABILITY_UNAVAILABLE");add_string(b,"reason",covered?"Une analyse compatible est déjà publiée.":active?"Une analyse identique est déjà en file ou active.":s&&s->availability==LOCAL_CAPABILITY_READY?"Preuve originale compatible sans analyse publiée.":s?s->reason:"Capability absente.");add_string(b,"input_revision",revision);add_string(b,"source_sha256",evidence_record_get_sha256(r));json_builder_set_member_name(b,"source_size");json_builder_add_int_value(b,evidence_record_get_size_bytes(r));json_builder_set_member_name(b,"available");json_builder_add_boolean_value(b,!covered&&!active&&s&&s->availability==LOCAL_CAPABILITY_READY);add_string(b,"network_contact","NONE");json_builder_end_object(b);
    if(covered){char *navigation_id=g_strdup_printf("navigation:%s",hash);json_builder_begin_object(b);add_string(b,"id",navigation_id);add_string(b,"kind","NAVIGATION");add_string(b,"object_id",id);add_string(b,"capability_id","labfy.navigation.analysis_result.v1");add_string(b,"capability_version","1");add_string(b,"adapter_id",NULL);add_string(b,"adapter_version",NULL);add_string(b,"priority","MEDIUM");add_string(b,"reason_code","VIEW_PERSISTED_RESULT");add_string(b,"reason","Consulter le résultat et ses observations persistées.");add_string(b,"input_revision",revision);add_string(b,"source_sha256",evidence_record_get_sha256(r));json_builder_set_member_name(b,"source_size");json_builder_add_int_value(b,evidence_record_get_size_bytes(r));json_builder_set_member_name(b,"available");json_builder_add_boolean_value(b,TRUE);add_string(b,"network_contact","NONE");json_builder_end_object(b);g_free(navigation_id);}
    g_free(rid);g_free(hash);g_string_free(key,TRUE);
  }
  json_builder_end_array(b);json_builder_end_object(b);JsonGenerator *g=json_generator_new();JsonNode *root=json_builder_get_root(b);json_generator_set_root(g,root);gsize n=0;char *data=json_generator_to_data(g,&n);GBytes *out=NULL;if(data&&n<=max_bytes)out=g_bytes_new_take(data,n);else{g_free(data);g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_NO_SPACE,"Snapshot planner trop volumineux.");}json_node_free(root);g_object_unref(g);g_object_unref(b);g_checksum_free(rev);g_ptr_array_unref(jobs);g_ptr_array_unref(x);g_ptr_array_unref(e);extraction_dao_free(xd);evidence_dao_free(ed);return out;
}
gboolean local_planner_snapshot_write_atomic(GBytes *snapshot,const char *path,GError **error){
  if(!snapshot||!path){g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_INVALID_ARGUMENT,"Destination planner invalide.");return FALSE;}char *tmp=g_strdup_printf("%s.XXXXXX",path);int fd=g_mkstemp_full(tmp,O_WRONLY|O_CLOEXEC,0600);gsize n=0,w=0;const guint8 *p=g_bytes_get_data(snapshot,&n);gboolean ok=fd>=0;while(ok&&w<n){ssize_t z=write(fd,p+w,n-w);if(z<0&&errno==EINTR)continue;if(z<=0)ok=FALSE;else w+=(gsize)z;}if(ok)ok=fsync(fd)==0;if(fd>=0&&close(fd)!=0)ok=FALSE;if(ok)ok=g_rename(tmp,path)==0;if(!ok){g_unlink(tmp);g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_FAILED,"Publication atomique planner impossible.");}g_free(tmp);return ok;
}
