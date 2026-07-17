/******************************************************************************
 * @file test_tool_catalog.c
 * @brief Tests unitaires du catalogue des outils externes.
 ******************************************************************************/

#include "core/tool_catalog.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <string.h>
#include <sys/stat.h>

/**
 * @struct ExpectedToolCatalogEntry
 * @brief Valeurs attendues pour une entrée du catalogue.
 */
typedef struct
{
    const char *identifier;
    const char *display_name;
    const char *executable_name;
    const char *version_argument;
} ExpectedToolCatalogEntry;

/**
 * @brief Catalogue attendu par les tests.
 */
static const ExpectedToolCatalogEntry expected_catalog_entries[] =
{
    {
        .identifier = "dns.dig",
        .display_name = "dig",
        .executable_name = "dig",
        .version_argument = "-v"
    },
    {
        .identifier = "dns.host",
        .display_name = "host",
        .executable_name = "host",
        .version_argument = "-V"
    },
    {
        .identifier = "network.whois",
        .display_name = "whois",
        .executable_name = "whois",
        .version_argument = "--version"
    },
    {
        .identifier = "http.curl",
        .display_name = "curl",
        .executable_name = "curl",
        .version_argument = "--version"
    },
    {
        .identifier = "tls.openssl",
        .display_name = "OpenSSL",
        .executable_name = "openssl",
        .version_argument = "version"
    }
};

static char *test_tool_catalog_directory = NULL;

typedef struct
{
    GCancellable *cancellable;
    guint64 delay_microseconds;
} ToolCatalogCancellationData;

static gpointer test_tool_catalog_cancel_after_delay(
    gpointer user_data
)
{
    ToolCatalogCancellationData *cancellation_data =
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

static ToolRegistry *test_tool_catalog_create_registry(
    gboolean refresh_registry
);

/**
 * @brief Crée le faux exécutable curl utilisé par les tests.
 *
 * @param script_content Contenu complet du script.
 */
static void test_tool_catalog_write_curl_script(
    const char *script_content
)
{
    char *curl_path = NULL;

    GError *error = NULL;

    gboolean write_success = FALSE;
    int chmod_result = 0;

    g_assert_nonnull(
        test_tool_catalog_directory
    );

    g_assert_nonnull(
        script_content
    );

    curl_path =
        g_build_filename(
            test_tool_catalog_directory,
            "curl",
            NULL
        );

    write_success =
        g_file_set_contents(
            curl_path,
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

    chmod_result =
        g_chmod(
            curl_path,
            S_IRUSR |
            S_IWUSR |
            S_IXUSR
        );

    g_assert_cmpint(
        chmod_result,
        ==,
        0
    );

    g_free(
        curl_path
    );
}

static void test_tool_catalog_version_stdout(void)
{
    ToolRegistry *tool_registry = NULL;

    char *detected_version = NULL;

    GError *error = NULL;

    test_tool_catalog_write_curl_script(
        "#!/bin/sh\n"
        "printf 'Fake curl 1.2.3\\n'\n"
        "exit 0\n"
    );

    tool_registry =
        test_tool_catalog_create_registry(
            TRUE
        );

    g_assert_true(
        tool_catalog_detect_version(
            tool_registry,
            "http.curl",
            NULL,
            &detected_version,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_cmpstr(
        detected_version,
        ==,
        "Fake curl 1.2.3"
    );

    g_free(
        detected_version
    );

    tool_registry_free(
        tool_registry
    );
}

static void test_tool_catalog_version_stderr(void)
{
    ToolRegistry *tool_registry = NULL;

    char *detected_version = NULL;

    GError *error = NULL;

    test_tool_catalog_write_curl_script(
        "#!/bin/sh\n"
        "printf 'Fake curl 2.3.4\\n' >&2\n"
        "exit 0\n"
    );

    tool_registry =
        test_tool_catalog_create_registry(
            TRUE
        );

    g_assert_true(
        tool_catalog_detect_version(
            tool_registry,
            "http.curl",
            NULL,
            &detected_version,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_cmpstr(
        detected_version,
        ==,
        "Fake curl 2.3.4"
    );

    g_free(
        detected_version
    );

    tool_registry_free(
        tool_registry
    );
}

static void test_tool_catalog_version_first_nonempty_line(void)
{
    ToolRegistry *tool_registry = NULL;

    char *detected_version = NULL;

    GError *error = NULL;

    test_tool_catalog_write_curl_script(
        "#!/bin/sh\n"
        "printf '\\n\\nFake curl 3.4.5\\nautre ligne\\n'\n"
        "exit 0\n"
    );

    tool_registry =
        test_tool_catalog_create_registry(
            TRUE
        );

    g_assert_true(
        tool_catalog_detect_version(
            tool_registry,
            "http.curl",
            NULL,
            &detected_version,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_cmpstr(
        detected_version,
        ==,
        "Fake curl 3.4.5"
    );

    g_free(
        detected_version
    );

    tool_registry_free(
        tool_registry
    );
}

static void test_tool_catalog_version_normalization(void)
{
    ToolRegistry *tool_registry = NULL;

    char *detected_version = NULL;

    GError *error = NULL;

    test_tool_catalog_write_curl_script(
        "#!/bin/sh\n"
        "printf '\\t   Fake curl 4.5.6   \\t\\r\\n'\n"
        "exit 0\n"
    );

    tool_registry =
        test_tool_catalog_create_registry(
            TRUE
        );

    g_assert_true(
        tool_catalog_detect_version(
            tool_registry,
            "http.curl",
            NULL,
            &detected_version,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_cmpstr(
        detected_version,
        ==,
        "Fake curl 4.5.6"
    );

    g_free(
        detected_version
    );

    tool_registry_free(
        tool_registry
    );
}

static void test_tool_catalog_version_non_utf8(void)
{
    ToolRegistry *tool_registry = NULL;

    char *detected_version = NULL;

    GError *error = NULL;

    test_tool_catalog_write_curl_script(
        "#!/bin/sh\n"
        "printf 'Fake curl \\377 5.6.7\\n'\n"
        "exit 0\n"
    );

    tool_registry =
        test_tool_catalog_create_registry(
            TRUE
        );

    g_assert_true(
        tool_catalog_detect_version(
            tool_registry,
            "http.curl",
            NULL,
            &detected_version,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        detected_version
    );

    g_assert_true(
        g_utf8_validate(
            detected_version,
            -1,
            NULL
        )
    );

    g_assert_true(
        g_str_has_prefix(
            detected_version,
            "Fake curl "
        )
    );

    g_free(
        detected_version
    );

    tool_registry_free(
        tool_registry
    );
}

static void test_tool_catalog_version_empty(void)
{
    ToolRegistry *tool_registry = NULL;

    char *detected_version = NULL;

    GError *error = NULL;

    test_tool_catalog_write_curl_script(
        "#!/bin/sh\n"
        "exit 0\n"
    );

    tool_registry =
        test_tool_catalog_create_registry(
            TRUE
        );

    g_assert_false(
        tool_catalog_detect_version(
            tool_registry,
            "http.curl",
            NULL,
            &detected_version,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_CATALOG_ERROR,
        TOOL_CATALOG_ERROR_VERSION_OUTPUT
    );

    g_assert_null(
        detected_version
    );

    g_clear_error(
        &error
    );

    tool_registry_free(
        tool_registry
    );
}

/**
 * @brief Crée un registre contenant le catalogue par défaut.
 *
 * @param refresh_registry TRUE pour rechercher les exécutables.
 *
 * @return Nouveau registre.
 */
static ToolRegistry *test_tool_catalog_create_registry(
    gboolean refresh_registry
)
{
    ToolRegistry *tool_registry = NULL;
    GError *error = NULL;

    tool_registry = tool_registry_new();

    g_assert_nonnull(
        tool_registry
    );

    g_assert_true(
        tool_catalog_register_defaults(
            tool_registry,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    if (refresh_registry)
    {
        g_assert_true(
            tool_registry_refresh(
                tool_registry,
                &error
            )
        );

        g_assert_no_error(
            error
        );
    }

    return tool_registry;
}

/**
 * @brief Vérifie le contenu exact du catalogue statique.
 */
static void test_tool_catalog_entries(void)
{
    const ToolCatalogEntry *catalog_entry = NULL;
    const ExpectedToolCatalogEntry *expected_entry = NULL;

    gsize entry_index = 0;

    g_assert_cmpuint(
        tool_catalog_get_count(),
        ==,
        G_N_ELEMENTS(expected_catalog_entries)
    );

    for (entry_index = 0;
         entry_index < G_N_ELEMENTS(expected_catalog_entries);
         entry_index++)
    {
        catalog_entry = tool_catalog_get_entry(
            entry_index
        );

        expected_entry =
            &expected_catalog_entries[entry_index];

        g_assert_nonnull(
            catalog_entry
        );

        g_assert_cmpstr(
            tool_catalog_entry_get_identifier(
                catalog_entry
            ),
            ==,
            expected_entry->identifier
        );

        g_assert_cmpstr(
            tool_catalog_entry_get_display_name(
                catalog_entry
            ),
            ==,
            expected_entry->display_name
        );

        g_assert_cmpstr(
            tool_catalog_entry_get_executable_name(
                catalog_entry
            ),
            ==,
            expected_entry->executable_name
        );

        g_assert_cmpint(
            tool_catalog_entry_get_requirement(
                catalog_entry
            ),
            ==,
            TOOL_REQUIREMENT_OPTIONAL
        );

        g_assert_cmpuint(
            tool_catalog_entry_get_version_argument_count(
                catalog_entry
            ),
            ==,
            1
        );

        g_assert_cmpstr(
            tool_catalog_entry_get_version_argument(
                catalog_entry,
                0
            ),
            ==,
            expected_entry->version_argument
        );

        g_assert_null(
            tool_catalog_entry_get_version_argument(
                catalog_entry,
                1
            )
        );
    }
}

/**
 * @brief Vérifie les index et accesseurs invalides.
 */
static void test_tool_catalog_invalid_index(void)
{
    g_assert_null(
        tool_catalog_get_entry(
            tool_catalog_get_count()
        )
    );

    g_assert_null(
        tool_catalog_get_entry(
            G_MAXSIZE
        )
    );

    g_assert_null(
        tool_catalog_entry_get_identifier(
            NULL
        )
    );

    g_assert_null(
        tool_catalog_entry_get_display_name(
            NULL
        )
    );

    g_assert_null(
        tool_catalog_entry_get_executable_name(
            NULL
        )
    );

    g_assert_cmpint(
        tool_catalog_entry_get_requirement(
            NULL
        ),
        ==,
        TOOL_REQUIREMENT_OPTIONAL
    );

    g_assert_cmpuint(
        tool_catalog_entry_get_version_argument_count(
            NULL
        ),
        ==,
        0
    );

    g_assert_null(
        tool_catalog_entry_get_version_argument(
            NULL,
            0
        )
    );
}

/**
 * @brief Vérifie la recherche des entrées par identifiant.
 */
static void test_tool_catalog_find(void)
{
    const ToolCatalogEntry *catalog_entry = NULL;

    gsize entry_index = 0;

    for (entry_index = 0;
         entry_index < G_N_ELEMENTS(expected_catalog_entries);
         entry_index++)
    {
        catalog_entry = tool_catalog_find(
            expected_catalog_entries[entry_index].identifier
        );

        g_assert_nonnull(
            catalog_entry
        );

        g_assert_cmpstr(
            tool_catalog_entry_get_identifier(
                catalog_entry
            ),
            ==,
            expected_catalog_entries[entry_index].identifier
        );
    }

    g_assert_null(
        tool_catalog_find(
            "unknown.tool"
        )
    );

    g_assert_null(
        tool_catalog_find(
            NULL
        )
    );

    g_assert_null(
        tool_catalog_find(
            ""
        )
    );
}

/**
 * @brief Vérifie l'unicité des identifiants du catalogue.
 */
static void test_tool_catalog_unique_entries(void)
{
    const ToolCatalogEntry *first_entry = NULL;
    const ToolCatalogEntry *second_entry = NULL;

    const char *first_identifier = NULL;
    const char *second_identifier = NULL;

    gsize first_index = 0;
    gsize second_index = 0;

    for (first_index = 0;
         first_index < tool_catalog_get_count();
         first_index++)
    {
        first_entry = tool_catalog_get_entry(
            first_index
        );

        g_assert_nonnull(
            first_entry
        );

        first_identifier =
            tool_catalog_entry_get_identifier(
                first_entry
            );

        g_assert_nonnull(
            first_identifier
        );

        g_assert_cmpstr(
            first_identifier,
            !=,
            ""
        );

        for (second_index = first_index + 1;
             second_index < tool_catalog_get_count();
             second_index++)
        {
            second_entry = tool_catalog_get_entry(
                second_index
            );

            g_assert_nonnull(
                second_entry
            );

            second_identifier =
                tool_catalog_entry_get_identifier(
                    second_entry
                );

            g_assert_cmpstr(
                first_identifier,
                !=,
                second_identifier
            );
        }
    }
}

/**
 * @brief Vérifie l'enregistrement complet dans un registre vide.
 */
static void test_tool_catalog_register_defaults(void)
{
    ToolRegistry *tool_registry = NULL;

    const ToolInfo *tool_info = NULL;
    const ExpectedToolCatalogEntry *expected_entry = NULL;

    GError *error = NULL;

    gsize entry_index = 0;

    tool_registry = tool_registry_new();

    g_assert_nonnull(
        tool_registry
    );

    g_assert_true(
        tool_catalog_register_defaults(
            tool_registry,
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
        G_N_ELEMENTS(expected_catalog_entries)
    );

    for (entry_index = 0;
         entry_index < G_N_ELEMENTS(expected_catalog_entries);
         entry_index++)
    {
        expected_entry =
            &expected_catalog_entries[entry_index];

        tool_info = tool_registry_get_tool(
            tool_registry,
            entry_index
        );

        g_assert_nonnull(
            tool_info
        );

        g_assert_cmpstr(
            tool_info_get_identifier(
                tool_info
            ),
            ==,
            expected_entry->identifier
        );

        g_assert_cmpstr(
            tool_info_get_display_name(
                tool_info
            ),
            ==,
            expected_entry->display_name
        );

        g_assert_cmpstr(
            tool_info_get_executable_name(
                tool_info
            ),
            ==,
            expected_entry->executable_name
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
    }

    tool_registry_free(
        tool_registry
    );
}

/**
 * @brief Vérifie qu'un doublon empêche toute insertion partielle.
 */
static void test_tool_catalog_register_duplicate(void)
{
    ToolRegistry *tool_registry = NULL;

    const ToolInfo *existing_tool = NULL;

    GError *error = NULL;

    tool_registry = tool_registry_new();

    g_assert_nonnull(
        tool_registry
    );

    g_assert_true(
        tool_registry_register(
            tool_registry,
            "http.curl",
            "Outil existant",
            "existing_curl",
            TOOL_REQUIREMENT_REQUIRED,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_false(
        tool_catalog_register_defaults(
            tool_registry,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_CATALOG_ERROR,
        TOOL_CATALOG_ERROR_REGISTRATION
    );

    g_clear_error(
        &error
    );

    /*
     * Aucun outil du catalogue ne doit avoir été ajouté.
     * Seul l'outil préexistant reste présent.
     */
    g_assert_cmpuint(
        tool_registry_get_count(
            tool_registry
        ),
        ==,
        1
    );

    existing_tool = tool_registry_find(
        tool_registry,
        "http.curl"
    );

    g_assert_nonnull(
        existing_tool
    );

    g_assert_cmpstr(
        tool_info_get_display_name(
            existing_tool
        ),
        ==,
        "Outil existant"
    );

    g_assert_cmpstr(
        tool_info_get_executable_name(
            existing_tool
        ),
        ==,
        "existing_curl"
    );

    g_assert_null(
        tool_registry_find(
            tool_registry,
            "dns.dig"
        )
    );

    g_assert_null(
        tool_registry_find(
            tool_registry,
            "dns.host"
        )
    );

    g_assert_null(
        tool_registry_find(
            tool_registry,
            "network.whois"
        )
    );

    g_assert_null(
        tool_registry_find(
            tool_registry,
            "tls.openssl"
        )
    );

    tool_registry_free(
        tool_registry
    );
}

/**
 * @brief Vérifie les arguments invalides de l'enregistrement.
 */
static void test_tool_catalog_register_invalid_arguments(void)
{
    GError *error = NULL;

    g_assert_false(
        tool_catalog_register_defaults(
            NULL,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_CATALOG_ERROR,
        TOOL_CATALOG_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );
}

/**
 * @brief Vérifie les arguments invalides de la détection.
 */
static void test_tool_catalog_detect_invalid_arguments(void)
{
    ToolRegistry *tool_registry = NULL;

    char *detected_version = NULL;

    GError *error = NULL;

    tool_registry =
        test_tool_catalog_create_registry(
            FALSE
        );

    g_assert_false(
        tool_catalog_detect_version(
            NULL,
            "http.curl",
            NULL,
            &detected_version,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_CATALOG_ERROR,
        TOOL_CATALOG_ERROR_INVALID_ARGUMENT
    );

    g_assert_null(
        detected_version
    );

    g_clear_error(
        &error
    );

    g_assert_false(
        tool_catalog_detect_version(
            tool_registry,
            NULL,
            NULL,
            &detected_version,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_CATALOG_ERROR,
        TOOL_CATALOG_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    g_assert_false(
        tool_catalog_detect_version(
            tool_registry,
            "",
            NULL,
            &detected_version,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_CATALOG_ERROR,
        TOOL_CATALOG_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    g_assert_false(
        tool_catalog_detect_version(
            tool_registry,
            "http.curl",
            NULL,
            NULL,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_CATALOG_ERROR,
        TOOL_CATALOG_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    detected_version =
        g_strdup(
            "version déjà présente"
        );

    g_assert_false(
        tool_catalog_detect_version(
            tool_registry,
            "http.curl",
            NULL,
            &detected_version,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_CATALOG_ERROR,
        TOOL_CATALOG_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    g_clear_pointer(
        &detected_version,
        g_free
    );

    tool_registry_free(
        tool_registry
    );
}

/**
 * @brief Vérifie le refus d'une entrée inconnue du catalogue.
 */
static void test_tool_catalog_detect_unknown_entry(void)
{
    ToolRegistry *tool_registry = NULL;

    char *detected_version = NULL;

    GError *error = NULL;

    tool_registry = tool_registry_new();

    g_assert_nonnull(
        tool_registry
    );

    g_assert_false(
        tool_catalog_detect_version(
            tool_registry,
            "unknown.tool",
            NULL,
            &detected_version,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_CATALOG_ERROR,
        TOOL_CATALOG_ERROR_ENTRY_NOT_FOUND
    );

    g_assert_null(
        detected_version
    );

    g_clear_error(
        &error
    );

    tool_registry_free(
        tool_registry
    );
}

/**
 * @brief Vérifie le refus d'un outil absent du registre.
 */
static void test_tool_catalog_detect_unregistered_tool(void)
{
    ToolRegistry *tool_registry = NULL;

    char *detected_version = NULL;

    GError *error = NULL;

    tool_registry = tool_registry_new();

    g_assert_nonnull(
        tool_registry
    );

    g_assert_false(
        tool_catalog_detect_version(
            tool_registry,
            "http.curl",
            NULL,
            &detected_version,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_CATALOG_ERROR,
        TOOL_CATALOG_ERROR_TOOL_NOT_REGISTERED
    );

    g_assert_null(
        detected_version
    );

    g_clear_error(
        &error
    );

    tool_registry_free(
        tool_registry
    );
}

/**
 * @brief Vérifie le refus d'un outil non encore recherché.
 */
static void test_tool_catalog_detect_unchecked_tool(void)
{
    ToolRegistry *tool_registry = NULL;

    char *detected_version = NULL;

    GError *error = NULL;

    tool_registry =
        test_tool_catalog_create_registry(
            FALSE
        );

    g_assert_false(
        tool_catalog_detect_version(
            tool_registry,
            "http.curl",
            NULL,
            &detected_version,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_CATALOG_ERROR,
        TOOL_CATALOG_ERROR_TOOL_NOT_CHECKED
    );

    g_assert_null(
        detected_version
    );

    g_clear_error(
        &error
    );

    tool_registry_free(
        tool_registry
    );
}

/**
 * @brief Vérifie le refus d'un exécutable absent.
 */
static void test_tool_catalog_detect_missing_tool(void)
{
    ToolRegistry *tool_registry = NULL;

    const ToolInfo *tool_info = NULL;

    char *detected_version = NULL;
    char *fake_curl_path = NULL;

    GError *error = NULL;

    fake_curl_path =
        g_build_filename(
            test_tool_catalog_directory,
            "curl",
            NULL
        );

    /*
     * Garantit que curl est réellement absent du PATH temporaire.
     */
    g_remove(
        fake_curl_path
    );

    tool_registry =
        test_tool_catalog_create_registry(
            TRUE
        );

    tool_info = tool_registry_find(
        tool_registry,
        "http.curl"
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

    g_assert_false(
        tool_catalog_detect_version(
            tool_registry,
            "http.curl",
            NULL,
            &detected_version,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_CATALOG_ERROR,
        TOOL_CATALOG_ERROR_TOOL_MISSING
    );

    g_assert_null(
        detected_version
    );

    g_clear_error(
        &error
    );

    g_free(
        fake_curl_path
    );

    tool_registry_free(
        tool_registry
    );
}

static void test_tool_catalog_version_nonzero_exit(void)
{
    ToolRegistry *tool_registry = NULL;
    char *detected_version = NULL;
    GError *error = NULL;

    test_tool_catalog_write_curl_script(
        "#!/bin/sh\n"
        "printf 'Fake curl 7.0.0\\n'\n"
        "exit 7\n"
    );

    tool_registry =
        test_tool_catalog_create_registry(
            TRUE
        );

    g_assert_false(
        tool_catalog_detect_version(
            tool_registry,
            "http.curl",
            NULL,
            &detected_version,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_CATALOG_ERROR,
        TOOL_CATALOG_ERROR_VERSION_COMMAND
    );

    g_assert_null(
        detected_version
    );

    g_clear_error(
        &error
    );

    tool_registry_free(
        tool_registry
    );
}

static void test_tool_catalog_version_signaled(void)
{
    ToolRegistry *tool_registry = NULL;
    char *detected_version = NULL;
    GError *error = NULL;

    test_tool_catalog_write_curl_script(
        "#!/bin/sh\n"
        "kill -TERM $$\n"
    );

    tool_registry =
        test_tool_catalog_create_registry(
            TRUE
        );

    g_assert_false(
        tool_catalog_detect_version(
            tool_registry,
            "http.curl",
            NULL,
            &detected_version,
            &error
        )
    );

    g_assert_error(
        error,
        TOOL_CATALOG_ERROR,
        TOOL_CATALOG_ERROR_VERSION_COMMAND
    );

    g_assert_null(
        detected_version
    );

    g_clear_error(
        &error
    );

    tool_registry_free(
        tool_registry
    );
}

static void test_tool_catalog_version_cancelled(void)
{
    ToolRegistry *tool_registry = NULL;

    GCancellable *cancellable = NULL;
    GThread *cancellation_thread = NULL;

    ToolCatalogCancellationData cancellation_data = {0};

    char *detected_version = NULL;

    gint64 started_at_us = 0;
    gint64 elapsed_us = 0;

    GError *error = NULL;

    test_tool_catalog_write_curl_script(
        "#!/bin/sh\n"
        "while :\n"
        "do\n"
        "    :\n"
        "done\n"
    );

    tool_registry =
        test_tool_catalog_create_registry(
            TRUE
        );

    cancellable =
        g_cancellable_new();

    cancellation_data.cancellable =
        cancellable;

    cancellation_data.delay_microseconds =
        200000;

    cancellation_thread =
        g_thread_new(
            "tool-catalog-cancellation",
            test_tool_catalog_cancel_after_delay,
            &cancellation_data
        );

    started_at_us =
        g_get_monotonic_time();

    g_assert_false(
        tool_catalog_detect_version(
            tool_registry,
            "http.curl",
            cancellable,
            &detected_version,
            &error
        )
    );

    elapsed_us =
        g_get_monotonic_time() -
        started_at_us;

    g_thread_join(
        cancellation_thread
    );

    g_assert_error(
        error,
        G_IO_ERROR,
        G_IO_ERROR_CANCELLED
    );

    g_assert_null(
        detected_version
    );

    g_assert_cmpint(
        elapsed_us,
        <,
        3000000
    );

    g_clear_error(
        &error
    );

    g_object_unref(
        cancellable
    );

    tool_registry_free(
        tool_registry
    );
}

static void test_tool_catalog_version_registry_independence(void)
{
    ToolRegistry *tool_registry = NULL;

    const ToolInfo *tool_info = NULL;

    char *detected_version = NULL;

    GError *error = NULL;

    test_tool_catalog_write_curl_script(
        "#!/bin/sh\n"
        "printf 'Fake curl 8.1.0\\n'\n"
    );

    tool_registry =
        test_tool_catalog_create_registry(
            TRUE
        );

    tool_info = tool_registry_find(
        tool_registry,
        "http.curl"
    );

    g_assert_nonnull(
        tool_info
    );

    g_assert_null(
        tool_info_get_detected_version(
            tool_info
        )
    );

    g_assert_true(
        tool_catalog_detect_version(
            tool_registry,
            "http.curl",
            NULL,
            &detected_version,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    /*
     * La détection seule ne doit pas modifier le registre.
     */
    g_assert_null(
        tool_info_get_detected_version(
            tool_info
        )
    );

    g_assert_true(
        tool_registry_set_version(
            tool_registry,
            "http.curl",
            detected_version,
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
        "Fake curl 8.1.0"
    );

    g_free(
        detected_version
    );

    tool_registry_free(
        tool_registry
    );
}

static void test_tool_catalog_version_successive_runs(void)
{
    ToolRegistry *tool_registry = NULL;

    char *first_version = NULL;
    char *second_version = NULL;

    GError *error = NULL;

    test_tool_catalog_write_curl_script(
        "#!/bin/sh\n"
        "printf 'Fake curl 1.0.0\\n'\n"
    );

    tool_registry =
        test_tool_catalog_create_registry(
            TRUE
        );

    g_assert_true(
        tool_catalog_detect_version(
            tool_registry,
            "http.curl",
            NULL,
            &first_version,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    test_tool_catalog_write_curl_script(
        "#!/bin/sh\n"
        "printf 'Fake curl 2.0.0\\n'\n"
    );

    g_assert_true(
        tool_catalog_detect_version(
            tool_registry,
            "http.curl",
            NULL,
            &second_version,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_cmpstr(
        first_version,
        ==,
        "Fake curl 1.0.0"
    );

    g_assert_cmpstr(
        second_version,
        ==,
        "Fake curl 2.0.0"
    );

    g_assert_cmpstr(
        first_version,
        !=,
        second_version
    );

    g_free(
        first_version
    );

    g_free(
        second_version
    );

    tool_registry_free(
        tool_registry
    );
}

static void test_tool_catalog_version_output_limit(void)
{
    ToolRegistry *tool_registry = NULL;

    char *long_line = NULL;
    char *script_content = NULL;
    char *detected_version = NULL;

    GError *error = NULL;

    long_line =
        g_strnfill(
            5000,
            'A'
        );

    script_content =
        g_strdup_printf(
            "#!/bin/sh\n"
            "printf '%s\\n'\n",
            long_line
        );

    test_tool_catalog_write_curl_script(
        script_content
    );

    tool_registry =
        test_tool_catalog_create_registry(
            TRUE
        );

    g_assert_true(
        tool_catalog_detect_version(
            tool_registry,
            "http.curl",
            NULL,
            &detected_version,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        detected_version
    );

    g_assert_cmpuint(
        strlen(
            detected_version
        ),
        ==,
        4096
    );

    g_assert_cmpint(
        detected_version[0],
        ==,
        'A'
    );

    g_assert_cmpint(
        detected_version[4095],
        ==,
        'A'
    );

    g_free(
        detected_version
    );

    g_free(
        script_content
    );

    g_free(
        long_line
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
    GError *error = NULL;
    int test_result = 0;

    g_test_init(
        &argc,
        &argv,
        NULL
    );

    test_tool_catalog_directory =
        g_dir_make_tmp(
            "labfy-tool-catalog-XXXXXX",
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        test_tool_catalog_directory
    );

    g_assert_true(
        g_setenv(
            "PATH",
            test_tool_catalog_directory,
            TRUE
        )
    );

    g_test_add_func(
        "/tool_catalog/entries",
        test_tool_catalog_entries
    );

    g_test_add_func(
        "/tool_catalog/invalid_index",
        test_tool_catalog_invalid_index
    );

    g_test_add_func(
        "/tool_catalog/find",
        test_tool_catalog_find
    );

    g_test_add_func(
        "/tool_catalog/unique_entries",
        test_tool_catalog_unique_entries
    );

    g_test_add_func(
        "/tool_catalog/register_defaults",
        test_tool_catalog_register_defaults
    );

    g_test_add_func(
        "/tool_catalog/register_duplicate",
        test_tool_catalog_register_duplicate
    );

    g_test_add_func(
        "/tool_catalog/register_invalid_arguments",
        test_tool_catalog_register_invalid_arguments
    );

    g_test_add_func(
        "/tool_catalog/detect_invalid_arguments",
        test_tool_catalog_detect_invalid_arguments
    );

    g_test_add_func(
        "/tool_catalog/detect_unknown_entry",
        test_tool_catalog_detect_unknown_entry
    );

    g_test_add_func(
        "/tool_catalog/detect_unregistered_tool",
        test_tool_catalog_detect_unregistered_tool
    );

    g_test_add_func(
        "/tool_catalog/detect_unchecked_tool",
        test_tool_catalog_detect_unchecked_tool
    );

    g_test_add_func(
        "/tool_catalog/detect_missing_tool",
        test_tool_catalog_detect_missing_tool
    );

    g_test_add_func(
        "/tool_catalog/version_stdout",
        test_tool_catalog_version_stdout
    );

    g_test_add_func(
        "/tool_catalog/version_stderr",
        test_tool_catalog_version_stderr
    );

    g_test_add_func(
        "/tool_catalog/version_first_nonempty_line",
        test_tool_catalog_version_first_nonempty_line
    );

    g_test_add_func(
        "/tool_catalog/version_normalization",
        test_tool_catalog_version_normalization
    );

    g_test_add_func(
        "/tool_catalog/version_non_utf8",
        test_tool_catalog_version_non_utf8
    );

    g_test_add_func(
        "/tool_catalog/version_empty",
        test_tool_catalog_version_empty
    );

    g_test_add_func(
        "/tool_catalog/version_nonzero_exit",
        test_tool_catalog_version_nonzero_exit
    );

    g_test_add_func(
        "/tool_catalog/version_signaled",
        test_tool_catalog_version_signaled
    );

    g_test_add_func(
        "/tool_catalog/version_cancelled",
        test_tool_catalog_version_cancelled
    );

    g_test_add_func(
        "/tool_catalog/version_registry_independence",
        test_tool_catalog_version_registry_independence
    );

    g_test_add_func(
        "/tool_catalog/version_successive_runs",
        test_tool_catalog_version_successive_runs
    );

    g_test_add_func(
        "/tool_catalog/version_output_limit",
        test_tool_catalog_version_output_limit
    );

    test_result = g_test_run();

    {
        char *curl_path =
            g_build_filename(
                test_tool_catalog_directory,
                "curl",
                NULL
            );

        g_remove(
            curl_path
        );

        g_free(
            curl_path
        );
    }

    g_rmdir(
        test_tool_catalog_directory
    );

    g_clear_pointer(
        &test_tool_catalog_directory,
        g_free
    );

    return test_result;
}
