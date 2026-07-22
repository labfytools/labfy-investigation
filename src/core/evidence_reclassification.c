/******************************************************************************
 * @file evidence_reclassification.c
 * @brief Reclassement cohérent d'une preuve sans modifier son contenu.
 ******************************************************************************/

#include "core/evidence_reclassification.h"

#include "core/file_hash.h"
#include "dao/evidence_dao.h"
#include "database/transaction.h"
#include "models/evidence_record.h"

#include <glib/gstdio.h>
#include <errno.h>

/** @brief Enregistre une erreur de reclassement si aucune n'existe encore. */
static void evidence_reclassification_set_error(
    GError **error, const char *message
)
{
    if (error != NULL && *error == NULL)
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED, message);
}

gboolean evidence_reclassification_apply(
    Database *database, const char *investigation_root,
    const char *evidence_identifier, const char *type_identifier,
    const char *relative_directory, const char *source,
    const char *description, GError **error
)
{
    EvidenceDao *dao = NULL; EvidenceRecord *record = NULL;
    char *old_path = NULL; char *new_path = NULL; char *new_relative_path = NULL;
    char *new_parent = NULL; char *sha256 = NULL; char *updated_at = NULL;
    guint64 size_bytes = 0U; GDateTime *now = NULL;
    gboolean moved = FALSE; gboolean transaction_started = FALSE;
    gboolean success = FALSE;
    if (database == NULL || investigation_root == NULL ||
        evidence_identifier == NULL || type_identifier == NULL ||
        relative_directory == NULL || g_path_is_absolute(relative_directory) ||
        strstr(relative_directory, "..") != NULL)
    {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
            "Les paramètres de reclassement sont invalides.");
        return FALSE;
    }
    dao = evidence_dao_new(database, error);
    if (dao == NULL) goto cleanup;
    record = evidence_dao_find_by_identifier(dao, evidence_identifier, error);
    if (record == NULL) goto cleanup;
    old_path = g_build_filename(investigation_root,
        evidence_record_get_relative_path(record), NULL);
    new_relative_path = g_build_filename(relative_directory,
        evidence_record_get_internal_name(record), NULL);
    new_path = g_build_filename(investigation_root, new_relative_path, NULL);
    new_parent = g_path_get_dirname(new_path);
    if (!file_hash_compute_sha256(old_path, NULL, &sha256, &size_bytes, error) ||
        g_strcmp0(sha256, evidence_record_get_sha256(record)) != 0 ||
        size_bytes != evidence_record_get_size_bytes(record))
    {
        evidence_reclassification_set_error(error,
            "L'intégrité de la preuve source n'est pas conforme.");
        goto cleanup;
    }
    g_clear_pointer(&sha256, g_free);
    if (g_mkdir_with_parents(new_parent, 0700) != 0)
    {
        g_set_error(error, G_IO_ERROR, g_io_error_from_errno(errno),
            "Impossible de préparer le dossier de destination : %s",
            g_strerror(errno));
        goto cleanup;
    }
    if (!database_transaction_begin(database))
    {
        evidence_reclassification_set_error(error,
            "Impossible de démarrer la transaction de reclassement.");
        goto cleanup;
    }
    transaction_started = TRUE;
    if (g_strcmp0(old_path, new_path) != 0)
    {
        if (g_file_test(new_path, G_FILE_TEST_EXISTS) ||
            g_rename(old_path, new_path) != 0)
        {
            g_set_error(error, G_IO_ERROR, g_io_error_from_errno(errno),
                "Impossible de déplacer la preuve : %s", g_strerror(errno));
            goto cleanup;
        }
        moved = TRUE;
    }
    now = g_date_time_new_now_utc();
    updated_at = now != NULL
        ? g_date_time_format(now, "%Y-%m-%dT%H:%M:%SZ") : NULL;
    if (!evidence_dao_update_metadata(dao, evidence_identifier,
            type_identifier, new_relative_path, source, description,
            updated_at, error) ||
        !file_hash_compute_sha256(new_path, NULL, &sha256, &size_bytes, error) ||
        g_strcmp0(sha256, evidence_record_get_sha256(record)) != 0 ||
        size_bytes != evidence_record_get_size_bytes(record))
    {
        evidence_reclassification_set_error(error,
            "L'intégrité après reclassement n'est pas conforme.");
        goto cleanup;
    }
    if (!database_transaction_commit(database))
    {
        evidence_reclassification_set_error(error,
            "Impossible de valider le reclassement.");
        goto cleanup;
    }
    transaction_started = FALSE; success = TRUE;
cleanup:
    if (!success && transaction_started) database_transaction_rollback(database);
    if (!success && moved && g_rename(new_path, old_path) != 0)
        g_warning("Impossible de restaurer la preuve '%s'.", old_path);
    g_clear_pointer(&now, g_date_time_unref); g_free(updated_at); g_free(sha256);
    g_free(new_parent); g_free(new_path); g_free(new_relative_path); g_free(old_path);
    evidence_record_free(record); evidence_dao_free(dao);
    return success;
}
