/******************************************************************************
 * @file test_evidence_reclassification.c
 * @brief Tests du reclassement cohérent des preuves.
 ******************************************************************************/

#include "core/evidence_reclassification.h"
#include "core/file_hash.h"
#include "dao/evidence_dao.h"
#include "database/database.h"
#include "models/evidence_record.h"

#include <glib.h>
#include <glib/gstdio.h>

static void test_reclassification_preserves_identity_and_hash(void)
{
    GError *error = NULL; guint64 size = 0U; char *sha256 = NULL;
    char *root = g_dir_make_tmp("labfy-reclassify-XXXXXX", &error);
    char *database_dir = g_build_filename(root, "00_BaseDeDonnees", NULL);
    char *database_path = g_build_filename(database_dir, "Enquete.sqlite", NULL);
    char *old_dir = g_build_filename(root, "01_Preuves_Originales", "Documents", NULL);
    char *old_path = g_build_filename(old_dir, "internal.bin", NULL);
    char *new_path = g_build_filename(root, "01_Preuves_Originales", "Photos",
        "internal.bin", NULL);
    Database *database = NULL; EvidenceDao *dao = NULL;
    EvidenceRecord *record = NULL; EvidenceRecord *loaded = NULL;
    g_assert_no_error(error);
    g_assert_cmpint(g_mkdir_with_parents(database_dir, 0700), ==, 0);
    g_assert_cmpint(g_mkdir_with_parents(old_dir, 0700), ==, 0);
    g_assert_true(g_file_set_contents(old_path, "binary\0content", 14, &error));
    g_assert_true(file_hash_compute_sha256(old_path, NULL, &sha256, &size, &error));
    g_assert_true(database_initialize(database_path, "Test", root));
    database = database_open(database_path); dao = evidence_dao_new(database, &error);
    record = evidence_record_new(
        "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa", "original.bin", "internal.bin",
        "01_Preuves_Originales/Documents/internal.bin", "document", size, sha256,
        "2026-07-22T10:00:00Z", "2026-07-18T10:00:00Z", "Victime", "Avant",
        EVIDENCE_INTEGRITY_STATUS_UNKNOWN, &error);
    g_assert_nonnull(record); g_assert_true(evidence_dao_insert(dao, record, &error));
    g_assert_true(evidence_reclassification_apply(
        database, root, "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa", "photo",
        "01_Preuves_Originales/Photos", "Victime", "Après", &error));
    g_assert_false(g_file_test(old_path, G_FILE_TEST_EXISTS));
    g_assert_true(g_file_test(new_path, G_FILE_TEST_IS_REGULAR));
    loaded = evidence_dao_find_by_identifier(
        dao, "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa", &error);
    g_assert_cmpstr(evidence_record_get_type_identifier(loaded), ==, "photo");
    g_assert_cmpstr(evidence_record_get_sha256(loaded), ==, sha256);
    g_assert_cmpstr(evidence_record_get_imported_at(loaded), ==,
        "2026-07-22T10:00:00Z");
    g_assert_cmpstr(evidence_record_get_description(loaded), ==, "Après");
    evidence_record_free(loaded); evidence_record_free(record);
    evidence_dao_free(dao); database_close(database);
    g_assert_cmpint(g_remove(new_path), ==, 0);
    g_assert_cmpint(g_remove(database_path), ==, 0);
    char *photos = g_path_get_dirname(new_path);
    char *originals = g_path_get_dirname(photos);
    g_assert_cmpint(g_rmdir(old_dir), ==, 0); g_assert_cmpint(g_rmdir(photos), ==, 0);
    g_assert_cmpint(g_rmdir(originals), ==, 0); g_assert_cmpint(g_rmdir(database_dir), ==, 0);
    g_assert_cmpint(g_rmdir(root), ==, 0);
    g_free(photos); g_free(originals); g_free(sha256); g_free(new_path);
    g_free(old_path); g_free(old_dir); g_free(database_path); g_free(database_dir); g_free(root);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/evidence-reclassification/preserve",
        test_reclassification_preserves_identity_and_hash);
    return g_test_run();
}
