/******************************************************************************
 * @file osint_dns_proposal.c
 * @brief Propositions structurées extraites d'une sortie DNS brute.
 ******************************************************************************/

#include "models/osint_dns_proposal.h"
#include "models/osint_dns_query.h"

#include <gio/gio.h>
#include <string.h>

struct OsintDnsProposal
{
    char *owner;
    char *record_type;
    char *value;
    char *target;
};

/** @brief Libère une proposition possédée par un tableau. */
static void osint_dns_proposal_free(gpointer data)
{
    OsintDnsProposal *proposal = data;
    if (proposal == NULL) return;
    g_free(proposal->target);
    g_free(proposal->value);
    g_free(proposal->record_type);
    g_free(proposal->owner);
    g_free(proposal);
}

/** @brief Indique si un type DNS produit une proposition structurée. */
static gboolean osint_dns_proposal_type_is_recognized(const char *record_type)
{
    static const char *recognized_types[] = {
        "A", "AAAA", "CAA", "CNAME", "MX", "NS", "PTR", "SOA", "SRV",
        "TXT", NULL
    };

    for (gsize index = 0; recognized_types[index] != NULL; index++)
    {
        if (g_strcmp0(record_type, recognized_types[index]) == 0) return TRUE;
    }
    return FALSE;
}

/** @brief Extrait le prochain champ séparé par des espaces ASCII. */
static char *osint_dns_proposal_take_field(char **cursor)
{
    char *field_start = NULL;
    char *field_end = NULL;

    if (cursor == NULL || *cursor == NULL) return NULL;
    field_start = *cursor;
    while (*field_start != '\0' && g_ascii_isspace(*field_start)) field_start++;
    if (*field_start == '\0')
    {
        *cursor = field_start;
        return NULL;
    }
    field_end = field_start;
    while (*field_end != '\0' && !g_ascii_isspace(*field_end)) field_end++;
    *cursor = field_end;
    return g_strndup(field_start, (gsize) (field_end - field_start));
}

/** @brief Parse une ligne de réponse DNS, ou retourne NULL. */
static OsintDnsProposal *osint_dns_proposal_parse_line(
    const char *target_value,
    const char *line
)
{
    char *line_copy = g_strdup(line);
    char *cursor = line_copy;
    char *owner = NULL;
    char *time_to_live = NULL;
    char *record_class = NULL;
    char *record_type = NULL;
    char *value = NULL;
    OsintDnsProposal *proposal = NULL;

    if (line_copy == NULL) return NULL;
    g_strstrip(line_copy);
    if (line_copy[0] == '\0' || line_copy[0] == ';') goto cleanup;

    owner = osint_dns_proposal_take_field(&cursor);
    time_to_live = osint_dns_proposal_take_field(&cursor);
    record_class = osint_dns_proposal_take_field(&cursor);
    record_type = osint_dns_proposal_take_field(&cursor);
    while (cursor != NULL && *cursor != '\0' && g_ascii_isspace(*cursor)) cursor++;
    value = cursor != NULL ? g_strdup(cursor) : NULL;
    if (value != NULL) g_strstrip(value);

    if (owner == NULL || time_to_live == NULL || record_class == NULL ||
        record_type == NULL || value == NULL || value[0] == '\0' ||
        !g_ascii_string_to_unsigned(time_to_live, 10, 0, G_MAXUINT32, NULL, NULL) ||
        g_strcmp0(record_class, "IN") != 0 ||
        !osint_dns_proposal_type_is_recognized(record_type))
    {
        goto cleanup;
    }

    proposal = g_try_new0(OsintDnsProposal, 1);
    if (proposal != NULL)
    {
        proposal->owner = g_strdup(owner);
        proposal->record_type = g_strdup(record_type);
        proposal->value = g_strdup(value);
        proposal->target = g_strdup(target_value);
    }
    if (proposal == NULL || proposal->owner == NULL ||
        proposal->record_type == NULL || proposal->value == NULL ||
        proposal->target == NULL)
    {
        osint_dns_proposal_free(proposal);
        proposal = NULL;
    }

cleanup:
    g_free(value);
    g_free(record_type);
    g_free(record_class);
    g_free(time_to_live);
    g_free(owner);
    g_free(line_copy);
    return proposal;
}

GPtrArray *osint_dns_proposal_parse(
    const char *target_value,
    const char *raw_output
)
{
    GPtrArray *proposals = NULL;
    char **lines = NULL;

    if (target_value == NULL || target_value[0] == '\0' || raw_output == NULL)
        return NULL;
    proposals = g_ptr_array_new_with_free_func(osint_dns_proposal_free);
    lines = g_strsplit(raw_output, "\n", -1);
    for (gsize index = 0; lines != NULL && lines[index] != NULL; index++)
    {
        OsintDnsProposal *proposal = osint_dns_proposal_parse_line(
            target_value, lines[index]
        );
        if (proposal != NULL) g_ptr_array_add(proposals, proposal);
    }
    g_strfreev(lines);
    return proposals;
}

const char *osint_dns_proposal_get_owner(const OsintDnsProposal *proposal)
{ return proposal != NULL ? proposal->owner : NULL; }
const char *osint_dns_proposal_get_record_type(const OsintDnsProposal *proposal)
{ return proposal != NULL ? proposal->record_type : NULL; }
const char *osint_dns_proposal_get_value(const OsintDnsProposal *proposal)
{ return proposal != NULL ? proposal->value : NULL; }
const char *osint_dns_proposal_get_target(const OsintDnsProposal *proposal)
{ return proposal != NULL ? proposal->target : NULL; }

const char *osint_dns_proposal_get_entity_type(
    const OsintDnsProposal *proposal
)
{
    const char *record_type = osint_dns_proposal_get_record_type(proposal);
    if (g_strcmp0(record_type, "A") == 0 ||
        g_strcmp0(record_type, "AAAA") == 0)
    {
        return "ip_address";
    }
    if (g_strcmp0(record_type, "CNAME") == 0 ||
        g_strcmp0(record_type, "NS") == 0 ||
        g_strcmp0(record_type, "PTR") == 0)
    {
        return "domain_name";
    }
    return NULL;
}

char *osint_dns_proposal_dup_normalized_value(
    const OsintDnsProposal *proposal
)
{
    const char *entity_type = osint_dns_proposal_get_entity_type(proposal);
    const char *value = osint_dns_proposal_get_value(proposal);

    if (entity_type == NULL || value == NULL) return NULL;
    if (g_strcmp0(entity_type, "ip_address") == 0)
    {
        GInetAddress *address = g_inet_address_new_from_string(value);
        char *normalized_value = NULL;
        if (address == NULL) return NULL;
        normalized_value = g_inet_address_to_string(address);
        g_object_unref(address);
        return normalized_value;
    }
    if (g_strcmp0(entity_type, "domain_name") == 0)
    {
        char *normalized_value = g_ascii_strdown(value, -1);
        gsize value_length = normalized_value != NULL
            ? strlen(normalized_value) : 0U;
        if (value_length > 0U && normalized_value[value_length - 1U] == '.')
            normalized_value[value_length - 1U] = '\0';
        if (!osint_dns_query_is_valid_target(normalized_value))
        {
            g_free(normalized_value);
            return NULL;
        }
        return normalized_value;
    }
    return NULL;
}
