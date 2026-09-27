/******************************************************************************
 * @file research_policy.h
 * @brief Policy pure pour les destinations de recherche assistée V1.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_RESEARCH_POLICY_H
#define LABFY_INVESTIGATION_RESEARCH_POLICY_H

#include "core/research_contracts.h"

G_BEGIN_DECLS

typedef struct {
  const guint16 *allowed_ports;
  gsize allowed_port_count;
  const char *const *allowed_endpoints;
  gsize allowed_endpoint_count;
} ResearchNetworkProfile;

typedef struct {
  char *scheme;
  char *host;
  guint16 port;
  char *path_and_query;
} ResearchHttpTarget;

typedef enum {
  RESEARCH_REDIRECT_REQUIRES_DECISION,
  RESEARCH_REDIRECT_INVALID
} ResearchRedirectDecision;

/**
 * CONTRACT: valide l'action et le grant exacts avant tout contact réseau.
 * WHY: le profil décrit une allowlist, jamais une règle par suffixe.
 * INVARIANT: sujet, endpoint, port et sélection sont comparés exactement.
 */
gboolean research_policy_validate_http_action(
    const ResearchAction *action, const ScopeGrant *grant,
    const ResearchNetworkProfile *profile, ResearchHttpTarget *out_target,
    GError **error);

/** INVARIANT: seules les adresses globalement routables sont acceptées. */
gboolean research_policy_address_is_public(const char *address);

/**
 * CONTRACT: une redirection valide reste une nouvelle destination à décider.
 * Cette fonction ne propage jamais l'autorisation de l'action initiale.
 */
ResearchRedirectDecision research_policy_check_redirect(
    const char *location, const ResearchNetworkProfile *profile,
    ResearchHttpTarget *out_target, GError **error);

void research_http_target_clear(ResearchHttpTarget *target);

G_END_DECLS
#endif
