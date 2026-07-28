#include "core/evidence_staging.h"
#include "core/file_hash.h"
#include <errno.h>
#include <glib/gstdio.h>
#include <string.h>
#include <sys/stat.h>

struct EvidenceStaging {
    gint reference_count;
    char *directory;
    GPtrArray *paths;
};
static GQuark staging_error(void)
{
    return g_quark_from_static_string("evidence-staging-error");
}
static const char *suggest_type(const char *mime)
{
    if (g_strcmp0(mime, "image/png") == 0) return "screenshot";
    if (g_strcmp0(mime, "image/jpeg") == 0) return "photo";
    if (g_strcmp0(mime, "application/pdf") == 0) return "document";
    if (mime != NULL && g_str_has_prefix(mime, "video/")) return "video";
    if (mime != NULL && g_str_has_prefix(mime, "audio/")) return "audio";
    if (mime != NULL && g_str_has_prefix(mime, "text/")) return "text";
    if (g_strcmp0(mime, "message/rfc822") == 0) return "email";
    return "other";
}
EvidenceStaging *evidence_staging_new(GError **error)
{
    EvidenceStaging *staging = g_new0(EvidenceStaging, 1);
    staging->directory = g_dir_make_tmp("labfy-person-staging-XXXXXX", error);
    if (staging->directory == NULL) {
        g_free(staging);
        return NULL;
    }
    staging->paths = g_ptr_array_new_with_free_func(g_free);
    staging->reference_count = 1;
    return staging;
}
EvidenceStaging *evidence_staging_ref(EvidenceStaging *staging)
{
    if (staging != NULL) g_atomic_int_inc(&staging->reference_count);
    return staging;
}
const char *evidence_staging_get_directory(const EvidenceStaging *staging)
{
    return staging != NULL ? staging->directory : NULL;
}
static char *detect_mime(const char *path, const char *name)
{
    gboolean uncertain = FALSE;
    char header[512];
    gsize length = 0;
    char *content = NULL;
    char *mime;
    if (!g_file_get_contents(path, &content, &length, NULL)) length = 0;
    length = MIN(length, sizeof header);
    if (length > 0) memcpy(header, content, length);
    g_free(content);
    mime = g_content_type_guess(name,
        length > 0 ? (const guchar *) header : NULL, length, &uncertain);
    (void) uncertain;
    if (mime == NULL) return g_strdup("application/octet-stream");
    char *result = g_content_type_get_mime_type(mime);
    g_free(mime);
    return result != NULL ? result : g_strdup("application/octet-stream");
}
EvidenceStagingResult *evidence_staging_prepare(EvidenceStaging *staging,
    const char *source_path, GCancellable *cancellable, GError **error)
{
    struct stat metadata;
    EvidenceStagingResult *result = NULL;
    GFile *source = NULL, *destination = NULL;
    char *source_hash = NULL, *copy_hash = NULL, *internal = NULL;
    guint64 hashed_size = 0;
    GDateTime *now = NULL;
    if (staging == NULL || source_path == NULL ||
        g_file_test(source_path, G_FILE_TEST_IS_SYMLINK) ||
        g_stat(source_path, &metadata) != 0 ||
        !S_ISREG(metadata.st_mode)) {
        g_set_error_literal(error, staging_error(), 1,
            "Le fichier source est absent, spécial ou non autorisé.");
        return NULL;
    }
    if ((guint64) metadata.st_size > EVIDENCE_STAGING_MAX_BYTES) {
        g_set_error_literal(error, staging_error(), 2,
            "Le fichier dépasse la taille maximale de staging.");
        return NULL;
    }
    if (!file_hash_compute_sha256(source_path, cancellable,
            &source_hash, &hashed_size, error)) goto cleanup;
    {
        char *identifier = g_uuid_string_random();
        internal = g_strdup_printf("%s.stage", identifier);
        g_free(identifier);
    }
    result = g_new0(EvidenceStagingResult, 1);
    result->source_path = g_canonicalize_filename(source_path, NULL);
    result->staging_path =
        g_build_filename(staging->directory, internal, NULL);
    result->original_name = g_path_get_basename(source_path);
    source = g_file_new_for_path(source_path);
    destination = g_file_new_for_path(result->staging_path);
    if (!g_file_copy(source, destination, G_FILE_COPY_NONE,
            cancellable, NULL, NULL, error)) goto cleanup;
    if (!file_hash_compute_sha256(result->staging_path, cancellable,
            &copy_hash, &hashed_size, error) ||
        g_strcmp0(source_hash, copy_hash) != 0) {
        if (error != NULL && *error == NULL)
            g_set_error_literal(error, staging_error(), 3,
                "La copie de staging ne correspond pas à la source.");
        goto cleanup;
    }
    result->mime_type = detect_mime(result->staging_path,
        result->original_name);
    result->suggested_type = g_strdup(suggest_type(result->mime_type));
    result->sha256 = g_steal_pointer(&copy_hash);
    result->size_bytes = (guint64) metadata.st_size;
    now = g_date_time_new_now_utc();
    result->prepared_at = g_date_time_format(now, "%Y-%m-%dT%H:%M:%SZ");
    g_ptr_array_add(staging->paths, g_strdup(result->staging_path));
cleanup:
    if (result != NULL && result->sha256 == NULL) {
        g_unlink(result->staging_path);
        evidence_staging_result_free(result);
        result = NULL;
    }
    g_clear_object(&source);
    g_clear_object(&destination);
    g_clear_pointer(&now, g_date_time_unref);
    g_free(source_hash);
    g_free(copy_hash);
    g_free(internal);
    return result;
}
void evidence_staging_result_free(EvidenceStagingResult *result)
{
    if (result == NULL) return;
    g_free(result->source_path);
    g_free(result->staging_path);
    g_free(result->original_name);
    g_free(result->mime_type);
    g_free(result->suggested_type);
    g_free(result->sha256);
    g_free(result->prepared_at);
    g_free(result);
}
gboolean evidence_staging_remove(EvidenceStaging *staging,
    const char *staging_path, GError **error)
{
    if (staging == NULL || staging_path == NULL ||
        !g_str_has_prefix(staging_path, staging->directory)) return FALSE;
    if (g_unlink(staging_path) != 0 && errno != ENOENT) {
        g_set_error(error, staging_error(), 4,
            "Impossible de supprimer la copie de staging : %s.",
            g_strerror(errno));
        return FALSE;
    }
    for (guint i = 0; i < staging->paths->len; i++)
        if (g_strcmp0(g_ptr_array_index(staging->paths, i),
                staging_path) == 0) {
            g_ptr_array_remove_index(staging->paths, i);
            break;
        }
    return TRUE;
}
gboolean evidence_staging_cleanup(EvidenceStaging *staging, GError **error)
{
    gboolean success = TRUE;
    if (staging == NULL) return TRUE;
    while (staging->paths->len > 0) {
        char *path = g_strdup(g_ptr_array_index(staging->paths, 0));
        if (!evidence_staging_remove(staging, path, error)) success = FALSE;
        g_free(path);
        if (!success) break;
    }
    if (success && staging->directory != NULL &&
        g_rmdir(staging->directory) != 0 && errno != ENOENT) {
        g_set_error(error, staging_error(), 5,
            "Impossible de supprimer le répertoire de staging : %s.",
            g_strerror(errno));
        success = FALSE;
    }
    return success;
}
void evidence_staging_free(EvidenceStaging *staging)
{
    if (staging == NULL) return;
    if (!g_atomic_int_dec_and_test(&staging->reference_count)) return;
    evidence_staging_cleanup(staging, NULL);
    g_ptr_array_unref(staging->paths);
    g_free(staging->directory);
    g_free(staging);
}
