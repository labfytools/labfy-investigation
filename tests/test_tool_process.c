/******************************************************************************
 * @file test_tool_process.c
 * @brief Tests unitaires de l’exécution sécurisée des outils externes.
 ******************************************************************************/

#include "core/tool_process.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <string.h>
#include <sys/stat.h>

#include <signal.h>

/**
 * @struct ToolProcessFixture
 * @brief Environnement temporaire utilisé par les tests.
 */
typedef struct
{
    char *temporary_directory;
    char *tool_path;
    char *injection_path;
} ToolProcessFixture;

/**
 * @struct ToolProcessCancellationData
 * @brief Données transmises au thread chargé d'annuler une exécution.
 */
typedef struct
{
    GCancellable *cancellable;
    guint64 delay_microseconds;
} ToolProcessCancellationData;

/**
 * @brief Annule une opération après un court délai.
 *
 * @param user_data Pointeur vers ToolProcessCancellationData.
 *
 * @return Toujours NULL.
 */
static gpointer test_tool_process_cancel_after_delay(
    gpointer user_data
)
{
    ToolProcessCancellationData *cancellation_data =
        user_data;

    if (cancellation_data == NULL ||
        cancellation_data->cancellable == NULL)
    {
        return NULL;
    }

    g_usleep(
        cancellation_data->delay_microseconds
    );

    g_cancellable_cancel(
        cancellation_data->cancellable
    );

    return NULL;
}

/**
 * @brief Crée un script exécutable utilisé comme faux outil.
 *
 * @param fixture Fixture du test.
 * @param script_content Contenu complet du script.
 */
static void test_tool_process_write_script(
    ToolProcessFixture *fixture,
    const char *script_content
)
{
    GError *error = NULL;
    gboolean write_success = FALSE;
    int chmod_result = 0;

    g_assert_nonnull(
        fixture
    );

    g_assert_nonnull(
        fixture->tool_path
    );

    g_assert_nonnull(
        script_content
    );

    write_success = g_file_set_contents(
        fixture->tool_path,
        script_content,
        -1,
        &error
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        write_success
    );

    chmod_result = g_chmod(
        fixture->tool_path,
        S_IRUSR |
        S_IWUSR |
        S_IXUSR
    );

    g_assert_cmpint(
        chmod_result,
        ==,
        0
    );
}

/**
 * @brief Vérifie qu’un GBytes contient exactement le texte attendu.
 *
 * La fonction ne suppose pas que les données sont terminées par '\0'.
 *
 * @param bytes Données à vérifier.
 * @param expected_text Texte attendu.
 */
static void test_tool_process_assert_bytes_equal_text(
    GBytes *bytes,
    const char *expected_text
)
{
    gconstpointer bytes_data = NULL;
    gsize bytes_size = 0;
    gsize expected_size = 0;

    g_assert_nonnull(
        bytes
    );

    g_assert_nonnull(
        expected_text
    );

    bytes_data = g_bytes_get_data(
        bytes,
        &bytes_size
    );

    expected_size = strlen(
        expected_text
    );

    g_assert_cmpuint(
        bytes_size,
        ==,
        expected_size
    );

    g_assert_cmpmem(
        bytes_data,
        bytes_size,
        expected_text,
        expected_size
    );
}

/**
 * @brief Prépare le répertoire temporaire d’un test.
 *
 * @param fixture Fixture à initialiser.
 * @param user_data Données inutilisées.
 */
static void test_tool_process_fixture_setup(
    ToolProcessFixture *fixture,
    gconstpointer user_data
)
{
    GError *error = NULL;

    (void) user_data;

    fixture->temporary_directory =
        g_dir_make_tmp(
            "labfy-tool-process-XXXXXX",
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        fixture->temporary_directory
    );

    fixture->tool_path = g_build_filename(
        fixture->temporary_directory,
        "fake_tool",
        NULL
    );

    fixture->injection_path = g_build_filename(
        fixture->temporary_directory,
        "injection_created",
        NULL
    );
}

/**
 * @brief Supprime les fichiers et le dossier temporaire.
 *
 * @param fixture Fixture à nettoyer.
 * @param user_data Données inutilisées.
 */
static void test_tool_process_fixture_teardown(
    ToolProcessFixture *fixture,
    gconstpointer user_data
)
{
    (void) user_data;

    if (fixture->tool_path != NULL)
    {
        g_remove(
            fixture->tool_path
        );
    }

    if (fixture->injection_path != NULL)
    {
        g_remove(
            fixture->injection_path
        );
    }

    if (fixture->temporary_directory != NULL)
    {
        g_rmdir(
            fixture->temporary_directory
        );
    }

    g_clear_pointer(
        &fixture->tool_path,
        g_free
    );

    g_clear_pointer(
        &fixture->injection_path,
        g_free
    );

    g_clear_pointer(
        &fixture->temporary_directory,
        g_free
    );
}

/**
 * @brief Vérifie le refus des arguments invalides.
 */
static void test_tool_process_invalid_arguments(
    ToolProcessFixture *fixture,
    gconstpointer user_data
)
{
    ToolProcessResult *result = NULL;
    ToolProcessResult *occupied_result = NULL;

    GError *error = NULL;

    (void) user_data;

    g_assert_false(
        tool_process_run(
            NULL,
            NULL,
            NULL,
            NULL,
            &result,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_PROCESS_ERROR,
        TOOL_PROCESS_ERROR_INVALID_ARGUMENT
    );

    g_assert_null(
        result
    );

    g_clear_error(
        &error
    );

    g_assert_false(
        tool_process_run(
            "",
            NULL,
            NULL,
            NULL,
            &result,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_PROCESS_ERROR,
        TOOL_PROCESS_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    g_assert_false(
        tool_process_run(
            fixture->tool_path,
            NULL,
            NULL,
            NULL,
            NULL,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_PROCESS_ERROR,
        TOOL_PROCESS_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    /*
     * out_result doit pointer vers NULL avant l’appel.
     *
     * Ce pointeur sert uniquement de marqueur et n’est jamais déréférencé.
     */
    occupied_result =
        (ToolProcessResult *) fixture;

    g_assert_false(
        tool_process_run(
            fixture->tool_path,
            NULL,
            NULL,
            NULL,
            &occupied_result,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_PROCESS_ERROR,
        TOOL_PROCESS_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    g_assert_false(
        tool_process_run(
            fixture->tool_path,
            NULL,
            "",
            NULL,
            &result,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_PROCESS_ERROR,
        TOOL_PROCESS_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );
}

/**
 * @brief Vérifie l’exécution d’un outil sans argument.
 */
static void test_tool_process_no_arguments(
    ToolProcessFixture *fixture,
    gconstpointer user_data
)
{
    ToolProcessResult *result = NULL;

    GBytes *stdout_bytes = NULL;
    GBytes *stderr_bytes = NULL;

    GError *error = NULL;

    (void) user_data;

    test_tool_process_write_script(
        fixture,
        "#!/bin/sh\n"
        "printf 'outil-execute'\n"
    );

    g_assert_true(
        tool_process_run(
            fixture->tool_path,
            NULL,
            NULL,
            NULL,
            &result,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        result
    );

    g_assert_true(
        tool_process_result_exited_normally(
            result
        )
    );

    g_assert_false(
        tool_process_result_was_signaled(
            result
        )
    );

    g_assert_cmpint(
        tool_process_result_get_exit_status(
            result
        ),
        ==,
        0
    );

    g_assert_cmpint(
        tool_process_result_get_termination_signal(
            result
        ),
        ==,
        0
    );

    g_assert_true(
        tool_process_result_is_success(
            result
        )
    );

    stdout_bytes =
        tool_process_result_ref_stdout(
            result
        );

    stderr_bytes =
        tool_process_result_ref_stderr(
            result
        );

    test_tool_process_assert_bytes_equal_text(
        stdout_bytes,
        "outil-execute"
    );

    test_tool_process_assert_bytes_equal_text(
        stderr_bytes,
        ""
    );

    g_bytes_unref(
        stdout_bytes
    );

    g_bytes_unref(
        stderr_bytes
    );

    tool_process_result_free(
        result
    );
}

/**
 * @brief Vérifie que les arguments ne sont pas interprétés par un shell.
 */
static void test_tool_process_literal_arguments(
    ToolProcessFixture *fixture,
    gconstpointer user_data
)
{
    ToolProcessResult *result = NULL;
    GBytes *stdout_bytes = NULL;

    char *injection_argument = NULL;
    char *expected_output = NULL;

    const char *arguments[6];

    GError *error = NULL;

    (void) user_data;

    test_tool_process_write_script(
        fixture,
        "#!/bin/sh\n"
        "for argument in \"$@\"\n"
        "do\n"
        "    printf '%s\\n' \"$argument\"\n"
        "done\n"
    );

    injection_argument = g_strdup_printf(
        "$(touch %s)",
        fixture->injection_path
    );

    arguments[0] = "example.org";
    arguments[1] = "nom avec espaces";
    arguments[2] = injection_argument;
    arguments[3] = "; echo danger";
    arguments[4] = "*";
    arguments[5] = NULL;

    expected_output = g_strdup_printf(
        "example.org\n"
        "nom avec espaces\n"
        "%s\n"
        "; echo danger\n"
        "*\n",
        injection_argument
    );

    g_assert_false(
        g_file_test(
            fixture->injection_path,
            G_FILE_TEST_EXISTS
        )
    );

    g_assert_true(
        tool_process_run(
            fixture->tool_path,
            arguments,
            NULL,
            NULL,
            &result,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        tool_process_result_is_success(
            result
        )
    );

    stdout_bytes =
        tool_process_result_ref_stdout(
            result
        );

    test_tool_process_assert_bytes_equal_text(
        stdout_bytes,
        expected_output
    );

    /*
     * Si un shell avait interprété l’argument, ce fichier existerait.
     */
    g_assert_false(
        g_file_test(
            fixture->injection_path,
            G_FILE_TEST_EXISTS
        )
    );

    g_bytes_unref(
        stdout_bytes
    );

    tool_process_result_free(
        result
    );

    g_free(
        expected_output
    );

    g_free(
        injection_argument
    );
}

/**
 * @brief Vérifie la séparation de stdout et stderr.
 */
static void test_tool_process_stdout_stderr(
    ToolProcessFixture *fixture,
    gconstpointer user_data
)
{
    ToolProcessResult *result = NULL;

    GBytes *stdout_bytes = NULL;
    GBytes *stderr_bytes = NULL;

    GError *error = NULL;

    (void) user_data;

    test_tool_process_write_script(
        fixture,
        "#!/bin/sh\n"
        "printf 'message-out'\n"
        "printf 'message-err' >&2\n"
    );

    g_assert_true(
        tool_process_run(
            fixture->tool_path,
            NULL,
            NULL,
            NULL,
            &result,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    stdout_bytes =
        tool_process_result_ref_stdout(
            result
        );

    stderr_bytes =
        tool_process_result_ref_stderr(
            result
        );

    test_tool_process_assert_bytes_equal_text(
        stdout_bytes,
        "message-out"
    );

    test_tool_process_assert_bytes_equal_text(
        stderr_bytes,
        "message-err"
    );

    g_bytes_unref(
        stdout_bytes
    );

    g_bytes_unref(
        stderr_bytes
    );

    tool_process_result_free(
        result
    );
}

/**
 * @brief Vérifie qu’un code non nul produit tout de même un résultat.
 */
static void test_tool_process_nonzero_exit(
    ToolProcessFixture *fixture,
    gconstpointer user_data
)
{
    ToolProcessResult *result = NULL;
    GError *error = NULL;

    (void) user_data;

    test_tool_process_write_script(
        fixture,
        "#!/bin/sh\n"
        "printf 'echec-fonctionnel' >&2\n"
        "exit 7\n"
    );

    g_assert_true(
        tool_process_run(
            fixture->tool_path,
            NULL,
            NULL,
            NULL,
            &result,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        result
    );

    g_assert_true(
        tool_process_result_exited_normally(
            result
        )
    );

    g_assert_cmpint(
        tool_process_result_get_exit_status(
            result
        ),
        ==,
        7
    );

    g_assert_false(
        tool_process_result_is_success(
            result
        )
    );

    tool_process_result_free(
        result
    );
}

/**
 * @brief Vérifie l’erreur produite par un exécutable inexistant.
 */
static void test_tool_process_missing_executable(
    ToolProcessFixture *fixture,
    gconstpointer user_data
)
{
    ToolProcessResult *result = NULL;
    char *missing_path = NULL;
    GError *error = NULL;

    (void) user_data;

    missing_path = g_build_filename(
        fixture->temporary_directory,
        "tool_that_does_not_exist",
        NULL
    );

    g_assert_false(
        tool_process_run(
            missing_path,
            NULL,
            NULL,
            NULL,
            &result,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_PROCESS_ERROR,
        TOOL_PROCESS_ERROR_SPAWN
    );

    g_assert_null(
        result
    );

    g_clear_error(
        &error
    );

    g_free(
        missing_path
    );
}

/**
 * @brief Vérifie l'utilisation du dossier de travail demandé.
 */
static void test_tool_process_working_directory(
    ToolProcessFixture *fixture,
    gconstpointer user_data
)
{
    ToolProcessResult *result = NULL;
    GBytes *stdout_bytes = NULL;

    char *canonical_directory = NULL;

    GError *error = NULL;

    (void) user_data;

    test_tool_process_write_script(
        fixture,
        "#!/bin/sh\n"
        "printf '%s' \"$PWD\"\n"
    );

    g_assert_true(
        tool_process_run(
            fixture->tool_path,
            NULL,
            fixture->temporary_directory,
            NULL,
            &result,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        result
    );

    canonical_directory = g_canonicalize_filename(
        fixture->temporary_directory,
        NULL
    );

    stdout_bytes =
        tool_process_result_ref_stdout(
            result
        );

    test_tool_process_assert_bytes_equal_text(
        stdout_bytes,
        canonical_directory
    );

    g_bytes_unref(
        stdout_bytes
    );

    g_free(
        canonical_directory
    );

    tool_process_result_free(
        result
    );
}

/**
 * @brief Vérifie une annulation demandée avant le lancement.
 */
static void test_tool_process_cancelled_before_start(
    ToolProcessFixture *fixture,
    gconstpointer user_data
)
{
    ToolProcessResult *result = NULL;
    GCancellable *cancellable = NULL;
    GError *error = NULL;

    (void) user_data;

    test_tool_process_write_script(
        fixture,
        "#!/bin/sh\n"
        "exit 0\n"
    );

    cancellable = g_cancellable_new();

    g_cancellable_cancel(
        cancellable
    );

    g_assert_false(
        tool_process_run(
            fixture->tool_path,
            NULL,
            NULL,
            cancellable,
            &result,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_PROCESS_ERROR,
        TOOL_PROCESS_ERROR_CANCELLED
    );

    g_assert_null(
        result
    );

    g_clear_error(
        &error
    );

    g_object_unref(
        cancellable
    );
}

/**
 * @brief Vérifie l'annulation d'un processus en cours.
 */
static void test_tool_process_cancelled_during_run(
    ToolProcessFixture *fixture,
    gconstpointer user_data
)
{
    ToolProcessResult *result = NULL;

    GCancellable *cancellable = NULL;
    GThread *cancellation_thread = NULL;

    ToolProcessCancellationData cancellation_data;

    gint64 start_time = 0;
    gint64 end_time = 0;
    gint64 elapsed_microseconds = 0;

    GError *error = NULL;

    (void) user_data;

    test_tool_process_write_script(
        fixture,
        "#!/bin/sh\n"
        "while :\n"
        "do\n"
        "    :\n"
        "done\n"
    );

    cancellable = g_cancellable_new();

    cancellation_data.cancellable =
        cancellable;

    cancellation_data.delay_microseconds =
        200000;

    cancellation_thread = g_thread_new(
        "tool-process-canceller",
        test_tool_process_cancel_after_delay,
        &cancellation_data
    );

    g_assert_nonnull(
        cancellation_thread
    );

    start_time = g_get_monotonic_time();

    g_assert_false(
        tool_process_run(
            fixture->tool_path,
            NULL,
            NULL,
            cancellable,
            &result,
            &error
        )
    );

    end_time = g_get_monotonic_time();

    g_thread_join(
        cancellation_thread
    );

    elapsed_microseconds =
        end_time - start_time;

    g_assert_error(
        error,
        TOOL_PROCESS_ERROR,
        TOOL_PROCESS_ERROR_CANCELLED
    );

    g_assert_null(
        result
    );

    /*
     * L'exécution doit être interrompue bien avant plusieurs secondes.
     */
    g_assert_cmpint(
        elapsed_microseconds,
        <,
        3000000
    );

    g_clear_error(
        &error
    );

    g_object_unref(
        cancellable
    );
}

/**
 * @brief Vérifie qu'une exécution sans sortie produit des GBytes vides.
 */
static void test_tool_process_empty_output(
    ToolProcessFixture *fixture,
    gconstpointer user_data
)
{
    ToolProcessResult *result = NULL;

    GBytes *stdout_bytes = NULL;
    GBytes *stderr_bytes = NULL;

    GError *error = NULL;

    (void) user_data;

    test_tool_process_write_script(
        fixture,
        "#!/bin/sh\n"
        "exit 0\n"
    );

    g_assert_true(
        tool_process_run(
            fixture->tool_path,
            NULL,
            NULL,
            NULL,
            &result,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    stdout_bytes =
        tool_process_result_ref_stdout(
            result
        );

    stderr_bytes =
        tool_process_result_ref_stderr(
            result
        );

    g_assert_nonnull(
        stdout_bytes
    );

    g_assert_nonnull(
        stderr_bytes
    );

    g_assert_cmpuint(
        g_bytes_get_size(
            stdout_bytes
        ),
        ==,
        0
    );

    g_assert_cmpuint(
        g_bytes_get_size(
            stderr_bytes
        ),
        ==,
        0
    );

    g_bytes_unref(
        stdout_bytes
    );

    g_bytes_unref(
        stderr_bytes
    );

    tool_process_result_free(
        result
    );
}

/**
 * @brief Vérifie la conservation d'une sortie non UTF-8.
 */
static void test_tool_process_non_utf8_output(
    ToolProcessFixture *fixture,
    gconstpointer user_data
)
{
    ToolProcessResult *result = NULL;
    GBytes *stdout_bytes = NULL;

    gconstpointer stdout_data = NULL;
    const guint8 *stdout_octets = NULL;

    gsize stdout_size = 0;

    GError *error = NULL;

    (void) user_data;

    test_tool_process_write_script(
        fixture,
        "#!/bin/sh\n"
        "printf '\\377'\n"
    );

    g_assert_true(
        tool_process_run(
            fixture->tool_path,
            NULL,
            NULL,
            NULL,
            &result,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    stdout_bytes =
        tool_process_result_ref_stdout(
            result
        );

    g_assert_nonnull(
        stdout_bytes
    );

    stdout_data = g_bytes_get_data(
        stdout_bytes,
        &stdout_size
    );

    stdout_octets = stdout_data;

    g_assert_cmpuint(
        stdout_size,
        ==,
        1
    );

    g_assert_nonnull(
        stdout_octets
    );

    g_assert_cmpuint(
        stdout_octets[0],
        ==,
        0xFF
    );

    g_bytes_unref(
        stdout_bytes
    );

    tool_process_result_free(
        result
    );
}

/**
 * @brief Vérifie la représentation d'une terminaison par signal.
 */
static void test_tool_process_signaled(
    ToolProcessFixture *fixture,
    gconstpointer user_data
)
{
    ToolProcessResult *result = NULL;
    GError *error = NULL;

    (void) user_data;

    test_tool_process_write_script(
        fixture,
        "#!/bin/sh\n"
        "kill -TERM $$\n"
    );

    g_assert_true(
        tool_process_run(
            fixture->tool_path,
            NULL,
            NULL,
            NULL,
            &result,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        result
    );

    g_assert_false(
        tool_process_result_exited_normally(
            result
        )
    );

    g_assert_true(
        tool_process_result_was_signaled(
            result
        )
    );

    g_assert_cmpint(
        tool_process_result_get_exit_status(
            result
        ),
        ==,
        -1
    );

    g_assert_cmpint(
        tool_process_result_get_termination_signal(
            result
        ),
        ==,
        SIGTERM
    );

    g_assert_false(
        tool_process_result_is_success(
            result
        )
    );

    tool_process_result_free(
        result
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

    g_test_add(
        "/tool_process/invalid_arguments",
        ToolProcessFixture,
        NULL,
        test_tool_process_fixture_setup,
        test_tool_process_invalid_arguments,
        test_tool_process_fixture_teardown
    );

    g_test_add(
        "/tool_process/no_arguments",
        ToolProcessFixture,
        NULL,
        test_tool_process_fixture_setup,
        test_tool_process_no_arguments,
        test_tool_process_fixture_teardown
    );

    g_test_add(
        "/tool_process/literal_arguments",
        ToolProcessFixture,
        NULL,
        test_tool_process_fixture_setup,
        test_tool_process_literal_arguments,
        test_tool_process_fixture_teardown
    );

    g_test_add(
        "/tool_process/stdout_stderr",
        ToolProcessFixture,
        NULL,
        test_tool_process_fixture_setup,
        test_tool_process_stdout_stderr,
        test_tool_process_fixture_teardown
    );

    g_test_add(
        "/tool_process/nonzero_exit",
        ToolProcessFixture,
        NULL,
        test_tool_process_fixture_setup,
        test_tool_process_nonzero_exit,
        test_tool_process_fixture_teardown
    );

    g_test_add(
        "/tool_process/missing_executable",
        ToolProcessFixture,
        NULL,
        test_tool_process_fixture_setup,
        test_tool_process_missing_executable,
        test_tool_process_fixture_teardown
    );

    g_test_add(
        "/tool_process/working_directory",
        ToolProcessFixture,
        NULL,
        test_tool_process_fixture_setup,
        test_tool_process_working_directory,
        test_tool_process_fixture_teardown
    );

    g_test_add(
        "/tool_process/cancelled_before_start",
        ToolProcessFixture,
        NULL,
        test_tool_process_fixture_setup,
        test_tool_process_cancelled_before_start,
        test_tool_process_fixture_teardown
    );

    g_test_add(
        "/tool_process/cancelled_during_run",
        ToolProcessFixture,
        NULL,
        test_tool_process_fixture_setup,
        test_tool_process_cancelled_during_run,
        test_tool_process_fixture_teardown
    );

    g_test_add(
        "/tool_process/empty_output",
        ToolProcessFixture,
        NULL,
        test_tool_process_fixture_setup,
        test_tool_process_empty_output,
        test_tool_process_fixture_teardown
    );

    g_test_add(
        "/tool_process/non_utf8_output",
        ToolProcessFixture,
        NULL,
        test_tool_process_fixture_setup,
        test_tool_process_non_utf8_output,
        test_tool_process_fixture_teardown
    );

    g_test_add(
        "/tool_process/signaled",
        ToolProcessFixture,
        NULL,
        test_tool_process_fixture_setup,
        test_tool_process_signaled,
        test_tool_process_fixture_teardown
    );

    return g_test_run();
}
