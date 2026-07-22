/******************************************************************************
 * @file exiftool_metadata.c
 * @brief Extraction locale des métadonnées techniques d'une preuve.
 ******************************************************************************/

#include "core/exiftool_metadata.h"

#include <gio/gio.h>

/** @brief Exécute ExifTool et retourne sa sortie standard en UTF-8. */
static gboolean exiftool_metadata_run(const char *const *arguments,
    char **output, GError **error)
{
    GSubprocess *process = NULL;
    char *stdout_text = NULL;
    char *stderr_text = NULL;
    gboolean success = FALSE;

    process = g_subprocess_newv(arguments,
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE, error);
    if (process == NULL) return FALSE;
    if (!g_subprocess_communicate_utf8(process, NULL, NULL, &stdout_text,
            &stderr_text, error)) goto cleanup;
    if (!g_subprocess_get_successful(process))
    {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
            "ExifTool a échoué : %s", stderr_text != NULL ? stderr_text :
            "aucun détail disponible");
        goto cleanup;
    }
    if (stdout_text == NULL || stdout_text[0] == '\0')
    {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
            "ExifTool n'a retourné aucune métadonnée.");
        goto cleanup;
    }
    *output = g_steal_pointer(&stdout_text);
    success = TRUE;
cleanup:
    g_free(stdout_text);
    g_free(stderr_text);
    g_object_unref(process);
    return success;
}

gboolean exiftool_metadata_extract(const char *file_path, char **summary,
    char **json, char **version, GError **error)
{
    const char *summary_arguments[] =
        { "exiftool", "-a", "-G1", "-s", file_path, NULL };
    const char *json_arguments[] =
        { "exiftool", "-j", "-a", "-G1", "-s", file_path, NULL };
    const char *version_arguments[] = { "exiftool", "-ver", NULL };
    char *local_summary = NULL;
    char *local_json = NULL;
    char *local_version = NULL;

    if (file_path == NULL || file_path[0] == '\0' || summary == NULL ||
        json == NULL)
    {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
            "Le fichier ou les sorties ExifTool sont absents.");
        return FALSE;
    }
    *summary = NULL;
    *json = NULL;
    if (version != NULL) *version = NULL;
    if (!exiftool_metadata_run(summary_arguments, &local_summary, error) ||
        !exiftool_metadata_run(json_arguments, &local_json, error))
        goto failure;
    if (version != NULL && !exiftool_metadata_run(version_arguments,
            &local_version, error)) goto failure;
    *summary = g_steal_pointer(&local_summary);
    *json = g_steal_pointer(&local_json);
    if (version != NULL) *version = g_steal_pointer(&local_version);
    return TRUE;
failure:
    g_free(local_summary);
    g_free(local_json);
    g_free(local_version);
    return FALSE;
}
