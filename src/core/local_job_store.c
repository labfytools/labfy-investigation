/******************************************************************************
 * @file local_job_store.c
 * @brief JobStore SQLite V3, distinct de la base métier V20.
 ******************************************************************************/
#include "core/local_job_store.h"

#include <errno.h>
#include <glib/gstdio.h>
#include <json-glib/json-glib.h>
#include <sqlite3.h>
#include <string.h>

struct LocalJobStore {
  sqlite3 *db;
  char *path;
  char *investigation_id;
};

static const char *const schema_sql =
    "PRAGMA foreign_keys=ON;"
    "CREATE TABLE metadata(key TEXT PRIMARY KEY,value TEXT NOT NULL);"
    "CREATE TABLE controls(singleton INTEGER PRIMARY KEY CHECK(singleton=1),"
    " paused INTEGER NOT NULL CHECK(paused IN(0,1)),"
    " stop_requested INTEGER NOT NULL CHECK(stop_requested IN(0,1)),"
    " updated_at TEXT NOT NULL);"
    "CREATE TABLE jobs("
    " job_id TEXT PRIMARY KEY,request_id TEXT NOT NULL UNIQUE,"
    " source_evidence_id TEXT NOT NULL,derivative_evidence_id TEXT NOT NULL "
    "UNIQUE,"
    " capability_id TEXT NOT NULL,adapter_id TEXT NOT NULL,adapter_version "
    "TEXT NOT NULL,"
    " source_sha256 TEXT NOT NULL,source_size INTEGER NOT NULL "
    "CHECK(source_size>=0),"
    " requested_at TEXT NOT NULL,parameters_json TEXT NOT NULL,"
    " state TEXT NOT NULL,cancel_requested INTEGER NOT NULL DEFAULT 0,"
    " max_attempts INTEGER NOT NULL CHECK(max_attempts BETWEEN 1 AND 8),"
    " attempt_count INTEGER NOT NULL DEFAULT 0,owner_token TEXT,heartbeat_at "
    "TEXT,"
    " result_status TEXT,diagnostic TEXT CHECK(diagnostic IS NULL OR "
    "length(CAST(diagnostic AS BLOB))<=2048),dependency_job_id TEXT REFERENCES "
    "jobs(job_id),"
    " revision INTEGER NOT NULL DEFAULT 1);"
    "CREATE TABLE attempts("
    " attempt_id TEXT PRIMARY KEY,job_id TEXT NOT NULL REFERENCES jobs(job_id),"
    " rank INTEGER NOT NULL,owner_token TEXT NOT NULL,started_at TEXT NOT NULL,"
    " heartbeat_at TEXT NOT NULL,finished_at TEXT,state TEXT NOT NULL,"
    " diagnostic TEXT CHECK(diagnostic IS NULL OR "
    "length(CAST(diagnostic AS BLOB))<=2048),reconciliation TEXT,"
    "reserved_active_ms INTEGER NOT NULL DEFAULT 0 CHECK(reserved_active_ms>=0),"
    "elapsed_active_ms INTEGER CHECK(elapsed_active_ms IS NULL OR elapsed_active_ms>=0),"
    "budget_finalized INTEGER NOT NULL DEFAULT 0 CHECK(budget_finalized IN(0,1)),"
    "UNIQUE(job_id,rank));"
    "CREATE TABLE transitions("
    " transition_id INTEGER PRIMARY KEY AUTOINCREMENT,job_id TEXT NOT NULL "
    "REFERENCES jobs(job_id),"
    " from_state TEXT,to_state TEXT NOT NULL,reason TEXT NOT NULL,created_at "
    "TEXT NOT NULL);"
    "CREATE INDEX idx_jobs_state ON jobs(state,requested_at,job_id);"
    "CREATE UNIQUE INDEX idx_jobs_active_analysis ON jobs(source_evidence_id,capability_id) "
    "WHERE state IN('QUEUED','RUNNING','RETRY_WAIT');";

static const char *plan_schema_sql =
    "CREATE TABLE plans(plan_id TEXT PRIMARY KEY,idempotency_key TEXT NOT NULL UNIQUE,"
    "intent_hash TEXT NOT NULL,input_revision TEXT NOT NULL,profile_id TEXT NOT NULL,"
    "created_at TEXT NOT NULL,state TEXT NOT NULL DEFAULT 'APPROVED',"
    "max_analyses INTEGER NOT NULL,max_attempts_total INTEGER NOT NULL,"
    "max_source_bytes INTEGER NOT NULL,max_active_ms INTEGER NOT NULL,"
    "reserved_analyses INTEGER NOT NULL,reserved_attempts INTEGER NOT NULL,"
    "reserved_source_bytes INTEGER NOT NULL,consumed_attempts INTEGER NOT NULL DEFAULT 0,"
    "consumed_active_ms INTEGER NOT NULL DEFAULT 0);"
    "CREATE TABLE plan_jobs(plan_id TEXT NOT NULL REFERENCES plans(plan_id),"
    "job_id TEXT NOT NULL UNIQUE REFERENCES jobs(job_id),rank INTEGER NOT NULL,"
    "PRIMARY KEY(plan_id,rank));";

static const char *v2_to_v3_sql =
    "ALTER TABLE attempts ADD COLUMN reserved_active_ms INTEGER NOT NULL DEFAULT 0 CHECK(reserved_active_ms>=0);"
    "ALTER TABLE attempts ADD COLUMN elapsed_active_ms INTEGER CHECK(elapsed_active_ms IS NULL OR elapsed_active_ms>=0);"
    "ALTER TABLE attempts ADD COLUMN budget_finalized INTEGER NOT NULL DEFAULT 0 CHECK(budget_finalized IN(0,1));"
    "CREATE UNIQUE INDEX idx_jobs_active_analysis ON jobs(source_evidence_id,capability_id) "
    "WHERE state IN('QUEUED','RUNNING','RETRY_WAIT');";

#ifdef LOCAL_JOB_STORE_ENABLE_TEST_HOOKS
static gboolean fail_migration_after_schema = FALSE;
void local_job_store_test_fail_migration_after_schema(gboolean enabled) {
  fail_migration_after_schema = enabled;
}
#endif

static void job_error(GError **error, GIOErrorEnum code, const char *message) {
  if (error != NULL && *error == NULL)
    g_set_error_literal(error, G_IO_ERROR, code, message);
}

static gboolean exec_sql(sqlite3 *db, const char *sql, GError **error) {
  char *message = NULL;
  if (sqlite3_exec(db, sql, NULL, NULL, &message) == SQLITE_OK)
    return TRUE;
  if (error != NULL && *error == NULL)
    g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "JobStore SQLite : %s",
                message != NULL ? message : "erreur");
  sqlite3_free(message);
  return FALSE;
}

const char *local_job_state_code(LocalJobState state) {
  static const char *const values[] = {
      "QUEUED", "RUNNING",   "RETRY_WAIT",        "COMPLETED",
      "FAILED", "CANCELLED", "RECOVERY_REQUIRED", "BLOCKED"};
  return state <= LOCAL_JOB_BLOCKED ? values[state] : "BLOCKED";
}

static gboolean state_parse(const char *value, LocalJobState *out) {
  for (int state = LOCAL_JOB_QUEUED; state <= LOCAL_JOB_BLOCKED; state++)
    if (g_strcmp0(value, local_job_state_code(state)) == 0) {
      *out = (LocalJobState)state;
      return TRUE;
    }
  return FALSE;
}

static gboolean bind_text(sqlite3_stmt *statement, int index,
                          const char *value) {
  return sqlite3_bind_text(statement, index, value, -1, SQLITE_TRANSIENT) ==
         SQLITE_OK;
}

static LocalJobStore *store_open_internal(const char *path,
                                          const char *investigation_id,
                                          int flags, GError **error) {
  if (path == NULL || investigation_id == NULL ||
      !g_uuid_string_is_valid(investigation_id)) {
    job_error(error, G_IO_ERROR_INVALID_ARGUMENT,
              "Chemin ou UUID JobStore invalide.");
    return NULL;
  }
  LocalJobStore *store = g_new0(LocalJobStore, 1);
  if (sqlite3_open_v2(path, &store->db, flags, NULL) != SQLITE_OK) {
    job_error(error, G_IO_ERROR_NOT_FOUND,
              "Le JobStore ne peut pas être ouvert.");
    local_job_store_close(store);
    return NULL;
  }
  store->path = g_strdup(path);
  store->investigation_id = g_strdup(investigation_id);
  sqlite3_busy_timeout(store->db, 1000);
  if (!exec_sql(store->db, "PRAGMA foreign_keys=ON;", error)) {
    local_job_store_close(store);
    return NULL;
  }
  return store;
}

LocalJobStore *local_job_store_create(const char *path,
                                      const char *investigation_id,
                                      GError **error) {
  if (g_file_test(path, G_FILE_TEST_EXISTS)) {
    job_error(error, G_IO_ERROR_EXISTS, "Le JobStore existe déjà.");
    return NULL;
  }
  char *directory = g_path_get_dirname(path);
  if (g_mkdir_with_parents(directory, 0700) != 0) {
    g_free(directory);
    job_error(error, G_IO_ERROR_FAILED,
              "Impossible de créer le répertoire privé du JobStore.");
    return NULL;
  }
  g_free(directory);
  LocalJobStore *store = store_open_internal(
      path, investigation_id,
      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_EXCLUSIVE,
      error);
  if (store == NULL)
    return NULL;
  char *sql = g_strdup_printf(
      "BEGIN IMMEDIATE;%s"
      "%sINSERT INTO metadata VALUES('schema_version','3');"
      "INSERT INTO metadata VALUES('investigation_id','%s');"
      "INSERT INTO metadata VALUES('revision','0');"
      "INSERT INTO controls VALUES(1,0,0,'1970-01-01T00:00:00Z');COMMIT;",
      schema_sql, plan_schema_sql, investigation_id);
  gboolean success =
      exec_sql(store->db, sql, error) && g_chmod(path, 0600) == 0;
  g_free(sql);
  if (!success) {
    local_job_store_close(store);
    (void)g_remove(path);
    return NULL;
  }
  return store;
}

LocalJobStore *local_job_store_open(const char *path,
                                    const char *investigation_id,
                                    gboolean read_only, GError **error) {
  if (!g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
    job_error(error, G_IO_ERROR_NOT_FOUND, "Le JobStore attendu est absent.");
    return NULL;
  }
  LocalJobStore *store = store_open_internal(
      path, investigation_id,
      read_only ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE, error);
  sqlite3_stmt *statement = NULL;
  const char *sql = "SELECT key,value FROM metadata WHERE key "
                    "IN('schema_version','investigation_id');";
  int schema_version = 0;
  gboolean owner = FALSE;
  if (store == NULL ||
      sqlite3_prepare_v2(store->db, sql, -1, &statement, NULL) != SQLITE_OK)
    goto failure;
  while (sqlite3_step(statement) == SQLITE_ROW) {
    const char *key = (const char *)sqlite3_column_text(statement, 0);
    const char *value = (const char *)sqlite3_column_text(statement, 1);
    if (g_strcmp0(key, "schema_version") == 0)
      schema_version = (int)g_ascii_strtoll(value, NULL, 10);
    if (g_strcmp0(key, "investigation_id") == 0)
      owner = g_strcmp0(value, investigation_id) == 0;
  }
  sqlite3_finalize(statement);
  if ((schema_version < 1 || schema_version > 3) || !owner) {
    job_error(error, G_IO_ERROR_INVALID_DATA,
              schema_version < 1 || schema_version > 3
                  ? "Version JobStore inconnue ; aucune recréation automatique."
                  : "Le JobStore appartient à une autre enquête.");
    local_job_store_close(store);
    return NULL;
  }
  /* WHY: V2 ne touche que l'ordonnanceur local. Le lecteur read-only conserve
   * la compatibilité V1 et ne migre jamais; seul un ouvreur exclusif en écriture
   * applique atomiquement l'ajout non destructif des plans et budgets. */
  if (schema_version < 3 && !read_only) {
    /* CONTRACT: schéma, données et version sont une seule transaction. La
     * séparation des appels rend testable un crash logique après mutation,
     * sans affaiblir l'atomicité SQLite. */
    gboolean migrated = exec_sql(store->db, "BEGIN IMMEDIATE;", error);
    if (migrated && schema_version == 1)
      migrated = exec_sql(store->db, plan_schema_sql, error);
    if (migrated)
      migrated = exec_sql(store->db, v2_to_v3_sql, error);
#ifdef LOCAL_JOB_STORE_ENABLE_TEST_HOOKS
    if (migrated && fail_migration_after_schema) {
      job_error(error, G_IO_ERROR_FAILED,
                "Faute de migration synthétique après mutation du schéma.");
      migrated = FALSE;
    }
#endif
    if (migrated)
      migrated = exec_sql(store->db,
          "UPDATE metadata SET value='3' WHERE key='schema_version';", error);
    if (migrated)
      migrated = exec_sql(store->db, "COMMIT;", error);
    if (!migrated) {
      exec_sql(store->db, "ROLLBACK;", NULL);
      local_job_store_close(store);
      return NULL;
    }
  }
  return store;
failure:
  sqlite3_finalize(statement);
  local_job_store_close(store);
  job_error(error, G_IO_ERROR_INVALID_DATA, "Métadonnées JobStore invalides.");
  return NULL;
}

void local_job_store_close(LocalJobStore *store) {
  if (store != NULL) {
    if (store->db != NULL)
      sqlite3_close(store->db);
    g_free(store->path);
    g_free(store->investigation_id);
    g_free(store);
  }
}

void local_job_record_free(LocalJobRecord *r) {
  if (r == NULL)
    return;
#define FREE_FIELD(name) g_free(r->name)
  FREE_FIELD(job_id);
  FREE_FIELD(request_id);
  FREE_FIELD(source_evidence_id);
  FREE_FIELD(derivative_evidence_id);
  FREE_FIELD(capability_id);
  FREE_FIELD(adapter_id);
  FREE_FIELD(adapter_version);
  FREE_FIELD(source_sha256);
  FREE_FIELD(requested_at);
  FREE_FIELD(parameters_json);
  FREE_FIELD(result_status);
  FREE_FIELD(diagnostic);
  FREE_FIELD(dependency_job_id);
#undef FREE_FIELD
  g_free(r);
}

static LocalJobRecord *record_from_stmt(sqlite3_stmt *s) {
  LocalJobRecord *r = g_new0(LocalJobRecord, 1);
#define COPYCOL(field, n)                                                      \
  r->field = g_strdup((const char *)sqlite3_column_text(s, n))
  COPYCOL(job_id, 0);
  COPYCOL(request_id, 1);
  COPYCOL(source_evidence_id, 2);
  COPYCOL(derivative_evidence_id, 3);
  COPYCOL(capability_id, 4);
  COPYCOL(adapter_id, 5);
  COPYCOL(adapter_version, 6);
  COPYCOL(source_sha256, 7);
  r->source_size = (guint64)sqlite3_column_int64(s, 8);
  COPYCOL(requested_at, 9);
  COPYCOL(parameters_json, 10);
  const char *state = (const char *)sqlite3_column_text(s, 11);
  if (!state_parse(state, &r->state))
    r->state = LOCAL_JOB_BLOCKED;
  r->cancel_requested = sqlite3_column_int(s, 12) != 0;
  r->max_attempts = (guint)sqlite3_column_int(s, 13);
  r->attempt_count = (guint)sqlite3_column_int(s, 14);
  COPYCOL(result_status, 15);
  COPYCOL(diagnostic, 16);
  COPYCOL(dependency_job_id, 17);
  r->revision = (guint64)sqlite3_column_int64(s, 18);
#undef COPYCOL
  return r;
}

#define JOB_COLUMNS                                                            \
  "job_id,request_id,source_evidence_id,derivative_evidence_id,"               \
  "capability_id,adapter_id,adapter_version,source_sha256,source_size,"        \
  "requested_at,"                                                              \
  "parameters_json,state,cancel_requested,max_attempts,attempt_count,result_"  \
  "status,"                                                                    \
  "diagnostic,dependency_job_id,revision"

LocalJobRecord *local_job_store_find(LocalJobStore *store, const char *job_id,
                                     GError **error) {
  sqlite3_stmt *s = NULL;
  LocalJobRecord *r = NULL;
  char *sql =
      g_strdup_printf("SELECT %s FROM jobs WHERE job_id=?1;", JOB_COLUMNS);
  if (store == NULL || job_id == NULL ||
      sqlite3_prepare_v2(store->db, sql, -1, &s, NULL) != SQLITE_OK ||
      !bind_text(s, 1, job_id))
    job_error(error, G_IO_ERROR_FAILED, "Lecture job impossible.");
  else if (sqlite3_step(s) == SQLITE_ROW)
    r = record_from_stmt(s);
  sqlite3_finalize(s);
  g_free(sql);
  return r;
}

GPtrArray *local_job_store_list(LocalJobStore *store, GError **error) {
  sqlite3_stmt *s = NULL;
  GPtrArray *items =
      g_ptr_array_new_with_free_func((GDestroyNotify)local_job_record_free);
  char *sql = g_strdup_printf(
      "SELECT %s FROM jobs ORDER BY requested_at,job_id;", JOB_COLUMNS);
  if (store == NULL ||
      sqlite3_prepare_v2(store->db, sql, -1, &s, NULL) != SQLITE_OK) {
    job_error(error, G_IO_ERROR_FAILED, "Liste jobs impossible.");
    g_ptr_array_unref(items);
    items = NULL;
  } else
    while (sqlite3_step(s) == SQLITE_ROW)
      g_ptr_array_add(items, record_from_stmt(s));
  sqlite3_finalize(s);
  g_free(sql);
  return items;
}

static gboolean submission_valid(const LocalJobSubmission *s) {
  return s != NULL && g_uuid_string_is_valid(s->job_id) &&
         g_uuid_string_is_valid(s->request_id) &&
         g_uuid_string_is_valid(s->source_evidence_id) &&
         g_uuid_string_is_valid(s->derivative_evidence_id) &&
         s->capability_id != NULL && s->adapter_id != NULL &&
         s->adapter_version != NULL && s->source_sha256 != NULL &&
         strlen(s->source_sha256) == 64U && s->requested_at != NULL &&
         s->parameters_json != NULL && s->max_attempts > 0U &&
         s->max_attempts <= 8U;
}

static gboolean local_job_store_has_capacity(LocalJobStore *store) {
  sqlite3_stmt *statement = NULL;
  gboolean has_capacity = FALSE;
  if (sqlite3_prepare_v2(store->db, "SELECT count(*) FROM jobs;", -1,
                         &statement, NULL) == SQLITE_OK &&
      sqlite3_step(statement) == SQLITE_ROW)
    has_capacity =
        sqlite3_column_int64(statement, 0) < LOCAL_JOB_STORE_MAX_JOBS;
  sqlite3_finalize(statement);
  return has_capacity;
}

gboolean local_job_store_enqueue(LocalJobStore *store,
                                 const LocalJobSubmission *s,
                                 gboolean *out_reused, GError **error) {
  if (out_reused != NULL)
    *out_reused = FALSE;
  if (store == NULL || !submission_valid(s)) {
    job_error(error, G_IO_ERROR_INVALID_ARGUMENT, "Soumission job invalide.");
    return FALSE;
  }
  LocalJobRecord *existing = local_job_store_find(store, s->job_id, error);
  if (existing != NULL) {
    gboolean same =
        g_strcmp0(existing->request_id, s->request_id) == 0 &&
        g_strcmp0(existing->source_evidence_id, s->source_evidence_id) == 0 &&
        g_strcmp0(existing->derivative_evidence_id,
                  s->derivative_evidence_id) == 0 &&
        g_strcmp0(existing->capability_id, s->capability_id) == 0 &&
        g_strcmp0(existing->source_sha256, s->source_sha256) == 0 &&
        existing->source_size == s->source_size &&
        g_strcmp0(existing->parameters_json, s->parameters_json) == 0;
    local_job_record_free(existing);
    if (!same) {
      job_error(error, G_IO_ERROR_EXISTS,
                "Identité job réutilisée avec un contenu différent.");
      return FALSE;
    }
    if (out_reused != NULL)
      *out_reused = TRUE;
    return TRUE;
  }
  if (!local_job_store_has_capacity(store)) {
    job_error(error, G_IO_ERROR_NO_SPACE,
              "La limite persistante de jobs est atteinte.");
    return FALSE;
  }
  if (s->dependency_job_id != NULL) {
    LocalJobRecord *dependency =
        local_job_store_find(store, s->dependency_job_id, error);
    if (dependency == NULL || g_strcmp0(dependency->job_id, s->job_id) == 0) {
      local_job_record_free(dependency);
      job_error(error, G_IO_ERROR_INVALID_ARGUMENT,
                "Dépendance absente, étrangère ou cyclique.");
      return FALSE;
    }
    local_job_record_free(dependency);
  }
  sqlite3_stmt *q = NULL;
  const char *sql =
      "INSERT INTO "
      "jobs(job_id,request_id,source_evidence_id,derivative_evidence_id,"
      "capability_id,adapter_id,adapter_version,source_sha256,source_size,"
      "requested_at,parameters_json,state,max_attempts,dependency_job_id) "
      "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,'QUEUED',?12,?13);";
  gboolean ok =
      sqlite3_prepare_v2(store->db, sql, -1, &q, NULL) == SQLITE_OK &&
      bind_text(q, 1, s->job_id) && bind_text(q, 2, s->request_id) &&
      bind_text(q, 3, s->source_evidence_id) &&
      bind_text(q, 4, s->derivative_evidence_id) &&
      bind_text(q, 5, s->capability_id) && bind_text(q, 6, s->adapter_id) &&
      bind_text(q, 7, s->adapter_version) &&
      bind_text(q, 8, s->source_sha256) &&
      sqlite3_bind_int64(q, 9, (sqlite3_int64)s->source_size) == SQLITE_OK &&
      bind_text(q, 10, s->requested_at) &&
      bind_text(q, 11, s->parameters_json) &&
      sqlite3_bind_int(q, 12, (int)s->max_attempts) == SQLITE_OK &&
      (s->dependency_job_id ? bind_text(q, 13, s->dependency_job_id)
                            : sqlite3_bind_null(q, 13) == SQLITE_OK) &&
      sqlite3_step(q) == SQLITE_DONE;
  sqlite3_finalize(q);
  if (!ok)
    job_error(error, G_IO_ERROR_FAILED,
              "Insertion job refusée (conflit, limite ou dépendance).");
  return ok;
}

gboolean local_job_store_admit_plan(LocalJobStore *store,
                                    const LocalPlanAdmission *plan,
                                    const LocalJobSubmission *jobs,
                                    gsize job_count, gboolean *out_reused,
                                    GError **error) {
  if (out_reused) *out_reused = FALSE;
  if (!store || !plan || !g_uuid_string_is_valid(plan->plan_id) ||
      !g_uuid_string_is_valid(plan->idempotency_key) || !plan->intent_hash ||
      !plan->input_revision || !plan->profile_id || !plan->created_at ||
      !jobs || job_count == 0 || job_count > plan->max_analyses ||
      plan->max_attempts_total == 0 || plan->max_source_bytes == 0 ||
      plan->max_active_ms == 0) {
    job_error(error, G_IO_ERROR_INVALID_ARGUMENT, "Contrat de plan invalide.");
    return FALSE;
  }
  if (!exec_sql(store->db, "BEGIN IMMEDIATE;", error)) return FALSE;
  sqlite3_stmt *lookup = NULL;
  gboolean found = FALSE, same = FALSE;
  if (sqlite3_prepare_v2(store->db,
      "SELECT plan_id,intent_hash FROM plans WHERE idempotency_key=?1;", -1,
      &lookup, NULL) == SQLITE_OK && bind_text(lookup, 1, plan->idempotency_key) &&
      sqlite3_step(lookup) == SQLITE_ROW) {
    found = TRUE;
    same = g_strcmp0((const char *)sqlite3_column_text(lookup, 0), plan->plan_id) == 0 &&
           g_strcmp0((const char *)sqlite3_column_text(lookup, 1), plan->intent_hash) == 0;
  }
  sqlite3_finalize(lookup);
  if (found) {
    if (!same) job_error(error, G_IO_ERROR_EXISTS,
        "Clé d'idempotence réutilisée pour une autre intention.");
    else { if (out_reused) *out_reused = TRUE; exec_sql(store->db,"COMMIT;",NULL); }
    if (!same) exec_sql(store->db,"ROLLBACK;",NULL);
    return same;
  }
  guint64 bytes = 0; guint attempts = 0;
  for (gsize i = 0; i < job_count; i++) {
    if (!submission_valid(&jobs[i]) || G_MAXUINT64 - bytes < jobs[i].source_size ||
        G_MAXUINT - attempts < jobs[i].max_attempts) {
      job_error(error, G_IO_ERROR_INVALID_ARGUMENT, "Élément de plan invalide.");
      exec_sql(store->db,"ROLLBACK;",NULL);
      return FALSE;
    }
    bytes += jobs[i].source_size; attempts += jobs[i].max_attempts;
  }
  if (bytes > plan->max_source_bytes || attempts > plan->max_attempts_total) {
    job_error(error, G_IO_ERROR_NO_SPACE, "Budget partagé insuffisant.");
    exec_sql(store->db,"ROLLBACK;",NULL);
    return FALSE;
  }
  sqlite3_stmt *p = NULL;
  gboolean ok = sqlite3_prepare_v2(store->db,
      "INSERT INTO plans(plan_id,idempotency_key,intent_hash,input_revision,profile_id,created_at,"
      "max_analyses,max_attempts_total,max_source_bytes,max_active_ms,reserved_analyses,"
      "reserved_attempts,reserved_source_bytes) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13);",
      -1, &p, NULL) == SQLITE_OK && bind_text(p,1,plan->plan_id) &&
      bind_text(p,2,plan->idempotency_key) && bind_text(p,3,plan->intent_hash) &&
      bind_text(p,4,plan->input_revision) && bind_text(p,5,plan->profile_id) &&
      bind_text(p,6,plan->created_at) && sqlite3_bind_int(p,7,(int)plan->max_analyses)==SQLITE_OK &&
      sqlite3_bind_int(p,8,(int)plan->max_attempts_total)==SQLITE_OK &&
      sqlite3_bind_int64(p,9,(sqlite3_int64)plan->max_source_bytes)==SQLITE_OK &&
      sqlite3_bind_int64(p,10,(sqlite3_int64)plan->max_active_ms)==SQLITE_OK &&
      sqlite3_bind_int(p,11,(int)job_count)==SQLITE_OK &&
      sqlite3_bind_int(p,12,(int)attempts)==SQLITE_OK &&
      sqlite3_bind_int64(p,13,(sqlite3_int64)bytes)==SQLITE_OK && sqlite3_step(p)==SQLITE_DONE;
  sqlite3_finalize(p);
  for (gsize i = 0; ok && i < job_count; i++) {
    gboolean reused = FALSE;
    ok = local_job_store_enqueue(store, &jobs[i], &reused, error) && !reused;
    sqlite3_stmt *link = NULL;
    ok = ok && sqlite3_prepare_v2(store->db,
        "INSERT INTO plan_jobs VALUES(?1,?2,?3);", -1, &link, NULL)==SQLITE_OK &&
        bind_text(link,1,plan->plan_id) && bind_text(link,2,jobs[i].job_id) &&
        sqlite3_bind_int(link,3,(int)i)==SQLITE_OK && sqlite3_step(link)==SQLITE_DONE;
    sqlite3_finalize(link);
  }
  ok = ok && exec_sql(store->db, "COMMIT;", error);
  if (!ok) exec_sql(store->db, "ROLLBACK;", NULL);
  return ok;
}

gboolean local_job_store_find_plan_intention(LocalJobStore *store,
    const char *key, const char *intent_hash, char **out_plan_id,
    GError **error) {
  if(out_plan_id)*out_plan_id=NULL;
  if(!store||!g_uuid_string_is_valid(key)||!intent_hash||!out_plan_id){
    job_error(error,G_IO_ERROR_INVALID_ARGUMENT,"Lookup de plan invalide.");return FALSE;}
  sqlite3_stmt *q=NULL;gboolean ok=sqlite3_prepare_v2(store->db,
      "SELECT plan_id,intent_hash FROM plans WHERE idempotency_key=?1;",-1,&q,NULL)==SQLITE_OK&&bind_text(q,1,key);
  if(ok&&sqlite3_step(q)==SQLITE_ROW){const char *stored=(const char*)sqlite3_column_text(q,1);
    if(g_strcmp0(stored,intent_hash)!=0){job_error(error,G_IO_ERROR_EXISTS,"Clé d'idempotence réutilisée pour une autre intention.");ok=FALSE;}
    else *out_plan_id=g_strdup((const char*)sqlite3_column_text(q,0));}
  sqlite3_finalize(q);if(!ok&&error!=NULL&&*error==NULL)job_error(error,G_IO_ERROR_FAILED,"Lookup de plan impossible.");return ok;
}

static gboolean transition(sqlite3 *db, const char *job, const char *from,
                           const char *to, const char *reason,
                           const char *now) {
  sqlite3_stmt *s = NULL;
  gboolean ok =
      sqlite3_prepare_v2(db,
                         "INSERT INTO "
                         "transitions(job_id,from_state,to_state,reason,"
                         "created_at) VALUES(?1,?2,?3,?4,?5);",
                         -1, &s, NULL) == SQLITE_OK &&
      bind_text(s, 1, job) &&
      (from ? bind_text(s, 2, from) : sqlite3_bind_null(s, 2) == SQLITE_OK) &&
      bind_text(s, 3, to) && bind_text(s, 4, reason) && bind_text(s, 5, now) &&
      sqlite3_step(s) == SQLITE_DONE;
  sqlite3_finalize(s);
  return ok;
}

gboolean local_job_store_claim_next_budgeted(LocalJobStore *store,
    const char *owner, const char *attempt, const char *now,
    guint requested_active_ms, guint *out_reserved_active_ms,
    LocalJobRecord **out, GError **error) {
  if (out)
    *out = NULL;
  if (out_reserved_active_ms) *out_reserved_active_ms = 0;
  if (!store || !owner || !attempt || !now || !out ||
      requested_active_ms == 0U || !out_reserved_active_ms) {
    job_error(error, G_IO_ERROR_INVALID_ARGUMENT, "Claim invalide.");
    return FALSE;
  }
  gboolean paused = FALSE;
  gboolean stopped = FALSE;
  if (!local_job_store_controls(store, &paused, &stopped, error))
    return FALSE;
  if (paused || stopped)
    return TRUE;
  if (!exec_sql(store->db, "BEGIN IMMEDIATE;", error))
    return FALSE;
  sqlite3_stmt *s = NULL;
  char *job = NULL;
  guint rank = 0;
  char *plan_id = NULL;
  guint reserved_ms = requested_active_ms;
  const char *select =
      "SELECT j.job_id,j.attempt_count,p.plan_id,p.max_active_ms,p.consumed_active_ms "
      "FROM jobs j LEFT JOIN jobs d ON "
      "d.job_id=j.dependency_job_id LEFT JOIN plan_jobs pj ON pj.job_id=j.job_id "
      "LEFT JOIN plans p ON p.plan_id=pj.plan_id "
      "WHERE j.state IN('QUEUED','RETRY_WAIT') AND j.cancel_requested=0 "
      "AND j.attempt_count<j.max_attempts "
      "AND (j.dependency_job_id IS NULL OR d.state='COMPLETED') "
      "AND (p.plan_id IS NULL OR (p.consumed_attempts<p.max_attempts_total "
      "AND p.consumed_active_ms<p.max_active_ms)) "
      "ORDER BY j.requested_at,j.job_id LIMIT 1;";
  if (sqlite3_prepare_v2(store->db, select, -1, &s, NULL) == SQLITE_OK &&
      sqlite3_step(s) == SQLITE_ROW) {
    job = g_strdup((const char *)sqlite3_column_text(s, 0));
    rank = (guint)sqlite3_column_int(s, 1) + 1U;
    if (sqlite3_column_type(s, 2) != SQLITE_NULL) {
      plan_id = g_strdup((const char *)sqlite3_column_text(s, 2));
      guint64 maximum = (guint64)sqlite3_column_int64(s, 3);
      guint64 consumed = (guint64)sqlite3_column_int64(s, 4);
      guint64 remaining = maximum > consumed ? maximum - consumed : 0U;
      reserved_ms = (guint)MIN((guint64)requested_active_ms, remaining);
    }
  }
  sqlite3_finalize(s);
  if (job == NULL) {
    exec_sql(store->db, "COMMIT;", NULL);
    return TRUE;
  }
  if (reserved_ms == 0U) {
    g_free(job); g_free(plan_id); exec_sql(store->db, "COMMIT;", NULL);
    return TRUE;
  }
  if (plan_id != NULL) {
    sqlite3_stmt *budget = NULL;
    gboolean budget_ok = sqlite3_prepare_v2(store->db,
        "UPDATE plans SET consumed_attempts=consumed_attempts+1,"
        "consumed_active_ms=consumed_active_ms+?2,state='ACTIVE' "
        "WHERE plan_id=?1 AND consumed_attempts<max_attempts_total "
        "AND consumed_active_ms+?2<=max_active_ms;", -1, &budget, NULL)==SQLITE_OK &&
        bind_text(budget,1,plan_id) && sqlite3_bind_int64(budget,2,reserved_ms)==SQLITE_OK &&
        sqlite3_step(budget)==SQLITE_DONE && sqlite3_changes(store->db)==1;
    sqlite3_finalize(budget);
    if (!budget_ok) { exec_sql(store->db,"ROLLBACK;",NULL);g_free(job);g_free(plan_id);return TRUE; }
  }
  sqlite3_stmt *u = NULL;
  gboolean ok =
      sqlite3_prepare_v2(store->db,
                         "UPDATE jobs SET "
                         "state='RUNNING',attempt_count=?2,owner_token=?3,"
                         "heartbeat_at=?4,revision=revision+1 WHERE job_id=?1 "
                         "AND state IN('QUEUED','RETRY_WAIT');",
                         -1, &u, NULL) == SQLITE_OK &&
      bind_text(u, 1, job) && sqlite3_bind_int(u, 2, (int)rank) == SQLITE_OK &&
      bind_text(u, 3, owner) && bind_text(u, 4, now) &&
      sqlite3_step(u) == SQLITE_DONE && sqlite3_changes(store->db) == 1;
  sqlite3_finalize(u);
  sqlite3_stmt *a = NULL;
  ok = ok &&
       sqlite3_prepare_v2(store->db,
                          "INSERT INTO attempts(attempt_id,job_id,rank,owner_token,started_at,heartbeat_at,finished_at,state,diagnostic,reconciliation,reserved_active_ms,elapsed_active_ms,budget_finalized) "
                          "VALUES(?1,?2,?3,?4,?5,?5,NULL,'RUNNING',NULL,NULL,?6,NULL,0);",
                          -1, &a, NULL) == SQLITE_OK &&
       bind_text(a, 1, attempt) && bind_text(a, 2, job) &&
       sqlite3_bind_int(a, 3, (int)rank) == SQLITE_OK &&
       bind_text(a, 4, owner) && bind_text(a, 5, now) &&
       sqlite3_bind_int64(a, 6, reserved_ms) == SQLITE_OK &&
       sqlite3_step(a) == SQLITE_DONE;
  sqlite3_finalize(a);
  ok = ok && transition(store->db, job, "QUEUED", "RUNNING", "claimed", now) &&
       exec_sql(store->db, "COMMIT;", error);
  if (!ok)
    exec_sql(store->db, "ROLLBACK;", NULL);
  if (ok)
    *out = local_job_store_find(store, job, error);
  if (ok) *out_reserved_active_ms = reserved_ms;
  g_free(job); g_free(plan_id);
  return ok;
}

gboolean local_job_store_claim_next(LocalJobStore *store, const char *owner,
                                    const char *attempt, const char *now,
                                    LocalJobRecord **out, GError **error) {
  guint ignored = 0;
  return local_job_store_claim_next_budgeted(store, owner, attempt, now,
      G_MAXUINT, &ignored, out, error);
}

gboolean local_job_store_heartbeat(LocalJobStore *s, const char *job,
                                   const char *owner, const char *now,
                                   GError **error) {
  sqlite3_stmt *q = NULL;
  gboolean ok =
      s &&
      sqlite3_prepare_v2(s->db,
                         "UPDATE jobs SET heartbeat_at=?3 WHERE job_id=?1 AND "
                         "state='RUNNING' AND owner_token=?2;",
                         -1, &q, NULL) == SQLITE_OK &&
      bind_text(q, 1, job) && bind_text(q, 2, owner) && bind_text(q, 3, now) &&
      sqlite3_step(q) == SQLITE_DONE && sqlite3_changes(s->db) == 1;
  sqlite3_finalize(q);
  if (!ok)
    job_error(error, G_IO_ERROR_PERMISSION_DENIED,
              "Heartbeat refusé : propriétaire obsolète.");
  return ok;
}

gboolean local_job_store_finish_budgeted(LocalJobStore *s, const char *job,
                                const char *owner, LocalJobState state,
                                const char *reason, const char *result,
                                const char *now, guint elapsed_active_ms,
                                GError **error) {
  if (state != LOCAL_JOB_COMPLETED && state != LOCAL_JOB_FAILED &&
      state != LOCAL_JOB_CANCELLED && state != LOCAL_JOB_RECOVERY_REQUIRED &&
      state != LOCAL_JOB_BLOCKED) {
    job_error(error, G_IO_ERROR_INVALID_ARGUMENT, "État terminal invalide.");
    return FALSE;
  }
  if (!exec_sql(s->db, "BEGIN IMMEDIATE;", error))
    return FALSE;
  sqlite3_stmt *budget_read = NULL;
  guint64 reserved_ms = 0; char *plan_id = NULL;
  if (sqlite3_prepare_v2(s->db,
      "SELECT a.reserved_active_ms,pj.plan_id FROM attempts a "
      "LEFT JOIN plan_jobs pj ON pj.job_id=a.job_id WHERE a.job_id=?1 "
      "AND a.owner_token=?2 AND a.state='RUNNING';",-1,&budget_read,NULL)==SQLITE_OK &&
      bind_text(budget_read,1,job)&&bind_text(budget_read,2,owner)&&
      sqlite3_step(budget_read)==SQLITE_ROW) {
    reserved_ms=(guint64)sqlite3_column_int64(budget_read,0);
    if(sqlite3_column_type(budget_read,1)!=SQLITE_NULL)
      plan_id=g_strdup((const char*)sqlite3_column_text(budget_read,1));
  }
  sqlite3_finalize(budget_read);
  guint64 charged_ms=MIN((guint64)elapsed_active_ms,reserved_ms);
  sqlite3_stmt *q = NULL;
  gboolean ok =
      sqlite3_prepare_v2(s->db,
                         "UPDATE jobs SET "
                         "state=?3,owner_token=NULL,heartbeat_at=NULL,result_"
                         "status=?4,diagnostic=?5,revision=revision+1 WHERE "
                         "job_id=?1 AND state='RUNNING' AND owner_token=?2;",
                         -1, &q, NULL) == SQLITE_OK &&
      bind_text(q, 1, job) && bind_text(q, 2, owner) &&
      bind_text(q, 3, local_job_state_code(state)) &&
      (result ? bind_text(q, 4, result)
              : sqlite3_bind_null(q, 4) == SQLITE_OK) &&
      (reason ? bind_text(q, 5, reason)
              : sqlite3_bind_null(q, 5) == SQLITE_OK) &&
      sqlite3_step(q) == SQLITE_DONE && sqlite3_changes(s->db) == 1;
  sqlite3_finalize(q);
  sqlite3_stmt *a = NULL;
  ok = ok &&
       sqlite3_prepare_v2(
           s->db,
           "UPDATE attempts SET "
           "state=?3,finished_at=?4,diagnostic=?5,reconciliation=?6,"
           "elapsed_active_ms=?7,budget_finalized=1 WHERE "
           "job_id=?1 AND owner_token=?2 AND state='RUNNING';",
           -1, &a, NULL) == SQLITE_OK &&
       bind_text(a, 1, job) && bind_text(a, 2, owner) &&
       bind_text(a, 3, local_job_state_code(state)) && bind_text(a, 4, now) &&
       (reason ? bind_text(a, 5, reason)
               : sqlite3_bind_null(a, 5) == SQLITE_OK) &&
       (result ? bind_text(a, 6, result)
               : sqlite3_bind_null(a, 6) == SQLITE_OK) &&
       sqlite3_bind_int64(a, 7, (sqlite3_int64)charged_ms) == SQLITE_OK &&
       sqlite3_step(a) == SQLITE_DONE;
  sqlite3_finalize(a);
  if (ok && plan_id != NULL && reserved_ms > charged_ms) {
    sqlite3_stmt *release = NULL;
    ok = sqlite3_prepare_v2(s->db,
        "UPDATE plans SET consumed_active_ms=consumed_active_ms-?2 "
        "WHERE plan_id=?1 AND consumed_active_ms>=?2;",-1,&release,NULL)==SQLITE_OK &&
        bind_text(release,1,plan_id) &&
        sqlite3_bind_int64(release,2,(sqlite3_int64)(reserved_ms-charged_ms))==SQLITE_OK &&
        sqlite3_step(release)==SQLITE_DONE && sqlite3_changes(s->db)==1;
    sqlite3_finalize(release);
  }
  ok = ok &&
       transition(s->db, job, "RUNNING", local_job_state_code(state),
                  reason ? reason : "finished", now) &&
       exec_sql(s->db, "COMMIT;", error);
  if (!ok) {
    exec_sql(s->db, "ROLLBACK;", NULL);
    job_error(error, G_IO_ERROR_PERMISSION_DENIED,
              "Fin refusée : token obsolète.");
  }
  g_free(plan_id);
  return ok;
}

gboolean local_job_store_finish(LocalJobStore *s, const char *job,
                                const char *owner, LocalJobState state,
                                const char *reason, const char *result,
                                const char *now, GError **error) {
  return local_job_store_finish_budgeted(s,job,owner,state,reason,result,now,
      G_MAXUINT,error);
}

gboolean local_job_store_requeue_interrupted(LocalJobStore *s, const char *job,
                                             const char *reason,
                                             const char *now, GError **error) {
  if (!exec_sql(s->db, "BEGIN IMMEDIATE;", error))
    return FALSE;
  sqlite3_stmt *q = NULL;
  gboolean ok =
      sqlite3_prepare_v2(s->db,
                         "UPDATE jobs SET "
                         "state='QUEUED',owner_token=NULL,heartbeat_at=NULL,"
                         "diagnostic=?2,revision=revision+1 WHERE job_id=?1 "
                         "AND state='RUNNING' AND attempt_count<max_attempts;",
                         -1, &q, NULL) == SQLITE_OK &&
      bind_text(q, 1, job) && bind_text(q, 2, reason) &&
      sqlite3_step(q) == SQLITE_DONE && sqlite3_changes(s->db) == 1;
  sqlite3_finalize(q);
  sqlite3_stmt *attempt = NULL;
  ok = ok &&
       sqlite3_prepare_v2(
           s->db,
           "UPDATE attempts SET state='INTERRUPTED',finished_at=?2,"
           "diagnostic=?3 WHERE job_id=?1 AND state='RUNNING';",
           -1, &attempt, NULL) == SQLITE_OK &&
       bind_text(attempt, 1, job) && bind_text(attempt, 2, now) &&
       bind_text(attempt, 3, reason) && sqlite3_step(attempt) == SQLITE_DONE;
  sqlite3_finalize(attempt);
  ok = ok && transition(s->db, job, "RUNNING", "QUEUED", reason, now) &&
       exec_sql(s->db, "COMMIT;", error);
  if (!ok)
    exec_sql(s->db, "ROLLBACK;", NULL);
  return ok;
}

gboolean
local_job_store_reconcile_interrupted(LocalJobStore *store, const char *job_id,
                                      LocalJobState state, const char *reason,
                                      const char *result_status,
                                      const char *now, GError **error) {
  if (store == NULL || job_id == NULL || reason == NULL || now == NULL ||
      (state != LOCAL_JOB_COMPLETED && state != LOCAL_JOB_RECOVERY_REQUIRED &&
       state != LOCAL_JOB_FAILED && state != LOCAL_JOB_CANCELLED)) {
    job_error(error, G_IO_ERROR_INVALID_ARGUMENT,
              "Réconciliation interrompue invalide.");
    return FALSE;
  }
  if (!exec_sql(store->db, "BEGIN IMMEDIATE;", error))
    return FALSE;
  sqlite3_stmt *job = NULL;
  gboolean ok =
      sqlite3_prepare_v2(
          store->db,
          "UPDATE jobs SET state=?2,owner_token=NULL,heartbeat_at=NULL,"
          "result_status=?3,diagnostic=?4,revision=revision+1 "
          "WHERE job_id=?1 AND state='RUNNING';",
          -1, &job, NULL) == SQLITE_OK &&
      bind_text(job, 1, job_id) &&
      bind_text(job, 2, local_job_state_code(state)) &&
      (result_status ? bind_text(job, 3, result_status)
                     : sqlite3_bind_null(job, 3) == SQLITE_OK) &&
      bind_text(job, 4, reason) && sqlite3_step(job) == SQLITE_DONE &&
      sqlite3_changes(store->db) == 1;
  sqlite3_finalize(job);
  sqlite3_stmt *attempt = NULL;
  ok = ok &&
       sqlite3_prepare_v2(
           store->db,
           "UPDATE attempts SET state=?2,finished_at=?3,diagnostic=?4,"
           "reconciliation=?5 WHERE job_id=?1 AND state='RUNNING';",
           -1, &attempt, NULL) == SQLITE_OK &&
       bind_text(attempt, 1, job_id) &&
       bind_text(attempt, 2, local_job_state_code(state)) &&
       bind_text(attempt, 3, now) && bind_text(attempt, 4, reason) &&
       (result_status ? bind_text(attempt, 5, result_status)
                      : sqlite3_bind_null(attempt, 5) == SQLITE_OK) &&
       sqlite3_step(attempt) == SQLITE_DONE;
  sqlite3_finalize(attempt);
  ok = ok &&
       transition(store->db, job_id, "RUNNING", local_job_state_code(state),
                  reason, now) &&
       exec_sql(store->db, "COMMIT;", error);
  if (!ok) {
    exec_sql(store->db, "ROLLBACK;", NULL);
    job_error(error, G_IO_ERROR_FAILED,
              "Réconciliation de la tentative interrompue refusée.");
  }
  return ok;
}

gboolean local_job_store_request_cancel(LocalJobStore *s, const char *job,
                                        const char *now, GError **error) {
  LocalJobRecord *before =
      s != NULL ? local_job_store_find(s, job, error) : NULL;
  if (before == NULL)
    return FALSE;
  sqlite3_stmt *q = NULL;
  gboolean ok =
      s &&
      sqlite3_prepare_v2(
          s->db,
          "UPDATE jobs SET cancel_requested=1,state=CASE WHEN state "
          "IN('QUEUED','RETRY_WAIT','BLOCKED') THEN 'CANCELLED' ELSE state "
          "END,diagnostic='cancel_requested',revision=revision+1 WHERE "
          "job_id=?1 AND state NOT "
          "IN('COMPLETED','FAILED','CANCELLED','RECOVERY_REQUIRED');",
          -1, &q, NULL) == SQLITE_OK &&
      bind_text(q, 1, job) && sqlite3_step(q) == SQLITE_DONE &&
      sqlite3_changes(s->db) == 1;
  sqlite3_finalize(q);
  if (ok) {
    const char *from = local_job_state_code(before->state);
    const char *to =
        before->state == LOCAL_JOB_RUNNING ? "RUNNING" : "CANCELLED";
    ok = transition(s->db, job, from, to, "cancel_requested", now);
  } else
    job_error(error, G_IO_ERROR_FAILED, "Annulation refusée.");
  local_job_record_free(before);
  return ok;
}

static gboolean control_update(LocalJobStore *s, const char *column,
                               const char *now, GError **error) {
  char *sql = g_strdup_printf(
      "UPDATE controls SET %s=1,updated_at=?1 WHERE singleton=1;", column);
  sqlite3_stmt *q = NULL;
  gboolean ok = sqlite3_prepare_v2(s->db, sql, -1, &q, NULL) == SQLITE_OK &&
                bind_text(q, 1, now) && sqlite3_step(q) == SQLITE_DONE;
  sqlite3_finalize(q);
  g_free(sql);
  if (!ok)
    job_error(error, G_IO_ERROR_FAILED, "Contrôle local non persisté.");
  return ok;
}
gboolean local_job_store_set_paused(LocalJobStore *s, gboolean paused,
                                    const char *now, GError **error) {
  sqlite3_stmt *q = NULL;
  gboolean ok =
      sqlite3_prepare_v2(
          s->db,
          "UPDATE controls SET paused=?1,updated_at=?2 WHERE singleton=1;", -1,
          &q, NULL) == SQLITE_OK &&
      sqlite3_bind_int(q, 1, paused) == SQLITE_OK && bind_text(q, 2, now) &&
      sqlite3_step(q) == SQLITE_DONE;
  sqlite3_finalize(q);
  if (!ok)
    job_error(error, G_IO_ERROR_FAILED, "Pause non persistée.");
  return ok;
}
gboolean local_job_store_request_stop(LocalJobStore *s, const char *now,
                                      GError **error) {
  return control_update(s, "stop_requested", now, error);
}
gboolean local_job_store_set_stopped(LocalJobStore *s, gboolean stopped,
                                     const char *now, GError **error) {
  sqlite3_stmt *q = NULL;
  gboolean ok =
      s != NULL && now != NULL &&
      sqlite3_prepare_v2(s->db,
                         "UPDATE controls SET stop_requested=?1,updated_at=?2 "
                         "WHERE singleton=1;",
                         -1, &q, NULL) == SQLITE_OK &&
      sqlite3_bind_int(q, 1, stopped) == SQLITE_OK && bind_text(q, 2, now) &&
      sqlite3_step(q) == SQLITE_DONE;
  sqlite3_finalize(q);
  if (!ok)
    job_error(error, G_IO_ERROR_FAILED, "État d'arrêt local non persisté.");
  return ok;
}
gboolean local_job_store_controls(LocalJobStore *s, gboolean *p, gboolean *stop,
                                  GError **error) {
  sqlite3_stmt *q = NULL;
  gboolean ok =
      s && p && stop &&
      sqlite3_prepare_v2(
          s->db,
          "SELECT paused,stop_requested FROM controls WHERE singleton=1;", -1,
          &q, NULL) == SQLITE_OK &&
      sqlite3_step(q) == SQLITE_ROW;
  if (ok) {
    *p = sqlite3_column_int(q, 0) != 0;
    *stop = sqlite3_column_int(q, 1) != 0;
  } else
    job_error(error, G_IO_ERROR_FAILED, "Contrôles JobStore illisibles.");
  sqlite3_finalize(q);
  return ok;
}

gboolean local_job_store_integrity(LocalJobStore *s, GError **error) {
  sqlite3_stmt *q = NULL;
  gboolean ok = sqlite3_prepare_v2(s->db, "PRAGMA integrity_check;", -1, &q,
                                   NULL) == SQLITE_OK &&
                sqlite3_step(q) == SQLITE_ROW &&
                g_strcmp0((const char *)sqlite3_column_text(q, 0), "ok") == 0;
  sqlite3_finalize(q);
  q = NULL;
  ok = ok &&
       sqlite3_prepare_v2(s->db, "PRAGMA foreign_key_check;", -1, &q, NULL) ==
           SQLITE_OK &&
       sqlite3_step(q) == SQLITE_DONE;
  sqlite3_finalize(q);
  if (!ok)
    job_error(error, G_IO_ERROR_INVALID_DATA, "Intégrité JobStore invalide.");
  return ok;
}

gboolean local_job_store_export_atomic(LocalJobStore *s, const char *path,
                                       GError **error) {
  GPtrArray *jobs = local_job_store_list(s, error);
  if (!jobs)
    return FALSE;
  JsonBuilder *b = json_builder_new();
  JsonGenerator *g = json_generator_new();
  json_builder_begin_object(b);
  json_builder_set_member_name(b, "contract");
  json_builder_add_string_value(b, "labfy.local_jobs.snapshot.v1");
  json_builder_set_member_name(b, "investigation_id");
  json_builder_add_string_value(b, s->investigation_id);
  json_builder_set_member_name(b, "jobs");
  json_builder_begin_array(b);
  for (guint i = 0; i < jobs->len; i++) {
    LocalJobRecord *r = g_ptr_array_index(jobs, i);
    json_builder_begin_object(b);
#define JS(n, v)                                                               \
  json_builder_set_member_name(b, n);                                          \
  json_builder_add_string_value(b, (v) ? (v) : "")
    JS("job_id", r->job_id);
    JS("request_id", r->request_id);
    JS("source_evidence_id", r->source_evidence_id);
    JS("derivative_evidence_id", r->derivative_evidence_id);
    JS("capability_id", r->capability_id);
    JS("state", local_job_state_code(r->state));
    JS("result_status", r->result_status);
    JS("diagnostic", r->diagnostic);
    json_builder_set_member_name(b, "attempt_count");
    json_builder_add_int_value(b, r->attempt_count);
    json_builder_set_member_name(b, "cancel_requested");
    json_builder_add_boolean_value(b, r->cancel_requested);
    json_builder_set_member_name(b, "revision");
    json_builder_add_int_value(b, (gint64)r->revision);
    json_builder_end_object(b);
#undef JS
  }
  json_builder_end_array(b);
  gboolean paused = FALSE, stopped = FALSE;
  (void)local_job_store_controls(s, &paused, &stopped, NULL);
  json_builder_set_member_name(b, "plans");
  json_builder_begin_array(b);
  sqlite3_stmt *plans = NULL;
  const char *plan_query =
      "SELECT p.plan_id,p.idempotency_key,p.profile_id,p.input_revision,p.created_at,"
      "p.max_analyses,p.max_attempts_total,p.max_source_bytes,p.max_active_ms,"
      "p.reserved_analyses,p.reserved_attempts,p.reserved_source_bytes,"
      "p.consumed_attempts,p.consumed_active_ms,"
      "sum(CASE WHEN j.state='RUNNING' THEN 1 ELSE 0 END),"
      "sum(CASE WHEN j.state IN('QUEUED','RETRY_WAIT') THEN 1 ELSE 0 END),"
      "sum(CASE WHEN j.state='COMPLETED' THEN 1 ELSE 0 END),"
      "sum(CASE WHEN j.state IN('FAILED','BLOCKED') THEN 1 ELSE 0 END),"
      "sum(CASE WHEN j.state='CANCELLED' THEN 1 ELSE 0 END),"
      "sum(CASE WHEN j.state='RECOVERY_REQUIRED' THEN 1 ELSE 0 END) "
      "FROM plans p JOIN plan_jobs pj ON pj.plan_id=p.plan_id "
      "JOIN jobs j ON j.job_id=pj.job_id GROUP BY p.plan_id ORDER BY p.created_at,p.plan_id;";
  if (sqlite3_prepare_v2(s->db, plan_query, -1, &plans, NULL) != SQLITE_OK) {
    json_node_unref(json_builder_get_root(b)); g_object_unref(g);g_object_unref(b);
    g_ptr_array_unref(jobs); job_error(error,G_IO_ERROR_FAILED,"Export des plans impossible.");return FALSE;
  }
  while (sqlite3_step(plans) == SQLITE_ROW) {
    guint running=sqlite3_column_int(plans,14), pending=sqlite3_column_int(plans,15);
    guint completed=sqlite3_column_int(plans,16), failed=sqlite3_column_int(plans,17);
    guint cancelled=sqlite3_column_int(plans,18), recovery=sqlite3_column_int(plans,19);
    guint total=completed+failed+cancelled+recovery+running+pending;
    guint64 max_attempts=(guint64)sqlite3_column_int64(plans,6);
    guint64 max_ms=(guint64)sqlite3_column_int64(plans,8);
    guint64 used_attempts=(guint64)sqlite3_column_int64(plans,12);
    guint64 used_ms=(guint64)sqlite3_column_int64(plans,13);
    const char *state = recovery?"RECOVERY_REQUIRED":running?"ACTIVE":
      paused&&pending?"PAUSED":(pending&&(used_attempts>=max_attempts||used_ms>=max_ms))?"LIMIT_REACHED":
      completed==total?"COMPLETED_SUCCESS":failed?"PROCESSED_WITH_FAILURES":
      (cancelled&&completed+cancelled==total)||stopped?"STOPPED":"PENDING";
    json_builder_begin_object(b);
#define PJS(n,c) json_builder_set_member_name(b,n);json_builder_add_string_value(b,(const char*)sqlite3_column_text(plans,c))
    PJS("plan_id",0);PJS("idempotency_key",1);PJS("profile_id",2);PJS("input_revision",3);PJS("created_at",4);
#undef PJS
    json_builder_set_member_name(b,"state");json_builder_add_string_value(b,state);
#define PJI(n,c) json_builder_set_member_name(b,n);json_builder_add_int_value(b,sqlite3_column_int64(plans,c))
    PJI("max_analyses",5);PJI("max_attempts_total",6);PJI("max_source_bytes",7);PJI("max_active_ms",8);
    PJI("reserved_analyses",9);PJI("reserved_attempts",10);PJI("reserved_source_bytes",11);
    PJI("consumed_attempts",12);PJI("consumed_active_ms",13);
#undef PJI
    json_builder_set_member_name(b,"remaining_attempts");json_builder_add_int_value(b,max_attempts>used_attempts?max_attempts-used_attempts:0);
    json_builder_set_member_name(b,"remaining_active_ms");json_builder_add_int_value(b,max_ms>used_ms?max_ms-used_ms:0);
    json_builder_set_member_name(b,"running_jobs");json_builder_add_int_value(b,running);
    json_builder_set_member_name(b,"pending_jobs");json_builder_add_int_value(b,pending);
    json_builder_set_member_name(b,"completed_jobs");json_builder_add_int_value(b,completed);
    json_builder_set_member_name(b,"failed_jobs");json_builder_add_int_value(b,failed);
    json_builder_set_member_name(b,"cancelled_jobs");json_builder_add_int_value(b,cancelled);
    json_builder_set_member_name(b,"recovery_jobs");json_builder_add_int_value(b,recovery);
    json_builder_end_object(b);
  }
  sqlite3_finalize(plans);
  json_builder_end_array(b);
  json_builder_end_object(b);
  JsonNode *root = json_builder_get_root(b);
  json_generator_set_root(g, root);
  char *data = json_generator_to_data(g, NULL);
  /* INVARIANT: deux exports concurrents ne partagent jamais un temporaire ;
   * seul le renommage final remplace la vue, jamais la publication métier. */
  char *suffix = g_uuid_string_random();
  char *stage = g_strdup_printf("%s.stage-%s", path, suffix);
  gboolean ok = data && g_file_set_contents(stage, data, -1, error) &&
                g_chmod(stage, 0600) == 0 && g_rename(stage, path) == 0;
  if (!ok)
    (void)g_remove(stage);
  g_free(stage);
  g_free(suffix);
  g_free(data);
  json_node_unref(root);
  g_object_unref(g);
  g_object_unref(b);
  g_ptr_array_unref(jobs);
  return ok;
}
