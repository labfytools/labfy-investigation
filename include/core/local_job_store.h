/******************************************************************************
 * @file local_job_store.h
 * @brief Stockage opérationnel V3 des jobs et plans locaux J5/J8.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_LOCAL_JOB_STORE_H
#define LABFY_INVESTIGATION_LOCAL_JOB_STORE_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define LOCAL_JOB_STORE_SCHEMA_VERSION 3
#define LOCAL_JOB_STORE_MAX_JOBS 128U
#define LOCAL_JOB_STORE_MAX_ATTEMPTS 3U
#define LOCAL_JOB_DIAGNOSTIC_MAX 2048U

typedef enum {
  LOCAL_JOB_QUEUED,
  LOCAL_JOB_RUNNING,
  LOCAL_JOB_RETRY_WAIT,
  LOCAL_JOB_COMPLETED,
  LOCAL_JOB_FAILED,
  LOCAL_JOB_CANCELLED,
  LOCAL_JOB_RECOVERY_REQUIRED,
  LOCAL_JOB_BLOCKED
} LocalJobState;

typedef struct LocalJobStore LocalJobStore;
typedef struct {
  char *job_id;
  char *request_id;
  char *source_evidence_id;
  char *derivative_evidence_id;
  char *capability_id;
  char *adapter_id;
  char *adapter_version;
  char *source_sha256;
  guint64 source_size;
  char *requested_at;
  char *parameters_json;
  LocalJobState state;
  gboolean cancel_requested;
  guint max_attempts;
  guint attempt_count;
  char *result_status;
  char *diagnostic;
  char *dependency_job_id;
  guint64 revision;
} LocalJobRecord;

typedef struct {
  const char *job_id;
  const char *request_id;
  const char *source_evidence_id;
  const char *derivative_evidence_id;
  const char *capability_id;
  const char *adapter_id;
  const char *adapter_version;
  const char *source_sha256;
  guint64 source_size;
  const char *requested_at;
  const char *parameters_json;
  const char *dependency_job_id;
  guint max_attempts;
} LocalJobSubmission;

typedef struct {
  const char *plan_id;
  const char *idempotency_key;
  const char *intent_hash;
  const char *input_revision;
  const char *profile_id;
  const char *created_at;
  guint max_analyses;
  guint max_attempts_total;
  guint64 max_source_bytes;
  guint64 max_active_ms;
} LocalPlanAdmission;

/**
 * Admet durablement un plan et tous ses jobs dans une seule transaction.
 * CONTRACT: une clé existante est examinée avant les préconditions de
 * création. Même intention => même plan; intention différente => conflit.
 * INVARIANT: aucune admission partielle ni réservation partielle de budget.
 */
gboolean local_job_store_admit_plan(LocalJobStore *store,
                                    const LocalPlanAdmission *plan,
                                    const LocalJobSubmission *jobs,
                                    gsize job_count, gboolean *out_reused,
                                    GError **error);
gboolean local_job_store_find_plan_intention(LocalJobStore *store,
    const char *idempotency_key, const char *intent_hash,
    char **out_plan_id, GError **error);

LocalJobStore *local_job_store_create(const char *path,
                                      const char *investigation_id,
                                      GError **error);
LocalJobStore *local_job_store_open(const char *path,
                                    const char *investigation_id,
                                    gboolean read_only, GError **error);
#ifdef LOCAL_JOB_STORE_ENABLE_TEST_HOOKS
/* TEST CONTRACT: injecte une faute après les ALTER/CREATE de migration mais
 * avant la publication de version. Ce symbole n'existe dans aucun binaire
 * opérationnel. */
void local_job_store_test_fail_migration_after_schema(gboolean enabled);
#endif
void local_job_store_close(LocalJobStore *store);

gboolean local_job_store_enqueue(LocalJobStore *store,
                                 const LocalJobSubmission *submission,
                                 gboolean *out_reused, GError **error);
LocalJobRecord *local_job_store_find(LocalJobStore *store, const char *job_id,
                                     GError **error);
GPtrArray *local_job_store_list(LocalJobStore *store, GError **error);
void local_job_record_free(LocalJobRecord *record);

gboolean local_job_store_claim_next(LocalJobStore *store,
                                    const char *owner_token,
                                    const char *attempt_id, const char *now,
                                    LocalJobRecord **out_job, GError **error);
gboolean local_job_store_claim_next_budgeted(LocalJobStore *store,
    const char *owner_token, const char *attempt_id, const char *now,
    guint requested_active_ms, guint *out_reserved_active_ms,
    LocalJobRecord **out_job, GError **error);
gboolean local_job_store_heartbeat(LocalJobStore *store, const char *job_id,
                                   const char *owner_token, const char *now,
                                   GError **error);
gboolean local_job_store_finish(LocalJobStore *store, const char *job_id,
                                const char *owner_token, LocalJobState state,
                                const char *reason, const char *result_status,
                                const char *now, GError **error);
gboolean local_job_store_finish_budgeted(LocalJobStore *store,
    const char *job_id, const char *owner_token, LocalJobState state,
    const char *reason, const char *result_status, const char *now,
    guint elapsed_active_ms, GError **error);
gboolean local_job_store_requeue_interrupted(LocalJobStore *store,
                                             const char *job_id,
                                             const char *reason,
                                             const char *now, GError **error);
gboolean local_job_store_reconcile_interrupted(LocalJobStore *store,
                                               const char *job_id,
                                               LocalJobState state,
                                               const char *reason,
                                               const char *result_status,
                                               const char *now, GError **error);
gboolean local_job_store_request_cancel(LocalJobStore *store,
                                        const char *job_id, const char *now,
                                        GError **error);
gboolean local_job_store_set_paused(LocalJobStore *store, gboolean paused,
                                    const char *now, GError **error);
gboolean local_job_store_request_stop(LocalJobStore *store, const char *now,
                                      GError **error);
gboolean local_job_store_set_stopped(LocalJobStore *store, gboolean stopped,
                                     const char *now, GError **error);
gboolean local_job_store_controls(LocalJobStore *store, gboolean *out_paused,
                                  gboolean *out_stop_requested, GError **error);
gboolean local_job_store_export_atomic(LocalJobStore *store, const char *path,
                                       GError **error);
gboolean local_job_store_integrity(LocalJobStore *store, GError **error);
const char *local_job_state_code(LocalJobState state);

G_END_DECLS
#endif
