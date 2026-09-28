#include "core/core_graph_projection_service.h"
#include "core/core_graph_snapshot_serializer.h"
#include "core/eml_analysis_persistence_service.h"
#include "core/evidence_importer.h"
#include "core/evidence_preview.h"
#include "core/file_hash.h"
#include "core/exiftool_persistence_service.h"
#include "core/local_capability_registry.h"
#include "core/local_job_store.h"
#include "core/local_correlation_service.h"
#include "core/local_planner_service.h"
#include "core/local_report_service.h"
#include "core/local_tool_runner.h"
#include "core/observation_review_service.h"
#include "core/research_store.h"
#include "dao/evidence_entity_dao.h"
#include "dao/evidence_dao.h"
#include "dao/extraction_dao.h"
#include "dao/investigation_dao.h"
#include "database/database.h"
#include "database/statement.h"
#include "models/evidence_record.h"
#include "models/investigation_record.h"

#include <fcntl.h>
#include <errno.h>
#include <glib/gstdio.h>
#include <json-glib/json-glib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <sqlite3.h>

#define EML_JOB "82000000-0000-4000-8000-000000000011"
#define EML_REQUEST "82000000-0000-4000-8000-000000000012"
#define EML_DERIVATIVE "82000000-0000-4000-8000-000000000013"
#define EXIF_JOB "82000000-0000-4000-8000-000000000021"
#define EXIF_REQUEST "82000000-0000-4000-8000-000000000022"
#define EXIF_DERIVATIVE "82000000-0000-4000-8000-000000000023"

static const char *const timestamp = "2026-09-26T21:00:00Z";
static const char *const eml_content =
    "From: Alice SPECIMEN <alice@example.test>\r\n"
    "To: Bob SPECIMEN <bob@example.test>\r\n"
    "Received: from relay.example.test ([192.0.2.80]) by mx.example.test;\r\n"
    "Message-ID: <j5@example.test>\r\nDate: Sat, 26 Sep 2026 21:00:00 +0200\r\n"
    "Subject: Reprise J5\r\n\r\nContenu entièrement synthétique.\r\n";
static const char *const png_base64 =
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+"
    "A8AAQUBAScY42YAAAAASUVORK5CYII=";
static const char *const j7_eml_a =
    "From: Alice SPECIMEN <alice@example.test>\r\n"
    "To: Pivot SPECIMEN <pivot@shared.test>\r\n"
    "Message-ID: <j7-a@shared.test>\r\n"
    "Received: from relay.shared.test ([192.0.2.44]) by mx.shared.test;\r\n"
    "Subject: Homonyme SPECIMEN\r\n\r\nCorpus synthétique J7.\r\n";
static const char *const j7_eml_b =
    "From: Alice SPECIMEN <other@shared.test>\r\n"
    "To: alice@example.test, alice@example.test\r\n"
    "Message-ID: <j7-b@shared.test>\r\n"
    "Received: from edge.shared.test ([192.0.2.44]) by mx.other.test;\r\n"
    "Subject: Répétition SPECIMEN\r\n\r\nCorpus synthétique J7.\r\n";
static const char *const j7_eml_c =
    "From: Homonyme SPECIMEN <unrelated@shared.test>\r\n"
    "To: distinct@example.invalid\r\n"
    "Message-ID: <j7-c@unrelated.test>\r\n"
    "Received: from shared.test ([198.51.100.9]) by mx.unrelated.test;\r\n"
    "Subject: Domaine seulement SPECIMEN\r\n\r\nAucune identité commune déduite.\r\n";

typedef struct {
  char *investigation_id;
  char *eml_id;
  char *image_id;
  char *title;
  gboolean synthetic;
} Manifest;

static gboolean manifest_read(const char *root, Manifest *manifest,
                              GError **error);
static void manifest_clear(Manifest *manifest);
static LocalJobStore *open_store(const char *root, Manifest *manifest,
                                 GError **error);
static gboolean write_private_atomic(const char *path, const char *contents,
                                     GError **error);

/* CONTRACT: research commands accept identifiers and revisions only.  The
 * laboratory provider authority is process configuration, never UI input. */
static gboolean research_prepare_json(const char *root, const char *selection,
    const char *question, const char *exclusions, const char *revision,
    const char *key, GError **error);
static gboolean research_grant_json(const char *root, const char *plan_id,
    const char *revision, const char *actions, const char *decisions,
    const char *exclusions,
    const char *key, GError **error);
static gboolean research_campaign_json(const char *root, const char *grant_id,
    const char *revision, const char *actions, const char *key, GError **error);
static gboolean research_snapshot_json(const char *root, GError **error);
static gboolean research_revoke_json(const char *root, const char *grant_id,
    GError **error);

static void json_add_nullable(JsonBuilder *builder, const char *name,
                              const char *value) {
  json_builder_set_member_name(builder, name);
  if (value != NULL) json_builder_add_string_value(builder, value);
  else json_builder_add_null_value(builder);
}

static gboolean print_json_builder(JsonBuilder *builder, GError **error) {
  JsonNode *root = json_builder_get_root(builder);
  JsonGenerator *generator = json_generator_new();
  json_generator_set_root(generator, root);
  char *data = json_generator_to_data(generator, NULL);
  gboolean ok = data != NULL && printf("%s\n", data) >= 0;
  if (!ok) g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                               "Impossible d'écrire la réponse JSON.");
  g_free(data); g_object_unref(generator); json_node_free(root);
  return ok;
}

static volatile sig_atomic_t worker_stop_requested = 0;

typedef struct { GMutex mutex; GCond condition; gboolean finished;
  guint timeout_ms; GCancellable *cancellable; } BudgetTimer;

static gpointer budget_timer_run(gpointer data) {
  BudgetTimer *timer=data; gint64 deadline=g_get_monotonic_time()+
      (gint64)timer->timeout_ms*1000;
  g_mutex_lock(&timer->mutex);
  while(!timer->finished && g_cond_wait_until(&timer->condition,&timer->mutex,deadline));
  gboolean expired=!timer->finished; g_mutex_unlock(&timer->mutex);
  if (expired)
    g_cancellable_cancel(timer->cancellable);
  return NULL;
}

static void worker_stop_handler(int signal_number) {
  (void)signal_number;
  worker_stop_requested = 1;
  local_tool_runner_request_process_cancel();
}

static char *workspace_path(const char *root, const char *leaf) {
  return g_build_filename(root, leaf, NULL);
}

static char *now_iso(void) {
  GDateTime *now = g_date_time_new_now_utc();
  char *value = g_date_time_format(now, "%Y-%m-%dT%H:%M:%SZ");
  g_date_time_unref(now);
  return value;
}

static gboolean export_correlations(const char *root, Database *database,
                                    const char *investigation_id,
                                    GError **error) {
  LocalCorrelationLimits limits = {2000U, 500U, 1000U, 1024U * 1024U};
  GBytes *snapshot = local_correlation_service_build(
      database, investigation_id, limits, error);
  char *path = workspace_path(root, "correlation-snapshot.json");
  gboolean ok = snapshot != NULL &&
                local_correlation_snapshot_write_atomic(snapshot, path, error);
  g_clear_pointer(&snapshot, g_bytes_unref);
  g_free(path);
  return ok;
}

static gboolean export_planner(const char *root, Database *database,
                               LocalJobStore *store,
                               LocalCapabilityRegistry *registry,
                               const char *investigation_id, GError **error) {
  GBytes *snapshot = local_planner_service_build(database, store, registry,
      investigation_id, 1024U * 1024U, error);
  char *path = workspace_path(root, "planner-snapshot.json");
  gboolean ok = snapshot != NULL &&
      local_planner_snapshot_write_atomic(snapshot, path, error);
  g_clear_pointer(&snapshot, g_bytes_unref); g_free(path); return ok;
}

static gboolean prepare_report(const char *root, const char *objects_csv,
    const char *title, const char *comment, const char *generated_at,
    const char *sections,
    GError **error) {
  if (objects_csv == NULL || title == NULL || generated_at == NULL) {
    g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_INVALID_ARGUMENT,
                        "Paramètres du rapport incomplets."); return FALSE;
  }
  gchar **objects=g_strsplit(objects_csv,",",65);gsize count=g_strv_length(objects);
  Manifest manifest={0};gboolean ok=count>0U&&count<=64U&&manifest_read(root,&manifest,error);
  char *database_path=workspace_path(root,"Enquete.sqlite");
  Database *database=ok?database_open_read_only(database_path,error):NULL;
  gboolean evidence=sections!=NULL&&strstr(sections,"evidence")!=NULL;
  gboolean timeline=sections!=NULL&&strstr(sections,"timeline")!=NULL;
  gboolean infrastructure=sections!=NULL&&strstr(sections,"infrastructure")!=NULL;
  if(!evidence&&!timeline&&!infrastructure){g_set_error_literal(error,G_IO_ERROR,
      G_IO_ERROR_INVALID_ARGUMENT,"Sections de rapport invalides.");ok=FALSE;}
  LocalReportRequest request={title,comment,"MINIMAL",generated_at,
      (const char *const *)objects,count,evidence,timeline,infrastructure};
  GBytes *document=database!=NULL?local_report_service_build(database,
      manifest.investigation_id,&request,(LocalReportLimits){64U,256U,256U,
      1024U*1024U},error):NULL;
  if(document!=NULL){gsize size=0U;const char *data=g_bytes_get_data(document,&size);
    ok=fwrite(data,1,size,stdout)==size&&fputc('\n',stdout)!=EOF;}else ok=FALSE;
  g_clear_pointer(&document,g_bytes_unref);database_close(database);
  g_free(database_path);manifest_clear(&manifest);g_strfreev(objects);return ok;
}

static char *derived_uuid(const char *key, const char *purpose) {
  char *input = g_strconcat(key, ":", purpose, NULL);
  char *hash = g_compute_checksum_for_string(G_CHECKSUM_SHA256, input, -1);
  char *uuid = g_strdup_printf("%.8s-%.4s-4%.3s-8%.3s-%.12s", hash, hash + 8,
                               hash + 12, hash + 15, hash + 18);
  g_free(hash);
  g_free(input);
  return uuid;
}

static void manifest_clear(Manifest *manifest) {
  g_free(manifest->investigation_id);
  g_free(manifest->eml_id);
  g_free(manifest->image_id);
  g_free(manifest->title);
  memset(manifest, 0, sizeof(*manifest));
}

static gboolean manifest_write(const char *root, const Manifest *manifest,
                               GError **error) {
  char *path = workspace_path(root, ".labfy/runtime/specimen.json");
  char *data = g_strdup_printf(
      "{\"contract\":\"labfy.local_jobs.specimen.v1\",\"synthetic\":true,"
      "\"snapshot_file\":\"core-snapshot.json\","
      "\"snapshot_contract\":\"labfy.web_graph.snapshot.v3\","
      "\"investigation_id\":\"%s\",\"eml_id\":\"%s\","
      "\"image_id\":\"%s\"}\n",
      manifest->investigation_id, manifest->eml_id, manifest->image_id);
  gboolean ok =
      g_file_set_contents(path, data, -1, error) && g_chmod(path, 0600) == 0;
  g_free(data);
  g_free(path);
  return ok;
}

static gboolean manifest_read(const char *root, Manifest *manifest,
                              GError **error) {
  char *path = workspace_path(root, ".labfy/runtime/workspace.json");
  gboolean generic = g_file_test(path, G_FILE_TEST_IS_REGULAR);
  if (!generic) {
    g_free(path);
    path = workspace_path(root, ".labfy/runtime/specimen.json");
  }
  JsonParser *parser = json_parser_new();
  gboolean ok = json_parser_load_from_file(parser, path, error);
  if (ok) {
    JsonObject *object = json_node_get_object(json_parser_get_root(parser));
    ok = object != NULL && g_strcmp0(json_object_get_string_member(object,
         "contract"), generic ? "labfy.local_workspace.v1"
                               : "labfy.local_jobs.specimen.v1") == 0;
    if (ok) {
      manifest->investigation_id =
          g_strdup(json_object_get_string_member(object, "investigation_id"));
      manifest->synthetic = !generic;
      manifest->title = g_strdup(generic
          ? json_object_get_string_member(object, "title") : "SPECIMEN J5");
      if (!generic) {
        manifest->eml_id = g_strdup(json_object_get_string_member(object, "eml_id"));
        manifest->image_id = g_strdup(json_object_get_string_member(object, "image_id"));
      }
      ok = g_uuid_string_is_valid(manifest->investigation_id) &&
           manifest->title != NULL && manifest->title[0] != '\0';
    }
  }
  if (!ok && error != NULL && *error == NULL)
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                        "Contexte d’espace local invalide.");
  g_object_unref(parser);
  g_free(path);
  return ok;
}

static gboolean create_workspace(const char *root, const char *title,
                                 GError **error) {
  if (title == NULL || !g_utf8_validate(title, -1, NULL) ||
      title[0] == '\0' || strlen(title) > 160U) {
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                        "Titre d’enquête invalide.");
    return FALSE;
  }
  char *runtime = workspace_path(root, ".labfy/runtime");
  char *database_path = workspace_path(root, "Enquete.sqlite");
  char *manifest_path = workspace_path(root, ".labfy/runtime/workspace.json");
  char *lock_path = workspace_path(root, ".labfy/lifecycle.lock");
  char *jobs_path = workspace_path(root, ".labfy/runtime/jobs.sqlite");
  char *snapshot_path = workspace_path(root, "core-snapshot.json");
  char *correlation_path = workspace_path(root, "correlation-snapshot.json");
  char *planner_path = workspace_path(root, "planner-snapshot.json");
  gboolean database_existed = g_file_test(database_path, G_FILE_TEST_EXISTS);
  gboolean manifest_existed = g_file_test(manifest_path, G_FILE_TEST_EXISTS);
  gboolean jobs_existed = g_file_test(jobs_path, G_FILE_TEST_EXISTS);
  gboolean snapshot_existed = g_file_test(snapshot_path, G_FILE_TEST_EXISTS);
  gboolean correlation_existed = g_file_test(correlation_path, G_FILE_TEST_EXISTS);
  gboolean planner_existed = g_file_test(planner_path, G_FILE_TEST_EXISTS);
  gboolean database_owned = FALSE;
  gboolean manifest_owned = FALSE;
  gboolean jobs_owned = FALSE;
  gboolean snapshot_owned = FALSE;
  gboolean correlation_owned = FALSE;
  gboolean planner_owned = FALSE;
  int lock_fd = -1;
  /* WHY: an obvious partial workspace is rejected before even creating the
   * lifecycle-lock parent. The same predicate is evaluated again under the
   * lock below to close the interprocess admission window. */
  if (database_existed || manifest_existed || jobs_existed ||
      snapshot_existed || correlation_existed || planner_existed) {
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_EXISTS,
                        "L’espace est déjà créé ou partiellement créé.");
    g_free(planner_path);g_free(correlation_path);g_free(snapshot_path);
    g_free(jobs_path);g_free(lock_path);g_free(manifest_path);
    g_free(database_path);g_free(runtime);
    return FALSE;
  }
  gboolean ok = g_mkdir_with_parents(runtime, 0700) == 0;
  if (ok) lock_fd = open(lock_path, O_CREAT | O_RDWR, 0600);
  if (lock_fd >= 0) (void)fcntl(lock_fd, F_SETFD, FD_CLOEXEC);
  if (lock_fd < 0 || flock(lock_fd, LOCK_EX | LOCK_NB) != 0) {
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_BUSY,
                        "Un propriétaire utilise déjà le cycle de vie de cet espace.");
    ok = FALSE;
  }
  /* CONTRACT: every managed artifact participates in the creation admission.
   * An orphan store or export is evidence of a partial/foreign workspace, not
   * scratch space that a new creation may open, truncate or replace. */
  if (ok && (g_file_test(database_path, G_FILE_TEST_EXISTS) ||
             g_file_test(manifest_path, G_FILE_TEST_EXISTS) ||
             g_file_test(jobs_path, G_FILE_TEST_EXISTS) ||
             g_file_test(snapshot_path, G_FILE_TEST_EXISTS) ||
             g_file_test(correlation_path, G_FILE_TEST_EXISTS) ||
             g_file_test(planner_path, G_FILE_TEST_EXISTS))) {
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_EXISTS,
                        "L’espace est déjà créé ou partiellement créé.");
    ok = FALSE;
  }
  if (ok) {
    ok = database_initialize(database_path, title, root);
    database_owned = !database_existed &&
                     g_file_test(database_path, G_FILE_TEST_IS_REGULAR);
  }
  Database *database = ok ? database_open(database_path) : NULL;
  InvestigationRecord *record = database != NULL
      ? investigation_dao_load(database) : NULL;
  const char *id = record != NULL ? investigation_record_get_id(record) : NULL;
  JsonBuilder *builder = ok ? json_builder_new() : NULL;
  JsonGenerator *generator = NULL;
  JsonNode *node = NULL;
  char *data = NULL;
  if (ok && database != NULL && id != NULL) {
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder,"contract");json_builder_add_string_value(builder,"labfy.local_workspace.v1");
    json_builder_set_member_name(builder,"version");json_builder_add_int_value(builder,1);
    json_builder_set_member_name(builder,"investigation_id");json_builder_add_string_value(builder,id);
    json_builder_set_member_name(builder,"title");json_builder_add_string_value(builder,title);
    json_builder_set_member_name(builder,"mode");json_builder_add_string_value(builder,"local_experimental");
    json_builder_set_member_name(builder,"database");json_builder_add_string_value(builder,"Enquete.sqlite");
    json_builder_end_object(builder);
    generator=json_generator_new();node=json_builder_get_root(builder);
    json_generator_set_root(generator,node);data=json_generator_to_data(generator,NULL);
    ok = data != NULL && write_private_atomic(manifest_path, data, error);
    manifest_owned = ok && !manifest_existed;
  } else if (ok) {
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                        "La base créée ne fournit pas d’identité d’enquête.");
    ok = FALSE;
  }
  LocalJobStore *store = ok ? local_job_store_create(jobs_path, id, error) : NULL;
  jobs_owned = !jobs_existed && g_file_test(jobs_path, G_FILE_TEST_IS_REGULAR);
  ok = ok && store != NULL;
  if (ok) {
    LocalCapabilityRegistry *registry = local_capability_registry_new(error);
    CoreGraphLimits limits = {500U, 1000U, 1024U * 1024U};
    CoreGraphSnapshot *snapshot = registry != NULL
        ? core_graph_projection_service_collect_operational(database, limits,
                                                             registry, error)
        : NULL;
    ok = snapshot != NULL && core_graph_snapshot_write_atomic(
        snapshot, FALSE, snapshot_path, error) &&
        export_correlations(root, database, id, error) &&
        export_planner(root, database, store, registry, id, error);
    snapshot_owned = !snapshot_existed &&
        g_file_test(snapshot_path, G_FILE_TEST_IS_REGULAR);
    correlation_owned = !correlation_existed &&
        g_file_test(correlation_path, G_FILE_TEST_IS_REGULAR);
    planner_owned = !planner_existed &&
        g_file_test(planner_path, G_FILE_TEST_IS_REGULAR);
    core_graph_snapshot_free(snapshot);
    local_capability_registry_free(registry);
  }
  local_job_store_close(store); g_free(data);
  if (node != NULL) json_node_free(node);
  g_clear_object(&generator);g_clear_object(&builder);
  investigation_record_free(record);
  database_close(database);
  /* OWNERSHIP: failure compensation may remove only artifacts created by this
   * invocation. A refusal on an existing or partial workspace owns nothing. */
  if (!ok && manifest_owned) (void)g_remove(manifest_path);
  if (!ok && database_owned) (void)g_remove(database_path);
  if (!ok && jobs_owned) (void)g_remove(jobs_path);
  if (!ok && snapshot_owned) (void)g_remove(snapshot_path);
  if (!ok && correlation_owned) (void)g_remove(correlation_path);
  if (!ok && planner_owned) (void)g_remove(planner_path);
  if (lock_fd >= 0) { (void)flock(lock_fd, LOCK_UN); close(lock_fd); }
  g_free(planner_path);g_free(correlation_path);g_free(snapshot_path);
  g_free(jobs_path);g_free(lock_path);g_free(manifest_path);
  g_free(database_path);g_free(runtime);
  return ok;
}

static gboolean write_private_atomic(const char *path, const char *contents,
                                     GError **error) {
  GFile *file = g_file_new_for_path(path);
  gboolean ok = g_file_replace_contents(file, contents, strlen(contents), NULL,
      FALSE, G_FILE_CREATE_PRIVATE, NULL, NULL, error);
  if (ok && g_chmod(path, 0600) != 0) {
    g_set_error(error, G_IO_ERROR, g_io_error_from_errno(errno),
                "Permissions privées impossibles pour %s.", path);
    ok = FALSE;
  }
  g_object_unref(file);
  return ok;
}

static char *import_intent_json(const Manifest *manifest, const char *upload_id,
    const char *key, const char *evidence_id, const char *original_name,
    const char *type, guint64 size, const char *sha256, const char *source,
    const char *description) {
  JsonBuilder *builder = json_builder_new();
  json_builder_begin_object(builder);
#define ADD_STRING(name, value) do { json_builder_set_member_name(builder, name); \
  json_builder_add_string_value(builder, value); } while (0)
  ADD_STRING("contract", "labfy.local_import.intent.v1");
  json_builder_set_member_name(builder, "version");
  json_builder_add_int_value(builder, 1);
  ADD_STRING("investigation_id", manifest->investigation_id);
  ADD_STRING("upload_id", upload_id);
  ADD_STRING("idempotency_key", key);
  ADD_STRING("evidence_id", evidence_id);
  ADD_STRING("original_name", original_name);
  ADD_STRING("type", type);
  json_builder_set_member_name(builder, "size");
  json_builder_add_int_value(builder, (gint64)size);
  ADD_STRING("sha256", sha256);
  ADD_STRING("source", source != NULL ? source : "");
  ADD_STRING("description", description != NULL ? description : "");
#undef ADD_STRING
  json_builder_end_object(builder);
  JsonGenerator *generator = json_generator_new();
  JsonNode *node = json_builder_get_root(builder);
  json_generator_set_root(generator, node);
  char *result = json_generator_to_data(generator, NULL);
  json_node_free(node);
  g_object_unref(generator);
  g_object_unref(builder);
  return result;
}

static gboolean receipt_matches(const char *path, const char *intent_hash,
                                const char *evidence_id, char **out_state,
                                GError **error) {
  JsonParser *parser = json_parser_new();
  gboolean ok = json_parser_load_from_file(parser, path, error);
  JsonObject *object = ok ? json_node_get_object(json_parser_get_root(parser)) : NULL;
  const char *contract = object != NULL
      ? json_object_get_string_member_with_default(object, "contract", NULL) : NULL;
  const char *saved_hash = object != NULL
      ? json_object_get_string_member_with_default(object, "intent_sha256", NULL) : NULL;
  const char *saved_id = object != NULL
      ? json_object_get_string_member_with_default(object, "evidence_id", NULL) : NULL;
  const char *state = object != NULL
      ? json_object_get_string_member_with_default(object, "state", NULL) : NULL;
  ok = ok && g_strcmp0(contract, "labfy.local_import.receipt.v1") == 0 &&
       g_strcmp0(saved_hash, intent_hash) == 0 &&
       g_strcmp0(saved_id, evidence_id) == 0 &&
       (g_strcmp0(state, "CONFIRMING") == 0 ||
        g_strcmp0(state, "IMPORTED") == 0);
  if (!ok && error != NULL && *error == NULL)
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                        "Reçu d’import tronqué, incohérent ou contradictoire.");
  if (ok && out_state != NULL) *out_state = g_strdup(state);
  g_object_unref(parser);
  return ok;
}

static gboolean upload_intent_matches(const char *path,
    const Manifest *manifest, const char *upload_id, const char *key,
    const char *evidence_id, const char *original_name, const char *type,
    guint64 expected_size, const char *sha256, const char *source,
    const char *description, GError **error) {
  if (g_file_test(path, G_FILE_TEST_IS_SYMLINK) ||
      !g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                        "Intention durable d’import absente ou non régulière.");
    return FALSE;
  }
  JsonParser *parser = json_parser_new();
  gboolean loaded = json_parser_load_from_file(parser, path, error);
  JsonObject *object = loaded
      ? json_node_get_object(json_parser_get_root(parser)) : NULL;
  JsonObject *confirmation = object != NULL &&
      json_object_has_member(object, "confirmation") &&
      JSON_NODE_HOLDS_OBJECT(json_object_get_member(object, "confirmation"))
      ? json_object_get_object_member(object, "confirmation") : NULL;
#define MEMBER(value, name) ((value) != NULL ? \
    json_object_get_string_member_with_default((value), (name), NULL) : NULL)
  const char *state = MEMBER(object, "state");
  gint64 saved_size = object != NULL
      ? json_object_get_int_member_with_default(object, "expected_size", -1)
      : -1;
  gboolean ok = loaded && object != NULL && confirmation != NULL &&
      g_strcmp0(MEMBER(object, "contract"), "labfy.local_upload.v1") == 0 &&
      g_strcmp0(MEMBER(object, "upload_id"), upload_id) == 0 &&
      g_strcmp0(MEMBER(object, "workspace_id"), manifest->investigation_id) == 0 &&
      g_strcmp0(MEMBER(object, "evidence_id"), evidence_id) == 0 &&
      g_strcmp0(MEMBER(object, "name"), original_name) == 0 &&
      g_strcmp0(MEMBER(object, "recognized_type"), type) == 0 &&
      saved_size >= 0 && (guint64)saved_size == expected_size &&
      g_strcmp0(MEMBER(object, "sha256"), sha256) == 0 &&
      (g_strcmp0(state, "CONFIRMING") == 0 ||
       g_strcmp0(state, "IMPORTED") == 0) &&
      g_strcmp0(MEMBER(confirmation, "contract"),
                "labfy.local_import.intent.v1") == 0 &&
      g_strcmp0(MEMBER(confirmation, "idempotency_key"), key) == 0 &&
      g_strcmp0(MEMBER(confirmation, "source"), source != NULL ? source : "") == 0 &&
      g_strcmp0(MEMBER(confirmation, "description"),
                description != NULL ? description : "") == 0;
#undef MEMBER
  if (!ok && error != NULL && *error == NULL)
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                        "Intention durable d’import incohérente ou contradictoire.");
  g_object_unref(parser);
  return ok;
}

static gboolean validate_published_evidence(const char *root,
    const EvidenceRecord *record, const char *evidence_id,
    const char *original_name, const char *type, guint64 expected_size,
    const char *sha256, const char *source, const char *description,
    GError **error) {
  const char *relative = evidence_record_get_relative_path(record);
  const char *saved_source = evidence_record_get_source(record);
  const char *saved_description = evidence_record_get_description(record);
  gboolean metadata_ok =
      g_strcmp0(evidence_record_get_identifier(record), evidence_id) == 0 &&
      g_strcmp0(evidence_record_get_original_name(record), original_name) == 0 &&
      g_strcmp0(evidence_record_get_type_identifier(record), type) == 0 &&
      evidence_record_get_size_bytes(record) == expected_size &&
      g_ascii_strcasecmp(evidence_record_get_sha256(record), sha256) == 0 &&
      g_strcmp0(saved_source != NULL ? saved_source : "",
                source != NULL ? source : "") == 0 &&
      g_strcmp0(saved_description != NULL ? saved_description : "",
                description != NULL ? description : "") == 0;
  if (!metadata_ok || relative == NULL || g_path_is_absolute(relative) ||
      strstr(relative, "..") != NULL) {
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                        "Record V20 incohérent avec l’intention durable.");
    return FALSE;
  }
  char *published = g_build_filename(root, relative, NULL);
  struct stat status = {0};
  char *actual = NULL;
  guint64 actual_size = 0U;
  gboolean ok = !g_file_test(published, G_FILE_TEST_IS_SYMLINK) &&
      stat(published, &status) == 0 && S_ISREG(status.st_mode) &&
      file_hash_compute_sha256(published, NULL, &actual, &actual_size, error) &&
      actual_size == expected_size && g_ascii_strcasecmp(actual, sha256) == 0;
  if (!ok && error != NULL && *error == NULL)
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                        "Original publié absent, spécial, symbolique ou altéré.");
  g_free(actual);
  g_free(published);
  return ok;
}

static gboolean confirm_import(const char *root, const char *upload_id,
    const char *key, const char *evidence_id, const char *original_name,
    const char *type, const char *size_text, const char *sha256,
    const char *source, const char *description, gboolean crash_after_commit,
    GError **error) {
  if (!g_uuid_string_is_valid(upload_id) || !g_uuid_string_is_valid(key) ||
      !g_uuid_string_is_valid(evidence_id) || original_name == NULL ||
      (g_strcmp0(type, "email") != 0 && g_strcmp0(type, "photo") != 0) ||
      sha256 == NULL || strlen(sha256) != 64U) {
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                        "Intention d’import invalide."); return FALSE;
  }
  char *end = NULL; guint64 expected_size = g_ascii_strtoull(size_text, &end, 10);
  if (end == size_text || *end != '\0' || expected_size > 4U * 1024U * 1024U) {
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                        "Taille d’import invalide."); return FALSE;
  }
  Manifest manifest = {0};
  if (!manifest_read(root, &manifest, error)) return FALSE;
  char *staged = g_build_filename(root, ".labfy", "uploads", upload_id,
                                  "payload", NULL);
  char *upload_intent = g_build_filename(root, ".labfy", "uploads", upload_id,
                                         "intent.json", NULL);
  char *database_path = workspace_path(root, "Enquete.sqlite");
  char *receipt_dir = workspace_path(root, ".labfy/imports");
  char *key_hash = g_compute_checksum_for_string(G_CHECKSUM_SHA256, key, -1);
  char *receipt = g_build_filename(receipt_dir, key_hash, NULL);
  char *canonical = import_intent_json(&manifest, upload_id, key, evidence_id,
      original_name, type, expected_size, sha256, source, description);
  char *intent = canonical != NULL
      ? g_compute_checksum_for_string(G_CHECKSUM_SHA256, canonical, -1) : NULL;
  /* INVARIANT: deux confirmations concurrentes du même espace observent le
   * reçu ou la preuve publiée par leur prédécesseur ; elles ne copient jamais
   * simultanément le même UUID réservé. */
  char *import_lock_path=workspace_path(root,".labfy/import.lock");
  int import_lock=open(import_lock_path,O_CREAT|O_RDWR,0600);
  if(import_lock<0||flock(import_lock,LOCK_EX)!=0){
    g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_BUSY,
                        "Verrou de publication indisponible.");
    if (import_lock >= 0)
      close(import_lock);
    g_free(import_lock_path);
    manifest_clear(&manifest);g_free(upload_intent);g_free(staged);
    g_free(canonical);g_free(intent);
    g_free(database_path);g_free(receipt_dir);g_free(key_hash);g_free(receipt);
    return FALSE;
  }
  Database *database = database_open(database_path);
  EvidenceDao *dao = database != NULL ? evidence_dao_new(database, error) : NULL;
  InvestigationRecord *investigation = database != NULL
      ? investigation_dao_load(database) : NULL;
  gboolean ok = canonical != NULL && intent != NULL && database != NULL &&
                dao != NULL && investigation != NULL &&
                g_strcmp0(investigation_record_get_id(investigation),
                          manifest.investigation_id) == 0 &&
                g_mkdir_with_parents(receipt_dir, 0700) == 0;
  if (!ok && error != NULL && *error == NULL)
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                        "Identité métier de l’espace incohérente.");
  /* INVARIANT: argv is only transport. The atomically published upload intent
   * is the authority for every identifier and every free-text field across a
   * server/process crash; no receipt or business row precedes this comparison. */
  if (ok)
    ok = upload_intent_matches(upload_intent, &manifest, upload_id, key,
        evidence_id, original_name, type, expected_size, sha256, source,
        description, error);
  char *receipt_state = NULL;
  if (ok && g_file_test(receipt, G_FILE_TEST_EXISTS))
    ok = g_file_test(receipt, G_FILE_TEST_IS_REGULAR) &&
         receipt_matches(receipt, intent, evidence_id, &receipt_state, error);
  else if (ok) {
    char *receipt_data = g_strdup_printf(
        "{\"contract\":\"labfy.local_import.receipt.v1\","
        "\"evidence_id\":\"%s\",\"intent_sha256\":\"%s\","
        "\"state\":\"CONFIRMING\",\"version\":1}\n",
        evidence_id, intent);
    ok = write_private_atomic(receipt, receipt_data, error);
    g_free(receipt_data);
    if (ok) receipt_state = g_strdup("CONFIRMING");
  }
  /* INVARIANT: the record lookup occurs after acquiring the publication lock;
   * no stale pre-lock object may decide whether an import already committed. */
  EvidenceRecord *existing = ok
      ? evidence_dao_find_by_identifier(dao, evidence_id, error) : NULL;
  if (ok && existing != NULL) {
    ok = validate_published_evidence(root, existing, evidence_id, original_name,
        type, expected_size, sha256, source, description, error);
  } else if (ok && (error == NULL || *error == NULL)) {
    char *actual = NULL;
    guint64 actual_size = 0U;
    ok = file_hash_compute_sha256(staged, NULL, &actual, &actual_size, error) &&
         actual_size == expected_size &&
         g_ascii_strcasecmp(actual, sha256) == 0;
    if (!ok && error != NULL && *error == NULL)
      g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                          "Empreinte du fichier préparé incohérente.");
    g_free(actual);
  }
  if (ok && existing == NULL) {
    char *folder = g_strcmp0(type, "email") == 0 ? "Emails" : "Images";
    char *destination = g_build_filename(root, "01_Preuves_Originales", folder, NULL);
    char *relative = g_build_filename("01_Preuves_Originales", folder, NULL);
    EvidenceImporter *importer = evidence_importer_new(database, error);
    EvidenceImportRequest request = {.source_path=staged,
      .destination_directory=destination,.relative_directory=relative,
      .type_identifier=type,.reserved_identifier=evidence_id,
      .original_name=original_name,.collected_at=NULL,
      .source=(source != NULL && source[0] != '\0') ? source : NULL,
      .description=(description != NULL && description[0] != '\0') ? description : NULL};
    ok = importer != NULL && g_mkdir_with_parents(destination, 0700) == 0;
    EvidenceRecord *record = ok ? evidence_importer_import(importer,&request,NULL,error):NULL;
    ok = record != NULL && validate_published_evidence(root, record, evidence_id,
         original_name, type, expected_size, sha256, source, description, error);
    evidence_record_free(record); evidence_importer_free(importer);
    g_free(relative); g_free(destination);
#ifdef LOCAL_JOBS_ENABLE_CRASH_HOOK
    if (ok && crash_after_commit)
      _exit(87);
#else
    (void)crash_after_commit;
#endif
  }
  if (ok) {
    char *receipt_data = g_strdup_printf(
        "{\"contract\":\"labfy.local_import.receipt.v1\","
        "\"evidence_id\":\"%s\",\"intent_sha256\":\"%s\","
        "\"state\":\"IMPORTED\",\"version\":1}\n",
        evidence_id, intent);
    ok = write_private_atomic(receipt, receipt_data, error);
    g_free(receipt_data);
  }
  gboolean projection_ok = TRUE;
  if (ok) {
    LocalCapabilityRegistry *registry = local_capability_registry_new(error);
    CoreGraphLimits limits={500U,1000U,1024U*1024U};
    CoreGraphSnapshot *snapshot=registry!=NULL?core_graph_projection_service_collect_operational(database,limits,registry,error):NULL;
    char *path=workspace_path(root,"core-snapshot.json");
    Manifest store_manifest = {0};
    LocalJobStore *store=open_store(root,&store_manifest,error);
    projection_ok=snapshot!=NULL&&store!=NULL&&core_graph_snapshot_write_atomic(snapshot,FALSE,path,error)&&
       export_correlations(root,database,manifest.investigation_id,error)&&
       export_planner(root,database,store,registry,manifest.investigation_id,error);
    local_job_store_close(store);manifest_clear(&store_manifest);
    g_free(path);core_graph_snapshot_free(snapshot);
    local_capability_registry_free(registry);
  }
  /* CONTRACT: snapshot export is retryable presentation work. Once V20 and the
   * receipt agree, an export failure cannot turn the import back into pending. */
  if (ok && !projection_ok) g_clear_error(error);
  if (ok) printf("{\"contract\":\"labfy.local_import.result.v1\","
      "\"state\":\"IMPORTED\",\"evidence_id\":\"%s\","
      "\"projection_available\":%s}\n", evidence_id,
      projection_ok ? "true" : "false");
  evidence_record_free(existing);investigation_record_free(investigation);
  evidence_dao_free(dao);database_close(database);
  (void)flock(import_lock,LOCK_UN);close(import_lock);g_free(import_lock_path);
  g_free(receipt_state);g_free(canonical);g_free(intent);g_free(receipt);
  g_free(key_hash);g_free(receipt_dir);g_free(database_path);g_free(staged);
  g_free(upload_intent);
  manifest_clear(&manifest);
  return ok;
}

static EvidenceRecord *import_file(Database *database, const char *root,
                                   const char *source, const char *folder,
                                   const char *type, GError **error) {
  char *destination =
      g_build_filename(root, "01_Preuves_Originales", folder, NULL);
  EvidenceImporter *importer = evidence_importer_new(database, error);
  EvidenceImportRequest request = {
      .source_path = source,
      .destination_directory = destination,
      .relative_directory = g_strconcat("01_Preuves_Originales/", folder, NULL),
      .type_identifier = type,
      .collected_at = timestamp,
      .source = "Générateur SPECIMEN J5",
      .description = "Preuve fictive pour reprise locale bornée."};
  EvidenceRecord *record = NULL;
  if (importer != NULL && g_mkdir_with_parents(destination, 0700) == 0)
    record = evidence_importer_import(importer, &request, NULL, error);
  g_free((char *)request.relative_directory);
  evidence_importer_free(importer);
  g_free(destination);
  return record;
}

static gboolean init_specimen(const char *root, GError **error) {
  char *database_path = workspace_path(root, "Enquete.sqlite");
  char *runtime = workspace_path(root, ".labfy/runtime");
  char *input = workspace_path(root, "SPECIMEN_INPUT");
  char *eml_path = g_build_filename(input, "message-j5.eml", NULL);
  char *image_path = g_build_filename(input, "image-j5.png", NULL);
  gboolean ok = !g_file_test(database_path, G_FILE_TEST_EXISTS) &&
                g_mkdir_with_parents(runtime, 0700) == 0 &&
                g_mkdir_with_parents(input, 0700) == 0 &&
                g_file_set_contents(eml_path, eml_content, -1, error);
  gsize png_size = 0;
  guchar *png = g_base64_decode(png_base64, &png_size);
  ok = ok && png != NULL &&
       g_file_set_contents(image_path, (char *)png, (gssize)png_size, error);
  g_free(png);
  ok = ok && database_initialize(database_path, "SPECIMEN J5", root);
  LocalCapabilityRegistry *registry =
      ok ? local_capability_registry_new(error) : NULL;
  const LocalCapabilityStatus *exif =
      registry != NULL ? local_capability_registry_lookup(
                             registry, LOCAL_CAPABILITY_EXIF_METADATA)
                       : NULL;
  const char *argv[] = {exif != NULL ? exif->executable : "",
                        "-config",
                        "",
                        "-overwrite_original",
                        "-XMP-dc:Creator=Alice SPECIMEN",
                        "--",
                        image_path,
                        NULL};
  LocalToolRunnerResult *tool_result = NULL;
  ok = ok && exif != NULL && exif->availability == LOCAL_CAPABILITY_READY &&
       local_tool_runner_run(exif->executable, argv, input,
                             &exif->descriptor->limits, NULL, &tool_result,
                             error) &&
       tool_result->state == LOCAL_TOOL_RUNNER_EXITED;
  local_tool_runner_result_free(tool_result);
  Database *database = ok ? database_open(database_path) : NULL;
  EvidenceRecord *eml = database != NULL ? import_file(database, root, eml_path,
                                                       "Emails", "email", error)
                                         : NULL;
  EvidenceRecord *image = eml != NULL ? import_file(database, root, image_path,
                                                    "Images", "photo", error)
                                      : NULL;
  InvestigationRecord *investigation =
      image != NULL ? investigation_dao_load(database) : NULL;
  Manifest manifest = {0};
  if (investigation != NULL) {
    manifest.investigation_id =
        g_strdup(investigation_record_get_id(investigation));
    manifest.eml_id = g_strdup(evidence_record_get_identifier(eml));
    manifest.image_id = g_strdup(evidence_record_get_identifier(image));
  }
  char *jobs_path = workspace_path(root, ".labfy/runtime/jobs.sqlite");
  LocalJobStore *store =
      investigation != NULL
          ? local_job_store_create(jobs_path, manifest.investigation_id, error)
          : NULL;
  ok = store != NULL && manifest_write(root, &manifest, error);
  if (ok) {
    CoreGraphLimits limits = {500U, 1000U, 1024U * 1024U};
    CoreGraphSnapshot *snapshot =
        core_graph_projection_service_collect_operational(database, limits,
                                                          registry, error);
    char *snapshot_path = workspace_path(root, "core-snapshot.json");
    ok = snapshot != NULL &&
         core_graph_snapshot_write_atomic(snapshot, TRUE, snapshot_path, error);
    g_free(snapshot_path);
    core_graph_snapshot_free(snapshot);
    if (ok)
      ok = export_correlations(root, database, manifest.investigation_id, error);
  }
  local_job_store_close(store);
  manifest_clear(&manifest);
  investigation_record_free(investigation);
  evidence_record_free(image);
  evidence_record_free(eml);
  database_close(database);
  local_capability_registry_free(registry);
  g_free(jobs_path);
  g_free(image_path);
  g_free(eml_path);
  g_free(input);
  g_free(runtime);
  g_free(database_path);
  return ok;
}

static gboolean init_j7_specimen(const char *root, GError **error) {
  if (!init_specimen(root, error)) return FALSE;
  char *input = workspace_path(root, "SPECIMEN_INPUT");
  const char *names[] = {"message-j7-a.eml", "message-j7-b.eml",
                         "message-j7-c.eml", "message-j7-b-copy.eml"};
  const char *contents[] = {j7_eml_a, j7_eml_b, j7_eml_c, j7_eml_b};
  char *database_path = workspace_path(root, "Enquete.sqlite");
  Database *database = database_open(database_path);
  GPtrArray *identifiers = g_ptr_array_new_with_free_func(g_free);
  gboolean ok = database != NULL;
  for (guint i = 0; ok && i < G_N_ELEMENTS(names); i++) {
    char *path = g_build_filename(input, names[i], NULL);
    ok = g_file_set_contents(path, contents[i], -1, error);
    EvidenceRecord *record = ok ? import_file(database, root, path, "Emails",
                                               "email", error) : NULL;
    ok = record != NULL;
    if (record != NULL)
      g_ptr_array_add(identifiers,
          g_strdup(evidence_record_get_identifier(record)));
    evidence_record_free(record); g_free(path);
  }
  Manifest manifest = {0};
  if (ok) ok = manifest_read(root, &manifest, error);
  if (ok) {
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "contract");
    json_builder_add_string_value(builder, "labfy.j7.specimen.v1");
    json_builder_set_member_name(builder, "synthetic");
    json_builder_add_boolean_value(builder, TRUE);
    json_builder_set_member_name(builder, "evidence_ids");
    json_builder_begin_array(builder);
    json_builder_add_string_value(builder, manifest.eml_id);
    json_builder_add_string_value(builder, manifest.image_id);
    for (guint i = 0; i < identifiers->len; i++)
      json_builder_add_string_value(builder, g_ptr_array_index(identifiers, i));
    json_builder_end_array(builder); json_builder_end_object(builder);
    JsonGenerator *generator = json_generator_new();
    JsonNode *node = json_builder_get_root(builder);
    json_generator_set_root(generator, node);
    char *data = json_generator_to_data(generator, NULL);
    char *path = workspace_path(root, ".labfy/runtime/j7-specimen.json");
    ok = data != NULL && g_file_set_contents(path, data, -1, error) &&
         g_chmod(path, 0600) == 0;
    g_free(path); g_free(data); json_node_free(node);
    g_object_unref(generator); g_object_unref(builder);
  }
  if (ok) {
    LocalCapabilityRegistry *registry = local_capability_registry_new(error);
    CoreGraphLimits limits = {500U, 1000U, 1024U * 1024U};
    CoreGraphSnapshot *graph = registry != NULL
        ? core_graph_projection_service_collect_operational(database, limits,
                                                             registry, error)
        : NULL;
    char *graph_path = workspace_path(root, "core-snapshot.json");
    ok = graph != NULL && core_graph_snapshot_write_atomic(
        graph, TRUE, graph_path, error) && export_correlations(
        root, database, manifest.investigation_id, error);
    g_free(graph_path); core_graph_snapshot_free(graph);
    local_capability_registry_free(registry);
  }
  manifest_clear(&manifest); g_ptr_array_unref(identifiers);
  database_close(database); g_free(database_path); g_free(input);
  return ok;
}

static LocalJobStore *open_store(const char *root, Manifest *manifest,
                                 GError **error) {
  if (!manifest_read(root, manifest, error))
    return NULL;
  char *path = workspace_path(root, ".labfy/runtime/jobs.sqlite");
  LocalJobStore *store =
      local_job_store_open(path, manifest->investigation_id, FALSE, error);
  g_free(path);
  return store;
}

static gboolean enqueue_jobs(const char *root, GError **error) {
  Manifest manifest = {0};
  LocalJobStore *store = open_store(root, &manifest, error);
  char *database_path = workspace_path(root, "Enquete.sqlite");
  Database *database = store != NULL ? database_open(database_path) : NULL;
  EvidenceDao *dao =
      database != NULL ? evidence_dao_new(database, error) : NULL;
  EvidenceRecord *eml =
      dao != NULL ? evidence_dao_find_by_identifier(dao, manifest.eml_id, error)
                  : NULL;
  EvidenceRecord *image =
      dao != NULL
          ? evidence_dao_find_by_identifier(dao, manifest.image_id, error)
          : NULL;
  LocalJobSubmission eml_job = {EML_JOB,
                                EML_REQUEST,
                                manifest.eml_id,
                                EML_DERIVATIVE,
                                LOCAL_CAPABILITY_EML_HEADERS,
                                EML_ANALYSIS_TOOL_ID,
                                EML_ANALYSIS_TOOL_VERSION,
                                eml ? evidence_record_get_sha256(eml) : NULL,
                                eml ? evidence_record_get_size_bytes(eml) : 0,
                                timestamp,
                                "{\"mode\":\"headers\"}",
                                NULL,
                                3};
  LocalJobSubmission exif_job = {
      EXIF_JOB,
      EXIF_REQUEST,
      manifest.image_id,
      EXIF_DERIVATIVE,
      LOCAL_CAPABILITY_EXIF_METADATA,
      "exiftool-json",
      "1",
      image ? evidence_record_get_sha256(image) : NULL,
      image ? evidence_record_get_size_bytes(image) : 0,
      timestamp,
      "{\"groups\":\"G1\",\"numeric_values\":true}",
      EML_JOB,
      3};
  gboolean reused = FALSE;
  gboolean ok = eml != NULL && image != NULL &&
                local_job_store_enqueue(store, &eml_job, &reused, error) &&
                local_job_store_enqueue(store, &exif_job, &reused, error);
  evidence_record_free(image);
  evidence_record_free(eml);
  evidence_dao_free(dao);
  database_close(database);
  local_job_store_close(store);
  manifest_clear(&manifest);
  g_free(database_path);
  return ok;
}

static gboolean submit_job(const char *root, const char *evidence_id,
                           const char *capability_id, const char *key,
                           char **out_job_id, GError **error) {
  if (!g_uuid_string_is_valid(evidence_id) || !g_uuid_string_is_valid(key) ||
      (g_strcmp0(capability_id, LOCAL_CAPABILITY_EML_HEADERS) != 0 &&
       g_strcmp0(capability_id, LOCAL_CAPABILITY_EXIF_METADATA) != 0)) {
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                        "Intention d'analyse inconnue ou invalide.");
    return FALSE;
  }
  Manifest manifest = {0};
  LocalJobStore *store = open_store(root, &manifest, error);
  char *database_path = workspace_path(root, "Enquete.sqlite");
  Database *database = store != NULL ? database_open(database_path) : NULL;
  EvidenceDao *dao =
      database != NULL ? evidence_dao_new(database, error) : NULL;
  EvidenceRecord *evidence =
      dao != NULL ? evidence_dao_find_by_identifier(dao, evidence_id, error)
                  : NULL;
  LocalCapabilityRegistry *registry =
      evidence != NULL ? local_capability_registry_new(error) : NULL;
  const LocalCapabilityStatus *status =
      registry != NULL
          ? local_capability_registry_lookup(registry, capability_id)
          : NULL;
  char *reason = NULL;
  const char *type =
      evidence != NULL ? evidence_record_get_type_identifier(evidence) : NULL;
  const char *mime = g_strcmp0(type, "email") == 0 ? "message/rfc822"
                     : g_strcmp0(type, "photo") == 0
                         ? "image/unknown"
                         : "application/octet-stream";
  gboolean applicable =
      status != NULL &&
      local_capability_status_applies(
          status, mime, evidence_record_get_relative_path(evidence), &reason);
  if (!applicable || status->availability != LOCAL_CAPABILITY_READY) {
    g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                "Capability indisponible pour cette preuve : %s",
                reason != NULL   ? reason
                : status != NULL ? status->reason
                                 : "capability absente");
  }
  char *request_id = derived_uuid(key, "request");
  char *derivative_id = derived_uuid(key, "derivative");
  char *requested_at = now_iso();
  LocalJobSubmission submission = {
      .job_id = key,
      .request_id = request_id,
      .source_evidence_id = evidence_id,
      .derivative_evidence_id = derivative_id,
      .capability_id = capability_id,
      .adapter_id = g_strcmp0(capability_id, LOCAL_CAPABILITY_EML_HEADERS) == 0
                        ? EML_ANALYSIS_TOOL_ID
                        : "exiftool-json",
      .adapter_version = "1",
      .source_sha256 =
          evidence != NULL ? evidence_record_get_sha256(evidence) : NULL,
      .source_size =
          evidence != NULL ? evidence_record_get_size_bytes(evidence) : 0,
      .requested_at = requested_at,
      .parameters_json =
          g_strcmp0(capability_id, LOCAL_CAPABILITY_EML_HEADERS) == 0
              ? "{\"mode\":\"headers\"}"
              : "{\"groups\":\"G1\",\"numeric_values\":true}",
      .max_attempts = 3};
  gboolean reused = FALSE;
  gboolean ok = evidence != NULL && applicable &&
                status->availability == LOCAL_CAPABILITY_READY &&
                local_job_store_enqueue(store, &submission, &reused, error);
  if (ok && out_job_id != NULL)
    *out_job_id = g_strdup(key);
  g_free(requested_at);
  g_free(derivative_id);
  g_free(request_id);
  g_free(reason);
  local_capability_registry_free(registry);
  evidence_record_free(evidence);
  evidence_dao_free(dao);
  database_close(database);
  g_free(database_path);
  local_job_store_close(store);
  manifest_clear(&manifest);
  return ok;
}

static gboolean submit_plan(const char *root, const char *revision,
                            const char *profile, const char *key,
                            const char *ids_csv, gboolean crash_after_admission,
                            GError **error) {
  if (!revision || !profile || !g_uuid_string_is_valid(key) || !ids_csv) {
    g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_INVALID_ARGUMENT,"Intention de plan invalide."); return FALSE;
  }
  char *planner_path=workspace_path(root,"planner-snapshot.json"); JsonParser *parser=json_parser_new();
  gboolean ok=json_parser_load_from_file(parser,planner_path,error); g_free(planner_path);
  JsonObject *root_object=ok?json_node_get_object(json_parser_get_root(parser)):NULL;
  if(ok && g_strcmp0(json_object_get_string_member(root_object,"input_revision"),revision)!=0){
    g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_INVALID_DATA,"Projection planner périmée.");ok=FALSE;}
  gchar **ids=g_strsplit(ids_csv,",",9); gsize count=g_strv_length(ids);
  guint max_analyses=g_strcmp0(profile,"SPECIMEN_SMALL")==0?2U:g_strcmp0(profile,"LOCAL_PRUDENT")==0?8U:0U;
  guint max_attempts=g_strcmp0(profile,"SPECIMEN_SMALL")==0?3U:16U;
  guint64 max_bytes=g_strcmp0(profile,"SPECIMEN_SMALL")==0?8U*1024U*1024U:256U*1024U*1024U;
  guint64 max_ms=g_strcmp0(profile,"SPECIMEN_SMALL")==0?30000U:240000U;
#ifdef LOCAL_JOBS_ENABLE_CRASH_HOOK
  const char *test_max_ms = g_getenv("LABFY_TEST_PLAN_ACTIVE_MS");
  if (test_max_ms != NULL && test_max_ms[0] != '\0')
    max_ms = g_ascii_strtoull(test_max_ms, NULL, 10);
#endif
  if(ok && (count==0||count>max_analyses)){g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_NO_SPACE,"Profil de budget insuffisant.");ok=FALSE;}
  Manifest manifest={0}; LocalJobStore *store=ok?open_store(root,&manifest,error):NULL;
  char *database_path=workspace_path(root,"Enquete.sqlite"); Database *database=store?database_open(database_path):NULL;
  EvidenceDao *dao=database?evidence_dao_new(database,error):NULL; LocalCapabilityRegistry *registry=dao?local_capability_registry_new(error):NULL;
  LocalJobSubmission *jobs=g_new0(LocalJobSubmission,count); GPtrArray *owned=g_ptr_array_new_with_free_func(g_free);
  JsonArray *recommendations=ok?json_object_get_array_member(root_object,"recommendations"):NULL;
  for(gsize i=0;ok&&i<count;i++){
    JsonObject *match=NULL; for(guint j=0;j<json_array_get_length(recommendations);j++){JsonObject *candidate=json_array_get_object_element(recommendations,j);if(g_strcmp0(json_object_get_string_member(candidate,"id"),ids[i])==0){match=candidate;break;}}
    if(!match||!json_object_get_boolean_member(match,"available")){g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_NOT_FOUND,"Recommandation absente ou indisponible.");ok=FALSE;break;}
    const char *evidence_id=json_object_get_string_member(match,"object_id"); const char *cap=json_object_get_string_member(match,"capability_id"); EvidenceRecord *evidence=evidence_dao_find_by_identifier(dao,evidence_id,error); const LocalCapabilityStatus *status=local_capability_registry_lookup(registry,cap);
    if(!evidence||!status||status->availability!=LOCAL_CAPABILITY_READY||g_strcmp0(evidence_record_get_sha256(evidence),json_object_get_string_member(match,"source_sha256"))!=0){evidence_record_free(evidence);g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_INVALID_DATA,"Précondition de recommandation modifiée.");ok=FALSE;break;}
    char *seed=g_strconcat(key,":",ids[i],NULL);char *job=derived_uuid(seed,"job"),*request=derived_uuid(seed,"request"),*derivative=derived_uuid(seed,"derivative");g_free(seed);char *at=now_iso();g_ptr_array_add(owned,job);g_ptr_array_add(owned,request);g_ptr_array_add(owned,derivative);g_ptr_array_add(owned,at);
    jobs[i]=(LocalJobSubmission){job,request,evidence_id,derivative,cap,status->descriptor->adapter_id,status->descriptor->adapter_version,evidence_record_get_sha256(evidence),evidence_record_get_size_bytes(evidence),at,g_strcmp0(cap,LOCAL_CAPABILITY_EML_HEADERS)==0?"{\"mode\":\"headers\"}":"{\"groups\":\"G1\",\"numeric_values\":true}",NULL,g_strcmp0(profile,"SPECIMEN_SMALL")==0?1U:2U};
    /* Les champs empruntés au record doivent survivre jusqu'à l'admission. */
    jobs[i].source_evidence_id=g_strdup(evidence_id);jobs[i].source_sha256=g_strdup(evidence_record_get_sha256(evidence));g_ptr_array_add(owned,(gpointer)jobs[i].source_evidence_id);g_ptr_array_add(owned,(gpointer)jobs[i].source_sha256);evidence_record_free(evidence);
  }
  char *plan_id=derived_uuid(key,"plan");GString *intent=g_string_new(revision);g_string_append(intent,profile);for(gsize i=0;i<count;i++)g_string_append(intent,ids[i]);char *intent_hash=g_compute_checksum_for_string(G_CHECKSUM_SHA256,intent->str,-1);char *created=now_iso();LocalPlanAdmission plan={plan_id,key,intent_hash,revision,profile,created,max_analyses,max_attempts,max_bytes,max_ms};gboolean reused=FALSE;
  if(ok)ok=local_job_store_admit_plan(store,&plan,jobs,count,&reused,error);
#ifdef LOCAL_JOBS_ENABLE_CRASH_HOOK
  if (ok && crash_after_admission) {
    /* TEST CONTRACT: le marqueur est émis seulement après le COMMIT durable;
     * _exit reproduit une réponse perdue sans exécuter le premier claim. */
    (void)write(STDERR_FILENO, "J8_BARRIER_ADMISSION_COMMITTED\n", 31);
    _exit(85);
  }
#else
  (void)crash_after_admission;
#endif
  if(ok){printf("{\"contract\":\"labfy.local_plan.v1\",\"accepted\":true,\"reused\":%s,\"plan_id\":\"%s\",\"jobs\":[",reused?"true":"false",plan_id);for(gsize i=0;i<count;i++)printf("%s\"%s\"",i?",":"",jobs[i].job_id);printf("]}\n");}
  g_free(created);g_free(intent_hash);g_string_free(intent,TRUE);g_free(plan_id);g_free(jobs);g_ptr_array_unref(owned);local_capability_registry_free(registry);evidence_dao_free(dao);database_close(database);g_free(database_path);local_job_store_close(store);manifest_clear(&manifest);g_strfreev(ids);g_object_unref(parser);return ok;
}

static gboolean lookup_plan(const char *root,const char *revision,
                            const char *profile,const char *key,
                            const char *ids_csv,GError **error) {
  Manifest manifest={0};LocalJobStore *store=open_store(root,&manifest,error);
  if (!store)
    return FALSE;
  GString *intent=g_string_new(revision);g_string_append(intent,profile);
  gchar **ids=g_strsplit(ids_csv,",",9);for(guint i=0;ids[i];i++)g_string_append(intent,ids[i]);
  char *hash=g_compute_checksum_for_string(G_CHECKSUM_SHA256,intent->str,-1),*plan_id=NULL;
  gboolean ok=local_job_store_find_plan_intention(store,key,hash,&plan_id,error);
  if(ok&&plan_id)printf("{\"contract\":\"labfy.local_plan.v1\",\"found\":true,\"reused\":true,\"plan_id\":\"%s\"}\n",plan_id);
  else if(ok)printf("{\"contract\":\"labfy.local_plan.v1\",\"found\":false}\n");
  g_free(plan_id);g_free(hash);g_strfreev(ids);g_string_free(intent,TRUE);local_job_store_close(store);manifest_clear(&manifest);return ok;
}

static gboolean execute_job(const char *root, Database *database,
                            LocalCapabilityRegistry *registry,
                            LocalJobRecord *job, GCancellable *cancellable,
                            GError **error) {
  if (g_strcmp0(job->capability_id, LOCAL_CAPABILITY_EML_HEADERS) == 0) {
    EmlAnalysisPersistenceService *service =
        eml_analysis_persistence_service_new(database, root, error);
    EmlAnalysisPersistenceRequest request = {
        job->request_id, job->source_evidence_id, job->derivative_evidence_id,
        job->requested_at};
    EmlAnalysisPrepared *prepared =
        service != NULL ? eml_analysis_persistence_service_prepare(
                              service, &request, cancellable, error)
                        : NULL;
    EmlAnalysisPublicationResult *result =
        prepared != NULL ? eml_analysis_persistence_service_publish(
                               service, prepared, cancellable, error)
                         : NULL;
    gboolean ok = result != NULL;
    eml_analysis_publication_result_free(result);
    eml_analysis_prepared_free(prepared);
    eml_analysis_persistence_service_free(service);
    return ok;
  }
  if (g_strcmp0(job->capability_id, LOCAL_CAPABILITY_EXIF_METADATA) != 0) {
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                        "Capability de job inconnue ; aucune exécution.");
    return FALSE;
  }
  ExiftoolPersistenceService *service =
      exiftool_persistence_service_new(database, root, registry, error);
  ExiftoolPersistenceRequest request = {
      job->request_id, job->source_evidence_id, job->derivative_evidence_id,
      job->requested_at};
  ExiftoolPublicationResult *result =
      service != NULL
          ? exiftool_persistence_service_execute(service, &request, cancellable, error)
          : NULL;
  gboolean ok = result != NULL;
  exiftool_publication_result_free(result);
  exiftool_persistence_service_free(service);
  return ok;
}

static gboolean run_worker(const char *root, guint crash_point,
                           GError **error) {
  struct sigaction stop_action = {0};
  struct sigaction previous_term = {0};
  struct sigaction previous_int = {0};
  stop_action.sa_handler = worker_stop_handler;
  sigemptyset(&stop_action.sa_mask);
  worker_stop_requested = 0;
  local_tool_runner_reset_process_cancel();
  (void)sigaction(SIGTERM, &stop_action, &previous_term);
  (void)sigaction(SIGINT, &stop_action, &previous_int);
  Manifest manifest = {0};
  LocalJobStore *store = open_store(root, &manifest, error);
  char *lock_path = workspace_path(root, ".labfy/runtime/worker.lock");
  int lock_fd = open(lock_path, O_CREAT | O_RDWR, 0600);
  if (lock_fd >= 0)
    (void)fcntl(lock_fd, F_SETFD, FD_CLOEXEC);
  if (store == NULL || lock_fd < 0 || flock(lock_fd, LOCK_EX | LOCK_NB) != 0) {
    if (error != NULL && *error == NULL)
      g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_BUSY,
                          "Un worker possède déjà cet espace.");
    local_job_store_close(store);
    if (lock_fd >= 0)
      close(lock_fd);
    g_free(lock_path);
    manifest_clear(&manifest);
    return FALSE;
  }
  char *database_path = workspace_path(root, "Enquete.sqlite");
  Database *database = database_open(database_path);
  LocalCapabilityRegistry *registry = local_capability_registry_new(error);
  GPtrArray *jobs = local_job_store_list(store, error);
  for (guint index = 0; jobs != NULL && index < jobs->len; index++) {
    LocalJobRecord *job = g_ptr_array_index(jobs, index);
    if (job->state == LOCAL_JOB_RUNNING) {
      ExtractionDao *extraction_dao = extraction_dao_new(database, NULL);
      ExtractionRecord *published =
          extraction_dao != NULL ? extraction_dao_find_by_identifier(
                                       extraction_dao, job->request_id, NULL)
                                 : NULL;
      extraction_dao_free(extraction_dao);
      GError *replay_error = NULL;
      if (published != NULL &&
          execute_job(root, database, registry, job, NULL, &replay_error)) {
        char *now = now_iso();
        local_job_store_reconcile_interrupted(
            store, job->job_id, LOCAL_JOB_COMPLETED, "recovered_after_publish",
            "published", now, error);
        g_free(now);
      } else if (published == NULL && job->cancel_requested) {
        char *now = now_iso();
        local_job_store_reconcile_interrupted(
            store, job->job_id, LOCAL_JOB_CANCELLED,
            "cancelled_after_worker_stop", "cancelled", now, error);
        g_free(now);
      } else if (published == NULL) {
        char *now = now_iso();
        local_job_store_requeue_interrupted(
            store, job->job_id, "interrupted_before_publish", now, error);
        g_free(now);
      } else {
        char *now = now_iso();
        local_job_store_reconcile_interrupted(
            store, job->job_id, LOCAL_JOB_RECOVERY_REQUIRED,
            replay_error != NULL ? replay_error->message : "résultat ambigu",
            "blocked", now, error);
        g_free(now);
      }
      extraction_record_free(published);
      g_clear_error(&replay_error);
    }
  }
  g_clear_pointer(&jobs, g_ptr_array_unref);
  while ((error == NULL || *error == NULL) && !worker_stop_requested) {
    char *owner = g_uuid_string_random();
    char *attempt = g_uuid_string_random();
    char *now = now_iso();
    LocalJobRecord *job = NULL;
    guint reserved_active_ms = 0;
    guint requested_active_ms = 30000U;
#ifdef LOCAL_JOBS_ENABLE_CRASH_HOOK
    const char *test_active_ms = g_getenv("LABFY_TEST_PLAN_ACTIVE_MS");
    if (test_active_ms != NULL && test_active_ms[0] != '\0')
      requested_active_ms = (guint)g_ascii_strtoull(test_active_ms, NULL, 10);
#endif
    gboolean ok =
        local_job_store_claim_next_budgeted(store, owner, attempt, now,
            requested_active_ms, &reserved_active_ms, &job, error);
    g_free(now);
    g_free(attempt);
    if (!ok || job == NULL) {
      g_free(owner);
      break;
    }
#ifdef LOCAL_JOBS_ENABLE_CRASH_HOOK
    if (crash_point == 1U) {
      (void)write(STDERR_FILENO, "J8_BARRIER_CLAIM_COMMITTED\n", 27);
      _exit(84);
    }
#endif
    GError *execution_error = NULL;
    GCancellable *cancellable = g_cancellable_new();
    BudgetTimer timer = {.finished=FALSE,.timeout_ms=reserved_active_ms,
                         .cancellable=cancellable};
    g_mutex_init(&timer.mutex);g_cond_init(&timer.condition);
    GThread *timer_thread=g_thread_new("plan-budget",budget_timer_run,&timer);
    gint64 started_us=g_get_monotonic_time();
#ifdef LOCAL_JOBS_ENABLE_CRASH_HOOK
    const char *test_delay = g_getenv("LABFY_TEST_TOOL_DELAY_MS");
    if (test_delay != NULL && test_delay[0] != '\0') {
      char *marker = workspace_path(root, ".labfy/runtime/test-tool-started");
      char *pid = g_strdup_printf("%ld\n", (long)getpid());
      (void)g_file_set_contents(marker, pid, -1, NULL);
      guint delay_ms = (guint)g_ascii_strtoull(test_delay, NULL, 10);
      for (guint waited = 0U; waited < delay_ms &&
           !g_cancellable_is_cancelled(cancellable) &&
           !worker_stop_requested; waited += 10U)
        g_usleep(10U * 1000U);
      g_free(pid); g_free(marker);
    }
#endif
    if (g_cancellable_is_cancelled(cancellable) || worker_stop_requested) {
      g_set_error_literal(&execution_error, G_IO_ERROR, G_IO_ERROR_CANCELLED,
                          "Outil synthétique interrompu dans sa limite.");
      ok = FALSE;
    } else
      ok = execute_job(root, database, registry, job, cancellable, &execution_error);
    gint64 elapsed_us=g_get_monotonic_time()-started_us;
    g_mutex_lock(&timer.mutex);timer.finished=TRUE;g_cond_signal(&timer.condition);
    g_mutex_unlock(&timer.mutex);g_thread_join(timer_thread);
    g_cond_clear(&timer.condition);g_mutex_clear(&timer.mutex);
    guint elapsed_ms=(guint)MIN((guint64)G_MAXUINT,
        (guint64)MAX((gint64)1,(elapsed_us+999)/1000));
#ifdef LOCAL_JOBS_ENABLE_CRASH_HOOK
    if (ok && crash_point == 2U)
      _exit(86);
#else
    (void)crash_point;
#endif
    now = now_iso();
    const char *diagnostic = ok ? "published"
                             : execution_error != NULL
                                 ? execution_error->message
                                 : "execution_failed";
    GError *finish_error = NULL;
    gboolean budget_expired = !worker_stop_requested &&
        g_cancellable_is_cancelled(cancellable);
    LocalJobState terminal_state = worker_stop_requested && !ok
        ? LOCAL_JOB_CANCELLED : budget_expired ? LOCAL_JOB_BLOCKED
        : ok ? LOCAL_JOB_COMPLETED : LOCAL_JOB_FAILED;
    gboolean recorded = local_job_store_finish_budgeted(
        store, job->job_id, owner, terminal_state,
        worker_stop_requested && !ok ? "cancelled_after_tool_cleanup"
        : budget_expired ? "active_time_limit" : diagnostic,
        worker_stop_requested && !ok ? "cancelled"
        : budget_expired ? "limit_reached" : ok ? "published" : "failed",
        now, elapsed_ms, &finish_error);
    g_free(now);
    local_job_record_free(job);
    g_free(owner);
    if (!recorded && error != NULL && *error == NULL)
      g_propagate_error(error, finish_error);
    else
      g_clear_error(&finish_error);
    g_clear_error(&execution_error);
    g_object_unref(cancellable);
    if (!recorded)
      break;
    char *jobs_export = workspace_path(root, "jobs-snapshot.json");
    local_job_store_export_atomic(store, jobs_export, NULL);
    g_free(jobs_export);
    if (ok) {
      CoreGraphLimits limits = {500U, 1000U, 1024U * 1024U};
      CoreGraphSnapshot *current =
          core_graph_projection_service_collect_operational(database, limits,
                                                            registry, NULL);
      char *graph_export = workspace_path(root, "core-snapshot.json");
      if (current != NULL)
        (void)core_graph_snapshot_write_atomic(current, manifest.synthetic, graph_export,
                                               NULL);
      g_free(graph_export);
      core_graph_snapshot_free(current);
      (void)export_correlations(root, database, manifest.investigation_id, NULL);
      (void)export_planner(root, database, store, registry,
                           manifest.investigation_id, NULL);
    }
  }
  jobs = local_job_store_list(store, NULL);
  gboolean complete = TRUE;
  for (guint i = 0; jobs != NULL && i < jobs->len; i++)
    complete =
        complete && ((LocalJobRecord *)g_ptr_array_index(jobs, i))->state ==
                        LOCAL_JOB_COMPLETED;
  g_clear_pointer(&jobs, g_ptr_array_unref);
  char *export_path = workspace_path(root, "jobs-snapshot.json");
  local_job_store_export_atomic(store, export_path, NULL);
  g_free(export_path);
  if (complete) {
    CoreGraphLimits limits = {500U, 1000U, 1024U * 1024U};
    CoreGraphSnapshot *snapshot =
        core_graph_projection_service_collect_operational(database, limits,
                                                          registry, error);
    char *snapshot_path = workspace_path(root, "core-snapshot.json");
    if (snapshot == NULL ||
        !core_graph_snapshot_write_atomic(snapshot, manifest.synthetic, snapshot_path, error))
      complete = FALSE;
    g_free(snapshot_path);
    core_graph_snapshot_free(snapshot);
  }
  local_capability_registry_free(registry);
  database_close(database);
  g_free(database_path);
  flock(lock_fd, LOCK_UN);
  close(lock_fd);
  g_free(lock_path);
  local_job_store_close(store);
  manifest_clear(&manifest);
  (void)sigaction(SIGTERM, &previous_term, NULL);
  (void)sigaction(SIGINT, &previous_int, NULL);
  return complete && (error == NULL || *error == NULL);
}

static gboolean status_or_export(const char *root, gboolean print,
                                 GError **error) {
  Manifest manifest = {0};
  LocalJobStore *store = open_store(root, &manifest, error);
  GPtrArray *jobs = store != NULL ? local_job_store_list(store, error) : NULL;
  for (guint i = 0; print && jobs != NULL && i < jobs->len; i++) {
    LocalJobRecord *job = g_ptr_array_index(jobs, i);
    printf("%s %s tentatives=%u %s\n", job->job_id,
           local_job_state_code(job->state), job->attempt_count,
           job->diagnostic != NULL ? job->diagnostic : "");
  }
  char *path = workspace_path(root, "jobs-snapshot.json");
  gboolean ok =
      jobs != NULL && local_job_store_export_atomic(store, path, error);
  char *database_path = workspace_path(root, "Enquete.sqlite");
  Database *database = ok ? database_open(database_path) : NULL;
  LocalCapabilityRegistry *registry = ok ? local_capability_registry_new(error) : NULL;
  if (ok)
    ok = database != NULL && registry != NULL && export_correlations(
        root, database, manifest.investigation_id, error) &&
        export_planner(root, database, store, registry,
                       manifest.investigation_id, error);
  local_capability_registry_free(registry);
  database_close(database);
  g_free(database_path);
  g_free(path);
  g_clear_pointer(&jobs, g_ptr_array_unref);
  local_job_store_close(store);
  manifest_clear(&manifest);
  return ok;
}

static Database *open_workspace_database(const char *root, Manifest *manifest,
                                         GError **error) {
  if (!manifest_read(root, manifest, error)) return NULL;
  char *path = workspace_path(root, "Enquete.sqlite");
  Database *database = database_open(path);
  g_free(path);
  if (database == NULL) {
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                        "Impossible d'ouvrir la base de l'espace.");
    manifest_clear(manifest);
  }
  return database;
}

static gboolean print_evidence_preview(const char *root,
                                       const char *evidence_id,
                                       GError **error) {
  Manifest manifest = {0};
  Database *database = NULL;
  EvidenceDao *dao = NULL;
  EvidenceRecord *evidence = NULL;
  EvidencePreviewRequest *request = NULL;
  EvidencePreviewResult *preview = NULL;
  gboolean ok = FALSE;
  if (!g_uuid_string_is_valid(evidence_id)) {
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                        "UUID de preuve invalide.");
    return FALSE;
  }
  database = open_workspace_database(root, &manifest, error);
  dao = database != NULL ? evidence_dao_new(database, error) : NULL;
  evidence = dao != NULL
      ? evidence_dao_find_by_identifier(dao, evidence_id, error) : NULL;
  if (evidence == NULL && error != NULL && *error == NULL)
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                        "Preuve inconnue dans cet espace.");
  if (evidence != NULL) request = evidence_preview_request_new(root, evidence_id,
      evidence_record_get_relative_path(evidence),
      evidence_record_get_sha256(evidence), NULL, 1U);
  preview = request != NULL ? evidence_preview_load(request, NULL, error) : NULL;
  if (preview != NULL) {
    const char *kind = preview->kind == EVIDENCE_PREVIEW_KIND_EMAIL ? "email" :
        preview->kind == EVIDENCE_PREVIEW_KIND_IMAGE ? "image" : "unsupported";
    char *cache_input = g_strdup_printf("%s:%s:%s:web-v1:preview-v1",
        manifest.investigation_id, evidence_id,
        evidence_record_get_sha256(evidence));
    char *cache_id = g_compute_checksum_for_string(G_CHECKSUM_SHA256,
                                                    cache_input, -1);
    char *image = preview->png_bytes != NULL
        ? g_base64_encode(g_bytes_get_data(preview->png_bytes, NULL),
                          g_bytes_get_size(preview->png_bytes)) : NULL;
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    json_add_nullable(builder, "contract", "labfy.evidence_preview.v1");
    json_add_nullable(builder, "evidence_id", evidence_id);
    json_add_nullable(builder, "display_name",
                      evidence_record_get_original_name(evidence));
    json_add_nullable(builder, "type", evidence_record_get_type_identifier(evidence));
    json_add_nullable(builder, "sha256", evidence_record_get_sha256(evidence));
    json_builder_set_member_name(builder, "size_bytes");
    json_builder_add_int_value(builder, (gint64) evidence_record_get_size_bytes(evidence));
    json_add_nullable(builder, "kind", kind);
    json_add_nullable(builder, "cache_id", cache_id);
    json_add_nullable(builder, "renderer_version", "preview-v1");
    json_add_nullable(builder, "text", preview->text);
    json_add_nullable(builder, "image_png_base64", image);
    json_builder_set_member_name(builder, "width");
    json_builder_add_int_value(builder, preview->width);
    json_builder_set_member_name(builder, "height");
    json_builder_add_int_value(builder, preview->height);
    json_builder_set_member_name(builder, "truncated");
    json_builder_add_boolean_value(builder, preview->truncated);
    json_builder_set_member_name(builder, "integrity_valid");
    json_builder_add_boolean_value(builder, preview->integrity_valid);
    json_builder_end_object(builder);
    ok = print_json_builder(builder, error);
    g_object_unref(builder); g_free(image); g_free(cache_id); g_free(cache_input);
  }
  evidence_preview_result_free(preview);
  evidence_preview_request_free(request);
  evidence_record_free(evidence); evidence_dao_free(dao);
  database_close(database); manifest_clear(&manifest);
  return ok;
}

static guint64 observation_revision(Database *database, const char *id) {
  DatabaseStatement *statement = database_statement_prepare(database,
      "SELECT COUNT(*) FROM journal WHERE action='observation.review.v1' "
      "AND objet_type='evidence_observation' AND objet_id=?;");
  int64_t count = 0;
  gboolean ok = statement != NULL &&
      database_statement_bind_text(statement, 1, id) &&
      database_statement_step(statement) == DATABASE_STATEMENT_STEP_ROW &&
      database_statement_column_int64(statement, 0, &count) && count >= 0;
  database_statement_finalize(statement);
  return ok ? (guint64) count : 0U;
}

static gboolean print_observations(const char *root, const char *evidence_id,
                                   const char *extraction_id, GError **error) {
  Manifest manifest = {0};
  Database *database = NULL; EvidenceDao *evidence_dao = NULL;
  ExtractionDao *extraction_dao = NULL; ExtractionRecord *extraction = NULL;
  EvidenceEntityDao *dao = NULL; EvidenceRecord *evidence = NULL;
  GPtrArray *items = NULL; gboolean ok = FALSE;
  if (!g_uuid_string_is_valid(evidence_id) ||
      (extraction_id != NULL && !g_uuid_string_is_valid(extraction_id))) {
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                        "UUID de preuve ou d'extraction invalide.");
    return FALSE;
  }
  database = open_workspace_database(root, &manifest, error);
  evidence_dao = database != NULL ? evidence_dao_new(database, error) : NULL;
  evidence = evidence_dao != NULL
      ? evidence_dao_find_by_identifier(evidence_dao, evidence_id, error) : NULL;
  if (evidence == NULL && error != NULL && *error == NULL)
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                        "Preuve inconnue dans cet espace.");
  extraction_dao = evidence != NULL && extraction_id != NULL
      ? extraction_dao_new(database, error) : NULL;
  extraction = extraction_dao != NULL
      ? extraction_dao_find_by_identifier(extraction_dao, extraction_id, error)
      : NULL;
  if (extraction_id != NULL && extraction == NULL &&
      error != NULL && *error == NULL)
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                        "Extraction inconnue dans cet espace.");
  if (extraction != NULL &&
      g_strcmp0(extraction->source_identifier, evidence_id) != 0 &&
      g_strcmp0(extraction->evidence_identifier, evidence_id) != 0)
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
                        "L'extraction n'appartient pas à la preuve demandée.");
  if (error != NULL && *error != NULL) {
    evidence_record_free(evidence);
    evidence = NULL;
  }
  dao = evidence != NULL ? evidence_entity_dao_new(database, error) : NULL;
  items = dao != NULL
      ? evidence_entity_dao_list_observations(dao, evidence_id, error) : NULL;
  if (items != NULL) {
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    json_add_nullable(builder, "contract", "labfy.evidence_observations.v1");
    json_add_nullable(builder, "evidence_id", evidence_id);
    json_builder_set_member_name(builder, "observations");
    json_builder_begin_array(builder);
    for (guint i = 0; i < items->len; i++) {
      EvidenceObservation *item = g_ptr_array_index(items, i);
      if (extraction_id != NULL &&
          g_strcmp0(item->extraction_identifier, extraction_id) != 0) continue;
      json_builder_begin_object(builder);
      json_add_nullable(builder, "id", item->identifier);
      json_add_nullable(builder, "extraction_id", item->extraction_identifier);
      json_add_nullable(builder, "entity_type", item->type_identifier);
      json_add_nullable(builder, "value_raw", item->value_raw);
      json_add_nullable(builder, "value_normalized", item->value_normalized);
      json_add_nullable(builder, "value_corrected", item->value_corrected);
      json_add_nullable(builder, "role", item->role);
      json_add_nullable(builder, "source_header", item->source_header);
      json_add_nullable(builder, "provenance_kind", item->provenance_kind);
      json_add_nullable(builder, "verification_status", item->verification_status);
      json_add_nullable(builder, "entity_id", item->entity_identifier);
      json_builder_set_member_name(builder, "revision");
      json_builder_add_int_value(builder,
                                 (gint64) observation_revision(database, item->identifier));
      json_builder_end_object(builder);
    }
    json_builder_end_array(builder); json_builder_end_object(builder);
    ok = print_json_builder(builder, error); g_object_unref(builder);
  }
  g_clear_pointer(&items, g_ptr_array_unref); evidence_entity_dao_free(dao);
  extraction_record_free(extraction); extraction_dao_free(extraction_dao);
  evidence_record_free(evidence); evidence_dao_free(evidence_dao);
  database_close(database); manifest_clear(&manifest); return ok;
}

static gboolean republish_after_review(const char *root, Database *database,
                                       const Manifest *manifest,
                                       GError **error) {
  char *jobs_path = workspace_path(root, ".labfy/runtime/jobs.sqlite");
  LocalJobStore *store = local_job_store_open(
      jobs_path, manifest->investigation_id, FALSE, error);
  g_free(jobs_path);
  LocalCapabilityRegistry *registry = store != NULL
      ? local_capability_registry_new(error) : NULL;
  CoreGraphLimits limits = {500U, 1000U, 1024U * 1024U};
  CoreGraphSnapshot *snapshot = registry != NULL
      ? core_graph_projection_service_collect_operational(database, limits,
                                                          registry, error) : NULL;
  char *path = workspace_path(root, "core-snapshot.json");
  gboolean ok = snapshot != NULL && core_graph_snapshot_write_atomic(
      snapshot, manifest->synthetic, path, error) && export_correlations(
      root, database, manifest->investigation_id, error) && export_planner(
      root, database, store, registry, manifest->investigation_id, error);
  g_free(path); core_graph_snapshot_free(snapshot);
  local_capability_registry_free(registry); local_job_store_close(store);
  return ok;
}

static gboolean review_observation(const char *root, const char *evidence_id,
    const char *observation_id, const char *operation_id, const char *revision,
    const char *action, const char *status, const char *corrected,
    const char *entity_id, const char *author, const char *reason,
    GError **error) {
  char *end = NULL; guint64 expected = 0U;
  ObservationReviewAction mapped;
  if (revision == NULL) {
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                        "Révision invalide."); return FALSE;
  }
  expected = g_ascii_strtoull(revision, &end, 10);
  if (end == revision || *end != '\0') {
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                        "Révision invalide."); return FALSE;
  }
  if (g_strcmp0(action, "decide") == 0) mapped = OBSERVATION_REVIEW_ACTION_DECIDE;
  else if (g_strcmp0(action, "correct") == 0) mapped = OBSERVATION_REVIEW_ACTION_CORRECT;
  else if (g_strcmp0(action, "promote_create") == 0) mapped = OBSERVATION_REVIEW_ACTION_PROMOTE_CREATE;
  else if (g_strcmp0(action, "promote_attach") == 0) mapped = OBSERVATION_REVIEW_ACTION_PROMOTE_ATTACH;
  else if (g_strcmp0(action, "withdraw") == 0) mapped = OBSERVATION_REVIEW_ACTION_WITHDRAW_PROMOTION;
  else { g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                             "Action de revue invalide."); return FALSE; }
  Manifest manifest = {0}; Database *database = open_workspace_database(
      root, &manifest, error); ObservationReviewService *service = database != NULL
      ? observation_review_service_new(database, error) : NULL;
  char *occurred_at = now_iso(); ObservationReviewResult result = {0};
  ObservationReviewRequest request = {operation_id, evidence_id, observation_id,
      expected, mapped, status, corrected, entity_id, author, reason, occurred_at};
  gboolean ok = service != NULL && observation_review_service_apply(
      service, &request, &result, error);
  /* CONTRACT: publication refreshes local projections only; no analysis job,
   * network access, budget admission or EML parsing is triggered here. */
  if (ok) ok = republish_after_review(root, database, &manifest, error);
  if (ok) {
    JsonBuilder *builder = json_builder_new(); json_builder_begin_object(builder);
    json_add_nullable(builder, "contract", "labfy.observation_review.command.v1");
    json_builder_set_member_name(builder, "replayed");
    json_builder_add_boolean_value(builder, result.replayed);
    json_builder_set_member_name(builder, "revision");
    json_builder_add_int_value(builder, (gint64) result.revision);
    json_add_nullable(builder, "entity_id", result.entity_identifier);
    json_builder_set_member_name(builder, "entity_created");
    json_builder_add_boolean_value(builder, result.entity_created);
    json_builder_set_member_name(builder, "projections_refreshed");
    json_builder_add_boolean_value(builder, TRUE); json_builder_end_object(builder);
    ok = print_json_builder(builder, error); g_object_unref(builder);
  }
  observation_review_result_clear(&result); g_free(occurred_at);
  observation_review_service_free(service); database_close(database);
  manifest_clear(&manifest); return ok;
}

static char *research_hash(const char *value) {
  return g_compute_checksum_for_string(G_CHECKSUM_SHA256, value, -1);
}

static gboolean research_open_db(const char *root, sqlite3 **out,
                                 GError **error) {
  char *path = workspace_path(root, ".labfy/runtime/jobs.sqlite");
  int result = sqlite3_open_v2(path, out, SQLITE_OPEN_READONLY, NULL);
  g_free(path);
  if (result == SQLITE_OK) return TRUE;
  g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                      "Snapshot de recherche indisponible.");
  if (*out != NULL) sqlite3_close(*out);
  *out = NULL; return FALSE;
}

static gboolean research_provider(char **out, GError **error) {
  const char *authority = g_getenv("LABFY_RESEARCH_FIXTURE_AUTHORITY");
  if (authority == NULL || !g_regex_match_simple(
      "^127\\.0\\.0\\.1:([1-9][0-9]{0,4})$", authority, 0, 0)) {
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
        "Fournisseur de laboratoire local non configuré."); return FALSE;
  }
  const char *separator = strrchr(authority, ':');
  guint64 port = g_ascii_strtoull(separator + 1, NULL, 10);
  if (port > 65535U || port == 8080U || port == 8081U) {
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
                        "Port fournisseur de laboratoire interdit."); return FALSE;
  }
  *out = g_strdup(authority); return TRUE;
}

static gboolean research_load_selection(const char *root, gchar **ids,
    GPtrArray *subjects, guint64 *snapshot_revision, GError **error) {
  char *path = workspace_path(root, "core-snapshot.json");
  JsonParser *parser = json_parser_new();
  gboolean ok = json_parser_load_from_file(parser, path, error); g_free(path);
  JsonObject *root_object = ok ? json_node_get_object(
      json_parser_get_root(parser)) : NULL;
  JsonArray *nodes = root_object != NULL ? json_object_get_array_member(
      root_object, "nodes") : NULL;
  if (nodes == NULL) ok = FALSE;
  if (ok) *snapshot_revision = (guint64)json_object_get_int_member(
      root_object, "revision");
  for (gsize i = 0; ok && ids[i] != NULL; i++) {
    const char *label = NULL;
    for (guint j = 0; j < json_array_get_length(nodes); j++) {
      JsonObject *node = json_array_get_object_element(nodes, j);
      if (g_strcmp0(json_object_get_string_member(node, "id"), ids[i]) == 0)
        label = json_object_get_string_member(node, "label");
    }
    if (label == NULL || !g_utf8_validate(label, -1, NULL)) ok = FALSE;
    else g_ptr_array_add(subjects, g_strdup(label));
  }
  g_object_unref(parser);
  if (!ok && error != NULL && *error == NULL)
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                        "Sélection absente du snapshot cœur.");
  return ok;
}

static gboolean research_prepare_json(const char *root, const char *selection,
    const char *question, const char *exclusions, const char *revision,
    const char *key, GError **error) {
  if (selection == NULL || question == NULL || revision == NULL || key == NULL ||
      !g_uuid_string_is_valid(key) || strlen(question) > 512U) {
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                        "Intention de recherche invalide."); return FALSE;
  }
  gchar **ids = g_strsplit(selection, ",", 9);
  gsize count = g_strv_length(ids); char *end = NULL;
  guint64 requested_revision = g_ascii_strtoull(revision, &end, 10);
  GPtrArray *subjects = g_ptr_array_new_with_free_func(g_free);
  guint64 current_revision = 0U; char *authority = NULL;
  gboolean ok = count > 0U && count <= 8U && end != revision && *end == '\0' &&
      research_load_selection(root, ids, subjects, &current_revision, error) &&
      current_revision == requested_revision && research_provider(&authority, error);
  if (!ok && error != NULL && *error == NULL)
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                        "Révision de sélection périmée.");
  /* CONTRACT: every selected identifier must resolve exactly against the core
   * snapshot before a plan or seed can be constructed. WHY: continuing after
   * rejection indexes an empty subject array and turns invalid input into a
   * process crash instead of the explicit bridge error. */
  if (!ok) {
    g_free(authority);
    g_ptr_array_unref(subjects);
    g_strfreev(ids);
    return FALSE;
  }
  Manifest manifest = {0}; LocalJobStore *store = ok ? open_store(
      root, &manifest, error) : NULL;
  char *plan_id = derived_uuid(key, "research-plan");
  char *action_ids[3] = {derived_uuid(key, "wave-1"),
      derived_uuid(key, "wave-2"), derived_uuid(key, "refused")};
  char *endpoints[3] = {NULL, NULL, NULL};
  for (guint i = 0; authority != NULL && i < 3U; i++)
    endpoints[i] = g_strdup_printf("http://%s/specimen/%s", authority,
                                    i == 0 ? "wave-1" : i == 1 ? "wave-2" : "refused");
  const char *subject = subjects->len > 0 ? g_ptr_array_index(subjects, 0) : NULL;
  ResearchSeed *seeds = g_new0(ResearchSeed, count + 1U);
  for (gsize i = 0; i < count; i++) seeds[i] = (ResearchSeed){
      RESEARCH_SEED_SEARCH_TERM, g_ptr_array_index(subjects, i)};
  seeds[count] = (ResearchSeed){RESEARCH_SEED_SEARCH_TERM, question};
  ResearchAction actions[3] = {
    {action_ids[0],"labfy.research.fixture.lookup.v1","fixture-provider",
     endpoints[0],subject,RESEARCH_CONTACT_THIRD_PARTY,
     "Le sujet exact est communiqué au fournisseur local SPECIMEN.",1U,4096U,2000U},
    {action_ids[1],"labfy.research.fixture.followup.v1","fixture-provider",
     endpoints[1],subject,RESEARCH_CONTACT_THIRD_PARTY,
     "Le sujet exact est communiqué au second contact local SPECIMEN.",1U,4096U,2000U},
    {action_ids[2],"labfy.research.fixture.optional.v1","fixture-provider",
     endpoints[2],subject,RESEARCH_CONTACT_THIRD_PARTY,
     "Action optionnelle : le sujet exact serait communiqué.",1U,4096U,2000U}};
  char *material = g_strdup_printf("%s|%s|%s|%s|%s", manifest.investigation_id,
      selection, question, exclusions != NULL ? exclusions : "",
      authority != NULL ? authority : "");
  char *fingerprint = research_hash(material); char *created = now_iso();
  ResearchPlan plan = {RESEARCH_PLAN_CONTRACT,plan_id,key,fingerprint,fingerprint,
      requested_revision,created,seeds,count+1U,actions,3U}; gboolean reused=FALSE;
  ok = store != NULL && research_store_admit_plan(store,&plan,&reused,error);
  if (ok) ok = research_snapshot_json(root,error);
  g_free(created);g_free(fingerprint);g_free(material);g_free(seeds);
  for(guint i=0;i<3U;i++){g_free(action_ids[i]);g_free(endpoints[i]);}
  g_free(plan_id);g_free(authority);local_job_store_close(store);
  manifest_clear(&manifest);g_ptr_array_unref(subjects);g_strfreev(ids);return ok;
}

static gboolean research_query_plan(sqlite3 *db, const char *plan_id,
    char **fingerprint, guint64 *revision, GError **error) {
  sqlite3_stmt *s=NULL; gboolean ok=sqlite3_prepare_v2(db,
      "SELECT content_fingerprint,input_revision FROM research_plans WHERE plan_id=?1;",
      -1,&s,NULL)==SQLITE_OK&&sqlite3_bind_text(s,1,plan_id,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
      sqlite3_step(s)==SQLITE_ROW;
  if(ok){*fingerprint=g_strdup((const char*)sqlite3_column_text(s,0));
    *revision=(guint64)sqlite3_column_int64(s,1);} sqlite3_finalize(s);
  if(!ok)g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_NOT_FOUND,"Plan de recherche introuvable.");
  return ok;
}

static gboolean research_grant_json(const char *root, const char *plan_id,
    const char *revision, const char *actions_csv, const char *decisions_csv,
    const char *exclusions_csv, const char *key, GError **error) {
  if(plan_id==NULL||revision==NULL||actions_csv==NULL||decisions_csv==NULL||key==NULL||
      !g_uuid_string_is_valid(plan_id)||!g_uuid_string_is_valid(key))return FALSE;
  sqlite3 *db=NULL;char *plan_fp=NULL;guint64 stored_revision=0U;char *end=NULL;
  guint64 expected=g_ascii_strtoull(revision,&end,10);
  gboolean ok=end!=revision&&*end=='\0'&&research_open_db(root,&db,error)&&
      research_query_plan(db,plan_id,&plan_fp,&stored_revision,error)&&expected==stored_revision;
  sqlite3_close(db);gchar **actions=g_strsplit(actions_csv,",",9);
  gchar **decision_items=g_strsplit(decisions_csv,",",9);
  gchar **excluded=g_strsplit(exclusions_csv!=NULL?exclusions_csv:"",",",9);
  gsize action_count=g_strv_length(actions),excluded_count=
      exclusions_csv!=NULL&&*exclusions_csv!='\0'?g_strv_length(excluded):0U;
  gsize decision_count=g_strv_length(decision_items);
  ResearchActionDecision *decisions=g_new0(ResearchActionDecision,decision_count);
  for(gsize i=0;ok&&i<decision_count;i++){
    char *separator=strchr(decision_items[i],'=');
    ok=separator!=NULL&&separator!=decision_items[i]&&separator[1]!='\0';
    if(ok){*separator='\0';decisions[i].action_id=decision_items[i];
      if(g_strcmp0(separator+1,"AUTHORIZE")==0)decisions[i].code=RESEARCH_ACTION_AUTHORIZE;
      else if(g_strcmp0(separator+1,"DEFER")==0)decisions[i].code=RESEARCH_ACTION_DEFER;
      else if(g_strcmp0(separator+1,"REFUSE")==0)decisions[i].code=RESEARCH_ACTION_REFUSE;
      else ok=FALSE;}
  }
  Manifest manifest={0};LocalJobStore *store=ok?open_store(root,&manifest,error):NULL;
  char *grant_id=derived_uuid(key,"research-grant");char *created=now_iso();
  GDateTime *date=g_date_time_new_now_utc();GDateTime *future=g_date_time_add_hours(date,1);
  char *expires=g_date_time_format(future,"%Y-%m-%dT%H:%M:%SZ");
  char *material=g_strdup_printf("%s|%s|%s|%s",plan_fp!=NULL?plan_fp:"",actions_csv,
      decisions_csv,exclusions_csv!=NULL?exclusions_csv:"");
  char *fingerprint=research_hash(material);ScopeGrant grant={RESEARCH_GRANT_CONTRACT,
      grant_id,manifest.investigation_id,key,fingerprint,plan_fp,created,expires,
      (const char*const*)actions,action_count,(const char*const*)excluded,excluded_count,
      (guint)action_count,4096U*action_count,2000U*action_count};gboolean reused=FALSE;
  ok=ok&&store!=NULL&&action_count>0U&&action_count<=8U&&decision_count>0U&&
      decision_count<=8U&&
      research_store_admit_grant(store,&grant,decisions,decision_count,created,
                                 &reused,error);
  if(ok)ok=research_snapshot_json(root,error);
  g_date_time_unref(future);g_date_time_unref(date);g_free(fingerprint);g_free(material);
  g_free(expires);g_free(created);g_free(grant_id);g_free(plan_fp);g_strfreev(actions);
  g_free(decisions);g_strfreev(decision_items);
  g_strfreev(excluded);local_job_store_close(store);manifest_clear(&manifest);return ok;
}

static gboolean research_http_fixture(const char *endpoint, const char *action_id,
    guint64 max_response_bytes, char **body, GError **error) {
  GUri *uri=g_uri_parse(endpoint,G_URI_FLAGS_NONE,error);if(uri==NULL)return FALSE;
  const char *host=g_uri_get_host(uri);gint port=g_uri_get_port(uri);
  if(g_strcmp0(host,"127.0.0.1")!=0||port<=0||port==8080||port==8081){
    g_uri_unref(uri);g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_PERMISSION_DENIED,
      "Seul le fournisseur loopback SPECIMEN est autorisé.");return FALSE;}
  GSocketClient *client=g_socket_client_new();g_socket_client_set_timeout(client,2U);
  /* INVARIANT fixture: le proxy de l'hôte ne peut jamais détourner ce
   * transport synthétique hors de l'autorité 127.0.0.1 validée ci-dessus. */
  g_socket_client_set_enable_proxy(client,FALSE);
  GSocketConnection *connection=g_socket_client_connect_to_host(client,host,(guint16)port,NULL,error);
  gboolean ok=connection!=NULL;GString *request=g_string_new(NULL);
  g_string_printf(request,"GET %s HTTP/1.1\r\nHost: %s:%d\r\nX-Labfy-Action: %s\r\nConnection: close\r\n\r\n",
      g_uri_get_path(uri),host,port,action_id);
  GOutputStream *out=ok?g_io_stream_get_output_stream(G_IO_STREAM(connection)):NULL;
  ok=ok&&g_output_stream_write_all(out,request->str,request->len,NULL,NULL,error);
  GInputStream *in=ok?g_io_stream_get_input_stream(G_IO_STREAM(connection)):NULL;
  const guint64 max_header_bytes=4096U;
  GByteArray *bytes=g_byte_array_new();guint8 buffer[1024];gssize got=0;
  while(ok&&(got=g_input_stream_read(in,buffer,sizeof(buffer),NULL,error))>0){
    g_byte_array_append(bytes,buffer,(guint)got);
    g_byte_array_append(bytes,(const guint8 *)"",1U);
    char *separator=strstr((char*)bytes->data,"\r\n\r\n");
    g_byte_array_set_size(bytes,bytes->len-1U);
    guint64 header_size=separator!=NULL?(guint64)(separator-(char*)bytes->data)+4U:
      (guint64)bytes->len;
    guint64 body_size=separator!=NULL?(guint64)bytes->len-header_size:0U;
    if(header_size>max_header_bytes||body_size>max_response_bytes){ok=FALSE;
      g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_NO_SPACE,
        "Réponse SPECIMEN trop volumineuse.");break;}}
  if (got < 0)
    ok = FALSE;
  g_byte_array_append(bytes, (const guint8 *)"", 1U);
  char *separator=ok?strstr((char*)bytes->data,"\r\n\r\n"):NULL;
  ok=ok&&g_str_has_prefix((char*)bytes->data,"HTTP/1.1 200")&&separator!=NULL;
  if(ok)*body=g_strdup(separator+4);else if(error!=NULL&&*error==NULL)
    g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_FAILED,"Réponse fournisseur SPECIMEN invalide.");
  g_byte_array_unref(bytes);g_string_free(request,TRUE);g_clear_object(&connection);
  g_object_unref(client);g_uri_unref(uri);return ok;
}

static gboolean research_result_exists(sqlite3 *db, const char *result_id,
    gboolean *out_exists, GError **error) {
  sqlite3_stmt *query=NULL;gboolean ok=sqlite3_prepare_v2(db,
      "SELECT 1 FROM research_results WHERE result_id=?1;",-1,&query,NULL)==
      SQLITE_OK&&sqlite3_bind_text(query,1,result_id,-1,SQLITE_TRANSIENT)==
      SQLITE_OK;int step=ok?sqlite3_step(query):SQLITE_ERROR;
  if(ok&&step!=SQLITE_ROW&&step!=SQLITE_DONE)ok=FALSE;
  if(ok)
    *out_exists=step==SQLITE_ROW;
  sqlite3_finalize(query);
  if(!ok)g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_FAILED,
      "Lecture de l'état du résultat de recherche impossible.");
  return ok;
}

static gboolean research_private_regular(const char *path, guint64 max_size,
    guint64 *out_size) {
  GFile *file=g_file_new_for_path(path);
  GFileInfo *info=g_file_query_info(file,
      G_FILE_ATTRIBUTE_STANDARD_TYPE "," G_FILE_ATTRIBUTE_STANDARD_SIZE,
      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,NULL,NULL);
  gboolean valid=info!=NULL&&g_file_info_get_file_type(info)==
      G_FILE_TYPE_REGULAR&&g_file_info_get_size(info)>=0&&
      (guint64)g_file_info_get_size(info)<=max_size;
  if(valid&&out_size!=NULL)
    *out_size=(guint64)g_file_info_get_size(info);
  g_clear_object(&info);g_object_unref(file);return valid;
}

static gboolean research_marker_write(const char *path, const char *result_id,
    const char *sha, GError **error) {
  char *data=g_strdup_printf("labfy.research_publish.v1\n%s\n%s\n",result_id,sha);
  gboolean ok=write_private_atomic(path,data,error);g_free(data);return ok;
}

static gboolean research_marker_matches(const char *path,
    const char *result_id, const char *sha) {
  if(!research_private_regular(path,256U,NULL))return FALSE;
  char *data=NULL;gsize length=0U;
  if(!g_file_get_contents(path,&data,&length,NULL))return FALSE;
  char *expected=g_strdup_printf("labfy.research_publish.v1\n%s\n%s\n",
      result_id,sha);gboolean matches=length==strlen(expected)&&
      memcmp(data,expected,length)==0;g_free(expected);g_free(data);
  return matches;
}

static gboolean research_reconcile_publication(sqlite3 *db,
    LocalJobStore *store, gboolean reused, const ResearchAction *action,
    const char *campaign_id, const char *grant_id, const char *created,
    const char *result_id, const char *receipt_id, const char *relative,
    const char *absolute, const char *staging, const char *marker,
    const char *status, gboolean *out_handled, GError **error) {
  gboolean exists=FALSE;*out_handled=FALSE;
  if(!research_result_exists(db,result_id,&exists,error))return FALSE;
  if(exists){*out_handled=TRUE;(void)g_remove(marker);(void)g_remove(staging);
    return TRUE;}
  if(!reused)return TRUE;
  *out_handled=TRUE;
  if(!g_file_test(marker,G_FILE_TEST_EXISTS)){
    if(g_file_test(absolute,G_FILE_TEST_EXISTS)){
      g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_INVALID_DATA,
          "Artefact final sans journal de publication possédé.");return FALSE;}
    return TRUE;
  }
  guint64 body_size=0U;char *body=NULL;gsize loaded=0U;
  gboolean valid=research_private_regular(absolute,action->max_response_bytes,
      &body_size)&&g_file_get_contents(absolute,&body,&loaded,NULL)&&
      loaded==body_size;char *sha=valid?g_compute_checksum_for_data(
      G_CHECKSUM_SHA256,(const guchar*)body,loaded):NULL;
  valid=valid&&research_marker_matches(marker,result_id,sha);
  if(valid){ResearchResult result={RESEARCH_RESULT_CONTRACT,result_id,
      campaign_id,action->action_id,relative,sha,status};ResearchReceipt receipt={
      RESEARCH_RECEIPT_CONTRACT,receipt_id,result_id,campaign_id,grant_id,
      action->action_id,created,"ALLOW"};
    valid=research_store_record_result(store,&result,&receipt,error);}
  if(!valid){(void)g_remove(absolute);(void)g_remove(staging);(void)g_remove(marker);
    if(error!=NULL&&*error==NULL)g_set_error_literal(error,G_IO_ERROR,
      G_IO_ERROR_INVALID_DATA,"Publication de recherche interrompue incohérente.");}
  else (void)g_remove(marker);
  g_free(sha);g_free(body);return valid;
}

static gboolean research_campaign_json(const char *root, const char *grant_id,
    const char *revision, const char *actions_csv, const char *key,
    GError **error) {
  if(grant_id==NULL||revision==NULL||actions_csv==NULL||key==NULL||
      !g_uuid_string_is_valid(grant_id)||!g_uuid_string_is_valid(key))return FALSE;
  gchar **requested=g_strsplit(actions_csv,",",9);gsize count=g_strv_length(requested);
  char *end=NULL;guint64 expected=g_ascii_strtoull(revision,&end,10);
  sqlite3 *db=NULL;sqlite3_stmt *meta=NULL;char *plan_fp=NULL;guint64 stored=0U;
  gboolean ok=count>0U&&count<=8U&&end!=revision&&*end=='\0'&&
      research_open_db(root,&db,error)&&sqlite3_prepare_v2(db,
      "SELECT g.plan_fingerprint,p.input_revision FROM scope_grants g JOIN research_plans p ON p.content_fingerprint=g.plan_fingerprint WHERE g.grant_id=?1;",
      -1,&meta,NULL)==SQLITE_OK&&sqlite3_bind_text(meta,1,grant_id,-1,SQLITE_TRANSIENT)==SQLITE_OK&&sqlite3_step(meta)==SQLITE_ROW;
  if(ok){plan_fp=g_strdup((const char*)sqlite3_column_text(meta,0));stored=(guint64)sqlite3_column_int64(meta,1);ok=stored==expected;}
  sqlite3_finalize(meta);if(!ok&&error!=NULL&&*error==NULL)g_set_error_literal(error,
      G_IO_ERROR,G_IO_ERROR_FAILED,"Grant absent ou révision périmée.");
  Manifest manifest={0};LocalJobStore *store=ok?open_store(root,&manifest,error):NULL;
  char *campaign_id=derived_uuid(key,"research-campaign");char *created=now_iso();
  char *material=g_strdup_printf("%s|%s|%s",grant_id,revision,actions_csv);
  char *fingerprint=research_hash(material);ResearchCampaign campaign={
      RESEARCH_CAMPAIGN_CONTRACT,campaign_id,manifest.investigation_id,grant_id,key,
      fingerprint,created};gboolean reused=FALSE;
  ok=store!=NULL&&research_store_admit_campaign(store,&campaign,&reused,error);
  /* INVARIANT: un rejeu idempotent réconcilie une publication interrompue
   * avant toute possibilité de reproduire le contact fournisseur. */
  for(gsize i=0;ok&&i<count;i++){
    sqlite3_stmt *a=NULL;ResearchAction action={0};
    ok=sqlite3_prepare_v2(db,"SELECT a.capability_id,a.provider_id,a.endpoint,a.subject,a.contact_class,a.disclosure,a.max_requests,a.max_response_bytes,a.max_active_ms FROM research_actions a JOIN scope_grant_actions ga ON ga.action_id=a.action_id WHERE ga.grant_id=?1 AND a.action_id=?2;",-1,&a,NULL)==SQLITE_OK&&
       sqlite3_bind_text(a,1,grant_id,-1,SQLITE_TRANSIENT)==SQLITE_OK&&
       sqlite3_bind_text(a,2,requested[i],-1,SQLITE_TRANSIENT)==SQLITE_OK&&sqlite3_step(a)==SQLITE_ROW;
    if(!ok){sqlite3_finalize(a);g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_PERMISSION_DENIED,"Action non autorisée par le grant.");break;}
    action=(ResearchAction){requested[i],(const char*)sqlite3_column_text(a,0),
      (const char*)sqlite3_column_text(a,1),(const char*)sqlite3_column_text(a,2),
      (const char*)sqlite3_column_text(a,3),
      g_strcmp0((const char*)sqlite3_column_text(a,4),"THIRD_PARTY")==0?
        RESEARCH_CONTACT_THIRD_PARTY:RESEARCH_CONTACT_NONE,
      (const char*)sqlite3_column_text(a,5),(guint)sqlite3_column_int64(a,6),
      (guint64)sqlite3_column_int64(a,7),(guint)sqlite3_column_int64(a,8)};
    ResearchPolicyRequest policy={manifest.investigation_id,plan_fp,&action,created,
      action.max_requests,action.max_response_bytes,action.max_active_ms};
    ResearchPolicyDecision decision=RESEARCH_POLICY_DENY_ACTION;
    if(!reused)ok=research_store_policy_decide(store,grant_id,&policy,
      &decision,error)&&decision==RESEARCH_POLICY_ALLOW;
    char *endpoint=g_strdup(action.endpoint);sqlite3_finalize(a);
    char *result_id=derived_uuid(key,requested[i]);char *receipt_id=
      derived_uuid(result_id,"receipt");char *relative=g_strdup_printf(
      ".labfy/research/%s.txt",result_id);char *directory=workspace_path(root,
      ".labfy/research");char *absolute=workspace_path(root,relative);
    char *staging_name=g_strdup_printf(".staging-%s-%s",result_id,key);
    char *staging=g_build_filename(directory,staging_name,NULL);
    char *marker_name=g_strdup_printf(".publishing-%s",result_id);
    char *marker=g_build_filename(directory,marker_name,NULL);
    const char *status=strstr(endpoint,"wave-1")?"NEW":
      strstr(endpoint,"wave-2")?"CONTRADICTION":"MISSING";
    gboolean handled=FALSE;
    if(ok)ok=research_reconcile_publication(db,store,reused,&action,campaign_id,
      grant_id,created,result_id,receipt_id,relative,absolute,staging,marker,
      status,&handled,error);
    char *body=NULL;
    if(ok&&!handled)ok=research_http_fixture(endpoint,requested[i],
      action.max_response_bytes,&body,error);
    if(ok&&!handled){
      gboolean staging_owned=FALSE,final_owned=FALSE;
      if(g_mkdir_with_parents(directory,0700)!=0){ok=FALSE;g_set_error(error,
        G_IO_ERROR,g_io_error_from_errno(errno),
        "Création du staging de recherche impossible.");}
      if(ok&&g_file_test(absolute,G_FILE_TEST_EXISTS)){ok=FALSE;
        g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_EXISTS,
          "L'artefact final de recherche existe déjà.");}
      if(ok){ok=write_private_atomic(staging,body,error);
        staging_owned=g_file_test(staging,G_FILE_TEST_IS_REGULAR);}
      char *sha=research_hash(body);
      if(ok)ok=research_marker_write(marker,result_id,sha,error);
      /* CONTRACT: le journal privé précède le renommage. Après un crash, il
       * prouve la propriété et l'empreinte nécessaires à une réconciliation
       * sans nouveau contact; il est retiré après le résultat/reçu atomique. */
      if(ok&&g_rename(staging,absolute)!=0){ok=FALSE;g_set_error(error,G_IO_ERROR,
        g_io_error_from_errno(errno),"Publication de l'artefact impossible.");}
      else if(ok){staging_owned=FALSE;final_owned=TRUE;}
      if(ok&&g_strcmp0(g_getenv("LABFY_TEST_RESEARCH_CRASH_AFTER_RENAME"),"1")==0)
        _exit(86);
      ResearchResult result={RESEARCH_RESULT_CONTRACT,result_id,campaign_id,
        requested[i],relative,sha,status};ResearchReceipt receipt={
        RESEARCH_RECEIPT_CONTRACT,receipt_id,result_id,campaign_id,grant_id,
        requested[i],created,"ALLOW"};
      if(ok)ok=research_store_record_result(store,&result,&receipt,error);
      if(!ok&&final_owned)(void)g_remove(absolute);
      if(staging_owned)(void)g_remove(staging);
      (void)g_remove(marker);g_free(sha);}
    g_free(body);g_free(marker);g_free(marker_name);g_free(staging);
    g_free(staging_name);g_free(absolute);g_free(directory);g_free(relative);
    g_free(receipt_id);g_free(result_id);g_free(endpoint);
  }
  sqlite3_close(db);if(ok)ok=research_snapshot_json(root,error);
  g_free(fingerprint);g_free(material);g_free(created);g_free(campaign_id);
  g_free(plan_fp);g_strfreev(requested);local_job_store_close(store);
  manifest_clear(&manifest);return ok;
}

static gboolean research_snapshot_json(const char *root, GError **error) {
  sqlite3 *db=NULL;if(!research_open_db(root,&db,error))return FALSE;
  sqlite3_stmt *plan=NULL;gboolean has=sqlite3_prepare_v2(db,
      "SELECT plan_id,content_fingerprint,input_revision,created_at,(SELECT subject FROM research_seeds s WHERE s.plan_id=p.plan_id ORDER BY rank DESC LIMIT 1) FROM research_plans p ORDER BY rowid DESC LIMIT 1;",
      -1,&plan,NULL)==SQLITE_OK&&sqlite3_step(plan)==SQLITE_ROW;
  JsonBuilder *b=json_builder_new();json_builder_begin_object(b);
  json_builder_set_member_name(b,"contract");json_builder_add_string_value(b,"labfy.research.snapshot.v1");
  json_builder_set_member_name(b,"state");json_builder_add_string_value(b,has?"PREPARED":"EMPTY");
  if(has){const char *plan_id=(const char*)sqlite3_column_text(plan,0);
    json_add_nullable(b,"plan_id",plan_id);json_add_nullable(b,"plan_fingerprint",(const char*)sqlite3_column_text(plan,1));
    json_builder_set_member_name(b,"input_revision");json_builder_add_int_value(b,sqlite3_column_int64(plan,2));
    json_add_nullable(b,"created_at",(const char*)sqlite3_column_text(plan,3));
    json_add_nullable(b,"question",(const char*)sqlite3_column_text(plan,4));
    sqlite3_stmt *a=NULL;sqlite3_prepare_v2(db,
      "SELECT a.action_id,a.capability_id,a.provider_id,a.subject,a.contact_class,a.disclosure,a.max_requests,a.max_response_bytes,a.max_active_ms,EXISTS(SELECT 1 FROM research_receipts r WHERE r.action_id=a.action_id),COALESCE((SELECT rr.status FROM research_results rr WHERE rr.action_id=a.action_id ORDER BY rowid DESC LIMIT 1),''),COALESCE((SELECT d.decision_code FROM research_action_decisions d WHERE d.action_id=a.action_id),'DEFER'),a.rank FROM research_actions a WHERE a.plan_id=?1 ORDER BY a.rank;",-1,&a,NULL);
    sqlite3_bind_text(a,1,plan_id,-1,SQLITE_TRANSIENT);json_builder_set_member_name(b,"actions");json_builder_begin_array(b);
    while(sqlite3_step(a)==SQLITE_ROW){json_builder_begin_object(b);
      const char *names[]={"action_id","capability_id","provider_id","subject","contact","disclosure"};
      for(guint i=0;i<6U;i++)json_add_nullable(b,names[i],(const char*)sqlite3_column_text(a,(int)i));
      json_builder_set_member_name(b,"max_requests");json_builder_add_int_value(b,sqlite3_column_int64(a,6));
      json_builder_set_member_name(b,"max_response_bytes");json_builder_add_int_value(b,sqlite3_column_int64(a,7));
      json_builder_set_member_name(b,"max_active_ms");json_builder_add_int_value(b,sqlite3_column_int64(a,8));
      json_builder_set_member_name(b,"contacted");json_builder_add_boolean_value(b,sqlite3_column_int(a,9)!=0);
      json_add_nullable(b,"result_status",(const char*)sqlite3_column_text(a,10));
      json_add_nullable(b,"decision",(const char*)sqlite3_column_text(a,11));
      json_builder_set_member_name(b,"wave");json_builder_add_int_value(b,sqlite3_column_int64(a,12)+1);
      json_builder_end_object(b);}sqlite3_finalize(a);json_builder_end_array(b);
    sqlite3_stmt *g=NULL;sqlite3_prepare_v2(db,"SELECT grant_id,created_at,expires_at,revoked_at FROM scope_grants WHERE plan_fingerprint=?1 ORDER BY rowid DESC;",-1,&g,NULL);
    sqlite3_bind_text(g,1,(const char*)sqlite3_column_text(plan,1),-1,SQLITE_TRANSIENT);
    json_builder_set_member_name(b,"grants");json_builder_begin_array(b);while(sqlite3_step(g)==SQLITE_ROW){json_builder_begin_object(b);json_add_nullable(b,"grant_id",(const char*)sqlite3_column_text(g,0));json_add_nullable(b,"created_at",(const char*)sqlite3_column_text(g,1));json_add_nullable(b,"expires_at",(const char*)sqlite3_column_text(g,2));json_add_nullable(b,"revoked_at",(const char*)sqlite3_column_text(g,3));json_builder_end_object(b);}sqlite3_finalize(g);json_builder_end_array(b);
  } else {json_builder_set_member_name(b,"actions");json_builder_begin_array(b);json_builder_end_array(b);json_builder_set_member_name(b,"grants");json_builder_begin_array(b);json_builder_end_array(b);}
  sqlite3_finalize(plan);json_builder_end_object(b);gboolean ok=print_json_builder(b,error);
  g_object_unref(b);sqlite3_close(db);return ok;
}

static gboolean research_revoke_json(const char *root, const char *grant_id,
    GError **error){Manifest manifest={0};LocalJobStore *store=open_store(root,&manifest,error);
  char *now=now_iso();gboolean ok=store!=NULL&&research_store_revoke_grant(store,grant_id,now,error);
  if (ok)
    ok = research_snapshot_json(root, error);
  g_free(now);
  local_job_store_close(store);
  manifest_clear(&manifest);return ok;}

static gboolean control(const char *root, const char *command,
                        const char *job_id, GError **error) {
  Manifest manifest = {0};
  LocalJobStore *store = open_store(root, &manifest, error);
  char *now = now_iso();
  gboolean ok = FALSE;
  gboolean was_running = FALSE;
  if (store != NULL && strcmp(command, "cancel") == 0) {
    LocalJobRecord *target = local_job_store_find(store, job_id, error);
    was_running = target != NULL && target->state == LOCAL_JOB_RUNNING;
    local_job_record_free(target);
    ok = (error == NULL || *error == NULL) &&
         local_job_store_request_cancel(store, job_id, now, error);
    if (ok)
      printf("{\"contract\":\"labfy.local_jobs.command.v1\","
             "\"accepted\":true,\"was_running\":%s}\n",
             was_running ? "true" : "false");
  }
  else if (store != NULL && strcmp(command, "pause") == 0)
    ok = local_job_store_set_paused(store, TRUE, now, error);
  else if (store != NULL && strcmp(command, "resume") == 0)
    ok = local_job_store_set_paused(store, FALSE, now, error) &&
         local_job_store_set_stopped(store, FALSE, now, error);
  else if (store != NULL && strcmp(command, "stop") == 0)
    ok = local_job_store_request_stop(store, now, error);
  g_free(now);
  local_job_store_close(store);
  manifest_clear(&manifest);
  return ok;
}

static int demonstrate(const char *root, const char *self, GError **error) {
  if (!init_specimen(root, error) || !enqueue_jobs(root, error))
    return 1;
  /* Deux processus réellement interrompus couvrent successivement les deux
   * publications. Le successeur réouvre les mêmes fichiers et réconcilie le
   * résultat typé avant de réclamer la demande suivante. */
  for (guint crash = 0; crash < 2; crash++) {
    pid_t child = fork();
    if (child == 0) {
      execl(self, self, "__test-crash", "--workspace", root, NULL);
      _exit(127);
    }
    int status = 0;
    if (child < 0 || waitpid(child, &status, 0) != child ||
        !WIFEXITED(status) || WEXITSTATUS(status) != 86) {
      g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                          "Le point de crash contrôlé n'a pas été atteint.");
      return 1;
    }
  }
  if (!run_worker(root, 0U, error))
    return 1;
  printf("SPECIMEN J5 repris sur le même espace : %s\n", root);
  return status_or_export(root, TRUE, error) ? 0 : 1;
}

int main(int argc, char **argv) {
  const char *workspace = NULL;
  for (int i = 2; i + 1 < argc; i++)
    if (strcmp(argv[i], "--workspace") == 0)
      workspace = argv[i + 1];
  if (argc < 2 || workspace == NULL) {
    fprintf(stderr, "Usage: %s COMMAND --workspace DIR [JOB_ID]\n", argv[0]);
    return 2;
  }
  GError *error = NULL;
  gboolean ok = FALSE;
  if (strcmp(argv[1], "init-specimen") == 0)
    ok = init_specimen(workspace, &error);
  else if (strcmp(argv[1], "create-workspace") == 0) {
    const char *title = NULL;
    for (int i = 4; i + 1 < argc; i += 2)
      if (strcmp(argv[i], "--title") == 0) title = argv[i + 1];
    ok = create_workspace(workspace, title, &error);
  } else if (strcmp(argv[1], "confirm-import-json") == 0 ||
             strcmp(argv[1], "__test-crash-import-after-commit") == 0) {
    const char *upload=NULL,*key=NULL,*evidence=NULL,*name=NULL,*type=NULL;
    const char *size=NULL,*hash=NULL,*source=NULL,*description=NULL;
    for(int i=4;i+1<argc;i+=2){
      if(strcmp(argv[i],"--upload")==0)upload=argv[i+1];
      else if(strcmp(argv[i],"--key")==0)key=argv[i+1];
      else if(strcmp(argv[i],"--evidence")==0)evidence=argv[i+1];
      else if(strcmp(argv[i],"--name")==0)name=argv[i+1];
      else if(strcmp(argv[i],"--type")==0)type=argv[i+1];
      else if(strcmp(argv[i],"--size")==0)size=argv[i+1];
      else if(strcmp(argv[i],"--sha256")==0)hash=argv[i+1];
      else if(strcmp(argv[i],"--source")==0)source=argv[i+1];
      else if(strcmp(argv[i],"--description")==0)description=argv[i+1];}
    ok=confirm_import(workspace,upload,key,evidence,name,type,size,hash,
                      source,description,
                      strcmp(argv[1],"__test-crash-import-after-commit")==0,
                      &error);
  }
  else if (strcmp(argv[1], "init-j7-specimen") == 0)
    ok = init_j7_specimen(workspace, &error);
  else if (strcmp(argv[1], "evidence-preview-json") == 0) {
    const char *evidence = NULL;
    for (int i = 4; i + 1 < argc; i += 2)
      if (strcmp(argv[i], "--evidence") == 0) evidence = argv[i + 1];
    ok = print_evidence_preview(workspace, evidence, &error);
  } else if (strcmp(argv[1], "observations-json") == 0) {
    const char *evidence = NULL, *extraction = NULL;
    for (int i = 4; i + 1 < argc; i += 2) {
      if (strcmp(argv[i], "--evidence") == 0) evidence = argv[i + 1];
      else if (strcmp(argv[i], "--extraction") == 0) extraction = argv[i + 1];
    }
    ok = print_observations(workspace, evidence, extraction, &error);
  } else if (strcmp(argv[1], "review-observation-json") == 0) {
    const char *evidence=NULL,*observation=NULL,*operation=NULL,*revision=NULL;
    const char *action=NULL,*status=NULL,*corrected=NULL,*entity=NULL;
    const char *author=NULL,*reason=NULL;
    for (int i=4;i+1<argc;i+=2) {
      if(strcmp(argv[i],"--evidence")==0)evidence=argv[i+1];
      else if(strcmp(argv[i],"--observation")==0)observation=argv[i+1];
      else if(strcmp(argv[i],"--operation")==0)operation=argv[i+1];
      else if(strcmp(argv[i],"--revision")==0)revision=argv[i+1];
      else if(strcmp(argv[i],"--action")==0)action=argv[i+1];
      else if(strcmp(argv[i],"--status")==0)status=argv[i+1];
      else if(strcmp(argv[i],"--corrected")==0)corrected=argv[i+1];
      else if(strcmp(argv[i],"--entity")==0)entity=argv[i+1];
      else if(strcmp(argv[i],"--author")==0)author=argv[i+1];
      else if(strcmp(argv[i],"--reason")==0)reason=argv[i+1];
    }
    ok=review_observation(workspace,evidence,observation,operation,revision,
                          action,status,corrected,entity,author,reason,&error);
  }
  else if (strcmp(argv[1], "enqueue") == 0)
    ok = enqueue_jobs(workspace, &error);
  else if (strcmp(argv[1], "submit-json") == 0 && argc >= 9) {
    const char *evidence = NULL, *capability = NULL, *key = NULL;
    for (int i = 4; i + 1 < argc; i += 2) {
      if (strcmp(argv[i], "--evidence") == 0)
        evidence = argv[i + 1];
      else if (strcmp(argv[i], "--capability") == 0)
        capability = argv[i + 1];
      else if (strcmp(argv[i], "--key") == 0)
        key = argv[i + 1];
    }
    char *job_id = NULL;
    ok = submit_job(workspace, evidence, capability, key, &job_id, &error);
    if (ok)
      printf("{\"contract\":\"labfy.local_jobs.command.v1\","
             "\"accepted\":true,\"job_id\":\"%s\"}\n",
             job_id);
    g_free(job_id);
  } else if (strcmp(argv[1], "submit-plan-json") == 0) {
    const char *revision=NULL,*profile=NULL,*key=NULL,*ids=NULL;
    for(int i=4;i+1<argc;i+=2){if(strcmp(argv[i],"--revision")==0)revision=argv[i+1];else if(strcmp(argv[i],"--profile")==0)profile=argv[i+1];else if(strcmp(argv[i],"--key")==0)key=argv[i+1];else if(strcmp(argv[i],"--recommendations")==0)ids=argv[i+1];}
    ok=submit_plan(workspace,revision,profile,key,ids,FALSE,&error);
  } else if (strcmp(argv[1], "__test-crash-after-admission") == 0) {
    const char *revision=NULL,*profile=NULL,*key=NULL,*ids=NULL;
    for(int i=4;i+1<argc;i+=2){if(strcmp(argv[i],"--revision")==0)revision=argv[i+1];else if(strcmp(argv[i],"--profile")==0)profile=argv[i+1];else if(strcmp(argv[i],"--key")==0)key=argv[i+1];else if(strcmp(argv[i],"--recommendations")==0)ids=argv[i+1];}
    ok=submit_plan(workspace,revision,profile,key,ids,TRUE,&error);
  } else if (strcmp(argv[1], "lookup-plan-json") == 0) {
    const char *revision=NULL,*profile=NULL,*key=NULL,*ids=NULL;
    for(int i=4;i+1<argc;i+=2){if(strcmp(argv[i],"--revision")==0)revision=argv[i+1];else if(strcmp(argv[i],"--profile")==0)profile=argv[i+1];else if(strcmp(argv[i],"--key")==0)key=argv[i+1];else if(strcmp(argv[i],"--recommendations")==0)ids=argv[i+1];}
    ok=lookup_plan(workspace,revision,profile,key,ids,&error);
  } else if (strcmp(argv[1], "prepare-report-json") == 0) {
    const char *objects=NULL,*title=NULL,*comment=NULL,*generated=NULL,*sections=NULL;
    for(int i=4;i+1<argc;i+=2){if(strcmp(argv[i],"--objects")==0)objects=argv[i+1];else if(strcmp(argv[i],"--title")==0)title=argv[i+1];else if(strcmp(argv[i],"--comment")==0)comment=argv[i+1];else if(strcmp(argv[i],"--generated-at")==0)generated=argv[i+1];else if(strcmp(argv[i],"--sections")==0)sections=argv[i+1];}
    ok=prepare_report(workspace,objects,title,comment,generated,sections,&error);
  } else if (strcmp(argv[1], "research-prepare-json") == 0) {
    const char *selection=NULL,*question=NULL,*exclusions=NULL,*revision=NULL,*key=NULL;
    for(int i=4;i+1<argc;i+=2){if(strcmp(argv[i],"--selection")==0)selection=argv[i+1];else if(strcmp(argv[i],"--question")==0)question=argv[i+1];else if(strcmp(argv[i],"--exclusions")==0)exclusions=argv[i+1];else if(strcmp(argv[i],"--revision")==0)revision=argv[i+1];else if(strcmp(argv[i],"--key")==0)key=argv[i+1];}
    ok=research_prepare_json(workspace,selection,question,exclusions,revision,key,&error);
  } else if (strcmp(argv[1], "research-grant-json") == 0) {
    const char *plan=NULL,*revision=NULL,*actions=NULL,*decisions=NULL,*exclusions=NULL,*key=NULL;
    for(int i=4;i+1<argc;i+=2){if(strcmp(argv[i],"--plan")==0)plan=argv[i+1];else if(strcmp(argv[i],"--revision")==0)revision=argv[i+1];else if(strcmp(argv[i],"--actions")==0)actions=argv[i+1];else if(strcmp(argv[i],"--decisions")==0)decisions=argv[i+1];else if(strcmp(argv[i],"--exclusions")==0)exclusions=argv[i+1];else if(strcmp(argv[i],"--key")==0)key=argv[i+1];}
    ok=research_grant_json(workspace,plan,revision,actions,decisions,exclusions,key,&error);
  } else if (strcmp(argv[1], "research-campaign-json") == 0) {
    const char *grant=NULL,*revision=NULL,*actions=NULL,*key=NULL;
    for(int i=4;i+1<argc;i+=2){if(strcmp(argv[i],"--grant")==0)grant=argv[i+1];else if(strcmp(argv[i],"--revision")==0)revision=argv[i+1];else if(strcmp(argv[i],"--actions")==0)actions=argv[i+1];else if(strcmp(argv[i],"--key")==0)key=argv[i+1];}
    ok=research_campaign_json(workspace,grant,revision,actions,key,&error);
  } else if (strcmp(argv[1], "research-snapshot-json") == 0)
    ok=research_snapshot_json(workspace,&error);
  else if (strcmp(argv[1], "research-revoke-json") == 0) {
    const char *grant=NULL;for(int i=4;i+1<argc;i+=2)if(strcmp(argv[i],"--grant")==0)grant=argv[i+1];
    ok=research_revoke_json(workspace,grant,&error);
  } else if (strcmp(argv[1], "run") == 0)
    ok = run_worker(workspace, 0U, &error);
  else if (strcmp(argv[1], "__test-crash-after-claim") == 0)
    ok = run_worker(workspace, 1U, &error);
  else if (strcmp(argv[1], "__test-crash") == 0)
    ok = run_worker(workspace, 2U, &error);
  else if (strcmp(argv[1], "status") == 0)
    ok = status_or_export(workspace, TRUE, &error);
  else if (strcmp(argv[1], "export") == 0)
    ok = status_or_export(workspace, FALSE, &error);
  else if (strcmp(argv[1], "demo") == 0)
    return demonstrate(workspace, argv[0], &error);
  else if (strcmp(argv[1], "cancel") == 0 && argc >= 5)
    ok = control(workspace, argv[1], argv[4], &error);
  else if (strcmp(argv[1], "pause") == 0 || strcmp(argv[1], "resume") == 0 ||
           strcmp(argv[1], "stop") == 0)
    ok = control(workspace, argv[1], NULL, &error);
  if (!ok) {
    fprintf(stderr, "local-jobs: %s\n",
            error != NULL ? error->message : "commande invalide");
    g_clear_error(&error);
    return 1;
  }
  return 0;
}
