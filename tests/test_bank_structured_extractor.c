#include "core/bank_analysis_revision.h"
#include "core/bank_structured_extractor.h"
#include <glib.h>
#include <string.h>

static const char *fr = "FR4830002005500000000000052";
static const char *de = "DE89370400440532013000";
static const char *gb = "GB82WEST12345698765432";

static guint count_type(const BankAnalysisResult *result, BankOccurrenceType type)
{
    guint count = 0;
    for (guint i = 0; i < result->occurrences->len; i++)
        if (((BankOccurrence *)g_ptr_array_index(result->occurrences, i))->type == type) count++;
    return count;
}

static BankOccurrence *first_type(const BankAnalysisResult *result, BankOccurrenceType type)
{
    for (guint i = 0; i < result->occurrences->len; i++) {
        BankOccurrence *o = g_ptr_array_index(result->occurrences, i);
        if (o->type == type) return o;
    }
    return NULL;
}

static void assert_offsets(const BankAnalysisResult *result)
{
    for (guint i = 0; i < result->occurrences->len; i++) {
        BankOccurrence *o = g_ptr_array_index(result->occurrences, i);
        g_assert_cmpuint(o->start_offset, <=, o->end_offset);
        g_assert_cmpmem(result->source_text + o->start_offset,
            o->end_offset - o->start_offset, o->raw_value, strlen(o->raw_value));
        if (i > 0) {
            BankOccurrence *previous = g_ptr_array_index(result->occurrences, i - 1U);
            g_assert_true(previous->start_offset < o->start_offset ||
                (previous->start_offset == o->start_offset && previous->end_offset <= o->end_offset));
        }
    }
}

static void test_three_accounts_and_holders(void)
{
    char *text = g_strdup_printf(
        "Titulaire : SPECIMEN ALPHA\nBanque : SPECIMEN BANK\nBIC : BNPAFRPP\n"
        "IBAN : FR48 3000 2005 5000 0000 0000 052\nIBAN : %s\n\n"
        "Titulaire : SPECIMEN BETA\nBIC : DEUTDEFFXXX\nIBAN : %s", de, gb);
    BankAnalysisResult *r = bank_structured_extractor_analyze(text, NULL);
    g_assert_nonnull(r); g_assert_cmpuint(count_type(r, BANK_OCCURRENCE_IBAN), ==, 3);
    g_assert_cmpuint(count_type(r, BANK_OCCURRENCE_BIC), ==, 2);
    g_assert_cmpuint(count_type(r, BANK_OCCURRENCE_DECLARED_HOLDER), ==, 2);
    g_assert_cmpuint(count_type(r, BANK_OCCURRENCE_DECLARED_INSTITUTION), ==, 1);
    g_assert_cmpuint(r->blocks->len, ==, 2); g_assert_cmpuint(r->transaction_proposals->len, ==, 0);
    assert_offsets(r); bank_analysis_result_free(r); g_free(text);
}

static void test_orphans_ambiguities_duplicates(void)
{
    char *text = g_strdup_printf("BIC : BNPAFRPP\n\nIBAN : FR00 3000 2005 5000 0000 0000 021\n\n"
        "IBAN : FRO8 3000 2005 5000 0000 0000 052\n\nIBAN : AD1200000000000000000000\n\n%s puis %s", fr, fr);
    BankAnalysisResult *r = bank_structured_extractor_analyze(text, NULL);
    g_assert_nonnull(r); g_assert_cmpuint(count_type(r, BANK_OCCURRENCE_BIC), ==, 1);
    g_assert_cmpuint(count_type(r, BANK_OCCURRENCE_IBAN), >=, 5);
    guint ambiguous = 0, unverifiable = 0;
    for (guint i = 0; i < r->occurrences->len; i++) {
        BankOccurrence *o = g_ptr_array_index(r->occurrences, i);
        ambiguous += o->validation == BANK_VALIDATION_AMBIGUOUS;
        unverifiable += o->validation == BANK_VALIDATION_UNVERIFIABLE;
    }
    g_assert_cmpuint(ambiguous, >=, 1); g_assert_cmpuint(unverifiable, >=, 1);
    assert_offsets(r); bank_analysis_result_free(r); g_free(text);
}

static void test_utf8_punctuation_crlf(void)
{
    char *text = g_strdup_printf("Préfixe SPECIMEN : [%s],\r\nBIC : BNPAFRPP", fr);
    BankAnalysisResult *r = bank_structured_extractor_analyze(text, NULL);
    BankOccurrence *iban = first_type(r, BANK_OCCURRENCE_IBAN);
    g_assert_nonnull(iban); g_assert_cmpuint(iban->start_offset, ==,
        (gsize)(strstr(text, fr) - text)); assert_offsets(r);
    bank_analysis_result_free(r); g_free(text);
}

static void test_amounts_and_currency(void)
{
    const char *text = "Montant : 1234,56 EUR\nMontant : 1 234,56 €\n"
        "Amount : EUR 1234.56\nMontant : 99 USD\nMontant : 1,234\nMontant : 12.123456789";
    BankAnalysisResult *r = bank_structured_extractor_analyze(text, NULL);
    g_assert_cmpuint(count_type(r, BANK_OCCURRENCE_AMOUNT), ==, 6);
    g_assert_cmpuint(count_type(r, BANK_OCCURRENCE_CURRENCY), ==, 4);
    guint ambiguous = 0, invalid = 0;
    for (guint i = 0; i < r->occurrences->len; i++) {
        BankOccurrence *o = g_ptr_array_index(r->occurrences, i);
        if (o->type == BANK_OCCURRENCE_AMOUNT) {
            ambiguous += o->validation == BANK_VALIDATION_AMBIGUOUS;
            invalid += o->validation == BANK_VALIDATION_INVALID;
        }
    }
    g_assert_cmpuint(ambiguous, ==, 1); g_assert_cmpuint(invalid, ==, 1);
    bank_analysis_result_free(r);
}

static void test_explicit_transaction(void)
{
    char *text = g_strdup_printf("Virement SPECIMEN\nCompte débité : %s\nCompte crédité : %s\n"
        "Montant : 1234,56 EUR\nPayeur : SPECIMEN ALPHA\nBénéficiaire : SPECIMEN BETA\n"
        "Date : 2026-02-03\nHeure : 14:05\nStatut : exécuté\n"
        "Référence de transaction : SPECIMEN-REF-42\n"
        "UETR : 11111111-2222-4333-8444-555555555555", fr, de);
    BankAnalysisResult *r = bank_structured_extractor_analyze(text, NULL);
    g_assert_cmpuint(r->transaction_proposals->len, ==, 1);
    BankTransactionProposal *p = g_ptr_array_index(r->transaction_proposals, 0);
    g_assert_cmpint(p->decision, ==, BANK_DECISION_PROPOSED);
    g_assert_false(p->create_transaction); g_assert_false(p->create_account);
    g_assert_false(p->create_person); g_assert_false(p->create_relation);
    BankOccurrence *status = first_type(r, BANK_OCCURRENCE_OBSERVED_STATUS);
    g_assert_cmpstr(status->raw_value, ==, "exécuté");
    g_assert_cmpint(first_type(r, BANK_OCCURRENCE_UETR)->validation, ==, BANK_VALIDATION_VALID);
    bank_analysis_result_free(r); g_free(text);
}

static void test_insufficient_transaction(void)
{
    char *text = g_strdup_printf("%s\n%s\nMontant : 10 EUR\nStatut : pending\n"
        "Référence : SPECIMEN-ONLY\nUETR : invalid", fr, de);
    BankAnalysisResult *r = bank_structured_extractor_analyze(text, NULL);
    g_assert_cmpuint(r->transaction_proposals->len, ==, 0);
    g_assert_cmpint(first_type(r, BANK_OCCURRENCE_UETR)->validation, ==, BANK_VALIDATION_INVALID);
    bank_analysis_result_free(r); g_free(text);
}

static void test_memory_revision(void)
{
    char *text = g_strdup_printf("Titulaire : SPECIMEN\nIBAN : %s\nIBAN : %s", fr, de);
    BankAnalysisResult *r = bank_structured_extractor_analyze(text, NULL);
    g_assert_cmpuint(r->blocks->len, ==, 1); BankProposedBlock *first = g_ptr_array_index(r->blocks, 0);
    char *first_id = g_strdup(first->identifier);
    char *occurrence = g_strdup(g_ptr_array_index(first->occurrence_identifiers, 0));
    g_assert_true(bank_analysis_revision_split(r, first_id, "manual-a", occurrence, NULL));
    g_assert_cmpuint(r->blocks->len, ==, 2);
    g_assert_true(bank_analysis_revision_move(r, "manual-a", first_id, occurrence, NULL));
    g_assert_true(bank_analysis_revision_create_block(r, "manual-empty", NULL, NULL));
    g_assert_true(bank_analysis_revision_mark_ambiguous(r, first_id, TRUE, NULL));
    g_assert_true(bank_analysis_revision_reject(r, first_id, NULL));
    g_assert_cmpint(bank_analysis_find_block(r, first_id)->decision, ==, BANK_DECISION_REJECTED);
    g_assert_nonnull(bank_analysis_find_occurrence(r, occurrence));
    guint before = bank_analysis_find_block(r, first_id)->occurrence_identifiers->len;
    g_assert_false(bank_analysis_revision_move(r, first_id, "absent", occurrence, NULL));
    g_assert_cmpuint(bank_analysis_find_block(r, first_id)->occurrence_identifiers->len, ==, before);
    g_assert_true(bank_analysis_revision_merge(r, first_id, "manual-empty", NULL));
    g_free(occurrence); g_free(first_id); bank_analysis_result_free(r); g_free(text);
}

static void test_determinism_and_limits(void)
{
    char *text = g_strdup_printf("IBAN : %s\nBIC : BNPAFRPP", fr);
    BankAnalysisResult *a = bank_structured_extractor_analyze(text, NULL);
    BankAnalysisResult *b = bank_structured_extractor_analyze(text, NULL);
    g_assert_cmpuint(a->occurrences->len, ==, b->occurrences->len);
    for (guint i = 0; i < a->occurrences->len; i++)
        g_assert_cmpstr(((BankOccurrence *)g_ptr_array_index(a->occurrences, i))->identifier,
            ==, ((BankOccurrence *)g_ptr_array_index(b->occurrences, i))->identifier);
    bank_analysis_result_free(a); bank_analysis_result_free(b); g_free(text);
    a = bank_structured_extractor_analyze(NULL, NULL); g_assert_cmpuint(a->occurrences->len, ==, 0);
    bank_analysis_result_free(a);
    char *large = g_malloc0(FINANCIAL_TEXT_MAX_BYTES + 2U);
    memset(large, 'A', FINANCIAL_TEXT_MAX_BYTES + 1U); GError *error = NULL;
    g_assert_null(bank_structured_extractor_analyze(large, &error)); g_assert_nonnull(error);
    g_clear_error(&error); g_free(large);
    GString *many = g_string_new(NULL);
    for (guint i = 0; i < BANK_ANALYSIS_MAX_OCCURRENCES + 1U; i++)
        g_string_append_printf(many, "%s,", fr);
    a = bank_structured_extractor_analyze(many->str, NULL);
    g_assert_nonnull(a); g_assert_true(a->occurrence_limit_reached);
    g_assert_cmpuint(a->occurrences->len, ==, BANK_ANALYSIS_MAX_OCCURRENCES);
    bank_analysis_result_free(a); g_string_free(many, TRUE);
    GString *sections = g_string_new(NULL);
    for (guint i = 0; i < BANK_ANALYSIS_MAX_BLOCKS + 1U; i++)
        g_string_append_printf(sections, "%s\n\n", fr);
    a = bank_structured_extractor_analyze(sections->str, NULL);
    g_assert_nonnull(a); g_assert_true(a->block_limit_reached);
    g_assert_cmpuint(a->blocks->len, ==, BANK_ANALYSIS_MAX_BLOCKS);
    bank_analysis_result_free(a); g_string_free(sections, TRUE);
    GString *long_holder = g_string_new("Titulaire : ");
    for (guint i = 0; i < BANK_ANALYSIS_MAX_VALUE_BYTES + 1U; i++)
        g_string_append_c(long_holder, 'A');
    error = NULL; g_assert_null(bank_structured_extractor_analyze(long_holder->str, &error));
    g_assert_nonnull(error); g_clear_error(&error); g_string_free(long_holder, TRUE);
}

static void assert_amount(const BankAnalysisResult *result, const char *raw,
    BankValidationState state, const char *normalized)
{
    for (guint i = 0; i < result->occurrences->len; i++) {
        BankOccurrence *o = g_ptr_array_index(result->occurrences, i);
        if (o->type == BANK_OCCURRENCE_AMOUNT && g_strcmp0(o->raw_value, raw) == 0) {
            g_assert_cmpint(o->validation, ==, state);
            g_assert_cmpstr(o->normalized_value, ==, normalized);
            g_assert_cmpmem(result->source_text + o->start_offset,
                o->end_offset - o->start_offset, raw, strlen(raw));
            return;
        }
    }
    g_assert_not_reached();
}

static void test_amount_complete_ambiguities(void)
{
    const char *text = "Montant : 1,234\nMontant : 1.234\nMontant : 1,234,567\n"
        "Montant : 1.234.567\nMontant : 1,234.56 USD\nMontant : 1.234,56 EUR\n"
        "Montant : 12,34,56\nMontant : 1..234\nMontant : 1,,234\n"
        "Montant : 1234,567,89 EUR\nMontant : (1234,56 EUR),";
    BankAnalysisResult *r = bank_structured_extractor_analyze(text, NULL);
    g_assert_nonnull(r);
    assert_amount(r, "1,234", BANK_VALIDATION_AMBIGUOUS, NULL);
    assert_amount(r, "1.234", BANK_VALIDATION_AMBIGUOUS, NULL);
    assert_amount(r, "1,234,567", BANK_VALIDATION_VALID, "1234567");
    assert_amount(r, "1.234.567", BANK_VALIDATION_VALID, "1234567");
    assert_amount(r, "1,234.56", BANK_VALIDATION_VALID, "1234.56");
    assert_amount(r, "1.234,56", BANK_VALIDATION_VALID, "1234.56");
    assert_amount(r, "12,34,56", BANK_VALIDATION_INVALID, NULL);
    assert_amount(r, "1..234", BANK_VALIDATION_INVALID, NULL);
    assert_amount(r, "1,,234", BANK_VALIDATION_INVALID, NULL);
    assert_amount(r, "1234,567,89", BANK_VALIDATION_INVALID, NULL);
    assert_amount(r, "1234,56", BANK_VALIDATION_VALID, "1234.56");
    bank_analysis_result_free(r);
}

static void test_amount_bounds_before_copy(void)
{
    GString *text = g_string_new("Montant : ");
    for (guint i = 0; i < BANK_ANALYSIS_MAX_AMOUNT_BYTES + 1U; i++)
        g_string_append_c(text, '1');
    BankAnalysisResult *r = bank_structured_extractor_analyze(text->str, NULL);
    g_assert_nonnull(r); g_assert_true(r->amount_limit_reached);
    g_assert_cmpuint(count_type(r, BANK_OCCURRENCE_AMOUNT), ==, 0);
    g_assert_cmpuint(r->warnings->len, >, 0);
    bank_analysis_result_free(r); g_string_free(text, TRUE);

    text = g_string_new("Montant : ");
    for (guint i = 0; i < BANK_ANALYSIS_MAX_AMOUNT_DIGITS; i++)
        g_string_append_c(text, '1');
    r = bank_structured_extractor_analyze(text->str, NULL);
    g_assert_nonnull(r); g_assert_false(r->amount_limit_reached);
    g_assert_cmpuint(count_type(r, BANK_OCCURRENCE_AMOUNT), ==, 1);
    bank_analysis_result_free(r); g_string_free(text, TRUE);

    text = g_string_new("Montant : 1");
    for (guint i = 0; i < BANK_ANALYSIS_MAX_AMOUNT_GROUPS + 1U; i++)
        g_string_append(text, " 234");
    r = bank_structured_extractor_analyze(text->str, NULL);
    g_assert_nonnull(r); g_assert_true(r->amount_limit_reached);
    g_assert_cmpuint(count_type(r, BANK_OCCURRENCE_AMOUNT), ==, 0);
    bank_analysis_result_free(r); g_string_free(text, TRUE);

    r = bank_structured_extractor_analyze("Montant : 12.12345678 EUR\n"
        "Montant : 12.123456789 EUR", NULL);
    assert_amount(r, "12.12345678", BANK_VALIDATION_VALID, "12.12345678");
    assert_amount(r, "12.123456789", BANK_VALIDATION_INVALID, NULL);
    bank_analysis_result_free(r);
}

static void test_amount_strict_groups(void)
{
    const char *text = "Montant: 12,34.56 USD\nMontant: 1.2,34 EUR\n"
        "Montant: 1,23.456 EUR\nMontant: 1.234,5,67 EUR\n"
        "Montant: 1,234.5.67 USD\nMontant: 10 20 EUR\n"
        "Montant: 1 23 456 EUR\nMontant: 1 234 56 EUR\n"
        "Montant: 1  234 EUR\nMontant: 1234,56 EUR\n"
        "Montant: 1234.56 USD\nMontant: 1 234,56 EUR\n"
        "Montant: 1 234.56 USD\nMontant: 1.234.567,89 EUR\n"
        "Montant: 1,234,567.89 USD";
    BankAnalysisResult *r = bank_structured_extractor_analyze(text, NULL);
    const char *invalid[] = {"12,34.56", "1.2,34", "1,23.456",
        "1.234,5,67", "1,234.5.67", "10 20", "1 23 456",
        "1 234 56", "1  234"};
    for (guint i = 0; i < G_N_ELEMENTS(invalid); i++)
        assert_amount(r, invalid[i], BANK_VALIDATION_INVALID, NULL);
    assert_amount(r, "1234,56", BANK_VALIDATION_VALID, "1234.56");
    assert_amount(r, "1234.56", BANK_VALIDATION_VALID, "1234.56");
    assert_amount(r, "1 234,56", BANK_VALIDATION_VALID, "1234.56");
    assert_amount(r, "1 234.56", BANK_VALIDATION_VALID, "1234.56");
    assert_amount(r, "1.234.567,89", BANK_VALIDATION_VALID, "1234567.89");
    assert_amount(r, "1,234,567.89", BANK_VALIDATION_VALID, "1234567.89");
    assert_offsets(r); bank_analysis_result_free(r);
}

static void test_amount_terminal_punctuation(void)
{
    const char *text = "Montant: 1234 EUR,\nMontant: 1234 EUR,,\n"
        "Montant: 1234 EUR...\nMontant: (1234 EUR)\nMontant: [1234 EUR]";
    BankAnalysisResult *r = bank_structured_extractor_analyze(text, NULL);
    g_assert_cmpuint(count_type(r, BANK_OCCURRENCE_AMOUNT), ==, 5);
    for (guint i = 0; i < r->occurrences->len; i++) {
        BankOccurrence *o = g_ptr_array_index(r->occurrences, i);
        if (o->type == BANK_OCCURRENCE_AMOUNT) {
            g_assert_cmpstr(o->raw_value, ==, "1234");
            g_assert_cmpint(o->validation, ==, BANK_VALIDATION_VALID);
        }
    }
    assert_offsets(r); bank_analysis_result_free(r);
}

static void test_amount_context_before_limits(void)
{
    GString *text = g_string_new("SPECIMEN ");
    for (guint i = 0; i < BANK_ANALYSIS_MAX_AMOUNT_BYTES + 64U; i++)
        g_string_append_c(text, '7');
    g_string_append(text, "\nRéférence: ");
    for (guint i = 0; i < BANK_ANALYSIS_MAX_AMOUNT_BYTES + 64U; i++)
        g_string_append_c(text, '8');
    g_string_append(text, "\nIdentifiant: 123456789012345678901234567890"
        "\nTéléphone SPECIMEN: 01 02 03 04 05");
    g_string_append(text, "\nSPECIMEN ");
    for (guint i = 0; i < BANK_ANALYSIS_MAX_AMOUNT_BYTES + 64U; i++)
        g_string_append_c(text, '5');
    g_string_append(text, " EURREFERENCE");
    BankAnalysisResult *r = bank_structured_extractor_analyze(text->str, NULL);
    g_assert_cmpuint(count_type(r, BANK_OCCURRENCE_AMOUNT), ==, 0);
    g_assert_false(r->amount_limit_reached); g_assert_cmpuint(r->warnings->len, ==, 0);
    bank_analysis_result_free(r); g_string_free(text, TRUE);

    text = g_string_new("Montant: ");
    for (guint i = 0; i < BANK_ANALYSIS_MAX_AMOUNT_BYTES + 1U; i++)
        g_string_append_c(text, '9');
    g_string_append(text, "\nSPECIMEN ");
    for (guint i = 0; i < BANK_ANALYSIS_MAX_AMOUNT_BYTES + 1U; i++)
        g_string_append_c(text, '6');
    g_string_append(text, " EUR");
    r = bank_structured_extractor_analyze(text->str, NULL);
    g_assert_true(r->amount_limit_reached); g_assert_cmpuint(r->warnings->len, ==, 1);
    g_assert_cmpuint(count_type(r, BANK_OCCURRENCE_AMOUNT), ==, 0);
    bank_analysis_result_free(r); g_string_free(text, TRUE);
}

static char *revision_snapshot(const BankAnalysisResult *result)
{
    GString *snapshot = g_string_new(NULL);
    g_string_append_printf(snapshot, "%u|", result->blocks->len);
    for (guint i = 0; i < result->blocks->len; i++) {
        BankProposedBlock *b = g_ptr_array_index(result->blocks, i);
        g_string_append_printf(snapshot, "%s:%zu:%zu:%d:%d:%d[", b->identifier,
            b->start_offset, b->end_offset, b->ambiguous, b->origin, b->decision);
        for (guint j = 0; j < b->occurrence_identifiers->len; j++)
            g_string_append_printf(snapshot, "%s,",
                (char *)g_ptr_array_index(b->occurrence_identifiers, j));
        g_string_append_c(snapshot, ']');
    }
    return g_string_free(snapshot, FALSE);
}

#define ASSERT_REVISION_FAILURE_UNCHANGED(result, expression) G_STMT_START { \
    char *before_snapshot = revision_snapshot(result); \
    g_assert_false((expression)); \
    char *after_snapshot = revision_snapshot(result); \
    g_assert_cmpstr(after_snapshot, ==, before_snapshot); \
    g_free(after_snapshot); g_free(before_snapshot); \
} G_STMT_END

static void test_revision_identifier_limits_and_atomicity(void)
{
    char *text = g_strdup_printf("IBAN : %s\nIBAN : %s", fr, de);
    BankAnalysisResult *r = bank_structured_extractor_analyze(text, NULL);
    BankProposedBlock *base = g_ptr_array_index(r->blocks, 0);
    const char *occurrence = g_ptr_array_index(base->occurrence_identifiers, 0);
    char valid[BANK_ANALYSIS_MAX_ID_BYTES + 1U];
    memset(valid, 'a', BANK_ANALYSIS_MAX_ID_BYTES); valid[BANK_ANALYSIS_MAX_ID_BYTES] = '\0';
    g_assert_true(bank_analysis_revision_create_block(r, valid, NULL, NULL));
    char *too_long = g_malloc0(BANK_ANALYSIS_MAX_ID_BYTES + 2U);
    memset(too_long, 'b', BANK_ANALYSIS_MAX_ID_BYTES + 1U);
    ASSERT_REVISION_FAILURE_UNCHANGED(r,
        bank_analysis_revision_create_block(r, too_long, NULL, NULL));
    ASSERT_REVISION_FAILURE_UNCHANGED(r,
        bank_analysis_revision_create_block(r, valid, NULL, NULL));
    ASSERT_REVISION_FAILURE_UNCHANGED(r,
        bank_analysis_revision_split(r, base->identifier, "new", "absent", NULL));
    ASSERT_REVISION_FAILURE_UNCHANGED(r,
        bank_analysis_revision_merge(r, base->identifier, base->identifier, NULL));
    ASSERT_REVISION_FAILURE_UNCHANGED(r,
        bank_analysis_revision_move(r, base->identifier, "absent", occurrence, NULL));
    ASSERT_REVISION_FAILURE_UNCHANGED(r,
        bank_analysis_revision_remove(r, base->identifier, "absent", NULL));
    ASSERT_REVISION_FAILURE_UNCHANGED(r,
        bank_analysis_revision_mark_ambiguous(r, "absent", TRUE, NULL));
    ASSERT_REVISION_FAILURE_UNCHANGED(r,
        bank_analysis_revision_reject(r, "absent", NULL));
    ASSERT_REVISION_FAILURE_UNCHANGED(r,
        bank_analysis_revision_split(r, base->identifier, too_long, occurrence, NULL));
    char *immense = g_malloc0(FINANCIAL_TEXT_MAX_BYTES + 1U);
    memset(immense, 'c', FINANCIAL_TEXT_MAX_BYTES);
    ASSERT_REVISION_FAILURE_UNCHANGED(r,
        bank_analysis_revision_create_block(r, immense, NULL, NULL));
    g_assert_nonnull(bank_analysis_find_occurrence(r, occurrence));
    g_free(immense); g_free(too_long); bank_analysis_result_free(r); g_free(text);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/bank-structured/three-accounts", test_three_accounts_and_holders);
    g_test_add_func("/bank-structured/orphans", test_orphans_ambiguities_duplicates);
    g_test_add_func("/bank-structured/utf8", test_utf8_punctuation_crlf);
    g_test_add_func("/bank-structured/amounts", test_amounts_and_currency);
    g_test_add_func("/bank-structured/transaction", test_explicit_transaction);
    g_test_add_func("/bank-structured/insufficient", test_insufficient_transaction);
    g_test_add_func("/bank-structured/revision", test_memory_revision);
    g_test_add_func("/bank-structured/determinism-limits", test_determinism_and_limits);
    g_test_add_func("/bank-structured/amount-complete", test_amount_complete_ambiguities);
    g_test_add_func("/bank-structured/amount-bounds", test_amount_bounds_before_copy);
    g_test_add_func("/bank-structured/amount-strict-groups", test_amount_strict_groups);
    g_test_add_func("/bank-structured/amount-punctuation", test_amount_terminal_punctuation);
    g_test_add_func("/bank-structured/amount-context-limits",
        test_amount_context_before_limits);
    g_test_add_func("/bank-structured/revision-atomicity",
        test_revision_identifier_limits_and_atomicity);
    return g_test_run();
}
