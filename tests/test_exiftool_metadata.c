/******************************************************************************
 * @file test_exiftool_metadata.c
 * @brief Tests de validation de l'extraction ExifTool.
 ******************************************************************************/

#include "core/exiftool_metadata.h"

#include <glib.h>

/** @brief Vérifie le rejet des arguments incomplets. */
static void test_exiftool_metadata_invalid_arguments(void)
{
    GError *error = NULL;
    char *summary = NULL;
    char *json = NULL;

    g_assert_false(exiftool_metadata_extract(NULL, &summary, &json, NULL,
        &error));
    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
    g_clear_error(&error);
    g_assert_false(exiftool_metadata_extract("preuve.jpg", NULL, &json, NULL,
        &error));
    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
    g_clear_error(&error);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/exiftool-metadata/invalid-arguments",
        test_exiftool_metadata_invalid_arguments);
    return g_test_run();
}
