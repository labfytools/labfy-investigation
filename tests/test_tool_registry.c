/******************************************************************************
 * @file test_tool_registry.c
 * @brief Tests unitaires du registre des outils externes.
 ******************************************************************************/

#include "core/tool_registry.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <sys/stat.h>

/**
 * @struct ToolRegistryPathFixture
 * @brief Environnement temporaire contenant un faux exécutable.
 */
typedef struct
{
    char *temporary_directory;
    char *fake_executable_path;
    char *original_path;
} ToolRegistryPathFixture;

/**
 * @brief Crée un faux exécutable.
 *
 * @param executable_path Chemin du fichier à créer.
 */
static void test_tool_registry_create_fake_executable(
    const char *executable_path
)
{
    GError *error = NULL;
    gboolean write_success = FALSE;
    int chmod_result = 0;

    g_assert_nonnull(
        executable_path
    );

    write_success = g_file_set_contents(
        executable_path,
        "#!/bin/sh\n"
        "exit 0\n",
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
        executable_path,
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
 * @brief Prépare un PATH contenant uniquement un faux outil.
 *
 * @param fixture Fixture à initialiser.
 * @param user_data Données inutilisées.
 */
static void test_tool_registry_path_fixture_setup(
    ToolRegistryPathFixture *fixture,
    gconstpointer user_data
)
{
    GError *error = NULL;

    (void) user_data;

    fixture->original_path = g_strdup(
        g_getenv("PATH")
    );

    fixture->temporary_directory =
        g_dir_make_tmp(
            "labfy-tool-registry-XXXXXX",
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        fixture->temporary_directory
    );

    fixture->fake_executable_path =
        g_build_filename(
            fixture->temporary_directory,
            "fake_osint_tool",
            NULL
        );

    test_tool_registry_create_fake_executable(
        fixture->fake_executable_path
    );

    g_assert_true(
        g_setenv(
            "PATH",
            fixture->temporary_directory,
            TRUE
        )
    );
}

/**
 * @brief Restaure le PATH et supprime les fichiers temporaires.
 *
 * @param fixture Fixture à nettoyer.
 * @param user_data Données inutilisées.
 */
static void test_tool_registry_path_fixture_teardown(
    ToolRegistryPathFixture *fixture,
    gconstpointer user_data
)
{
    (void) user_data;

    if (fixture->original_path != NULL)
    {
        g_setenv(
            "PATH",
            fixture->original_path,
            TRUE
        );
    }
    else
    {
        g_unsetenv(
            "PATH"
        );
    }

    if (fixture->fake_executable_path != NULL)
    {
        g_remove(
            fixture->fake_executable_path
        );
    }

    if (fixture->temporary_directory != NULL)
    {
        g_rmdir(
            fixture->temporary_directory
        );
    }

    g_clear_pointer(
        &fixture->original_path,
        g_free
    );

    g_clear_pointer(
        &fixture->fake_executable_path,
        g_free
    );

    g_clear_pointer(
        &fixture->temporary_directory,
        g_free
    );
}

/**
 * @brief Vérifie la création d’un registre vide.
 */
static void test_tool_registry_construction(void)
{
    ToolRegistry *tool_registry = NULL;

    tool_registry = tool_registry_new();

    g_assert_nonnull(
        tool_registry
    );

    g_assert_cmpuint(
        tool_registry_get_count(
            tool_registry
        ),
        ==,
        0
    );

    g_assert_null(
        tool_registry_get_tool(
            tool_registry,
            0
        )
    );

    g_assert_null(
        tool_registry_find(
            tool_registry,
            "unknown.tool"
        )
    );

    tool_registry_free(
        tool_registry
    );

    tool_registry_free(
        NULL
    );
}

/**
 * @brief Vérifie l’enregistrement et les accesseurs.
 */
static void test_tool_registry_register(void)
{
    ToolRegistry *tool_registry = NULL;
    const ToolInfo *tool_info = NULL;
    GError *error = NULL;

    tool_registry = tool_registry_new();

    g_assert_true(
        tool_registry_register(
            tool_registry,
            "dns.dig",
            "dig",
            "dig",
            TOOL_REQUIREMENT_OPTIONAL,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_cmpuint(
        tool_registry_get_count(
            tool_registry
        ),
        ==,
        1
    );

    tool_info = tool_registry_get_tool(
        tool_registry,
        0
    );

    g_assert_nonnull(
        tool_info
    );

    g_assert_cmpstr(
        tool_info_get_identifier(
            tool_info
        ),
        ==,
        "dns.dig"
    );

    g_assert_cmpstr(
        tool_info_get_display_name(
            tool_info
        ),
        ==,
        "dig"
    );

    g_assert_cmpstr(
        tool_info_get_executable_name(
            tool_info
        ),
        ==,
        "dig"
    );

    g_assert_cmpint(
        tool_info_get_requirement(
            tool_info
        ),
        ==,
        TOOL_REQUIREMENT_OPTIONAL
    );

    g_assert_cmpint(
        tool_info_get_availability(
            tool_info
        ),
        ==,
        TOOL_AVAILABILITY_UNKNOWN
    );

    g_assert_null(
        tool_info_get_resolved_path(
            tool_info
        )
    );

    g_assert_null(
        tool_info_get_detected_version(
            tool_info
        )
    );

    tool_registry_free(
        tool_registry
    );
}

/**
 * @brief Vérifie que le registre duplique les chaînes reçues.
 */
static void test_tool_registry_string_ownership(void)
{
    ToolRegistry *tool_registry = NULL;
    const ToolInfo *tool_info = NULL;

    char *identifier = NULL;
    char *display_name = NULL;
    char *executable_name = NULL;

    GError *error = NULL;

    identifier = g_strdup(
        "network.fake"
    );

    display_name = g_strdup(
        "Fake Tool"
    );

    executable_name = g_strdup(
        "fake_tool"
    );

    tool_registry = tool_registry_new();

    g_assert_true(
        tool_registry_register(
            tool_registry,
            identifier,
            display_name,
            executable_name,
            TOOL_REQUIREMENT_REQUIRED,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_free(
        identifier
    );

    g_free(
        display_name
    );

    g_free(
        executable_name
    );

    tool_info = tool_registry_find(
        tool_registry,
        "network.fake"
    );

    g_assert_nonnull(
        tool_info
    );

    g_assert_cmpstr(
        tool_info_get_identifier(
            tool_info
        ),
        ==,
        "network.fake"
    );

    g_assert_cmpstr(
        tool_info_get_display_name(
            tool_info
        ),
        ==,
        "Fake Tool"
    );

    g_assert_cmpstr(
        tool_info_get_executable_name(
            tool_info
        ),
        ==,
        "fake_tool"
    );

    tool_registry_free(
        tool_registry
    );
}

/**
 * @brief Vérifie le refus des identifiants en double.
 */
static void test_tool_registry_duplicate_identifier(void)
{
    ToolRegistry *tool_registry = NULL;
    GError *error = NULL;

    tool_registry = tool_registry_new();

    g_assert_true(
        tool_registry_register(
            tool_registry,
            "dns.dig",
            "dig",
            "dig",
            TOOL_REQUIREMENT_OPTIONAL,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_false(
        tool_registry_register(
            tool_registry,
            "dns.dig",
            "Autre outil",
            "other_tool",
            TOOL_REQUIREMENT_REQUIRED,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_REGISTRY_ERROR,
        TOOL_REGISTRY_ERROR_DUPLICATE_IDENTIFIER
    );

    g_clear_error(
        &error
    );

    g_assert_cmpuint(
        tool_registry_get_count(
            tool_registry
        ),
        ==,
        1
    );

    tool_registry_free(
        tool_registry
    );
}

/**
 * @brief Vérifie les arguments invalides.
 */
static void test_tool_registry_invalid_arguments(void)
{
    ToolRegistry *tool_registry = NULL;
    GError *error = NULL;

    tool_registry = tool_registry_new();

    g_assert_false(
        tool_registry_register(
            NULL,
            "dns.dig",
            "dig",
            "dig",
            TOOL_REQUIREMENT_OPTIONAL,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_REGISTRY_ERROR,
        TOOL_REGISTRY_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    g_assert_false(
        tool_registry_register(
            tool_registry,
            "",
            "dig",
            "dig",
            TOOL_REQUIREMENT_OPTIONAL,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_REGISTRY_ERROR,
        TOOL_REGISTRY_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    g_assert_false(
        tool_registry_register(
            tool_registry,
            "dns.dig",
            "",
            "dig",
            TOOL_REQUIREMENT_OPTIONAL,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_REGISTRY_ERROR,
        TOOL_REGISTRY_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    g_assert_false(
        tool_registry_register(
            tool_registry,
            "dns.dig",
            "dig",
            "",
            TOOL_REQUIREMENT_OPTIONAL,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_REGISTRY_ERROR,
        TOOL_REGISTRY_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    g_assert_false(
        tool_registry_register(
            tool_registry,
            "dns.dig",
            "dig",
            "dig",
            (ToolRequirement) 999,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_REGISTRY_ERROR,
        TOOL_REGISTRY_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    g_assert_false(
        tool_registry_refresh(
            NULL,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_REGISTRY_ERROR,
        TOOL_REGISTRY_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    tool_registry_free(
        tool_registry
    );
}

/**
 * @brief Vérifie la recherche par identifiant.
 */
static void test_tool_registry_find_tool(void)
{
    ToolRegistry *tool_registry = NULL;
    const ToolInfo *tool_info = NULL;
    GError *error = NULL;

    tool_registry = tool_registry_new();

    g_assert_true(
        tool_registry_register(
            tool_registry,
            "tls.openssl",
            "OpenSSL",
            "openssl",
            TOOL_REQUIREMENT_REQUIRED,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    tool_info = tool_registry_find(
        tool_registry,
        "tls.openssl"
    );

    g_assert_nonnull(
        tool_info
    );

    g_assert_null(
        tool_registry_find(
            tool_registry,
            "unknown.tool"
        )
    );

    g_assert_null(
        tool_registry_find(
            tool_registry,
            NULL
        )
    );

    g_assert_null(
        tool_registry_find(
            tool_registry,
            ""
        )
    );

    tool_registry_free(
        tool_registry
    );
}

/**
 * @brief Vérifie la détection d’un faux exécutable.
 */
static void test_tool_registry_available_tool(
    ToolRegistryPathFixture *fixture,
    gconstpointer user_data
)
{
    ToolRegistry *tool_registry = NULL;
    const ToolInfo *tool_info = NULL;
    GError *error = NULL;

    (void) user_data;

    tool_registry = tool_registry_new();

    g_assert_true(
        tool_registry_register(
            tool_registry,
            "test.fake",
            "Fake OSINT Tool",
            "fake_osint_tool",
            TOOL_REQUIREMENT_REQUIRED,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        tool_registry_refresh(
            tool_registry,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    tool_info = tool_registry_find(
        tool_registry,
        "test.fake"
    );

    g_assert_nonnull(
        tool_info
    );

    g_assert_cmpint(
        tool_info_get_availability(
            tool_info
        ),
        ==,
        TOOL_AVAILABILITY_AVAILABLE
    );

    g_assert_nonnull(
        tool_info_get_resolved_path(
            tool_info
        )
    );

    g_assert_cmpstr(
        tool_info_get_resolved_path(
            tool_info
        ),
        ==,
        fixture->fake_executable_path
    );

    tool_registry_free(
        tool_registry
    );
}

/**
 * @brief Vérifie la détection d’un outil absent.
 */
static void test_tool_registry_missing_tool(void)
{
    ToolRegistry *tool_registry = NULL;
    const ToolInfo *tool_info = NULL;
    GError *error = NULL;

    tool_registry = tool_registry_new();

    g_assert_true(
        tool_registry_register(
            tool_registry,
            "test.missing",
            "Missing Tool",
            "labfy_tool_that_must_not_exist_036",
            TOOL_REQUIREMENT_OPTIONAL,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        tool_registry_refresh(
            tool_registry,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    tool_info = tool_registry_find(
        tool_registry,
        "test.missing"
    );

    g_assert_nonnull(
        tool_info
    );

    g_assert_cmpint(
        tool_info_get_availability(
            tool_info
        ),
        ==,
        TOOL_AVAILABILITY_MISSING
    );

    g_assert_null(
        tool_info_get_resolved_path(
            tool_info
        )
    );

    tool_registry_free(
        tool_registry
    );
}

/**
 * @brief Vérifie les transitions de disponibilité.
 */
static void test_tool_registry_successive_refresh(
    ToolRegistryPathFixture *fixture,
    gconstpointer user_data
)
{
    ToolRegistry *tool_registry = NULL;
    const ToolInfo *tool_info = NULL;
    GError *error = NULL;

    (void) user_data;

    tool_registry = tool_registry_new();

    g_assert_true(
        tool_registry_register(
            tool_registry,
            "test.fake",
            "Fake OSINT Tool",
            "fake_osint_tool",
            TOOL_REQUIREMENT_REQUIRED,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    /*
     * Première détection : le fichier existe.
     */
    g_assert_true(
        tool_registry_refresh(
            tool_registry,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    tool_info = tool_registry_find(
        tool_registry,
        "test.fake"
    );

    g_assert_cmpint(
        tool_info_get_availability(
            tool_info
        ),
        ==,
        TOOL_AVAILABILITY_AVAILABLE
    );

    /*
     * Deuxième détection : le fichier a été supprimé.
     */
    g_assert_cmpint(
        g_remove(
            fixture->fake_executable_path
        ),
        ==,
        0
    );

    g_assert_true(
        tool_registry_refresh(
            tool_registry,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_cmpint(
        tool_info_get_availability(
            tool_info
        ),
        ==,
        TOOL_AVAILABILITY_MISSING
    );

    g_assert_null(
        tool_info_get_resolved_path(
            tool_info
        )
    );

    /*
     * Troisième détection : le fichier est recréé.
     */
    test_tool_registry_create_fake_executable(
        fixture->fake_executable_path
    );

    g_assert_true(
        tool_registry_refresh(
            tool_registry,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_cmpint(
        tool_info_get_availability(
            tool_info
        ),
        ==,
        TOOL_AVAILABILITY_AVAILABLE
    );

    g_assert_nonnull(
        tool_info_get_resolved_path(
            tool_info
        )
    );

    tool_registry_free(
        tool_registry
    );
}

/**
 * @brief Vérifie le stockage et l’effacement d’une version.
 */
static void test_tool_registry_version(void)
{
    ToolRegistry *tool_registry = NULL;
    const ToolInfo *tool_info = NULL;
    GError *error = NULL;

    tool_registry = tool_registry_new();

    g_assert_true(
        tool_registry_register(
            tool_registry,
            "tls.openssl",
            "OpenSSL",
            "openssl",
            TOOL_REQUIREMENT_OPTIONAL,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        tool_registry_set_version(
            tool_registry,
            "tls.openssl",
            "3.5.1",
            &error
        )
    );

    g_assert_no_error(
        error
    );

    tool_info = tool_registry_find(
        tool_registry,
        "tls.openssl"
    );

    g_assert_cmpstr(
        tool_info_get_detected_version(
            tool_info
        ),
        ==,
        "3.5.1"
    );

    g_assert_true(
        tool_registry_set_version(
            tool_registry,
            "tls.openssl",
            "3.6.0",
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_cmpstr(
        tool_info_get_detected_version(
            tool_info
        ),
        ==,
        "3.6.0"
    );

    g_assert_true(
        tool_registry_set_version(
            tool_registry,
            "tls.openssl",
            NULL,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_null(
        tool_info_get_detected_version(
            tool_info
        )
    );

    g_assert_false(
        tool_registry_set_version(
            tool_registry,
            "unknown.tool",
            "1.0",
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_REGISTRY_ERROR,
        TOOL_REGISTRY_ERROR_NOT_FOUND
    );

    g_clear_error(
        &error
    );

    tool_registry_free(
        tool_registry
    );
}

/**
 * @brief Vérifie l’état global des dépendances obligatoires.
 */
static void test_tool_registry_required_tools(
    ToolRegistryPathFixture *fixture,
    gconstpointer user_data
)
{
    ToolRegistry *tool_registry = NULL;
    GError *error = NULL;

    (void) fixture;
    (void) user_data;

    tool_registry = tool_registry_new();

    g_assert_true(
        tool_registry_all_required_tools_available(
            tool_registry
        )
    );

    g_assert_false(
        tool_registry_has_missing_required_tools(
            tool_registry
        )
    );

    g_assert_true(
        tool_registry_register(
            tool_registry,
            "test.required",
            "Required Fake Tool",
            "fake_osint_tool",
            TOOL_REQUIREMENT_REQUIRED,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        tool_registry_register(
            tool_registry,
            "test.optional",
            "Optional Missing Tool",
            "labfy_optional_tool_that_must_not_exist_036",
            TOOL_REQUIREMENT_OPTIONAL,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    /*
     * L’outil obligatoire est encore UNKNOWN.
     */
    g_assert_false(
        tool_registry_all_required_tools_available(
            tool_registry
        )
    );

    g_assert_false(
        tool_registry_has_missing_required_tools(
            tool_registry
        )
    );

    g_assert_true(
        tool_registry_refresh(
            tool_registry,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    /*
     * L’outil obligatoire existe.
     * L’outil optionnel absent ne bloque pas l’état global.
     */
    g_assert_true(
        tool_registry_all_required_tools_available(
            tool_registry
        )
    );

    g_assert_false(
        tool_registry_has_missing_required_tools(
            tool_registry
        )
    );

    tool_registry_free(
        tool_registry
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
        "/tool_registry/construction",
        test_tool_registry_construction
    );

    g_test_add_func(
        "/tool_registry/register",
        test_tool_registry_register
    );

    g_test_add_func(
        "/tool_registry/string_ownership",
        test_tool_registry_string_ownership
    );

    g_test_add_func(
        "/tool_registry/duplicate_identifier",
        test_tool_registry_duplicate_identifier
    );

    g_test_add_func(
        "/tool_registry/invalid_arguments",
        test_tool_registry_invalid_arguments
    );

    g_test_add_func(
        "/tool_registry/find",
        test_tool_registry_find_tool
    );

    g_test_add(
        "/tool_registry/available_tool",
        ToolRegistryPathFixture,
        NULL,
        test_tool_registry_path_fixture_setup,
        test_tool_registry_available_tool,
        test_tool_registry_path_fixture_teardown
    );

    g_test_add_func(
        "/tool_registry/missing_tool",
        test_tool_registry_missing_tool
    );

    g_test_add(
        "/tool_registry/successive_refresh",
        ToolRegistryPathFixture,
        NULL,
        test_tool_registry_path_fixture_setup,
        test_tool_registry_successive_refresh,
        test_tool_registry_path_fixture_teardown
    );

    g_test_add_func(
        "/tool_registry/version",
        test_tool_registry_version
    );

    g_test_add(
        "/tool_registry/required_tools",
        ToolRegistryPathFixture,
        NULL,
        test_tool_registry_path_fixture_setup,
        test_tool_registry_required_tools,
        test_tool_registry_path_fixture_teardown
    );

    return g_test_run();
}
