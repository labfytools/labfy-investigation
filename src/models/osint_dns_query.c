/******************************************************************************
 * @file osint_dns_query.c
 * @brief Validation des cibles utilisées pour une résolution DNS OSINT.
 ******************************************************************************/

#include "models/osint_dns_query.h"

#include <string.h>

gboolean osint_dns_query_is_valid_target(
    const char *target_value
)
{
    gsize target_length = 0;
    gsize label_length = 0;

    if (target_value == NULL || target_value[0] == '\0')
    {
        return FALSE;
    }

    target_length = strlen(target_value);
    if (target_length > 253U)
    {
        return FALSE;
    }

    for (gsize character_index = 0;
         character_index < target_length;
         character_index++)
    {
        const char character = target_value[character_index];

        if (character == '.')
        {
            if (label_length == 0U)
            {
                return character_index == target_length - 1U &&
                       character_index > 0U;
            }

            if (target_value[character_index - 1U] == '-')
            {
                return FALSE;
            }

            if (character_index == target_length - 1U)
            {
                return TRUE;
            }

            label_length = 0U;
            continue;
        }

        if (!(g_ascii_isalnum(character) || character == '-'))
        {
            return FALSE;
        }

        if ((label_length == 0U && character == '-') || label_length >= 63U)
        {
            return FALSE;
        }

        label_length++;
    }

    return label_length > 0U && target_value[target_length - 1U] != '-';
}
