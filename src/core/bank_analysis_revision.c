#include "core/bank_analysis_revision.h"

static GQuark revision_error(void)
{ return g_quark_from_static_string("bank-analysis-revision-error"); }

static gboolean fail(GError **error, const char *message)
{
    g_set_error_literal(error, revision_error(), 1, message); return FALSE;
}

static gboolean valid_identifier(const char *identifier)
{
    if (identifier == NULL || identifier[0] == '\0') return FALSE;
    gsize length = 0;
    while (length <= BANK_ANALYSIS_MAX_ID_BYTES && identifier[length] != '\0')
        length++;
    return length <= BANK_ANALYSIS_MAX_ID_BYTES;
}

static gint identifier_index(const GPtrArray *items, const char *identifier)
{
    if (items == NULL || identifier == NULL) return -1;
    for (guint i = 0; i < items->len; i++)
        if (g_strcmp0(g_ptr_array_index((GPtrArray *)items, i), identifier) == 0)
            return (gint)i;
    return -1;
}

static void recompute_bounds(BankAnalysisResult *result, BankProposedBlock *block)
{
    if (block->occurrence_identifiers->len == 0) {
        block->start_offset = block->end_offset = 0; return;
    }
    block->start_offset = G_MAXSIZE; block->end_offset = 0;
    for (guint i = 0; i < block->occurrence_identifiers->len; i++) {
        BankOccurrence *o = bank_analysis_find_occurrence(result,
            g_ptr_array_index(block->occurrence_identifiers, i));
        block->start_offset = MIN(block->start_offset, o->start_offset);
        block->end_offset = MAX(block->end_offset, o->end_offset);
    }
}

gboolean bank_analysis_revision_create_block(BankAnalysisResult *result,
    const char *identifier, const char *occurrence_identifier, GError **error)
{
    if (result == NULL || !valid_identifier(identifier) ||
        (occurrence_identifier != NULL && !valid_identifier(occurrence_identifier)) ||
        bank_analysis_find_block(result, identifier) != NULL ||
        result->blocks->len >= BANK_ANALYSIS_MAX_BLOCKS)
        return fail(error, "Le bloc manuel est invalide.");
    BankOccurrence *o = occurrence_identifier == NULL ? NULL :
        bank_analysis_find_occurrence(result, occurrence_identifier);
    if (occurrence_identifier != NULL && o == NULL)
        return fail(error, "L’occurrence est introuvable.");
    BankProposedBlock *block = g_new0(BankProposedBlock, 1);
    block->identifier = g_strdup(identifier);
    block->occurrence_identifiers = g_ptr_array_new_with_free_func(g_free);
    block->reasons = g_ptr_array_new_with_free_func(g_free);
    g_ptr_array_add(block->reasons, g_strdup("human-modification"));
    block->origin = BANK_ORIGIN_HUMAN; block->decision = BANK_DECISION_PROPOSED;
    if (o != NULL) {
        g_ptr_array_add(block->occurrence_identifiers, g_strdup(o->identifier));
        block->start_offset = o->start_offset; block->end_offset = o->end_offset;
    }
    g_ptr_array_add(result->blocks, block); return TRUE;
}

gboolean bank_analysis_revision_remove(BankAnalysisResult *result,
    const char *block_id, const char *occurrence_id, GError **error)
{
    if (!valid_identifier(block_id) || !valid_identifier(occurrence_id))
        return fail(error, "Un identifiant est invalide.");
    BankProposedBlock *block = bank_analysis_find_block(result, block_id);
    gint index = block == NULL ? -1 : identifier_index(block->occurrence_identifiers, occurrence_id);
    if (index < 0) return fail(error, "Le bloc ou l’occurrence est introuvable.");
    g_ptr_array_remove_index(block->occurrence_identifiers, (guint)index);
    block->origin = BANK_ORIGIN_HUMAN; recompute_bounds(result, block); return TRUE;
}

gboolean bank_analysis_revision_move(BankAnalysisResult *result,
    const char *from_id, const char *to_id, const char *occurrence_id, GError **error)
{
    if (!valid_identifier(from_id) || !valid_identifier(to_id) ||
        !valid_identifier(occurrence_id))
        return fail(error, "Un identifiant est invalide.");
    BankProposedBlock *from = bank_analysis_find_block(result, from_id);
    BankProposedBlock *to = bank_analysis_find_block(result, to_id);
    gint source_index = from == NULL ? -1 : identifier_index(from->occurrence_identifiers, occurrence_id);
    if (source_index < 0 || to == NULL ||
        identifier_index(to->occurrence_identifiers, occurrence_id) >= 0 ||
        to->occurrence_identifiers->len >= BANK_ANALYSIS_MAX_ITEMS_PER_BLOCK)
        return fail(error, "Le déplacement demandé est invalide.");
    char *copy = g_strdup(occurrence_id);
    g_ptr_array_add(to->occurrence_identifiers, copy);
    g_ptr_array_remove_index(from->occurrence_identifiers, (guint)source_index);
    from->origin = to->origin = BANK_ORIGIN_HUMAN;
    recompute_bounds(result, from); recompute_bounds(result, to); return TRUE;
}

gboolean bank_analysis_revision_merge(BankAnalysisResult *result,
    const char *target_id, const char *source_id, GError **error)
{
    if (!valid_identifier(target_id) || !valid_identifier(source_id))
        return fail(error, "Un identifiant est invalide.");
    BankProposedBlock *target = bank_analysis_find_block(result, target_id);
    BankProposedBlock *source = bank_analysis_find_block(result, source_id);
    if (target == NULL || source == NULL || target == source)
        return fail(error, "La fusion demandée est invalide.");
    guint additions = 0;
    for (guint i = 0; i < source->occurrence_identifiers->len; i++)
        if (identifier_index(target->occurrence_identifiers,
                g_ptr_array_index(source->occurrence_identifiers, i)) < 0) additions++;
    if (target->occurrence_identifiers->len + additions > BANK_ANALYSIS_MAX_ITEMS_PER_BLOCK)
        return fail(error, "Le bloc fusionné dépasserait la limite.");
    GPtrArray *copies = g_ptr_array_new_with_free_func(g_free);
    for (guint i = 0; i < source->occurrence_identifiers->len; i++) {
        const char *id = g_ptr_array_index(source->occurrence_identifiers, i);
        if (identifier_index(target->occurrence_identifiers, id) < 0)
            g_ptr_array_add(copies, g_strdup(id));
    }
    for (guint i = 0; i < copies->len; i++)
        g_ptr_array_add(target->occurrence_identifiers, g_strdup(g_ptr_array_index(copies, i)));
    g_ptr_array_unref(copies); target->origin = BANK_ORIGIN_HUMAN;
    g_ptr_array_add(target->reasons, g_strdup("human-modification"));
    recompute_bounds(result, target);
    for (guint i = 0; i < result->blocks->len; i++)
        if (g_ptr_array_index(result->blocks, i) == source) {
            g_ptr_array_remove_index(result->blocks, i); break;
        }
    return TRUE;
}

gboolean bank_analysis_revision_split(BankAnalysisResult *result,
    const char *source_id, const char *new_id, const char *occurrence_id, GError **error)
{
    if (!valid_identifier(source_id) || !valid_identifier(new_id) ||
        !valid_identifier(occurrence_id))
        return fail(error, "Un identifiant est invalide.");
    BankProposedBlock *source = bank_analysis_find_block(result, source_id);
    if (source == NULL || identifier_index(source->occurrence_identifiers, occurrence_id) < 0)
        return fail(error, "La séparation demandée est invalide.");
    if (!bank_analysis_revision_create_block(result, new_id, occurrence_id, error)) return FALSE;
    return bank_analysis_revision_remove(result, source_id, occurrence_id, error);
}

gboolean bank_analysis_revision_mark_ambiguous(BankAnalysisResult *result,
    const char *block_id, gboolean ambiguous, GError **error)
{
    if (!valid_identifier(block_id)) return fail(error, "L’identifiant est invalide.");
    BankProposedBlock *block = bank_analysis_find_block(result, block_id);
    if (block == NULL) return fail(error, "Le bloc est introuvable.");
    block->ambiguous = ambiguous; block->origin = BANK_ORIGIN_HUMAN; return TRUE;
}

gboolean bank_analysis_revision_reject(BankAnalysisResult *result,
    const char *block_id, GError **error)
{
    if (!valid_identifier(block_id)) return fail(error, "L’identifiant est invalide.");
    BankProposedBlock *block = bank_analysis_find_block(result, block_id);
    if (block == NULL) return fail(error, "Le bloc est introuvable.");
    block->decision = BANK_DECISION_REJECTED; block->origin = BANK_ORIGIN_HUMAN;
    return TRUE;
}
