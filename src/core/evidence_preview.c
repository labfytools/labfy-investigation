#include "core/evidence_preview.h"
#include "core/evidence_integrity_verifier.h"
#include <cairo.h>
#include <stdio.h>
#include <jpeglib.h>
#include <setjmp.h>

static GQuark preview_error(void)
{ return g_quark_from_static_string("evidence-preview-error"); }

typedef struct {
    struct jpeg_error_mgr base;
    jmp_buf jump;
} PreviewJpegError;
static void jpeg_failed(j_common_ptr info)
{
    PreviewJpegError *error = (PreviewJpegError *) info->err;
    longjmp(error->jump, 1);
}
static cairo_status_t write_png(void *closure,
    const unsigned char *data, unsigned int length)
{
    g_byte_array_append(closure, data, length);
    return CAIRO_STATUS_SUCCESS;
}
static gboolean surface_to_result(cairo_surface_t *source, gint source_width,
    gint source_height, const EvidencePreviewRequest *request,
    EvidencePreviewResult **out_result, GError **error)
{
    double scale = MIN(1.0, MIN(
        (double) EVIDENCE_PREVIEW_MAX_EDGE / source_width,
        (double) EVIDENCE_PREVIEW_MAX_EDGE / source_height));
    gint width = MAX(1, (gint) (source_width * scale));
    gint height = MAX(1, (gint) (source_height * scale));
    cairo_surface_t *target = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, width, height);
    cairo_t *cr = cairo_create(target);
    GByteArray *bytes = g_byte_array_new();
    cairo_scale(cr, scale, scale);
    cairo_set_source_surface(cr, source, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);
    if (cairo_surface_write_to_png_stream(target, write_png, bytes) !=
        CAIRO_STATUS_SUCCESS) {
        g_set_error_literal(error, preview_error(), 4,
            "Impossible de produire l'aperçu en mémoire.");
        g_byte_array_unref(bytes); cairo_surface_destroy(target);
        return FALSE;
    }
    cairo_surface_destroy(target);
    *out_result = g_new0(EvidencePreviewResult, 1);
    (*out_result)->evidence_identifier =
        g_strdup(request->evidence_identifier);
    (*out_result)->request_generation = request->request_generation;
    (*out_result)->width = width; (*out_result)->height = height;
    (*out_result)->png_bytes = g_byte_array_free_to_bytes(bytes);
    return TRUE;
}

EvidencePreviewRequest *evidence_preview_request_new(const char *root,
    const char *evidence_identifier, const char *relative_path,
    const char *sha256, const char *mime_type, guint64 generation)
{
    if (root == NULL || !g_uuid_string_is_valid(evidence_identifier) ||
        relative_path == NULL || sha256 == NULL || mime_type == NULL)
        return NULL;
    EvidencePreviewRequest *r = g_new0(EvidencePreviewRequest, 1);
    r->investigation_root_path = g_strdup(root);
    r->evidence_identifier = g_strdup(evidence_identifier);
    r->relative_path = g_strdup(relative_path);
    r->expected_sha256 = g_strdup(sha256);
    r->mime_type = g_strdup(mime_type);
    r->request_generation = generation;
    return r;
}
void evidence_preview_request_free(EvidencePreviewRequest *r)
{
    if (r == NULL) return;
    g_free(r->investigation_root_path); g_free(r->evidence_identifier);
    g_free(r->relative_path); g_free(r->expected_sha256);
    g_free(r->mime_type); g_free(r);
}
void evidence_preview_result_free(EvidencePreviewResult *r)
{
    if (r == NULL) return;
    g_free(r->evidence_identifier);
    g_clear_pointer(&r->png_bytes, g_bytes_unref);
    g_free(r);
}
gboolean evidence_preview_result_matches(const EvidencePreviewResult *r,
    const char *identifier, guint64 generation)
{
    return r != NULL && r->request_generation == generation &&
        g_strcmp0(r->evidence_identifier, identifier) == 0;
}

EvidencePreviewResult *evidence_preview_load(
    const EvidencePreviewRequest *request, GCancellable *cancellable,
    GError **error)
{
    EvidenceIntegrityVerificationResult *verification = NULL;
    EvidencePreviewResult *result = NULL;
    char *path = NULL;
    gint width = 0, height = 0;
    if (request == NULL ||
        !(g_str_equal(request->mime_type, "image/png") ||
          g_str_equal(request->mime_type, "image/jpeg"))) {
        g_set_error_literal(error, preview_error(), 1,
            "Aperçu indisponible pour ce format.");
        return NULL;
    }
    verification = evidence_integrity_verifier_verify(
        request->investigation_root_path, request->relative_path,
        request->expected_sha256, cancellable, error);
    if (verification == NULL) return NULL;
    if (evidence_integrity_verification_result_get_status(verification) !=
        EVIDENCE_INTEGRITY_STATUS_VALID) {
        g_set_error_literal(error, preview_error(), 2,
            "Aperçu refusé : l'intégrité de la preuve est invalide.");
        goto cleanup;
    }
    if (evidence_integrity_verification_result_get_size_bytes(verification) >
        EVIDENCE_PREVIEW_MAX_FILE_BYTES) {
        g_set_error_literal(error, preview_error(), 3,
            "L'image dépasse la taille maximale autorisée.");
        goto cleanup;
    }
    if (cancellable != NULL &&
        g_cancellable_set_error_if_cancelled(cancellable, error)) goto cleanup;
    path = g_build_filename(request->investigation_root_path,
        request->relative_path, NULL);
    if (g_str_equal(request->mime_type, "image/png")) {
        cairo_surface_t *surface = cairo_image_surface_create_from_png(path);
        if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
            cairo_surface_destroy(surface);
            g_set_error_literal(error, preview_error(), 4,
                "Le fichier image est illisible ou corrompu.");
            goto cleanup;
        }
        width = cairo_image_surface_get_width(surface);
        height = cairo_image_surface_get_height(surface);
        if (width > EVIDENCE_PREVIEW_MAX_DIMENSION ||
            height > EVIDENCE_PREVIEW_MAX_DIMENSION ||
            (guint64) width * (guint64) height > EVIDENCE_PREVIEW_MAX_PIXELS) {
            g_set_error_literal(error, preview_error(), 5,
                "Les dimensions de l'image dépassent les limites sûres.");
            cairo_surface_destroy(surface);
            goto cleanup;
        }
        surface_to_result(surface, width, height, request, &result, error);
        cairo_surface_destroy(surface);
        goto cleanup;
    }
    FILE *jpeg_file = fopen(path, "rb");
    struct jpeg_decompress_struct jpeg = {0};
    PreviewJpegError jpeg_error;
    cairo_surface_t *surface = NULL;
    if (jpeg_file == NULL) {
        g_set_error_literal(error, preview_error(), 4,
            "Le fichier image est illisible ou corrompu.");
        goto cleanup;
    }
    jpeg.err = jpeg_std_error(&jpeg_error.base);
    jpeg_error.base.error_exit = jpeg_failed;
    if (setjmp(jpeg_error.jump) != 0) {
        jpeg_destroy_decompress(&jpeg); fclose(jpeg_file);
        if (surface != NULL) cairo_surface_destroy(surface);
        g_set_error_literal(error, preview_error(), 4,
            "Le fichier JPEG est illisible ou corrompu.");
        goto cleanup;
    }
    jpeg_create_decompress(&jpeg); jpeg_stdio_src(&jpeg, jpeg_file);
    jpeg_read_header(&jpeg, TRUE);
    width = (gint) jpeg.image_width; height = (gint) jpeg.image_height;
    if (width > EVIDENCE_PREVIEW_MAX_DIMENSION ||
        height > EVIDENCE_PREVIEW_MAX_DIMENSION ||
        (guint64) width * (guint64) height > EVIDENCE_PREVIEW_MAX_PIXELS) {
        g_set_error_literal(error, preview_error(), 5,
            "Les dimensions de l'image dépassent les limites sûres.");
        jpeg_destroy_decompress(&jpeg); fclose(jpeg_file);
        goto cleanup;
    }
    jpeg.out_color_space = JCS_RGB;
    jpeg_start_decompress(&jpeg);
    surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
    unsigned char *pixels = cairo_image_surface_get_data(surface);
    gint stride = cairo_image_surface_get_stride(surface);
    JSAMPARRAY row = (*jpeg.mem->alloc_sarray)(
        (j_common_ptr) &jpeg, JPOOL_IMAGE, (JDIMENSION) width * 3U, 1);
    while (jpeg.output_scanline < jpeg.output_height) {
        jpeg_read_scanlines(&jpeg, row, 1);
        guint y = jpeg.output_scanline - 1;
        guint32 *destination = (guint32 *) (pixels + y * stride);
        for (gint x = 0; x < width; x++)
            destination[x] = ((guint32) row[0][x * 3] << 16) |
                ((guint32) row[0][x * 3 + 1] << 8) | row[0][x * 3 + 2];
        if (cancellable != NULL && g_cancellable_is_cancelled(cancellable))
            break;
    }
    cairo_surface_mark_dirty(surface);
    jpeg_finish_decompress(&jpeg); jpeg_destroy_decompress(&jpeg);
    fclose(jpeg_file);
    if (cancellable != NULL &&
        g_cancellable_set_error_if_cancelled(cancellable, error)) {
        cairo_surface_destroy(surface); goto cleanup;
    }
    surface_to_result(surface, width, height, request, &result, error);
    cairo_surface_destroy(surface);
cleanup:
    g_free(path);
    evidence_integrity_verification_result_free(verification);
    return result;
}
