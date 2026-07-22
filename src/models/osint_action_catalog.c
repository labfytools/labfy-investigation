/******************************************************************************
 * @file osint_action_catalog.c
 * @brief Implémentation du catalogue des actions OSINT.
 ******************************************************************************/

#include "models/osint_action_catalog.h"

struct OsintAction
{
    char *identifier;
    char *label;
    char *description;
    OsintSelectionContextKind target_kind;
    char *compatible_type;
    char *required_tool_identifier;
    gboolean available;
    char *unavailable_reason;
    char *tool_version;
};

struct OsintActionCatalog
{
    GPtrArray *actions;
};

/** @brief Libère une action possédée par le catalogue. */
static void osint_action_free(gpointer data)
{
    OsintAction *action = data;
    if (action == NULL) return;
    g_free(action->unavailable_reason);
    g_free(action->tool_version);
    g_free(action->compatible_type);
    g_free(action->required_tool_identifier);
    g_free(action->description);
    g_free(action->label);
    g_free(action->identifier);
    g_free(action);
}

/** @brief Crée une action en copiant toutes ses chaînes. */
static OsintAction *osint_action_new(
    const char *identifier,
    const char *label,
    const char *description,
    OsintSelectionContextKind target_kind,
    const char *compatible_type,
    const char *required_tool_identifier,
    gboolean available,
    const char *unavailable_reason
)
{
    OsintAction *action = g_try_new0(OsintAction, 1);
    if (action == NULL) return NULL;
    action->identifier = g_strdup(identifier);
    action->label = g_strdup(label);
    action->description = g_strdup(description);
    action->target_kind = target_kind;
    action->compatible_type = g_strdup(compatible_type);
    action->required_tool_identifier = g_strdup(required_tool_identifier);
    action->available = available;
    action->unavailable_reason = g_strdup(unavailable_reason);
    if (action->identifier == NULL || action->label == NULL ||
        action->description == NULL ||
        (!available && action->unavailable_reason == NULL))
    {
        osint_action_free(action);
        return NULL;
    }
    return action;
}

OsintActionCatalog *osint_action_catalog_new_defaults(void)
{
    OsintActionCatalog *catalog = g_try_new0(OsintActionCatalog, 1);
    OsintAction *demonstration_action = NULL;
    OsintAction *dns_action = NULL;
    if (catalog == NULL) return NULL;
    catalog->actions = g_ptr_array_new_with_free_func(osint_action_free);
    demonstration_action = osint_action_new(
        "selection-preview", "Aperçu de la sélection",
        "Action locale de démonstration sans exécution externe.",
        OSINT_SELECTION_CONTEXT_KIND_UNKNOWN, NULL, NULL, TRUE, NULL
    );
    dns_action = osint_action_new(
        "dns-preview", "Résolution DNS",
        "Préfiguration de l'adaptateur DNS.",
        OSINT_SELECTION_CONTEXT_KIND_ENTITY, "domain_name", "dns.dig", FALSE,
        "L'outil requis n'a pas encore été vérifié."
    );
    if (catalog->actions == NULL || demonstration_action == NULL ||
        dns_action == NULL)
    {
        osint_action_free(demonstration_action);
        osint_action_free(dns_action);
        osint_action_catalog_free(catalog);
        return NULL;
    }
    g_ptr_array_add(catalog->actions, demonstration_action);
    g_ptr_array_add(catalog->actions, dns_action);
    return catalog;
}

/** @brief Compare deux actions par libellé. */
static gint osint_action_compare(gconstpointer first, gconstpointer second)
{
    const OsintAction *const *a = first;
    const OsintAction *const *b = second;
    return g_strcmp0((*a)->label, (*b)->label);
}

GPtrArray *osint_action_catalog_list_compatible(
    const OsintActionCatalog *catalog,
    const OsintSelectionContext *context
)
{
    GPtrArray *compatible_actions = NULL;
    guint index = 0;
    if (catalog == NULL || context == NULL || catalog->actions == NULL)
        return NULL;
    compatible_actions = g_ptr_array_new();
    if (compatible_actions == NULL) return NULL;
    for (index = 0; index < catalog->actions->len; index++)
    {
        OsintAction *action = g_ptr_array_index(catalog->actions, index);
        gboolean kind_matches = action->target_kind ==
            OSINT_SELECTION_CONTEXT_KIND_UNKNOWN ||
            action->target_kind == osint_selection_context_get_kind(context);
        gboolean type_matches = action->compatible_type == NULL ||
            g_strcmp0(action->compatible_type,
                osint_selection_context_get_type(context)) == 0;
        if (kind_matches && type_matches)
            g_ptr_array_add(compatible_actions, action);
    }
    g_ptr_array_sort(compatible_actions, osint_action_compare);
    return compatible_actions;
}

void osint_action_catalog_update_tool_state(
    OsintActionCatalog *catalog,
    const char *tool_identifier,
    OsintActionToolState state,
    const char *version
)
{
    guint index = 0;
    if (catalog == NULL || catalog->actions == NULL ||
        tool_identifier == NULL || tool_identifier[0] == '\0') return;
    for (index = 0; index < catalog->actions->len; index++)
    {
        OsintAction *action = g_ptr_array_index(catalog->actions, index);
        if (g_strcmp0(action->required_tool_identifier, tool_identifier) != 0)
            continue;
        g_clear_pointer(&action->tool_version, g_free);
        g_clear_pointer(&action->unavailable_reason, g_free);
        action->available = state == OSINT_ACTION_TOOL_STATE_AVAILABLE;
        if (action->available)
            action->tool_version = g_strdup(version);
        else if (state == OSINT_ACTION_TOOL_STATE_MISSING)
            action->unavailable_reason = g_strdup("L'outil requis est absent.");
        else if (state == OSINT_ACTION_TOOL_STATE_INCOMPATIBLE)
            action->unavailable_reason = g_strdup("La version installée est incompatible.");
        else
            action->unavailable_reason = g_strdup("L'outil requis n'a pas encore été vérifié.");
    }
}

void osint_action_catalog_free(OsintActionCatalog *catalog)
{
    if (catalog == NULL) return;
    g_clear_pointer(&catalog->actions, g_ptr_array_unref);
    g_free(catalog);
}

const char *osint_action_get_identifier(const OsintAction *action)
{ return action != NULL ? action->identifier : NULL; }
const char *osint_action_get_label(const OsintAction *action)
{ return action != NULL ? action->label : NULL; }
const char *osint_action_get_description(const OsintAction *action)
{ return action != NULL ? action->description : NULL; }
const char *osint_action_get_required_tool_identifier(const OsintAction *action)
{ return action != NULL ? action->required_tool_identifier : NULL; }
gboolean osint_action_is_available(const OsintAction *action)
{ return action != NULL && action->available; }
const char *osint_action_get_unavailable_reason(const OsintAction *action)
{ return action != NULL ? action->unavailable_reason : NULL; }
const char *osint_action_get_tool_version(const OsintAction *action)
{ return action != NULL ? action->tool_version : NULL; }
