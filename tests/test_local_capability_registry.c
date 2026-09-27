/******************************************************************************
 * @file test_local_capability_registry.c
 * @brief Contrats du registre J4 et sonde ExifTool réelle optionnelle.
 ******************************************************************************/
#include "core/local_capability_registry.h"
#include <glib.h>

static void test_registry(void)
{
    GError *error = NULL;
    LocalCapabilityRegistry *registry = local_capability_registry_new(&error);
    g_assert_no_error(error); g_assert_nonnull(registry);
    const GPtrArray *statuses = local_capability_registry_get_statuses(registry);
    g_assert_cmpuint(statuses->len, ==, 2U);
    const LocalCapabilityStatus *eml = local_capability_registry_lookup(
        registry, LOCAL_CAPABILITY_EML_HEADERS);
    const LocalCapabilityStatus *exif = local_capability_registry_lookup(
        registry, LOCAL_CAPABILITY_EXIF_METADATA);
    g_assert_cmpint(eml->availability, ==, LOCAL_CAPABILITY_READY);
    g_assert_nonnull(exif); g_assert_nonnull(exif->reason);
    char *reason = NULL;
    g_assert_true(local_capability_status_applies(eml, "message/rfc822",
        "preuve.eml", &reason)); g_free(reason);
    g_assert_false(local_capability_status_applies(exif, "text/plain",
        "person.txt", &reason)); g_free(reason);
    local_capability_registry_free(registry);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/local-capability/registry", test_registry);
    return g_test_run();
}
