/******************************************************************************
 * @file tool_catalog.c
 * @brief Catalogue statique des outils externes connus.
 ******************************************************************************/

#include "core/tool_catalog.h"
#include "core/tool_process.h"

#define TOOL_CATALOG_VERSION_OUTPUT_LIMIT 4096

/**
 * @struct ToolCatalogEntry
 * @brief Description statique d'un outil externe connu.
 */
struct ToolCatalogEntry
{
    const char *identifier;
    const char *display_name;
    const char *executable_name;

    ToolRequirement requirement;

    const char *const *version_arguments;
    gsize version_argument_count;
};

/*
 * Arguments de détection des versions.
 *
 * Chaque tableau est terminé par NULL afin de pouvoir être transmis
 * directement à ToolProcess.
 */
static const char *const tool_catalog_dig_version_arguments[] =
{
    "-v",
    NULL
};

static const char *const tool_catalog_host_version_arguments[] =
{
    "-V",
    NULL
};

static const char *const tool_catalog_whois_version_arguments[] =
{
    "--version",
    NULL
};

static const char *const tool_catalog_curl_version_arguments[] =
{
    "--version",
    NULL
};

static const char *const tool_catalog_openssl_version_arguments[] =
{
    "version",
    NULL
};
static const char *const tool_catalog_tesseract_version_arguments[] =
{
    "--version",
    NULL
};
static const char *const tool_catalog_exiftool_version_arguments[] =
{
    "-ver",
    NULL
};
static const char *const tool_catalog_john_version_arguments[] =
{
    "--list=build-info",
    NULL
};
static const char *const tool_catalog_pdf2john_version_arguments[] =
{
    "--help",
    NULL
};
static const char *const tool_catalog_qpdf_version_arguments[] =
{
    "--version",
    NULL
};
static const char *const tool_catalog_sherlock_version_arguments[] =
{
    "--version",
    NULL
};
static const char *const tool_catalog_maigret_version_arguments[] =
{
    "--version",
    NULL
};
static const char *const tool_catalog_holehe_version_arguments[] =
{
    "--version",
    NULL
};

/**
 * @brief Catalogue statique initial.
 */
static const ToolCatalogEntry tool_catalog_entries[] =
{
    {
        .identifier = "dns.dig",
        .display_name = "dig",
        .executable_name = "dig",
        .requirement = TOOL_REQUIREMENT_OPTIONAL,
        .version_arguments =
            tool_catalog_dig_version_arguments,
        .version_argument_count = 1
    },
    {
        .identifier = "dns.host",
        .display_name = "host",
        .executable_name = "host",
        .requirement = TOOL_REQUIREMENT_OPTIONAL,
        .version_arguments =
            tool_catalog_host_version_arguments,
        .version_argument_count = 1
    },
    {
        .identifier = "network.whois",
        .display_name = "whois",
        .executable_name = "whois",
        .requirement = TOOL_REQUIREMENT_OPTIONAL,
        .version_arguments =
            tool_catalog_whois_version_arguments,
        .version_argument_count = 1
    },
    {
        .identifier = "http.curl",
        .display_name = "curl",
        .executable_name = "curl",
        .requirement = TOOL_REQUIREMENT_OPTIONAL,
        .version_arguments =
            tool_catalog_curl_version_arguments,
        .version_argument_count = 1
    },
    {
        .identifier = "tls.openssl",
        .display_name = "OpenSSL",
        .executable_name = "openssl",
        .requirement = TOOL_REQUIREMENT_OPTIONAL,
        .version_arguments =
            tool_catalog_openssl_version_arguments,
        .version_argument_count = 1
    },
    {
        .identifier = "ocr.tesseract",
        .display_name = "Tesseract OCR",
        .executable_name = "tesseract",
        .requirement = TOOL_REQUIREMENT_OPTIONAL,
        .version_arguments = tool_catalog_tesseract_version_arguments,
        .version_argument_count = 1
    },
    {
        .identifier = "metadata.exiftool",
        .display_name = "ExifTool",
        .executable_name = "exiftool",
        .requirement = TOOL_REQUIREMENT_OPTIONAL,
        .version_arguments = tool_catalog_exiftool_version_arguments,
        .version_argument_count = 1
    },
    {
        .identifier = "password.john",
        .display_name = "John the Ripper",
        .executable_name = "john",
        .requirement = TOOL_REQUIREMENT_OPTIONAL,
        .version_arguments = tool_catalog_john_version_arguments,
        .version_argument_count = 1
    },
    {
        .identifier = "password.pdf2john",
        .display_name = "pdf2john",
        .executable_name = "pdf2john",
        .requirement = TOOL_REQUIREMENT_OPTIONAL,
        .version_arguments = tool_catalog_pdf2john_version_arguments,
        .version_argument_count = 1
    },
    {
        .identifier = "pdf.qpdf",
        .display_name = "qpdf",
        .executable_name = "qpdf",
        .requirement = TOOL_REQUIREMENT_OPTIONAL,
        .version_arguments = tool_catalog_qpdf_version_arguments,
        .version_argument_count = 1
    },
    {
        .identifier = "osint.sherlock",
        .display_name = "Sherlock",
        .executable_name = "sherlock",
        .requirement = TOOL_REQUIREMENT_OPTIONAL,
        .version_arguments = tool_catalog_sherlock_version_arguments,
        .version_argument_count = 1
    },
    {
        .identifier = "osint.maigret",
        .display_name = "Maigret",
        .executable_name = "maigret",
        .requirement = TOOL_REQUIREMENT_OPTIONAL,
        .version_arguments = tool_catalog_maigret_version_arguments,
        .version_argument_count = 1
    },
    {
        .identifier = "osint.holehe",
        .display_name = "Holehe (vérification email)",
        .executable_name = "holehe",
        .requirement = TOOL_REQUIREMENT_OPTIONAL,
        .version_arguments = tool_catalog_holehe_version_arguments,
        .version_argument_count = 1
    }
};

/**
 * @brief Vérifie qu'une chaîne est définie et non vide.
 */
static gboolean tool_catalog_string_is_valid(
    const char *text
)
{
    return text != NULL &&
           text[0] != '\0';
}

/**
 * @brief Encapsule une erreur provenant d'un autre module.
 */
static void tool_catalog_set_wrapped_error(
    GError **error,
    ToolCatalogError error_code,
    const char *context_message,
    const GError *cause
)
{
    if (cause == NULL ||
        cause->message == NULL)
    {
        g_set_error_literal(
            error,
            TOOL_CATALOG_ERROR,
            error_code,
            context_message
        );

        return;
    }

    g_set_error(
        error,
        TOOL_CATALOG_ERROR,
        error_code,
        "%s : %s",
        context_message,
        cause->message
    );
}

/**
 * @brief Extrait la première ligne non vide d'une sortie.
 *
 * Au maximum TOOL_CATALOG_VERSION_OUTPUT_LIMIT octets sont inspectés.
 * Les séquences UTF-8 invalides sont remplacées.
 *
 * @param bytes Sortie brute du processus.
 *
 * @return Nouvelle chaîne normalisée, ou NULL.
 */
static char *tool_catalog_extract_first_nonempty_line(
    GBytes *bytes
)
{
    gconstpointer bytes_data = NULL;

    gsize bytes_size = 0;
    gsize inspected_size = 0;
    gsize line_index = 0;

    char *utf8_text = NULL;
    char **lines = NULL;
    char *version = NULL;
    char *trimmed_line = NULL;

    if (bytes == NULL)
    {
        return NULL;
    }

    bytes_data = g_bytes_get_data(
        bytes,
        &bytes_size
    );

    if (bytes_data == NULL ||
        bytes_size == 0)
    {
        return NULL;
    }

    inspected_size = MIN(
        bytes_size,
        (gsize) TOOL_CATALOG_VERSION_OUTPUT_LIMIT
    );

    utf8_text = g_utf8_make_valid(
        bytes_data,
        (gssize) inspected_size
    );

    if (utf8_text == NULL)
    {
        return NULL;
    }

    lines = g_strsplit(
        utf8_text,
        "\n",
        -1
    );

    if (lines == NULL)
    {
        g_free(
            utf8_text
        );

        return NULL;
    }

    for (line_index = 0;
         lines[line_index] != NULL;
         line_index++)
    {
        /*
         * g_strstrip() retire notamment :
         *
         * - espaces ;
         * - tabulations ;
         * - retours chariot ;
         * - fins de ligne.
         */
        trimmed_line = g_strstrip(
            lines[line_index]
        );

        if (trimmed_line[0] == '\0')
        {
            continue;
        }

        version = g_strdup(
            trimmed_line
        );

        break;
    }

    g_strfreev(
        lines
    );

    g_free(
        utf8_text
    );

    return version;
}

GQuark tool_catalog_error_quark(void)
{
    return g_quark_from_static_string(
        "labfy-investigation-tool-catalog-error"
    );
}

gsize tool_catalog_get_count(void)
{
    return G_N_ELEMENTS(
        tool_catalog_entries
    );
}

const ToolCatalogEntry *tool_catalog_get_entry(
    gsize index
)
{
    if (index >= tool_catalog_get_count())
    {
        return NULL;
    }

    return &tool_catalog_entries[index];
}

const ToolCatalogEntry *tool_catalog_find(
    const char *identifier
)
{
    gsize entry_index = 0;

    if (!tool_catalog_string_is_valid(
            identifier
        ))
    {
        return NULL;
    }

    for (entry_index = 0;
         entry_index < tool_catalog_get_count();
         entry_index++)
    {
        if (g_strcmp0(
                tool_catalog_entries[entry_index].identifier,
                identifier
            ) == 0)
        {
            return &tool_catalog_entries[entry_index];
        }
    }

    return NULL;
}

const char *tool_catalog_entry_get_identifier(
    const ToolCatalogEntry *entry
)
{
    if (entry == NULL)
    {
        return NULL;
    }

    return entry->identifier;
}

const char *tool_catalog_entry_get_display_name(
    const ToolCatalogEntry *entry
)
{
    if (entry == NULL)
    {
        return NULL;
    }

    return entry->display_name;
}

const char *tool_catalog_entry_get_executable_name(
    const ToolCatalogEntry *entry
)
{
    if (entry == NULL)
    {
        return NULL;
    }

    return entry->executable_name;
}

ToolRequirement tool_catalog_entry_get_requirement(
    const ToolCatalogEntry *entry
)
{
    if (entry == NULL)
    {
        /*
         * Valeur neutre et non bloquante pour un appel invalide.
         */
        return TOOL_REQUIREMENT_OPTIONAL;
    }

    return entry->requirement;
}

gsize tool_catalog_entry_get_version_argument_count(
    const ToolCatalogEntry *entry
)
{
    if (entry == NULL)
    {
        return 0;
    }

    return entry->version_argument_count;
}

const char *tool_catalog_entry_get_version_argument(
    const ToolCatalogEntry *entry,
    gsize index
)
{
    if (entry == NULL ||
        entry->version_arguments == NULL ||
        index >= entry->version_argument_count)
    {
        return NULL;
    }

    return entry->version_arguments[index];
}

gboolean tool_catalog_register_defaults(
    ToolRegistry *tool_registry,
    GError **error
)
{
    const ToolCatalogEntry *entry = NULL;

    GError *registration_error = NULL;

    gsize entry_index = 0;

    gboolean registration_success = FALSE;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (tool_registry == NULL)
    {
        g_set_error_literal(
            error,
            TOOL_CATALOG_ERROR,
            TOOL_CATALOG_ERROR_INVALID_ARGUMENT,
            "Le registre fourni au catalogue est invalide."
        );

        return FALSE;
    }

    /*
     * Prévalidation complète.
     *
     * Aucun outil ne doit être ajouté lorsqu'au moins un identifiant
     * existe déjà dans le registre.
     */
    for (entry_index = 0;
         entry_index < tool_catalog_get_count();
         entry_index++)
    {
        entry = tool_catalog_get_entry(
            entry_index
        );

        if (entry == NULL)
        {
            g_set_error_literal(
                error,
                TOOL_CATALOG_ERROR,
                TOOL_CATALOG_ERROR_REGISTRATION,
                "Le catalogue contient une entrée invalide."
            );

            return FALSE;
        }

        if (tool_registry_find(
                tool_registry,
                entry->identifier
            ) != NULL)
        {
            g_set_error(
                error,
                TOOL_CATALOG_ERROR,
                TOOL_CATALOG_ERROR_REGISTRATION,
                "L'outil '%s' existe déjà dans le registre.",
                entry->identifier
            );

            return FALSE;
        }
    }

    /*
     * Aucun doublon n'a été trouvé. Les entrées peuvent maintenant
     * être enregistrées.
     */
    for (entry_index = 0;
         entry_index < tool_catalog_get_count();
         entry_index++)
    {
        entry = tool_catalog_get_entry(
            entry_index
        );

        registration_success =
            tool_registry_register(
                tool_registry,
                entry->identifier,
                entry->display_name,
                entry->executable_name,
                entry->requirement,
                &registration_error
            );

        if (!registration_success)
        {
            tool_catalog_set_wrapped_error(
                error,
                TOOL_CATALOG_ERROR_REGISTRATION,
                "Une entrée du catalogue n'a pas pu être enregistrée",
                registration_error
            );

            g_clear_error(
                &registration_error
            );

            return FALSE;
        }
    }

    return TRUE;
}

gboolean tool_catalog_detect_version(
    const ToolRegistry *tool_registry,
    const char *identifier,
    GCancellable *cancellable,
    char **out_version,
    GError **error
)
{
    const ToolCatalogEntry *catalog_entry = NULL;
    const ToolInfo *tool_info = NULL;

    ToolAvailability availability;

    const char *resolved_path = NULL;

    char *executable_path = NULL;
    char *detected_version = NULL;

    ToolProcessResult *process_result = NULL;

    GBytes *stdout_bytes = NULL;
    GBytes *stderr_bytes = NULL;

    GError *process_error = NULL;

    gboolean process_success = FALSE;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (tool_registry == NULL ||
        !tool_catalog_string_is_valid(
            identifier
        ) ||
        out_version == NULL ||
        *out_version != NULL)
    {
        g_set_error_literal(
            error,
            TOOL_CATALOG_ERROR,
            TOOL_CATALOG_ERROR_INVALID_ARGUMENT,
            "Les arguments de détection de version sont invalides."
        );

        return FALSE;
    }

    *out_version = NULL;

    catalog_entry = tool_catalog_find(
        identifier
    );

    if (catalog_entry == NULL)
    {
        g_set_error(
            error,
            TOOL_CATALOG_ERROR,
            TOOL_CATALOG_ERROR_ENTRY_NOT_FOUND,
            "L'identifiant '%s' n'existe pas dans le catalogue.",
            identifier
        );

        return FALSE;
    }

    tool_info = tool_registry_find(
        tool_registry,
        identifier
    );

    if (tool_info == NULL)
    {
        g_set_error(
            error,
            TOOL_CATALOG_ERROR,
            TOOL_CATALOG_ERROR_TOOL_NOT_REGISTERED,
            "L'outil '%s' n'est pas enregistré dans le registre.",
            identifier
        );

        return FALSE;
    }

    availability = tool_info_get_availability(
        tool_info
    );

    if (availability ==
        TOOL_AVAILABILITY_UNKNOWN)
    {
        g_set_error(
            error,
            TOOL_CATALOG_ERROR,
            TOOL_CATALOG_ERROR_TOOL_NOT_CHECKED,
            "La disponibilité de l'outil '%s' n'a pas été vérifiée.",
            identifier
        );

        return FALSE;
    }

    if (availability ==
        TOOL_AVAILABILITY_MISSING)
    {
        g_set_error(
            error,
            TOOL_CATALOG_ERROR,
            TOOL_CATALOG_ERROR_TOOL_MISSING,
            "L'outil '%s' est absent de la machine.",
            identifier
        );

        return FALSE;
    }

    if (availability !=
        TOOL_AVAILABILITY_AVAILABLE)
    {
        g_set_error(
            error,
            TOOL_CATALOG_ERROR,
            TOOL_CATALOG_ERROR_INVALID_TOOL_STATE,
            "L'outil '%s' possède un état de disponibilité invalide.",
            identifier
        );

        return FALSE;
    }

    resolved_path = tool_info_get_resolved_path(
        tool_info
    );

    if (!tool_catalog_string_is_valid(
            resolved_path
        ))
    {
        g_set_error(
            error,
            TOOL_CATALOG_ERROR,
            TOOL_CATALOG_ERROR_INVALID_TOOL_STATE,
            "L'outil '%s' est disponible mais ne possède aucun chemin.",
            identifier
        );

        return FALSE;
    }

    /*
     * La copie permet de ne plus dépendre du ToolInfo pendant
     * l'exécution du processus.
     */
    executable_path = g_strdup(
        resolved_path
    );

    process_success = tool_process_run(
        executable_path,
        catalog_entry->version_arguments,
        NULL,
        cancellable,
        &process_result,
        &process_error
    );

    g_free(
        executable_path
    );

    executable_path = NULL;

    if (!process_success)
    {
        if (process_error != NULL &&
            g_error_matches(
                process_error,
                TOOL_PROCESS_ERROR,
                TOOL_PROCESS_ERROR_CANCELLED
            ))
        {
            g_set_error(
                error,
                G_IO_ERROR,
                G_IO_ERROR_CANCELLED,
                "%s",
                process_error->message != NULL
                    ? process_error->message
                    : "La détection de version a été annulée."
            );
        }
        else
        {
            tool_catalog_set_wrapped_error(
                error,
                TOOL_CATALOG_ERROR_PROCESS,
                "La commande de version n'a pas pu être exécutée",
                process_error
            );
        }

        g_clear_error(
            &process_error
        );

        tool_process_result_free(
            process_result
        );

        return FALSE;
    }

    if (process_result == NULL)
    {
        g_set_error_literal(
            error,
            TOOL_CATALOG_ERROR,
            TOOL_CATALOG_ERROR_PROCESS,
            "La commande de version n'a produit aucun résultat."
        );

        return FALSE;
    }

    /*
     * Une commande de version doit se terminer normalement avec
     * le code zéro.
     */
    if (!tool_process_result_exited_normally(
            process_result
        ))
    {
        if (tool_process_result_was_signaled(
                process_result
            ))
        {
            g_set_error(
                error,
                TOOL_CATALOG_ERROR,
                TOOL_CATALOG_ERROR_VERSION_COMMAND,
                "La commande de version de '%s' a été terminée "
                "par le signal %d.",
                identifier,
                tool_process_result_get_termination_signal(
                    process_result
                )
            );
        }
        else
        {
            g_set_error(
                error,
                TOOL_CATALOG_ERROR,
                TOOL_CATALOG_ERROR_VERSION_COMMAND,
                "La commande de version de '%s' ne s'est pas "
                "terminée normalement.",
                identifier
            );
        }

        tool_process_result_free(
            process_result
        );

        return FALSE;
    }

    if (tool_process_result_get_exit_status(
            process_result
        ) != 0)
    {
        g_set_error(
            error,
            TOOL_CATALOG_ERROR,
            TOOL_CATALOG_ERROR_VERSION_COMMAND,
            "La commande de version de '%s' a retourné le code %d.",
            identifier,
            tool_process_result_get_exit_status(
                process_result
            )
        );

        tool_process_result_free(
            process_result
        );

        return FALSE;
    }

    stdout_bytes =
        tool_process_result_ref_stdout(
            process_result
        );

    detected_version =
        tool_catalog_extract_first_nonempty_line(
            stdout_bytes
        );

    /*
     * Certains outils écrivent leur version sur stderr.
     */
    if (detected_version == NULL)
    {
        stderr_bytes =
            tool_process_result_ref_stderr(
                process_result
            );

        detected_version =
            tool_catalog_extract_first_nonempty_line(
                stderr_bytes
            );
    }

    g_clear_pointer(
        &stdout_bytes,
        g_bytes_unref
    );

    g_clear_pointer(
        &stderr_bytes,
        g_bytes_unref
    );

    tool_process_result_free(
        process_result
    );

    process_result = NULL;

    if (detected_version == NULL ||
        detected_version[0] == '\0')
    {
        g_clear_pointer(
            &detected_version,
            g_free
        );

        g_set_error(
            error,
            TOOL_CATALOG_ERROR,
            TOOL_CATALOG_ERROR_VERSION_OUTPUT,
            "L'outil '%s' n'a produit aucune version exploitable.",
            identifier
        );

        return FALSE;
    }

    *out_version = detected_version;

    return TRUE;
}
