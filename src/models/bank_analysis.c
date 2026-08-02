#include "models/bank_analysis.h"

const char *bank_occurrence_type_to_code(BankOccurrenceType type)
{
    static const char *codes[] = {"iban","bic","declared_holder",
        "declared_institution","declared_address","amount","currency",
        "observed_date","observed_time","observed_status","bank_reference",
        "uetr","declared_payer","declared_beneficiary","debtor_account",
        "creditor_account"};
    if ((gint)type < 0 || type >= BANK_OCCURRENCE_COUNT)
        return "unknown";
    return codes[type];
}

void bank_occurrence_free(BankOccurrence *o)
{
    if (o == NULL) return;
    g_free(o->identifier); g_free(o->raw_value); g_free(o->normalized_value);
    g_free(o->extraction_rule); g_clear_pointer(&o->warnings, g_ptr_array_unref);
    g_free(o->source_context); g_free(o);
}

void bank_proposed_block_free(BankProposedBlock *b)
{
    if (b == NULL) return;
    g_free(b->identifier);
    g_clear_pointer(&b->occurrence_identifiers, g_ptr_array_unref);
    g_clear_pointer(&b->reasons, g_ptr_array_unref); g_free(b);
}

void bank_transaction_proposal_free(BankTransactionProposal *p)
{
    if (p == NULL) return;
    g_free(p->identifier);
    g_clear_pointer(&p->source_occurrence_identifiers, g_ptr_array_unref);
    g_clear_pointer(&p->missing_fields, g_ptr_array_unref);
    g_clear_pointer(&p->ambiguities, g_ptr_array_unref);
    g_clear_pointer(&p->reasons, g_ptr_array_unref); g_free(p);
}

void bank_analysis_result_free(BankAnalysisResult *r)
{
    if (r == NULL) return;
    g_free(r->source_text); g_clear_pointer(&r->occurrences, g_ptr_array_unref);
    g_clear_pointer(&r->blocks, g_ptr_array_unref);
    g_clear_pointer(&r->transaction_proposals, g_ptr_array_unref);
    g_clear_pointer(&r->warnings, g_ptr_array_unref); g_free(r);
}

BankOccurrence *bank_analysis_find_occurrence(const BankAnalysisResult *r,
    const char *id)
{
    if (r == NULL || id == NULL) return NULL;
    for (guint i = 0; i < r->occurrences->len; i++) {
        BankOccurrence *o = g_ptr_array_index(r->occurrences, i);
        if (g_strcmp0(o->identifier, id) == 0) return o;
    }
    return NULL;
}

BankProposedBlock *bank_analysis_find_block(const BankAnalysisResult *r,
    const char *id)
{
    if (r == NULL || id == NULL) return NULL;
    for (guint i = 0; i < r->blocks->len; i++) {
        BankProposedBlock *b = g_ptr_array_index(r->blocks, i);
        if (g_strcmp0(b->identifier, id) == 0) return b;
    }
    return NULL;
}
