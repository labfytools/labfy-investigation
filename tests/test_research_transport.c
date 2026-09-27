#include "core/research_transport.h"
#include "core/research_http_transport_curl.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

static const char *selected[] = {"11111111-1111-4111-8111-111111111111"};
static const guint16 ports[] = {443};
static const char *endpoints[] = {"https://example.org/specimen"};
static ResearchNetworkProfile profile = {ports, 1, endpoints, 1};
static ResearchAction action = {"11111111-1111-4111-8111-111111111111",
  "page.fetch","specimen","https://example.org/specimen","example.org",
  RESEARCH_CONTACT_TARGET,"host",1,4096,1000};
static ScopeGrant grant = {RESEARCH_GRANT_CONTRACT,
  "22222222-2222-4222-8222-222222222222","33333333-3333-4333-8333-333333333333",
  "key","aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
  "2026-01-01T00:00:00Z","2027-01-01T00:00:00Z",selected,1,NULL,0,1,4096,1000};

static ResearchTransportRequest request(void) {
  return (ResearchTransportRequest){&action,&grant,&profile,"GET",NULL,0,
                                    2048,4096,512,NULL};
}

static void test_production_unavailable(void) {
  ResearchTransportRequest req=request(); ResearchTransportReply reply={0};
  g_assert_true(research_transport_execute(&req,&reply,NULL));
  g_assert_cmpint(reply.state,==,RESEARCH_TRANSPORT_UNAVAILABLE);
  research_transport_reply_clear(&reply);
}

static void test_headers_and_fixture_states(void) {
  ResearchHeader forbidden={"Cookie","secret=forbidden"};
  ResearchTransportRequest req=request();req.headers=&forbidden;req.header_count=1;
  ResearchHttpTarget target={0};
  g_assert_false(research_transport_request_validate(&req,&target,NULL));
  req=request();
  ResearchHeader headers[]={{"Retry-After","17"}};
  ResearchTransportFixture fixture={429,headers,1,(const guint8 *)"busy",4,
    FALSE,4,"127.0.0.1",NULL,FALSE,FALSE};
  const char *authorities[]={"example.org"};ResearchTransportReply reply={0};
  g_assert_true(research_transport_execute_fixture(&req,&fixture,authorities,1,&reply,NULL));
  g_assert_cmpint(reply.state,==,RESEARCH_TRANSPORT_RATE_LIMITED);
  g_assert_true(reply.retry_after_present);g_assert_cmpuint(reply.retry_after_seconds,==,17);
  research_transport_reply_clear(&reply);
  fixture.status_code=403;fixture.headers=NULL;fixture.header_count=0;
  g_assert_true(research_transport_execute_fixture(&req,&fixture,authorities,1,&reply,NULL));
  g_assert_cmpint(reply.state,==,RESEARCH_TRANSPORT_FORBIDDEN);
  research_transport_reply_clear(&reply);
}

static void test_bounds_cancel_redirect(void) {
  ResearchTransportRequest req=request();ResearchTransportReply reply={0};
  ResearchTransportFixture fixture={200,NULL,0,(const guint8 *)"x",1,FALSE,
    5000,"127.0.0.1",NULL,FALSE,FALSE};const char *authorities[]={"example.org"};
  g_assert_true(research_transport_execute_fixture(&req,&fixture,authorities,1,&reply,NULL));
  g_assert_cmpint(reply.state,==,RESEARCH_TRANSPORT_TOO_LARGE);research_transport_reply_clear(&reply);
  fixture.decompressed_size=1;fixture.status_code=302;fixture.redirect_location="https://example.net/next";
  g_assert_true(research_transport_execute_fixture(&req,&fixture,authorities,1,&reply,NULL));
  g_assert_cmpint(reply.state,==,RESEARCH_TRANSPORT_REDIRECT);research_transport_reply_clear(&reply);
  GCancellable *cancel=g_cancellable_new();g_cancellable_cancel(cancel);req.cancellable=cancel;
  fixture.status_code=200;fixture.redirect_location=NULL;
  g_assert_true(research_transport_execute_fixture(&req,&fixture,authorities,1,&reply,NULL));
  g_assert_cmpint(reply.state,==,RESEARCH_TRANSPORT_CANCELLED);research_transport_reply_clear(&reply);g_object_unref(cancel);
}

typedef struct {
  int listener;
  const guint8 *response;
  gsize response_size;
  char *request_log;
} LocalServer;

static gpointer serve_once(gpointer data) {
  LocalServer *server = data;
  int client = accept(server->listener, NULL, NULL);
  g_assert_cmpint(client, >=, 0);
  GString *log = g_string_new(NULL);
  char buffer[512];
  while (strstr(log->str, "\r\n\r\n") == NULL) {
    ssize_t count = recv(client, buffer, sizeof buffer, 0);
    if (count <= 0)
      break;
    g_string_append_len(log, buffer, count);
  }
  server->request_log = g_string_free(log, FALSE);
  gsize remaining = server->response_size;
  const guint8 *cursor = server->response;
  while (remaining > 0) {
    ssize_t count = send(client, cursor, remaining, 0);
    if (count <= 0)
      break;
    cursor += count;
    remaining -= (gsize)count;
  }
  close(client);
  close(server->listener);
  return NULL;
}

static guint16 local_server_start_bytes(LocalServer *server,
                                        const guint8 *response,
                                        gsize response_size,
                                        GThread **thread) {
  server->listener = socket(AF_INET, SOCK_STREAM, 0);
  g_assert_cmpint(server->listener, >=, 0);
  struct sockaddr_in address = {.sin_family = AF_INET,
                                .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
                                .sin_port = 0};
  g_assert_cmpint(bind(server->listener, (struct sockaddr *)&address,
                       sizeof address), ==, 0);
  g_assert_cmpint(listen(server->listener, 1), ==, 0);
  socklen_t length = sizeof address;
  g_assert_cmpint(getsockname(server->listener, (struct sockaddr *)&address,
                             &length), ==, 0);
  server->response = response;
  server->response_size = response_size;
  *thread = g_thread_new("research-http-fixture", serve_once, server);
  return ntohs(address.sin_port);
}

static guint16 local_server_start(LocalServer *server, const char *response,
                                  GThread **thread) {
  return local_server_start_bytes(server, (const guint8 *)response,
                                  strlen(response), thread);
}

static void test_curl_local_fixture(void) {
  LocalServer server = {0};
  GThread *thread = NULL;
  guint16 port = local_server_start(
      &server, "HTTP/1.1 200 OK\r\nContent-Length: 7\r\n\r\nspecimen", &thread);
  char *endpoint = g_strdup_printf("http://fixture.test:%u/specimen", port);
  const char *allowed_endpoints[] = {endpoint};
  guint16 allowed_ports[] = {port};
  ResearchNetworkProfile local_profile = {allowed_ports, 1,
                                          allowed_endpoints, 1};
  ResearchAction local_action = action;
  local_action.endpoint = endpoint;
  local_action.subject = "fixture.test";
  ResearchHeader headers[] = {{"Accept", "text/plain"}};
  ResearchTransportRequest req = {&local_action, &grant, &local_profile, "GET",
      headers, 1, 64, 64, 256, NULL};
  ResearchTransportReply reply = {0};
  g_assert_true(research_http_transport_curl_execute_fixture(
      &req, "fixture.test", "127.0.0.1", &reply, NULL));
  g_assert_cmpint(reply.state, ==, RESEARCH_TRANSPORT_OK);
  g_assert_cmpuint(g_bytes_get_size(reply.body), ==, 7);
  g_thread_join(thread);
  g_assert_nonnull(strstr(server.request_log, "GET /specimen HTTP/1.1"));
  g_assert_null(strstr(server.request_log, "Authorization:"));
  g_assert_null(strstr(server.request_log, "Cookie:"));
  g_assert_null(strstr(server.request_log, "Proxy-Authorization:"));
  g_free(server.request_log);
  research_transport_reply_clear(&reply);
  g_free(endpoint);
}

static void test_curl_body_limit(void) {
  LocalServer server = {0};
  GThread *thread = NULL;
  guint16 port = local_server_start(&server,
      "HTTP/1.1 200 OK\r\nContent-Length: 12\r\n\r\nabcdefghijkl", &thread);
  char *endpoint = g_strdup_printf("http://fixture.test:%u/large", port);
  const char *allowed_endpoints[] = {endpoint};
  guint16 allowed_ports[] = {port};
  ResearchNetworkProfile local_profile = {allowed_ports, 1, allowed_endpoints, 1};
  ResearchAction local_action = action;
  local_action.endpoint = endpoint;
  local_action.subject = "fixture.test";
  ResearchTransportRequest req = {&local_action, &grant, &local_profile, "GET",
                                   NULL, 0, 8, 8, 256, NULL};
  ResearchTransportReply reply = {0};
  g_assert_true(research_http_transport_curl_execute_fixture(
      &req, "fixture.test", "127.0.0.1", &reply, NULL));
  g_assert_cmpint(reply.state, ==, RESEARCH_TRANSPORT_TOO_LARGE);
  g_thread_join(thread);
  g_free(server.request_log);
  research_transport_reply_clear(&reply);
  g_free(endpoint);
}

static void test_curl_chunked_compressed_limit(void) {
  static const guint8 response[] = {
      'H','T','T','P','/','1','.','1',' ','2','0','0',' ','O','K','\r','\n',
      'C','o','n','t','e','n','t','-','E','n','c','o','d','i','n','g',':',' ',
      'g','z','i','p','\r','\n','T','r','a','n','s','f','e','r','-','E','n',
      'c','o','d','i','n','g',':',' ','c','h','u','n','k','e','d','\r','\n',
      '\r','\n','2','8','\r','\n',
      0x1f,0x8b,0x08,0x00,0x00,0x00,0x00,0x00,0x02,0xff,
      0xed,0xc1,0x01,0x0d,0x00,0x00,0x00,0xc2,0xa0,0x6c,
      0xef,0x5f,0xca,0x1e,0x0e,0x28,0x00,0x00,0x00,0xe0,
      0xdd,0x00,0x40,0x34,0xa6,0xfe,0x00,0x10,0x00,0x00,
      '\r','\n','0','\r','\n','\r','\n'};
  LocalServer server = {0};
  GThread *thread = NULL;
  guint16 port = local_server_start_bytes(&server, response, sizeof response,
                                           &thread);
  char *endpoint = g_strdup_printf("http://fixture.test:%u/compressed", port);
  const char *allowed_endpoints[] = {endpoint};
  guint16 allowed_ports[] = {port};
  ResearchNetworkProfile local_profile = {allowed_ports, 1,
                                          allowed_endpoints, 1};
  ResearchAction local_action = action;
  local_action.endpoint = endpoint;
  local_action.subject = "fixture.test";
  ResearchTransportRequest req = {&local_action, &grant, &local_profile, "GET",
                                   NULL, 0, 32, 4096, 256, NULL};
  ResearchTransportReply reply = {0};
  g_assert_true(research_http_transport_curl_execute_fixture(
      &req, "fixture.test", "127.0.0.1", &reply, NULL));
  g_assert_cmpint(reply.state, ==, RESEARCH_TRANSPORT_TOO_LARGE);
  g_thread_join(thread);
  g_free(server.request_log);
  research_transport_reply_clear(&reply);
  g_free(endpoint);
}

int main(int argc,char **argv){g_test_init(&argc,&argv,NULL);
  g_test_add_func("/research/transport/unavailable",test_production_unavailable);
  g_test_add_func("/research/transport/states",test_headers_and_fixture_states);
  g_test_add_func("/research/transport/bounds",test_bounds_cancel_redirect);
  g_test_add_func("/research/transport/curl-local", test_curl_local_fixture);
  g_test_add_func("/research/transport/curl-body-limit", test_curl_body_limit);
  g_test_add_func("/research/transport/curl-compressed-limit",
                  test_curl_chunked_compressed_limit);
  return g_test_run();}
