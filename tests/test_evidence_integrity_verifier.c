/******************************************************************************
 * @file test_evidence_integrity_verifier.c
 * @brief Tests de la vérification d'intégrité des preuves.
 ******************************************************************************/

#define _POSIX_C_SOURCE 200809L

#include "core/evidence_integrity_verifier.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>

#define TEST_SHA256_ABC \
    "ba7816bf8f01cfea414140de5dae2223" \
    "b00361a396177a9cb410ff61f20015ad"

#define TEST_RELATIVE_DIRECTORY \
    "01_Preuves_Originales/Documents"

/**
 * @brief Environnement temporaire des tests du vérificateur.
 */
typedef struct
{
    char *temporary_directory;
    char *evidence_root_directory;
    char *document_directory;
} TestEvidenceIntegrityVerifierFixture;

/**
 * @brief Vérifie une erreur technique du vérificateur.
 */
static void test_evidence_integrity_verifier_assert_error(
    const GError *error,
    EvidenceIntegrityVerifierError expected_error
)
{
    assert(error != NULL);

    assert(
        error->domain ==
        EVIDENCE_INTEGRITY_VERIFIER_ERROR
    );

    assert(
        error->code ==
        (gint) expected_error
    );

    assert(error->message != NULL);
    assert(error->message[0] != '\0');
}

/**
 * @brief Crée une arborescence d'enquête temporaire.
 */
static TestEvidenceIntegrityVerifierFixture
test_evidence_integrity_verifier_fixture_create(void)
{
    TestEvidenceIntegrityVerifierFixture fixture = {0};

    GError *error = NULL;

    fixture.temporary_directory =
        g_dir_make_tmp(
            "labfy-integrity-verifier-test-XXXXXX",
            &error
        );

    assert(fixture.temporary_directory != NULL);
    assert(error == NULL);

    fixture.evidence_root_directory =
        g_build_filename(
            fixture.temporary_directory,
            "01_Preuves_Originales",
            NULL
        );

    fixture.document_directory =
        g_build_filename(
            fixture.evidence_root_directory,
            "Documents",
            NULL
        );

    assert(fixture.evidence_root_directory != NULL);
    assert(fixture.document_directory != NULL);

    assert(
        g_mkdir_with_parents(
            fixture.document_directory,
            0700
        ) == 0
    );

    return fixture;
}

/**
 * @brief Supprime l'arborescence temporaire.
 *
 * Tous les fichiers de test doivent avoir été supprimés auparavant.
 */
static void test_evidence_integrity_verifier_fixture_clear(
    TestEvidenceIntegrityVerifierFixture *fixture
)
{
    assert(fixture != NULL);
    assert(fixture->temporary_directory != NULL);
    assert(fixture->evidence_root_directory != NULL);
    assert(fixture->document_directory != NULL);

    assert(
        g_rmdir(
            fixture->document_directory
        ) == 0
    );

    assert(
        g_rmdir(
            fixture->evidence_root_directory
        ) == 0
    );

    assert(
        g_rmdir(
            fixture->temporary_directory
        ) == 0
    );

    g_free(
        fixture->document_directory
    );

    g_free(
        fixture->evidence_root_directory
    );

    g_free(
        fixture->temporary_directory
    );

    fixture->document_directory = NULL;
    fixture->evidence_root_directory = NULL;
    fixture->temporary_directory = NULL;
}

/**
 * @brief Construit le chemin absolu d'un fichier dans Documents.
 */
static char *test_evidence_integrity_verifier_build_file_path(
    const TestEvidenceIntegrityVerifierFixture *fixture,
    const char *file_name
)
{
    assert(fixture != NULL);
    assert(fixture->document_directory != NULL);
    assert(file_name != NULL);

    return g_build_filename(
        fixture->document_directory,
        file_name,
        NULL
    );
}

/**
 * @brief Construit le chemin relatif d'un fichier dans Documents.
 */
static char *test_evidence_integrity_verifier_build_relative_path(
    const char *file_name
)
{
    assert(file_name != NULL);

    return g_build_filename(
        TEST_RELATIVE_DIRECTORY,
        file_name,
        NULL
    );
}

/**
 * @brief Vérifie qu'un fichier intact produit VALID.
 */
static void test_evidence_integrity_verifier_valid(void)
{
    TestEvidenceIntegrityVerifierFixture fixture =
        test_evidence_integrity_verifier_fixture_create();

    EvidenceIntegrityVerificationResult *result = NULL;

    char *file_path = NULL;
    char *relative_path = NULL;

    GError *error = NULL;

    file_path =
        test_evidence_integrity_verifier_build_file_path(
            &fixture,
            "valid.bin"
        );

    relative_path =
        test_evidence_integrity_verifier_build_relative_path(
            "valid.bin"
        );

    assert(file_path != NULL);
    assert(relative_path != NULL);

    assert(
        g_file_set_contents(
            file_path,
            "abc",
            3,
            &error
        )
    );

    assert(error == NULL);

    result =
        evidence_integrity_verifier_verify(
            fixture.temporary_directory,
            relative_path,
            TEST_SHA256_ABC,
            NULL,
            &error
        );

    assert(result != NULL);
    assert(error == NULL);

    assert(
        evidence_integrity_verification_result_get_status(
            result
        ) == EVIDENCE_INTEGRITY_STATUS_VALID
    );

    assert(
        strcmp(
            evidence_integrity_verification_result_get_computed_sha256(
                result
            ),
            TEST_SHA256_ABC
        ) == 0
    );

    assert(
        evidence_integrity_verification_result_get_size_bytes(
            result
        ) == 3
    );

    assert(
        evidence_integrity_verification_result_get_diagnostic(
            result
        ) == NULL
    );

    evidence_integrity_verification_result_free(
        result
    );

    assert(
        g_remove(
            file_path
        ) == 0
    );

    g_free(
        relative_path
    );

    g_free(
        file_path
    );

    test_evidence_integrity_verifier_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie qu'une empreinte différente produit MODIFIED.
 */
static void test_evidence_integrity_verifier_modified(void)
{
    TestEvidenceIntegrityVerifierFixture fixture =
        test_evidence_integrity_verifier_fixture_create();

    EvidenceIntegrityVerificationResult *result = NULL;

    const char *computed_sha256 = NULL;
    const char *diagnostic = NULL;

    char *file_path = NULL;
    char *relative_path = NULL;

    GError *error = NULL;

    file_path =
        test_evidence_integrity_verifier_build_file_path(
            &fixture,
            "modified.bin"
        );

    relative_path =
        test_evidence_integrity_verifier_build_relative_path(
            "modified.bin"
        );

    assert(file_path != NULL);
    assert(relative_path != NULL);

    assert(
        g_file_set_contents(
            file_path,
            "abcd",
            4,
            &error
        )
    );

    assert(error == NULL);

    result =
        evidence_integrity_verifier_verify(
            fixture.temporary_directory,
            relative_path,
            TEST_SHA256_ABC,
            NULL,
            &error
        );

    assert(result != NULL);
    assert(error == NULL);

    assert(
        evidence_integrity_verification_result_get_status(
            result
        ) == EVIDENCE_INTEGRITY_STATUS_MODIFIED
    );

    computed_sha256 =
        evidence_integrity_verification_result_get_computed_sha256(
            result
        );

    assert(computed_sha256 != NULL);

    assert(
        strcmp(
            computed_sha256,
            TEST_SHA256_ABC
        ) != 0
    );

    assert(
        evidence_integrity_verification_result_get_size_bytes(
            result
        ) == 4
    );

    diagnostic =
        evidence_integrity_verification_result_get_diagnostic(
            result
        );

    assert(diagnostic != NULL);
    assert(diagnostic[0] != '\0');

    evidence_integrity_verification_result_free(
        result
    );

    assert(
        g_remove(
            file_path
        ) == 0
    );

    g_free(
        relative_path
    );

    g_free(
        file_path
    );

    test_evidence_integrity_verifier_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie qu'un fichier absent produit MISSING.
 */
static void test_evidence_integrity_verifier_missing(void)
{
    TestEvidenceIntegrityVerifierFixture fixture =
        test_evidence_integrity_verifier_fixture_create();

    EvidenceIntegrityVerificationResult *result = NULL;

    const char *diagnostic = NULL;

    char *relative_path = NULL;

    GError *error = NULL;

    relative_path =
        test_evidence_integrity_verifier_build_relative_path(
            "missing.bin"
        );

    assert(relative_path != NULL);

    result =
        evidence_integrity_verifier_verify(
            fixture.temporary_directory,
            relative_path,
            TEST_SHA256_ABC,
            NULL,
            &error
        );

    assert(result != NULL);
    assert(error == NULL);

    assert(
        evidence_integrity_verification_result_get_status(
            result
        ) == EVIDENCE_INTEGRITY_STATUS_MISSING
    );

    assert(
        evidence_integrity_verification_result_get_computed_sha256(
            result
        ) == NULL
    );

    assert(
        evidence_integrity_verification_result_get_size_bytes(
            result
        ) == 0
    );

    diagnostic =
        evidence_integrity_verification_result_get_diagnostic(
            result
        );

    assert(diagnostic != NULL);
    assert(diagnostic[0] != '\0');

    evidence_integrity_verification_result_free(
        result
    );

    g_free(
        relative_path
    );

    test_evidence_integrity_verifier_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie qu'un chemin absolu produit ERROR.
 */
static void test_evidence_integrity_verifier_absolute_path(void)
{
    TestEvidenceIntegrityVerifierFixture fixture =
        test_evidence_integrity_verifier_fixture_create();

    EvidenceIntegrityVerificationResult *result = NULL;

    char *absolute_path = NULL;

    GError *error = NULL;

    absolute_path =
        test_evidence_integrity_verifier_build_file_path(
            &fixture,
            "absolute.bin"
        );

    assert(absolute_path != NULL);

    result =
        evidence_integrity_verifier_verify(
            fixture.temporary_directory,
            absolute_path,
            TEST_SHA256_ABC,
            NULL,
            &error
        );

    assert(result != NULL);
    assert(error == NULL);

    assert(
        evidence_integrity_verification_result_get_status(
            result
        ) == EVIDENCE_INTEGRITY_STATUS_ERROR
    );

    assert(
        evidence_integrity_verification_result_get_diagnostic(
            result
        ) != NULL
    );

    evidence_integrity_verification_result_free(
        result
    );

    g_free(
        absolute_path
    );

    test_evidence_integrity_verifier_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie que la traversée ".." produit ERROR.
 */
static void test_evidence_integrity_verifier_parent_traversal(void)
{
    TestEvidenceIntegrityVerifierFixture fixture =
        test_evidence_integrity_verifier_fixture_create();

    EvidenceIntegrityVerificationResult *result = NULL;

    GError *error = NULL;

    result =
        evidence_integrity_verifier_verify(
            fixture.temporary_directory,
            "01_Preuves_Originales/../outside.bin",
            TEST_SHA256_ABC,
            NULL,
            &error
        );

    assert(result != NULL);
    assert(error == NULL);

    assert(
        evidence_integrity_verification_result_get_status(
            result
        ) == EVIDENCE_INTEGRITY_STATUS_ERROR
    );

    assert(
        evidence_integrity_verification_result_get_diagnostic(
            result
        ) != NULL
    );

    evidence_integrity_verification_result_free(
        result
    );

    test_evidence_integrity_verifier_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie qu'un lien symbolique sortant produit ERROR.
 */
static void test_evidence_integrity_verifier_outgoing_symbolic_link(void)
{
    TestEvidenceIntegrityVerifierFixture fixture =
        test_evidence_integrity_verifier_fixture_create();

    EvidenceIntegrityVerificationResult *result = NULL;

    char *outside_directory = NULL;
    char *outside_file_path = NULL;
    char *link_path = NULL;
    char *relative_path = NULL;

    GError *error = NULL;

    outside_directory =
        g_dir_make_tmp(
            "labfy-integrity-outside-test-XXXXXX",
            &error
        );

    assert(outside_directory != NULL);
    assert(error == NULL);

    outside_file_path =
        g_build_filename(
            outside_directory,
            "outside.bin",
            NULL
        );

    link_path =
        test_evidence_integrity_verifier_build_file_path(
            &fixture,
            "outside_link.bin"
        );

    relative_path =
        test_evidence_integrity_verifier_build_relative_path(
            "outside_link.bin"
        );

    assert(outside_file_path != NULL);
    assert(link_path != NULL);
    assert(relative_path != NULL);

    assert(
        g_file_set_contents(
            outside_file_path,
            "abc",
            3,
            &error
        )
    );

    assert(error == NULL);

    assert(
        symlink(
            outside_file_path,
            link_path
        ) == 0
    );

    result =
        evidence_integrity_verifier_verify(
            fixture.temporary_directory,
            relative_path,
            TEST_SHA256_ABC,
            NULL,
            &error
        );

    assert(result != NULL);
    assert(error == NULL);

    assert(
        evidence_integrity_verification_result_get_status(
            result
        ) == EVIDENCE_INTEGRITY_STATUS_ERROR
    );

    assert(
        evidence_integrity_verification_result_get_diagnostic(
            result
        ) != NULL
    );

    evidence_integrity_verification_result_free(
        result
    );

    assert(
        g_remove(
            link_path
        ) == 0
    );

    assert(
        g_remove(
            outside_file_path
        ) == 0
    );

    assert(
        g_rmdir(
            outside_directory
        ) == 0
    );

    g_free(
        relative_path
    );

    g_free(
        link_path
    );

    g_free(
        outside_file_path
    );

    g_free(
        outside_directory
    );

    test_evidence_integrity_verifier_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie qu'un dossier produit ERROR.
 */
static void test_evidence_integrity_verifier_directory(void)
{
    TestEvidenceIntegrityVerifierFixture fixture =
        test_evidence_integrity_verifier_fixture_create();

    EvidenceIntegrityVerificationResult *result = NULL;

    char *directory_path = NULL;
    char *relative_path = NULL;

    GError *error = NULL;

    directory_path =
        test_evidence_integrity_verifier_build_file_path(
            &fixture,
            "directory"
        );

    relative_path =
        test_evidence_integrity_verifier_build_relative_path(
            "directory"
        );

    assert(directory_path != NULL);
    assert(relative_path != NULL);

    assert(
        g_mkdir(
            directory_path,
            0700
        ) == 0
    );

    result =
        evidence_integrity_verifier_verify(
            fixture.temporary_directory,
            relative_path,
            TEST_SHA256_ABC,
            NULL,
            &error
        );

    assert(result != NULL);
    assert(error == NULL);

    assert(
        evidence_integrity_verification_result_get_status(
            result
        ) == EVIDENCE_INTEGRITY_STATUS_ERROR
    );

    evidence_integrity_verification_result_free(
        result
    );

    assert(
        g_rmdir(
            directory_path
        ) == 0
    );

    g_free(
        relative_path
    );

    g_free(
        directory_path
    );

    test_evidence_integrity_verifier_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie qu'un tube nommé produit ERROR sans blocage.
 */
static void test_evidence_integrity_verifier_named_pipe(void)
{
    TestEvidenceIntegrityVerifierFixture fixture =
        test_evidence_integrity_verifier_fixture_create();

    EvidenceIntegrityVerificationResult *result = NULL;

    char *pipe_path = NULL;
    char *relative_path = NULL;

    GError *error = NULL;

    pipe_path =
        test_evidence_integrity_verifier_build_file_path(
            &fixture,
            "proof.fifo"
        );

    relative_path =
        test_evidence_integrity_verifier_build_relative_path(
            "proof.fifo"
        );

    assert(pipe_path != NULL);
    assert(relative_path != NULL);

    assert(
        mkfifo(
            pipe_path,
            0600
        ) == 0
    );

    result =
        evidence_integrity_verifier_verify(
            fixture.temporary_directory,
            relative_path,
            TEST_SHA256_ABC,
            NULL,
            &error
        );

    assert(result != NULL);
    assert(error == NULL);

    assert(
        evidence_integrity_verification_result_get_status(
            result
        ) == EVIDENCE_INTEGRITY_STATUS_ERROR
    );

    evidence_integrity_verification_result_free(
        result
    );

    assert(
        g_remove(
            pipe_path
        ) == 0
    );

    g_free(
        relative_path
    );

    g_free(
        pipe_path
    );

    test_evidence_integrity_verifier_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie une annulation demandée avant le démarrage.
 */
static void test_evidence_integrity_verifier_cancelled(void)
{
    TestEvidenceIntegrityVerifierFixture fixture =
        test_evidence_integrity_verifier_fixture_create();

    EvidenceIntegrityVerificationResult *result = NULL;

    GCancellable *cancellable = NULL;

    GError *error = NULL;

    cancellable =
        g_cancellable_new();

    assert(cancellable != NULL);

    g_cancellable_cancel(
        cancellable
    );

    result =
        evidence_integrity_verifier_verify(
            fixture.temporary_directory,
            "01_Preuves_Originales/Documents/cancelled.bin",
            TEST_SHA256_ABC,
            cancellable,
            &error
        );

    assert(result == NULL);

    test_evidence_integrity_verifier_assert_error(
        error,
        EVIDENCE_INTEGRITY_VERIFIER_ERROR_CANCELLED
    );

    g_clear_error(
        &error
    );

    g_object_unref(
        cancellable
    );

    test_evidence_integrity_verifier_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus des paramètres invalides.
 */
static void test_evidence_integrity_verifier_invalid_arguments(void)
{
    TestEvidenceIntegrityVerifierFixture fixture =
        test_evidence_integrity_verifier_fixture_create();

    EvidenceIntegrityVerificationResult *result = NULL;

    GError *error = NULL;

    result =
        evidence_integrity_verifier_verify(
            NULL,
            "01_Preuves_Originales/Documents/proof.bin",
            TEST_SHA256_ABC,
            NULL,
            &error
        );

    assert(result == NULL);

    test_evidence_integrity_verifier_assert_error(
        error,
        EVIDENCE_INTEGRITY_VERIFIER_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    result =
        evidence_integrity_verifier_verify(
            fixture.temporary_directory,
            NULL,
            TEST_SHA256_ABC,
            NULL,
            &error
        );

    assert(result == NULL);

    test_evidence_integrity_verifier_assert_error(
        error,
        EVIDENCE_INTEGRITY_VERIFIER_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    result =
        evidence_integrity_verifier_verify(
            fixture.temporary_directory,
            "01_Preuves_Originales/Documents/proof.bin",
            "sha256-invalide",
            NULL,
            &error
        );

    assert(result == NULL);

    test_evidence_integrity_verifier_assert_error(
        error,
        EVIDENCE_INTEGRITY_VERIFIER_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    test_evidence_integrity_verifier_fixture_clear(
        &fixture
    );
}

int main(void)
{
    test_evidence_integrity_verifier_valid();
    test_evidence_integrity_verifier_modified();
    test_evidence_integrity_verifier_missing();
    test_evidence_integrity_verifier_absolute_path();
    test_evidence_integrity_verifier_parent_traversal();
    test_evidence_integrity_verifier_outgoing_symbolic_link();
    test_evidence_integrity_verifier_directory();
    test_evidence_integrity_verifier_named_pipe();
    test_evidence_integrity_verifier_cancelled();
    test_evidence_integrity_verifier_invalid_arguments();

    printf(
        "EvidenceIntegrityVerifier : tous les tests sont valides.\n"
    );

    return 0;
}
