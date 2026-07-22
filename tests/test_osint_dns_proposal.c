/******************************************************************************
 * @file test_osint_dns_proposal.c
 * @brief Tests des propositions structurées extraites de dig.
 ******************************************************************************/

#include "models/osint_dns_proposal.h"

#include <glib.h>

static void test_parse_recognized_records(void)
{
    const char *output =
        "example.org. 300 IN A 93.184.216.34\n"
        "example.org. 300 IN MX 10 mail.example.org.\n"
        "example.org. 300 IN TXT \"texte avec espaces\"\n";
    GPtrArray *proposals = osint_dns_proposal_parse("example.org", output);
    const OsintDnsProposal *first = g_ptr_array_index(proposals, 0);
    const OsintDnsProposal *third = g_ptr_array_index(proposals, 2);

    g_assert_cmpuint(proposals->len, ==, 3);
    g_assert_cmpstr(osint_dns_proposal_get_owner(first), ==, "example.org.");
    g_assert_cmpstr(osint_dns_proposal_get_record_type(first), ==, "A");
    g_assert_cmpstr(osint_dns_proposal_get_value(first), ==, "93.184.216.34");
    g_assert_cmpstr(osint_dns_proposal_get_target(first), ==, "example.org");
    g_assert_cmpstr(
        osint_dns_proposal_get_value(third), ==, "\"texte avec espaces\""
    );
    g_ptr_array_unref(proposals);
}

static void test_ignore_unrecognized_lines(void)
{
    const char *output =
        ";; comment\n"
        "invalid line\n"
        "example.org. nope IN A 93.184.216.34\n"
        "example.org. 60 CH A 93.184.216.34\n"
        "example.org. 60 IN TYPE999 opaque\n";
    GPtrArray *proposals = osint_dns_proposal_parse("example.org", output);
    g_assert_nonnull(proposals);
    g_assert_cmpuint(proposals->len, ==, 0);
    g_ptr_array_unref(proposals);
}

static void test_invalid_arguments(void)
{
    g_assert_null(osint_dns_proposal_parse(NULL, ""));
    g_assert_null(osint_dns_proposal_parse("", ""));
    g_assert_null(osint_dns_proposal_parse("example.org", NULL));
}

static void test_entity_mapping_and_normalization(void)
{
    const char *output =
        "example.org. 60 IN A 192.0.2.10\n"
        "example.org. 60 IN AAAA 2001:0db8:0:0:0:0:0:1\n"
        "example.org. 60 IN CNAME WWW.Example.ORG.\n"
        "example.org. 60 IN MX 10 mail.example.org.\n";
    GPtrArray *proposals = osint_dns_proposal_parse("example.org", output);
    const OsintDnsProposal *ipv4 = g_ptr_array_index(proposals, 0);
    const OsintDnsProposal *ipv6 = g_ptr_array_index(proposals, 1);
    const OsintDnsProposal *domain = g_ptr_array_index(proposals, 2);
    const OsintDnsProposal *unsupported = g_ptr_array_index(proposals, 3);
    char *ipv4_value = osint_dns_proposal_dup_normalized_value(ipv4);
    char *ipv6_value = osint_dns_proposal_dup_normalized_value(ipv6);
    char *domain_value = osint_dns_proposal_dup_normalized_value(domain);

    g_assert_cmpstr(osint_dns_proposal_get_entity_type(ipv4), ==, "ip_address");
    g_assert_cmpstr(ipv4_value, ==, "192.0.2.10");
    g_assert_cmpstr(
        osint_dns_proposal_get_relation_type(ipv4), ==, "resolves_to"
    );
    g_assert_cmpstr(ipv6_value, ==, "2001:db8::1");
    g_assert_cmpstr(osint_dns_proposal_get_entity_type(domain), ==, "domain_name");
    g_assert_cmpstr(domain_value, ==, "www.example.org");
    g_assert_cmpstr(
        osint_dns_proposal_get_relation_type(domain), ==, "aliases_to"
    );
    g_assert_null(osint_dns_proposal_get_entity_type(unsupported));
    g_assert_null(osint_dns_proposal_get_relation_type(unsupported));
    g_assert_null(osint_dns_proposal_dup_normalized_value(unsupported));

    g_free(ipv4_value);
    g_free(ipv6_value);
    g_free(domain_value);
    g_ptr_array_unref(proposals);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/osint-dns-proposal/recognized", test_parse_recognized_records);
    g_test_add_func("/osint-dns-proposal/ignored", test_ignore_unrecognized_lines);
    g_test_add_func("/osint-dns-proposal/invalid", test_invalid_arguments);
    g_test_add_func(
        "/osint-dns-proposal/mapping", test_entity_mapping_and_normalization
    );
    return g_test_run();
}
