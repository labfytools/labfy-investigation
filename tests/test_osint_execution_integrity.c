/******************************************************************************
 * @file test_osint_execution_integrity.c
 * @brief Tests du contrôle d'intégrité des sorties OSINT.
 ******************************************************************************/

#include "core/osint_execution_integrity.h"

#include <glib.h>

static void test_empty_outputs_are_intact(void)
{
    GBytes *empty = g_bytes_new_static("", 0U);
    const char *expected =
        "6e340b9cffb37a989ca544e6bb780a2c78901d3fb33738768511a30617afa01d";
    g_assert_cmpint(osint_execution_integrity_verify(empty, empty, expected), ==,
        OSINT_EXECUTION_INTEGRITY_INTACT);
    g_bytes_unref(empty);
}

static void test_binary_outputs_and_alteration(void)
{
    const guint8 stdout_data[] = {0x41U, 0x00U, 0xFFU};
    const guint8 stderr_data[] = {0x80U, 0x42U};
    GBytes *stdout_raw = g_bytes_new_static(stdout_data, sizeof(stdout_data));
    GBytes *stderr_raw = g_bytes_new_static(stderr_data, sizeof(stderr_data));
    char *sha256 = osint_execution_integrity_calculate(stdout_raw, stderr_raw);
    g_assert_nonnull(sha256);
    g_assert_cmpint(osint_execution_integrity_verify(
        stdout_raw, stderr_raw, sha256), ==, OSINT_EXECUTION_INTEGRITY_INTACT);
    sha256[0] = sha256[0] == '0' ? '1' : '0';
    g_assert_cmpint(osint_execution_integrity_verify(
        stdout_raw, stderr_raw, sha256), ==, OSINT_EXECUTION_INTEGRITY_ALTERED);
    g_assert_cmpint(osint_execution_integrity_verify(
        NULL, stderr_raw, sha256), ==, OSINT_EXECUTION_INTEGRITY_UNAVAILABLE);
    g_free(sha256); g_bytes_unref(stdout_raw); g_bytes_unref(stderr_raw);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/osint-execution-integrity/empty",
        test_empty_outputs_are_intact);
    g_test_add_func("/osint-execution-integrity/binary-altered",
        test_binary_outputs_and_alteration);
    return g_test_run();
}
