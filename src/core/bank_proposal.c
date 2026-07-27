/******************************************************************************
 * @file bank_proposal.c
 * @brief Détection, normalisation et modèle de proposition bancaire (IBAN, RIB, BIC).
 ******************************************************************************/
#include "core/bank_proposal.h"
#include "core/iban_analyzer.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void bank_proposal_free(BankProposal *p)
{
    if (p == NULL)
        return;
    g_free(p->id);
    g_free(p->raw_iban);
    g_free(p->normalized_iban);
    g_free(p->raw_bic);
    g_free(p->bic);
    g_free(p->holder_name);
    g_free(p->bank_name);
    g_free(p->bank_address);
    g_free(p->country_code);
    g_free(p->bank_code);
    g_free(p->branch_code);
    g_free(p->account_number);
    g_free(p->rib_key);
    g_free(p->iban_validation);
    g_free(p->suggested_ocr_fix);
    g_free(p->verification_status);
    g_free(p->provenance_kind);
    g_free(p->evidence_id);
    g_free(p->extraction_id);
    g_free(p->created_at);
    g_free(p->updated_at);
    g_free(p);
}

static char *bank_proposal_collapse_spaces(const char *value)
{
    GString *result = NULL;
    gboolean previous_was_space = FALSE;

    if (value == NULL)
        return NULL;

    result = g_string_new(NULL);
    for (const char *cursor = value; *cursor != '\0'; cursor++)
    {
        if (g_ascii_isspace(*cursor))
        {
            if (!previous_was_space)
                g_string_append_c(result, ' ');
            previous_was_space = TRUE;
        }
        else
        {
            g_string_append_c(result, *cursor);
            previous_was_space = FALSE;
        }
    }
    g_strstrip(result->str);
    return g_string_free(result, FALSE);
}

static char *bank_proposal_extract_label(
    const char *text,
    const char *labels_pattern
)
{
    char *pattern = g_strdup_printf(
        "(?im)^(?:%s)[ \\t]*:[ \\t]*(.+)$",
        labels_pattern
    );
    GRegex *regex = g_regex_new(pattern, G_REGEX_OPTIMIZE, 0, NULL);
    GMatchInfo *match = NULL;
    char *raw_value = NULL;
    char *value = NULL;

    g_free(pattern);
    g_regex_match(regex, text, 0, &match);
    if (g_match_info_matches(match))
        raw_value = g_match_info_fetch(match, 1);
    value = bank_proposal_collapse_spaces(raw_value);
    g_free(raw_value);
    g_match_info_free(match);
    g_regex_unref(regex);
    return value;
}

static void bank_proposal_extract_bic(
    BankProposal *proposal,
    const char *text
)
{
    GRegex *regex = g_regex_new(
        "(?i)\\b[A-Z]{6}[A-Z0-9]{2}(?:[A-Z0-9]{3})?\\b",
        G_REGEX_OPTIMIZE,
        0,
        NULL
    );
    GMatchInfo *match = NULL;

    g_regex_match(regex, text, 0, &match);
    while (g_match_info_matches(match))
    {
        char *candidate = g_match_info_fetch(match, 0);
        char *normalized = g_ascii_strup(candidate, -1);

        if (bank_proposal_validate_bic(normalized))
        {
            proposal->raw_bic = candidate;
            proposal->bic = normalized;
            break;
        }
        g_free(candidate);
        g_free(normalized);
        if (!g_match_info_next(match, NULL))
            break;
    }
    g_match_info_free(match);
    g_regex_unref(regex);
}

gboolean bank_proposal_validate_iban(const char *iban)
{
    static const struct
    {
        const char *country_code;
        gsize length;
    } national_lengths[] = {
        { "BE", 16 }, { "DE", 22 }, { "ES", 24 }, { "FR", 27 },
        { "GB", 22 }, { "IT", 27 }, { "LU", 20 }, { "NL", 18 },
        { "PT", 25 }
    };
    char *normalized = iban_analyzer_normalize(iban);

    if (normalized == NULL)
        return FALSE;

    gsize len = strlen(normalized);
    if (len < 15 || len > 34)
    {
        g_free(normalized);
        return FALSE;
    }

    /* Vérification des 2 premières lettres (Code pays) */
    if (!g_ascii_isalpha(normalized[0]) ||
        !g_ascii_isalpha(normalized[1]) ||
        !g_ascii_isdigit(normalized[2]) ||
        !g_ascii_isdigit(normalized[3]))
    {
        g_free(normalized);
        return FALSE;
    }

    for (guint index = 0; index < G_N_ELEMENTS(national_lengths); index++)
    {
        if (g_ascii_strncasecmp(
                normalized,
                national_lengths[index].country_code,
                2
            ) == 0 &&
            len != national_lengths[index].length)
        {
            g_free(normalized);
            return FALSE;
        }
    }

    /* Repositionnement des 4 premiers caractères à la fin */
    GString *rearranged = g_string_new(normalized + 4);
    g_string_append_len(rearranged, normalized, 4);
    g_free(normalized);

    /* Conversion des lettres en chiffres (A=10, Z=35) */
    GString *numeric = g_string_new(NULL);
    for (gsize i = 0; i < rearranged->len; i++)
    {
        char c = rearranged->str[i];
        if (g_ascii_isalpha(c))
        {
            int val = g_ascii_toupper(c) - 'A' + 10;
            g_string_append_printf(numeric, "%d", val);
        }
        else if (g_ascii_isdigit(c))
        {
            g_string_append_c(numeric, c);
        }
        else
        {
            g_string_free(numeric, TRUE);
            g_string_free(rearranged, TRUE);
            return FALSE;
        }
    }

    g_string_free(rearranged, TRUE);

    /* Calcul du modulo 97 par blocs */
    guint remainder = 0;
    for (gsize i = 0; i < numeric->len; i++)
    {
        int digit = numeric->str[i] - '0';
        remainder = (remainder * 10 + (guint)digit) % 97;
    }

    g_string_free(numeric, TRUE);

    return (remainder == 1);


}

gboolean bank_proposal_validate_bic(const char *bic)
{
    if (bic == NULL)
        return FALSE;

    gsize len = strlen(bic);
    if (len != 8 && len != 11)
        return FALSE;

    for (gsize i = 0; i < 6; i++)
    {
        if (!g_ascii_isalpha(bic[i]))
            return FALSE;
    }
    for (gsize i = 6; i < len; i++)
    {
        if (!g_ascii_isalnum(bic[i]))
            return FALSE;
    }

    return TRUE;
}

gboolean bank_proposal_derive_french_rib(BankProposal *proposal)
{
    if (proposal == NULL || proposal->normalized_iban == NULL)
        return FALSE;

    /* Vérification d'un IBAN français : "FR76..." (27 caractères) */
    if (strlen(proposal->normalized_iban) != 27 ||
        g_ascii_strncasecmp(proposal->normalized_iban, "FR", 2) != 0)
    {
        return FALSE;
    }

    if (!proposal->is_iban_valid)
        return FALSE;

    proposal->bank_code = g_strndup(proposal->normalized_iban + 4, 5);
    proposal->branch_code = g_strndup(proposal->normalized_iban + 9, 5);
    proposal->account_number = g_strndup(proposal->normalized_iban + 14, 11);
    proposal->rib_key = g_strndup(proposal->normalized_iban + 25, 2);
    proposal->is_derived_bban = TRUE;

    return TRUE;
}

BankProposal *bank_proposal_analyze_text(const char *raw_text, const char *evidence_id)
{
    GRegex *iban_regex = NULL;
    GMatchInfo *iban_match = NULL;
    char *raw_iban = NULL;
    char *normalized_iban = NULL;

    if (raw_text == NULL || raw_text[0] == '\0')
        return NULL;

    iban_regex = g_regex_new(
        "(?i)\\b[A-Z]{2}[0-9]{2}(?:[ \\t-]*[A-Z0-9]){11,30}\\b",
        G_REGEX_OPTIMIZE,
        0,
        NULL
    );
    g_regex_match(iban_regex, raw_text, 0, &iban_match);
    if (g_match_info_matches(iban_match))
        raw_iban = g_match_info_fetch(iban_match, 0);
    g_match_info_free(iban_match);
    g_regex_unref(iban_regex);

    normalized_iban = iban_analyzer_normalize(raw_iban);
    if (normalized_iban == NULL)
    {
        g_free(raw_iban);
        return NULL;
    }

    BankProposal *p = g_new0(BankProposal, 1);
    p->id = g_uuid_string_random();
    p->raw_iban = raw_iban;
    p->normalized_iban = normalized_iban;
    p->country_code = g_strndup(p->normalized_iban, 2);
    p->is_iban_valid = bank_proposal_validate_iban(p->normalized_iban);
    p->iban_validation = g_strdup(
        p->is_iban_valid ? "valid" : "invalid"
    );
    p->verification_status = g_strdup("proposed");
    p->provenance_kind = g_strdup("ocr");
    p->evidence_id = evidence_id != NULL ? g_strdup(evidence_id) : NULL;

    /* Horodatage UTC courant */
    GDateTime *now = g_date_time_new_now_utc();
    p->created_at = g_date_time_format(now, "%Y-%m-%dT%H:%M:%SZ");
    p->updated_at = g_strdup(p->created_at);
    g_date_time_unref(now);

    /* Dérivation RIB si IBAN français */
    if (g_ascii_strcasecmp(p->country_code, "FR") == 0)
    {
        bank_proposal_derive_french_rib(p);
    }

    bank_proposal_extract_bic(p, raw_text);
    p->holder_name = bank_proposal_extract_label(
        raw_text,
        "Titulaire|Account holder"
    );
    p->bank_name = bank_proposal_extract_label(
        raw_text,
        "Banque|Bank"
    );
    p->bank_address = bank_proposal_extract_label(
        raw_text,
        "Adresse(?: de la banque)?|Bank address"
    );

    if (!p->is_iban_valid &&
        (strchr(p->normalized_iban, 'O') != NULL ||
         strchr(p->normalized_iban, 'I') != NULL))
    {
        char *suggestion = g_strdup(p->normalized_iban);
        for (char *cursor = suggestion; *cursor != '\0'; cursor++)
        {
            if (*cursor == 'O')
                *cursor = '0';
            else if (*cursor == 'I')
                *cursor = '1';
        }
        if (bank_proposal_validate_iban(suggestion))
            p->suggested_ocr_fix = suggestion;
        else
            g_free(suggestion);
    }

    return p;
}
