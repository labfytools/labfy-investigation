#include "core/research_policy.h"

static const char *selected[] = {"11111111-1111-4111-8111-111111111111"};
static ResearchAction action = {
    "11111111-1111-4111-8111-111111111111", "page.fetch", "specimen",
    "https://example.org/specimen", "example.org", RESEARCH_CONTACT_TARGET,
    "hostname", 1, 4096, 1000};
static ScopeGrant grant = {
    RESEARCH_GRANT_CONTRACT, "22222222-2222-4222-8222-222222222222",
    "33333333-3333-4333-8333-333333333333", "grant-specimen",
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
    "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
    "2026-01-01T00:00:00Z", "2027-01-01T00:00:00Z", selected, 1,
    NULL, 0, 1, 4096, 1000};

static ResearchNetworkProfile profile(void) {
  static const guint16 ports[] = {443};
  static const char *endpoints[] = {"https://example.org/specimen"};
  return (ResearchNetworkProfile){ports, 1, endpoints, 1};
}

static void test_exact_action(void) {
  ResearchNetworkProfile p = profile();
  ResearchHttpTarget target = {0};
  g_assert_true(research_policy_validate_http_action(&action, &grant, &p,
                                                     &target, NULL));
  g_assert_cmpstr(target.host, ==, "example.org");
  research_http_target_clear(&target);

  ResearchAction bad = action;
  bad.subject = "sub.example.org";
  g_assert_false(research_policy_validate_http_action(&bad, &grant, &p,
                                                      &target, NULL));
  bad = action;
  bad.endpoint = "https://user@example.org/specimen";
  g_assert_false(research_policy_validate_http_action(&bad, &grant, &p,
                                                      &target, NULL));
  bad.endpoint = "https:\\example.org\\specimen";
  g_assert_false(research_policy_validate_http_action(&bad, &grant, &p,
                                                      &target, NULL));
  bad.endpoint = "ftp://example.org/specimen";
  g_assert_false(research_policy_validate_http_action(&bad, &grant, &p,
                                                      &target, NULL));
}

static void test_ports_and_addresses(void) {
  ResearchNetworkProfile p = profile();
  ResearchHttpTarget target = {0};
  ResearchAction bad = action;
  bad.endpoint = "https://example.org:8080/specimen";
  g_assert_false(research_policy_validate_http_action(&bad, &grant, &p,
                                                      &target, NULL));
  bad.endpoint = "https://example.org:8081/specimen";
  g_assert_false(research_policy_validate_http_action(&bad, &grant, &p,
                                                      &target, NULL));
  const char *blocked[] = {
      "127.0.0.1", "10.0.0.1", "100.64.0.1", "169.254.169.254",
      "192.0.0.1", "192.0.2.1", "192.88.99.1", "192.168.1.1",
      "198.18.0.1", "198.51.100.1", "203.0.113.1", "224.0.0.1",
      "0.0.0.0", "::1", "64:ff9b:1::1", "100::1", "2001:2::1",
      "2001:10::1", "2001:20::1", "2001:db8::1", "2002::1",
      "3fff::1", "5f00::1", "fe80::1", "fec0::1", "fc00::1", "ff02::1",
      "::", "::ffff:127.0.0.1", "::ffff:10.0.0.1", "::ffff:192.0.2.1"};
  for (gsize i = 0; i < G_N_ELEMENTS(blocked); i++)
    g_assert_false(research_policy_address_is_public(blocked[i]));
  g_assert_true(research_policy_address_is_public("8.8.8.8"));
  g_assert_true(research_policy_address_is_public("2606:4700:4700::1111"));
}

static void test_redirect_requires_decision(void) {
  ResearchNetworkProfile p = profile();
  ResearchHttpTarget target = {0};
  g_assert_cmpint(research_policy_check_redirect("https://example.net/next", &p,
                                                &target, NULL), ==,
                  RESEARCH_REDIRECT_REQUIRES_DECISION);
  research_http_target_clear(&target);
  g_assert_cmpint(research_policy_check_redirect("http://127.0.0.1/", &p,
                                                &target, NULL), ==,
                  RESEARCH_REDIRECT_INVALID);
}

int main(int argc, char **argv) {
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/research/policy/exact-action", test_exact_action);
  g_test_add_func("/research/policy/ports-addresses", test_ports_and_addresses);
  g_test_add_func("/research/policy/redirect", test_redirect_requires_decision);
  return g_test_run();
}
