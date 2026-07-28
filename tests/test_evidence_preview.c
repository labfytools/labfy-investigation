#include "core/evidence_preview.h"
#include "core/file_hash.h"
#include <cairo.h>
#include <stdio.h>
#include <jpeglib.h>
#include "core/evidence_preview_task.h"
#include <glib/gstdio.h>

static void test_preview_png_and_guards(void)
{
    GError *error = NULL;
    char *root = g_dir_make_tmp("labfy-preview-test-XXXXXX", &error);
    char *path = g_build_filename(root, "SPECIMEN.png", NULL);
    cairo_surface_t *fixture = cairo_image_surface_create(
        CAIRO_FORMAT_RGB24, 32, 20);
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

static void write_jpeg(const char *path)
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
    while (compressor.next_scanline < compressor.image_height) {
        JSAMPROW rows[] = {row};
        jpeg_write_scanlines(&compressor, rows, 1);
    }
    jpeg_finish_compress(&compressor); jpeg_destroy_compress(&compressor);
    fclose(output);
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
        sha, "image/jpeg", 9);
    BackgroundTask *task = evidence_preview_task_start(manager, request,
        preview_done, &state, NULL, &error);
    g_assert_nonnull(task);
    g_main_loop_run(state.loop);
    g_assert_true(state.completed);
    EvidencePreviewResult *result = background_task_get_result(task);
    g_assert_nonnull(result);
    g_assert_cmpint(result->width, ==, 32);
    g_assert_cmpint(result->height, ==, 20);
    background_task_unref(task);
    evidence_preview_request_free(request);
    task_manager_free(manager); g_main_loop_unref(state.loop);
    g_assert_cmpint(g_remove(path), ==, 0); g_assert_cmpint(g_rmdir(root), ==, 0);
    g_free(sha); g_free(path); g_free(root);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/evidence-preview/png-and-guards",
        test_preview_png_and_guards);
    g_test_add_func("/evidence-preview/jpeg-async",
        test_preview_jpeg_async);
    return g_test_run();
}
