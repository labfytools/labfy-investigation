#ifndef LABFY_INVESTIGATION_PERSON_EVIDENCE_SELECTION_H
#define LABFY_INVESTIGATION_PERSON_EVIDENCE_SELECTION_H

#include "models/evidence_record.h"
#include <glib.h>

G_BEGIN_DECLS

typedef enum {
    PERSON_EVIDENCE_ORIGIN_EXISTING,
    PERSON_EVIDENCE_ORIGIN_STAGED,
    PERSON_EVIDENCE_ORIGIN_IMPORTED
} PersonEvidenceOrigin;

typedef enum {
    PERSON_EVIDENCE_STATE_PREPARING,
    PERSON_EVIDENCE_STATE_READY,
    PERSON_EVIDENCE_STATE_DUPLICATE,
    PERSON_EVIDENCE_STATE_INVALID,
    PERSON_EVIDENCE_STATE_ERROR,
    PERSON_EVIDENCE_STATE_CANCELLED
} PersonEvidenceState;

typedef struct PersonEvidenceSelection PersonEvidenceSelection;
typedef struct PersonEvidenceSelectionItem PersonEvidenceSelectionItem;

PersonEvidenceSelection *person_evidence_selection_new(void);
PersonEvidenceSelection *person_evidence_selection_copy(
    const PersonEvidenceSelection *selection);
void person_evidence_selection_free(PersonEvidenceSelection *selection);
guint person_evidence_selection_get_count(
    const PersonEvidenceSelection *selection);
const PersonEvidenceSelectionItem *person_evidence_selection_get(
    const PersonEvidenceSelection *selection, guint index);
const PersonEvidenceSelectionItem *person_evidence_selection_get_active(
    const PersonEvidenceSelection *selection);
gboolean person_evidence_selection_set_active(
    PersonEvidenceSelection *selection, const char *item_identifier);
gboolean person_evidence_selection_add_existing(
    PersonEvidenceSelection *selection, const EvidenceRecord *record,
    GError **error);
gboolean person_evidence_selection_add_staged(
    PersonEvidenceSelection *selection, const char *source_path,
    const char *staging_path, const char *original_name,
    const char *mime_type, const char *type_identifier, guint64 size_bytes,
    const char *sha256, const char *description, const char *prepared_at,
    GError **error);
gboolean person_evidence_selection_remove(PersonEvidenceSelection *selection,
    const char *item_identifier);
gboolean person_evidence_selection_set_type(PersonEvidenceSelection *selection,
    const char *item_identifier, const char *type_identifier);
gboolean person_evidence_selection_is_confirmable(
    const PersonEvidenceSelection *selection);
const char *person_evidence_selection_find_existing_by_sha256(
    const PersonEvidenceSelection *selection, const char *sha256);

const char *person_evidence_selection_item_get_identifier(
    const PersonEvidenceSelectionItem *item);
PersonEvidenceOrigin person_evidence_selection_item_get_origin(
    const PersonEvidenceSelectionItem *item);
PersonEvidenceState person_evidence_selection_item_get_state(
    const PersonEvidenceSelectionItem *item);
const EvidenceRecord *person_evidence_selection_item_get_record(
    const PersonEvidenceSelectionItem *item);
const char *person_evidence_selection_item_get_evidence_identifier(
    const PersonEvidenceSelectionItem *item);
const char *person_evidence_selection_item_get_source_path(
    const PersonEvidenceSelectionItem *item);
const char *person_evidence_selection_item_get_staging_path(
    const PersonEvidenceSelectionItem *item);
const char *person_evidence_selection_item_get_original_name(
    const PersonEvidenceSelectionItem *item);
const char *person_evidence_selection_item_get_mime_type(
    const PersonEvidenceSelectionItem *item);
const char *person_evidence_selection_item_get_type_identifier(
    const PersonEvidenceSelectionItem *item);
const char *person_evidence_selection_item_get_sha256(
    const PersonEvidenceSelectionItem *item);
const char *person_evidence_selection_item_get_description(
    const PersonEvidenceSelectionItem *item);
const char *person_evidence_selection_item_get_prepared_at(
    const PersonEvidenceSelectionItem *item);
guint64 person_evidence_selection_item_get_size_bytes(
    const PersonEvidenceSelectionItem *item);

G_END_DECLS
#endif
