#ifndef LABFY_IDENTITY_OCR_H
#define LABFY_IDENTITY_OCR_H
#include <glib.h>
G_BEGIN_DECLS
typedef enum {
    IDENTITY_REVIEW_PROPOSED, IDENTITY_REVIEW_ACCEPTED,
    IDENTITY_REVIEW_MODIFIED, IDENTITY_REVIEW_REJECTED,
    IDENTITY_REVIEW_CONFLICT
} IdentityReviewStatus;
typedef struct {
    gint page, x, y, width, height, image_width, image_height;
    gboolean available;
} IdentitySourceBox;
typedef struct IdentityFieldObservation IdentityFieldObservation;
typedef struct IdentityOcrRun IdentityOcrRun;

IdentityFieldObservation *identity_field_observation_new(
    const char *code, const char *raw_value, double confidence,
    const IdentitySourceBox *box, guint order);
IdentityFieldObservation *identity_field_observation_new_manual(
    const char *code, const char *value, guint order);
IdentityFieldObservation *identity_field_observation_copy(
    const IdentityFieldObservation *field);
void identity_field_observation_free(IdentityFieldObservation *field);
gboolean identity_field_observation_accept(IdentityFieldObservation *field);
gboolean identity_field_observation_modify(IdentityFieldObservation *field,
    const char *corrected_value, const char *note);
gboolean identity_field_observation_restore_raw(
    IdentityFieldObservation *field);
void identity_field_observation_reject(IdentityFieldObservation *field);
gboolean identity_field_observation_set_origin(
    IdentityFieldObservation *field, const char *origin);
void identity_field_observation_mark_conflict(IdentityFieldObservation *field);
const char *identity_field_observation_get_code(
    const IdentityFieldObservation *field);
const char *identity_field_observation_get_raw_value(
    const IdentityFieldObservation *field);
const char *identity_field_observation_get_corrected_value(
    const IdentityFieldObservation *field);
IdentityReviewStatus identity_field_observation_get_status(
    const IdentityFieldObservation *field);
double identity_field_observation_get_confidence(
    const IdentityFieldObservation *field);
const IdentitySourceBox *identity_field_observation_get_box(
    const IdentityFieldObservation *field);

IdentityOcrRun *identity_ocr_run_new(const char *evidence_id,
    const char *expected_sha256, const char *document_type,
    const char *document_side, guint page, const char *languages,
    const char *profile);
IdentityOcrRun *identity_ocr_run_copy(const IdentityOcrRun *run);
void identity_ocr_run_free(IdentityOcrRun *run);
void identity_ocr_run_set_outputs(IdentityOcrRun *run, const char *version,
    const char *available_languages, const char *parameters,
    const char *raw_text, const char *tsv);
void identity_ocr_run_set_preview(IdentityOcrRun *run, GBytes *png_bytes);
GBytes *identity_ocr_run_get_preview(const IdentityOcrRun *run);
void identity_ocr_run_add_field(IdentityOcrRun *run,
    IdentityFieldObservation *field);
gboolean identity_ocr_run_replace_evidence_id(
    IdentityOcrRun *run, const char *evidence_identifier);
gboolean identity_ocr_run_replace_identifier(
    IdentityOcrRun *run, const char *identifier);
void identity_ocr_run_set_factual_notes(
    IdentityOcrRun *run, const char *notes);
gboolean identity_ocr_run_set_corrected_transcription(
    IdentityOcrRun *run, const char *transcription, const char *corrected_at);
void identity_ocr_run_reset_corrected_transcription(IdentityOcrRun *run);
const char *identity_ocr_run_get_corrected_transcription(
    const IdentityOcrRun *run);
const char *identity_ocr_run_get_transcription_corrected_at(
    const IdentityOcrRun *run);
gboolean identity_ocr_run_has_human_transcription(const IdentityOcrRun *run);
const char *identity_ocr_run_get_factual_notes(const IdentityOcrRun *run);
const char *identity_ocr_run_get_identifier(const IdentityOcrRun *run);
const char *identity_ocr_run_get_evidence_id(const IdentityOcrRun *run);
const char *identity_ocr_run_get_raw_text(const IdentityOcrRun *run);
const char *identity_ocr_run_get_tsv(const IdentityOcrRun *run);
const char *identity_ocr_run_get_document_type(const IdentityOcrRun *run);
const char *identity_ocr_run_get_document_side(const IdentityOcrRun *run);
const char *identity_ocr_run_get_languages(const IdentityOcrRun *run);
const char *identity_ocr_run_get_expected_sha256(const IdentityOcrRun *run);
const char *identity_ocr_run_get_profile(const IdentityOcrRun *run);
const char *identity_ocr_run_get_version(const IdentityOcrRun *run);
const char *identity_ocr_run_get_available_languages(const IdentityOcrRun *run);
const char *identity_field_observation_get_origin(
    const IdentityFieldObservation *field);
guint identity_field_observation_get_order(
    const IdentityFieldObservation *field);
guint identity_ocr_run_get_page(const IdentityOcrRun *run);
const GPtrArray *identity_ocr_run_get_fields(const IdentityOcrRun *run);
gboolean identity_ocr_document_type_is_valid(const char *value);
gboolean identity_ocr_document_side_is_valid(const char *value);
gboolean identity_ocr_field_code_is_valid(const char *value);
G_END_DECLS
#endif
