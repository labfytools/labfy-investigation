#include "widgets/evidence_preview_widget.h"
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

int main(int argc, char **argv)
{
    if (!gtk_init_check()) {
        g_print("SKIP: aucun affichage GTK disponible.\n");
        return 0;
    }
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/evidence-preview-widget/eml-lifecycle",
        test_widget_lifecycle_and_eml);
    return g_test_run();
}
