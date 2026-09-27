/******************************************************************************
 * @file research_transport.c
 * @brief Validation et classification d'un transport de recherche borné.
 ******************************************************************************/
#include "core/research_transport.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static void transport_error(GError **error, GIOErrorEnum code,
                            const char *message) {
  if (error != NULL && *error == NULL)
    g_set_error_literal(error, G_IO_ERROR, code, message);
}

static gboolean header_text_valid(const char *value) {
  if (value == NULL)
    return FALSE;
  for (const unsigned char *p = (const unsigned char *)value; *p != '\0'; p++)
    if (*p < 0x20 || *p == 0x7f)
      return FALSE;
  return TRUE;
}

static gboolean request_header_allowed(const ResearchHeader *header) {
  static const char *const allowed[] = {
      "accept", "accept-language", "user-agent", "if-none-match",
      "if-modified-since"};
  if (header == NULL || !header_text_valid(header->name) ||
      !header_text_valid(header->value))
    return FALSE;
  for (gsize i = 0; i < G_N_ELEMENTS(allowed); i++)
    if (g_ascii_strcasecmp(header->name, allowed[i]) == 0)
      return TRUE;
  return FALSE;
}

gboolean research_transport_request_validate(
    const ResearchTransportRequest *request, ResearchHttpTarget *out_target,
    GError **error) {
  if (request == NULL || request->action == NULL || out_target == NULL ||
      g_strcmp0(request->method, "GET") != 0 ||
      request->max_compressed_bytes == 0 ||
      request->max_decompressed_bytes == 0 ||
      request->max_header_bytes == 0 ||
      (request->header_count > 0 && request->headers == NULL) ||
      request->max_compressed_bytes > request->action->max_response_bytes ||
      request->max_decompressed_bytes > request->action->max_response_bytes) {
    transport_error(error, G_IO_ERROR_INVALID_ARGUMENT,
                    "Requête de transport non bornée ou invalide.");
    return FALSE;
  }
  guint64 header_bytes = 0;
  for (gsize i = 0; i < request->header_count; i++) {
    if (!request_header_allowed(&request->headers[i])) {
      transport_error(error, G_IO_ERROR_INVALID_ARGUMENT,
                      "En-tête de requête interdit.");
      return FALSE;
    }
    header_bytes += strlen(request->headers[i].name) +
                    strlen(request->headers[i].value) + 4;
  }
  if (header_bytes > request->max_header_bytes) {
    transport_error(error, G_IO_ERROR_NO_SPACE, "En-têtes trop volumineux.");
    return FALSE;
  }
  return research_policy_validate_http_action(
      request->action, request->grant, request->profile, out_target, error);
}

gboolean research_transport_execute(const ResearchTransportRequest *request,
                                    ResearchTransportReply *out_reply,
                                    GError **error) {
  ResearchHttpTarget target = {0};
  if (out_reply == NULL ||
      !research_transport_request_validate(request, &target, error))
    return FALSE;
  research_http_target_clear(&target);
  *out_reply = (ResearchTransportReply){
      .state = RESEARCH_TRANSPORT_UNAVAILABLE,
      .diagnostic = g_strdup("Transport réseau non configuré.")};
  return TRUE;
}

#ifdef RESEARCH_TRANSPORT_ENABLE_TEST_HOOKS
static guint retry_after_parse(const char *value, gboolean *present) {
  if (value == NULL) {
    *present = FALSE;
    return 0;
  }
  errno = 0;
  char *end = NULL;
  guint64 seconds = g_ascii_strtoull(value, &end, 10);
  if (errno != 0 || end == value || *end != '\0' || seconds > G_MAXUINT) {
    *present = FALSE;
    return 0;
  }
  *present = TRUE;
  return (guint)seconds;
}

static ResearchTransportState status_state(guint status) {
  if (status >= 200 && status < 300)
    return RESEARCH_TRANSPORT_OK;
  if (status >= 300 && status < 400)
    return RESEARCH_TRANSPORT_REDIRECT;
  if (status == 403)
    return RESEARCH_TRANSPORT_FORBIDDEN;
  if (status == 429)
    return RESEARCH_TRANSPORT_RATE_LIMITED;
  return RESEARCH_TRANSPORT_FAILED;
}

static gboolean fixture_loopback_allowed(const char *host,
                                         const char *const *authorities,
                                         gsize count) {
  for (gsize i = 0; i < count; i++)
    if (g_ascii_strcasecmp(host, authorities[i]) == 0)
      return TRUE;
  return FALSE;
}

gboolean research_transport_execute_fixture(
    const ResearchTransportRequest *request,
    const ResearchTransportFixture *fixture,
    const char *const *loopback_authorities, gsize authority_count,
    ResearchTransportReply *out_reply, GError **error) {
  ResearchHttpTarget target = {0};
  if (fixture == NULL || out_reply == NULL || fixture->peer_address == NULL ||
      (fixture->header_count > 0 && fixture->headers == NULL) ||
      (fixture->body_size > 0 && fixture->body == NULL) ||
      !research_transport_request_validate(request, &target, error))
    return FALSE;
  GInetAddress *peer = g_inet_address_new_from_string(fixture->peer_address);
  gboolean peer_ok = peer != NULL &&
      (research_policy_address_is_public(fixture->peer_address) ||
       (g_inet_address_get_is_loopback(peer) &&
        fixture_loopback_allowed(target.host, loopback_authorities,
                                 authority_count)));
  g_clear_object(&peer);
  if (!peer_ok) {
    research_http_target_clear(&target);
    transport_error(error, G_IO_ERROR_PERMISSION_DENIED,
                    "Adresse de connexion interdite avant contact.");
    return FALSE;
  }
  ResearchTransportReply reply = {0};
  reply.status_code = fixture->status_code;
  reply.state = fixture->unavailable ? RESEARCH_TRANSPORT_UNAVAILABLE
      : fixture->timed_out ? RESEARCH_TRANSPORT_TIMEOUT
      : status_state(fixture->status_code);
  if (request->cancellable != NULL &&
      g_cancellable_is_cancelled(request->cancellable))
    reply.state = RESEARCH_TRANSPORT_CANCELLED;
  guint64 header_bytes = 0;
  reply.headers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  for (gsize i = 0; i < fixture->header_count; i++) {
    const ResearchHeader *header = &fixture->headers[i];
    if (!header_text_valid(header->name) || !header_text_valid(header->value)) {
      reply.state = RESEARCH_TRANSPORT_INVALID_RESPONSE;
      break;
    }
    header_bytes += strlen(header->name) + strlen(header->value) + 4;
    char *name = g_ascii_strdown(header->name, -1);
    g_hash_table_replace(reply.headers, name, g_strdup(header->value));
  }
  if (header_bytes > request->max_header_bytes ||
      fixture->body_size > request->max_compressed_bytes ||
      fixture->decompressed_size > request->max_decompressed_bytes)
    reply.state = RESEARCH_TRANSPORT_TOO_LARGE;
  const char *retry = g_hash_table_lookup(reply.headers, "retry-after");
  reply.retry_after_seconds = retry_after_parse(retry, &reply.retry_after_present);
  reply.body_is_compressed = fixture->compressed;
  if (reply.state != RESEARCH_TRANSPORT_TOO_LARGE)
    reply.body = g_bytes_new(fixture->body, fixture->body_size);
  if (reply.state == RESEARCH_TRANSPORT_REDIRECT) {
    ResearchHttpTarget redirected = {0};
    if (fixture->redirect_location == NULL ||
        research_policy_check_redirect(fixture->redirect_location,
                                       request->profile, &redirected,
                                       NULL) !=
            RESEARCH_REDIRECT_REQUIRES_DECISION)
      reply.state = RESEARCH_TRANSPORT_INVALID_RESPONSE;
    else
      reply.redirect_location = g_strdup(fixture->redirect_location);
    research_http_target_clear(&redirected);
  }
  research_http_target_clear(&target);
  *out_reply = reply;
  return TRUE;
}
#endif

void research_transport_reply_clear(ResearchTransportReply *reply) {
  if (reply == NULL)
    return;
  g_clear_pointer(&reply->body, g_bytes_unref);
  g_clear_pointer(&reply->headers, g_hash_table_unref);
  g_clear_pointer(&reply->redirect_location, g_free);
  g_clear_pointer(&reply->diagnostic, g_free);
  *reply = (ResearchTransportReply){0};
}

const char *research_transport_state_code(ResearchTransportState state) {
  static const char *const codes[] = {
      "OK", "UNAVAILABLE", "CANCELLED", "TIMEOUT", "RATE_LIMITED",
      "FORBIDDEN", "REDIRECT_REQUIRES_DECISION", "TOO_LARGE",
      "INVALID_RESPONSE", "FAILED"};
  return state <= RESEARCH_TRANSPORT_FAILED ? codes[state] : "FAILED";
}
