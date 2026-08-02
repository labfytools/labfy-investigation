#include "core/bank_structured_extractor.h"
#include "core/bank_text_extractor.h"
#include "core/bic_validator.h"
#include <string.h>

static GQuark analysis_error(void)
{ return g_quark_from_static_string("bank-structured-extractor-error"); }

static char *bounded_context(const char *text, gsize start, gsize end)
{
    gsize length = strlen(text), left = start > 128U ? start - 128U : 0U;
    gsize right = MIN(length, end + 128U);
    if (right - left > BANK_ANALYSIS_MAX_CONTEXT_BYTES)
        right = left + BANK_ANALYSIS_MAX_CONTEXT_BYTES;
    return g_strndup(text + left, right - left);
}

static BankOccurrence *occurrence_new(const char *text, BankOccurrenceType type,
    gsize start, gsize end, const char *normalized, BankValidationState validation,
    const char *rule, GError **error)
{
    gsize length = strlen(text);
    if (start > end || end > length || end - start > BANK_ANALYSIS_MAX_VALUE_BYTES) {
        g_set_error_literal(error, analysis_error(), 2,
            "Une valeur bancaire dépasse les limites autorisées.");
        return NULL;
    }
    BankOccurrence *o = g_new0(BankOccurrence, 1);
    o->type = type; o->raw_value = g_strndup(text + start, end - start);
    o->normalized_value = g_strdup(normalized); o->start_offset = start;
    o->end_offset = end; o->validation = validation;
    o->extraction_rule = g_strdup(rule);
    o->warnings = g_ptr_array_new_with_free_func(g_free);
    o->source_context = bounded_context(text, start, end);
    return o;
}

static gboolean add_occurrence(BankAnalysisResult *r, BankOccurrence *o)
{
    if (r->occurrences->len >= BANK_ANALYSIS_MAX_OCCURRENCES) {
        r->occurrence_limit_reached = TRUE; bank_occurrence_free(o); return FALSE;
    }
    g_ptr_array_add(r->occurrences, o); return TRUE;
}

static gint occurrence_compare(gconstpointer a, gconstpointer b)
{
    const BankOccurrence *left = *(BankOccurrence * const *)a;
    const BankOccurrence *right = *(BankOccurrence * const *)b;
    if (left->start_offset != right->start_offset)
        return left->start_offset < right->start_offset ? -1 : 1;
    if (left->end_offset != right->end_offset)
        return left->end_offset < right->end_offset ? -1 : 1;
    return (gint)left->type - (gint)right->type;
}

static gboolean overlaps_type(const BankAnalysisResult *r, BankOccurrenceType type,
    gsize start, gsize end)
{
    for (guint i = 0; i < r->occurrences->len; i++) {
        BankOccurrence *o = g_ptr_array_index(r->occurrences, i);
        if (o->type == type && o->start_offset == start && o->end_offset == end)
            return TRUE;
    }
    return FALSE;
}

static gboolean scan_regex(BankAnalysisResult *r, const char *pattern,
    BankOccurrenceType type, guint capture, BankValidationState validation,
    const char *rule, gboolean normalize_upper, GError **error)
{
    GRegex *regex = g_regex_new(pattern, G_REGEX_OPTIMIZE, 0, error);
    if (regex == NULL) return FALSE;
    gsize text_length = strlen(r->source_text);
    GMatchInfo *matches = NULL; g_regex_match(regex, r->source_text, 0, &matches);
    while (g_match_info_matches(matches)) {
        gint start = 0, end = 0; g_match_info_fetch_pos(matches, capture, &start, &end);
        if (start >= 0 && end >= start && !overlaps_type(r, type, start, end)) {
            if ((gsize)end < text_length &&
                r->source_text[end] != '\r' && r->source_text[end] != '\n') {
                g_set_error_literal(error, analysis_error(), 2,
                    "Une valeur bancaire dépasse les limites autorisées.");
                g_match_info_free(matches); g_regex_unref(regex); return FALSE;
            }
            char *raw = g_strndup(r->source_text + start, (gsize)(end - start));
            char *normalized = normalize_upper ? g_utf8_strup(raw, -1) : NULL;
            BankOccurrence *o = occurrence_new(r->source_text, type, start, end,
                normalized, validation, rule, error);
            g_free(normalized); g_free(raw);
            if (o == NULL) { g_match_info_free(matches); g_regex_unref(regex); return FALSE; }
            add_occurrence(r, o);
        }
        if (!g_match_info_next(matches, error)) break;
    }
    g_match_info_free(matches); g_regex_unref(regex);
    return error == NULL || *error == NULL;
}

typedef struct {
    BankValidationState state;
    gsize decimal_offset;
    guint digits;
} AmountGrammar;

static gboolean amount_byte(const char *text, gsize offset, gsize length)
{
    if (offset >= length) return FALSE;
    return g_ascii_isdigit(text[offset]) || text[offset] == ',' ||
        text[offset] == '.' || text[offset] == ' ' || text[offset] == '\t' ||
        ((guchar)text[offset] == 0xc2U && offset + 1U < length &&
            (guchar)text[offset + 1U] == 0xa0U);
}

static gboolean amount_has_numeric_before(const char *text, gsize offset)
{
    if (offset == 0U) return FALSE;
    gsize cursor = offset;
    while (cursor > 0U && (text[cursor - 1U] == ' ' || text[cursor - 1U] == '\t'))
        cursor--;
    if (cursor == 0U) return FALSE;
    return g_ascii_isdigit(text[cursor - 1U]) || text[cursor - 1U] == ',' ||
        text[cursor - 1U] == '.';
}

static gboolean currency_at(const char *text, gsize offset, gsize length,
    gsize *currency_length)
{
    if (offset < length && text[offset] == '$') { *currency_length = 1U; return TRUE; }
    if (offset + 3U <= length && memcmp(text + offset, "€", 3U) == 0) {
        *currency_length = 3U; return TRUE;
    }
    if (offset + 3U <= length &&
        (g_ascii_strncasecmp(text + offset, "EUR", 3U) == 0 ||
         g_ascii_strncasecmp(text + offset, "USD", 3U) == 0)) {
        *currency_length = 3U; return TRUE;
    }
    return FALSE;
}

static gboolean amount_marker_before(const char *text, gsize number_start)
{
    gsize line_start = number_start;
    while (line_start > 0U && text[line_start - 1U] != '\n' &&
        text[line_start - 1U] != '\r') line_start--;
    gsize length = number_start - line_start;
    if (length > 64U) line_start = number_start - 64U, length = 64U;
    char *prefix = g_strndup(text + line_start, length);
    gboolean found = g_regex_match_simple(
        "(?i)(montant|amount|somme|total)[ \\t]*:[ \\t]*(?:\\(|\\[)?[ \\t]*$",
        prefix, 0, 0);
    g_free(prefix); return found;
}

static gboolean currency_before(const char *text, gsize number_start,
    gsize *currency_start, gsize *currency_length)
{
    gsize cursor = number_start;
    while (cursor > 0U && (text[cursor - 1U] == ' ' || text[cursor - 1U] == '\t'))
        cursor--;
    gsize candidates[] = {3U, 1U};
    for (guint i = 0; i < G_N_ELEMENTS(candidates); i++) {
        if (cursor < candidates[i]) continue;
        gsize start = cursor - candidates[i], length = 0;
        if (currency_at(text, start, number_start, &length) && length == candidates[i]) {
            if (start > 0U && g_ascii_isalnum(text[start - 1U])) continue;
            *currency_start = start; *currency_length = length; return TRUE;
        }
    }
    return FALSE;
}

static gboolean currency_after(const char *text, gsize number_end, gsize text_length,
    gsize *currency_start, gsize *currency_length)
{
    gsize cursor = number_end;
    while (cursor < text_length && (text[cursor] == ' ' || text[cursor] == '\t'))
        cursor++;
    if (!currency_at(text, cursor, text_length, currency_length)) return FALSE;
    if (cursor + *currency_length < text_length &&
        g_ascii_isalnum(text[cursor + *currency_length])) return FALSE;
    *currency_start = cursor; return TRUE;
}

static gboolean amount_limits_ok(const char *raw, gsize length,
    guint *digits, guint *separators, guint *groups)
{
    *digits = *separators = *groups = 0;
    if (length > BANK_ANALYSIS_MAX_AMOUNT_BYTES) return FALSE;
    for (gsize i = 0; i < length; i++) {
        if (g_ascii_isdigit(raw[i])) (*digits)++;
        else if (raw[i] == ',' || raw[i] == '.') (*separators)++;
        else if (raw[i] == ' ' || raw[i] == '\t' || (guchar)raw[i] == 0xc2U) {
            (*groups)++;
            if ((guchar)raw[i] == 0xc2U && i + 1U < length &&
                (guchar)raw[i + 1U] == 0xa0U) i++;
        }
        if (*digits > BANK_ANALYSIS_MAX_AMOUNT_DIGITS ||
            *separators > BANK_ANALYSIS_MAX_AMOUNT_SEPARATORS ||
            *groups > BANK_ANALYSIS_MAX_AMOUNT_GROUPS) return FALSE;
    }
    return TRUE;
}

static gboolean grouped_integer_valid(const char *raw, gsize end, char separator)
{
    guint group_digits = 0, group_index = 0;
    for (gsize i = 0; i <= end; i++) {
        if (i < end && g_ascii_isdigit(raw[i])) { group_digits++; continue; }
        if (i < end && raw[i] != separator) return FALSE;
        if ((group_index == 0U && (group_digits == 0U || group_digits > 3U)) ||
            (group_index > 0U && group_digits != 3U)) return FALSE;
        group_index++; group_digits = 0;
    }
    return group_index > 1U;
}

static AmountGrammar analyze_amount_grammar(const char *raw, gsize length)
{
    AmountGrammar grammar = {BANK_VALIDATION_INVALID, G_MAXSIZE, 0U};
    guint commas = 0, dots = 0, spaces = 0;
    gsize last_comma = G_MAXSIZE, last_dot = G_MAXSIZE;
    for (gsize i = 0; i < length; i++) {
        if (g_ascii_isdigit(raw[i])) grammar.digits++;
        else if (raw[i] == ',') commas++, last_comma = i;
        else if (raw[i] == '.') dots++, last_dot = i;
        else if (raw[i] == ' ') spaces++;
        else return grammar; /* tabulation, espace insécable ou octet inattendu */
    }
    if (grammar.digits == 0U) return grammar;
    guint punctuations = commas + dots;
    if (spaces > 0U) {
        if (punctuations > 1U) return grammar;
        grammar.decimal_offset = punctuations == 1U ?
            (commas == 1U ? last_comma : last_dot) : G_MAXSIZE;
        gsize integer_end = grammar.decimal_offset == G_MAXSIZE ? length : grammar.decimal_offset;
        if (!grouped_integer_valid(raw, integer_end, ' ')) return grammar;
        if (grammar.decimal_offset != G_MAXSIZE) {
            gsize precision = length - grammar.decimal_offset - 1U;
            if (precision == 0U || precision > BANK_ANALYSIS_MAX_AMOUNT_PRECISION)
                return grammar;
        }
        grammar.state = BANK_VALIDATION_VALID; return grammar;
    }
    if (punctuations == 0U) {
        grammar.state = BANK_VALIDATION_VALID; return grammar;
    }
    if (punctuations == 1U) {
        grammar.decimal_offset = commas == 1U ? last_comma : last_dot;
        gsize precision = length - grammar.decimal_offset - 1U;
        if (precision == 0U || precision > BANK_ANALYSIS_MAX_AMOUNT_PRECISION)
            return grammar;
        grammar.state = precision == 3U ? BANK_VALIDATION_AMBIGUOUS :
            BANK_VALIDATION_VALID;
        if (grammar.state != BANK_VALIDATION_VALID) grammar.decimal_offset = G_MAXSIZE;
        return grammar;
    }
    if (commas > 0U && dots > 0U) {
        grammar.decimal_offset = MAX(last_comma, last_dot);
        char decimal = raw[grammar.decimal_offset];
        char thousands = decimal == ',' ? '.' : ',';
        gsize precision = length - grammar.decimal_offset - 1U;
        if (precision == 0U || precision > BANK_ANALYSIS_MAX_AMOUNT_PRECISION ||
            !grouped_integer_valid(raw, grammar.decimal_offset, thousands))
            return grammar;
        for (gsize i = 0; i < grammar.decimal_offset; i++)
            if (!g_ascii_isdigit(raw[i]) && raw[i] != thousands) return grammar;
        for (gsize i = grammar.decimal_offset + 1U; i < length; i++)
            if (!g_ascii_isdigit(raw[i])) return grammar;
        grammar.state = BANK_VALIDATION_VALID; return grammar;
    }
    char separator = commas > 0U ? ',' : '.';
    if (grouped_integer_valid(raw, length, separator)) {
        grammar.state = BANK_VALIDATION_VALID; grammar.decimal_offset = G_MAXSIZE;
    }
    return grammar;
}

static char *normalize_valid_amount(const char *raw, gsize length,
    const AmountGrammar *grammar)
{
    GString *normalized = g_string_sized_new(grammar->digits + 1U);
    for (gsize i = 0; i < length; i++) {
        if (g_ascii_isdigit(raw[i])) g_string_append_c(normalized, raw[i]);
        else if (i == grammar->decimal_offset) g_string_append_c(normalized, '.');
    }
    return g_string_free(normalized, FALSE);
}

static void signal_amount_limit(BankAnalysisResult *result)
{
    result->amount_limit_reached = TRUE;
    for (guint i = 0; i < result->warnings->len; i++)
        if (g_strcmp0(g_ptr_array_index(result->warnings, i),
                "amount-limit-reached") == 0) return;
    if (result->warnings->len < BANK_ANALYSIS_MAX_WARNINGS)
        g_ptr_array_add(result->warnings, g_strdup("amount-limit-reached"));
}

static gboolean scan_amounts(BankAnalysisResult *r, GError **error)
{
    const char *text = r->source_text; gsize text_length = strlen(text);
    for (gsize start = 0; start < text_length; start++) {
        if (!g_ascii_isdigit(text[start]) || amount_has_numeric_before(text, start))
            continue;
        gsize scan_end = start + 1U;
        while (amount_byte(text, scan_end, text_length)) {
            if ((guchar)text[scan_end] == 0xc2U) scan_end += 2U;
            else scan_end++;
        }
        gsize number_end = scan_end;
        while (number_end > start && (text[number_end - 1U] == ' ' ||
            text[number_end - 1U] == '\t')) number_end--;
        while (number_end > start && (text[number_end - 1U] == ',' ||
            text[number_end - 1U] == '.')) number_end--;
        gsize before_currency_start = 0, before_currency_length = 0;
        gsize after_currency_start = 0, after_currency_length = 0;
        gboolean has_before_currency = currency_before(text, start,
            &before_currency_start, &before_currency_length);
        gboolean has_after_currency = currency_after(text, number_end, text_length,
            &after_currency_start, &after_currency_length);
        gboolean has_context = amount_marker_before(text, start) ||
            has_before_currency || has_after_currency;
        if (!has_context) { start = scan_end - 1U; continue; }
        gsize candidate_length = number_end - start;
        if (candidate_length > BANK_ANALYSIS_MAX_AMOUNT_BYTES) {
            signal_amount_limit(r); start = scan_end - 1U; continue;
        }
        guint digits = 0, separators = 0, groups = 0;
        if (!amount_limits_ok(text + start, candidate_length,
                &digits, &separators, &groups)) {
            signal_amount_limit(r); start = scan_end - 1U; continue;
        }
        AmountGrammar grammar = analyze_amount_grammar(text + start, candidate_length);
        char *normalized = grammar.state == BANK_VALIDATION_VALID ?
            normalize_valid_amount(text + start, candidate_length, &grammar) : NULL;
        BankOccurrence *amount = occurrence_new(text, BANK_OCCURRENCE_AMOUNT,
            start, number_end, normalized, grammar.state, "amount-explicit", error);
        g_free(normalized); if (amount == NULL) return FALSE;
        add_occurrence(r, amount);
        gsize currency_start = has_after_currency ? after_currency_start : before_currency_start;
        gsize currency_length = has_after_currency ? after_currency_length : before_currency_length;
        if (has_after_currency || has_before_currency) {
            const char *currency_raw = text + currency_start;
            const char *code = currency_length == 3U &&
                memcmp(currency_raw, "€", 3U) == 0 ? "EUR" :
                (currency_length == 1U ? "USD" : NULL);
            char *upper = code != NULL ? g_strdup(code) :
                g_ascii_strup(currency_raw, currency_length);
            if (!overlaps_type(r, BANK_OCCURRENCE_CURRENCY, currency_start,
                    currency_start + currency_length)) {
                BankOccurrence *currency = occurrence_new(text, BANK_OCCURRENCE_CURRENCY,
                    currency_start, currency_start + currency_length, upper,
                    BANK_VALIDATION_VALID, "explicit-currency", error);
                g_free(upper); if (currency == NULL) return FALSE;
                add_occurrence(r, currency);
            } else g_free(upper);
        }
        start = scan_end - 1U;
    }
    return TRUE;
}

static gboolean same_paragraph(const char *text, gsize left, gsize right)
{
    if (left > right) { gsize tmp = left; left = right; right = tmp; }
    for (gsize i = left; i + 1U < right; i++)
        if (text[i] == '\n' && text[i + 1U] == '\n') return FALSE;
    return TRUE;
}

static void build_blocks(BankAnalysisResult *r)
{
    BankProposedBlock *current = NULL;
    for (guint i = 0; i < r->occurrences->len; i++) {
        BankOccurrence *o = g_ptr_array_index(r->occurrences, i);
        if (current == NULL || !same_paragraph(r->source_text,
                current->end_offset, o->start_offset)) {
            if (r->blocks->len >= BANK_ANALYSIS_MAX_BLOCKS) {
                r->block_limit_reached = TRUE; break;
            }
            current = g_new0(BankProposedBlock, 1);
            current->identifier = g_strdup_printf("block-%04u", r->blocks->len + 1U);
            current->occurrence_identifiers = g_ptr_array_new_with_free_func(g_free);
            current->reasons = g_ptr_array_new_with_free_func(g_free);
            g_ptr_array_add(current->reasons, g_strdup("same-paragraph"));
            current->start_offset = o->start_offset; current->origin = BANK_ORIGIN_AUTOMATIC;
            current->decision = BANK_DECISION_PROPOSED; g_ptr_array_add(r->blocks, current);
        }
        if (current->occurrence_identifiers->len < BANK_ANALYSIS_MAX_ITEMS_PER_BLOCK)
            g_ptr_array_add(current->occurrence_identifiers, g_strdup(o->identifier));
        else current->ambiguous = TRUE;
        current->end_offset = MAX(current->end_offset, o->end_offset);
    }
    for (guint i = 0; i < r->blocks->len; i++) {
        BankProposedBlock *block = g_ptr_array_index(r->blocks, i);
        guint holders = 0, ibans = 0, bics = 0;
        for (guint j = 0; j < block->occurrence_identifiers->len; j++) {
            BankOccurrence *o = bank_analysis_find_occurrence(r,
                g_ptr_array_index(block->occurrence_identifiers, j));
            holders += o->type == BANK_OCCURRENCE_DECLARED_HOLDER;
            ibans += o->type == BANK_OCCURRENCE_IBAN;
            bics += o->type == BANK_OCCURRENCE_BIC;
        }
        if (holders > 1U || (bics > 0U && ibans > 1U)) {
            block->ambiguous = TRUE;
            g_ptr_array_add(block->reasons, g_strdup("multiple-possible-associations"));
        }
    }
}

static guint block_count_type(const BankAnalysisResult *r,
    const BankProposedBlock *block, BankOccurrenceType type)
{
    guint count = 0;
    for (guint i = 0; i < block->occurrence_identifiers->len; i++) {
        BankOccurrence *o = bank_analysis_find_occurrence(r,
            g_ptr_array_index(block->occurrence_identifiers, i));
        count += o->type == type;
    }
    return count;
}

static void build_transaction_proposal(BankAnalysisResult *r)
{
    for (guint block_index = 0; block_index < r->blocks->len; block_index++) {
        BankProposedBlock *block = g_ptr_array_index(r->blocks, block_index);
        gsize paragraph_start = block->start_offset;
        while (paragraph_start > 0 && !(paragraph_start > 1U &&
            r->source_text[paragraph_start - 1U] == '\n' &&
            r->source_text[paragraph_start - 2U] == '\n')) paragraph_start--;
        char *prefix = g_strndup(r->source_text + paragraph_start,
            block->start_offset - paragraph_start);
        gboolean marker = g_regex_match_simple(
            "(?i)\\b(transaction|virement|transfer)\\b", prefix, 0, 0);
        g_free(prefix);
        guint debtors = block_count_type(r, block, BANK_OCCURRENCE_DEBTOR_ACCOUNT);
        guint creditors = block_count_type(r, block, BANK_OCCURRENCE_CREDITOR_ACCOUNT);
        guint amounts = block_count_type(r, block, BANK_OCCURRENCE_AMOUNT);
        if (!marker || debtors == 0 || creditors == 0 || amounts == 0) continue;
        if (r->transaction_proposals->len >= BANK_ANALYSIS_MAX_TRANSACTIONS) {
            r->transaction_limit_reached = TRUE; break;
        }
        BankTransactionProposal *p = g_new0(BankTransactionProposal, 1);
        p->identifier = g_strdup_printf("transaction-proposal-%04u",
            r->transaction_proposals->len + 1U);
        p->source_occurrence_identifiers = g_ptr_array_new_with_free_func(g_free);
        p->missing_fields = g_ptr_array_new_with_free_func(g_free);
        p->ambiguities = g_ptr_array_new_with_free_func(g_free);
        p->reasons = g_ptr_array_new_with_free_func(g_free);
        g_ptr_array_add(p->reasons, g_strdup("explicit-transaction-marker"));
        g_ptr_array_add(p->reasons, g_strdup("explicit-debit-credit-markers"));
        p->decision = BANK_DECISION_PROPOSED;
        for (guint i = 0; i < block->occurrence_identifiers->len; i++)
            g_ptr_array_add(p->source_occurrence_identifiers,
                g_strdup(g_ptr_array_index(block->occurrence_identifiers, i)));
        if (debtors > 1 || creditors > 1 || amounts > 1)
            g_ptr_array_add(p->ambiguities, g_strdup("multiple-explicit-candidates"));
        if (block_count_type(r, block, BANK_OCCURRENCE_OBSERVED_DATE) == 0)
            g_ptr_array_add(p->missing_fields, g_strdup("observed_date"));
        g_ptr_array_add(r->transaction_proposals, p);
    }
}

BankAnalysisResult *bank_structured_extractor_analyze(const char *text, GError **error)
{
    BankAnalysisResult *r = g_new0(BankAnalysisResult, 1);
    r->source_text = g_strdup(text == NULL ? "" : text);
    r->occurrences = g_ptr_array_new_with_free_func((GDestroyNotify)bank_occurrence_free);
    r->blocks = g_ptr_array_new_with_free_func((GDestroyNotify)bank_proposed_block_free);
    r->transaction_proposals = g_ptr_array_new_with_free_func((GDestroyNotify)bank_transaction_proposal_free);
    r->warnings = g_ptr_array_new_with_free_func(g_free);
    if (text == NULL || text[0] == '\0') return r;
    if (strlen(text) > FINANCIAL_TEXT_MAX_BYTES) {
        g_set_error_literal(error, analysis_error(), 1, "Le texte dépasse la limite autorisée.");
        bank_analysis_result_free(r); return NULL;
    }
    GPtrArray *ibans = bank_text_extractor_extract(text, error);
    if (error != NULL && *error != NULL) { g_ptr_array_unref(ibans); bank_analysis_result_free(r); return NULL; }
    if (ibans->len >= FINANCIAL_CANDIDATE_MAX_RESULTS)
        r->occurrence_limit_reached = TRUE;
    for (guint i = 0; i < ibans->len; i++) {
        BankCandidate *c = g_ptr_array_index(ibans, i);
        BankOccurrence *o = occurrence_new(text, BANK_OCCURRENCE_IBAN,
            c->start_offset, c->end_offset, c->normalized_iban, c->validation,
            "canonical-iban", error);
        if (o == NULL) { g_ptr_array_unref(ibans); bank_analysis_result_free(r); return NULL; }
        for (guint w = 0; w < c->warnings->len; w++)
            g_ptr_array_add(o->warnings, g_strdup(g_ptr_array_index(c->warnings, w)));
        add_occurrence(r, o);
    }
    g_ptr_array_unref(ibans);
    if (!scan_regex(r, "(?im)^(?:BIC|SWIFT)[ \\t]*:[ \\t]*([^\\r\\n]{1,32})", BANK_OCCURRENCE_BIC, 1,
        BANK_VALIDATION_UNVERIFIABLE, "explicit-bic", TRUE, error) ||
        !scan_regex(r, "(?im)^(?:Titulaire|Nom du titulaire|Account holder)[ \\t]*:[ \\t]*([^\\r\\n]{1,512})", BANK_OCCURRENCE_DECLARED_HOLDER, 1,
        BANK_VALIDATION_UNVERIFIABLE, "explicit-holder", FALSE, error) ||
        !scan_regex(r, "(?im)^(?:Banque|Établissement)[ \\t]*:[ \\t]*([^\\r\\n]{1,512})", BANK_OCCURRENCE_DECLARED_INSTITUTION, 1,
        BANK_VALIDATION_UNVERIFIABLE, "explicit-institution", FALSE, error) ||
        !scan_regex(r, "(?im)^Adresse de la banque[ \\t]*:[ \\t]*([^\\r\\n]{1,512})", BANK_OCCURRENCE_DECLARED_ADDRESS, 1,
        BANK_VALIDATION_UNVERIFIABLE, "explicit-bank-address", FALSE, error) ||
        !scan_regex(r, "(?im)^Payeur[ \\t]*:[ \\t]*([^\\r\\n]{1,512})", BANK_OCCURRENCE_DECLARED_PAYER, 1,
        BANK_VALIDATION_UNVERIFIABLE, "explicit-payer", FALSE, error) ||
        !scan_regex(r, "(?im)^Bénéficiaire[ \\t]*:[ \\t]*([^\\r\\n]{1,512})", BANK_OCCURRENCE_DECLARED_BENEFICIARY, 1,
        BANK_VALIDATION_UNVERIFIABLE, "explicit-beneficiary", FALSE, error) ||
        !scan_regex(r, "(?im)^(?:Compte débité|Débiteur)[ \\t]*:[ \\t]*([^\\r\\n]{1,512})", BANK_OCCURRENCE_DEBTOR_ACCOUNT, 1,
        BANK_VALIDATION_UNVERIFIABLE, "explicit-debtor", FALSE, error) ||
        !scan_regex(r, "(?im)^(?:Compte crédité|Créditeur)[ \\t]*:[ \\t]*([^\\r\\n]{1,512})", BANK_OCCURRENCE_CREDITOR_ACCOUNT, 1,
        BANK_VALIDATION_UNVERIFIABLE, "explicit-creditor", FALSE, error) ||
        !scan_regex(r, "(?im)^Date[ \\t]*:[ \\t]*([^\\r\\n]{1,64})", BANK_OCCURRENCE_OBSERVED_DATE, 1,
        BANK_VALIDATION_UNVERIFIABLE, "explicit-date", FALSE, error) ||
        !scan_regex(r, "(?im)^Heure[ \\t]*:[ \\t]*([^\\r\\n]{1,32})", BANK_OCCURRENCE_OBSERVED_TIME, 1,
        BANK_VALIDATION_UNVERIFIABLE, "explicit-time", FALSE, error) ||
        !scan_regex(r, "(?im)^Statut[ \\t]*:[ \\t]*([^\\r\\n]{1,128})", BANK_OCCURRENCE_OBSERVED_STATUS, 1,
        BANK_VALIDATION_UNVERIFIABLE, "explicit-observed-status", FALSE, error) ||
        !scan_regex(r, "(?im)^(?:Référence|Référence de transaction)[ \\t]*:[ \\t]*([^\\r\\n]{1,256})", BANK_OCCURRENCE_BANK_REFERENCE, 1,
        BANK_VALIDATION_UNVERIFIABLE, "explicit-reference", FALSE, error) ||
        !scan_regex(r, "(?im)^UETR[ \\t]*:[ \\t]*([^\\r\\n]{1,64})", BANK_OCCURRENCE_UETR, 1,
        BANK_VALIDATION_UNVERIFIABLE, "explicit-uetr", TRUE, error) || !scan_amounts(r, error)) {
        bank_analysis_result_free(r); return NULL;
    }
    for (guint i = 0; i < r->occurrences->len; i++) {
        BankOccurrence *o = g_ptr_array_index(r->occurrences, i);
        if (o->type == BANK_OCCURRENCE_BIC) {
            BicValidatorResult check = bic_validator_check(o->normalized_value);
            o->validation = check == BIC_VALIDATOR_VALID ? BANK_VALIDATION_VALID : BANK_VALIDATION_INVALID;
            if (check != BIC_VALIDATOR_VALID) g_clear_pointer(&o->normalized_value, g_free);
        } else if (o->type == BANK_OCCURRENCE_UETR) {
            GRegex *uuid = g_regex_new("^[0-9A-F]{8}-[0-9A-F]{4}-[1-5][0-9A-F]{3}-[89AB][0-9A-F]{3}-[0-9A-F]{12}$", 0, 0, NULL);
            o->validation = g_regex_match(uuid, o->normalized_value, 0, NULL) ? BANK_VALIDATION_VALID : BANK_VALIDATION_INVALID;
            if (o->validation != BANK_VALIDATION_VALID) g_clear_pointer(&o->normalized_value, g_free);
            g_regex_unref(uuid);
        }
    }
    g_ptr_array_sort(r->occurrences, occurrence_compare);
    for (guint i = 0; i < r->occurrences->len; i++) {
        BankOccurrence *o = g_ptr_array_index(r->occurrences, i);
        o->identifier = g_strdup_printf("occ-%04u-%s", i + 1U, bank_occurrence_type_to_code(o->type));
    }
    build_blocks(r); build_transaction_proposal(r); return r;
}
