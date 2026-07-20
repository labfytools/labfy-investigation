/******************************************************************************
 * @file test_evidence_import_dialog.c
 * @brief Tests de validation du dialogue d'import d'une preuve.
 ******************************************************************************/

#include "views/evidence_import_dialog.h"

#include <glib.h>
#include <glib/gstdio.h>

/**
 * @brief Fichier temporaire utilisé par les tests.
 */
typedef struct
{
    char *directory_path;
    char *file_path;
} TestEvidenceImportDialogFixture;

/**
 * @brief Crée un fichier ordinaire temporaire.
 */
static TestEvidenceImportDialogFixture
test_evidence_import_dialog_fixture_create(void)
{
    TestEvidenceImportDialogFixture fixture = {0};
    GError *error = NULL;

    fixture.directory_path =
        g_dir_make_tmp(
            "labfy-evidence-import-dialog-test-XXXXXX",
            &error
        );

    g_assert_no_error(error);
    g_assert_nonnull(fixture.directory_path);

    fixture.file_path =
        g_build_filename(
            fixture.directory_path,
            "preuve.txt",
            NULL
        );

    g_assert_nonnull(fixture.file_path);

    g_file_set_contents(
        fixture.file_path,
        "preuve de test\n",
        -1,
        &error
    );

    g_assert_no_error(error);

    return fixture;
}

/**
 * @brief Supprime les ressources temporaires.
 */
static void test_evidence_import_dialog_fixture_clear(
    TestEvidenceImportDialogFixture *fixture
)
{
    g_assert_nonnull(fixture);

    g_assert_cmpint(
        g_remove(fixture->file_path),
        ==,
        0
    );

    g_assert_cmpint(
        g_rmdir(fixture->directory_path),
        ==,
        0
    );

    g_free(fixture->file_path);
    g_free(fixture->directory_path);

    fixture->file_path = NULL;
    fixture->directory_path = NULL;
}

/**
 * @brief Vérifie une date UTC valide.
 */
static void test_evidence_import_dialog_validate_utc(void)
{
    TestEvidenceImportDialogFixture fixture =
        test_evidence_import_dialog_fixture_create();

    GError *error = NULL;

    g_assert_true(
        evidence_import_dialog_validate_values(
            fixture.file_path,
            "document",
            "2026-07-20T08:30:00Z",
            &error
        )
    );

    g_assert_no_error(error);

    test_evidence_import_dialog_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie une date avec décalage horaire valide.
 */
static void test_evidence_import_dialog_validate_offset(void)
{
    TestEvidenceImportDialogFixture fixture =
        test_evidence_import_dialog_fixture_create();

    GError *error = NULL;

    g_assert_true(
        evidence_import_dialog_validate_values(
            fixture.file_path,
            "photo",
            "2026-07-20T10:30:00+02:00",
            &error
        )
    );

    g_assert_no_error(error);

    test_evidence_import_dialog_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d'un chemin absent.
 */
static void test_evidence_import_dialog_missing_path(void)
{
    GError *error = NULL;

    g_assert_false(
        evidence_import_dialog_validate_values(
            NULL,
            "document",
            "2026-07-20T08:30:00Z",
            &error
        )
    );

    g_assert_error(
        error,
        EVIDENCE_IMPORT_DIALOG_ERROR,
        EVIDENCE_IMPORT_DIALOG_ERROR_INVALID_DATA
    );

    g_clear_error(&error);
}

/**
 * @brief Vérifie le refus d'un fichier inexistant.
 */
static void test_evidence_import_dialog_missing_file(void)
{
    GError *error = NULL;

    g_assert_false(
        evidence_import_dialog_validate_values(
            "/tmp/labfy-file-that-does-not-exist",
            "document",
            "2026-07-20T08:30:00Z",
            &error
        )
    );

    g_assert_error(
        error,
        EVIDENCE_IMPORT_DIALOG_ERROR,
        EVIDENCE_IMPORT_DIALOG_ERROR_INVALID_DATA
    );

    g_clear_error(&error);
}

/**
 * @brief Vérifie le refus d'un type vide.
 */
static void test_evidence_import_dialog_missing_type(void)
{
    TestEvidenceImportDialogFixture fixture =
        test_evidence_import_dialog_fixture_create();

    GError *error = NULL;

    g_assert_false(
        evidence_import_dialog_validate_values(
            fixture.file_path,
            "   ",
            "2026-07-20T08:30:00Z",
            &error
        )
    );

    g_assert_error(
        error,
        EVIDENCE_IMPORT_DIALOG_ERROR,
        EVIDENCE_IMPORT_DIALOG_ERROR_INVALID_DATA
    );

    g_clear_error(&error);

    test_evidence_import_dialog_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d'une date absente.
 */
static void test_evidence_import_dialog_missing_date(void)
{
    TestEvidenceImportDialogFixture fixture =
        test_evidence_import_dialog_fixture_create();

    GError *error = NULL;

    g_assert_false(
        evidence_import_dialog_validate_values(
            fixture.file_path,
            "document",
            NULL,
            &error
        )
    );

    g_assert_error(
        error,
        EVIDENCE_IMPORT_DIALOG_ERROR,
        EVIDENCE_IMPORT_DIALOG_ERROR_INVALID_DATA
    );

    g_clear_error(&error);

    test_evidence_import_dialog_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d'une date sans fuseau horaire.
 */
static void test_evidence_import_dialog_missing_timezone(void)
{
    TestEvidenceImportDialogFixture fixture =
        test_evidence_import_dialog_fixture_create();

    GError *error = NULL;

    g_assert_false(
        evidence_import_dialog_validate_values(
            fixture.file_path,
            "document",
            "2026-07-20T08:30:00",
            &error
        )
    );

    g_assert_error(
        error,
        EVIDENCE_IMPORT_DIALOG_ERROR,
        EVIDENCE_IMPORT_DIALOG_ERROR_INVALID_DATA
    );

    g_clear_error(&error);

    test_evidence_import_dialog_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d'une date civile impossible.
 */
static void test_evidence_import_dialog_invalid_date(void)
{
    TestEvidenceImportDialogFixture fixture =
        test_evidence_import_dialog_fixture_create();

    GError *error = NULL;

    g_assert_false(
        evidence_import_dialog_validate_values(
            fixture.file_path,
            "document",
            "2026-02-30T08:30:00+01:00",
            &error
        )
    );

    g_assert_error(
        error,
        EVIDENCE_IMPORT_DIALOG_ERROR,
        EVIDENCE_IMPORT_DIALOG_ERROR_INVALID_DATA
    );

    g_clear_error(&error);

    test_evidence_import_dialog_fixture_clear(
        &fixture
    );
}

int main(
    int argc,
    char **argv
)
{
    g_test_init(
        &argc,
        &argv,
        NULL
    );

    g_test_add_func(
        "/evidence-import-dialog/validate-utc",
        test_evidence_import_dialog_validate_utc
    );

    g_test_add_func(
        "/evidence-import-dialog/validate-offset",
        test_evidence_import_dialog_validate_offset
    );

    g_test_add_func(
        "/evidence-import-dialog/missing-path",
        test_evidence_import_dialog_missing_path
    );

    g_test_add_func(
        "/evidence-import-dialog/missing-file",
        test_evidence_import_dialog_missing_file
    );

    g_test_add_func(
        "/evidence-import-dialog/missing-type",
        test_evidence_import_dialog_missing_type
    );

    g_test_add_func(
        "/evidence-import-dialog/missing-date",
        test_evidence_import_dialog_missing_date
    );

    g_test_add_func(
        "/evidence-import-dialog/missing-timezone",
        test_evidence_import_dialog_missing_timezone
    );

    g_test_add_func(
        "/evidence-import-dialog/invalid-date",
        test_evidence_import_dialog_invalid_date
    );

    return g_test_run();
}
