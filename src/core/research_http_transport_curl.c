#include "core/research_http_transport_curl.h"

#include <arpa/inet.h>
#include <curl/curl.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct {
  const ResearchTransportRequest *request;
  ResearchTransportReply *reply;
  GByteArray *body;
  guint64 header_bytes;
  gboolean too_large;
  gboolean allow_loopback;
} CurlContext;

static gboolean socket_address_public(const struct curl_sockaddr *address,
                                      gboolean allow_loopback) {
  char text[INET6_ADDRSTRLEN] = {0};
  const void *source = NULL;
  if (address->family == AF_INET)
    source = &((const struct sockaddr_in *)&address->addr)->sin_addr;
  else if (address->family == AF_INET6)
    source = &((const struct sockaddr_in6 *)&address->addr)->sin6_addr;
  if (source == NULL || inet_ntop(address->family, source, text, sizeof text) == NULL)
    return FALSE;
  GInetAddress *inet = g_inet_address_new_from_string(text);
  gboolean loopback = inet != NULL && g_inet_address_get_is_loopback(inet);
  g_clear_object(&inet);
  return research_policy_address_is_public(text) || (allow_loopback && loopback);
}

static curl_socket_t open_socket(void *data, curlsocktype purpose,
                                 struct curl_sockaddr *address) {
  CurlContext *context = data;
  if (purpose != CURLSOCKTYPE_IPCXN ||
      !socket_address_public(address, context->allow_loopback))
    return CURL_SOCKET_BAD;
  return socket(address->family, address->socktype, address->protocol);
}

static size_t write_body(char *data, size_t size, size_t count, void *user_data) {
  CurlContext *context = user_data;
  gsize length = size * count;
  if (length > context->request->max_decompressed_bytes - context->body->len) {
    context->too_large = TRUE;
    return 0;
  }
  g_byte_array_append(context->body, (const guint8 *)data, length);
  return length;
}

static void store_header(CurlContext *context, const char *line, gsize length) {
  const char *colon = memchr(line, ':', length);
  if (colon == NULL)
    return;
  gsize name_length = (gsize)(colon - line);
  const char *value = colon + 1;
  const char *end = line + length;
  while (value < end && g_ascii_isspace(*value))
    value++;
  while (end > value && g_ascii_isspace(end[-1]))
    end--;
  char *name = g_ascii_strdown(line, name_length);
  char *copy = g_strndup(value, (gsize)(end - value));
  g_hash_table_replace(context->reply->headers, name, copy);
}

static size_t write_header(char *data, size_t size, size_t count,
                           void *user_data) {
  CurlContext *context = user_data;
  gsize length = size * count;
  if (length > context->request->max_header_bytes - context->header_bytes) {
    context->too_large = TRUE;
    return 0;
  }
  context->header_bytes += length;
  store_header(context, data, length);
  return length;
}

static int progress_cancel(void *data, curl_off_t down_total,
                           curl_off_t down_now, curl_off_t up_total,
                           curl_off_t up_now) {
  (void)down_total;
  (void)up_total;
  (void)up_now;
  CurlContext *context = data;
  /* CONTRACT: dlnow est le compteur libcurl des octets téléchargés avant
   * décompression. Il rend le plafond compressé opposable même sans
   * Content-Length (notamment en transfert chunked). */
  if (down_now < 0 || (guint64)down_now >
                          context->request->max_compressed_bytes) {
    context->too_large = TRUE;
    return 1;
  }
  return context->request->cancellable != NULL &&
         g_cancellable_is_cancelled(context->request->cancellable);
}

static ResearchTransportState response_state(CURLcode code, long status,
                                              const CurlContext *context) {
  if (context->request->cancellable != NULL &&
      g_cancellable_is_cancelled(context->request->cancellable))
    return RESEARCH_TRANSPORT_CANCELLED;
  if (context->too_large)
    return RESEARCH_TRANSPORT_TOO_LARGE;
  if (code == CURLE_FILESIZE_EXCEEDED)
    return RESEARCH_TRANSPORT_TOO_LARGE;
  if (code == CURLE_OPERATION_TIMEDOUT)
    return RESEARCH_TRANSPORT_TIMEOUT;
  if (code != CURLE_OK)
    return RESEARCH_TRANSPORT_FAILED;
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

static void parse_reply_headers(ResearchTransportReply *reply) {
  const char *retry = g_hash_table_lookup(reply->headers, "retry-after");
  if (retry != NULL) {
    char *end = NULL;
    guint64 seconds = g_ascii_strtoull(retry, &end, 10);
    if (end != retry && *end == '\0' && seconds <= G_MAXUINT) {
      reply->retry_after_present = TRUE;
      reply->retry_after_seconds = (guint)seconds;
    }
  }
  const char *location = g_hash_table_lookup(reply->headers, "location");
  if (reply->state == RESEARCH_TRANSPORT_REDIRECT && location != NULL)
    reply->redirect_location = g_strdup(location);
}

static gboolean execute_internal(const ResearchTransportRequest *request,
                                 const char *resolve_entry,
                                 gboolean allow_loopback,
                                 ResearchTransportReply *out_reply,
                                 GError **error) {
  ResearchHttpTarget target = {0};
  if (!research_transport_request_validate(request, &target, error))
    return FALSE;
  CURL *curl = curl_easy_init();
  if (curl == NULL) {
    research_http_target_clear(&target);
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                        "Initialisation libcurl impossible.");
    return FALSE;
  }
  ResearchTransportReply reply = {0};
  reply.headers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  CurlContext context = {request, &reply, g_byte_array_new(), 0, FALSE,
                         allow_loopback};
  struct curl_slist *headers = NULL;
  for (gsize i = 0; i < request->header_count; i++) {
    char *line = g_strdup_printf("%s: %s", request->headers[i].name,
                                 request->headers[i].value);
    headers = curl_slist_append(headers, line);
    g_free(line);
  }
  struct curl_slist *resolve = NULL;
  if (resolve_entry != NULL)
    resolve = curl_slist_append(resolve, resolve_entry);
  curl_easy_setopt(curl, CURLOPT_URL, request->action->endpoint);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
  curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
  curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
  curl_easy_setopt(curl, CURLOPT_PROXY, "");
  curl_easy_setopt(curl, CURLOPT_NOPROXY, "*");
  curl_easy_setopt(curl, CURLOPT_NETRC, CURL_NETRC_IGNORED);
  curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_NONE);
  curl_easy_setopt(curl, CURLOPT_UNRESTRICTED_AUTH, 0L);
  curl_easy_setopt(curl, CURLOPT_COOKIEFILE, NULL);
  curl_easy_setopt(curl, CURLOPT_COOKIEJAR, NULL);
  curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)request->action->max_active_ms);
  curl_easy_setopt(curl, CURLOPT_MAXFILESIZE_LARGE,
                   (curl_off_t)request->max_compressed_bytes);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &context);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_header);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &context);
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_cancel);
  curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &context);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
  curl_easy_setopt(curl, CURLOPT_OPENSOCKETFUNCTION, open_socket);
  curl_easy_setopt(curl, CURLOPT_OPENSOCKETDATA, &context);
  if (resolve != NULL)
    curl_easy_setopt(curl, CURLOPT_RESOLVE, resolve);
  CURLcode code = curl_easy_perform(curl);
  long status = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  reply.status_code = status >= 0 && status <= G_MAXUINT ? (guint)status : 0;
  reply.state = response_state(code, status, &context);
  if (reply.state != RESEARCH_TRANSPORT_TOO_LARGE)
    reply.body = g_byte_array_free_to_bytes(context.body);
  else
    g_byte_array_unref(context.body);
  parse_reply_headers(&reply);
  curl_slist_free_all(headers);
  curl_slist_free_all(resolve);
  curl_easy_cleanup(curl);
  research_http_target_clear(&target);
  *out_reply = reply;
  return TRUE;
}

gboolean research_http_transport_curl_execute(
    const ResearchTransportRequest *request, ResearchTransportReply *out_reply,
    GError **error) {
  return execute_internal(request, NULL, FALSE, out_reply, error);
}

#ifdef RESEARCH_TRANSPORT_ENABLE_TEST_HOOKS
gboolean research_http_transport_curl_execute_fixture(
    const ResearchTransportRequest *request, const char *authority,
    const char *loopback_address, ResearchTransportReply *out_reply,
    GError **error) {
  ResearchHttpTarget target = {0};
  if (authority == NULL || loopback_address == NULL ||
      !research_transport_request_validate(request, &target, error))
    return FALSE;
  GInetAddress *loopback = g_inet_address_new_from_string(loopback_address);
  gboolean valid = loopback != NULL && g_inet_address_get_is_loopback(loopback) &&
                   g_ascii_strcasecmp(authority, target.host) == 0;
  g_clear_object(&loopback);
  if (!valid) {
    research_http_target_clear(&target);
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
                        "Autorité loopback de fixture interdite.");
    return FALSE;
  }
  char *resolve = g_strdup_printf("%s:%u:%s", authority, target.port,
                                  loopback_address);
  research_http_target_clear(&target);
  gboolean ok = execute_internal(request, resolve, TRUE, out_reply, error);
  g_free(resolve);
  return ok;
}
#endif
