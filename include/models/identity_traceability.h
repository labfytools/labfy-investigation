#ifndef LABFY_IDENTITY_TRACEABILITY_H
#define LABFY_IDENTITY_TRACEABILITY_H
#include <glib.h>
G_BEGIN_DECLS

typedef struct {
    char *identifier, *evidence_identifier, *ocr_run_identifier;
    char *status, *justification, *assessed_at;
    char *previous_identifier, *technical_note, *origin;
} DocumentAuthenticityAssessment;

typedef struct {
    char *identifier, *person_identifier, *evidence_identifier;
    char *ocr_run_identifier, *relation_type, *factual_note;
    char *observed_at, *origin;
    gboolean active;
} PersonEvidenceFactualRelation;

typedef struct {
    char *code, *label, *description;
    gint display_order;
    gboolean active, requires_justification, sensitive;
} PersonRoleVocabularyEntry;

typedef PersonRoleVocabularyEntry IdentificationStatusVocabularyEntry;

gboolean identity_traceability_authenticity_status_valid(const char *status);
gboolean identity_traceability_relation_type_valid(const char *type);
gboolean identity_traceability_identification_status_valid(const char *status);
gboolean identity_traceability_value_quality_valid(const char *quality);
gboolean identity_traceability_field_is_projectable(
    const char *review_status, const char *quality,
    const char *confirmation_state, const char *confirmed_value);

DocumentAuthenticityAssessment *document_authenticity_assessment_new(
    const char *identifier, const char *evidence_identifier,
    const char *ocr_run_identifier, const char *status,
    const char *justification, const char *assessed_at,
    const char *previous_identifier, const char *technical_note);
DocumentAuthenticityAssessment *document_authenticity_assessment_copy(
    const DocumentAuthenticityAssessment *assessment);
void document_authenticity_assessment_free(
    DocumentAuthenticityAssessment *assessment);
const char *document_authenticity_assessment_get_identifier(const DocumentAuthenticityAssessment *assessment);
const char *document_authenticity_assessment_get_evidence_identifier(const DocumentAuthenticityAssessment *assessment);
const char *document_authenticity_assessment_get_ocr_run_identifier(const DocumentAuthenticityAssessment *assessment);
const char *document_authenticity_assessment_get_status(const DocumentAuthenticityAssessment *assessment);
const char *document_authenticity_assessment_get_justification(const DocumentAuthenticityAssessment *assessment);
const char *document_authenticity_assessment_get_assessed_at(const DocumentAuthenticityAssessment *assessment);
const char *document_authenticity_assessment_get_previous_identifier(const DocumentAuthenticityAssessment *assessment);
const char *document_authenticity_assessment_get_technical_note(const DocumentAuthenticityAssessment *assessment);
const char *document_authenticity_assessment_get_origin(const DocumentAuthenticityAssessment *assessment);

PersonEvidenceFactualRelation *person_evidence_factual_relation_new(
    const char *identifier, const char *person_identifier,
    const char *evidence_identifier, const char *ocr_run_identifier,
    const char *relation_type, const char *factual_note,
    const char *observed_at, gboolean active);
PersonEvidenceFactualRelation *person_evidence_factual_relation_copy(
    const PersonEvidenceFactualRelation *relation);
void person_evidence_factual_relation_free(
    PersonEvidenceFactualRelation *relation);
const char *person_evidence_factual_relation_get_identifier(const PersonEvidenceFactualRelation *relation);
const char *person_evidence_factual_relation_get_person_identifier(const PersonEvidenceFactualRelation *relation);
const char *person_evidence_factual_relation_get_evidence_identifier(const PersonEvidenceFactualRelation *relation);
const char *person_evidence_factual_relation_get_ocr_run_identifier(const PersonEvidenceFactualRelation *relation);
const char *person_evidence_factual_relation_get_relation_type(const PersonEvidenceFactualRelation *relation);
const char *person_evidence_factual_relation_get_factual_note(const PersonEvidenceFactualRelation *relation);
const char *person_evidence_factual_relation_get_observed_at(const PersonEvidenceFactualRelation *relation);
const char *person_evidence_factual_relation_get_origin(const PersonEvidenceFactualRelation *relation);
gboolean person_evidence_factual_relation_get_active(const PersonEvidenceFactualRelation *relation);
void person_role_vocabulary_entry_free(PersonRoleVocabularyEntry *entry);
void identification_status_vocabulary_entry_free(
    IdentificationStatusVocabularyEntry *entry);
G_END_DECLS
#endif
