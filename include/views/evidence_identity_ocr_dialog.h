#ifndef LABFY_EVIDENCE_IDENTITY_OCR_DIALOG_H
#define LABFY_EVIDENCE_IDENTITY_OCR_DIALOG_H

#include "core/tool_registry.h"
#include "core/background_task.h"
#include "models/entity_record.h"
#include "models/identity_ocr.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct EvidenceIdentityOcrDialogResult
    EvidenceIdentityOcrDialogResult;
typedef gboolean (*EvidenceIdentityOcrDialogSessionCheck)(gpointer user_data);
typedef void (*EvidenceIdentityOcrDialogCallback)(
    EvidenceIdentityOcrDialogResult *result, gpointer user_data);

gboolean evidence_identity_ocr_dialog_file_is_compatible(
    const char *file_path);
gboolean evidence_identity_ocr_dialog_present(
    GtkWindow *parent, const char *file_path, const GPtrArray *persons,
    const char *preselected_person_identifier,
    const ToolInfo *tesseract_tool,
    EvidenceIdentityOcrDialogSessionCheck session_check,
    EvidenceIdentityOcrDialogCallback callback,
    gpointer user_data, GDestroyNotify user_data_destroy,
    GError **error);
gboolean evidence_identity_ocr_dialog_present_review(
    GtkWindow *parent,const char *file_path,const GPtrArray *persons,
    const char *preselected_person_identifier,
    const IdentityOcrRun *persisted_run,
    EvidenceIdentityOcrDialogSessionCheck session_check,
    EvidenceIdentityOcrDialogCallback callback,
    gpointer user_data,GDestroyNotify user_data_destroy,GError **error);
void evidence_identity_ocr_dialog_result_free(
    EvidenceIdentityOcrDialogResult *result);
gboolean evidence_identity_ocr_dialog_result_has_ocr(
    const EvidenceIdentityOcrDialogResult *result);
const char *evidence_identity_ocr_dialog_result_get_person_identifier(
    const EvidenceIdentityOcrDialogResult *result);
const char *evidence_identity_ocr_dialog_result_get_temporary_identifier(
    const EvidenceIdentityOcrDialogResult *result);
const char *evidence_identity_ocr_dialog_result_get_sha256(
    const EvidenceIdentityOcrDialogResult *result);
guint64 evidence_identity_ocr_dialog_result_get_size(
    const EvidenceIdentityOcrDialogResult *result);
const char *evidence_identity_ocr_dialog_result_get_mime_type(
    const EvidenceIdentityOcrDialogResult *result);
IdentityOcrRun *evidence_identity_ocr_dialog_result_steal_run(
    EvidenceIdentityOcrDialogResult *result);
GtkWindow *evidence_identity_ocr_dialog_result_get_dialog(
    const EvidenceIdentityOcrDialogResult *result);
void evidence_identity_ocr_dialog_finish_import(
    GtkWindow *dialog, const GError *error);
void evidence_identity_ocr_dialog_set_submission_task(
    GtkWindow *dialog, BackgroundTask *task);

G_END_DECLS

#endif
