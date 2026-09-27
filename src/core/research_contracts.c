/******************************************************************************
 * @file research_contracts.c
 * @brief Validation des contrats de recherche assistée V1.
 ******************************************************************************/
#include "core/research_contracts.h"

#include <string.h>

static void contract_error(GError **error, const char *message) {
  if (error != NULL && *error == NULL)
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT, message);
}

static gboolean nonempty(const char *value) {
  return value != NULL && value[0] != '\0';
}

static gboolean fingerprint_valid(const char *value) {
  if (value == NULL || strlen(value) != 64)
    return FALSE;
  for (gsize i = 0; i < 64; i++)
    if (!g_ascii_isxdigit(value[i]))
      return FALSE;
  return TRUE;
}

static gboolean timestamp_valid(const char *value) {
  GDateTime *parsed = value != NULL
                          ? g_date_time_new_from_iso8601(value, NULL)
                          : NULL;
  if (parsed == NULL)
    return FALSE;
  g_date_time_unref(parsed);
  return TRUE;
}

static gboolean relative_artifact_valid(const char *value) {
  if (!nonempty(value) || g_path_is_absolute(value) || strchr(value, '\\'))
    return FALSE;
  char **parts = g_strsplit(value, "/", -1);
  gboolean valid = TRUE;
  for (gsize i = 0; valid && parts[i] != NULL; i++)
    valid = parts[i][0] != '\0' && g_strcmp0(parts[i], ".") != 0 &&
            g_strcmp0(parts[i], "..") != 0;
  g_strfreev(parts);
  return valid;
}

const char *research_seed_kind_code(ResearchSeedKind kind) {
  static const char *const codes[] = {"DOMAIN", "IP", "HTTP_URL", "EMAIL",
                                      "SEARCH_TERM"};
  return kind <= RESEARCH_SEED_SEARCH_TERM ? codes[kind] : NULL;
}

const char *research_contact_class_code(ResearchContactClass contact) {
  static const char *const codes[] = {"NONE", "THIRD_PARTY", "TARGET"};
  return contact <= RESEARCH_CONTACT_TARGET ? codes[contact] : NULL;
}

const char *research_policy_decision_code(ResearchPolicyDecision decision) {
  static const char *const codes[] = {
      "ALLOW",          "DENY_EXCLUDED", "DENY_REVOKED",
      "DENY_EXPIRED",   "DENY_INVESTIGATION", "DENY_PLAN",
      "DENY_ACTION",    "DENY_ACTION_CONTENT", "DENY_BUDGET"};
  return decision <= RESEARCH_POLICY_DENY_BUDGET ? codes[decision]
                                                  : "DENY_ACTION";
}

const char *research_action_decision_code(ResearchActionDecisionCode decision) {
  static const char *const codes[] = {"AUTHORIZE", "DEFER", "REFUSE"};
  return decision <= RESEARCH_ACTION_REFUSE ? codes[decision] : NULL;
}

static gboolean action_valid(const ResearchAction *action) {
  return action != NULL && g_uuid_string_is_valid(action->action_id) &&
         nonempty(action->capability_id) && nonempty(action->provider_id) &&
         nonempty(action->endpoint) && nonempty(action->subject) &&
         research_contact_class_code(action->contact) != NULL &&
         nonempty(action->disclosure) && action->max_requests > 0 &&
         action->max_response_bytes > 0 &&
         action->max_response_bytes <= G_MAXINT64 && action->max_active_ms > 0;
}

gboolean research_plan_validate(const ResearchPlan *plan, GError **error) {
  if (plan == NULL || g_strcmp0(plan->contract, RESEARCH_PLAN_CONTRACT) != 0 ||
      !g_uuid_string_is_valid(plan->plan_id) ||
      !nonempty(plan->idempotency_key) ||
      !fingerprint_valid(plan->content_fingerprint) ||
      !fingerprint_valid(plan->input_fingerprint) || plan->input_revision == 0 ||
      !timestamp_valid(plan->created_at) || plan->seeds == NULL ||
      plan->seed_count == 0 || plan->actions == NULL ||
      plan->action_count == 0) {
    contract_error(error, "Contrat ResearchPlan V1 invalide.");
    return FALSE;
  }
  GHashTable *identities = g_hash_table_new(g_str_hash, g_str_equal);
  gboolean valid = TRUE;
  for (gsize i = 0; valid && i < plan->seed_count; i++)
    valid = research_seed_kind_code(plan->seeds[i].kind) != NULL &&
            nonempty(plan->seeds[i].subject);
  for (gsize i = 0; valid && i < plan->action_count; i++) {
    const ResearchAction *action = &plan->actions[i];
    valid = action_valid(action) &&
            g_hash_table_add(identities, (gpointer)action->action_id);
  }
  g_hash_table_unref(identities);
  if (!valid)
    contract_error(error, "Seed ou action ResearchPlan invalide ou dupliquée.");
  return valid;
}

gboolean research_scope_grant_validate(const ScopeGrant *grant,
                                       GError **error) {
  if (grant == NULL ||
      g_strcmp0(grant->contract, RESEARCH_GRANT_CONTRACT) != 0 ||
      !g_uuid_string_is_valid(grant->grant_id) ||
      !g_uuid_string_is_valid(grant->investigation_id) ||
      !nonempty(grant->idempotency_key) ||
      !fingerprint_valid(grant->content_fingerprint) ||
      !fingerprint_valid(grant->plan_fingerprint) ||
      !timestamp_valid(grant->created_at) ||
      !timestamp_valid(grant->expires_at) ||
      grant->selected_action_ids == NULL || grant->selected_action_count == 0 ||
      grant->max_requests == 0 || grant->max_response_bytes == 0 ||
      grant->max_response_bytes > G_MAXINT64 ||
      grant->max_active_ms == 0) {
    contract_error(error, "Contrat ScopeGrant V1 invalide.");
    return FALSE;
  }
  GHashTable *identities = g_hash_table_new(g_str_hash, g_str_equal);
  gboolean valid = TRUE;
  for (gsize i = 0; valid && i < grant->selected_action_count; i++)
    valid = g_uuid_string_is_valid(grant->selected_action_ids[i]) &&
            g_hash_table_add(identities,
                             (gpointer)grant->selected_action_ids[i]);
  g_hash_table_remove_all(identities);
  for (gsize i = 0; valid && i < grant->excluded_subject_count; i++)
    valid = nonempty(grant->excluded_subjects[i]) &&
            g_hash_table_add(identities,
                             (gpointer)grant->excluded_subjects[i]);
  g_hash_table_unref(identities);
  if (!valid)
    contract_error(error, "Sélection ou exclusion ScopeGrant invalide.");
  return valid;
}

gboolean research_campaign_validate(const ResearchCampaign *campaign,
                                    GError **error) {
  gboolean valid = campaign != NULL &&
      g_strcmp0(campaign->contract, RESEARCH_CAMPAIGN_CONTRACT) == 0 &&
      g_uuid_string_is_valid(campaign->campaign_id) &&
      g_uuid_string_is_valid(campaign->investigation_id) &&
      g_uuid_string_is_valid(campaign->grant_id) &&
      nonempty(campaign->idempotency_key) &&
      fingerprint_valid(campaign->content_fingerprint) &&
      timestamp_valid(campaign->created_at);
  if (!valid)
    contract_error(error, "Contrat ResearchCampaign V1 invalide.");
  return valid;
}

gboolean research_result_validate(const ResearchResult *result,
                                  GError **error) {
  gboolean valid = result != NULL &&
      g_strcmp0(result->contract, RESEARCH_RESULT_CONTRACT) == 0 &&
      g_uuid_string_is_valid(result->result_id) &&
      g_uuid_string_is_valid(result->campaign_id) &&
      g_uuid_string_is_valid(result->action_id) &&
      relative_artifact_valid(result->raw_artifact_relative_path) &&
      fingerprint_valid(result->content_sha256) && nonempty(result->status);
  if (!valid)
    contract_error(error, "Contrat ResearchResult V1 invalide.");
  return valid;
}

gboolean research_receipt_validate(const ResearchReceipt *receipt,
                                   GError **error) {
  gboolean valid = receipt != NULL &&
      g_strcmp0(receipt->contract, RESEARCH_RECEIPT_CONTRACT) == 0 &&
      g_uuid_string_is_valid(receipt->receipt_id) &&
      g_uuid_string_is_valid(receipt->result_id) &&
      g_uuid_string_is_valid(receipt->campaign_id) &&
      g_uuid_string_is_valid(receipt->grant_id) &&
      g_uuid_string_is_valid(receipt->action_id) &&
      timestamp_valid(receipt->decided_at) && nonempty(receipt->decision_code);
  if (!valid)
    contract_error(error, "Contrat ResearchReceipt V1 invalide.");
  return valid;
}
