/******************************************************************************
 * @file test_osint_dns_query.c
 * @brief Tests de validation des cibles de résolution DNS OSINT.
 ******************************************************************************/

#include "models/osint_dns_query.h"

#include <glib.h>

static void test_valid_targets(void)
{
    g_assert_true(osint_dns_query_is_valid_target("example.org"));
    g_assert_true(osint_dns_query_is_valid_target("WWW.Example.ORG"));
    g_assert_true(osint_dns_query_is_valid_target("sub-domain.example.org"));
    g_assert_true(osint_dns_query_is_valid_target("example.org."));
}

static void test_invalid_targets(void)
{
    g_assert_false(osint_dns_query_is_valid_target(NULL));
    g_assert_false(osint_dns_query_is_valid_target(""));
    g_assert_false(osint_dns_query_is_valid_target("-x.example.org"));
    g_assert_false(osint_dns_query_is_valid_target("example..org"));
    g_assert_false(osint_dns_query_is_valid_target("example.org +trace"));
    g_assert_false(osint_dns_query_is_valid_target("example_.org"));
    g_assert_false(osint_dns_query_is_valid_target("example-.org"));
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/osint-dns-query/valid", test_valid_targets);
    g_test_add_func("/osint-dns-query/invalid", test_invalid_targets);
    return g_test_run();
}
