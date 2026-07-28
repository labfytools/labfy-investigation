/******************************************************************************
 * @file test_eml_pipeline_task.c
 * @brief Tests unitaires du pipeline asynchrone d'analyse EML et pièces jointes.
 ******************************************************************************/
#include "core/eml_pipeline_task.h"
#include <gio/gio.h>
#include <glib.h>
#include <glib/gstdio.h>

static void test_eml_pipeline_basic(void)
{
    char *tmp_dir = g_dir_make_tmp("labfy-eml-test-XXXXXX", NULL);
    g_assert_nonnull(tmp_dir);

    char *eml_path = g_build_filename(tmp_dir, "test.eml", NULL);
    const char *eml_content =
        "From: Alice <alice@example.com>\r\n"
        "To: Bob <bob@example.com>\r\n"
        "Subject: Rib suspect\r\n"
        "Date: Thu, 23 Jul 2026 12:00:00 +0200\r\n"
        "MIME-Version: 1.0\r\n"
        "Content-Type: multipart/mixed; boundary=\"BOUNDARY123\"\r\n"
        "\r\n"
        "--BOUNDARY123\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "\r\n"
        "Veuillez trouver ci-joint mon RIB.\r\n"
        "--BOUNDARY123\r\n"
        "Content-Type: text/plain; name=\"../rib_suspect.txt\"\r\n"
        "Content-Disposition: attachment; filename=\"../rib_suspect.txt\"\r\n"
        "\r\n"
        "FR4830002005500000000000052\r\n"
        "--BOUNDARY123\r\n"
        "Content-Type: text/plain; name=\"../rib_suspect.txt\"\r\n"
        "Content-Disposition: attachment; filename=\"../rib_suspect.txt\"\r\n"
        "Content-Transfer-Encoding: quoted-printable\r\n"
        "\r\n"
        "Deuxi=C3=A8me RIB\r\n"
        "--BOUNDARY123--\r\n";

    g_file_set_contents(eml_path, eml_content, -1, NULL);

    char *processed_dir = g_build_filename(tmp_dir, "02_Preuves_Traitees", NULL);
    g_mkdir_with_parents(processed_dir, 0755);

    BackgroundTask *task = eml_pipeline_task_new(eml_path, processed_dir, "evidence-uuid-1");
    g_assert_nonnull(task);

    g_assert_cmpint(background_task_get_state(task), !=,
        BACKGROUND_TASK_STATE_FAILED);

    /* Attente de la fin de la tâche */
    while (background_task_get_state(task) == BACKGROUND_TASK_STATE_RUNNING ||
           background_task_get_state(task) == BACKGROUND_TASK_STATE_PENDING)
    {
        g_main_context_iteration(NULL, TRUE);
    }

    g_assert_cmpint(background_task_get_state(task), ==, BACKGROUND_TASK_STATE_COMPLETED);

    EmlPipelineResult *result = background_task_get_result(task);
    g_assert_nonnull(result);
    g_assert_nonnull(result->analysis);
    g_assert_nonnull(result->mime_result);

    /* Vérification de la protection contre les traversées de chemin "../" */
    g_assert_cmpuint(result->mime_result->attachments->len, ==, 2);
    EmlAttachment *att = g_ptr_array_index(result->mime_result->attachments, 0);
    g_assert_cmpstr(att->sanitized_filename, ==, "___rib_suspect.txt");
    att = g_ptr_array_index(result->mime_result->attachments, 1);
    g_assert_cmpstr(att->sanitized_filename, ==, "___rib_suspect-2.txt");
    g_assert_true(g_str_has_suffix(att->extracted_path,
        "/___rib_suspect-2.txt"));
    g_assert_cmpuint(att->decoded_size, >, 0U);

    /* Vérification de la détection de la proposition bancaire dans la pièce jointe */
    g_assert_cmpuint(result->bank_proposals->len, ==, 1);
    BankProposal *bp = g_ptr_array_index(result->bank_proposals, 0);
    g_assert_cmpstr(bp->normalized_iban, ==, "FR4830002005500000000000052");
    g_assert_true(bp->is_iban_valid);

    background_task_unref(task);
    g_remove(eml_path);
    g_free(eml_path);
    g_free(processed_dir);
    g_free(tmp_dir);
}

static void test_eml_pipeline_document_analysis(void)
{
    GError *error = NULL;
    char *tmp_dir = g_dir_make_tmp("labfy-eml-document-XXXXXX", &error);
    g_assert_no_error(error);
    char *eml_path = g_build_filename(tmp_dir, "document.eml", NULL);
    char *processed_dir = g_build_filename(
        tmp_dir, "02_Preuves_Traitees", NULL);
    static const char eml[] =
        "From: synthetic@example.test\r\n"
        "Content-Type: multipart/mixed; boundary=x\r\n\r\n"
        "--x\r\nContent-Type: image/png; name=synthetic.png\r\n"
        "Content-Disposition: attachment; filename=synthetic.png\r\n"
        "Content-Transfer-Encoding: base64\r\n\r\n"
        "UE5H\r\n--x--\r\n";
    g_assert_true(g_file_set_contents(eml_path, eml, -1, &error));
    g_assert_no_error(error);
    DocumentAnalysisTools tools = {
        .exiftool = "tests/fake_document_tool",
        .tesseract = "tests/fake_document_tool",
        .pdfinfo = "tests/fake_document_tool",
        .pdftotext = "tests/fake_document_tool",
        .pdftoppm = "tests/fake_document_tool"
    };
    BackgroundTask *task = eml_pipeline_task_new_with_tools(
        eml_path, processed_dir, "synthetic-evidence", &tools);
    g_assert_nonnull(task);
    while (background_task_get_state(task) ==
               BACKGROUND_TASK_STATE_RUNNING ||
           background_task_get_state(task) ==
               BACKGROUND_TASK_STATE_PENDING)
        g_main_context_iteration(NULL, TRUE);
    g_assert_cmpint(background_task_get_state(task), ==,
        BACKGROUND_TASK_STATE_COMPLETED);
    EmlPipelineResult *result = background_task_get_result(task);
    g_assert_nonnull(result);
    g_assert_cmpuint(result->document_analyses->len, ==, 1);
    DocumentFileAnalysis *document = g_ptr_array_index(
        result->document_analyses, 0);
    g_assert_nonnull(document->metadata);
    g_assert_nonnull(document->ocr);
    g_assert_cmpstr(document->ocr->text, ==,
        "Texte OCR synthétique page une.\n");
    background_task_unref(task);
    g_remove(eml_path);
    g_free(processed_dir);
    g_free(eml_path);
    g_free(tmp_dir);
}

static void wait_for_task(BackgroundTask *task)
{
    while (background_task_get_state(task) ==
               BACKGROUND_TASK_STATE_RUNNING ||
           background_task_get_state(task) ==
               BACKGROUND_TASK_STATE_PENDING)
        g_main_context_iteration(NULL, TRUE);
}

static void remove_tree(const char *path)
{
    GFile *directory = g_file_new_for_path(path);
    GFileEnumerator *enumerator = g_file_enumerate_children(directory,
        G_FILE_ATTRIBUTE_STANDARD_NAME "," G_FILE_ATTRIBUTE_STANDARD_TYPE,
        G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, NULL);
    if (enumerator != NULL)
    {
        GFileInfo *info = NULL;
        while ((info = g_file_enumerator_next_file(
                    enumerator, NULL, NULL)) != NULL)
        {
            GFile *child = g_file_get_child(directory,
                g_file_info_get_name(info));
            char *child_path = g_file_get_path(child);
            if (g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY)
                remove_tree(child_path);
            else
                g_assert_true(g_file_delete(child, NULL, NULL));
            g_free(child_path);
            g_object_unref(child);
            g_object_unref(info);
        }
        g_object_unref(enumerator);
    }
    g_assert_true(g_file_delete(directory, NULL, NULL));
    g_object_unref(directory);
}

static DocumentAnalysisTools synthetic_tools(void)
{
    DocumentAnalysisTools tools = {
        .exiftool = "tests/fake_document_tool",
        .tesseract = "tests/fake_document_tool",
        .pdfinfo = "tests/fake_document_tool",
        .pdftotext = "tests/fake_document_tool",
        .pdftoppm = "tests/fake_document_tool"
    };
    return tools;
}

typedef struct
{
    gboolean called;
    BackgroundTaskState state;
} PipelineCompletion;

static void pipeline_completed(BackgroundTask *task, gpointer user_data)
{
    PipelineCompletion *completion = user_data;
    completion->called = TRUE;
    completion->state = background_task_get_state(task);
}

static void test_manual_fixture_async_start(void)
{
    GError *error = NULL;
    char *source_before = NULL;
    gsize source_length = 0;
    g_assert_true(g_file_get_contents(
        "tests/fixtures/eml/manual_smoke_test.eml",
        &source_before, &source_length, &error));
    g_assert_no_error(error);
    char *staging = g_dir_make_tmp("labfy-eml-manual-XXXXXX", &error);
    g_assert_no_error(error);
    DocumentAnalysisTools tools = synthetic_tools();
    PipelineCompletion completion = { 0 };
    BackgroundTask *task = eml_pipeline_task_start(
        "tests/fixtures/eml/manual_smoke_test.eml", staging,
        "synthetic-evidence", &tools, pipeline_completed,
        &completion, NULL);
    g_assert_nonnull(task);
    wait_for_task(task);
    while (!completion.called)
        g_main_context_iteration(NULL, TRUE);
    g_assert_cmpint(completion.state, ==,
        BACKGROUND_TASK_STATE_COMPLETED);
    EmlPipelineResult *result = background_task_get_result(task);
    g_assert_nonnull(result);
    g_assert_cmpuint(result->mime_result->attachments->len, ==, 2);
    g_assert_cmpuint(result->bank_proposals->len, ==, 1);
    BankProposal *bank = g_ptr_array_index(result->bank_proposals, 0);
    g_assert_false(bank->is_iban_valid);
    g_assert_cmpstr(bank->normalized_iban, ==,
        "FR0000000000000000000000000");
    g_assert_cmpuint(
        eml_analysis_get_header_values(result->analysis, "received")->len,
        ==, 2);
    char *source_after = NULL;
    gsize after_length = 0;
    g_assert_true(g_file_get_contents(
        "tests/fixtures/eml/manual_smoke_test.eml",
        &source_after, &after_length, &error));
    g_assert_no_error(error);
    g_assert_cmpmem(source_after, after_length, source_before, source_length);
    g_free(source_after);
    g_free(source_before);
    background_task_unref(task);
    remove_tree(staging);
    g_free(staging);
}

static void test_eml_pipeline_pdf_end_to_end_and_limit(void)
{
    GError *error = NULL;
    char *tmp_dir = g_dir_make_tmp("labfy-eml-pdf-XXXXXX", &error);
    g_assert_no_error(error);
    char *eml_path = g_build_filename(tmp_dir, "pdf.eml", NULL);
    char *processed_dir = g_build_filename(tmp_dir, "processed", NULL);
    static const char eml[] =
        "From: synthetic@example.test\r\n"
        "MIME-Version: 1.0\r\n"
        "Content-Type: multipart/mixed; boundary=pdf-boundary\r\n\r\n"
        "--pdf-boundary\r\n"
        "Content-Type: application/pdf; name=scan.pdf\r\n"
        "Content-Disposition: attachment; filename=scan.pdf\r\n"
        "Content-Transfer-Encoding: base64\r\n\r\n"
        "JVBERi1zeW50aGV0aWM=\r\n"
        "--pdf-boundary\r\n"
        "Content-Type: application/pdf; name=second.pdf\r\n"
        "Content-Disposition: attachment; filename=second.pdf\r\n"
        "Content-Transfer-Encoding: base64\r\n\r\n"
        "JVBERi1zeW50aGV0aWM=\r\n"
        "--pdf-boundary--\r\n";
    g_assert_true(g_file_set_contents(eml_path, eml, -1, &error));
    g_assert_no_error(error);
    char *source_before = NULL;
    gsize source_before_length = 0;
    g_assert_true(g_file_get_contents(
        eml_path, &source_before, &source_before_length, &error));
    g_assert_no_error(error);

    DocumentAnalysisTools tools = synthetic_tools();
    BackgroundTask *task = eml_pipeline_task_new_with_tools_and_limit(
        eml_path, processed_dir, "synthetic-evidence", &tools, 1);
    g_assert_nonnull(task);
    wait_for_task(task);
    g_assert_cmpint(background_task_get_state(task), ==,
        BACKGROUND_TASK_STATE_COMPLETED);
    EmlPipelineResult *result = background_task_get_result(task);
    g_assert_nonnull(result);
    g_assert_cmpuint(result->mime_result->attachments->len, ==, 2);
    g_assert_cmpuint(result->document_analyses->len, ==, 1);
    g_assert_cmpuint(result->skipped_document_analyses, ==, 1);
    g_assert_cmpint(result->state, ==, DOCUMENT_ANALYSIS_STATE_PARTIAL);
    g_assert_cmpuint(result->warnings->len, ==, 1);
    DocumentFileAnalysis *document = g_ptr_array_index(
        result->document_analyses, 0);
    g_assert_nonnull(document->pdf);
    g_assert_false(document->pdf->native_text_usable);
    g_assert_cmpuint(document->pdf->pages->len, ==, 2);
    PdfPageAnalysis *first = g_ptr_array_index(document->pdf->pages, 0);
    PdfPageAnalysis *second = g_ptr_array_index(document->pdf->pages, 1);
    g_assert_cmpuint(first->page_number, ==, 1);
    g_assert_cmpuint(second->page_number, ==, 2);
    g_assert_nonnull(first->render_execution);
    g_assert_nonnull(first->execution);

    char *source_after = NULL;
    gsize source_after_length = 0;
    g_assert_true(g_file_get_contents(
        eml_path, &source_after, &source_after_length, &error));
    g_assert_no_error(error);
    g_assert_cmpuint(source_after_length, ==, source_before_length);
    g_assert_cmpmem(source_after, source_after_length,
        source_before, source_before_length);
    g_free(source_after);
    g_free(source_before);
    background_task_unref(task);
    g_remove(eml_path);
    g_free(processed_dir);
    g_free(eml_path);
    g_free(tmp_dir);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/eml-pipeline-task/basic", test_eml_pipeline_basic);
    g_test_add_func("/eml-pipeline-task/document-analysis",
        test_eml_pipeline_document_analysis);
    g_test_add_func("/eml-pipeline-task/pdf-end-to-end-limit",
        test_eml_pipeline_pdf_end_to_end_and_limit);
    g_test_add_func("/eml-pipeline-task/manual-fixture-async",
        test_manual_fixture_async_start);
    return g_test_run();
}
