/******************************************************************************
 * @file test_file_hash.c
 * @brief Tests du calcul SHA-256 des fichiers de preuve.
 ******************************************************************************/

#define _POSIX_C_SOURCE 200809L

#ifndef FILE_HASH_ENABLE_TEST_HOOKS
#define FILE_HASH_ENABLE_TEST_HOOKS
#endif

#include "core/file_hash.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <sys/stat.h>

/**
 * @brief Environnement temporaire des tests FileHash.
 */
typedef struct
{
    char *temporary_directory;
} TestFileHashFixture;

/**
 * @brief Contexte d’annulation déclenchée après un bloc.
 */
typedef struct
{
    GCancellable *cancellable;
    guint block_count;
} TestFileHashCancellationContext;

/**
 * @brief Vérifie une erreur FileHash.
 */
static void test_file_hash_assert_error(
    const GError *error,
    FileHashError expected_error
)
{
    assert(error != NULL);
    assert(error->domain == FILE_HASH_ERROR);

    assert(
        error->code ==
        (gint) expected_error
    );

    assert(error->message != NULL);
    assert(error->message[0] != '\0');
}

/**
 * @brief Crée un dossier temporaire isolé.
 */
static TestFileHashFixture test_file_hash_fixture_create(void)
{
    TestFileHashFixture fixture = {0};
    GError *error = NULL;

    fixture.temporary_directory =
        g_dir_make_tmp(
            "labfy-file-hash-test-XXXXXX",
            &error
        );

    assert(fixture.temporary_directory != NULL);
    assert(error == NULL);

    return fixture;
}

/**
 * @brief Supprime le dossier temporaire.
 *
 * Tous les fichiers qu’il contient doivent avoir été supprimés avant.
 */
static void test_file_hash_fixture_clear(
    TestFileHashFixture *fixture
)
{
    assert(fixture != NULL);
    assert(fixture->temporary_directory != NULL);

    assert(
        g_rmdir(
            fixture->temporary_directory
        ) == 0
    );

    g_free(
        fixture->temporary_directory
    );

    fixture->temporary_directory = NULL;
}

/**
 * @brief Construit le chemin d’un fichier dans la fixture.
 */
static char *test_file_hash_build_path(
    const TestFileHashFixture *fixture,
    const char *file_name
)
{
    assert(fixture != NULL);
    assert(fixture->temporary_directory != NULL);
    assert(file_name != NULL);

    return g_build_filename(
        fixture->temporary_directory,
        file_name,
        NULL
    );
}

/**
 * @brief Vérifie le SHA-256 d’un fichier vide.
 */
static void test_file_hash_empty_file(void)
{
    TestFileHashFixture fixture =
        test_file_hash_fixture_create();

    char *file_path = NULL;
    char *sha256 = NULL;

    guint64 size_bytes = 42;

    GError *error = NULL;

    file_path =
        test_file_hash_build_path(
            &fixture,
            "empty.bin"
        );

    assert(file_path != NULL);

    assert(
        g_file_set_contents(
            file_path,
            "",
            0,
            &error
        )
    );

    assert(error == NULL);

    assert(
        file_hash_compute_sha256(
            file_path,
            NULL,
            &sha256,
            &size_bytes,
            &error
        )
    );

    assert(error == NULL);
    assert(sha256 != NULL);
    assert(size_bytes == 0);

    assert(
        strlen(
            sha256
        ) == 64
    );

    assert(
        strcmp(
            sha256,
            "e3b0c44298fc1c149afbf4c8996fb924"
            "27ae41e4649b934ca495991b7852b855"
        ) == 0
    );

    g_free(
        sha256
    );

    assert(
        g_remove(
            file_path
        ) == 0
    );

    g_free(
        file_path
    );

    test_file_hash_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le SHA-256 du contenu connu « abc ».
 */
static void test_file_hash_known_content(void)
{
    TestFileHashFixture fixture =
        test_file_hash_fixture_create();

    char *file_path = NULL;
    char *sha256 = NULL;

    guint64 size_bytes = 0;

    GError *error = NULL;

    file_path =
        test_file_hash_build_path(
            &fixture,
            "abc.txt"
        );

    assert(file_path != NULL);

    /*
     * Aucun saut de ligne ne doit être ajouté.
     */
    assert(
        g_file_set_contents(
            file_path,
            "abc",
            3,
            &error
        )
    );

    assert(error == NULL);

    assert(
        file_hash_compute_sha256(
            file_path,
            NULL,
            &sha256,
            &size_bytes,
            &error
        )
    );

    assert(error == NULL);
    assert(sha256 != NULL);
    assert(size_bytes == 3);

    assert(
        strlen(
            sha256
        ) == 64
    );

    assert(
        strcmp(
            sha256,
            "ba7816bf8f01cfea414140de5dae2223"
            "b00361a396177a9cb410ff61f20015ad"
        ) == 0
    );

    g_free(
        sha256
    );

    assert(
        g_remove(
            file_path
        ) == 0
    );

    g_free(
        file_path
    );

    test_file_hash_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le calcul d’un fichier réparti sur plusieurs blocs.
 */
static void test_file_hash_multiple_blocks(void)
{
    TestFileHashFixture fixture =
        test_file_hash_fixture_create();

    const gsize data_size =
        (200U * 1024U) + 123U;

    guint8 *data = NULL;

    char *file_path = NULL;
    char *sha256 = NULL;

    guint64 size_bytes = 0;
    gsize data_index = 0;

    GError *error = NULL;

    file_path =
        test_file_hash_build_path(
            &fixture,
            "multiple_blocks.bin"
        );

    assert(file_path != NULL);

    data = g_malloc(
        data_size
    );

    assert(data != NULL);

    /*
     * Motif déterministe couvrant toutes les limites de blocs.
     */
    for (data_index = 0;
         data_index < data_size;
         data_index++)
    {
        data[data_index] =
            (guint8) (data_index % 251U);
    }

    assert(
        g_file_set_contents(
            file_path,
            (const char *) data,
            (gssize) data_size,
            &error
        )
    );

    assert(error == NULL);

    assert(
        file_hash_compute_sha256(
            file_path,
            NULL,
            &sha256,
            &size_bytes,
            &error
        )
    );

    assert(error == NULL);
    assert(sha256 != NULL);

    assert(
        size_bytes ==
        (guint64) data_size
    );

    assert(
        strcmp(
            sha256,
            "bc5ad58e926b1f79d9840418910f47ba"
            "1e947fb137170b8d5403a537b888c4af"
        ) == 0
    );

    g_free(
        sha256
    );

    g_free(
        data
    );

    assert(
        g_remove(
            file_path
        ) == 0
    );

    g_free(
        file_path
    );

    test_file_hash_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie que les données sont traitées comme des octets.
 */
static void test_file_hash_binary_content(void)
{
    TestFileHashFixture fixture =
        test_file_hash_fixture_create();

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

    char *file_path = NULL;
    char *sha256 = NULL;

    guint64 size_bytes = 0;

    GError *error = NULL;

    file_path =
        test_file_hash_build_path(
            &fixture,
            "binary.bin"
        );

    assert(file_path != NULL);

    assert(
        g_file_set_contents(
            file_path,
            (const char *) binary_data,
            (gssize) sizeof(binary_data),
            &error
        )
    );

    assert(error == NULL);

    assert(
        file_hash_compute_sha256(
            file_path,
            NULL,
            &sha256,
            &size_bytes,
            &error
        )
    );

    assert(error == NULL);
    assert(sha256 != NULL);

    assert(
        size_bytes ==
        sizeof(binary_data)
    );

    assert(
        strcmp(
            sha256,
            "c90d3a7754f84abc867e780e5a0c0f8d"
            "a5c663bd1766ad89e538536a3763f367"
        ) == 0
    );

    g_free(
        sha256
    );

    assert(
        g_remove(
            file_path
        ) == 0
    );

    g_free(
        file_path
    );

    test_file_hash_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus des paramètres invalides.
 */
static void test_file_hash_invalid_arguments(void)
{
    char *sha256 = NULL;
    guint64 size_bytes = 123;

    GError *error = NULL;

    assert(
        !file_hash_compute_sha256(
            NULL,
            NULL,
            &sha256,
            &size_bytes,
            &error
        )
    );

    assert(sha256 == NULL);
    assert(size_bytes == 0);

    test_file_hash_assert_error(
        error,
        FILE_HASH_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    size_bytes = 123;

    assert(
        !file_hash_compute_sha256(
            "",
            NULL,
            &sha256,
            &size_bytes,
            &error
        )
    );

    assert(sha256 == NULL);
    assert(size_bytes == 0);

    test_file_hash_assert_error(
        error,
        FILE_HASH_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    size_bytes = 123;

    assert(
        !file_hash_compute_sha256(
            "/tmp/file-hash-invalid",
            NULL,
            NULL,
            &size_bytes,
            &error
        )
    );

    assert(size_bytes == 0);

    test_file_hash_assert_error(
        error,
        FILE_HASH_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    assert(
        !file_hash_compute_sha256(
            "/tmp/file-hash-invalid",
            NULL,
            &sha256,
            NULL,
            &error
        )
    );

    assert(sha256 == NULL);

    test_file_hash_assert_error(
        error,
        FILE_HASH_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );
}

/**
 * @brief Vérifie que le module ne remplace pas une chaîne appartenant
 *        déjà à l’appelant.
 */
static void test_file_hash_non_null_output(void)
{
    char *sha256 =
        g_strdup(
            "ancienne-valeur"
        );

    guint64 size_bytes = 123;

    GError *error = NULL;

    assert(sha256 != NULL);

    assert(
        !file_hash_compute_sha256(
            "/tmp/file-hash-output-test",
            NULL,
            &sha256,
            &size_bytes,
            &error
        )
    );

    /*
     * La chaîne reste la propriété de l’appelant et n’est pas modifiée.
     */
    assert(
        strcmp(
            sha256,
            "ancienne-valeur"
        ) == 0
    );

    assert(size_bytes == 0);

    test_file_hash_assert_error(
        error,
        FILE_HASH_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    g_free(
        sha256
    );
}

/**
 * @brief Vérifie l’erreur produite pour un chemin absent.
 */
static void test_file_hash_missing_file(void)
{
    TestFileHashFixture fixture =
        test_file_hash_fixture_create();

    char *file_path = NULL;
    char *sha256 = NULL;

    guint64 size_bytes = 123;

    GError *error = NULL;

    file_path =
        test_file_hash_build_path(
            &fixture,
            "absent.bin"
        );

    assert(file_path != NULL);

    assert(
        !file_hash_compute_sha256(
            file_path,
            NULL,
            &sha256,
            &size_bytes,
            &error
        )
    );

    assert(sha256 == NULL);
    assert(size_bytes == 0);

    test_file_hash_assert_error(
        error,
        FILE_HASH_ERROR_NOT_FOUND
    );

    g_clear_error(
        &error
    );

    g_free(
        file_path
    );

    test_file_hash_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie qu’un dossier est refusé.
 */
static void test_file_hash_directory(void)
{
    TestFileHashFixture fixture =
        test_file_hash_fixture_create();

    char *sha256 = NULL;
    guint64 size_bytes = 123;

    GError *error = NULL;

    assert(
        !file_hash_compute_sha256(
            fixture.temporary_directory,
            NULL,
            &sha256,
            &size_bytes,
            &error
        )
    );

    assert(sha256 == NULL);
    assert(size_bytes == 0);

    test_file_hash_assert_error(
        error,
        FILE_HASH_ERROR_NOT_REGULAR
    );

    g_clear_error(
        &error
    );

    test_file_hash_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie qu’un lien symbolique n’est jamais suivi.
 */
static void test_file_hash_symbolic_link(void)
{
    TestFileHashFixture fixture =
        test_file_hash_fixture_create();

    char *target_path = NULL;
    char *link_path = NULL;
    char *sha256 = NULL;

    guint64 size_bytes = 123;

    GError *error = NULL;

    target_path =
        test_file_hash_build_path(
            &fixture,
            "target.bin"
        );

    link_path =
        test_file_hash_build_path(
            &fixture,
            "link.bin"
        );

    assert(target_path != NULL);
    assert(link_path != NULL);

    assert(
        g_file_set_contents(
            target_path,
            "contenu",
            7,
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

    assert(
        !file_hash_compute_sha256(
            link_path,
            NULL,
            &sha256,
            &size_bytes,
            &error
        )
    );

    assert(sha256 == NULL);
    assert(size_bytes == 0);

    test_file_hash_assert_error(
        error,
        FILE_HASH_ERROR_NOT_REGULAR
    );

    g_clear_error(
        &error
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
        link_path
    );

    g_free(
        target_path
    );

    test_file_hash_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie une annulation demandée avant l’ouverture du fichier.
 */
static void test_file_hash_cancelled_before_start(void)
{
    TestFileHashFixture fixture =
        test_file_hash_fixture_create();

    GCancellable *cancellable = NULL;

    char *file_path = NULL;
    char *sha256 = NULL;

    guint64 size_bytes = 123;

    GError *error = NULL;

    file_path =
        test_file_hash_build_path(
            &fixture,
            "cancelled.bin"
        );

    assert(file_path != NULL);

    assert(
        g_file_set_contents(
            file_path,
            "abc",
            3,
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

    assert(
        !file_hash_compute_sha256(
            file_path,
            cancellable,
            &sha256,
            &size_bytes,
            &error
        )
    );

    assert(sha256 == NULL);
    assert(size_bytes == 0);

    test_file_hash_assert_error(
        error,
        FILE_HASH_ERROR_CANCELLED
    );

    g_clear_error(
        &error
    );

    g_object_unref(
        cancellable
    );

    assert(
        g_remove(
            file_path
        ) == 0
    );

    g_free(
        file_path
    );

    test_file_hash_fixture_clear(
        &fixture
    );
}

/**
 * @brief Annule le calcul après la lecture du premier bloc.
 */
static void test_file_hash_cancel_after_first_block(
    guint64 total_size_bytes,
    gpointer user_data
)
{
    TestFileHashCancellationContext *context =
        user_data;

    assert(context != NULL);
    assert(context->cancellable != NULL);
    assert(total_size_bytes > 0);

    context->block_count++;

    if (context->block_count == 1)
    {
        g_cancellable_cancel(
            context->cancellable
        );
    }
}

/**
 * @brief Vérifie une annulation observée entre deux blocs.
 */
static void test_file_hash_cancelled_during_read(void)
{
    TestFileHashFixture fixture =
        test_file_hash_fixture_create();

    TestFileHashCancellationContext context = {0};

    const gsize data_size =
        256U * 1024U;

    guint8 *data = NULL;
    char *file_path = NULL;
    char *sha256 = NULL;

    guint64 size_bytes = 123;
    gsize data_index = 0;

    gboolean result = FALSE;
    GError *error = NULL;

    file_path =
        test_file_hash_build_path(
            &fixture,
            "cancelled_during_read.bin"
        );

    assert(file_path != NULL);

    data = g_malloc(
        data_size
    );

    assert(data != NULL);

    for (data_index = 0;
         data_index < data_size;
         data_index++)
    {
        data[data_index] =
            (guint8) (data_index % 251U);
    }

    assert(
        g_file_set_contents(
            file_path,
            (const char *) data,
            (gssize) data_size,
            &error
        )
    );

    assert(error == NULL);

    context.cancellable =
        g_cancellable_new();

    assert(context.cancellable != NULL);

    file_hash_test_set_block_hook(
        test_file_hash_cancel_after_first_block,
        &context
    );

    result =
        file_hash_compute_sha256(
            file_path,
            context.cancellable,
            &sha256,
            &size_bytes,
            &error
        );

    /*
     * Le crochet doit toujours être retiré avant de poursuivre les tests.
     */
    file_hash_test_set_block_hook(
        NULL,
        NULL
    );

    assert(!result);
    assert(sha256 == NULL);
    assert(size_bytes == 0);
    assert(context.block_count == 1);

    test_file_hash_assert_error(
        error,
        FILE_HASH_ERROR_CANCELLED
    );

    g_clear_error(
        &error
    );

    g_object_unref(
        context.cancellable
    );

    g_free(
        data
    );

    assert(
        g_remove(
            file_path
        ) == 0
    );

    g_free(
        file_path
    );

    test_file_hash_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie que plusieurs calculs successifs sont indépendants.
 */
static void test_file_hash_successive_calls(void)
{
    TestFileHashFixture fixture =
        test_file_hash_fixture_create();

    char *file_path = NULL;
    char *first_sha256 = NULL;
    char *second_sha256 = NULL;

    guint64 first_size_bytes = 0;
    guint64 second_size_bytes = 0;

    GError *error = NULL;

    file_path =
        test_file_hash_build_path(
            &fixture,
            "successive_calls.bin"
        );

    assert(file_path != NULL);

    assert(
        g_file_set_contents(
            file_path,
            "contenu stable",
            14,
            &error
        )
    );

    assert(error == NULL);

    assert(
        file_hash_compute_sha256(
            file_path,
            NULL,
            &first_sha256,
            &first_size_bytes,
            &error
        )
    );

    assert(error == NULL);
    assert(first_sha256 != NULL);

    assert(
        file_hash_compute_sha256(
            file_path,
            NULL,
            &second_sha256,
            &second_size_bytes,
            &error
        )
    );

    assert(error == NULL);
    assert(second_sha256 != NULL);

    assert(first_size_bytes == 14);
    assert(second_size_bytes == 14);

    assert(
        strcmp(
            first_sha256,
            second_sha256
        ) == 0
    );

    /*
     * Chaque appel doit retourner sa propre chaîne.
     */
    assert(first_sha256 != second_sha256);

    g_free(
        second_sha256
    );

    g_free(
        first_sha256
    );

    assert(
        g_remove(
            file_path
        ) == 0
    );

    g_free(
        file_path
    );

    test_file_hash_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie que la modification d’un fichier change son empreinte.
 */
static void test_file_hash_modified_file(void)
{
    TestFileHashFixture fixture =
        test_file_hash_fixture_create();

    char *file_path = NULL;
    char *first_sha256 = NULL;
    char *second_sha256 = NULL;

    guint64 first_size_bytes = 0;
    guint64 second_size_bytes = 0;

    GError *error = NULL;

    file_path =
        test_file_hash_build_path(
            &fixture,
            "modified_file.bin"
        );

    assert(file_path != NULL);

    assert(
        g_file_set_contents(
            file_path,
            "abc",
            3,
            &error
        )
    );

    assert(error == NULL);

    assert(
        file_hash_compute_sha256(
            file_path,
            NULL,
            &first_sha256,
            &first_size_bytes,
            &error
        )
    );

    assert(error == NULL);
    assert(first_sha256 != NULL);
    assert(first_size_bytes == 3);

    assert(
        g_file_set_contents(
            file_path,
            "abcdef",
            6,
            &error
        )
    );

    assert(error == NULL);

    assert(
        file_hash_compute_sha256(
            file_path,
            NULL,
            &second_sha256,
            &second_size_bytes,
            &error
        )
    );

    assert(error == NULL);
    assert(second_sha256 != NULL);
    assert(second_size_bytes == 6);

    assert(
        strcmp(
            first_sha256,
            second_sha256
        ) != 0
    );

    g_free(
        second_sha256
    );

    g_free(
        first_sha256
    );

    assert(
        g_remove(
            file_path
        ) == 0
    );

    g_free(
        file_path
    );

    test_file_hash_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie qu’un tube nommé est refusé sans blocage.
 */
static void test_file_hash_named_pipe(void)
{
    TestFileHashFixture fixture =
        test_file_hash_fixture_create();

    char *pipe_path = NULL;
    char *sha256 = NULL;

    guint64 size_bytes = 123;

    GError *error = NULL;

    pipe_path =
        test_file_hash_build_path(
            &fixture,
            "preuve.fifo"
        );

    assert(pipe_path != NULL);

    assert(
        mkfifo(
            pipe_path,
            0600
        ) == 0
    );

    assert(
        !file_hash_compute_sha256(
            pipe_path,
            NULL,
            &sha256,
            &size_bytes,
            &error
        )
    );

    assert(sha256 == NULL);
    assert(size_bytes == 0);

    test_file_hash_assert_error(
        error,
        FILE_HASH_ERROR_NOT_REGULAR
    );

    g_clear_error(
        &error
    );

    assert(
        g_remove(
            pipe_path
        ) == 0
    );

    g_free(
        pipe_path
    );

    test_file_hash_fixture_clear(
        &fixture
    );
}

int main(void)
{
    test_file_hash_empty_file();
    test_file_hash_known_content();
    test_file_hash_multiple_blocks();
    test_file_hash_binary_content();
    test_file_hash_invalid_arguments();
    test_file_hash_non_null_output();
    test_file_hash_missing_file();
    test_file_hash_directory();
    test_file_hash_symbolic_link();
    test_file_hash_cancelled_before_start();
    test_file_hash_cancelled_during_read();
    test_file_hash_successive_calls();
    test_file_hash_modified_file();
    test_file_hash_named_pipe();

    printf(
        "FileHash : tests des contenus connus valides.\n"
    );

    return 0;
}
