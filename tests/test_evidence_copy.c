/******************************************************************************
 * @file test_evidence_copy.c
 * @brief Tests de la copie sûre des fichiers de preuve.
 ******************************************************************************/

#define _POSIX_C_SOURCE 200809L

#ifndef EVIDENCE_COPY_ENABLE_TEST_HOOKS
#define EVIDENCE_COPY_ENABLE_TEST_HOOKS
#endif

#include "core/evidence_copy.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <sys/stat.h>

/**
 * @brief Environnement temporaire des tests EvidenceCopy.
 */
typedef struct
{
    char *root_directory;
    char *source_directory;
    char *destination_directory;
} TestEvidenceCopyFixture;

/**
 * @brief Contexte d’annulation pendant la copie.
 */
typedef struct
{
    GCancellable *cancellable;
    guint block_count;
} TestEvidenceCopyCancellationContext;

/**
 * @brief Contexte d’une erreur d’écriture simulée.
 */
typedef struct
{
    guint64 fail_after_size_bytes;
    guint call_count;
} TestEvidenceCopyWriteFailureContext;

/**
 * @brief Contexte de corruption avant vérification.
 */
typedef struct
{
    guint call_count;
} TestEvidenceCopyVerifyFailureContext;

/**
 * @brief Contexte de modification du fichier source.
 */
typedef struct
{
    const char *source_path;
    guint call_count;
} TestEvidenceCopySourceModificationContext;

/**
 * @brief Contexte d’un échec de nettoyage simulé.
 */
typedef struct
{
    guint call_count;
} TestEvidenceCopyCleanupFailureContext;

/**
 * @brief Crée un environnement temporaire isolé.
 */
static TestEvidenceCopyFixture
test_evidence_copy_fixture_create(void)
{
    TestEvidenceCopyFixture fixture = {0};

    GError *error = NULL;

    fixture.root_directory =
        g_dir_make_tmp(
            "labfy-evidence-copy-test-XXXXXX",
            &error
        );

    assert(error == NULL);
    assert(fixture.root_directory != NULL);

    fixture.source_directory =
        g_build_filename(
            fixture.root_directory,
            "source",
            NULL
        );

    fixture.destination_directory =
        g_build_filename(
            fixture.root_directory,
            "destination",
            NULL
        );

    assert(fixture.source_directory != NULL);
    assert(fixture.destination_directory != NULL);

    assert(
        g_mkdir(
            fixture.source_directory,
            0700
        ) == 0
    );

    assert(
        g_mkdir(
            fixture.destination_directory,
            0700
        ) == 0
    );

    return fixture;
}

/**
 * @brief Vérifie une erreur EvidenceCopy.
 */
static void test_evidence_copy_assert_error(
    const GError *error,
    EvidenceCopyError expected_error
)
{
    assert(error != NULL);
    assert(error->domain == EVIDENCE_COPY_ERROR);

    assert(
        error->code ==
        (gint) expected_error
    );

    assert(error->message != NULL);
    assert(error->message[0] != '\0');
}

/**
 * @brief Supprime un environnement temporaire vide.
 */
static void test_evidence_copy_fixture_clear(
    TestEvidenceCopyFixture *fixture
)
{
    assert(fixture != NULL);
    assert(fixture->root_directory != NULL);
    assert(fixture->source_directory != NULL);
    assert(fixture->destination_directory != NULL);

    assert(
        g_rmdir(
            fixture->destination_directory
        ) == 0
    );

    assert(
        g_rmdir(
            fixture->source_directory
        ) == 0
    );

    assert(
        g_rmdir(
            fixture->root_directory
        ) == 0
    );

    g_free(
        fixture->destination_directory
    );

    g_free(
        fixture->source_directory
    );

    g_free(
        fixture->root_directory
    );

    fixture->destination_directory = NULL;
    fixture->source_directory = NULL;
    fixture->root_directory = NULL;
}

/**
 * @brief Construit un chemin dans le dossier source.
 */
static char *test_evidence_copy_build_source_path(
    const TestEvidenceCopyFixture *fixture,
    const char *file_name
)
{
    assert(fixture != NULL);
    assert(fixture->source_directory != NULL);
    assert(file_name != NULL);

    return g_build_filename(
        fixture->source_directory,
        file_name,
        NULL
    );
}

/**
 * @brief Construit un chemin dans le dossier destination.
 */
static char *test_evidence_copy_build_destination_path(
    const TestEvidenceCopyFixture *fixture,
    const char *file_name
)
{
    assert(fixture != NULL);
    assert(fixture->destination_directory != NULL);
    assert(file_name != NULL);

    return g_build_filename(
        fixture->destination_directory,
        file_name,
        NULL
    );
}

/**
 * @brief Vérifie les accesseurs avec un résultat NULL.
 */
static void test_evidence_copy_null_result(void)
{
    assert(
        evidence_copy_result_get_destination_path(
            NULL
        ) == NULL
    );

    assert(
        evidence_copy_result_get_destination_name(
            NULL
        ) == NULL
    );

    assert(
        evidence_copy_result_get_sha256(
            NULL
        ) == NULL
    );

    assert(
        evidence_copy_result_get_size_bytes(
            NULL
        ) == 0
    );

    evidence_copy_result_free(
        NULL
    );
}

/**
 * @brief Vérifie la copie complète du contenu connu « abc ».
 */
static void test_evidence_copy_text_file(void)
{
    TestEvidenceCopyFixture fixture =
        test_evidence_copy_fixture_create();

    EvidenceCopyResult *result = NULL;

    char *source_path = NULL;
    char *destination_path = NULL;
    char *destination_name = NULL;

    char *source_contents = NULL;
    char *destination_contents = NULL;

    gsize source_contents_size = 0;
    gsize destination_contents_size = 0;

    GError *error = NULL;

    source_path =
        test_evidence_copy_build_source_path(
            &fixture,
            "original.txt"
        );

    destination_path =
        test_evidence_copy_build_destination_path(
            &fixture,
            "copie.txt"
        );

    destination_name =
        g_strdup(
            "copie.txt"
        );

    assert(source_path != NULL);
    assert(destination_path != NULL);
    assert(destination_name != NULL);

    assert(
        g_file_set_contents(
            source_path,
            "abc",
            3,
            &error
        )
    );

    assert(error == NULL);

    result =
        evidence_copy_file(
            source_path,
            fixture.destination_directory,
            destination_name,
            NULL,
            &error
        );

    assert(error == NULL);
    assert(result != NULL);

    /*
     * La chaîne fournie par l’appelant peut être libérée :
     * le résultat doit posséder sa propre copie.
     */
    g_free(
        destination_name
    );

    destination_name = NULL;

    assert(
        strcmp(
            evidence_copy_result_get_destination_path(
                result
            ),
            destination_path
        ) == 0
    );

    assert(
        strcmp(
            evidence_copy_result_get_destination_name(
                result
            ),
            "copie.txt"
        ) == 0
    );

    assert(
        strcmp(
            evidence_copy_result_get_sha256(
                result
            ),
            "ba7816bf8f01cfea414140de5dae2223"
            "b00361a396177a9cb410ff61f20015ad"
        ) == 0
    );

    assert(
        evidence_copy_result_get_size_bytes(
            result
        ) == 3
    );

    /*
     * Libérer le résultat ne doit pas supprimer la copie validée.
     */
    evidence_copy_result_free(
        result
    );

    result = NULL;

    assert(
        g_file_test(
            destination_path,
            G_FILE_TEST_IS_REGULAR
        )
    );

    assert(
        g_file_get_contents(
            source_path,
            &source_contents,
            &source_contents_size,
            &error
        )
    );

    assert(error == NULL);

    assert(
        g_file_get_contents(
            destination_path,
            &destination_contents,
            &destination_contents_size,
            &error
        )
    );

    assert(error == NULL);

    assert(source_contents_size == 3);
    assert(destination_contents_size == 3);

    assert(
        memcmp(
            source_contents,
            destination_contents,
            3
        ) == 0
    );

    assert(
        memcmp(
            source_contents,
            "abc",
            3
        ) == 0
    );

    g_free(
        destination_contents
    );

    g_free(
        source_contents
    );

    assert(
        g_remove(
            destination_path
        ) == 0
    );

    assert(
        g_remove(
            source_path
        ) == 0
    );

    g_free(
        destination_path
    );

    g_free(
        source_path
    );

    test_evidence_copy_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie la copie d’un fichier vide.
 */
static void test_evidence_copy_empty_file(void)
{
    TestEvidenceCopyFixture fixture =
        test_evidence_copy_fixture_create();

    EvidenceCopyResult *result = NULL;

    char *source_path = NULL;
    char *destination_path = NULL;
    char *destination_contents = NULL;

    gsize destination_contents_size = 42;

    GError *error = NULL;

    source_path =
        test_evidence_copy_build_source_path(
            &fixture,
            "empty.bin"
        );

    destination_path =
        test_evidence_copy_build_destination_path(
            &fixture,
            "empty-copy.bin"
        );

    assert(source_path != NULL);
    assert(destination_path != NULL);

    assert(
        g_file_set_contents(
            source_path,
            "",
            0,
            &error
        )
    );

    assert(error == NULL);

    result =
        evidence_copy_file(
            source_path,
            fixture.destination_directory,
            "empty-copy.bin",
            NULL,
            &error
        );

    assert(error == NULL);
    assert(result != NULL);

    assert(
        evidence_copy_result_get_size_bytes(
            result
        ) == 0
    );

    assert(
        strcmp(
            evidence_copy_result_get_sha256(
                result
            ),
            "e3b0c44298fc1c149afbf4c8996fb924"
            "27ae41e4649b934ca495991b7852b855"
        ) == 0
    );

    assert(
        strcmp(
            evidence_copy_result_get_destination_path(
                result
            ),
            destination_path
        ) == 0
    );

    evidence_copy_result_free(
        result
    );

    result = NULL;

    assert(
        g_file_test(
            destination_path,
            G_FILE_TEST_IS_REGULAR
        )
    );

    assert(
        g_file_get_contents(
            destination_path,
            &destination_contents,
            &destination_contents_size,
            &error
        )
    );

    assert(error == NULL);
    assert(destination_contents != NULL);
    assert(destination_contents_size == 0);

    g_free(
        destination_contents
    );

    assert(
        g_remove(
            destination_path
        ) == 0
    );

    assert(
        g_remove(
            source_path
        ) == 0
    );

    g_free(
        destination_path
    );

    g_free(
        source_path
    );

    test_evidence_copy_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie la copie exacte de données binaires.
 */
static void test_evidence_copy_binary_file(void)
{
    TestEvidenceCopyFixture fixture =
        test_evidence_copy_fixture_create();

    static const guint8 binary_data[] =
    {
        0x00,
        0xFF,
        0x80,
        0x41,
        0x00,
        0x7F,
        0x10
    };

    EvidenceCopyResult *result = NULL;

    char *source_path = NULL;
    char *destination_path = NULL;
    char *destination_contents = NULL;

    gsize destination_contents_size = 0;

    GError *error = NULL;

    source_path =
        test_evidence_copy_build_source_path(
            &fixture,
            "binary-source.bin"
        );

    destination_path =
        test_evidence_copy_build_destination_path(
            &fixture,
            "binary-copy.bin"
        );

    assert(source_path != NULL);
    assert(destination_path != NULL);

    assert(
        g_file_set_contents(
            source_path,
            (const char *) binary_data,
            (gssize) sizeof(binary_data),
            &error
        )
    );

    assert(error == NULL);

    result =
        evidence_copy_file(
            source_path,
            fixture.destination_directory,
            "binary-copy.bin",
            NULL,
            &error
        );

    assert(error == NULL);
    assert(result != NULL);

    assert(
        evidence_copy_result_get_size_bytes(
            result
        ) == sizeof(binary_data)
    );

    assert(
        strcmp(
            evidence_copy_result_get_sha256(
                result
            ),
            "c90d3a7754f84abc867e780e5a0c0f8d"
            "a5c663bd1766ad89e538536a3763f367"
        ) == 0
    );

    evidence_copy_result_free(
        result
    );

    assert(
        g_file_get_contents(
            destination_path,
            &destination_contents,
            &destination_contents_size,
            &error
        )
    );

    assert(error == NULL);

    assert(
        destination_contents_size ==
        sizeof(binary_data)
    );

    assert(
        memcmp(
            destination_contents,
            binary_data,
            sizeof(binary_data)
        ) == 0
    );

    g_free(
        destination_contents
    );

    assert(
        g_remove(
            destination_path
        ) == 0
    );

    assert(
        g_remove(
            source_path
        ) == 0
    );

    g_free(
        destination_path
    );

    g_free(
        source_path
    );

    test_evidence_copy_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie la copie d’un fichier supérieur au tampon.
 */
static void test_evidence_copy_multiple_blocks(void)
{
    TestEvidenceCopyFixture fixture =
        test_evidence_copy_fixture_create();

    const gsize data_size =
        (200U * 1024U) + 123U;

    EvidenceCopyResult *result = NULL;

    guint8 *source_data = NULL;

    char *source_path = NULL;
    char *destination_path = NULL;
    char *destination_contents = NULL;

    gsize destination_contents_size = 0;
    gsize data_index = 0;

    GError *error = NULL;

    source_path =
        test_evidence_copy_build_source_path(
            &fixture,
            "large-source.bin"
        );

    destination_path =
        test_evidence_copy_build_destination_path(
            &fixture,
            "large-copy.bin"
        );

    assert(source_path != NULL);
    assert(destination_path != NULL);

    source_data =
        g_malloc(
            data_size
        );

    assert(source_data != NULL);

    for (data_index = 0;
         data_index < data_size;
         data_index++)
    {
        source_data[data_index] =
            (guint8) (data_index % 251U);
    }

    assert(
        g_file_set_contents(
            source_path,
            (const char *) source_data,
            (gssize) data_size,
            &error
        )
    );

    assert(error == NULL);

    result =
        evidence_copy_file(
            source_path,
            fixture.destination_directory,
            "large-copy.bin",
            NULL,
            &error
        );

    assert(error == NULL);
    assert(result != NULL);

    assert(
        evidence_copy_result_get_size_bytes(
            result
        ) == data_size
    );

    assert(
        strcmp(
            evidence_copy_result_get_sha256(
                result
            ),
            "bc5ad58e926b1f79d9840418910f47ba"
            "1e947fb137170b8d5403a537b888c4af"
        ) == 0
    );

    evidence_copy_result_free(
        result
    );

    assert(
        g_file_get_contents(
            destination_path,
            &destination_contents,
            &destination_contents_size,
            &error
        )
    );

    assert(error == NULL);
    assert(destination_contents_size == data_size);

    assert(
        memcmp(
            destination_contents,
            source_data,
            data_size
        ) == 0
    );

    g_free(
        destination_contents
    );

    g_free(
        source_data
    );

    assert(
        g_remove(
            destination_path
        ) == 0
    );

    assert(
        g_remove(
            source_path
        ) == 0
    );

    g_free(
        destination_path
    );

    g_free(
        source_path
    );

    test_evidence_copy_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie les permissions restrictives de la copie.
 */
static void test_evidence_copy_permissions(void)
{
    TestEvidenceCopyFixture fixture =
        test_evidence_copy_fixture_create();

    EvidenceCopyResult *result = NULL;

    struct stat destination_status;

    char *source_path = NULL;
    char *destination_path = NULL;

    GError *error = NULL;

    source_path =
        test_evidence_copy_build_source_path(
            &fixture,
            "permissions-source.bin"
        );

    destination_path =
        test_evidence_copy_build_destination_path(
            &fixture,
            "permissions-copy.bin"
        );

    assert(source_path != NULL);
    assert(destination_path != NULL);

    assert(
        g_file_set_contents(
            source_path,
            "preuve",
            6,
            &error
        )
    );

    assert(error == NULL);

    result =
        evidence_copy_file(
            source_path,
            fixture.destination_directory,
            "permissions-copy.bin",
            NULL,
            &error
        );

    assert(error == NULL);
    assert(result != NULL);

    assert(
        stat(
            destination_path,
            &destination_status
        ) == 0
    );

    assert(
        S_ISREG(
            destination_status.st_mode
        )
    );

    /*
     * Aucun droit pour le groupe ou les autres utilisateurs.
     */
    assert(
        (destination_status.st_mode & 0077) == 0
    );

    evidence_copy_result_free(
        result
    );

    assert(
        g_remove(
            destination_path
        ) == 0
    );

    assert(
        g_remove(
            source_path
        ) == 0
    );

    g_free(
        destination_path
    );

    g_free(
        source_path
    );

    test_evidence_copy_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie qu’un fichier existant n’est jamais écrasé.
 */
static void test_evidence_copy_destination_exists(void)
{
    TestEvidenceCopyFixture fixture =
        test_evidence_copy_fixture_create();

    EvidenceCopyResult *result = NULL;

    char *source_path = NULL;
    char *destination_path = NULL;

    char *source_contents = NULL;
    char *destination_contents = NULL;

    gsize source_contents_size = 0;
    gsize destination_contents_size = 0;

    GError *error = NULL;

    source_path =
        test_evidence_copy_build_source_path(
            &fixture,
            "existing-source.bin"
        );

    destination_path =
        test_evidence_copy_build_destination_path(
            &fixture,
            "existing-copy.bin"
        );

    assert(source_path != NULL);
    assert(destination_path != NULL);

    assert(
        g_file_set_contents(
            source_path,
            "nouvelle preuve",
            15,
            &error
        )
    );

    assert(error == NULL);

    assert(
        g_file_set_contents(
            destination_path,
            "ne pas modifier",
            15,
            &error
        )
    );

    assert(error == NULL);

    result =
        evidence_copy_file(
            source_path,
            fixture.destination_directory,
            "existing-copy.bin",
            NULL,
            &error
        );

    assert(result == NULL);

    test_evidence_copy_assert_error(
        error,
        EVIDENCE_COPY_ERROR_DESTINATION_EXISTS
    );

    g_clear_error(
        &error
    );

    assert(
        g_file_get_contents(
            source_path,
            &source_contents,
            &source_contents_size,
            &error
        )
    );

    assert(error == NULL);

    assert(
        g_file_get_contents(
            destination_path,
            &destination_contents,
            &destination_contents_size,
            &error
        )
    );

    assert(error == NULL);

    assert(source_contents_size == 15);
    assert(destination_contents_size == 15);

    assert(
        memcmp(
            source_contents,
            "nouvelle preuve",
            15
        ) == 0
    );

    assert(
        memcmp(
            destination_contents,
            "ne pas modifier",
            15
        ) == 0
    );

    g_free(
        destination_contents
    );

    g_free(
        source_contents
    );

    assert(
        g_remove(
            destination_path
        ) == 0
    );

    assert(
        g_remove(
            source_path
        ) == 0
    );

    g_free(
        destination_path
    );

    g_free(
        source_path
    );

    test_evidence_copy_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie qu’aucun fichier destination n’a été créé.
 */
static void test_evidence_copy_assert_destination_absent(
    const char *destination_path
)
{
    assert(destination_path != NULL);

    assert(
        !g_file_test(
            destination_path,
            G_FILE_TEST_EXISTS
        )
    );
}

/**
 * @brief Vérifie le refus des paramètres obligatoires invalides.
 */
static void test_evidence_copy_invalid_arguments(void)
{
    TestEvidenceCopyFixture fixture =
        test_evidence_copy_fixture_create();

    EvidenceCopyResult *result = NULL;

    GError *error = NULL;

    result =
        evidence_copy_file(
            NULL,
            fixture.destination_directory,
            "copy.bin",
            NULL,
            &error
        );

    assert(result == NULL);

    test_evidence_copy_assert_error(
        error,
        EVIDENCE_COPY_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    result =
        evidence_copy_file(
            "",
            fixture.destination_directory,
            "copy.bin",
            NULL,
            &error
        );

    assert(result == NULL);

    test_evidence_copy_assert_error(
        error,
        EVIDENCE_COPY_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    result =
        evidence_copy_file(
            "/tmp/source.bin",
            NULL,
            "copy.bin",
            NULL,
            &error
        );

    assert(result == NULL);

    test_evidence_copy_assert_error(
        error,
        EVIDENCE_COPY_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    result =
        evidence_copy_file(
            "/tmp/source.bin",
            "",
            "copy.bin",
            NULL,
            &error
        );

    assert(result == NULL);

    test_evidence_copy_assert_error(
        error,
        EVIDENCE_COPY_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    test_evidence_copy_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus des noms pouvant représenter un chemin.
 */
static void test_evidence_copy_invalid_destination_names(void)
{
    TestEvidenceCopyFixture fixture =
        test_evidence_copy_fixture_create();

    static const char *invalid_names[] =
    {
        NULL,
        "",
        ".",
        "..",
        "subdirectory/copy.bin",
        "subdirectory\\copy.bin"
    };

    EvidenceCopyResult *result = NULL;

    char *source_path = NULL;

    gsize name_index = 0;

    GError *error = NULL;

    source_path =
        test_evidence_copy_build_source_path(
            &fixture,
            "valid-source.bin"
        );

    assert(source_path != NULL);

    assert(
        g_file_set_contents(
            source_path,
            "source",
            6,
            &error
        )
    );

    assert(error == NULL);

    for (name_index = 0;
         name_index < G_N_ELEMENTS(invalid_names);
         name_index++)
    {
        result =
            evidence_copy_file(
                source_path,
                fixture.destination_directory,
                invalid_names[name_index],
                NULL,
                &error
            );

        assert(result == NULL);

        test_evidence_copy_assert_error(
            error,
            EVIDENCE_COPY_ERROR_INVALID_ARGUMENT
        );

        g_clear_error(
            &error
        );
    }

    assert(
        g_remove(
            source_path
        ) == 0
    );

    g_free(
        source_path
    );

    test_evidence_copy_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d’un fichier source absent.
 */
static void test_evidence_copy_missing_source(void)
{
    TestEvidenceCopyFixture fixture =
        test_evidence_copy_fixture_create();

    EvidenceCopyResult *result = NULL;

    char *source_path = NULL;
    char *destination_path = NULL;

    GError *error = NULL;

    source_path =
        test_evidence_copy_build_source_path(
            &fixture,
            "missing.bin"
        );

    destination_path =
        test_evidence_copy_build_destination_path(
            &fixture,
            "copy.bin"
        );

    assert(source_path != NULL);
    assert(destination_path != NULL);

    result =
        evidence_copy_file(
            source_path,
            fixture.destination_directory,
            "copy.bin",
            NULL,
            &error
        );

    assert(result == NULL);

    test_evidence_copy_assert_error(
        error,
        EVIDENCE_COPY_ERROR_SOURCE_NOT_FOUND
    );

    g_clear_error(
        &error
    );

    test_evidence_copy_assert_destination_absent(
        destination_path
    );

    g_free(
        destination_path
    );

    g_free(
        source_path
    );

    test_evidence_copy_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie qu’un dossier ne peut pas devenir une preuve.
 */
static void test_evidence_copy_source_directory(void)
{
    TestEvidenceCopyFixture fixture =
        test_evidence_copy_fixture_create();

    EvidenceCopyResult *result = NULL;

    char *destination_path = NULL;

    GError *error = NULL;

    destination_path =
        test_evidence_copy_build_destination_path(
            &fixture,
            "copy.bin"
        );

    assert(destination_path != NULL);

    result =
        evidence_copy_file(
            fixture.source_directory,
            fixture.destination_directory,
            "copy.bin",
            NULL,
            &error
        );

    assert(result == NULL);

    test_evidence_copy_assert_error(
        error,
        EVIDENCE_COPY_ERROR_SOURCE_NOT_REGULAR
    );

    g_clear_error(
        &error
    );

    test_evidence_copy_assert_destination_absent(
        destination_path
    );

    g_free(
        destination_path
    );

    test_evidence_copy_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie qu’un lien symbolique source n’est jamais suivi.
 */
static void test_evidence_copy_source_symbolic_link(void)
{
    TestEvidenceCopyFixture fixture =
        test_evidence_copy_fixture_create();

    EvidenceCopyResult *result = NULL;

    char *target_path = NULL;
    char *link_path = NULL;
    char *destination_path = NULL;

    GError *error = NULL;

    target_path =
        test_evidence_copy_build_source_path(
            &fixture,
            "target.bin"
        );

    link_path =
        test_evidence_copy_build_source_path(
            &fixture,
            "source-link.bin"
        );

    destination_path =
        test_evidence_copy_build_destination_path(
            &fixture,
            "copy.bin"
        );

    assert(target_path != NULL);
    assert(link_path != NULL);
    assert(destination_path != NULL);

    assert(
        g_file_set_contents(
            target_path,
            "secret",
            6,
            &error
        )
    );

    assert(error == NULL);

    assert(
        symlink(
            target_path,
            link_path
        ) == 0
    );

    result =
        evidence_copy_file(
            link_path,
            fixture.destination_directory,
            "copy.bin",
            NULL,
            &error
        );

    assert(result == NULL);

    test_evidence_copy_assert_error(
        error,
        EVIDENCE_COPY_ERROR_SOURCE_NOT_REGULAR
    );

    g_clear_error(
        &error
    );

    test_evidence_copy_assert_destination_absent(
        destination_path
    );

    assert(
        g_remove(
            link_path
        ) == 0
    );

    assert(
        g_remove(
            target_path
        ) == 0
    );

    g_free(
        destination_path
    );

    g_free(
        link_path
    );

    g_free(
        target_path
    );

    test_evidence_copy_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie qu’un tube nommé est refusé sans blocage.
 */
static void test_evidence_copy_source_named_pipe(void)
{
    TestEvidenceCopyFixture fixture =
        test_evidence_copy_fixture_create();

    EvidenceCopyResult *result = NULL;

    char *pipe_path = NULL;
    char *destination_path = NULL;

    GError *error = NULL;

    pipe_path =
        test_evidence_copy_build_source_path(
            &fixture,
            "source.fifo"
        );

    destination_path =
        test_evidence_copy_build_destination_path(
            &fixture,
            "copy.bin"
        );

    assert(pipe_path != NULL);
    assert(destination_path != NULL);

    assert(
        mkfifo(
            pipe_path,
            0600
        ) == 0
    );

    result =
        evidence_copy_file(
            pipe_path,
            fixture.destination_directory,
            "copy.bin",
            NULL,
            &error
        );

    assert(result == NULL);

    test_evidence_copy_assert_error(
        error,
        EVIDENCE_COPY_ERROR_SOURCE_NOT_REGULAR
    );

    g_clear_error(
        &error
    );

    test_evidence_copy_assert_destination_absent(
        destination_path
    );

    assert(
        g_remove(
            pipe_path
        ) == 0
    );

    g_free(
        destination_path
    );

    g_free(
        pipe_path
    );

    test_evidence_copy_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie une annulation demandée avant l’import.
 */
static void test_evidence_copy_cancelled_before_start(void)
{
    TestEvidenceCopyFixture fixture =
        test_evidence_copy_fixture_create();

    EvidenceCopyResult *result = NULL;

    GCancellable *cancellable = NULL;

    char *source_path = NULL;
    char *destination_path = NULL;

    GError *error = NULL;

    source_path =
        test_evidence_copy_build_source_path(
            &fixture,
            "cancelled-source.bin"
        );

    destination_path =
        test_evidence_copy_build_destination_path(
            &fixture,
            "cancelled-copy.bin"
        );

    assert(source_path != NULL);
    assert(destination_path != NULL);

    assert(
        g_file_set_contents(
            source_path,
            "source stable",
            13,
            &error
        )
    );

    assert(error == NULL);

    cancellable =
        g_cancellable_new();

    assert(cancellable != NULL);

    g_cancellable_cancel(
        cancellable
    );

    result =
        evidence_copy_file(
            source_path,
            fixture.destination_directory,
            "cancelled-copy.bin",
            cancellable,
            &error
        );

    assert(result == NULL);

    test_evidence_copy_assert_error(
        error,
        EVIDENCE_COPY_ERROR_CANCELLED
    );

    g_clear_error(
        &error
    );

    test_evidence_copy_assert_destination_absent(
        destination_path
    );

    g_object_unref(
        cancellable
    );

    assert(
        g_remove(
            source_path
        ) == 0
    );

    g_free(
        destination_path
    );

    g_free(
        source_path
    );

    test_evidence_copy_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d’un dossier destination absent.
 */
static void test_evidence_copy_missing_destination_directory(void)
{
    TestEvidenceCopyFixture fixture =
        test_evidence_copy_fixture_create();

    EvidenceCopyResult *result = NULL;

    char *source_path = NULL;
    char *missing_directory = NULL;
    char *destination_path = NULL;

    GError *error = NULL;

    source_path =
        test_evidence_copy_build_source_path(
            &fixture,
            "source.bin"
        );

    missing_directory =
        g_build_filename(
            fixture.root_directory,
            "missing-destination",
            NULL
        );

    destination_path =
        g_build_filename(
            missing_directory,
            "copy.bin",
            NULL
        );

    assert(source_path != NULL);
    assert(missing_directory != NULL);
    assert(destination_path != NULL);

    assert(
        g_file_set_contents(
            source_path,
            "preuve",
            6,
            &error
        )
    );

    assert(error == NULL);

    result =
        evidence_copy_file(
            source_path,
            missing_directory,
            "copy.bin",
            NULL,
            &error
        );

    assert(result == NULL);

    test_evidence_copy_assert_error(
        error,
        EVIDENCE_COPY_ERROR_DESTINATION_INVALID
    );

    g_clear_error(
        &error
    );

    test_evidence_copy_assert_destination_absent(
        destination_path
    );

    assert(
        g_remove(
            source_path
        ) == 0
    );

    g_free(
        destination_path
    );

    g_free(
        missing_directory
    );

    g_free(
        source_path
    );

    test_evidence_copy_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie qu’un fichier ne peut pas servir de dossier destination.
 */
static void test_evidence_copy_destination_is_file(void)
{
    TestEvidenceCopyFixture fixture =
        test_evidence_copy_fixture_create();

    EvidenceCopyResult *result = NULL;

    char *source_path = NULL;
    char *fake_directory_path = NULL;

    char *fake_directory_contents = NULL;
    gsize fake_directory_contents_size = 0;

    GError *error = NULL;

    source_path =
        test_evidence_copy_build_source_path(
            &fixture,
            "source.bin"
        );

    fake_directory_path =
        g_build_filename(
            fixture.root_directory,
            "not-a-directory",
            NULL
        );

    assert(source_path != NULL);
    assert(fake_directory_path != NULL);

    assert(
        g_file_set_contents(
            source_path,
            "preuve",
            6,
            &error
        )
    );

    assert(error == NULL);

    assert(
        g_file_set_contents(
            fake_directory_path,
            "contenu intact",
            14,
            &error
        )
    );

    assert(error == NULL);

    result =
        evidence_copy_file(
            source_path,
            fake_directory_path,
            "copy.bin",
            NULL,
            &error
        );

    assert(result == NULL);

    test_evidence_copy_assert_error(
        error,
        EVIDENCE_COPY_ERROR_DESTINATION_INVALID
    );

    g_clear_error(
        &error
    );

    /*
     * Le fichier pris pour un dossier doit rester intact.
     */
    assert(
        g_file_get_contents(
            fake_directory_path,
            &fake_directory_contents,
            &fake_directory_contents_size,
            &error
        )
    );

    assert(error == NULL);
    assert(fake_directory_contents_size == 14);

    assert(
        memcmp(
            fake_directory_contents,
            "contenu intact",
            14
        ) == 0
    );

    g_free(
        fake_directory_contents
    );

    assert(
        g_remove(
            fake_directory_path
        ) == 0
    );

    assert(
        g_remove(
            source_path
        ) == 0
    );

    g_free(
        fake_directory_path
    );

    g_free(
        source_path
    );

    test_evidence_copy_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie qu’un dossier destination symbolique n’est pas suivi.
 */
static void test_evidence_copy_destination_symbolic_link(void)
{
    TestEvidenceCopyFixture fixture =
        test_evidence_copy_fixture_create();

    EvidenceCopyResult *result = NULL;

    char *source_path = NULL;
    char *link_path = NULL;
    char *real_destination_path = NULL;

    GError *error = NULL;

    source_path =
        test_evidence_copy_build_source_path(
            &fixture,
            "source.bin"
        );

    link_path =
        g_build_filename(
            fixture.root_directory,
            "destination-link",
            NULL
        );

    real_destination_path =
        test_evidence_copy_build_destination_path(
            &fixture,
            "copy.bin"
        );

    assert(source_path != NULL);
    assert(link_path != NULL);
    assert(real_destination_path != NULL);

    assert(
        g_file_set_contents(
            source_path,
            "preuve",
            6,
            &error
        )
    );

    assert(error == NULL);

    assert(
        symlink(
            fixture.destination_directory,
            link_path
        ) == 0
    );

    result =
        evidence_copy_file(
            source_path,
            link_path,
            "copy.bin",
            NULL,
            &error
        );

    assert(result == NULL);

    test_evidence_copy_assert_error(
        error,
        EVIDENCE_COPY_ERROR_DESTINATION_INVALID
    );

    g_clear_error(
        &error
    );

    /*
     * La cible réelle du lien ne doit recevoir aucun fichier.
     */
    test_evidence_copy_assert_destination_absent(
        real_destination_path
    );

    assert(
        g_remove(
            link_path
        ) == 0
    );

    assert(
        g_remove(
            source_path
        ) == 0
    );

    g_free(
        real_destination_path
    );

    g_free(
        link_path
    );

    g_free(
        source_path
    );

    test_evidence_copy_fixture_clear(
        &fixture
    );
}

/**
 * @brief Annule la copie après le premier bloc.
 */
static void test_evidence_copy_cancel_after_first_block(
    guint64 copied_size_bytes,
    gpointer user_data
)
{
    TestEvidenceCopyCancellationContext *context =
        user_data;

    assert(context != NULL);
    assert(context->cancellable != NULL);
    assert(copied_size_bytes > 0);

    context->block_count++;

    if (context->block_count == 1)
    {
        g_cancellable_cancel(
            context->cancellable
        );
    }
}

/**
 * @brief Vérifie l’annulation pendant une copie en cours.
 */
static void test_evidence_copy_cancelled_during_copy(void)
{
    TestEvidenceCopyFixture fixture =
        test_evidence_copy_fixture_create();

    TestEvidenceCopyCancellationContext context = {0};

    const gsize data_size =
        256U * 1024U;

    EvidenceCopyResult *result = NULL;

    guint8 *source_data = NULL;

    char *source_path = NULL;
    char *destination_path = NULL;

    gsize data_index = 0;

    GError *error = NULL;

    source_path =
        test_evidence_copy_build_source_path(
            &fixture,
            "cancel-source.bin"
        );

    destination_path =
        test_evidence_copy_build_destination_path(
            &fixture,
            "cancel-copy.bin"
        );

    assert(source_path != NULL);
    assert(destination_path != NULL);

    source_data =
        g_malloc(
            data_size
        );

    assert(source_data != NULL);

    for (data_index = 0;
         data_index < data_size;
         data_index++)
    {
        source_data[data_index] =
            (guint8) (data_index % 251U);
    }

    assert(
        g_file_set_contents(
            source_path,
            (const char *) source_data,
            (gssize) data_size,
            &error
        )
    );

    assert(error == NULL);

    context.cancellable =
        g_cancellable_new();

    assert(context.cancellable != NULL);

    evidence_copy_test_set_block_hook(
        test_evidence_copy_cancel_after_first_block,
        &context
    );

    result =
        evidence_copy_file(
            source_path,
            fixture.destination_directory,
            "cancel-copy.bin",
            context.cancellable,
            &error
        );

    /*
     * Toujours retirer le crochet avant les assertions suivantes.
     */
    evidence_copy_test_set_block_hook(
        NULL,
        NULL
    );

    assert(result == NULL);
    assert(context.block_count == 1);

    test_evidence_copy_assert_error(
        error,
        EVIDENCE_COPY_ERROR_CANCELLED
    );

    g_clear_error(
        &error
    );

    /*
     * La copie partielle doit avoir été supprimée.
     */
    test_evidence_copy_assert_destination_absent(
        destination_path
    );

    /*
     * Le fichier source doit rester intact.
     */
    assert(
        g_file_test(
            source_path,
            G_FILE_TEST_IS_REGULAR
        )
    );

    g_object_unref(
        context.cancellable
    );

    g_free(
        source_data
    );

    assert(
        g_remove(
            source_path
        ) == 0
    );

    g_free(
        destination_path
    );

    g_free(
        source_path
    );

    test_evidence_copy_fixture_clear(
        &fixture
    );
}

/**
 * @brief Déclenche une erreur après la quantité demandée.
 */
static gboolean test_evidence_copy_fail_write_after_size(
    guint64 written_size_bytes,
    gpointer user_data
)
{
    TestEvidenceCopyWriteFailureContext *context =
        user_data;

    assert(context != NULL);

    context->call_count++;

    return written_size_bytes >=
        context->fail_after_size_bytes;
}

/**
 * @brief Vérifie le nettoyage après une erreur d’écriture.
 */
static void test_evidence_copy_write_failure_cleanup(void)
{
    TestEvidenceCopyFixture fixture =
        test_evidence_copy_fixture_create();

    TestEvidenceCopyWriteFailureContext context = {0};

    const gsize data_size =
        256U * 1024U;

    EvidenceCopyResult *result = NULL;

    guint8 *source_data = NULL;

    char *source_path = NULL;
    char *destination_path = NULL;
    char *source_contents = NULL;

    gsize data_index = 0;
    gsize source_contents_size = 0;

    GError *error = NULL;

    source_path =
        test_evidence_copy_build_source_path(
            &fixture,
            "write-failure-source.bin"
        );

    destination_path =
        test_evidence_copy_build_destination_path(
            &fixture,
            "write-failure-copy.bin"
        );

    assert(source_path != NULL);
    assert(destination_path != NULL);

    source_data =
        g_malloc(
            data_size
        );

    assert(source_data != NULL);

    for (data_index = 0;
         data_index < data_size;
         data_index++)
    {
        source_data[data_index] =
            (guint8) (data_index % 251U);
    }

    assert(
        g_file_set_contents(
            source_path,
            (const char *) source_data,
            (gssize) data_size,
            &error
        )
    );

    assert(error == NULL);

    /*
     * Le premier bloc de 64 Kio est écrit.
     * L’écriture suivante doit échouer.
     */
    context.fail_after_size_bytes =
        64U * 1024U;

    evidence_copy_test_set_before_write_hook(
        test_evidence_copy_fail_write_after_size,
        &context
    );

    result =
        evidence_copy_file(
            source_path,
            fixture.destination_directory,
            "write-failure-copy.bin",
            NULL,
            &error
        );

    evidence_copy_test_set_before_write_hook(
        NULL,
        NULL
    );

    assert(result == NULL);
    assert(context.call_count >= 2);

    test_evidence_copy_assert_error(
        error,
        EVIDENCE_COPY_ERROR_WRITE
    );

    g_clear_error(
        &error
    );

    /*
     * Le fichier partiel ne doit plus exister.
     */
    test_evidence_copy_assert_destination_absent(
        destination_path
    );

    /*
     * Le fichier source doit rester strictement intact.
     */
    assert(
        g_file_get_contents(
            source_path,
            &source_contents,
            &source_contents_size,
            &error
        )
    );

    assert(error == NULL);
    assert(source_contents_size == data_size);

    assert(
        memcmp(
            source_contents,
            source_data,
            data_size
        ) == 0
    );

    g_free(
        source_contents
    );

    g_free(
        source_data
    );

    assert(
        g_remove(
            source_path
        ) == 0
    );

    g_free(
        destination_path
    );

    g_free(
        source_path
    );

    test_evidence_copy_fixture_clear(
        &fixture
    );
}

/**
 * @brief Corrompt volontairement la destination avant son contrôle.
 */
static void test_evidence_copy_corrupt_before_verify(
    const char *destination_path,
    gpointer user_data
)
{
    TestEvidenceCopyVerifyFailureContext *context =
        user_data;

    GError *error = NULL;

    assert(context != NULL);
    assert(destination_path != NULL);

    context->call_count++;

    assert(
        g_file_set_contents(
            destination_path,
            "copie corrompue",
            15,
            &error
        )
    );

    assert(error == NULL);
}

/**
 * @brief Vérifie le nettoyage après une corruption de la copie.
 */
static void test_evidence_copy_verification_failure_cleanup(void)
{
    TestEvidenceCopyFixture fixture =
        test_evidence_copy_fixture_create();

    TestEvidenceCopyVerifyFailureContext context = {0};

    EvidenceCopyResult *result = NULL;

    char *source_path = NULL;
    char *destination_path = NULL;
    char *source_contents = NULL;

    gsize source_contents_size = 0;

    GError *error = NULL;

    source_path =
        test_evidence_copy_build_source_path(
            &fixture,
            "verify-source.bin"
        );

    destination_path =
        test_evidence_copy_build_destination_path(
            &fixture,
            "verify-copy.bin"
        );

    assert(source_path != NULL);
    assert(destination_path != NULL);

    assert(
        g_file_set_contents(
            source_path,
            "preuve originale",
            16,
            &error
        )
    );

    assert(error == NULL);

    evidence_copy_test_set_before_verify_hook(
        test_evidence_copy_corrupt_before_verify,
        &context
    );

    result =
        evidence_copy_file(
            source_path,
            fixture.destination_directory,
            "verify-copy.bin",
            NULL,
            &error
        );

    evidence_copy_test_set_before_verify_hook(
        NULL,
        NULL
    );

    assert(result == NULL);
    assert(context.call_count == 1);

    test_evidence_copy_assert_error(
        error,
        EVIDENCE_COPY_ERROR_VERIFY
    );

    g_clear_error(
        &error
    );

    /*
     * La copie corrompue doit avoir été supprimée.
     */
    test_evidence_copy_assert_destination_absent(
        destination_path
    );

    /*
     * La source doit être restée intacte.
     */
    assert(
        g_file_get_contents(
            source_path,
            &source_contents,
            &source_contents_size,
            &error
        )
    );

    assert(error == NULL);
    assert(source_contents_size == 16);

    assert(
        memcmp(
            source_contents,
            "preuve originale",
            16
        ) == 0
    );

    g_free(
        source_contents
    );

    assert(
        g_remove(
            source_path
        ) == 0
    );

    g_free(
        destination_path
    );

    g_free(
        source_path
    );

    test_evidence_copy_fixture_clear(
        &fixture
    );
}

/**
 * @brief Modifie la source après sa copie.
 */
static void test_evidence_copy_modify_source_before_verify(
    const char *destination_path,
    gpointer user_data
)
{
    TestEvidenceCopySourceModificationContext *context =
        user_data;

    GError *error = NULL;

    assert(destination_path != NULL);
    assert(context != NULL);
    assert(context->source_path != NULL);

    context->call_count++;

    assert(
        g_file_set_contents(
            context->source_path,
            "source modifiee",
            15,
            &error
        )
    );

    assert(error == NULL);
}
/**
 * @brief Vérifie la détection d’une source modifiée pendant la copie.
 */
static void test_evidence_copy_source_modified_during_copy(void)
{
    TestEvidenceCopyFixture fixture =
        test_evidence_copy_fixture_create();

    TestEvidenceCopySourceModificationContext context = {0};

    EvidenceCopyResult *result = NULL;

    char *source_path = NULL;
    char *destination_path = NULL;
    char *source_contents = NULL;

    gsize source_contents_size = 0;

    GError *error = NULL;

    source_path =
        test_evidence_copy_build_source_path(
            &fixture,
            "changing-source.bin"
        );

    destination_path =
        test_evidence_copy_build_destination_path(
            &fixture,
            "changing-copy.bin"
        );

    assert(source_path != NULL);
    assert(destination_path != NULL);

    assert(
        g_file_set_contents(
            source_path,
            "source initiale",
            15,
            &error
        )
    );

    assert(error == NULL);

    context.source_path =
        source_path;

    evidence_copy_test_set_before_verify_hook(
        test_evidence_copy_modify_source_before_verify,
        &context
    );

    result =
        evidence_copy_file(
            source_path,
            fixture.destination_directory,
            "changing-copy.bin",
            NULL,
            &error
        );

    evidence_copy_test_set_before_verify_hook(
        NULL,
        NULL
    );

    assert(result == NULL);
    assert(context.call_count == 1);

    test_evidence_copy_assert_error(
        error,
        EVIDENCE_COPY_ERROR_VERIFY
    );

    g_clear_error(
        &error
    );

    /*
     * La destination n’est pas fiable et doit être supprimée.
     */
    test_evidence_copy_assert_destination_absent(
        destination_path
    );

    /*
     * EvidenceCopy ne doit pas tenter de restaurer ou modifier la source.
     * Elle reste donc dans l’état imposé par le test.
     */
    assert(
        g_file_get_contents(
            source_path,
            &source_contents,
            &source_contents_size,
            &error
        )
    );

    assert(error == NULL);
    assert(source_contents_size == 15);

    assert(
        memcmp(
            source_contents,
            "source modifiee",
            15
        ) == 0
    );

    g_free(
        source_contents
    );

    assert(
        g_remove(
            source_path
        ) == 0
    );

    g_free(
        destination_path
    );

    g_free(
        source_path
    );

    test_evidence_copy_fixture_clear(
        &fixture
    );
}

/**
 * @brief Simule systématiquement un échec de suppression.
 */
static gboolean test_evidence_copy_fail_cleanup(
    const char *destination_path,
    gpointer user_data
)
{
    TestEvidenceCopyCleanupFailureContext *context =
        user_data;

    assert(context != NULL);
    assert(destination_path != NULL);
    assert(destination_path[0] != '\0');

    context->call_count++;

    return TRUE;
}

/**
 * @brief Vérifie le signalement d’un fichier orphelin.
 */
static void test_evidence_copy_cleanup_failure_reported(void)
{
    TestEvidenceCopyFixture fixture =
        test_evidence_copy_fixture_create();

    TestEvidenceCopyVerifyFailureContext verify_context = {0};
    TestEvidenceCopyCleanupFailureContext cleanup_context = {0};

    EvidenceCopyResult *result = NULL;

    char *source_path = NULL;
    char *destination_path = NULL;

    GError *error = NULL;

    source_path =
        test_evidence_copy_build_source_path(
            &fixture,
            "cleanup-source.bin"
        );

    destination_path =
        test_evidence_copy_build_destination_path(
            &fixture,
            "cleanup-copy.bin"
        );

    assert(source_path != NULL);
    assert(destination_path != NULL);

    assert(
        g_file_set_contents(
            source_path,
            "preuve originale",
            16,
            &error
        )
    );

    assert(error == NULL);

    evidence_copy_test_set_before_verify_hook(
        test_evidence_copy_corrupt_before_verify,
        &verify_context
    );

    evidence_copy_test_set_cleanup_hook(
        test_evidence_copy_fail_cleanup,
        &cleanup_context
    );

    result =
        evidence_copy_file(
            source_path,
            fixture.destination_directory,
            "cleanup-copy.bin",
            NULL,
            &error
        );

    /*
     * Retirer les deux crochets avant toute autre opération.
     */
    evidence_copy_test_set_cleanup_hook(
        NULL,
        NULL
    );

    evidence_copy_test_set_before_verify_hook(
        NULL,
        NULL
    );

    assert(result == NULL);
    assert(verify_context.call_count == 1);
    assert(cleanup_context.call_count == 1);

    test_evidence_copy_assert_error(
        error,
        EVIDENCE_COPY_ERROR_CLEANUP
    );

    /*
     * Le message doit identifier le fichier potentiellement orphelin.
     */
    assert(
        strstr(
            error->message,
            destination_path
        ) != NULL
    );

    /*
     * La cause initiale doit rester visible.
     */
    assert(
        strstr(
            error->message,
            "Cause initiale"
        ) != NULL
    );

    g_clear_error(
        &error
    );

    /*
     * L’échec étant simulé, le fichier doit toujours exister.
     */
    assert(
        g_file_test(
            destination_path,
            G_FILE_TEST_IS_REGULAR
        )
    );

    /*
     * Le test effectue ensuite lui-même le nettoyage réel.
     */
    assert(
        g_remove(
            destination_path
        ) == 0
    );

    assert(
        g_remove(
            source_path
        ) == 0
    );

    g_free(
        destination_path
    );

    g_free(
        source_path
    );

    test_evidence_copy_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie l’indépendance de plusieurs copies successives.
 */
static void test_evidence_copy_successive_calls(void)
{
    TestEvidenceCopyFixture fixture =
        test_evidence_copy_fixture_create();

    EvidenceCopyResult *first_result = NULL;
    EvidenceCopyResult *second_result = NULL;

    char *first_source_path = NULL;
    char *second_source_path = NULL;

    char *first_destination_path = NULL;
    char *second_destination_path = NULL;

    GError *error = NULL;

    first_source_path =
        test_evidence_copy_build_source_path(
            &fixture,
            "first-source.bin"
        );

    second_source_path =
        test_evidence_copy_build_source_path(
            &fixture,
            "second-source.bin"
        );

    first_destination_path =
        test_evidence_copy_build_destination_path(
            &fixture,
            "first-copy.bin"
        );

    second_destination_path =
        test_evidence_copy_build_destination_path(
            &fixture,
            "second-copy.bin"
        );

    assert(first_source_path != NULL);
    assert(second_source_path != NULL);
    assert(first_destination_path != NULL);
    assert(second_destination_path != NULL);

    assert(
        g_file_set_contents(
            first_source_path,
            "premiere preuve",
            15,
            &error
        )
    );

    assert(error == NULL);

    assert(
        g_file_set_contents(
            second_source_path,
            "deuxieme preuve",
            15,
            &error
        )
    );

    assert(error == NULL);

    first_result =
        evidence_copy_file(
            first_source_path,
            fixture.destination_directory,
            "first-copy.bin",
            NULL,
            &error
        );

    assert(error == NULL);
    assert(first_result != NULL);

    second_result =
        evidence_copy_file(
            second_source_path,
            fixture.destination_directory,
            "second-copy.bin",
            NULL,
            &error
        );

    assert(error == NULL);
    assert(second_result != NULL);

    assert(
        strcmp(
            evidence_copy_result_get_destination_path(
                first_result
            ),
            first_destination_path
        ) == 0
    );

    assert(
        strcmp(
            evidence_copy_result_get_destination_path(
                second_result
            ),
            second_destination_path
        ) == 0
    );

    assert(
        strcmp(
            evidence_copy_result_get_destination_name(
                first_result
            ),
            "first-copy.bin"
        ) == 0
    );

    assert(
        strcmp(
            evidence_copy_result_get_destination_name(
                second_result
            ),
            "second-copy.bin"
        ) == 0
    );

    assert(
        evidence_copy_result_get_size_bytes(
            first_result
        ) == 15
    );

    assert(
        evidence_copy_result_get_size_bytes(
            second_result
        ) == 15
    );

    assert(
        strcmp(
            evidence_copy_result_get_sha256(
                first_result
            ),
            evidence_copy_result_get_sha256(
                second_result
            )
        ) != 0
    );

    assert(
        g_file_test(
            first_destination_path,
            G_FILE_TEST_IS_REGULAR
        )
    );

    assert(
        g_file_test(
            second_destination_path,
            G_FILE_TEST_IS_REGULAR
        )
    );

    evidence_copy_result_free(
        second_result
    );

    evidence_copy_result_free(
        first_result
    );

    assert(
        g_remove(
            second_destination_path
        ) == 0
    );

    assert(
        g_remove(
            first_destination_path
        ) == 0
    );

    assert(
        g_remove(
            second_source_path
        ) == 0
    );

    assert(
        g_remove(
            first_source_path
        ) == 0
    );

    g_free(
        second_destination_path
    );

    g_free(
        first_destination_path
    );

    g_free(
        second_source_path
    );

    g_free(
        first_source_path
    );

    test_evidence_copy_fixture_clear(
        &fixture
    );
}

int main(void)
{
    test_evidence_copy_null_result();
    test_evidence_copy_text_file();
    test_evidence_copy_empty_file();
    test_evidence_copy_binary_file();
    test_evidence_copy_multiple_blocks();
    test_evidence_copy_permissions();
    test_evidence_copy_destination_exists();
    test_evidence_copy_invalid_arguments();
    test_evidence_copy_invalid_destination_names();
    test_evidence_copy_missing_source();
    test_evidence_copy_source_directory();
    test_evidence_copy_source_symbolic_link();
    test_evidence_copy_source_named_pipe();
    test_evidence_copy_cancelled_before_start();
    test_evidence_copy_missing_destination_directory();
    test_evidence_copy_destination_is_file();
    test_evidence_copy_destination_symbolic_link();
    test_evidence_copy_cancelled_during_copy();
    test_evidence_copy_write_failure_cleanup();
    test_evidence_copy_verification_failure_cleanup();
    test_evidence_copy_source_modified_during_copy();
    test_evidence_copy_cleanup_failure_reported();
    test_evidence_copy_successive_calls();

    printf(
        "EvidenceCopy : premiers tests de copie valides.\n"
    );

    return 0;
}
