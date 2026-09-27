#include "core/evidence_preview.h"
#include "core/file_hash.h"
#include <cairo.h>
#include <cairo-pdf.h>
#include <stdio.h>
#include <jpeglib.h>
#include <libheif/heif.h>
#include "core/evidence_preview_task.h"
#include <glib/gstdio.h>

static const char *const preview_identifier =
    "10000000-0000-4000-8000-000000000109";

static void test_preview_png_and_guards(void)
{
    GError *error = NULL;
    char *root = g_dir_make_tmp("labfy-preview-test-XXXXXX", &error);
    char *path = g_build_filename(root, "SPECIMEN.png", NULL);
    /* CONTRACT: even a valid imported image is rendered with a long edge no
     * greater than the preview budget. */
    cairo_surface_t *fixture = cairo_image_surface_create(
        CAIRO_FORMAT_RGB24, 2048, 20);
    cairo_t *cr = cairo_create(fixture);
    char *sha256 = NULL;
    guint64 size_bytes = 0;
    EvidencePreviewRequest *request;
    EvidencePreviewResult *result;
    g_assert_nonnull(root);
    cairo_set_source_rgb(cr, 0.2, 0.4, 0.6);
    cairo_paint(cr);
    g_assert_cmpint(cairo_surface_write_to_png(fixture, path), ==,
        CAIRO_STATUS_SUCCESS);
    g_assert_true(file_hash_compute_sha256(
        path, NULL, &sha256, &size_bytes, &error));
    request = evidence_preview_request_new(root,
        "10000000-0000-4000-8000-000000000109", "SPECIMEN.png",
        sha256, "image/png", 7);
    result = evidence_preview_load(request, NULL, &error);
    g_assert_no_error(error);
    g_assert_nonnull(result);
    g_assert_cmpstr(result->effective_mime_type, ==, "image/png");
    g_assert_cmpint(result->width, ==, EVIDENCE_PREVIEW_MAX_EDGE);
    g_assert_cmpint(result->height, ==, 10);
    g_assert_true(evidence_preview_result_matches(result,
        "10000000-0000-4000-8000-000000000109", 7));
    g_assert_false(evidence_preview_result_matches(result,
        "10000000-0000-4000-8000-000000000109", 8));
    evidence_preview_result_free(result);
    g_free(request->expected_sha256);
    request->expected_sha256 = g_strdup(
        "0000000000000000000000000000000000000000000000000000000000000000");
    result = evidence_preview_load(request, NULL, &error);
    g_assert_null(result);
    g_assert_error(error, g_quark_from_static_string(
        "evidence-preview-error"), 2);
    g_clear_error(&error);
    evidence_preview_request_free(request);
    cairo_destroy(cr);
    cairo_surface_destroy(fixture);
    g_assert_cmpint(g_remove(path), ==, 0);
    g_assert_cmpint(g_rmdir(root), ==, 0);
    g_free(sha256); g_free(path); g_free(root);
}

static void write_jpeg_oriented(const char *path, guint8 orientation)
{
    struct jpeg_compress_struct compressor;
    struct jpeg_error_mgr errors;
    FILE *output = fopen(path, "wb");
    JSAMPLE row[3 * 32];
    g_assert_nonnull(output);
    memset(row, 0x80, sizeof(row));
    compressor.err = jpeg_std_error(&errors);
    jpeg_create_compress(&compressor);
    jpeg_stdio_dest(&compressor, output);
    compressor.image_width = 32; compressor.image_height = 20;
    compressor.input_components = 3; compressor.in_color_space = JCS_RGB;
    jpeg_set_defaults(&compressor); jpeg_start_compress(&compressor, TRUE);
    if (orientation != 0) {
        static const guint8 exif_prefix[] = {
            'E','x','i','f',0,0,'I','I',42,0,8,0,0,0,1,0,
            0x12,0x01,3,0,1,0,0,0};
        guint8 exif[32] = {0};
        memcpy(exif, exif_prefix, sizeof(exif_prefix));
        exif[24] = orientation;
        jpeg_write_marker(&compressor, JPEG_APP0 + 1, exif, sizeof(exif));
    }
    while (compressor.next_scanline < compressor.image_height) {
        JSAMPROW rows[] = {row};
        jpeg_write_scanlines(&compressor, rows, 1);
    }
    jpeg_finish_compress(&compressor); jpeg_destroy_compress(&compressor);
    fclose(output);
}

static void write_jpeg(const char *path)
{
    write_jpeg_oriented(path, 0);
}

static EvidencePreviewRequest *request_with_current_hash(const char *root,
    const char *relative_path, guint64 generation)
{
    GError *error = NULL;
    char *path = g_build_filename(root, relative_path, NULL);
    char *sha256 = NULL;
    guint64 size = 0;
    g_assert_true(file_hash_compute_sha256(
        path, NULL, &sha256, &size, &error));
    g_assert_no_error(error);
    EvidencePreviewRequest *request = evidence_preview_request_new(root,
        preview_identifier, relative_path, sha256, NULL, generation);
    g_free(sha256);
    g_free(path);
    return request;
}

static void test_preview_controlled_path_and_integrity(void)
{
    GError *error = NULL;
    char *root = g_dir_make_tmp("labfy-preview-integrity-XXXXXX", &error);
    char *path = g_build_filename(root, "SPECIMEN.txt", NULL);
    char *link_path = g_build_filename(root, "SPECIMEN-link.txt", NULL);
    const char *valid = "SPECIMEN intact\n";
    g_assert_no_error(error);
    g_assert_true(g_file_set_contents(path, valid, -1, &error));
    g_assert_null(evidence_preview_request_new(root, preview_identifier,
        "../SPECIMEN.txt", "00", NULL, 1));
    g_assert_null(evidence_preview_request_new(root, preview_identifier,
        "/tmp/SPECIMEN.txt", "00", NULL, 1));

    EvidencePreviewRequest *request = request_with_current_hash(
        root, "SPECIMEN.txt", 2);
    g_assert_true(g_file_set_contents(path, "SPECIMEN altéré\n", -1, &error));
    g_assert_null(evidence_preview_load(request, NULL, &error));
    g_assert_nonnull(error);
    g_clear_error(&error);
    evidence_preview_request_free(request);

    request = evidence_preview_request_new(root, preview_identifier,
        "absent.txt",
        "0000000000000000000000000000000000000000000000000000000000000000",
        NULL, 3);
    g_assert_null(evidence_preview_load(request, NULL, &error));
    g_assert_nonnull(error);
    g_clear_error(&error);
    evidence_preview_request_free(request);

    GFile *link = g_file_new_for_path(link_path);
    g_assert_true(g_file_make_symbolic_link(link, path, NULL, &error));
    g_assert_no_error(error);
    g_object_unref(link);
    char *target_sha256 = NULL;
    guint64 target_size = 0;
    g_assert_true(file_hash_compute_sha256(
        path, NULL, &target_sha256, &target_size, &error));
    request = evidence_preview_request_new(root, preview_identifier,
        "SPECIMEN-link.txt", target_sha256, NULL, 4);
    g_free(target_sha256);
    g_assert_null(evidence_preview_load(request, NULL, &error));
    g_assert_nonnull(error);
    g_clear_error(&error);
    evidence_preview_request_free(request);
    g_assert_cmpint(g_remove(link_path), ==, 0);
    g_assert_cmpint(g_remove(path), ==, 0);
    g_assert_cmpint(g_rmdir(root), ==, 0);
    g_free(link_path);
    g_free(path);
    g_free(root);
}

static void test_preview_hostile_image_headers(void)
{
    GError *error = NULL;
    char *root = g_dir_make_tmp("labfy-preview-image-guards-XXXXXX", &error);
    static const guint8 truncated_jpeg[] = {0xff, 0xd8, 0xff, 0xe1, 0, 16};
    static const guint8 oversized_png[] = {
        0x89,'P','N','G','\r','\n',0x1a,'\n',
        0,0,0,13,'I','H','D','R',0,0,0x30,0,0,0,0x30,0,
        8,2,0,0,0};
    const struct {
        const char *name;
        const guint8 *data;
        gsize size;
    } fixtures[] = {
        {"truncated.jpg", truncated_jpeg, sizeof(truncated_jpeg)},
        {"oversized.png", oversized_png, sizeof(oversized_png)}
    };
    for (guint i = 0; i < G_N_ELEMENTS(fixtures); i++) {
        char *path = g_build_filename(root, fixtures[i].name, NULL);
        g_assert_true(g_file_set_contents(path,
            (const char *) fixtures[i].data, fixtures[i].size, &error));
        EvidencePreviewRequest *request = request_with_current_hash(
            root, fixtures[i].name, 10 + i);
        g_assert_null(evidence_preview_load(request, NULL, &error));
        g_assert_nonnull(error);
        g_clear_error(&error);
        evidence_preview_request_free(request);
        g_assert_cmpint(g_remove(path), ==, 0);
        g_free(path);
    }
    g_assert_cmpint(g_rmdir(root), ==, 0);
    g_free(root);
}

static void test_preview_jpeg_orientation_and_edge(void)
{
    GError *error = NULL;
    char *root = g_dir_make_tmp("labfy-preview-orientation-XXXXXX", &error);
    char *path = g_build_filename(root, "oriented.jpg", NULL);
    write_jpeg_oriented(path, 6);
    EvidencePreviewRequest *request = request_with_current_hash(
        root, "oriented.jpg", 20);
    EvidencePreviewResult *result = evidence_preview_load(request, NULL, &error);
    g_assert_no_error(error);
    g_assert_nonnull(result);
    g_assert_cmpint(result->width, ==, 20);
    g_assert_cmpint(result->height, ==, 32);
    g_assert_cmpint(MAX(result->width, result->height), <=,
        EVIDENCE_PREVIEW_MAX_EDGE);
    evidence_preview_result_free(result);
    evidence_preview_request_free(request);
    g_assert_cmpint(g_remove(path), ==, 0);
    g_assert_cmpint(g_rmdir(root), ==, 0);
    g_free(path);
    g_free(root);
}

typedef struct { GMainLoop *loop; gboolean completed; } AsyncState;
static void preview_done(BackgroundTask *task, gpointer data)
{
    AsyncState *state = data;
    state->completed = background_task_get_state(task) ==
        BACKGROUND_TASK_STATE_COMPLETED;
    g_main_loop_quit(state->loop);
}
static void test_preview_jpeg_async(void)
{
    GError *error = NULL;
    char *root = g_dir_make_tmp("labfy-preview-jpeg-XXXXXX", &error);
    char *path = g_build_filename(root, "SPECIMEN.jpg", NULL);
    char *sha = NULL; guint64 size = 0;
    TaskManager *manager = task_manager_new();
    AsyncState state = {.loop = g_main_loop_new(NULL, FALSE)};
    write_jpeg(path);
    g_assert_true(file_hash_compute_sha256(path, NULL, &sha, &size, &error));
    EvidencePreviewRequest *request = evidence_preview_request_new(root,
        "10000000-0000-4000-8000-000000000110", "SPECIMEN.jpg",
        sha, NULL, 9);
    BackgroundTask *task = evidence_preview_task_start(manager, request,
        preview_done, &state, NULL, &error);
    g_assert_nonnull(task);
    g_main_loop_run(state.loop);
    g_assert_true(state.completed);
    EvidencePreviewResult *result = background_task_get_result(task);
    g_assert_nonnull(result);
    g_assert_cmpint(result->width, ==, 32);
    g_assert_cmpint(result->height, ==, 20);
    g_assert_cmpstr(result->effective_mime_type, ==, "image/jpeg");
    background_task_unref(task);
    evidence_preview_request_free(request);
    task_manager_free(manager); g_main_loop_unref(state.loop);
    g_assert_cmpint(g_remove(path), ==, 0); g_assert_cmpint(g_rmdir(root), ==, 0);
    g_free(sha); g_free(path); g_free(root);
}

static void test_preview_rejects_false_jpeg(void)
{
    GError *error = NULL;
    char *root = g_dir_make_tmp("labfy-preview-invalid-XXXXXX", &error);
    char *path = g_build_filename(root, "SPECIMEN.jpg", NULL);
    char *sha = NULL;
    guint64 size = 0;
    EvidencePreviewRequest *request;
    g_assert_true(g_file_set_contents(path, "SPECIMEN non image", -1,
        &error));
    g_assert_true(file_hash_compute_sha256(path, NULL, &sha, &size, &error));
    request = evidence_preview_request_new(root,
        "10000000-0000-4000-8000-000000000111", "SPECIMEN.jpg",
        sha, "image/jpeg", 10);
    EvidencePreviewResult *result =
        evidence_preview_load(request, NULL, &error);
    g_assert_no_error(error);
    g_assert_nonnull(result);
    g_assert_cmpint(result->kind, ==, EVIDENCE_PREVIEW_KIND_UNSUPPORTED);
    evidence_preview_result_free(result);
    evidence_preview_request_free(request);
    g_assert_cmpint(g_remove(path), ==, 0);
    g_assert_cmpint(g_rmdir(root), ==, 0);
    g_free(sha);
    g_free(path);
    g_free(root);
}

static EvidencePreviewResult *load_fixture(const char *root,
    const char *name, const guint8 *data, gsize length, const char *mime)
{
    GError *error = NULL;
    char *path = g_build_filename(root, name, NULL);
    char *sha = NULL; guint64 size = 0;
    g_assert_true(g_file_set_contents(path, (const char *) data,
        (gssize) length, &error));
    g_assert_true(file_hash_compute_sha256(path, NULL, &sha, &size, &error));
    EvidencePreviewRequest *request = evidence_preview_request_new(root,
        "10000000-0000-4000-8000-000000000112", name, sha, mime, 11);
    EvidencePreviewResult *result = evidence_preview_load(request, NULL, &error);
    g_assert_no_error(error); g_assert_nonnull(result);
    evidence_preview_request_free(request); g_free(sha); g_free(path);
    return result;
}

static void test_preview_dispatch_text_video_email(void)
{
    GError *error = NULL;
    char *root = g_dir_make_tmp("labfy-preview-dispatch-XXXXXX", &error);
    static const guint8 text[] = "SPECIMEN,évidence\n1,contrôlée\n";
    static const guint8 mp4[] = {
        0,0,0,24,'f','t','y','p','i','s','o','m',0,0,0,0,
        'i','s','o','m','m','p','4','2'};
    static const guint8 mov[] = {
        0,0,0,20,'f','t','y','p','q','t',' ',' ',0,0,0,0,'q','t',' ',' '};
    static const guint8 eml[] =
        "From: Alice SPECIMEN <alice@example.com>\r\n"
        "To: Bob SPECIMEN <bob@example.com>\r\n"
        "Date: Tue, 28 Jul 2026 10:00:00 +0200\r\n"
        "Subject: SPECIMEN\r\nMessage-ID: <specimen@example.com>\r\n"
        "Content-Type: text/plain; charset=UTF-8\r\n\r\n"
        "Corps SPECIMEN sans ressource distante.\r\n";
    EvidencePreviewResult *result = load_fixture(root, "SPECIMEN.csv",
        text, sizeof(text) - 1, NULL);
    g_assert_cmpint(result->kind, ==, EVIDENCE_PREVIEW_KIND_TEXT);
    g_assert_nonnull(strstr(result->text, "évidence"));
    evidence_preview_result_free(result);
    result = load_fixture(root, "SPECIMEN.bin", mp4, sizeof(mp4), "text/plain");
    g_assert_cmpint(result->kind, ==, EVIDENCE_PREVIEW_KIND_VIDEO);
    g_assert_cmpstr(result->effective_mime_type, ==, "video/mp4");
    evidence_preview_result_free(result);
    result = load_fixture(root, "SPECIMEN.data", mov, sizeof(mov), NULL);
    g_assert_cmpint(result->kind, ==, EVIDENCE_PREVIEW_KIND_VIDEO);
    g_assert_cmpstr(result->effective_mime_type, ==, "video/quicktime");
    evidence_preview_result_free(result);
    result = load_fixture(root, "SPECIMEN.eml", eml, sizeof(eml) - 1, NULL);
    g_assert_cmpint(result->kind, ==, EVIDENCE_PREVIEW_KIND_EMAIL);
    g_assert_nonnull(strstr(result->text, "Alice SPECIMEN"));
    evidence_preview_result_free(result);
    const char *const fixture_names[] = {
        "SPECIMEN.csv", "SPECIMEN.bin", "SPECIMEN.data", "SPECIMEN.eml"
    };
    for (guint i = 0; i < G_N_ELEMENTS(fixture_names); i++) {
        char *path = g_build_filename(root, fixture_names[i], NULL);
        g_assert_cmpint(g_remove(path), ==, 0);
        g_free(path);
    }
    g_assert_cmpint(g_rmdir(root), ==, 0); g_free(root);
}

static void test_preview_pdf_first_page(void)
{
    GError *error = NULL;
    char *root = g_dir_make_tmp("labfy-preview-pdf-XXXXXX", &error);
    char *path = g_build_filename(root, "SPECIMEN.pdf", NULL);
    cairo_surface_t *surface = cairo_pdf_surface_create(path, 320, 200);
    cairo_t *cr = cairo_create(surface);
    cairo_set_source_rgb(cr, 0.2, 0.4, 0.6);
    cairo_paint(cr);
    cairo_show_page(cr);
    cairo_destroy(cr); cairo_surface_destroy(surface);
    char *contents = NULL; gsize length = 0;
    g_assert_true(g_file_get_contents(path, &contents, &length, &error));
    EvidencePreviewResult *result = load_fixture(root, "SPECIMEN.pdf",
        (const guint8 *) contents, length, "application/octet-stream");
    g_assert_cmpint(result->kind, ==, EVIDENCE_PREVIEW_KIND_PDF);
    g_assert_nonnull(result->png_bytes); g_assert_cmpuint(result->item_count, ==, 1);
    evidence_preview_result_free(result);
    g_free(contents); g_assert_cmpint(g_remove(path), ==, 0);
    g_assert_cmpint(g_rmdir(root), ==, 0); g_free(path); g_free(root);
}

static void test_preview_pdf_multiple_pages_and_limits(void)
{
    GError *error = NULL;
    char *root = g_dir_make_tmp("labfy-preview-pdf-pages-XXXXXX", &error);
    char *path = g_build_filename(root, "SPECIMEN-3-pages.pdf", NULL);
    cairo_surface_t *surface = cairo_pdf_surface_create(path, 320, 200);
    cairo_t *cr = cairo_create(surface);
    for (guint page = 1; page <= 3; page++) {
        cairo_set_source_rgb(cr, page / 3.0, 0.2, 0.4);
        cairo_paint(cr);
        cairo_show_page(cr);
    }
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    char *sha256 = NULL;
    guint64 size = 0;
    g_assert_true(file_hash_compute_sha256(
        path, NULL, &sha256, &size, &error));
    g_assert_no_error(error);
    EvidencePreviewRequest *request = evidence_preview_request_new(
        root, "10000000-0000-4000-8000-000000000120",
        "SPECIMEN-3-pages.pdf", sha256, "application/pdf", 20);
    EvidencePreviewResult *first =
        evidence_preview_load(request, NULL, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(first->item_count, ==, 3);
    g_assert_cmpuint(first->current_page, ==, 1);
    evidence_preview_result_free(first);
    evidence_preview_request_set_pdf_page(request, 1);
    EvidencePreviewResult *second =
        evidence_preview_load(request, NULL, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(second->current_page, ==, 2);
    evidence_preview_result_free(second);
    evidence_preview_request_set_pdf_page(request, 2);
    EvidencePreviewResult *last =
        evidence_preview_load(request, NULL, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(last->current_page, ==, 3);
    evidence_preview_result_free(last);
    evidence_preview_request_set_pdf_page(request, 3);
    g_assert_null(evidence_preview_load(request, NULL, &error));
    g_assert_error(error,
        g_quark_from_static_string("evidence-preview-error"), 8);
    g_clear_error(&error);
    evidence_preview_request_free(request);
    g_free(sha256);
    g_assert_cmpint(g_remove(path), ==, 0);
    g_assert_cmpint(g_rmdir(root), ==, 0);
    g_free(path);
    g_free(root);
}

static void test_preview_pdf_size_limit_and_cancellation(void)
{
    GError *error = NULL;
    char *root = g_dir_make_tmp("labfy-preview-pdf-guard-XXXXXX", &error);
    char *oversized_path = g_build_filename(root, "SPECIMEN-large.pdf", NULL);
    const gsize oversized_size = 8U * 1024U * 1024U + 1U;
    char *oversized = g_malloc0(oversized_size);
    memcpy(oversized, "%PDF-1.7\n", 9);
    g_assert_true(g_file_set_contents(oversized_path, oversized,
        oversized_size, &error));
    EvidencePreviewRequest *request = request_with_current_hash(root,
        "SPECIMEN-large.pdf", 31);
    g_assert_null(evidence_preview_load(request, NULL, &error));
    g_assert_error(error,
        g_quark_from_static_string("evidence-preview-error"), 9);
    g_clear_error(&error);
    evidence_preview_request_free(request);
    g_free(oversized);
    g_assert_cmpint(g_remove(oversized_path), ==, 0);
    g_free(oversized_path);

    char *pdf_path = g_build_filename(root, "SPECIMEN-cancel.pdf", NULL);
    cairo_surface_t *surface = cairo_pdf_surface_create(pdf_path, 320, 200);
    cairo_t *cr = cairo_create(surface);
    cairo_set_source_rgb(cr, 0.2, 0.4, 0.6);
    cairo_paint(cr);
    cairo_destroy(cr); cairo_surface_destroy(surface);
    request = request_with_current_hash(root, "SPECIMEN-cancel.pdf", 32);
    GCancellable *cancellable = g_cancellable_new();
    g_cancellable_cancel(cancellable);
    g_assert_null(evidence_preview_load(request, cancellable, &error));
    /* CONTRACT: cancellation never publishes a partially rendered PDF. */
    g_assert_nonnull(error);
    g_clear_error(&error);
    g_object_unref(cancellable); evidence_preview_request_free(request);
    g_assert_cmpint(g_remove(pdf_path), ==, 0);
    g_free(pdf_path);
    g_assert_cmpint(g_rmdir(root), ==, 0);
    g_free(root);
}

static gboolean write_heic_specimen(const char *path, gint width, gint height)
{
    struct heif_context *context = heif_context_alloc();
    struct heif_encoder *encoder = NULL;
    struct heif_image *image = NULL;
    struct heif_image_handle *handle = NULL;
    struct heif_error status = heif_context_get_encoder_for_format(
        context, heif_compression_HEVC, &encoder);
    if (status.code != heif_error_Ok) {
        heif_context_free(context); return FALSE;
    }
    status = heif_image_create(width, height, heif_colorspace_RGB,
        heif_chroma_interleaved_RGB, &image);
    if (status.code == heif_error_Ok)
        status = heif_image_add_plane(image, heif_channel_interleaved,
            width, height, 8);
    if (status.code == heif_error_Ok) {
        gint stride = 0;
        guint8 *pixels = heif_image_get_plane(image,
            heif_channel_interleaved, &stride);
        for (gint y = 0; y < height; y++)
            for (gint x = 0; x < width; x++) {
                pixels[y * stride + x * 3] = (guint8) (x % 255);
                pixels[y * stride + x * 3 + 1] = (guint8) (y % 255);
                pixels[y * stride + x * 3 + 2] = 96;
            }
        status = heif_context_encode_image(context, image, encoder,
            NULL, &handle);
    }
    if (status.code == heif_error_Ok)
        status = heif_context_write_to_file(context, path);
    if (handle != NULL) heif_image_handle_release(handle);
    if (image != NULL) heif_image_release(image);
    heif_encoder_release(encoder); heif_context_free(context);
    return status.code == heif_error_Ok;
}

static void test_preview_real_heic(void)
{
    GError *error = NULL;
    char *root = g_dir_make_tmp("labfy-preview-heic-XXXXXX", &error);
    char *path = g_build_filename(root, "SPECIMEN", NULL);
    if (!write_heic_specimen(path, 48, 32)) {
        g_test_skip("Aucun encodeur HEVC libheif disponible.");
        g_free(path); g_rmdir(root); g_free(root); return;
    }
    char *contents = NULL; gsize length = 0;
    g_assert_true(g_file_get_contents(path, &contents, &length, &error));
    EvidencePreviewResult *result = load_fixture(root, "SPECIMEN",
        (const guint8 *) contents, length, NULL);
    g_assert_cmpint(result->kind, ==, EVIDENCE_PREVIEW_KIND_IMAGE);
    g_assert_cmpstr(result->effective_mime_type, ==, "image/heic");
    g_assert_cmpint(result->width, ==, 48);
    g_assert_cmpint(result->height, ==, 32);
    g_assert_cmpuint(result->item_count, ==, 1);
    evidence_preview_result_free(result);
    for (gsize offset = 0; offset + 4 <= length; offset++)
        if (memcmp(contents + offset, "heic", 4) == 0)
            memcpy(contents + offset, "mif1", 4);
    result = load_fixture(root, "SPECIMEN.heif",
        (const guint8 *) contents, length, NULL);
    g_assert_cmpint(result->kind, ==, EVIDENCE_PREVIEW_KIND_IMAGE);
    g_assert_cmpstr(result->effective_mime_type, ==, "image/heif");
    evidence_preview_result_free(result);
    char *heif_path = g_build_filename(root, "SPECIMEN.heif", NULL);
    g_assert_cmpint(g_remove(heif_path), ==, 0); g_free(heif_path);
    g_free(contents); g_assert_cmpint(g_remove(path), ==, 0);
    g_assert_cmpint(g_rmdir(root), ==, 0); g_free(path); g_free(root);
}

static void test_preview_heic_cancel_and_invalid(void)
{
    GError *error = NULL;
    char *root = g_dir_make_tmp("labfy-preview-heic-invalid-XXXXXX", &error);
    static const guint8 invalid[] = {
        0,0,0,20,'f','t','y','p','h','e','i','c',0,0,0,0,'m','i','f','1'};
    char *path = g_build_filename(root, "SPECIMEN.heic", NULL);
    g_assert_true(g_file_set_contents(path, (const char *) invalid,
        sizeof(invalid), &error));
    char *sha = NULL; guint64 size = 0;
    g_assert_true(file_hash_compute_sha256(path, NULL, &sha, &size, &error));
    EvidencePreviewRequest *request = evidence_preview_request_new(root,
        "10000000-0000-4000-8000-000000000113", "SPECIMEN.heic",
        sha, NULL, 12);
    g_assert_null(evidence_preview_load(request, NULL, &error));
    g_assert_nonnull(error); g_clear_error(&error);
    GCancellable *cancellable = g_cancellable_new();
    g_cancellable_cancel(cancellable);
    g_assert_null(evidence_preview_load(request, cancellable, &error));
    g_assert_nonnull(error);
    g_clear_error(&error); g_object_unref(cancellable);
    evidence_preview_request_free(request); g_free(sha);
    g_assert_cmpint(g_remove(path), ==, 0);
    g_assert_cmpint(g_rmdir(root), ==, 0); g_free(path); g_free(root);
}

static void test_preview_dimension_limits_without_allocation(void)
{
    g_assert_true(evidence_preview_dimensions_are_safe(8000, 5000));
    g_assert_false(evidence_preview_dimensions_are_safe(8001, 5000));
    g_assert_false(evidence_preview_dimensions_are_safe(12001, 1));
    g_assert_false(evidence_preview_dimensions_are_safe(1, 12001));
    g_assert_false(evidence_preview_dimensions_are_safe(0, 100));
}

static void test_preview_eml_multipart_inventory(void)
{
    GError *error = NULL;
    char *root = g_dir_make_tmp("labfy-preview-eml-XXXXXX", &error);
    static const guint8 eml[] =
        "From: Alice SPECIMEN <alice@example.com>\r\n"
        "To: Bob <bob@example.com>, Eve <eve@example.com>\r\n"
        "Subject: =?UTF-8?Q?Sujet_=C3=A9tude_SPECIMEN?=\r\n"
        "Date: Tue, 28 Jul 2026 10:00:00 +0200\r\n"
        "Message-ID: <nested@example.com>\r\nMIME-Version: 1.0\r\n"
        "Content-Type: multipart/mixed; boundary=outer\r\n\r\n"
        "--outer\r\nContent-Type: multipart/alternative; boundary=inner\r\n\r\n"
        "--inner\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n"
        "<style>secret{}</style><p>HTML distant<img src=\"https://example.invalid/pixel\"></p>\r\n"
        "--inner\r\nContent-Type: text/plain; charset=UTF-8\r\n"
        "Content-Transfer-Encoding: quoted-printable\r\n\r\n"
        "Corps=20pr=C3=A9f=C3=A9r=C3=A9=20SPECIMEN\r\n--inner--\r\n"
        "--outer\r\nContent-Type: text/plain\r\n"
        "Content-Disposition: attachment; filename*0*=UTF-8''rapport%20;"
        " filename*1*=synth%C3%A9tique.txt\r\n"
        "Content-Transfer-Encoding: base64\r\n\r\nU1BFQ0lNRU4=\r\n"
        "--outer\r\nContent-Type: image/png; name=\"inline.png\"\r\n"
        "Content-Disposition: inline\r\nContent-ID: <specimen-inline@example.com>\r\n"
        "Content-Transfer-Encoding: base64\r\n\r\nUE5H\r\n--outer--\r\n";
    EvidencePreviewResult *result = load_fixture(root, "SPECIMEN.eml",
        eml, sizeof(eml) - 1, "message/rfc822");
    g_assert_cmpint(result->kind, ==, EVIDENCE_PREVIEW_KIND_EMAIL);
    g_assert_cmpuint(result->item_count, ==, 2);
    g_assert_nonnull(strstr(result->text, "Corps préféré SPECIMEN"));
    g_assert_nonnull(strstr(result->text, "rapport synthétique.txt"));
    g_assert_nonnull(strstr(result->text, "inline.png"));
    g_assert_nonnull(strstr(result->text, "Content-ID: specimen-inline@example.com"));
    g_assert_null(strstr(result->text, "--outer"));
    evidence_preview_result_free(result);
    char *path = g_build_filename(root, "SPECIMEN.eml", NULL);
    g_assert_cmpint(g_remove(path), ==, 0);
    g_assert_cmpint(g_rmdir(root), ==, 0); g_free(path); g_free(root);
}

static void test_preview_eml_inert_and_bounded(void)
{
    GError *error = NULL;
    char *root = g_dir_make_tmp("labfy-preview-eml-hostile-XXXXXX", &error);
    static const guint8 hostile[] =
        "From: Mallory SPECIMEN <mallory@example.invalid>\r\n"
        "To: Analyste <analyste@example.invalid>\r\n"
        "Subject: HTML hostile SPECIMEN\r\n"
        "Date: Tue, 28 Jul 2026 10:00:00 +0200\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n\r\n"
        "<script>window.location='https://example.invalid/collect'</script>"
        "<img src=\"https://example.invalid/pixel\"><p>Texte sûr</p>";
    EvidencePreviewResult *result = load_fixture(root, "hostile.eml",
        hostile, sizeof(hostile) - 1, "message/rfc822");
    g_assert_cmpint(result->kind, ==, EVIDENCE_PREVIEW_KIND_EMAIL);
    g_assert_true(result->preview_available);
    g_assert_null(result->png_bytes);
    g_assert_cmpuint(strlen(result->text), <=,
        EVIDENCE_PREVIEW_MAX_TEXT_BYTES);
    g_assert_null(strstr(result->text, "<script"));
    g_assert_null(strstr(result->text, "<img"));
    evidence_preview_result_free(result);

    gsize oversized_size = EVIDENCE_PREVIEW_MAX_TEXT_BYTES + 1U;
    char *oversized = g_malloc0(oversized_size);
    const char *prefix =
        "From: Alice <alice@example.invalid>\n"
        "Subject: SPECIMEN volumineux\n\n";
    memcpy(oversized, prefix, strlen(prefix));
    memset(oversized + strlen(prefix), 'A', oversized_size - strlen(prefix));
    result = load_fixture(root, "oversized.eml",
        (const guint8 *) oversized, oversized_size, "message/rfc822");
    g_assert_cmpint(result->kind, ==, EVIDENCE_PREVIEW_KIND_EMAIL);
    g_assert_false(result->preview_available);
    g_assert_null(result->text);
    g_assert_true(result->truncated);
    evidence_preview_result_free(result);
    g_free(oversized);

    char *hostile_path = g_build_filename(root, "hostile.eml", NULL);
    char *oversized_path = g_build_filename(root, "oversized.eml", NULL);
    g_assert_cmpint(g_remove(hostile_path), ==, 0);
    g_assert_cmpint(g_remove(oversized_path), ==, 0);
    g_assert_cmpint(g_rmdir(root), ==, 0);
    g_free(oversized_path);
    g_free(hostile_path);
    g_free(root);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/evidence-preview/png-and-guards",
        test_preview_png_and_guards);
    g_test_add_func("/evidence-preview/jpeg-async",
        test_preview_jpeg_async);
    g_test_add_func("/evidence-preview/rejects-false-jpeg",
        test_preview_rejects_false_jpeg);
    g_test_add_func("/evidence-preview/dispatch-text-video-email",
        test_preview_dispatch_text_video_email);
    g_test_add_func("/evidence-preview/pdf-first-page",
        test_preview_pdf_first_page);
    g_test_add_func("/evidence-preview/pdf-multiple-pages-limits",
        test_preview_pdf_multiple_pages_and_limits);
    g_test_add_func("/evidence-preview/pdf-size-limit-and-cancellation",
        test_preview_pdf_size_limit_and_cancellation);
    g_test_add_func("/evidence-preview/real-heic",
        test_preview_real_heic);
    g_test_add_func("/evidence-preview/heic-cancel-invalid",
        test_preview_heic_cancel_and_invalid);
    g_test_add_func("/evidence-preview/eml-multipart-inventory",
        test_preview_eml_multipart_inventory);
    g_test_add_func("/evidence-preview/controlled-path-integrity",
        test_preview_controlled_path_and_integrity);
    g_test_add_func("/evidence-preview/hostile-image-headers",
        test_preview_hostile_image_headers);
    g_test_add_func("/evidence-preview/jpeg-orientation-edge",
        test_preview_jpeg_orientation_and_edge);
    g_test_add_func("/evidence-preview/eml-inert-bounded",
        test_preview_eml_inert_and_bounded);
    g_test_add_func("/evidence-preview/dimension-limits",
        test_preview_dimension_limits_without_allocation);
    return g_test_run();
}
