/******************************************************************************
 * @file eml_analyzer.c
 * @brief Analyse locale et non destructive des en-têtes d'un fichier EML.
 ******************************************************************************/
#include "core/eml_analyzer.h"
#include <string.h>
#define EML_ANALYZER_MAX_FILE_SIZE (25U * 1024U * 1024U)
#define EML_ANALYZER_MAX_HEADER_SIZE (2U * 1024U * 1024U)
struct EmlAnalysis
{
    GHashTable *headers;
    GPtrArray *emails;
    GPtrArray *domains;
    GPtrArray *ips;
    GPtrArray *sender_ips;
    GPtrArray *destination_ips;
    char *raw_headers;
};
/** @brief Libère un tableau de valeurs d'en-tête. */
static void eml_analyzer_values_free(gpointer data)
{
    g_ptr_array_unref(data);
}
/** @brief Ajoute une chaîne normalisée si elle n'existe pas déjà. */
static void eml_analyzer_add_unique(GPtrArray *values, const char *value,
    gboolean lowercase)
{
    char *copy = value != NULL ? g_strdup(value) : NULL;
    if (copy == NULL) return;
    g_strstrip(copy);
    if (lowercase)
    {
        char *lower = g_utf8_strdown(copy, -1); g_free(copy); copy = lower;
    }
    if (copy == NULL || copy[0] == '\0') { g_free(copy); return; }
    for (guint i = 0; i < values->len; i++)
        if (g_ascii_strcasecmp(g_ptr_array_index(values, i), copy) == 0)
        { g_free(copy); return; }
    g_ptr_array_add(values, copy);
}
/** @brief Extrait toutes les occurrences reconnues par une expression. */
static void eml_analyzer_extract_regex(GRegex *regex, const char *text,
    GPtrArray *values, gboolean lowercase)
{
    GMatchInfo *matches = NULL;
    if (regex == NULL || text == NULL) return;
    g_regex_match(regex, text, 0, &matches);
    while (g_match_info_matches(matches))
    {
        char *match = g_match_info_fetch(matches, 1);
        eml_analyzer_add_unique(values, match, lowercase);
        g_free(match);
        if (!g_match_info_next(matches, NULL)) break;
    }
    g_match_info_free(matches);
}
/** @brief Extrait les IP d'une portion nommée d'un en-tête Received. */
static void eml_analyzer_extract_received_part(GRegex *part_regex,
    GRegex *ip_regex, const char *received, GPtrArray *values)
{
    GMatchInfo *match = NULL;
    char *part = NULL;

    if (part_regex == NULL || ip_regex == NULL || received == NULL)
        return;
    if (g_regex_match(part_regex, received, 0, &match))
    {
        part = g_match_info_fetch(match, 1);
        eml_analyzer_extract_regex(ip_regex, part, values, FALSE);
        g_free(part);
    }
    g_match_info_free(match);
}
/** @brief Ajoute un en-tête déplié au résultat. */
static gboolean eml_analyzer_add_header(EmlAnalysis *analysis,
    const char *name, const char *value)
{
    GPtrArray *values = NULL;
    char *key = NULL, *safe_value = NULL;
    if (analysis == NULL || name == NULL || value == NULL) return FALSE;
    key = g_ascii_strdown(name, -1);
    safe_value = g_utf8_make_valid(value, -1);
    if (key == NULL || safe_value == NULL) { g_free(key); g_free(safe_value); return FALSE; }
    g_strstrip(key); g_strstrip(safe_value);
    values = g_hash_table_lookup(analysis->headers, key);
    if (values == NULL)
    {
        values = g_ptr_array_new_with_free_func(g_free);
        g_hash_table_insert(analysis->headers, key, values); key = NULL;
    }
    g_ptr_array_add(values, safe_value); g_free(key); return TRUE;
}
EmlAnalysis *eml_analyzer_analyze_file(const char *file_path, GError **error)
{
    EmlAnalysis *analysis = NULL;
    GError *local_error = NULL;
    GMappedFile *mapped = NULL;
    const char *data = NULL, *separator = NULL;
    gsize size = 0, header_size = 0;
    char *utf8 = NULL, **lines = NULL, *current_name = NULL;
    GString *current_value = NULL;
    GRegex *email_regex = NULL, *domain_regex = NULL, *ip_regex = NULL;
    GRegex *received_from_regex = NULL, *received_by_regex = NULL;
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);
    if (file_path == NULL || file_path[0] == '\0')
    { g_set_error_literal(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
        "Le chemin du fichier EML est invalide."); return NULL; }
    mapped = g_mapped_file_new(file_path, FALSE, &local_error);
    if (mapped == NULL) { g_propagate_error(error, local_error); return NULL; }
    size = g_mapped_file_get_length(mapped); data = g_mapped_file_get_contents(mapped);
    if (size == 0 || size > EML_ANALYZER_MAX_FILE_SIZE)
    { g_set_error_literal(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
        "Le fichier EML est vide ou dépasse la limite de 25 Mio."); goto cleanup; }
    separator = g_strstr_len(data, (gssize) size, "\r\n\r\n");
    header_size = separator != NULL ? (gsize) (separator - data) : 0;
    if (separator == NULL)
    { separator = g_strstr_len(data, (gssize) size, "\n\n");
      header_size = separator != NULL ? (gsize) (separator - data) : 0; }
    if (separator == NULL || header_size == 0 || header_size > EML_ANALYZER_MAX_HEADER_SIZE)
    { g_set_error_literal(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
        "Le fichier ne contient pas un bloc d'en-têtes EML valide."); goto cleanup; }
    analysis = g_new0(EmlAnalysis, 1);
    analysis->headers = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
        eml_analyzer_values_free);
    analysis->emails = g_ptr_array_new_with_free_func(g_free);
    analysis->domains = g_ptr_array_new_with_free_func(g_free);
    analysis->ips = g_ptr_array_new_with_free_func(g_free);
    analysis->sender_ips = g_ptr_array_new_with_free_func(g_free);
    analysis->destination_ips = g_ptr_array_new_with_free_func(g_free);
    analysis->raw_headers = g_utf8_make_valid(data, (gssize) header_size);
    utf8 = g_strdup(analysis->raw_headers); lines = g_strsplit(utf8, "\n", -1);
    current_value = g_string_new(NULL);
    for (guint i = 0; lines[i] != NULL; i++)
    {
        char *line = lines[i]; g_strchomp(line);
        if ((line[0] == ' ' || line[0] == '\t') && current_name != NULL)
        { g_string_append_c(current_value, ' '); g_string_append(current_value, g_strstrip(line)); continue; }
        if (current_name != NULL)
        { eml_analyzer_add_header(analysis, current_name, current_value->str);
          g_clear_pointer(&current_name, g_free); g_string_truncate(current_value, 0); }
        char *colon = strchr(line, ':');
        if (colon == NULL || colon == line) continue;
        current_name = g_strndup(line, (gsize) (colon - line));
        g_string_assign(current_value, g_strstrip(colon + 1));
    }
    if (current_name != NULL) eml_analyzer_add_header(analysis,
        current_name, current_value->str);
    email_regex = g_regex_new("([A-Za-z0-9.!#$%&'*+/=?^_`{|}~-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,})", 0, 0, NULL);
    domain_regex = g_regex_new("(?:@|[.\\s<([])([A-Za-z0-9-]+(?:\\.[A-Za-z0-9-]+)+)", 0, 0, NULL);
    ip_regex = g_regex_new("(?:^|[^0-9])([0-9]{1,3}(?:\\.[0-9]{1,3}){3})(?:[^0-9]|$)", 0, 0, NULL);
    received_from_regex = g_regex_new("(?i)\\bfrom\\b(.*?)\\bby\\b", 0, 0, NULL);
    received_by_regex = g_regex_new("(?i)\\bby\\b(.*?)(?:\\bwith\\b|;|$)", 0, 0, NULL);
    eml_analyzer_extract_regex(email_regex, analysis->raw_headers, analysis->emails, TRUE);
    eml_analyzer_extract_regex(domain_regex, analysis->raw_headers, analysis->domains, TRUE);
    eml_analyzer_extract_regex(ip_regex, analysis->raw_headers, analysis->ips, FALSE);
    const GPtrArray *received_values = eml_analysis_get_header_values(
        analysis, "received");
    for (guint i = 0; received_values != NULL && i < received_values->len; i++)
    {
        const char *received = g_ptr_array_index(
            (GPtrArray *) received_values, i);
        eml_analyzer_extract_received_part(received_from_regex, ip_regex,
            received, analysis->sender_ips);
        eml_analyzer_extract_received_part(received_by_regex, ip_regex,
            received, analysis->destination_ips);
    }
cleanup:
    g_clear_pointer(&email_regex, g_regex_unref); g_clear_pointer(&domain_regex, g_regex_unref);
    g_clear_pointer(&ip_regex, g_regex_unref); g_clear_pointer(&current_name, g_free);
    g_clear_pointer(&received_from_regex, g_regex_unref);
    g_clear_pointer(&received_by_regex, g_regex_unref);
    if (current_value != NULL) g_string_free(current_value, TRUE);
    g_strfreev(lines); g_free(utf8); g_mapped_file_unref(mapped);
    return analysis;
}
void eml_analysis_free(EmlAnalysis *analysis)
{
    if (analysis == NULL) return;
    g_hash_table_unref(analysis->headers); g_ptr_array_unref(analysis->emails);
    g_ptr_array_unref(analysis->domains); g_ptr_array_unref(analysis->ips);
    g_ptr_array_unref(analysis->sender_ips);
    g_ptr_array_unref(analysis->destination_ips);
    g_free(analysis->raw_headers); g_free(analysis);
}
const GPtrArray *eml_analysis_get_header_values(const EmlAnalysis *analysis,
    const char *name)
{
    char *key = NULL; GPtrArray *values = NULL;
    if (analysis == NULL || name == NULL) return NULL;
    key = g_ascii_strdown(name, -1); values = g_hash_table_lookup(analysis->headers, key);
    g_free(key); return values;
}
const char *eml_analysis_get_first_header(const EmlAnalysis *analysis,
    const char *name)
{
    const GPtrArray *values = eml_analysis_get_header_values(analysis, name);
    return values != NULL && values->len > 0 ? g_ptr_array_index((GPtrArray *) values, 0) : NULL;
}
const GPtrArray *eml_analysis_get_email_addresses(const EmlAnalysis *a) { return a != NULL ? a->emails : NULL; }
const GPtrArray *eml_analysis_get_domains(const EmlAnalysis *a) { return a != NULL ? a->domains : NULL; }
const GPtrArray *eml_analysis_get_ip_addresses(const EmlAnalysis *a) { return a != NULL ? a->ips : NULL; }
const GPtrArray *eml_analysis_get_sender_ip_addresses(const EmlAnalysis *a) { return a != NULL ? a->sender_ips : NULL; }
const GPtrArray *eml_analysis_get_destination_ip_addresses(const EmlAnalysis *a) { return a != NULL ? a->destination_ips : NULL; }
const char *eml_analysis_get_raw_headers(const EmlAnalysis *a) { return a != NULL ? a->raw_headers : NULL; }
