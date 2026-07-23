#include "core/relation_type_normalizer.h"
#include <glib.h>

static void assert_same(const char *left, const char *right)
{
    char *left_key = relation_type_normalize_key(left);
    char *right_key = relation_type_normalize_key(right);
    g_assert_cmpstr(left_key, ==, right_key);
    g_free(right_key); g_free(left_key);
}

static void test_spaces(void)
{
    assert_same("email", " Email ");
    assert_same("Réseaux sociaux", "Réseaux   Sociaux");
    assert_same("Réseaux sociaux", "Réseaux\t\nSociaux");
}
static void test_unicode(void)
{
    assert_same("RéSEAUX", "re\xCC\x81seaux");
    assert_same("STRASSE", "Straße");
}
static void test_distinct_meanings(void)
{
    char *first = relation_type_normalize_key("Envoie fausses places");
    char *second = relation_type_normalize_key("Envoie des fausses places");
    g_assert_cmpstr(first, !=, second);
    g_free(second); g_free(first);
}
int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/relation-type-normalizer/spaces", test_spaces);
    g_test_add_func("/relation-type-normalizer/unicode", test_unicode);
    g_test_add_func("/relation-type-normalizer/distinct", test_distinct_meanings);
    return g_test_run();
}
