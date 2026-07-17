/******************************************************************************
 * @file tool_registry.c
 * @brief Implémentation du registre des outils externes.
 ******************************************************************************/

#include "core/tool_registry.h"

/**
 * @struct ToolInfo
 * @brief Informations internes décrivant un outil externe.
 */
struct ToolInfo
{
    char *identifier;
    char *display_name;
    char *executable_name;

    ToolRequirement requirement;
    ToolAvailability availability;

    char *resolved_path;
    char *detected_version;
};

/**
 * @struct ToolRegistry
 * @brief Registre interne des outils externes.
 */
struct ToolRegistry
{
    GPtrArray *tools;
};

/**
 * @brief Vérifie qu'une chaîne est définie et non vide.
 *
 * @param text Chaîne à vérifier.
 *
 * @return TRUE si la chaîne est valide.
 */
static gboolean tool_registry_string_is_valid(
    const char *text
)
{
    return text != NULL &&
           text[0] != '\0';
}

/**
 * @brief Vérifie qu'une importance de dépendance est valide.
 *
 * @param requirement Valeur à vérifier.
 *
 * @return TRUE si la valeur est reconnue.
 */
static gboolean tool_registry_requirement_is_valid(
    ToolRequirement requirement
)
{
    return requirement == TOOL_REQUIREMENT_OPTIONAL ||
           requirement == TOOL_REQUIREMENT_REQUIRED;
}

/**
 * @brief Libère un outil et toutes ses chaînes.
 *
 * Cette fonction est utilisée comme fonction de destruction du GPtrArray.
 *
 * @param user_data Pointeur vers ToolInfo.
 */
static void tool_info_free(
    gpointer user_data
)
{
    ToolInfo *tool_info = user_data;

    if (tool_info == NULL)
    {
        return;
    }

    g_clear_pointer(
        &tool_info->identifier,
        g_free
    );

    g_clear_pointer(
        &tool_info->display_name,
        g_free
    );

    g_clear_pointer(
        &tool_info->executable_name,
        g_free
    );

    g_clear_pointer(
        &tool_info->resolved_path,
        g_free
    );

    g_clear_pointer(
        &tool_info->detected_version,
        g_free
    );

    g_free(
        tool_info
    );
}

/**
 * @brief Recherche un outil modifiable dans le registre.
 *
 * @param tool_registry Registre consulté.
 * @param identifier Identifiant recherché.
 *
 * @return Outil correspondant, ou NULL.
 */
static ToolInfo *tool_registry_find_internal(
    const ToolRegistry *tool_registry,
    const char *identifier
)
{
    guint index = 0;

    if (tool_registry == NULL ||
        tool_registry->tools == NULL ||
        !tool_registry_string_is_valid(identifier))
    {
        return NULL;
    }

    for (index = 0;
         index < tool_registry->tools->len;
         index++)
    {
        ToolInfo *tool_info = NULL;

        tool_info = g_ptr_array_index(
            tool_registry->tools,
            index
        );

        if (tool_info != NULL &&
            g_strcmp0(
                tool_info->identifier,
                identifier
            ) == 0)
        {
            return tool_info;
        }
    }

    return NULL;
}

GQuark tool_registry_error_quark(void)
{
    return g_quark_from_static_string(
        "labfy-investigation-tool-registry-error"
    );
}

ToolRegistry *tool_registry_new(void)
{
    ToolRegistry *tool_registry = NULL;

    tool_registry = g_new0(
        ToolRegistry,
        1
    );

    tool_registry->tools =
        g_ptr_array_new_with_free_func(
            tool_info_free
        );

    return tool_registry;
}

void tool_registry_free(
    ToolRegistry *tool_registry
)
{
    if (tool_registry == NULL)
    {
        return;
    }

    if (tool_registry->tools != NULL)
    {
        g_ptr_array_unref(
            tool_registry->tools
        );

        tool_registry->tools = NULL;
    }

    g_free(
        tool_registry
    );
}

gboolean tool_registry_register(
    ToolRegistry *tool_registry,
    const char *identifier,
    const char *display_name,
    const char *executable_name,
    ToolRequirement requirement,
    GError **error
)
{
    ToolInfo *tool_info = NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (tool_registry == NULL ||
        tool_registry->tools == NULL ||
        !tool_registry_string_is_valid(identifier) ||
        !tool_registry_string_is_valid(display_name) ||
        !tool_registry_string_is_valid(executable_name) ||
        !tool_registry_requirement_is_valid(requirement))
    {
        g_set_error_literal(
            error,
            TOOL_REGISTRY_ERROR,
            TOOL_REGISTRY_ERROR_INVALID_ARGUMENT,
            "Les arguments fournis au registre d'outils sont invalides."
        );

        return FALSE;
    }

    if (tool_registry_find_internal(
            tool_registry,
            identifier
        ) != NULL)
    {
        g_set_error(
            error,
            TOOL_REGISTRY_ERROR,
            TOOL_REGISTRY_ERROR_DUPLICATE_IDENTIFIER,
            "Un outil portant l'identifiant '%s' est déjà enregistré.",
            identifier
        );

        return FALSE;
    }

    tool_info = g_new0(
        ToolInfo,
        1
    );

    tool_info->identifier = g_strdup(
        identifier
    );

    tool_info->display_name = g_strdup(
        display_name
    );

    tool_info->executable_name = g_strdup(
        executable_name
    );

    tool_info->requirement = requirement;
    tool_info->availability =
        TOOL_AVAILABILITY_UNKNOWN;

    tool_info->resolved_path = NULL;
    tool_info->detected_version = NULL;

    g_ptr_array_add(
        tool_registry->tools,
        tool_info
    );

    return TRUE;
}

gboolean tool_registry_refresh(
    ToolRegistry *tool_registry,
    GError **error
)
{
    guint index = 0;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (tool_registry == NULL ||
        tool_registry->tools == NULL)
    {
        g_set_error_literal(
            error,
            TOOL_REGISTRY_ERROR,
            TOOL_REGISTRY_ERROR_INVALID_ARGUMENT,
            "Le registre d'outils est invalide."
        );

        return FALSE;
    }

    for (index = 0;
         index < tool_registry->tools->len;
         index++)
    {
        ToolInfo *tool_info = NULL;
        char *found_path = NULL;
        char *canonical_path = NULL;

        tool_info = g_ptr_array_index(
            tool_registry->tools,
            index
        );

        if (tool_info == NULL)
        {
            continue;
        }

        found_path = g_find_program_in_path(
            tool_info->executable_name
        );

        g_clear_pointer(
            &tool_info->resolved_path,
            g_free
        );

        if (found_path == NULL)
        {
            tool_info->availability =
                TOOL_AVAILABILITY_MISSING;

            continue;
        }

        /*
         * g_find_program_in_path() retourne une nouvelle chaîne.
         *
         * La canonicalisation garantit que le registre mémorise un
         * chemin absolu et normalisé.
         */
        canonical_path = g_canonicalize_filename(
            found_path,
            NULL
        );

        g_free(
            found_path
        );

        tool_info->resolved_path =
            canonical_path;

        tool_info->availability =
            TOOL_AVAILABILITY_AVAILABLE;
    }

    return TRUE;
}

gsize tool_registry_get_count(
    const ToolRegistry *tool_registry
)
{
    if (tool_registry == NULL ||
        tool_registry->tools == NULL)
    {
        return 0;
    }

    return tool_registry->tools->len;
}

const ToolInfo *tool_registry_get_tool(
    const ToolRegistry *tool_registry,
    gsize index
)
{
    if (tool_registry == NULL ||
        tool_registry->tools == NULL ||
        index >= tool_registry->tools->len)
    {
        return NULL;
    }

    return g_ptr_array_index(
        tool_registry->tools,
        (guint) index
    );
}

const ToolInfo *tool_registry_find(
    const ToolRegistry *tool_registry,
    const char *identifier
)
{
    return tool_registry_find_internal(
        tool_registry,
        identifier
    );
}

gboolean tool_registry_set_version(
    ToolRegistry *tool_registry,
    const char *identifier,
    const char *detected_version,
    GError **error
)
{
    ToolInfo *tool_info = NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (tool_registry == NULL ||
        tool_registry->tools == NULL ||
        !tool_registry_string_is_valid(identifier))
    {
        g_set_error_literal(
            error,
            TOOL_REGISTRY_ERROR,
            TOOL_REGISTRY_ERROR_INVALID_ARGUMENT,
            "Le registre ou l'identifiant de l'outil est invalide."
        );

        return FALSE;
    }

    tool_info = tool_registry_find_internal(
        tool_registry,
        identifier
    );

    if (tool_info == NULL)
    {
        g_set_error(
            error,
            TOOL_REGISTRY_ERROR,
            TOOL_REGISTRY_ERROR_NOT_FOUND,
            "Aucun outil portant l'identifiant '%s' n'est enregistré.",
            identifier
        );

        return FALSE;
    }

    g_clear_pointer(
        &tool_info->detected_version,
        g_free
    );

    if (tool_registry_string_is_valid(
            detected_version
        ))
    {
        tool_info->detected_version =
            g_strdup(
                detected_version
            );
    }

    return TRUE;
}

gboolean tool_registry_has_missing_required_tools(
    const ToolRegistry *tool_registry
)
{
    guint index = 0;

    if (tool_registry == NULL ||
        tool_registry->tools == NULL)
    {
        return FALSE;
    }

    for (index = 0;
         index < tool_registry->tools->len;
         index++)
    {
        const ToolInfo *tool_info = NULL;

        tool_info = g_ptr_array_index(
            tool_registry->tools,
            index
        );

        if (tool_info != NULL &&
            tool_info->requirement ==
                TOOL_REQUIREMENT_REQUIRED &&
            tool_info->availability ==
                TOOL_AVAILABILITY_MISSING)
        {
            return TRUE;
        }
    }

    return FALSE;
}

gboolean tool_registry_all_required_tools_available(
    const ToolRegistry *tool_registry
)
{
    guint index = 0;

    if (tool_registry == NULL ||
        tool_registry->tools == NULL)
    {
        return FALSE;
    }

    for (index = 0;
         index < tool_registry->tools->len;
         index++)
    {
        const ToolInfo *tool_info = NULL;

        tool_info = g_ptr_array_index(
            tool_registry->tools,
            index
        );

        if (tool_info != NULL &&
            tool_info->requirement ==
                TOOL_REQUIREMENT_REQUIRED &&
            tool_info->availability !=
                TOOL_AVAILABILITY_AVAILABLE)
        {
            return FALSE;
        }
    }

    /*
     * Lorsqu'aucun outil obligatoire n'est enregistré, toutes les
     * dépendances obligatoires sont considérées comme satisfaites.
     */
    return TRUE;
}

const char *tool_info_get_identifier(
    const ToolInfo *tool_info
)
{
    if (tool_info == NULL)
    {
        return NULL;
    }

    return tool_info->identifier;
}

const char *tool_info_get_display_name(
    const ToolInfo *tool_info
)
{
    if (tool_info == NULL)
    {
        return NULL;
    }

    return tool_info->display_name;
}

const char *tool_info_get_executable_name(
    const ToolInfo *tool_info
)
{
    if (tool_info == NULL)
    {
        return NULL;
    }

    return tool_info->executable_name;
}

ToolRequirement tool_info_get_requirement(
    const ToolInfo *tool_info
)
{
    if (tool_info == NULL)
    {
        return TOOL_REQUIREMENT_OPTIONAL;
    }

    return tool_info->requirement;
}

ToolAvailability tool_info_get_availability(
    const ToolInfo *tool_info
)
{
    if (tool_info == NULL)
    {
        return TOOL_AVAILABILITY_UNKNOWN;
    }

    return tool_info->availability;
}

const char *tool_info_get_resolved_path(
    const ToolInfo *tool_info
)
{
    if (tool_info == NULL)
    {
        return NULL;
    }

    return tool_info->resolved_path;
}

const char *tool_info_get_detected_version(
    const ToolInfo *tool_info
)
{
    if (tool_info == NULL)
    {
        return NULL;
    }

    return tool_info->detected_version;
}
