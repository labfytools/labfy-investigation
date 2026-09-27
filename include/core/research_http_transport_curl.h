#ifndef LABFY_INVESTIGATION_RESEARCH_HTTP_TRANSPORT_CURL_H
#define LABFY_INVESTIGATION_RESEARCH_HTTP_TRANSPORT_CURL_H

#include "core/research_transport.h"

G_BEGIN_DECLS

/**
 * CONTRACT: exécute une requête préalablement validée, sans état ambiant.
 * INVARIANT: le callback socket refuse toute adresse qui n'est pas publique.
 */
gboolean research_http_transport_curl_execute(
    const ResearchTransportRequest *request, ResearchTransportReply *out_reply,
    GError **error);

#ifdef RESEARCH_TRANSPORT_ENABLE_TEST_HOOKS
/**
 * TEST-ONLY: route une autorité exacte vers une adresse loopback finie.
 * Cette fonction n'existe pas dans l'API de production.
 */
gboolean research_http_transport_curl_execute_fixture(
    const ResearchTransportRequest *request, const char *authority,
    const char *loopback_address, ResearchTransportReply *out_reply,
    GError **error);
#endif

G_END_DECLS
#endif
