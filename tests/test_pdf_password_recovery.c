/******************************************************************************
 * @file test_pdf_password_recovery.c
 * @brief Tests de validation de la récupération PDF.
 ******************************************************************************/

#include "core/pdf_password_recovery.h"

#include <glib.h>

/** @brief Vérifie le rejet d'une configuration incomplète. */
static void test_pdf_password_recovery_invalid_arguments(void)
{
    GError *error = NULL;
    g_assert_null(pdf_password_recovery_start(NULL, "preuve.pdf", "/tmp",
        "00000000-0000-4000-8000-000000000000",
        PDF_PASSWORD_RECOVERY_DICTIONARY, "mots.txt", NULL, NULL, NULL,
        &error));
    g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
    g_clear_error(&error);
    g_assert_false(pdf_password_recovery_result_is_recovered(NULL));
    g_assert_null(pdf_password_recovery_result_get_password(NULL));
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/pdf-password-recovery/invalid-arguments",
        test_pdf_password_recovery_invalid_arguments);
    return g_test_run();
}
