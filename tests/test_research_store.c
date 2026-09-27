#include "core/research_store.h"

#include <glib/gstdio.h>
#include <sqlite3.h>

#define INVESTIGATION_ID "82000000-0000-4000-8000-000000000001"
#define OTHER_INVESTIGATION_ID "82000000-0000-4000-8000-000000000002"
#define PLAN_ID "82000000-0000-4000-8000-000000000010"
#define ACTION_DNS_ID "82000000-0000-4000-8000-000000000011"
#define ACTION_HTTP_ID "82000000-0000-4000-8000-000000000012"
#define GRANT_ID "82000000-0000-4000-8000-000000000020"
#define EXPIRED_GRANT_ID "82000000-0000-4000-8000-000000000021"
#define BAD_GRANT_ID "82000000-0000-4000-8000-000000000022"
#define WAVE_GRANT_ID "82000000-0000-4000-8000-000000000023"
#define REFUSE_GRANT_ID "82000000-0000-4000-8000-000000000024"
#define CAMPAIGN_ID "82000000-0000-4000-8000-000000000030"
#define RESULT_ID "82000000-0000-4000-8000-000000000031"
#define RECEIPT_ID "82000000-0000-4000-8000-000000000032"
#define HASH_A "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
#define HASH_B "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
#define HASH_C "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
#define HASH_D "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"

static const ResearchActionDecision grant_decisions[] = {
    {ACTION_DNS_ID, RESEARCH_ACTION_AUTHORIZE},
    {ACTION_HTTP_ID, RESEARCH_ACTION_AUTHORIZE}};
static const char *const decision_time = "2026-09-27T10:01:00Z";

typedef struct {
  char *directory;
  char *path;
  LocalJobStore *store;
} Fixture;

static void setup(Fixture *fixture, gconstpointer unused) {
  (void)unused;
  GError *error = NULL;
  fixture->directory = g_dir_make_tmp("labfy-research-XXXXXX", &error);
  g_assert_no_error(error);
  fixture->path = g_build_filename(fixture->directory, "jobs.sqlite", NULL);
  fixture->store = local_job_store_create(fixture->path, INVESTIGATION_ID,
                                           &error);
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

static ResearchPlan plan_fixture(ResearchAction actions[2],
                                 ResearchSeed seeds[2]) {
  seeds[0] = (ResearchSeed){RESEARCH_SEED_DOMAIN, "specimen.example"};
  seeds[1] = (ResearchSeed){RESEARCH_SEED_SEARCH_TERM, "SPECIMEN ALPHA"};
  actions[0] = (ResearchAction){
      .action_id = ACTION_DNS_ID,
      .capability_id = "research.dns.passive",
      .provider_id = "specimen.provider",
      .endpoint = "https://api.example.test/dns",
      .subject = "specimen.example",
      .contact = RESEARCH_CONTACT_THIRD_PARTY,
      .disclosure = "DOMAIN_TO_PROVIDER",
      .max_requests = 2,
      .max_response_bytes = 4096,
      .max_active_ms = 500};
  actions[1] = (ResearchAction){
      .action_id = ACTION_HTTP_ID,
      .capability_id = "research.http.target",
      .provider_id = "direct",
      .endpoint = "https://specimen.example/profile",
      .subject = "https://specimen.example/profile",
      .contact = RESEARCH_CONTACT_TARGET,
      .disclosure = "SOURCE_IP_TO_TARGET",
      .max_requests = 1,
      .max_response_bytes = 8192,
      .max_active_ms = 700};
  return (ResearchPlan){
      .contract = RESEARCH_PLAN_CONTRACT,
      .plan_id = PLAN_ID,
      .idempotency_key = "plan-key-specimen",
      .content_fingerprint = HASH_A,
      .input_fingerprint = HASH_B,
      .input_revision = 3,
      .created_at = "2026-09-27T10:00:00Z",
      .seeds = seeds,
      .seed_count = 2,
      .actions = actions,
      .action_count = 2};
}

static ScopeGrant grant_fixture(const char *grant_id, const char *key,
                                const char *fingerprint,
                                const char *expires_at,
                                const char *const *selected,
                                gsize selected_count) {
  static const char *const exclusions[] = {
      "https://specimen.example/profile"};
  return (ScopeGrant){
      .contract = RESEARCH_GRANT_CONTRACT,
      .grant_id = grant_id,
      .investigation_id = INVESTIGATION_ID,
      .idempotency_key = key,
      .content_fingerprint = fingerprint,
      .plan_fingerprint = HASH_A,
      .created_at = "2026-09-27T10:01:00Z",
      .expires_at = expires_at,
      .selected_action_ids = selected,
      .selected_action_count = selected_count,
      .excluded_subjects = exclusions,
      .excluded_subject_count = G_N_ELEMENTS(exclusions),
      .max_requests = 3,
      .max_response_bytes = 12288,
      .max_active_ms = 1200};
}

static gint64 table_count(const char *path, const char *table) {
  sqlite3 *db = NULL;
  sqlite3_stmt *statement = NULL;
  g_assert_cmpint(sqlite3_open(path, &db), ==, SQLITE_OK);
  char *sql = g_strdup_printf("SELECT count(*) FROM %s;", table);
  g_assert_cmpint(sqlite3_prepare_v2(db, sql, -1, &statement, NULL), ==,
                  SQLITE_OK);
  g_assert_cmpint(sqlite3_step(statement), ==, SQLITE_ROW);
  gint64 value = sqlite3_column_int64(statement, 0);
  sqlite3_finalize(statement);
  sqlite3_close(db);
  g_free(sql);
  return value;
}

static void admit_valid_plan(Fixture *fixture, ResearchAction actions[2],
                             ResearchSeed seeds[2]) {
  GError *error = NULL;
  ResearchPlan plan = plan_fixture(actions, seeds);
  gboolean reused = TRUE;
  g_assert_true(
      research_store_admit_plan(fixture->store, &plan, &reused, &error));
  g_assert_no_error(error);
  g_assert_false(reused);
}

static void test_plan_atomic_idempotence(Fixture *fixture,
                                         gconstpointer unused) {
  (void)unused;
  ResearchAction actions[2];
  ResearchSeed seeds[2];
  ResearchPlan plan = plan_fixture(actions, seeds);
  actions[1].max_requests = 0;
  GError *error = NULL;
  g_assert_false(research_store_admit_plan(fixture->store, &plan, NULL,
                                           &error));
  g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_clear_error(&error);
  g_assert_cmpint(table_count(fixture->path, "research_plans"), ==, 0);
  g_assert_cmpint(table_count(fixture->path, "research_actions"), ==, 0);

  plan = plan_fixture(actions, seeds);
  gboolean reused = FALSE;
  g_assert_true(
      research_store_admit_plan(fixture->store, &plan, &reused, &error));
  g_assert_false(reused);
  g_assert_true(
      research_store_admit_plan(fixture->store, &plan, &reused, &error));
  g_assert_true(reused);
  plan.content_fingerprint = HASH_C;
  g_assert_false(
      research_store_admit_plan(fixture->store, &plan, &reused, &error));
  g_assert_error(error, G_IO_ERROR, G_IO_ERROR_EXISTS);
  g_clear_error(&error);
  g_assert_cmpint(table_count(fixture->path, "research_plans"), ==, 1);
  g_assert_cmpint(table_count(fixture->path, "research_actions"), ==, 2);
}

static ResearchPolicyDecision decide(Fixture *fixture, const char *grant_id,
                                     const ResearchAction *action,
                                     const char *investigation_id,
                                     const char *plan_fingerprint,
                                     const char *now) {
  ResearchPolicyRequest request = {
      .investigation_id = investigation_id,
      .plan_fingerprint = plan_fingerprint,
      .action = action,
      .now = now,
      .requested_requests = 1,
      .requested_response_bytes = 1024,
      .requested_active_ms = 100};
  ResearchPolicyDecision decision = RESEARCH_POLICY_DENY_ACTION;
  GError *error = NULL;
  g_assert_true(research_store_policy_decide(fixture->store, grant_id,
                                              &request, &decision, &error));
  g_assert_no_error(error);
  return decision;
}

static void test_grant_policy(Fixture *fixture, gconstpointer unused) {
  (void)unused;
  ResearchAction actions[2];
  ResearchSeed seeds[2];
  admit_valid_plan(fixture, actions, seeds);
  static const char *const selected[] = {ACTION_DNS_ID};
  ScopeGrant grant = grant_fixture(GRANT_ID, "grant-key-specimen", HASH_C,
      "2026-09-28T00:00:00Z", selected, G_N_ELEMENTS(selected));
  GError *error = NULL;
  gboolean reused = FALSE;
  g_assert_true(
      research_store_admit_grant(fixture->store, &grant, grant_decisions,
          G_N_ELEMENTS(grant_decisions), decision_time, &reused, &error));
  g_assert_false(reused);
  g_assert_true(
      research_store_admit_grant(fixture->store, &grant, grant_decisions,
          G_N_ELEMENTS(grant_decisions), decision_time, &reused, &error));
  g_assert_true(reused);
  grant.content_fingerprint = HASH_D;
  g_assert_false(
      research_store_admit_grant(fixture->store, &grant, grant_decisions,
          G_N_ELEMENTS(grant_decisions), decision_time, &reused, &error));
  g_assert_error(error, G_IO_ERROR, G_IO_ERROR_EXISTS);
  g_clear_error(&error);

  g_assert_cmpint(decide(fixture, GRANT_ID, &actions[0], INVESTIGATION_ID,
                         HASH_A, "2026-09-27T12:00:00Z"), ==,
                  RESEARCH_POLICY_ALLOW);
  g_assert_cmpint(decide(fixture, GRANT_ID, &actions[0],
                         OTHER_INVESTIGATION_ID, HASH_A,
                         "2026-09-27T12:00:00Z"), ==,
                  RESEARCH_POLICY_DENY_INVESTIGATION);
  ResearchAction redirected = actions[0];
  redirected.endpoint = "https://redirected.example.test/dns";
  g_assert_cmpint(decide(fixture, GRANT_ID, &redirected, INVESTIGATION_ID,
                         HASH_A, "2026-09-27T12:00:00Z"), ==,
                  RESEARCH_POLICY_DENY_ACTION);
  g_assert_cmpint(decide(fixture, GRANT_ID, &actions[1], INVESTIGATION_ID,
                         HASH_A, "2026-09-27T12:00:00Z"), ==,
                  RESEARCH_POLICY_DENY_EXCLUDED);

  g_assert_true(research_store_revoke_grant(
      fixture->store, GRANT_ID, "2026-09-27T13:00:00Z", &error));
  g_assert_cmpint(decide(fixture, GRANT_ID, &actions[1], INVESTIGATION_ID,
                         HASH_A, "2026-09-27T14:00:00Z"), ==,
                  RESEARCH_POLICY_DENY_EXCLUDED);
  g_assert_cmpint(decide(fixture, GRANT_ID, &actions[0], INVESTIGATION_ID,
                         HASH_A, "2026-09-27T14:00:00Z"), ==,
                  RESEARCH_POLICY_DENY_REVOKED);
}

static void test_expiry_and_atomic_bad_action(Fixture *fixture,
                                               gconstpointer unused) {
  (void)unused;
  ResearchAction actions[2];
  ResearchSeed seeds[2];
  admit_valid_plan(fixture, actions, seeds);
  static const char *const selected[] = {ACTION_DNS_ID};
  ScopeGrant expired = grant_fixture(EXPIRED_GRANT_ID, "expired-key", HASH_C,
      "2026-09-27T11:00:00Z", selected, G_N_ELEMENTS(selected));
  GError *error = NULL;
  g_assert_true(research_store_admit_grant(fixture->store, &expired,
      grant_decisions, G_N_ELEMENTS(grant_decisions), decision_time, NULL,
      &error));
  g_assert_cmpint(decide(fixture, EXPIRED_GRANT_ID, &actions[0],
                         INVESTIGATION_ID, HASH_A,
                         "2026-09-27T11:00:00Z"), ==,
                  RESEARCH_POLICY_DENY_EXPIRED);

  static const char *const unknown[] = {
      "82000000-0000-4000-8000-000000000099"};
  ScopeGrant bad = grant_fixture(BAD_GRANT_ID, "bad-key", HASH_D,
      "2026-09-28T00:00:00Z", unknown, G_N_ELEMENTS(unknown));
  g_assert_false(
      research_store_admit_grant(fixture->store, &bad, grant_decisions,
          G_N_ELEMENTS(grant_decisions), decision_time, NULL, &error));
  g_assert_error(error, G_IO_ERROR, G_IO_ERROR_FAILED);
  g_clear_error(&error);
  g_assert_cmpint(table_count(fixture->path, "scope_grants"), ==, 1);
  g_assert_true(local_job_store_integrity(fixture->store, &error));
  g_assert_no_error(error);
}

static void test_cross_investigation_admission(Fixture *fixture,
                                                gconstpointer unused) {
  (void)unused;
  ResearchAction actions[2];
  ResearchSeed seeds[2];
  admit_valid_plan(fixture, actions, seeds);
  static const char *const selected[] = {ACTION_DNS_ID};
  ScopeGrant grant = grant_fixture(GRANT_ID, "cross-key", HASH_C,
      "2026-09-28T00:00:00Z", selected, G_N_ELEMENTS(selected));
  grant.investigation_id = OTHER_INVESTIGATION_ID;
  GError *error = NULL;
  g_assert_false(
      research_store_admit_grant(fixture->store, &grant, grant_decisions,
          G_N_ELEMENTS(grant_decisions), decision_time, NULL, &error));
  g_assert_error(error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED);
  g_clear_error(&error);
  g_assert_cmpint(table_count(fixture->path, "scope_grants"), ==, 0);
}

static void test_grant_decision_coverage(Fixture *fixture,
                                         gconstpointer unused) {
  (void)unused;
  ResearchAction actions[2];
  ResearchSeed seeds[2];
  admit_valid_plan(fixture, actions, seeds);
  static const char *const selected[] = {ACTION_DNS_ID};
  ScopeGrant grant = grant_fixture(GRANT_ID, "coverage-key", HASH_C,
      "2026-09-28T00:00:00Z", selected, G_N_ELEMENTS(selected));
  ResearchActionDecision incomplete[] = {
      {ACTION_DNS_ID, RESEARCH_ACTION_AUTHORIZE}};
  ResearchActionDecision complete[] = {
      {ACTION_DNS_ID, RESEARCH_ACTION_AUTHORIZE},
      {ACTION_HTTP_ID, RESEARCH_ACTION_DEFER}};
  GError *error = NULL;
  g_assert_false(research_store_admit_grant(fixture->store, &grant,
      incomplete, G_N_ELEMENTS(incomplete), decision_time, NULL, &error));
  g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_clear_error(&error);
  g_assert_cmpint(table_count(fixture->path, "scope_grants"), ==, 0);
  g_assert_cmpint(table_count(fixture->path, "research_action_decisions"), ==,
                  0);
  g_assert_true(research_store_admit_grant(fixture->store, &grant,
      complete, G_N_ELEMENTS(complete), decision_time, NULL, &error));
  g_assert_no_error(error);
  g_assert_cmpint(table_count(fixture->path, "scope_grants"), ==, 1);
  g_assert_cmpint(table_count(fixture->path, "research_action_decisions"), ==,
                  2);
}

static void test_refusal_is_terminal(Fixture *fixture, gconstpointer unused) {
  (void)unused;
  ResearchAction actions[2];
  ResearchSeed seeds[2];
  admit_valid_plan(fixture, actions, seeds);
  static const char *const first_selected[] = {ACTION_DNS_ID};
  ResearchActionDecision refused[] = {
      {ACTION_DNS_ID, RESEARCH_ACTION_AUTHORIZE},
      {ACTION_HTTP_ID, RESEARCH_ACTION_REFUSE}};
  ScopeGrant first = grant_fixture(GRANT_ID, "refusal-first", HASH_C,
      "2026-09-28T00:00:00Z", first_selected, G_N_ELEMENTS(first_selected));
  GError *error = NULL;
  g_assert_true(research_store_admit_grant(fixture->store, &first, refused,
      G_N_ELEMENTS(refused), decision_time, NULL, &error));

  static const char *const refused_selected[] = {ACTION_HTTP_ID};
  ResearchActionDecision retry[] = {
      {ACTION_DNS_ID, RESEARCH_ACTION_AUTHORIZE},
      {ACTION_HTTP_ID, RESEARCH_ACTION_AUTHORIZE}};
  ScopeGrant second = grant_fixture(REFUSE_GRANT_ID, "refusal-retry", HASH_D,
      "2026-09-28T00:00:00Z", refused_selected,
      G_N_ELEMENTS(refused_selected));
  g_assert_false(research_store_admit_grant(fixture->store, &second, retry,
      G_N_ELEMENTS(retry), decision_time, NULL, &error));
  g_assert_error(error, G_IO_ERROR, G_IO_ERROR_FAILED);
  g_clear_error(&error);
  g_assert_cmpint(table_count(fixture->path, "scope_grants"), ==, 1);
}

static void test_campaign_result_receipt(Fixture *fixture,
                                          gconstpointer unused) {
  (void)unused;
  ResearchAction actions[2];
  ResearchSeed seeds[2];
  admit_valid_plan(fixture, actions, seeds);
  static const char *const wave_two[] = {ACTION_HTTP_ID};
  ScopeGrant premature = grant_fixture(WAVE_GRANT_ID, "wave-two-premature",
      HASH_C, "2026-09-28T00:00:00Z", wave_two, G_N_ELEMENTS(wave_two));
  GError *error = NULL;
  g_assert_false(research_store_admit_grant(fixture->store, &premature,
      grant_decisions, G_N_ELEMENTS(grant_decisions), decision_time, NULL,
      &error));
  g_assert_error(error, G_IO_ERROR, G_IO_ERROR_FAILED);
  g_clear_error(&error);
  static const char *const selected[] = {ACTION_DNS_ID};
  ScopeGrant grant = grant_fixture(GRANT_ID, "campaign-grant", HASH_C,
      "2026-09-28T00:00:00Z", selected, G_N_ELEMENTS(selected));
  g_assert_true(research_store_admit_grant(fixture->store, &grant,
      grant_decisions, G_N_ELEMENTS(grant_decisions), decision_time, NULL,
      &error));
  ResearchCampaign campaign = {
      .contract = RESEARCH_CAMPAIGN_CONTRACT,
      .campaign_id = CAMPAIGN_ID,
      .investigation_id = INVESTIGATION_ID,
      .grant_id = GRANT_ID,
      .idempotency_key = "campaign-key",
      .content_fingerprint = HASH_D,
      .created_at = "2026-09-27T10:02:00Z"};
  gboolean reused = FALSE;
  g_assert_true(research_store_admit_campaign(fixture->store, &campaign,
                                               &reused, &error));
  g_assert_false(reused);
  g_assert_true(research_store_admit_campaign(fixture->store, &campaign,
                                               &reused, &error));
  g_assert_true(reused);

  ResearchResult result = {
      .contract = RESEARCH_RESULT_CONTRACT,
      .result_id = RESULT_ID,
      .campaign_id = CAMPAIGN_ID,
      .action_id = ACTION_DNS_ID,
      .raw_artifact_relative_path = "research/raw/specimen-dns.json",
      .content_sha256 = HASH_B,
      .status = "CAPTURED"};
  ResearchReceipt receipt = {
      .contract = RESEARCH_RECEIPT_CONTRACT,
      .receipt_id = RECEIPT_ID,
      .result_id = RESULT_ID,
      .campaign_id = CAMPAIGN_ID,
      .grant_id = GRANT_ID,
      .action_id = ACTION_DNS_ID,
      .decided_at = "2026-09-27T10:03:00Z",
      .decision_code = "ALLOW"};
  result.raw_artifact_relative_path = "../outside.json";
  g_assert_false(research_store_record_result(fixture->store, &result, &receipt,
                                               &error));
  g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_clear_error(&error);
  g_assert_cmpint(table_count(fixture->path, "research_results"), ==, 0);
  result.raw_artifact_relative_path = "research/raw/specimen-dns.json";
  g_assert_true(research_store_record_result(fixture->store, &result, &receipt,
                                              &error));
  g_assert_cmpint(table_count(fixture->path, "research_results"), ==, 1);
  g_assert_cmpint(table_count(fixture->path, "research_receipts"), ==, 1);

  ScopeGrant admitted_wave = grant_fixture(WAVE_GRANT_ID, "wave-two-after-first",
      HASH_C, "2026-09-28T00:00:00Z", wave_two, G_N_ELEMENTS(wave_two));
  g_assert_true(research_store_admit_grant(fixture->store, &admitted_wave,
      grant_decisions, G_N_ELEMENTS(grant_decisions), decision_time, NULL,
      &error));
  g_assert_true(local_job_store_integrity(fixture->store, &error));
  g_assert_no_error(error);
}

int main(int argc, char **argv) {
  g_test_init(&argc, &argv, NULL);
  g_test_add("/research/plan-atomic-idempotence", Fixture, NULL, setup,
             test_plan_atomic_idempotence, teardown);
  g_test_add("/research/grant-policy", Fixture, NULL, setup,
             test_grant_policy, teardown);
  g_test_add("/research/expiry-atomic-bad-action", Fixture, NULL, setup,
             test_expiry_and_atomic_bad_action, teardown);
  g_test_add("/research/cross-investigation", Fixture, NULL, setup,
             test_cross_investigation_admission, teardown);
  g_test_add("/research/grant-decision-coverage", Fixture, NULL, setup,
             test_grant_decision_coverage, teardown);
  g_test_add("/research/refusal-terminal", Fixture, NULL, setup,
             test_refusal_is_terminal, teardown);
  g_test_add("/research/campaign-result-receipt", Fixture, NULL, setup,
             test_campaign_result_receipt, teardown);
  return g_test_run();
}
