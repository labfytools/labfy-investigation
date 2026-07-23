/******************************************************************************
 * @file relation_type_normalizer.c
 * @brief Normalisation Unicode des types de relations.
 ******************************************************************************/

#include "core/relation_type_normalizer.h"

char *relation_type_normalize_key(const char *label)
{
    char *normalized = NULL;
    char *folded = NULL;
    GString *collapsed = NULL;
    const char *cursor = NULL;
    gboolean pending_space = FALSE;

    if (label == NULL || !g_utf8_validate(label, -1, NULL)) return NULL;
    normalized = g_utf8_normalize(label, -1, G_NORMALIZE_DEFAULT_COMPOSE);
    if (normalized == NULL) return NULL;
    collapsed = g_string_new(NULL);
    for (cursor = normalized; *cursor != '\0'; cursor = g_utf8_next_char(cursor))
    {
        gunichar character = g_utf8_get_char(cursor);
        if (g_unichar_isspace(character))
        {
            if (collapsed->len > 0U) pending_space = TRUE;
            continue;
        }
        if (pending_space)
        {
            g_string_append_c(collapsed, ' ');
            pending_space = FALSE;
        }
        g_string_append_unichar(collapsed, character);
    }
    g_free(normalized);
    if (collapsed->len == 0U)
    {
        g_string_free(collapsed, TRUE);
        return NULL;
    }
    folded = g_utf8_casefold(collapsed->str, -1);
    g_string_free(collapsed, TRUE);
    if (folded == NULL) return NULL;
    normalized = g_utf8_normalize(folded, -1, G_NORMALIZE_DEFAULT_COMPOSE);
    g_free(folded);
    return normalized;
}
