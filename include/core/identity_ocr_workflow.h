#ifndef LABFY_IDENTITY_OCR_WORKFLOW_H
#define LABFY_IDENTITY_OCR_WORKFLOW_H

#include "core/identity_ocr_preprocessor.h"
#include "models/identity_ocr.h"

G_BEGIN_DECLS

typedef struct {
    const char *root_path;
    const char *evidence_identifier;
    const char *relative_path;
    const char *expected_sha256;
    const char *mime_type;
    const char *document_type;
    const char *document_side;
    const char *languages;
    const char *preprocessing_profile;
    const char *tesseract_executable;
    const char *tesseract_version;
    guint page_number;
    IdentityOcrPreprocessProfile profile;
    guint64 generation;
} IdentityOcrWorkflowRequest;

gboolean identity_ocr_workflow_request_is_valid(
    const IdentityOcrWorkflowRequest *request);
IdentityOcrRun *identity_ocr_workflow_execute(
    const IdentityOcrWorkflowRequest *request,
    GCancellable *cancellable,
    GError **error);

G_END_DECLS

#endif
