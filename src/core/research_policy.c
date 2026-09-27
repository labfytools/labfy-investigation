/******************************************************************************
 * @file research_policy.c
 * @brief Validation sans I/O des destinations de recherche assistée.
 ******************************************************************************/
#include "core/research_policy.h"

#include <netinet/in.h>
#include <string.h>

static void policy_error(GError **error, const char *message) {
  if (error != NULL && *error == NULL)
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT, message);
}

static gboolean clean_text(const char *value) {
  if (value == NULL || value[0] == '\0' || strchr(value, '\\') != NULL)
    return FALSE;
  for (const unsigned char *p = (const unsigned char *)value; *p != '\0'; p++)
    if (*p < 0x20 || *p == 0x7f)
      return FALSE;
  return TRUE;
}

static gboolean exact_member(const char *value, const char *const *values,
                             gsize count) {
  if (value == NULL || values == NULL)
    return FALSE;
  for (gsize i = 0; i < count; i++)
    if (g_strcmp0(value, values[i]) == 0)
      return TRUE;
  return FALSE;
}

static gboolean port_allowed(guint16 port, const ResearchNetworkProfile *profile) {
  if (profile == NULL || profile->allowed_ports == NULL)
    return FALSE;
  for (gsize i = 0; i < profile->allowed_port_count; i++)
    if (port == profile->allowed_ports[i])
      return TRUE;
  return FALSE;
}

static gboolean parse_http_target(const char *text,
                                  const ResearchNetworkProfile *profile,
                                  gboolean require_endpoint,
                                  ResearchHttpTarget *out, GError **error) {
  if (!clean_text(text) || strchr(text, '@') != NULL || profile == NULL ||
      out == NULL) {
    policy_error(error, "Destination HTTP(S) absente ou ambiguë.");
    return FALSE;
  }
  GError *uri_error = NULL;
  GUri *uri = g_uri_parse(text, G_URI_FLAGS_PARSE_RELAXED, &uri_error);
  if (uri == NULL) {
    g_clear_error(&uri_error);
    policy_error(error, "Destination HTTP(S) invalide.");
    return FALSE;
  }
  const char *scheme = g_uri_get_scheme(uri);
  const char *host = g_uri_get_host(uri);
  const char *userinfo = g_uri_get_userinfo(uri);
  gint parsed_port = g_uri_get_port(uri);
  guint16 port = parsed_port >= 0 ? (guint16)parsed_port
                                  : (g_strcmp0(scheme, "https") == 0 ? 443 : 80);
  gboolean valid = (g_strcmp0(scheme, "http") == 0 ||
                    g_strcmp0(scheme, "https") == 0) &&
                   clean_text(host) && userinfo == NULL && parsed_port <= 65535 &&
                   port_allowed(port, profile) &&
                   (!require_endpoint ||
                    exact_member(text, profile->allowed_endpoints,
                                 profile->allowed_endpoint_count));
  if (valid) {
    GInetAddress *literal = g_inet_address_new_from_string(host);
    if (literal != NULL) {
      valid = research_policy_address_is_public(host);
      g_object_unref(literal);
    }
  }
  if (!valid) {
    g_uri_unref(uri);
    policy_error(error, "Destination hors profil réseau ou non publique.");
    return FALSE;
  }
  const char *path = g_uri_get_path(uri);
  const char *query = g_uri_get_query(uri);
  out->scheme = g_ascii_strdown(scheme, -1);
  out->host = g_ascii_strdown(host, -1);
  out->port = port;
  out->path_and_query = query != NULL
                            ? g_strdup_printf("%s?%s", path[0] ? path : "/", query)
                            : g_strdup(path[0] ? path : "/");
  g_uri_unref(uri);
  return TRUE;
}

static gboolean subject_matches_host(const char *subject,
                                     const ResearchHttpTarget *target) {
  if (!clean_text(subject))
    return FALSE;
  if (g_str_has_prefix(subject, "http://") ||
      g_str_has_prefix(subject, "https://")) {
    GUri *uri = g_uri_parse(subject, G_URI_FLAGS_NONE, NULL);
    const char *host = uri != NULL ? g_uri_get_host(uri) : NULL;
    gboolean match = host != NULL && g_uri_get_userinfo(uri) == NULL &&
                     g_ascii_strcasecmp(host, target->host) == 0;
    if (uri != NULL)
      g_uri_unref(uri);
    return match;
  }
  return strchr(subject, '/') == NULL && strchr(subject, '@') == NULL &&
         g_ascii_strcasecmp(subject, target->host) == 0;
}

gboolean research_policy_validate_http_action(
    const ResearchAction *action, const ScopeGrant *grant,
    const ResearchNetworkProfile *profile, ResearchHttpTarget *out_target,
    GError **error) {
  if (action == NULL || grant == NULL || out_target == NULL ||
      !research_scope_grant_validate(grant, error))
    return FALSE;
  gboolean selected = FALSE;
  for (gsize i = 0; i < grant->selected_action_count; i++)
    selected |= g_strcmp0(action->action_id, grant->selected_action_ids[i]) == 0;
  for (gsize i = 0; i < grant->excluded_subject_count; i++)
    if (g_strcmp0(action->subject, grant->excluded_subjects[i]) == 0) {
      policy_error(error, "Sujet explicitement exclu du ScopeGrant.");
      return FALSE;
    }
  if (!selected || action->max_requests == 0 ||
      action->max_requests > grant->max_requests ||
      action->max_response_bytes == 0 ||
      action->max_response_bytes > grant->max_response_bytes ||
      action->max_active_ms == 0 ||
      action->max_active_ms > grant->max_active_ms) {
    policy_error(error, "Action absente du grant ou budget hors grant.");
    return FALSE;
  }
  ResearchHttpTarget parsed = {0};
  if (!parse_http_target(action->endpoint, profile, TRUE, &parsed, error))
    return FALSE;
  if (!subject_matches_host(action->subject, &parsed)) {
    research_http_target_clear(&parsed);
    policy_error(error, "Le sujet ne correspond pas exactement à la destination.");
    return FALSE;
  }
  *out_target = parsed;
  return TRUE;
}

gboolean research_policy_address_is_public(const char *address) {
  GInetAddress *inet = g_inet_address_new_from_string(address);
  if (inet == NULL)
    return FALSE;
  const guint8 *bytes = g_inet_address_to_bytes(inet);
  gboolean public = TRUE;
  if (g_inet_address_get_family(inet) == G_SOCKET_FAMILY_IPV4) {
    /* CONTRACT: seules les adresses global-unicast sont admissibles. Les
     * préfixes de documentation et d'essai ne deviennent jamais des cibles
     * réseau parce qu'ils sont syntaxiquement valides. */
    public = !(bytes[0] == 0 || bytes[0] == 10 || bytes[0] == 127 ||
               (bytes[0] == 169 && bytes[1] == 254) ||
               (bytes[0] == 172 && bytes[1] >= 16 && bytes[1] <= 31) ||
               (bytes[0] == 192 && bytes[1] == 0 && bytes[2] == 0) ||
               (bytes[0] == 192 && bytes[1] == 0 && bytes[2] == 2) ||
               (bytes[0] == 192 && bytes[1] == 88 && bytes[2] == 99) ||
               (bytes[0] == 192 && bytes[1] == 168) ||
               (bytes[0] == 100 && bytes[1] >= 64 && bytes[1] <= 127) ||
               (bytes[0] == 198 && (bytes[1] == 18 || bytes[1] == 19)) ||
               (bytes[0] == 198 && bytes[1] == 51 && bytes[2] == 100) ||
               (bytes[0] == 203 && bytes[1] == 0 && bytes[2] == 113) ||
               bytes[0] >= 224);
  } else {
    const struct in6_addr *ipv6 = (const struct in6_addr *)bytes;
    if (IN6_IS_ADDR_V4MAPPED(ipv6)) {
      char *mapped = g_strdup_printf("%u.%u.%u.%u", bytes[12], bytes[13],
                                     bytes[14], bytes[15]);
      public = research_policy_address_is_public(mapped);
      g_free(mapped);
    } else {
      public = !(IN6_IS_ADDR_UNSPECIFIED(ipv6) ||
                 IN6_IS_ADDR_LOOPBACK(ipv6) ||
                 (bytes[0] == 0x00 && bytes[1] == 0x64 &&
                  bytes[2] == 0xff && bytes[3] == 0x9b &&
                  bytes[4] == 0x00 && bytes[5] == 0x01) ||
                 (bytes[0] == 0x01 && bytes[1] == 0x00 &&
                  bytes[2] == 0x00 && bytes[3] == 0x00 &&
                  bytes[4] == 0x00 && bytes[5] == 0x00 &&
                  bytes[6] == 0x00 && bytes[7] == 0x00) ||
                 (bytes[0] == 0x20 && bytes[1] == 0x01 &&
                  bytes[2] == 0x00 && bytes[3] == 0x02 &&
                  bytes[4] == 0x00 && bytes[5] == 0x00) ||
                 (bytes[0] == 0x20 && bytes[1] == 0x01 &&
                  bytes[2] == 0x0d && bytes[3] == 0xb8) ||
                 (bytes[0] == 0x20 && bytes[1] == 0x01 &&
                  bytes[2] == 0x00 &&
                  ((bytes[3] & 0xf0) == 0x10 ||
                   (bytes[3] & 0xf0) == 0x20)) ||
                 (bytes[0] == 0x20 && bytes[1] == 0x02) ||
                 (bytes[0] == 0x3f && bytes[1] == 0xff &&
                  (bytes[2] & 0xf0) == 0x00) ||
                 (bytes[0] == 0x5f && bytes[1] == 0x00) ||
                 (bytes[0] == 0xfc || bytes[0] == 0xfd) ||
                 (bytes[0] == 0xfe && (bytes[1] & 0x80) == 0x80) ||
                 bytes[0] == 0xff);
    }
  }
  g_object_unref(inet);
  return public;
}

ResearchRedirectDecision research_policy_check_redirect(
    const char *location, const ResearchNetworkProfile *profile,
    ResearchHttpTarget *out_target, GError **error) {
  if (!parse_http_target(location, profile, FALSE, out_target, error))
    return RESEARCH_REDIRECT_INVALID;
  return RESEARCH_REDIRECT_REQUIRES_DECISION;
}

void research_http_target_clear(ResearchHttpTarget *target) {
  if (target == NULL)
    return;
  g_clear_pointer(&target->scheme, g_free);
  g_clear_pointer(&target->host, g_free);
  g_clear_pointer(&target->path_and_query, g_free);
  target->port = 0;
}
