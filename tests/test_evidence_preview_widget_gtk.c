#include "widgets/evidence_preview_widget.h"
#include "core/file_hash.h"
#include <cairo-pdf.h>
#include <glib/gstdio.h>

static void drain_until(EvidencePreviewWidget *widget, const char *state)
{
    gint64 deadline = g_get_monotonic_time() + 5 * G_TIME_SPAN_SECOND;
    while (g_strcmp0(evidence_preview_widget_get_state(widget), state) != 0 &&
           g_get_monotonic_time() < deadline)
        g_main_context_iteration(NULL, TRUE);
}

static void test_widget_lifecycle_and_eml(void)
{
    static const char eml[] =
        "From: alice@example.test\r\n"
        "To: bob@example.test\r\n"
        "Cc: copie@example.test\r\n"
        "Date: Tue, 28 Jul 2026 10:00:00 +0200\r\n"
        "Subject: SPECIMEN partagé\r\n"
        "Message-ID: <specimen@example.test>\r\n"
        "Content-Type: multipart/mixed; boundary=x\r\n\r\n"
        "--x\r\nContent-Type: text/plain; charset=utf-8\r\n\r\n"
        "Corps synthétique.\r\n"
        "--x\r\nContent-Type: text/plain; name=piece.txt\r\n"
        "Content-Disposition: attachment; filename=piece.txt\r\n\r\n"
        "contenu\r\n--x--\r\n";
    GError *error = NULL;
    char *directory = g_dir_make_tmp("labfy-preview-widget-XXXXXX", &error);
    char *path;
    char *sha;
    EvidencePreviewRequest *request;
    TaskManager *manager;
    EvidencePreviewWidget *widget;
    char *text;
    if (directory == NULL) {
        g_test_skip(error->message);
        g_clear_error(&error);
        return;
    }
    path = g_build_filename(directory, "specimen.eml", NULL);
    g_assert_true(g_file_set_contents(path, eml, -1, &error));
    g_assert_no_error(error);
    sha = g_compute_checksum_for_string(G_CHECKSUM_SHA256, eml, -1);
    manager = task_manager_new();
    widget = evidence_preview_widget_new(manager, NULL, NULL);
    g_assert_nonnull(widget);
    g_assert_cmpstr(evidence_preview_widget_get_state(widget), ==, "empty");
    request = evidence_preview_request_new(directory,
        "11111111-1111-4111-8111-111111111111",
        "specimen.eml", sha, NULL, 1);
    evidence_preview_widget_show(widget, request, "SPECIMEN");
    g_assert_cmpstr(evidence_preview_widget_get_state(widget), ==, "loading");
    drain_until(widget, "email");
    g_assert_cmpstr(evidence_preview_widget_get_state(widget), ==, "email");
    text = evidence_preview_widget_dup_text(widget);
    g_assert_nonnull(g_strstr_len(text, -1, "alice@example.test"));
    g_assert_nonnull(g_strstr_len(text, -1, "Corps synthétique."));
    g_assert_nonnull(g_strstr_len(text, -1, "piece.txt"));
    g_free(text);
    evidence_preview_widget_clear(widget);
    g_assert_cmpstr(evidence_preview_widget_get_state(widget), ==, "empty");
    evidence_preview_widget_cancel(widget);
    evidence_preview_widget_cancel(widget);
    evidence_preview_widget_free(widget);
    task_manager_free(manager);
    evidence_preview_request_free(request);
    g_remove(path);
    g_rmdir(directory);
    g_free(sha);
    g_free(path);
    g_free(directory);
}

static void write_image(const char *path, const char *format)
{
    GError *error = NULL;
    GdkPixbuf *pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
        640, 360);
    g_assert_nonnull(pixbuf);
    gdk_pixbuf_fill(pixbuf, 0x2f6fa8ff);
    g_assert_true(gdk_pixbuf_save(pixbuf, path, format, &error, NULL));
    g_assert_no_error(error);
    g_object_unref(pixbuf);
}

static void write_pdf(const char *path)
{
    cairo_surface_t *surface = cairo_pdf_surface_create(path, 640, 360);
    cairo_t *cr = cairo_create(surface);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_move_to(cr, 30, 80);
    cairo_show_text(cr, "PAGE PDF SPECIMEN");
    cairo_show_page(cr);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

static EvidencePreviewRequest *request_for(const char *directory,
    const char *name, const char *identifier, const char *mime,
    guint64 generation)
{
    GError *error = NULL;
    char *path = g_build_filename(directory, name, NULL);
    char *sha256 = NULL;
    guint64 size = 0;
    g_assert_true(file_hash_compute_sha256(
        path, NULL, &sha256, &size, &error));
    g_assert_no_error(error);
    EvidencePreviewRequest *request = evidence_preview_request_new(
        directory, identifier, name, sha256, mime, generation);
    g_free(sha256);
    g_free(path);
    return request;
}

static void test_paned_png_jpeg_pdf(void)
{
    GError *error = NULL;
    char *directory =
        g_dir_make_tmp("labfy-preview-formats-XXXXXX", &error);
    g_assert_no_error(error);
    char *png = g_build_filename(directory, "SPECIMEN.png", NULL);
    char *jpeg = g_build_filename(directory, "SPECIMEN.jpg", NULL);
    char *pdf = g_build_filename(directory, "SPECIMEN.pdf", NULL);
    write_image(png, "png");
    write_image(jpeg, "jpeg");
    write_pdf(pdf);
    TaskManager *manager = task_manager_new();
    EvidencePreviewWidget *widget =
        evidence_preview_widget_new(manager, NULL, NULL);
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *list = gtk_list_box_new();
    gtk_widget_set_size_request(list, 320, -1);
    gtk_list_box_append(GTK_LIST_BOX(list), gtk_label_new("SPECIMEN.png"));
    gtk_list_box_append(GTK_LIST_BOX(list), gtk_label_new("SPECIMEN.jpg"));
    gtk_list_box_append(GTK_LIST_BOX(list), gtk_label_new("SPECIMEN.pdf"));
    gtk_paned_set_start_child(GTK_PANED(paned), list);
    gtk_paned_set_end_child(GTK_PANED(paned),
        evidence_preview_widget_get_widget(widget));
    gtk_paned_set_position(GTK_PANED(paned), 380);
    gtk_paned_set_resize_start_child(GTK_PANED(paned), FALSE);
    gtk_paned_set_resize_end_child(GTK_PANED(paned), TRUE);
    GtkWindow *window = GTK_WINDOW(gtk_window_new());
    gtk_window_set_default_size(window, 1200, 800);
    gtk_window_set_child(window, paned);
    gtk_window_present(window);
    while (g_main_context_iteration(NULL, FALSE));
    g_assert_true(gtk_paned_get_start_child(GTK_PANED(paned)) == list);
    g_assert_true(gtk_paned_get_end_child(GTK_PANED(paned)) ==
        evidence_preview_widget_get_widget(widget));
    g_assert_cmpint(gtk_widget_get_width(
        evidence_preview_widget_get_widget(widget)), >,
        gtk_widget_get_width(list));
    EvidencePreviewRequest *png_request = request_for(
        directory, "SPECIMEN.png",
        "10000000-0000-4000-8000-000000000001", "image/png", 1);
    evidence_preview_widget_show(widget, png_request, "PNG SPECIMEN");
    drain_until(widget, "image");
    g_assert_cmpstr(evidence_preview_widget_get_state(widget), ==, "image");
    EvidencePreviewRequest *jpeg_request = request_for(
        directory, "SPECIMEN.jpg",
        "10000000-0000-4000-8000-000000000002", "image/jpeg", 2);
    evidence_preview_widget_show(widget, jpeg_request, "JPEG SPECIMEN");
    drain_until(widget, "image");
    g_assert_cmpstr(evidence_preview_widget_get_state(widget), ==, "image");
    EvidencePreviewRequest *pdf_request = request_for(
        directory, "SPECIMEN.pdf",
        "10000000-0000-4000-8000-000000000003", "application/pdf", 3);
    evidence_preview_widget_show(widget, pdf_request, "PDF SPECIMEN");
    drain_until(widget, "pdf");
    g_assert_cmpstr(evidence_preview_widget_get_state(widget), ==, "pdf");
    gtk_window_set_default_size(window, 760, 560);
    gtk_paned_set_position(GTK_PANED(paned), 300);
    while (g_main_context_iteration(NULL, FALSE));
    g_assert_cmpint(gtk_paned_get_position(GTK_PANED(paned)), ==, 300);
    evidence_preview_widget_show(widget, png_request, "résultat ancien");
    evidence_preview_widget_show(widget, jpeg_request, "résultat courant");
    drain_until(widget, "image");
    g_assert_cmpstr(evidence_preview_widget_get_state(widget), ==, "image");
    evidence_preview_widget_show(widget, pdf_request, "fermeture");
    gtk_window_destroy(window);
    evidence_preview_widget_cancel(widget);
    evidence_preview_widget_free(widget);
    task_manager_free(manager);
    evidence_preview_request_free(png_request);
    evidence_preview_request_free(jpeg_request);
    evidence_preview_request_free(pdf_request);
    g_unlink(png);
    g_unlink(jpeg);
    g_unlink(pdf);
    g_rmdir(directory);
    g_free(png);
    g_free(jpeg);
    g_free(pdf);
    g_free(directory);
}

int main(int argc, char **argv)
{
    if (!gtk_init_check()) {
        g_print("SKIP: aucun affichage GTK disponible.\n");
        return 0;
    }
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/evidence-preview-widget/eml-lifecycle",
        test_widget_lifecycle_and_eml);
    g_test_add_func("/evidence-preview-widget/paned-png-jpeg-pdf",
        test_paned_png_jpeg_pdf);
    return g_test_run();
}
