#ifndef LABFY_IDENTITY_OCR_DAO_H
#define LABFY_IDENTITY_OCR_DAO_H
#include "database/database.h"
#include "models/identity_ocr.h"
G_BEGIN_DECLS
typedef struct IdentityOcrDao IdentityOcrDao;
typedef struct {
 char *id,*evidence_id,*expected_sha256,*document_type,*document_side;
 char *engine,*engine_version,*requested_languages,*available_languages;
 char *parameters,*preprocessing_profile,*executed_at,*status,*error_message;
 char *text_relative_path,*text_sha256,*tsv_relative_path,*tsv_sha256;
 char *work_image_relative_path,*work_image_sha256; gint64 page_number;
 char *corrected_transcription,*transcription_corrected_at;
 char *transcription_origin; gboolean transcription_is_human;
} IdentityOcrRunRecord;
typedef struct {
 char *id,*person_id,*evidence_id,*ocr_run_id,*document_type;
 char *issuing_country_declared,*document_side,*review_state,*observed_at;
 char *factual_notes; gint64 page_number;
} IdentityDocumentObservationRecord;
typedef struct {
 char *id,*observation_id,*field_code,*raw_value,*corrected_value;
 char *normalized_value,*review_status,*origin,*evidence_id,*ocr_run_id;
 char *confirmed_value,*confirmation_state,*value_quality;
 char *reviewed_at,*review_note; double confidence; gboolean has_confidence;
 gint64 page_number,source_x,source_y,source_width,source_height;
 gint64 source_image_width,source_image_height,display_order;
 gboolean has_source_box;
} IdentityFieldObservationRecord;
IdentityOcrDao *identity_ocr_dao_new(Database *database);
void identity_ocr_dao_free(IdentityOcrDao *dao);
gboolean identity_ocr_dao_insert(IdentityOcrDao *dao,
 const char *person_id,const char *evidence_id,const IdentityOcrRun *run,
 const char *text_relative,const char *text_sha,
 const char *tsv_relative,const char *tsv_sha,const char *timestamp,
 GError **error);
IdentityOcrRunRecord *identity_ocr_dao_find_run(IdentityOcrDao *dao,
 const char *identifier,GError **error);
GPtrArray *identity_ocr_dao_list_runs_by_evidence(IdentityOcrDao *dao,
 const char *evidence_identifier,GError **error);
IdentityDocumentObservationRecord *identity_ocr_dao_find_document(
 IdentityOcrDao *dao,const char *identifier,GError **error);
GPtrArray *identity_ocr_dao_list_documents_by_person(IdentityOcrDao *dao,
 const char *person_identifier,GError **error);
GPtrArray *identity_ocr_dao_list_documents_by_evidence(IdentityOcrDao *dao,
 const char *evidence_identifier,GError **error);
IdentityFieldObservationRecord *identity_ocr_dao_find_field(
 IdentityOcrDao *dao,const char *identifier,GError **error);
GPtrArray *identity_ocr_dao_list_fields_by_document(IdentityOcrDao *dao,
 const char *document_identifier,GError **error);
GPtrArray *identity_ocr_dao_list_confirmed_fields(IdentityOcrDao *dao,
 const char *document_identifier,GError **error);
IdentityOcrRun *identity_ocr_dao_load_run(
 IdentityOcrDao *dao,const char *investigation_root,
 const char *run_identifier,char **person_identifier,GError **error);
gboolean identity_ocr_dao_update_review(
 IdentityOcrDao *dao,const IdentityOcrRun *run,
 const char *reviewed_at,GError **error);
void identity_ocr_run_record_free(IdentityOcrRunRecord *record);
void identity_document_observation_record_free(
 IdentityDocumentObservationRecord *record);
void identity_field_observation_record_free(
 IdentityFieldObservationRecord *record);
G_END_DECLS
#endif
