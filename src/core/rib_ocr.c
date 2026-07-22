/******************************************************************************
 * @file rib_ocr.c
 * @brief Exécution locale de Tesseract pour les preuves de type RIB.
 ******************************************************************************/
#include "core/rib_ocr.h"
#include <gio/gio.h>
gboolean rib_ocr_extract_text(const char *image_path, char **out_text,
    char **out_version, GError **error)
{
    GSubprocess *process = NULL; GSubprocess *version_process = NULL;
    char *stdout_text = NULL; char *stderr_text = NULL; char *version = NULL;
    gboolean success = FALSE;
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    if (out_text != NULL) *out_text = NULL;
    if (out_version != NULL) *out_version = NULL;
    if (image_path == NULL || !g_file_test(image_path, G_FILE_TEST_IS_REGULAR))
    {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
            "La preuve image à analyser est invalide."); return FALSE;
    }
    process = g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE |
        G_SUBPROCESS_FLAGS_STDERR_PIPE, error, "tesseract", image_path,
        "stdout", "-l", "fra+eng", NULL);
    if (process == NULL) goto cleanup;
    if (!g_subprocess_communicate_utf8(process, NULL, NULL, &stdout_text,
            &stderr_text, error) || !g_subprocess_get_successful(process))
    {
        if (error != NULL && *error == NULL)
            g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                "Tesseract a échoué : %s", stderr_text != NULL
                    ? stderr_text : "erreur inconnue");
        goto cleanup;
    }
    version_process = g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE |
        G_SUBPROCESS_FLAGS_STDERR_PIPE, NULL, "tesseract", "--version", NULL);
    if (version_process != NULL) g_subprocess_communicate_utf8(version_process,
        NULL, NULL, &version, NULL, NULL);
    if (out_text != NULL) *out_text = g_utf8_make_valid(stdout_text, -1);
    if (out_version != NULL && version != NULL)
        *out_version = g_strdup(g_strstrip(version));
    success = out_text == NULL || *out_text != NULL;
cleanup:
    g_free(stdout_text); g_free(stderr_text); g_free(version);
    g_clear_object(&process); g_clear_object(&version_process); return success;
}
