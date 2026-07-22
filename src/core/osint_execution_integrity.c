/******************************************************************************
 * @file osint_execution_integrity.c
 * @brief Calcul et vérification de l'intégrité des sorties OSINT.
 ******************************************************************************/

#include "core/osint_execution_integrity.h"

#include <string.h>

char *osint_execution_integrity_calculate(GBytes *stdout_raw, GBytes *stderr_raw)
{
    GChecksum *checksum = NULL;
    gconstpointer data = NULL;
    gsize data_size = 0U;
    const guint8 separator = 0U;
    char *digest = NULL;
    if (stdout_raw == NULL || stderr_raw == NULL) return NULL;
    checksum = g_checksum_new(G_CHECKSUM_SHA256);
    if (checksum == NULL) return NULL;
    data = g_bytes_get_data(stdout_raw, &data_size);
    if (data_size > 0U) g_checksum_update(checksum, data, data_size);
    g_checksum_update(checksum, &separator, 1U);
    data = g_bytes_get_data(stderr_raw, &data_size);
    if (data_size > 0U) g_checksum_update(checksum, data, data_size);
    digest = g_strdup(g_checksum_get_string(checksum));
    g_checksum_free(checksum);
    return digest;
}

OsintExecutionIntegrityStatus osint_execution_integrity_verify(
    GBytes *stdout_raw, GBytes *stderr_raw, const char *expected_sha256
)
{
    char *calculated_sha256 = NULL;
    OsintExecutionIntegrityStatus status = OSINT_EXECUTION_INTEGRITY_UNAVAILABLE;
    if (expected_sha256 == NULL || strlen(expected_sha256) != 64U) return status;
    calculated_sha256 = osint_execution_integrity_calculate(stdout_raw, stderr_raw);
    if (calculated_sha256 != NULL)
        status = g_ascii_strcasecmp(calculated_sha256, expected_sha256) == 0
            ? OSINT_EXECUTION_INTEGRITY_INTACT
            : OSINT_EXECUTION_INTEGRITY_ALTERED;
    g_free(calculated_sha256);
    return status;
}
