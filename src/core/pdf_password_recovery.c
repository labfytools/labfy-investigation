/******************************************************************************
 * @file pdf_password_recovery.c
 * @brief Récupération locale du mot de passe d'une preuve PDF.
 ******************************************************************************/

#include "core/pdf_password_recovery.h"
#include "core/tool_process.h"

#include <glib/gstdio.h>
#include <errno.h>
#include <string.h>

typedef struct
{
    char *pdf_path;
    char *output_directory;
    char *evidence_identifier;
    PdfPasswordRecoveryMethod method;
    char *parameter;
} PdfPasswordRecoveryData;

struct PdfPasswordRecoveryResult
{
    gboolean recovered;
    char *password;
};

/** @brief Libère les paramètres possédés par le worker. */
static void pdf_password_recovery_data_free(gpointer data)
{
    PdfPasswordRecoveryData *recovery_data = data;
    if (recovery_data == NULL) return;
    g_free(recovery_data->pdf_path);
    g_free(recovery_data->output_directory);
    g_free(recovery_data->evidence_identifier);
    g_free(recovery_data->parameter);
    g_free(recovery_data);
}

/** @brief Efface puis libère le résultat contenant le secret. */
static void pdf_password_recovery_result_free(gpointer data)
{
    PdfPasswordRecoveryResult *result = data;
    if (result == NULL) return;
    if (result->password != NULL)
        memset(result->password, 0, strlen(result->password));
    g_free(result->password);
    g_free(result);
}

/** @brief Convertit une sortie binaire d'outil en texte UTF-8. */
static char *pdf_password_recovery_output_to_text(
    const ToolProcessResult *process_result)
{
    GBytes *bytes = tool_process_result_ref_stdout(process_result);
    gconstpointer data = NULL;
    gsize size = 0;
    char *text = NULL;
    if (bytes != NULL) data = g_bytes_get_data(bytes, &size);
    if (data != NULL) text = g_utf8_make_valid(data, (gssize) size);
    g_clear_pointer(&bytes, g_bytes_unref);
    return text;
}

/** @brief Exécute un outil et exige un code de sortie nul. */
static gboolean pdf_password_recovery_run(const char *executable,
    const char *const arguments[], const char *working_directory,
    GCancellable *cancellable, ToolProcessResult **result, GError **error)
{
    if (!tool_process_run(executable, arguments, working_directory,
            cancellable, result, error)) return FALSE;
    if (!tool_process_result_is_success(*result))
    {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
            "%s s'est terminé avec le code %d.", executable,
            tool_process_result_get_exit_status(*result));
        tool_process_result_free(*result);
        *result = NULL;
        return FALSE;
    }
    return TRUE;
}

/** @brief Exécute pdf2john puis John dans un thread secondaire. */
static gboolean pdf_password_recovery_worker(BackgroundTask *task,
    GCancellable *cancellable, gpointer worker_data, gpointer *out_result,
    GError **error)
{
    PdfPasswordRecoveryData *data = worker_data;
    PdfPasswordRecoveryResult *result = NULL;
    ToolProcessResult *process_result = NULL;
    char *hash_text = NULL;
    char *hash_name = NULL;
    char *hash_path = NULL;
    char *journal_name = NULL;
    char *journal_path = NULL;
    char *temp_directory = NULL;
    char *pot_path = NULL;
    char *session_path = NULL;
    char *method_argument = NULL;
    char *pot_argument = NULL;
    char *session_argument = NULL;
    char *home_argument = NULL;
    char *show_text = NULL;
    char *separator = NULL;
    char *line_end = NULL;
    char *journal = NULL;
    gchar **numeric_bounds = NULL;
    const char *pdf_arguments[] = { data->pdf_path, NULL };
    const char *john_arguments[7] =
        { NULL, "john", NULL, NULL, NULL, NULL, NULL };
    const char *show_arguments[6] =
        { NULL, "john", "--show", NULL, NULL, NULL };
    gboolean success = FALSE;

    *out_result = NULL;
    background_task_report_progress(task, 0.05, "Extraction du hash PDF");
    if (!pdf_password_recovery_run("pdf2john", pdf_arguments, NULL,
            cancellable, &process_result, error)) goto cleanup;
    hash_text = pdf_password_recovery_output_to_text(process_result);
    tool_process_result_free(process_result);
    process_result = NULL;
    if (hash_text == NULL || strstr(hash_text, "$pdf$") == NULL)
    {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
            "pdf2john n'a produit aucun hash PDF exploitable.");
        goto cleanup;
    }
    if (g_mkdir_with_parents(data->output_directory, 0750) != 0)
    {
        g_set_error(error, G_IO_ERROR, g_io_error_from_errno(errno),
            "Impossible de créer le dossier de récupération : %s",
            g_strerror(errno));
        goto cleanup;
    }
    hash_name = g_strdup_printf("%s-pdf.hash", data->evidence_identifier);
    hash_path = g_build_filename(data->output_directory, hash_name, NULL);
    if (!g_file_set_contents(hash_path, hash_text, -1, error)) goto cleanup;
    if (data->method == PDF_PASSWORD_RECOVERY_NUMERIC_RANGE)
    {
        guint64 minimum = 0;
        guint64 maximum = 0;
        guint64 candidate = 0;
        GError *qpdf_error = NULL;
        numeric_bounds = g_strsplit(data->parameter, ":", 2);
        minimum = g_ascii_strtoull(numeric_bounds[0], NULL, 10);
        maximum = g_ascii_strtoull(numeric_bounds[1], NULL, 10);
        result = g_new0(PdfPasswordRecoveryResult, 1);
        for (candidate = minimum; candidate <= maximum; candidate++)
        {
            char *password = g_strdup_printf("%" G_GUINT64_FORMAT, candidate);
            char *password_argument = g_strdup_printf("--password=%s", password);
            const char *qpdf_arguments[] =
                { password_argument, "--decrypt", data->pdf_path,
                  "/dev/null", NULL };
            background_task_report_progress(task,
                maximum == minimum ? 0.5 : 0.1 +
                (0.85 * ((double) (candidate - minimum) /
                (double) (maximum - minimum + 1))),
                "Recherche numérique en cours");
            g_clear_error(&qpdf_error);
            if (pdf_password_recovery_run("qpdf", qpdf_arguments, NULL,
                    cancellable, &process_result, &qpdf_error))
            {
                result->recovered = TRUE;
                result->password = g_steal_pointer(&password);
                g_free(password_argument);
                break;
            }
            if (qpdf_error != NULL && g_error_matches(qpdf_error,
                    TOOL_PROCESS_ERROR, TOOL_PROCESS_ERROR_CANCELLED))
            {
                g_propagate_error(error, qpdf_error);
                g_free(password);
                g_free(password_argument);
                goto cleanup;
            }
            g_clear_error(&qpdf_error);
            tool_process_result_free(process_result);
            process_result = NULL;
            g_free(password);
            g_free(password_argument);
            if (candidate == G_MAXUINT64) break;
        }
        journal_name = g_strdup_printf("%s-recovery.log",
            data->evidence_identifier);
        journal_path = g_build_filename(data->output_directory, journal_name, NULL);
        journal = g_strdup_printf("Outil: qpdf\nMéthode: plage numérique\n"
            "Résultat: %s\nLe mot de passe n'est pas enregistré dans ce journal.\n",
            result->recovered ? "trouvé" : "non trouvé");
        if (!g_file_set_contents(journal_path, journal, -1, error)) goto cleanup;
        *out_result = g_steal_pointer(&result);
        success = TRUE;
        goto cleanup;
    }
    temp_directory = g_dir_make_tmp("labfy-pdf-recovery-XXXXXX", error);
    if (temp_directory == NULL) goto cleanup;
    pot_path = g_build_filename(temp_directory, "john.pot", NULL);
    session_path = g_build_filename(temp_directory, "john-session", NULL);
    pot_argument = g_strdup_printf("--pot=%s", pot_path);
    session_argument = g_strdup_printf("--session=%s", session_path);
    home_argument = g_strdup_printf("HOME=%s", temp_directory);
    method_argument = g_strdup_printf(data->method ==
        PDF_PASSWORD_RECOVERY_DICTIONARY ? "--wordlist=%s" : "--mask=%s",
        data->parameter);
    john_arguments[0] = home_argument;
    john_arguments[2] = pot_argument;
    john_arguments[3] = session_argument;
    john_arguments[4] = method_argument;
    john_arguments[5] = hash_path;
    background_task_report_progress(task, 0.15,
        "Recherche du mot de passe avec John");
    if (!pdf_password_recovery_run("env", john_arguments, temp_directory,
            cancellable, &process_result, error)) goto cleanup;
    tool_process_result_free(process_result);
    process_result = NULL;
    show_arguments[0] = home_argument;
    show_arguments[3] = pot_argument;
    show_arguments[4] = hash_path;
    if (!pdf_password_recovery_run("env", show_arguments, temp_directory,
            cancellable, &process_result, error)) goto cleanup;
    show_text = pdf_password_recovery_output_to_text(process_result);
    result = g_new0(PdfPasswordRecoveryResult, 1);
    /* John peut écrire des avertissements MPI contenant ':' avant la sortie
     * de --show ; on ne considère que la ligne du fichier hash. */
    separator = show_text != NULL ? strstr(show_text, data->pdf_path) : NULL;
    if (separator != NULL)
        separator = strchr(separator, ':');
    if (separator != NULL && strstr(show_text, "0 password hashes cracked") == NULL)
    {
        separator++;
        line_end = strchr(separator, '\n');
        result->password = line_end != NULL ? g_strndup(separator,
            (gsize) (line_end - separator)) : g_strdup(separator);
        result->recovered = result->password != NULL;
    }
    journal_name = g_strdup_printf("%s-recovery.log",
        data->evidence_identifier);
    journal_path = g_build_filename(data->output_directory, journal_name, NULL);
    journal = g_strdup_printf("Outil: John the Ripper\nMéthode: %s\nRésultat: %s\n"
        "Le mot de passe n'est pas enregistré dans ce journal.\n",
        data->method == PDF_PASSWORD_RECOVERY_DICTIONARY ?
        "dictionnaire" : "masque", result->recovered ? "trouvé" : "non trouvé");
    if (!g_file_set_contents(journal_path, journal, -1, error)) goto cleanup;
    background_task_report_progress(task, 1.0, result->recovered ?
        "Mot de passe retrouvé" : "Aucun mot de passe trouvé");
    *out_result = g_steal_pointer(&result);
    success = TRUE;
cleanup:
    tool_process_result_free(process_result);
    pdf_password_recovery_result_free(result);
    if (pot_path != NULL) g_unlink(pot_path);
    if (session_path != NULL)
    {
        char *rec_path = g_strconcat(session_path, ".rec", NULL);
        char *log_path = g_strconcat(session_path, ".log", NULL);
        g_unlink(rec_path);
        g_unlink(log_path);
        g_free(rec_path);
        g_free(log_path);
    }
    if (temp_directory != NULL)
    {
        char *john_home = g_build_filename(temp_directory, ".john", NULL);
        g_rmdir(john_home);
        g_free(john_home);
        g_rmdir(temp_directory);
    }
    g_free(hash_text); g_free(hash_name); g_free(hash_path);
    g_free(journal_name); g_free(journal_path); g_free(temp_directory);
    g_free(pot_path); g_free(session_path); g_free(method_argument);
    g_free(pot_argument); g_free(session_argument); g_free(home_argument);
    g_free(show_text);
    g_free(journal);
    g_strfreev(numeric_bounds);
    return success;
}

BackgroundTask *pdf_password_recovery_start(TaskManager *task_manager,
    const char *pdf_path, const char *output_directory,
    const char *evidence_identifier, PdfPasswordRecoveryMethod method,
    const char *parameter, BackgroundTaskCompletionCallback completion_callback,
    gpointer completion_data, GDestroyNotify completion_data_destroy,
    GError **error)
{
    PdfPasswordRecoveryData *data = NULL;
    BackgroundTask *task = NULL;
    if (task_manager == NULL || pdf_path == NULL || output_directory == NULL ||
        evidence_identifier == NULL || parameter == NULL || parameter[0] == '\0' ||
        (method != PDF_PASSWORD_RECOVERY_DICTIONARY &&
         method != PDF_PASSWORD_RECOVERY_MASK &&
         method != PDF_PASSWORD_RECOVERY_NUMERIC_RANGE))
    {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
            "Les paramètres de récupération du PDF sont invalides.");
        return NULL;
    }
    data = g_new0(PdfPasswordRecoveryData, 1);
    data->pdf_path = g_strdup(pdf_path);
    data->output_directory = g_strdup(output_directory);
    data->evidence_identifier = g_strdup(evidence_identifier);
    data->method = method;
    data->parameter = g_strdup(parameter);
    task = background_task_new("Récupération du mot de passe PDF");
    if (task == NULL || !task_manager_add(task_manager, task, error) ||
        !background_task_start(task, pdf_password_recovery_worker, data,
            pdf_password_recovery_data_free,
            pdf_password_recovery_result_free, completion_callback,
            completion_data, completion_data_destroy, error))
    {
        if (task != NULL) task_manager_remove(task_manager, task);
        pdf_password_recovery_data_free(data);
        background_task_unref(task);
        return NULL;
    }
    background_task_unref(task);
    return task;
}

const PdfPasswordRecoveryResult *pdf_password_recovery_get_result(
    const BackgroundTask *task)
{
    return task != NULL ? background_task_get_result(task) : NULL;
}

gboolean pdf_password_recovery_result_is_recovered(
    const PdfPasswordRecoveryResult *result)
{
    return result != NULL && result->recovered;
}

const char *pdf_password_recovery_result_get_password(
    const PdfPasswordRecoveryResult *result)
{
    return result != NULL ? result->password : NULL;
}
