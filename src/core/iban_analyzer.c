/******************************************************************************
 * @file iban_analyzer.c
 * @brief Normalisation, validation et extraction locale d'IBAN.
 ******************************************************************************/
#include "core/iban_analyzer.h"
#include <string.h>
char *iban_analyzer_normalize_canonical(const char *text)
{
    if(text==NULL||strlen(text)>(1024U * 1024U))return NULL;
    GString *r=g_string_new(NULL);
    for(const unsigned char *p=(const unsigned char *)text;*p;p++) {
        if(*p==' '||*p=='\t'||*p=='\r'||*p=='\n'||*p=='-')continue;
        if(!g_ascii_isalnum(*p)){g_string_free(r,TRUE);return NULL;}
        g_string_append_c(r,g_ascii_toupper(*p));
    }
    if(r->len==0){g_string_free(r,TRUE);return NULL;}return g_string_free(r,FALSE);
}
char *iban_analyzer_normalize(const char *text)
{
    GString *result = NULL;
    if (text == NULL) return NULL;
    result = g_string_new(NULL);
    for (const char *cursor = text; *cursor != '\0'; cursor++)
        if (g_ascii_isalnum(*cursor))
            g_string_append_c(result, g_ascii_toupper(*cursor));
    if (result->len == 0) { g_string_free(result, TRUE); return NULL; }
    return g_string_free(result, FALSE);
}
gboolean iban_analyzer_validate(const char *iban)
{
    char *normalized = iban_analyzer_normalize(iban);
    guint remainder = 0; gsize length = 0;
    if (normalized == NULL) return FALSE;
    length = strlen(normalized);
    if (length < 15 || length > 34 || !g_ascii_isalpha(normalized[0]) ||
        !g_ascii_isalpha(normalized[1]) || !g_ascii_isdigit(normalized[2]) ||
        !g_ascii_isdigit(normalized[3])) { g_free(normalized); return FALSE; }
    for (gsize offset = 0; offset < length; offset++)
    {
        char character = normalized[(offset + 4) % length];
        if (g_ascii_isdigit(character))
            remainder = (remainder * 10 + (guint)(character - '0')) % 97;
        else if (g_ascii_isalpha(character))
        {
            guint value = (guint)(character - 'A') + 10;
            remainder = (remainder * 10 + value / 10) % 97;
            remainder = (remainder * 10 + value % 10) % 97;
        }
        else { g_free(normalized); return FALSE; }
    }
    g_free(normalized); return remainder == 1;
}
IbanAnalyzerResult iban_analyzer_validate_canonical_result(const char *iban)
{
    char *normalized=iban_analyzer_normalize_canonical(iban);
    static const struct { const char *country; gsize length; } lengths[]={
        {"BE",16},{"DE",22},{"ES",24},{"FR",27},{"GB",22},
        {"IT",27},{"LU",20},{"NL",18},{"PT",25}};
    if(normalized==NULL||strlen(normalized)<15||strlen(normalized)>34||
       !g_ascii_isalpha(normalized[0])||!g_ascii_isalpha(normalized[1])||
       !g_ascii_isdigit(normalized[2])||!g_ascii_isdigit(normalized[3])) {
        g_free(normalized);return IBAN_ANALYZER_RESULT_INVALID;
    }
    for(guint i=0;i<G_N_ELEMENTS(lengths);i++) {
        if(g_str_has_prefix(normalized,lengths[i].country)) {
            gboolean valid=strlen(normalized)==lengths[i].length&&
                iban_analyzer_validate(normalized);
            g_free(normalized);return valid?IBAN_ANALYZER_RESULT_VALID:
                IBAN_ANALYZER_RESULT_INVALID;
        }
    }
    g_free(normalized);return IBAN_ANALYZER_RESULT_UNSUPPORTED_COUNTRY;
}
gboolean iban_analyzer_validate_canonical(const char *iban)
{
    return iban_analyzer_validate_canonical_result(iban)==
        IBAN_ANALYZER_RESULT_VALID;
}
GPtrArray *iban_analyzer_extract(const char *text)
{
    GPtrArray *results = g_ptr_array_new_with_free_func(g_free);
    GRegex *regex = NULL; GMatchInfo *matches = NULL; GError *error = NULL;
    if (text == NULL) return results;
    regex = g_regex_new("(?i)\\b(?:FR[ \\t-]*[0-9]{2}(?:[ \\t-]*[A-Z0-9]){23}|[A-Z]{2}[0-9]{2}[A-Z0-9]{11,30})\\b",
        G_REGEX_OPTIMIZE, 0, &error);
    if (regex == NULL) { g_clear_error(&error); return results; }
    g_regex_match(regex, text, 0, &matches);
    while (g_match_info_matches(matches))
    {
        char *raw = g_match_info_fetch(matches, 0);
        char *candidate = iban_analyzer_normalize(raw);
        gboolean duplicate = FALSE;
        for (guint index = 0; candidate != NULL && index < results->len; index++)
            if (g_strcmp0(candidate, g_ptr_array_index(results, index)) == 0)
                duplicate = TRUE;
        if (candidate != NULL && iban_analyzer_validate(candidate) && !duplicate)
            g_ptr_array_add(results, candidate);
        else g_free(candidate);
        g_free(raw); g_match_info_next(matches, NULL);
    }
    g_match_info_free(matches); g_regex_unref(regex); return results;
}
