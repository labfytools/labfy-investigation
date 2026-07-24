/******************************************************************************
 * @file bank_proposal.c
 * @brief Détection, normalisation et modèle de proposition bancaire (IBAN, RIB, BIC).
 ******************************************************************************/
#include "core/bank_proposal.h"
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
    g_free(p->bic);
    g_free(p->holder_name);
    g_free(p->bank_name);
    g_free(p->bank_address);
    g_free(p->country_code);
    g_free(p->bank_code);
    g_free(p->branch_code);
    g_free(p->account_number);
    g_free(p->rib_key);
    g_free(p->suggested_ocr_fix);
    g_free(p->verification_status);
    g_free(p->provenance_kind);
    g_free(p->evidence_id);
    g_free(p->extraction_id);
    g_free(p->created_at);
    g_free(p->updated_at);
    g_free(p);
}

gboolean bank_proposal_validate_iban(const char *iban)
{
    if (iban == NULL)
        return FALSE;

    gsize len = strlen(iban);
    if (len < 15 || len > 34)
        return FALSE;

    /* Vérification des 2 premières lettres (Code pays) */
    if (!g_ascii_isalpha(iban[0]) || !g_ascii_isalpha(iban[1]))
        return FALSE;

    /* Repositionnement des 4 premiers caractères à la fin */
    GString *rearranged = g_string_new(iban + 4);
    g_string_append_len(rearranged, iban, 4);

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

    for (gsize i = 0; i < 4; i++)
    {
        if (!g_ascii_isalpha(bic[i]))
            return FALSE;
    }
    for (gsize i = 4; i < 6; i++)
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
    if (raw_text == NULL || raw_text[0] == '\0')
        return NULL;

    /* Nettoyage des espaces pour recherche d'IBAN */
    GString *clean = g_string_new(NULL);
    gsize raw_len = strlen(raw_text);
    for (gsize i = 0; i < raw_len; i++)
    {
        char c = raw_text[i];
        if (g_ascii_isalnum(c))
        {
            g_string_append_c(clean, g_ascii_toupper(c));
        }
    }

    /* Recherche de motif IBAN (ex: FR76...) */
    const char *data = clean->str;
    const char *iban_start = strstr(data, "FR");
    if (iban_start == NULL)
    {
        /* Essai avec d'autres codes pays à 2 lettres */
        if (clean->len >= 15 && g_ascii_isalpha(data[0]) && g_ascii_isalpha(data[1]))
            iban_start = data;
    }

    if (iban_start == NULL)
    {
        g_string_free(clean, TRUE);
        return NULL;
    }

    BankProposal *p = g_new0(BankProposal, 1);
    p->id = g_uuid_string_random();
    p->raw_iban = g_strdup(raw_text);
    p->normalized_iban = g_strndup(iban_start, 27 < strlen(iban_start) ? 27 : strlen(iban_start));
    p->country_code = g_strndup(p->normalized_iban, 2);
    p->is_iban_valid = bank_proposal_validate_iban(p->normalized_iban);
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

    g_string_free(clean, TRUE);
    return p;
}
