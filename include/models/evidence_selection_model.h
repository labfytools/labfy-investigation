#ifndef LABFY_INVESTIGATION_EVIDENCE_SELECTION_MODEL_H
#define LABFY_INVESTIGATION_EVIDENCE_SELECTION_MODEL_H
#include "models/evidence_record.h"
#include <glib.h>
G_BEGIN_DECLS
typedef struct EvidenceSelectionModel EvidenceSelectionModel;
/** Crée un modèle possédant des copies profondes des preuves fournies. */
EvidenceSelectionModel *evidence_selection_model_new(const GPtrArray *records);
void evidence_selection_model_free(EvidenceSelectionModel *model);
void evidence_selection_model_set_query(EvidenceSelectionModel *model,
    const char *query);
void evidence_selection_model_set_type(EvidenceSelectionModel *model,
    const char *type_code);
GPtrArray *evidence_selection_model_list_visible(
    const EvidenceSelectionModel *model);
gboolean evidence_selection_model_select(EvidenceSelectionModel *model,
    const char *identifier);
const EvidenceRecord *evidence_selection_model_get_selected(
    const EvidenceSelectionModel *model);
GPtrArray *evidence_selection_model_list_type_codes(
    const EvidenceSelectionModel *model);
const EvidenceRecord *evidence_selection_model_find_by_sha256(
    const EvidenceSelectionModel *model, const char *sha256);
const EvidenceRecord *evidence_selection_model_find_by_identifier(
    const EvidenceSelectionModel *model, const char *identifier);
G_END_DECLS
#endif
