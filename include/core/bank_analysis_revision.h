#ifndef LABFY_INVESTIGATION_BANK_ANALYSIS_REVISION_H
#define LABFY_INVESTIGATION_BANK_ANALYSIS_REVISION_H

#include "models/bank_analysis.h"

G_BEGIN_DECLS

/* Ces opérations ne modifient que les blocs en mémoire. Elles ne suppriment
 * jamais une occurrence ni ne modifient source_text/raw_value. */
gboolean bank_analysis_revision_create_block(BankAnalysisResult *result,
    const char *identifier, const char *occurrence_identifier, GError **error);
gboolean bank_analysis_revision_remove(BankAnalysisResult *result,
    const char *block_identifier, const char *occurrence_identifier, GError **error);
gboolean bank_analysis_revision_move(BankAnalysisResult *result,
    const char *from_block, const char *to_block, const char *occurrence, GError **error);
gboolean bank_analysis_revision_merge(BankAnalysisResult *result,
    const char *target_block, const char *source_block, GError **error);
gboolean bank_analysis_revision_split(BankAnalysisResult *result,
    const char *source_block, const char *new_block,
    const char *occurrence_identifier, GError **error);
gboolean bank_analysis_revision_mark_ambiguous(BankAnalysisResult *result,
    const char *block_identifier, gboolean ambiguous, GError **error);
gboolean bank_analysis_revision_reject(BankAnalysisResult *result,
    const char *block_identifier, GError **error);

G_END_DECLS
#endif
