#ifndef LABFY_INVESTIGATION_EVIDENCE_PREVIEW_H
#define LABFY_INVESTIGATION_EVIDENCE_PREVIEW_H
#include <gio/gio.h>
#include <glib.h>
G_BEGIN_DECLS
#define EVIDENCE_PREVIEW_MAX_FILE_BYTES (25U * 1024U * 1024U)
#define EVIDENCE_PREVIEW_MAX_DIMENSION 12000
#define EVIDENCE_PREVIEW_MAX_PIXELS 40000000U
#define EVIDENCE_PREVIEW_MAX_EDGE 1024
#define EVIDENCE_PREVIEW_MAX_TEXT_BYTES (1024U * 1024U)
typedef enum {
    EVIDENCE_PREVIEW_KIND_IMAGE,
    EVIDENCE_PREVIEW_KIND_VIDEO,
    EVIDENCE_PREVIEW_KIND_TEXT,
    EVIDENCE_PREVIEW_KIND_EMAIL,
    EVIDENCE_PREVIEW_KIND_PDF,
    EVIDENCE_PREVIEW_KIND_UNSUPPORTED,
    EVIDENCE_PREVIEW_KIND_ERROR
} EvidencePreviewKind;
typedef struct {
    char *investigation_root_path;
    char *evidence_identifier;
    char *relative_path;
    char *expected_sha256;
    char *mime_type;
    guint64 request_generation;
    guint pdf_page;
} EvidencePreviewRequest;
typedef struct {
    EvidencePreviewKind kind;
    char *evidence_identifier;
    guint64 request_generation;
    GBytes *png_bytes;
    gint width;
    gint height;
    char *effective_mime_type;
    char *effective_format;
    char *display_name;
    char *controlled_path;
    char *text;
    char *message;
    guint64 size_bytes;
    guint item_count;
    guint current_page;
    gboolean integrity_valid;
    gboolean preview_available;
    gboolean truncated;
} EvidencePreviewResult;
EvidencePreviewRequest *evidence_preview_request_new(const char *root,
    const char *evidence_identifier, const char *relative_path,
    const char *sha256, const char *mime_type, guint64 generation);
void evidence_preview_request_set_pdf_page(
    EvidencePreviewRequest *request, guint page);
void evidence_preview_request_free(EvidencePreviewRequest *request);
EvidencePreviewResult *evidence_preview_load(
    const EvidencePreviewRequest *request, GCancellable *cancellable,
    GError **error);
void evidence_preview_result_free(EvidencePreviewResult *result);
gboolean evidence_preview_result_matches(const EvidencePreviewResult *result,
    const char *evidence_identifier, guint64 generation);
gboolean evidence_preview_dimensions_are_safe(gint width, gint height);
G_END_DECLS
#endif
