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
    g_assert_cmpstr(att->sanitized_filename, ==, "___rib_suspect.txt");
    g_assert_true(g_str_has_suffix(att->extracted_path,
        "/1____rib_suspect.txt"));
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

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/eml-pipeline-task/basic", test_eml_pipeline_basic);
    return g_test_run();
}
