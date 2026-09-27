#include "core/local_job_store.h"

#include <glib/gstdio.h>
#include <json-glib/json-glib.h>
#include <sqlite3.h>

#define INVESTIGATION_ID "81000000-0000-4000-8000-000000000001"
#define JOB_ID "81000000-0000-4000-8000-000000000011"
#define REQUEST_ID "81000000-0000-4000-8000-000000000012"
#define SOURCE_ID "81000000-0000-4000-8000-000000000013"
#define DERIVATIVE_ID "81000000-0000-4000-8000-000000000014"
#define OWNER_ID "81000000-0000-4000-8000-000000000015"
#define ATTEMPT_ID "81000000-0000-4000-8000-000000000016"
#define PLAN_ID "81000000-0000-4000-8000-000000000021"

static const char *historical_schema =
    "CREATE TABLE metadata(key TEXT PRIMARY KEY,value TEXT NOT NULL);"
    "CREATE TABLE controls(singleton INTEGER PRIMARY KEY CHECK(singleton=1),paused INTEGER NOT NULL,stop_requested INTEGER NOT NULL,updated_at TEXT NOT NULL);"
    "CREATE TABLE jobs(job_id TEXT PRIMARY KEY,request_id TEXT NOT NULL UNIQUE,source_evidence_id TEXT NOT NULL,derivative_evidence_id TEXT NOT NULL UNIQUE,capability_id TEXT NOT NULL,adapter_id TEXT NOT NULL,adapter_version TEXT NOT NULL,source_sha256 TEXT NOT NULL,source_size INTEGER NOT NULL,requested_at TEXT NOT NULL,parameters_json TEXT NOT NULL,state TEXT NOT NULL,cancel_requested INTEGER NOT NULL DEFAULT 0,max_attempts INTEGER NOT NULL,attempt_count INTEGER NOT NULL DEFAULT 0,owner_token TEXT,heartbeat_at TEXT,result_status TEXT,diagnostic TEXT,dependency_job_id TEXT REFERENCES jobs(job_id),revision INTEGER NOT NULL DEFAULT 1);"
    "CREATE TABLE attempts(attempt_id TEXT PRIMARY KEY,job_id TEXT NOT NULL REFERENCES jobs(job_id),rank INTEGER NOT NULL,owner_token TEXT NOT NULL,started_at TEXT NOT NULL,heartbeat_at TEXT NOT NULL,finished_at TEXT,state TEXT NOT NULL,diagnostic TEXT,reconciliation TEXT,UNIQUE(job_id,rank));"
    "CREATE TABLE transitions(transition_id INTEGER PRIMARY KEY AUTOINCREMENT,job_id TEXT NOT NULL REFERENCES jobs(job_id),from_state TEXT,to_state TEXT NOT NULL,reason TEXT NOT NULL,created_at TEXT NOT NULL);"
    "CREATE INDEX idx_jobs_state ON jobs(state,requested_at,job_id);";

static void sql_ok(sqlite3 *db, const char *sql) {
  char *message = NULL;
  g_assert_cmpint(sqlite3_exec(db, sql, NULL, NULL, &message), ==, SQLITE_OK);
  sqlite3_free(message);
}

static char *make_historical_fixture(guint version) {
  GError *error = NULL;
  char *directory = g_dir_make_tmp("labfy-jobs-migration-XXXXXX", &error);
  g_assert_no_error(error);
  char *path = g_build_filename(directory, "jobs.sqlite", NULL);
  sqlite3 *db = NULL;
  g_assert_cmpint(sqlite3_open(path, &db), ==, SQLITE_OK);
  sql_ok(db, historical_schema);
  char *metadata = sqlite3_mprintf(
      "INSERT INTO metadata VALUES('schema_version','%u');"
      "INSERT INTO metadata VALUES('investigation_id','%q');"
      "INSERT INTO metadata VALUES('revision','7');", version, INVESTIGATION_ID);
  sql_ok(db, metadata); sqlite3_free(metadata);
  sql_ok(db,
      "INSERT INTO controls VALUES(1,1,0,'2026-09-25T00:00:00Z');"
      "INSERT INTO jobs VALUES('81000000-0000-4000-8000-000000000011','81000000-0000-4000-8000-000000000012','81000000-0000-4000-8000-000000000013','81000000-0000-4000-8000-000000000014','local.eml.headers','labfy.eml_analyzer','1','0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef',42,'2026-09-25T00:00:00Z','{}','COMPLETED',0,3,1,NULL,NULL,'published','historical',NULL,4);"
      "INSERT INTO attempts VALUES('81000000-0000-4000-8000-000000000016','81000000-0000-4000-8000-000000000011',1,'81000000-0000-4000-8000-000000000015','2026-09-25T00:00:01Z','2026-09-25T00:00:01Z','2026-09-25T00:00:02Z','COMPLETED','historical','published');"
      "INSERT INTO transitions(job_id,from_state,to_state,reason,created_at) VALUES('81000000-0000-4000-8000-000000000011','RUNNING','COMPLETED','published','2026-09-25T00:00:02Z');");
  if (version >= 2) {
    sql_ok(db,
      "CREATE TABLE plans(plan_id TEXT PRIMARY KEY,idempotency_key TEXT NOT NULL UNIQUE,intent_hash TEXT NOT NULL,input_revision TEXT NOT NULL,profile_id TEXT NOT NULL,created_at TEXT NOT NULL,state TEXT NOT NULL DEFAULT 'APPROVED',max_analyses INTEGER NOT NULL,max_attempts_total INTEGER NOT NULL,max_source_bytes INTEGER NOT NULL,max_active_ms INTEGER NOT NULL,reserved_analyses INTEGER NOT NULL,reserved_attempts INTEGER NOT NULL,reserved_source_bytes INTEGER NOT NULL,consumed_attempts INTEGER NOT NULL DEFAULT 0,consumed_active_ms INTEGER NOT NULL DEFAULT 0);"
      "CREATE TABLE plan_jobs(plan_id TEXT NOT NULL REFERENCES plans(plan_id),job_id TEXT NOT NULL UNIQUE REFERENCES jobs(job_id),rank INTEGER NOT NULL,PRIMARY KEY(plan_id,rank));"
      "INSERT INTO plans VALUES('81000000-0000-4000-8000-000000000021','historical-key','historical-intent','revision-7','LOCAL_PRUDENT','2026-09-25T00:00:00Z','APPROVED',1,3,42,90000,1,3,42,1,1200);"
      "INSERT INTO plan_jobs VALUES('81000000-0000-4000-8000-000000000021','81000000-0000-4000-8000-000000000011',0);");
  }
  if (version >= 3) {
    sql_ok(db,
      "ALTER TABLE attempts ADD COLUMN reserved_active_ms INTEGER NOT NULL DEFAULT 0 CHECK(reserved_active_ms>=0);"
      "ALTER TABLE attempts ADD COLUMN elapsed_active_ms INTEGER CHECK(elapsed_active_ms IS NULL OR elapsed_active_ms>=0);"
      "ALTER TABLE attempts ADD COLUMN budget_finalized INTEGER NOT NULL DEFAULT 0 CHECK(budget_finalized IN(0,1));"
      "CREATE UNIQUE INDEX idx_jobs_active_analysis ON jobs(source_evidence_id,capability_id) WHERE state IN('QUEUED','RUNNING','RETRY_WAIT');");
  }
  sqlite3_close(db);
  g_free(directory);
  return path;
}

static gint64 scalar(sqlite3 *db, const char *sql) {
  sqlite3_stmt *statement = NULL; gint64 value = -1;
  g_assert_cmpint(sqlite3_prepare_v2(db, sql, -1, &statement, NULL), ==, SQLITE_OK);
  g_assert_cmpint(sqlite3_step(statement), ==, SQLITE_ROW);
  value = sqlite3_column_int64(statement, 0); sqlite3_finalize(statement);
  return value;
}

static void remove_fixture(char *path) {
  char *directory = g_path_get_dirname(path);
  g_remove(path); g_rmdir(directory); g_free(directory); g_free(path);
}

static void assert_historical_rows(sqlite3 *db, guint version) {
  g_assert_cmpint(scalar(db, "SELECT count(*) FROM jobs WHERE diagnostic='historical' AND revision=4"), ==, 1);
  g_assert_cmpint(scalar(db, "SELECT count(*) FROM attempts WHERE reconciliation='published'"), ==, 1);
  g_assert_cmpint(scalar(db, "SELECT count(*) FROM transitions WHERE reason='published'"), ==, 1);
  g_assert_cmpint(scalar(db, "SELECT paused FROM controls"), ==, 1);
  if (version >= 2)
    g_assert_cmpint(scalar(db, "SELECT count(*) FROM plans JOIN plan_jobs USING(plan_id)"), ==, 1);
}

static void test_historical_migration(gconstpointer data) {
  guint version = GPOINTER_TO_UINT(data); char *path = make_historical_fixture(version);
  GError *error = NULL;
  local_job_store_test_fail_migration_after_schema(TRUE);
  LocalJobStore *store = local_job_store_open(path, INVESTIGATION_ID, FALSE, &error);
  g_assert_null(store); g_assert_error(error, G_IO_ERROR, G_IO_ERROR_FAILED); g_clear_error(&error);
  sqlite3 *db = NULL; g_assert_cmpint(sqlite3_open(path, &db), ==, SQLITE_OK);
  g_assert_cmpint(scalar(db, "SELECT CAST(value AS INTEGER) FROM metadata WHERE key='schema_version'"), ==, version);
  g_assert_cmpint(scalar(db, "SELECT count(*) FROM sqlite_master WHERE name='research_plans'"), ==, 0);
  assert_historical_rows(db, version); sqlite3_close(db);
  local_job_store_test_fail_migration_after_schema(FALSE);
  store = local_job_store_open(path, INVESTIGATION_ID, FALSE, &error);
  g_assert_no_error(error); g_assert_nonnull(store); g_assert_true(local_job_store_integrity(store, &error));
  local_job_store_close(store);
  store = local_job_store_open(path, INVESTIGATION_ID, FALSE, &error);
  g_assert_no_error(error); g_assert_nonnull(store); local_job_store_close(store);
  g_assert_cmpint(sqlite3_open(path, &db), ==, SQLITE_OK);
  g_assert_cmpint(scalar(db, "SELECT CAST(value AS INTEGER) FROM metadata WHERE key='schema_version'"), ==, 4);
  g_assert_cmpint(scalar(db, "SELECT count(*) FROM pragma_table_info('attempts') WHERE name IN('reserved_active_ms','elapsed_active_ms','budget_finalized')"), ==, 3);
  g_assert_cmpint(scalar(db, "SELECT count(*) FROM sqlite_master WHERE type='table' AND name IN('research_plans','research_seeds','research_actions','research_action_decisions','scope_grants','scope_grant_actions','scope_grant_exclusions','research_campaigns','research_results','research_receipts')"), ==, 10);
  assert_historical_rows(db, version);
  g_assert_cmpint(scalar(db, "PRAGMA integrity_check"), ==, 0); /* text coerces to zero */
  g_assert_cmpint(scalar(db, "SELECT count(*) FROM pragma_foreign_key_check"), ==, 0);
  sqlite3_close(db); remove_fixture(path);
}

static void test_read_only_contract(void) {
  char *path = make_historical_fixture(3); GError *error = NULL;
  LocalJobStore *store = local_job_store_open(path, INVESTIGATION_ID, TRUE, &error);
  g_assert_no_error(error); g_assert_nonnull(store); local_job_store_close(store);
  sqlite3 *db = NULL; g_assert_cmpint(sqlite3_open(path, &db), ==, SQLITE_OK);
  g_assert_cmpint(scalar(db, "SELECT CAST(value AS INTEGER) FROM metadata WHERE key='schema_version'"), ==, 3);
  g_assert_cmpint(scalar(db, "SELECT count(*) FROM sqlite_master WHERE name='research_plans'"), ==, 0);
  sqlite3_close(db); remove_fixture(path);
  char *directory = g_dir_make_tmp("labfy-jobs-absent-XXXXXX", &error);
  char *absent = g_build_filename(directory, "absent.sqlite", NULL);
  store = local_job_store_open(absent, INVESTIGATION_ID, TRUE, &error);
  g_assert_null(store); g_assert_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
  g_clear_error(&error); g_assert_false(g_file_test(absent, G_FILE_TEST_EXISTS));
  g_free(absent); g_rmdir(directory); g_free(directory);
}

typedef struct {
  char *directory;
  char *path;
  LocalJobStore *store;
} Fixture;

static void setup(Fixture *fixture, gconstpointer unused) {
  (void)unused;
  GError *error = NULL;
  fixture->directory = g_dir_make_tmp("labfy-jobs-XXXXXX", &error);
  g_assert_no_error(error);
  fixture->path = g_build_filename(fixture->directory, "jobs.sqlite", NULL);
  fixture->store =
      local_job_store_create(fixture->path, INVESTIGATION_ID, &error);
  g_assert_no_error(error);
  g_assert_nonnull(fixture->store);
}

static void teardown(Fixture *fixture, gconstpointer unused) {
  (void)unused;
  local_job_store_close(fixture->store);
  g_remove(fixture->path);
  g_rmdir(fixture->directory);
  g_free(fixture->path);
  g_free(fixture->directory);
}

static LocalJobSubmission submission(void) {
  return (LocalJobSubmission){
      .job_id = JOB_ID,
      .request_id = REQUEST_ID,
      .source_evidence_id = SOURCE_ID,
      .derivative_evidence_id = DERIVATIVE_ID,
      .capability_id = "local.eml.headers",
      .adapter_id = "labfy.eml_analyzer",
      .adapter_version = "1",
      .source_sha256 =
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
      .source_size = 42,
      .requested_at = "2026-09-26T20:00:00Z",
      .parameters_json = "{\"mode\":\"headers\"}",
      .max_attempts = 3};
}

static LocalPlanAdmission plan_admission(void) {
  return (LocalPlanAdmission){
    .plan_id="81000000-0000-4000-8000-000000000021",
    .idempotency_key="81000000-0000-4000-8000-000000000022",
    .intent_hash="intent-v1",.input_revision="revision-v1",
    .profile_id="SPECIMEN_SMALL",.created_at="2026-09-26T20:00:00Z",
    .max_analyses=1,.max_attempts_total=2,.max_source_bytes=42,
    .max_active_ms=150};
}

static void test_plan_budget_claim_and_finish(Fixture *fixture,
                                               gconstpointer unused) {
  (void)unused; GError *error=NULL; LocalJobSubmission item=submission();
  item.max_attempts=2;
  LocalPlanAdmission plan=plan_admission(); gboolean reused=TRUE;
  g_assert_true(local_job_store_admit_plan(fixture->store,&plan,&item,1,&reused,&error));
  g_assert_false(reused); guint reserved=0; LocalJobRecord *job=NULL;
  g_assert_true(local_job_store_claim_next_budgeted(fixture->store,OWNER_ID,
      ATTEMPT_ID,"2026-09-26T20:00:01Z",100,&reserved,&job,&error));
  g_assert_cmpuint(reserved,==,100);local_job_record_free(job);
  g_assert_true(local_job_store_finish_budgeted(fixture->store,JOB_ID,OWNER_ID,
      LOCAL_JOB_FAILED,"synthetic_failure","failed","2026-09-26T20:00:02Z",40,&error));
  char *path=g_build_filename(fixture->directory,"snapshot.json",NULL);
  g_assert_true(local_job_store_export_atomic(fixture->store,path,&error));
  JsonParser *parser=json_parser_new();g_assert_true(json_parser_load_from_file(parser,path,&error));
  JsonArray *plans=json_object_get_array_member(json_node_get_object(json_parser_get_root(parser)),"plans");
  JsonObject *p=json_array_get_object_element(plans,0);
  g_assert_cmpint(json_object_get_int_member(p,"consumed_attempts"),==,1);
  g_assert_cmpint(json_object_get_int_member(p,"consumed_active_ms"),==,40);
  g_assert_cmpint(json_object_get_int_member(p,"remaining_active_ms"),==,110);
  g_assert_cmpstr(json_object_get_string_member(p,"state"),==,"PROCESSED_WITH_FAILURES");
  g_object_unref(parser);g_remove(path);g_free(path);
}

static void test_plan_crash_keeps_reservation(Fixture *fixture,
                                               gconstpointer unused) {
  (void)unused;GError *error=NULL;LocalJobSubmission item=submission();
  item.max_attempts=2;
  LocalPlanAdmission plan=plan_admission();
  g_assert_true(local_job_store_admit_plan(fixture->store,&plan,&item,1,NULL,&error));
  guint reserved=0;LocalJobRecord *job=NULL;
  g_assert_true(local_job_store_claim_next_budgeted(fixture->store,OWNER_ID,
      ATTEMPT_ID,"2026-09-26T20:00:01Z",100,&reserved,&job,&error));
  local_job_record_free(job);g_assert_cmpuint(reserved,==,100);
  g_assert_true(local_job_store_requeue_interrupted(fixture->store,JOB_ID,
      "crash_before_publish","2026-09-26T20:00:02Z",&error));
  char *owner=g_uuid_string_random(),*attempt=g_uuid_string_random();
  g_assert_true(local_job_store_claim_next_budgeted(fixture->store,owner,attempt,
      "2026-09-26T20:00:03Z",100,&reserved,&job,&error));
  g_assert_cmpuint(reserved,==,50);g_assert_nonnull(job);local_job_record_free(job);
  g_free(owner);g_free(attempt);
}

static void test_future_version_refused(Fixture *fixture,
                                        gconstpointer unused) {
  (void)unused;local_job_store_close(fixture->store);fixture->store=NULL;
  sqlite3 *db=NULL;g_assert_cmpint(sqlite3_open(fixture->path,&db),==,SQLITE_OK);
  g_assert_cmpint(sqlite3_exec(db,"UPDATE metadata SET value='5' WHERE key='schema_version';",NULL,NULL,NULL),==,SQLITE_OK);sqlite3_close(db);
  GError *error=NULL;fixture->store=local_job_store_open(fixture->path,INVESTIGATION_ID,FALSE,&error);
  g_assert_null(fixture->store);g_assert_error(error,G_IO_ERROR,G_IO_ERROR_INVALID_DATA);g_clear_error(&error);
}

static void test_fresh_v4(Fixture *fixture, gconstpointer unused) {
  (void)unused;
  sqlite3 *db = NULL;
  g_assert_cmpint(sqlite3_open(fixture->path, &db), ==, SQLITE_OK);
  g_assert_cmpint(scalar(db, "SELECT CAST(value AS INTEGER) FROM metadata WHERE key='schema_version'"), ==, 4);
  g_assert_cmpint(scalar(db, "SELECT count(*) FROM sqlite_master WHERE type='table' AND name IN('research_plans','research_seeds','research_actions','research_action_decisions','scope_grants','scope_grant_actions','scope_grant_exclusions','research_campaigns','research_results','research_receipts')"), ==, 10);
  g_assert_cmpint(scalar(db, "SELECT count(*) FROM pragma_foreign_key_check"), ==, 0);
  sqlite3_close(db);

  /* Compatibilité avec une V4 intermédiaire antérieure à l'extension
   * additive des décisions de recherche. */
  local_job_store_close(fixture->store);
  fixture->store = NULL;
  g_assert_cmpint(sqlite3_open(fixture->path, &db), ==, SQLITE_OK);
  g_assert_cmpint(sqlite3_exec(db, "DROP TABLE research_action_decisions;",
                               NULL, NULL, NULL), ==, SQLITE_OK);
  sqlite3_close(db);
  GError *error = NULL;
  fixture->store = local_job_store_open(fixture->path, INVESTIGATION_ID,
                                         FALSE, &error);
  g_assert_no_error(error);
  g_assert_nonnull(fixture->store);
  g_assert_cmpint(sqlite3_open(fixture->path, &db), ==, SQLITE_OK);
  g_assert_cmpint(scalar(db, "SELECT count(*) FROM sqlite_master WHERE type='table' AND name='research_action_decisions'"), ==, 1);
  sqlite3_close(db);
}

static void test_lifecycle_and_stale_owner(Fixture *fixture,
                                           gconstpointer unused) {
  (void)unused;
  GError *error = NULL;
  LocalJobSubmission item = submission();
  gboolean reused = TRUE;
  g_assert_true(
      local_job_store_enqueue(fixture->store, &item, &reused, &error));
  g_assert_no_error(error);
  g_assert_false(reused);
  g_assert_true(
      local_job_store_enqueue(fixture->store, &item, &reused, &error));
  g_assert_true(reused);

  LocalJobRecord *job = NULL;
  g_assert_true(local_job_store_claim_next(fixture->store, OWNER_ID, ATTEMPT_ID,
                                           "2026-09-26T20:00:01Z", &job,
                                           &error));
  g_assert_no_error(error);
  g_assert_nonnull(job);
  g_assert_cmpint(job->state, ==, LOCAL_JOB_RUNNING);
  g_assert_cmpuint(job->attempt_count, ==, 1);
  local_job_record_free(job);

  g_assert_false(local_job_store_finish(
      fixture->store, JOB_ID, "81000000-0000-4000-8000-000000000099",
      LOCAL_JOB_COMPLETED, "stale", "published", "2026-09-26T20:00:02Z",
      &error));
  g_assert_error(error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED);
  g_clear_error(&error);
  g_assert_true(local_job_store_finish(
      fixture->store, JOB_ID, OWNER_ID, LOCAL_JOB_COMPLETED, "published",
      "published", "2026-09-26T20:00:03Z", &error));
  g_assert_no_error(error);
  g_assert_true(local_job_store_integrity(fixture->store, &error));
}

static void test_crash_reconciliation(Fixture *fixture, gconstpointer unused) {
  (void)unused;
  GError *error = NULL;
  LocalJobSubmission item = submission();
  g_assert_true(local_job_store_enqueue(fixture->store, &item, NULL, &error));
  LocalJobRecord *job = NULL;
  g_assert_true(local_job_store_claim_next(fixture->store, OWNER_ID, ATTEMPT_ID,
                                           "2026-09-26T20:00:01Z", &job,
                                           &error));
  local_job_record_free(job);
  /* Simule un nouveau superviseur ayant acquis le verrou après SIGKILL. */
  local_job_store_close(fixture->store);
  fixture->store =
      local_job_store_open(fixture->path, INVESTIGATION_ID, FALSE, &error);
  g_assert_nonnull(fixture->store);
  g_assert_true(local_job_store_reconcile_interrupted(
      fixture->store, JOB_ID, LOCAL_JOB_COMPLETED, "recovered_after_publish",
      "published", "2026-09-26T20:00:04Z", &error));
  job = local_job_store_find(fixture->store, JOB_ID, &error);
  g_assert_cmpint(job->state, ==, LOCAL_JOB_COMPLETED);
  g_assert_cmpuint(job->attempt_count, ==, 1);
  local_job_record_free(job);
}

static void test_pause_cancel_and_owner(Fixture *fixture,
                                        gconstpointer unused) {
  (void)unused;
  GError *error = NULL;
  LocalJobSubmission item = submission();
  g_assert_true(local_job_store_enqueue(fixture->store, &item, NULL, &error));
  g_assert_true(local_job_store_set_paused(fixture->store, TRUE,
                                           "2026-09-26T20:00:01Z", &error));
  LocalJobRecord *job = (LocalJobRecord *)0x1;
  g_assert_true(local_job_store_claim_next(fixture->store, OWNER_ID, ATTEMPT_ID,
                                           "2026-09-26T20:00:02Z", &job,
                                           &error));
  g_assert_null(job);
  g_assert_true(local_job_store_request_cancel(fixture->store, JOB_ID,
                                               "2026-09-26T20:00:03Z", &error));
  job = local_job_store_find(fixture->store, JOB_ID, &error);
  g_assert_cmpint(job->state, ==, LOCAL_JOB_CANCELLED);
  local_job_record_free(job);

  LocalJobStore *wrong = local_job_store_open(
      fixture->path, "81000000-0000-4000-8000-000000000099", FALSE, &error);
  g_assert_null(wrong);
  g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
  g_clear_error(&error);
}

int main(int argc, char **argv) {
  g_test_init(&argc, &argv, NULL);
  g_test_add("/local-jobs/lifecycle", Fixture, NULL, setup,
             test_lifecycle_and_stale_owner, teardown);
  g_test_add("/local-jobs/crash-reconciliation", Fixture, NULL, setup,
             test_crash_reconciliation, teardown);
  g_test_add("/local-jobs/pause-cancel-owner", Fixture, NULL, setup,
             test_pause_cancel_and_owner, teardown);
  g_test_add("/local-jobs/plan-budget-finish", Fixture, NULL, setup,
             test_plan_budget_claim_and_finish, teardown);
  g_test_add("/local-jobs/plan-crash-reservation", Fixture, NULL, setup,
             test_plan_crash_keeps_reservation, teardown);
  g_test_add("/local-jobs/future-version", Fixture, NULL, setup,
             test_future_version_refused, teardown);
  g_test_add("/local-jobs/fresh-v4", Fixture, NULL, setup, test_fresh_v4,
             teardown);
  g_test_add_data_func("/local-jobs/migration-v1-v4", GUINT_TO_POINTER(1), test_historical_migration);
  g_test_add_data_func("/local-jobs/migration-v2-v4", GUINT_TO_POINTER(2), test_historical_migration);
  g_test_add_data_func("/local-jobs/migration-v3-v4", GUINT_TO_POINTER(3), test_historical_migration);
  g_test_add_func("/local-jobs/read-only-contract", test_read_only_contract);
  return g_test_run();
}
