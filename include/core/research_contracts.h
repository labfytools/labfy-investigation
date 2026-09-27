/******************************************************************************
 * @file research_contracts.h
 * @brief Contrats versionnés de recherche assistée, sans exécution réseau.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_RESEARCH_CONTRACTS_H
#define LABFY_INVESTIGATION_RESEARCH_CONTRACTS_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define RESEARCH_PLAN_CONTRACT "labfy.research_plan.v1"
#define RESEARCH_GRANT_CONTRACT "labfy.scope_grant.v1"
#define RESEARCH_CAMPAIGN_CONTRACT "labfy.research_campaign.v1"
#define RESEARCH_RESULT_CONTRACT "labfy.research_result.v1"
#define RESEARCH_RECEIPT_CONTRACT "labfy.research_receipt.v1"

typedef enum {
  RESEARCH_SEED_DOMAIN,
  RESEARCH_SEED_IP,
  RESEARCH_SEED_HTTP_URL,
  RESEARCH_SEED_EMAIL,
  RESEARCH_SEED_SEARCH_TERM
} ResearchSeedKind;

typedef enum {
  RESEARCH_CONTACT_NONE,
  RESEARCH_CONTACT_THIRD_PARTY,
  RESEARCH_CONTACT_TARGET
} ResearchContactClass;

typedef struct {
  ResearchSeedKind kind;
  const char *subject;
} ResearchSeed;

typedef struct {
  const char *action_id;
  const char *capability_id;
  const char *provider_id;
  const char *endpoint;
  const char *subject;
  ResearchContactClass contact;
  const char *disclosure;
  guint max_requests;
  guint64 max_response_bytes;
  guint max_active_ms;
} ResearchAction;

typedef struct {
  const char *contract;
  const char *plan_id;
  const char *idempotency_key;
  const char *content_fingerprint;
  const char *input_fingerprint;
  guint64 input_revision;
  const char *created_at;
  const ResearchSeed *seeds;
  gsize seed_count;
  const ResearchAction *actions;
  gsize action_count;
} ResearchPlan;

typedef enum {
  RESEARCH_ACTION_AUTHORIZE,
  RESEARCH_ACTION_DEFER,
  RESEARCH_ACTION_REFUSE
} ResearchActionDecisionCode;

typedef struct {
  const char *action_id;
  ResearchActionDecisionCode code;
} ResearchActionDecision;

typedef struct {
  const char *contract;
  const char *grant_id;
  const char *investigation_id;
  const char *idempotency_key;
  const char *content_fingerprint;
  const char *plan_fingerprint;
  const char *created_at;
  const char *expires_at;
  const char *const *selected_action_ids;
  gsize selected_action_count;
  const char *const *excluded_subjects;
  gsize excluded_subject_count;
  guint max_requests;
  guint64 max_response_bytes;
  guint max_active_ms;
} ScopeGrant;

typedef struct {
  const char *contract;
  const char *campaign_id;
  const char *investigation_id;
  const char *grant_id;
  const char *idempotency_key;
  const char *content_fingerprint;
  const char *created_at;
} ResearchCampaign;

typedef struct {
  const char *contract;
  const char *result_id;
  const char *campaign_id;
  const char *action_id;
  const char *raw_artifact_relative_path;
  const char *content_sha256;
  const char *status;
} ResearchResult;

typedef struct {
  const char *contract;
  const char *receipt_id;
  const char *result_id;
  const char *campaign_id;
  const char *grant_id;
  const char *action_id;
  const char *decided_at;
  const char *decision_code;
} ResearchReceipt;

typedef enum {
  RESEARCH_POLICY_ALLOW,
  RESEARCH_POLICY_DENY_EXCLUDED,
  RESEARCH_POLICY_DENY_REVOKED,
  RESEARCH_POLICY_DENY_EXPIRED,
  RESEARCH_POLICY_DENY_INVESTIGATION,
  RESEARCH_POLICY_DENY_PLAN,
  RESEARCH_POLICY_DENY_ACTION,
  RESEARCH_POLICY_DENY_ACTION_CONTENT,
  RESEARCH_POLICY_DENY_BUDGET
} ResearchPolicyDecision;

typedef struct {
  const char *investigation_id;
  const char *plan_fingerprint;
  const ResearchAction *action;
  const char *now;
  guint requested_requests;
  guint64 requested_response_bytes;
  guint requested_active_ms;
} ResearchPolicyRequest;

/**
 * Vérifie la forme complète d'un plan avant toute écriture.
 * CONTRACT: les identités et empreintes sont stables et toutes les actions
 * sont bornées. INVARIANT: une seed dérivée n'accorde jamais de scope.
 */
gboolean research_plan_validate(const ResearchPlan *plan, GError **error);

/**
 * Vérifie le contenu durable d'un grant, à l'exclusion des cookies/CSRF qui
 * n'appartiennent pas à ce contrat persistant.
 */
gboolean research_scope_grant_validate(const ScopeGrant *grant,
                                       GError **error);
gboolean research_campaign_validate(const ResearchCampaign *campaign,
                                    GError **error);
gboolean research_result_validate(const ResearchResult *result,
                                  GError **error);
gboolean research_receipt_validate(const ResearchReceipt *receipt,
                                   GError **error);

const char *research_seed_kind_code(ResearchSeedKind kind);
const char *research_contact_class_code(ResearchContactClass contact);
const char *research_policy_decision_code(ResearchPolicyDecision decision);
const char *research_action_decision_code(ResearchActionDecisionCode decision);

G_END_DECLS
#endif
