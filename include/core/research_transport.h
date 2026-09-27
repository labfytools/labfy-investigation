/******************************************************************************
 * @file research_transport.h
 * @brief Transport borné et sans autorité implicite pour la recherche V1.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_RESEARCH_TRANSPORT_H
#define LABFY_INVESTIGATION_RESEARCH_TRANSPORT_H

#include "core/research_policy.h"

G_BEGIN_DECLS

typedef enum {
  RESEARCH_TRANSPORT_OK,
  RESEARCH_TRANSPORT_UNAVAILABLE,
  RESEARCH_TRANSPORT_CANCELLED,
  RESEARCH_TRANSPORT_TIMEOUT,
  RESEARCH_TRANSPORT_RATE_LIMITED,
  RESEARCH_TRANSPORT_FORBIDDEN,
  RESEARCH_TRANSPORT_REDIRECT,
  RESEARCH_TRANSPORT_TOO_LARGE,
  RESEARCH_TRANSPORT_INVALID_RESPONSE,
  RESEARCH_TRANSPORT_FAILED
} ResearchTransportState;

typedef struct {
  const char *name;
  const char *value;
} ResearchHeader;

typedef struct {
  const ResearchAction *action;
  const ScopeGrant *grant;
  const ResearchNetworkProfile *profile;
  const char *method;
  const ResearchHeader *headers;
  gsize header_count;
  guint64 max_compressed_bytes;
  guint64 max_decompressed_bytes;
  guint64 max_header_bytes;
  GCancellable *cancellable;
} ResearchTransportRequest;

typedef struct {
  ResearchTransportState state;
  guint status_code;
  guint retry_after_seconds;
  gboolean retry_after_present;
  gboolean body_is_compressed;
  GBytes *body;
  GHashTable *headers;
  char *redirect_location;
  char *diagnostic;
} ResearchTransportReply;

/**
 * CONTRACT: la production n'utilise ni proxy, cookies, netrc, auth ambiante,
 * redirection automatique, ni résolution fournie par l'appelant.
 * CURRENT: aucun backend réseau n'est lié; retourne UNAVAILABLE sans contact.
 */
gboolean research_transport_execute(const ResearchTransportRequest *request,
                                    ResearchTransportReply *out_reply,
                                    GError **error);

gboolean research_transport_request_validate(
    const ResearchTransportRequest *request, ResearchHttpTarget *out_target,
    GError **error);
void research_transport_reply_clear(ResearchTransportReply *reply);
const char *research_transport_state_code(ResearchTransportState state);

#ifdef RESEARCH_TRANSPORT_ENABLE_TEST_HOOKS
typedef struct {
  guint status_code;
  const ResearchHeader *headers;
  gsize header_count;
  const guint8 *body;
  gsize body_size;
  gboolean compressed;
  guint64 decompressed_size;
  const char *peer_address;
  const char *redirect_location;
  gboolean timed_out;
  gboolean unavailable;
} ResearchTransportFixture;

/** TEST-ONLY: réponse finie injectée, sans socket ni résolution réseau. */
gboolean research_transport_execute_fixture(
    const ResearchTransportRequest *request,
    const ResearchTransportFixture *fixture,
    const char *const *loopback_authorities, gsize authority_count,
    ResearchTransportReply *out_reply, GError **error);
#endif

G_END_DECLS
#endif
