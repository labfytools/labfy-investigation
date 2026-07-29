#ifndef LABFY_IDENTITY_OCR_PREPROCESSOR_H
#define LABFY_IDENTITY_OCR_PREPROCESSOR_H
#include "core/evidence_preview.h"
G_BEGIN_DECLS
typedef enum { IDENTITY_OCR_PREPROCESS_NONE, IDENTITY_OCR_PREPROCESS_ORIENTATION,
 IDENTITY_OCR_PREPROCESS_GRAYSCALE, IDENTITY_OCR_PREPROCESS_UPSCALE
} IdentityOcrPreprocessProfile;
typedef struct { char *path; char *sha256; gint width; gint height;
 guint page; } IdentityOcrWorkImage;
IdentityOcrWorkImage *identity_ocr_preprocessor_prepare(
 const EvidencePreviewRequest *request, guint pdf_page,
 IdentityOcrPreprocessProfile profile, GCancellable *cancellable,
 GError **error);
void identity_ocr_work_image_free(IdentityOcrWorkImage *image);
gboolean identity_ocr_preprocessor_dimensions_are_safe(gint width,
 gint height, IdentityOcrPreprocessProfile profile);
G_END_DECLS
#endif
