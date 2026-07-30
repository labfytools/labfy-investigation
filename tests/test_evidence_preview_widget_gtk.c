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

static gboolean mark_frame(GtkWidget *widget, GdkFrameClock *clock,
    gpointer data)
{
    gboolean *frame_seen = data;
    (void) widget;
    (void) clock;
    *frame_seen = TRUE;
    return G_SOURCE_REMOVE;
}

static void wait_for_frame(GtkWidget *widget)
{
    gboolean frame_seen = FALSE;
    gtk_widget_add_tick_callback(widget, mark_frame, &frame_seen, NULL);
    while (!frame_seen)
        g_main_context_iteration(NULL, TRUE);
}

static gboolean task_manager_has_running_task(TaskManager *manager)
{
    gsize count = task_manager_get_count(manager);
    for (gsize index = 0; index < count; index++) {
        BackgroundTask *task = task_manager_get_task(manager, index);
        BackgroundTaskState state = background_task_get_state(task);
        background_task_unref(task);
        if (state == BACKGROUND_TASK_STATE_PENDING ||
            state == BACKGROUND_TASK_STATE_RUNNING)
            return TRUE;
    }
    return FALSE;
}

static GtkWidget *find_named(GtkWidget *widget, const char *name)
{
    if (g_strcmp0(gtk_widget_get_name(widget), name) == 0) return widget;
    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child != NULL; child = gtk_widget_get_next_sibling(child)) {
        GtkWidget *found = find_named(child, name);
        if (found != NULL) return found;
    }
    return NULL;
}

static void click_named(EvidencePreviewWidget *widget, const char *name)
{
    GtkWidget *button = find_named(
        evidence_preview_widget_get_widget(widget), name);
    g_assert_nonnull(button);
    g_assert_true(gtk_widget_get_sensitive(button));
    g_signal_emit_by_name(button, "clicked");
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
    for (guint page = 1; page <= 3; page++) {
        char *label = g_strdup_printf("PAGE PDF %u SPECIMEN", page);
        cairo_set_source_rgb(cr, page == 1 ? 0.8 : 0.2,
            page == 2 ? 0.8 : 0.2, page == 3 ? 0.8 : 0.2);
        cairo_paint(cr);
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_move_to(cr, 30, 80);
        cairo_show_text(cr, label);
        cairo_show_page(cr);
        g_free(label);
    }
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
    g_assert_true(evidence_preview_widget_is_fit(widget));
    click_named(widget, "evidence-preview-zoom-in");
    g_assert_cmpfloat(evidence_preview_widget_get_zoom(widget), ==, 1.25);
    for (guint index = 0; index < 4; index++)
        click_named(widget, "evidence-preview-zoom-in");
    g_assert_cmpfloat(evidence_preview_widget_get_zoom(widget), ==, 4.0);
    wait_for_frame(GTK_WIDGET(window));
    GtkScrolledWindow *image_scroll = GTK_SCROLLED_WINDOW(find_named(
        evidence_preview_widget_get_widget(widget),
        "evidence-preview-image-scroll"));
    GtkAdjustment *horizontal =
        gtk_scrolled_window_get_hadjustment(image_scroll);
    GtkAdjustment *vertical =
        gtk_scrolled_window_get_vadjustment(image_scroll);
    g_assert_cmpfloat(gtk_adjustment_get_upper(horizontal), >,
        gtk_adjustment_get_page_size(horizontal));
    g_assert_cmpfloat(gtk_adjustment_get_upper(vertical), >,
        gtk_adjustment_get_page_size(vertical));
    gtk_adjustment_set_value(horizontal,
        gtk_adjustment_get_upper(horizontal) / 3.0);
    gtk_adjustment_set_value(vertical,
        gtk_adjustment_get_upper(vertical) / 3.0);
    g_assert_cmpfloat(gtk_adjustment_get_value(horizontal), >, 0.0);
    g_assert_cmpfloat(gtk_adjustment_get_value(vertical), >, 0.0);
    click_named(widget, "evidence-preview-zoom-out");
    g_assert_cmpfloat(evidence_preview_widget_get_zoom(widget), ==, 3.0);
    click_named(widget, "evidence-preview-fit");
    g_assert_true(evidence_preview_widget_is_fit(widget));
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
    g_assert_cmpuint(evidence_preview_widget_get_pdf_page_count(widget), ==, 3);
    g_assert_cmpuint(evidence_preview_widget_get_pdf_page(widget), ==, 0);
    GtkWidget *previous = find_named(
        evidence_preview_widget_get_widget(widget),
        "evidence-preview-previous-page");
    GtkWidget *next = find_named(
        evidence_preview_widget_get_widget(widget),
        "evidence-preview-next-page");
    GtkLabel *page_label = GTK_LABEL(find_named(
        evidence_preview_widget_get_widget(widget),
        "evidence-preview-page-label"));
    g_assert_false(gtk_widget_get_sensitive(previous));
    g_assert_true(gtk_widget_get_sensitive(next));
    g_assert_cmpstr(gtk_label_get_text(page_label), ==, "Page 1 / 3");
    click_named(widget, "evidence-preview-next-page");
    drain_until(widget, "pdf");
    g_assert_cmpuint(evidence_preview_widget_get_pdf_page(widget), ==, 1);
    g_assert_cmpstr(gtk_label_get_text(page_label), ==, "Page 2 / 3");
    click_named(widget, "evidence-preview-zoom-in");
    g_assert_cmpfloat(evidence_preview_widget_get_zoom(widget), ==, 1.25);
    click_named(widget, "evidence-preview-next-page");
    drain_until(widget, "pdf");
    g_assert_cmpuint(evidence_preview_widget_get_pdf_page(widget), ==, 2);
    g_assert_false(gtk_widget_get_sensitive(next));
    g_assert_cmpfloat(evidence_preview_widget_get_zoom(widget), ==, 1.25);
    click_named(widget, "evidence-preview-previous-page");
    click_named(widget, "evidence-preview-previous-page");
    drain_until(widget, "pdf");
    g_assert_cmpuint(evidence_preview_widget_get_pdf_page(widget), ==, 0);
    g_assert_false(gtk_widget_get_sensitive(previous));
    click_named(widget, "evidence-preview-next-page");
    click_named(widget, "evidence-preview-next-page");
    click_named(widget, "evidence-preview-previous-page");
    drain_until(widget, "pdf");
    g_assert_cmpuint(evidence_preview_widget_get_pdf_page(widget), ==, 1);
    gtk_window_set_default_size(window, 760, 560);
    gtk_paned_set_position(GTK_PANED(paned), 300);
    while (g_main_context_iteration(NULL, FALSE));
    g_assert_cmpint(gtk_paned_get_position(GTK_PANED(paned)), ==, 300);
    g_assert_true(gtk_widget_get_mapped(find_named(
        evidence_preview_widget_get_widget(widget),
        "evidence-preview-toolbar")));
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

static void test_pdf_twenty_open_close_cycles(void)
{
    GError *error = NULL;
    char *directory =
        g_dir_make_tmp("labfy-preview-cycles-XXXXXX", &error);
    g_assert_no_error(error);
    char *pdf = g_build_filename(directory, "SPECIMEN-multipage.pdf", NULL);
    write_pdf(pdf);
    TaskManager *manager = task_manager_new();
    EvidencePreviewRequest *request = request_for(
        directory, "SPECIMEN-multipage.pdf",
        "10000000-0000-4000-8000-000000000020", "application/pdf", 20);
    for (guint cycle = 0; cycle < 20; cycle++) {
        EvidencePreviewWidget *widget =
            evidence_preview_widget_new(manager, NULL, NULL);
        GtkWindow *window = GTK_WINDOW(gtk_window_new());
        gtk_window_set_default_size(window, 760, 560);
        gtk_window_set_child(window,
            evidence_preview_widget_get_widget(widget));
        gtk_window_present(window);
        evidence_preview_widget_show(widget, request, "PDF SPECIMEN");
        if ((cycle % 2U) == 0) {
            drain_until(widget, "pdf");
            click_named(widget, "evidence-preview-next-page");
        }
        gtk_window_destroy(window);
        evidence_preview_widget_cancel(widget);
        evidence_preview_widget_free(widget);
    }
    while (task_manager_has_running_task(manager))
        g_main_context_iteration(NULL, TRUE);
    while (g_main_context_iteration(NULL, FALSE));
    task_manager_free(manager);
    evidence_preview_request_free(request);
    g_unlink(pdf);
    g_rmdir(directory);
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
    g_test_add_func("/evidence-preview-widget/pdf-twenty-open-close-cycles",
        test_pdf_twenty_open_close_cycles);
    return g_test_run();
}
