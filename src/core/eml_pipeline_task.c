/******************************************************************************
 * @file eml_pipeline_task.c
 * @brief Pipeline d'analyse asynchrone complète pour fichier EML.
 ******************************************************************************/
#include "core/eml_pipeline_task.h"
#include "core/file_hash.h"
#include "core/rib_ocr.h"
#include <gio/gio.h>
#include <glib.h>
#include <string.h>

typedef struct
{
    char *eml_path;
    char *processed_evidence_dir;
    char *evidence_id;
} EmlPipelineTaskData;

static void eml_pipeline_task_data_free(gpointer user_data)
{
    EmlPipelineTaskData *data = user_data;
    if (data == NULL)
        return;
    g_free(data->eml_path);
    g_free(data->processed_evidence_dir);
    g_free(data->evidence_id);
    g_free(data);
}

void eml_pipeline_result_free(EmlPipelineResult *res)
{
    if (res == NULL)
        return;
    if (res->analysis != NULL)
        eml_analysis_free(res->analysis);
    if (res->mime_result != NULL)
        eml_mime_result_free(res->mime_result);
    if (res->bank_proposals != NULL)
        g_ptr_array_unref(res->bank_proposals);
    if (res->warnings != NULL)
        g_ptr_array_unref(res->warnings);
    g_free(res);
}

static gboolean eml_pipeline_task_worker(BackgroundTask *task,
                                          GCancellable *cancellable,
                                          gpointer worker_data,
                                          gpointer *out_result,
                                          GError **error)
{
    EmlPipelineTaskData *data = worker_data;
    g_return_val_if_fail(data != NULL, FALSE);

    background_task_report_progress(task, 0.10, "Analyse des en-têtes EML...");
    if (g_cancellable_is_cancelled(cancellable))
        return FALSE;

    EmlAnalysis *analysis = eml_analyzer_analyze_file(data->eml_path, error);
    if (analysis == NULL)
        return FALSE;

    background_task_report_progress(task, 0.30, "Calcul d'empreinte SHA-256...");
    char *eml_hash = NULL;
    guint64 size = 0;
    file_hash_compute_sha256(data->eml_path, cancellable, &eml_hash, &size,
        NULL);

    char *target_dir = g_build_filename(data->processed_evidence_dir, "eml_attachments",
                                         eml_hash != NULL ? eml_hash : "default", NULL);
    g_free(eml_hash);

    background_task_report_progress(task, 0.50, "Extraction sécurisée des pièces jointes...");
    if (g_cancellable_is_cancelled(cancellable))
    {
        g_free(target_dir);
        eml_analysis_free(analysis);
        return FALSE;
    }

    EmlMimeResult *mime_res = eml_mime_extract_attachments(data->eml_path, target_dir, error);
    g_free(target_dir);

    if (mime_res == NULL)
    {
        /* Si l'extraction MIME échoue, on conserve quand même l'analyse des en-têtes (résultat partiel) */
        mime_res = g_new0(EmlMimeResult, 1);
        mime_res->attachments = g_ptr_array_new_with_free_func((GDestroyNotify) eml_attachment_free);
        mime_res->warnings = g_ptr_array_new_with_free_func(g_free);
        g_ptr_array_add(mime_res->warnings, g_strdup("L'extraction MIME a échoué ou ne contient aucune pièce jointe."));
    }

    background_task_report_progress(task, 0.75, "Analyse OCR et détection bancaire...");
    GPtrArray *bank_proposals = g_ptr_array_new_with_free_func((GDestroyNotify) bank_proposal_free);

    for (guint i = 0; mime_res->attachments != NULL && i < mime_res->attachments->len; i++)
    {
        EmlAttachment *att = g_ptr_array_index(mime_res->attachments, i);
        if (att->extracted_path == NULL)
            continue;

        /* Analyse bancaire sur les fichiers texte/images compatibles */
        if (g_str_has_suffix(att->extracted_path, ".txt") || g_str_has_suffix(att->extracted_path, ".eml"))
        {
            char *content = NULL;
            if (g_file_get_contents(att->extracted_path, &content, NULL, NULL))
            {
                BankProposal *bp = bank_proposal_analyze_text(content, data->evidence_id);
                if (bp != NULL)
                {
                    bp->extraction_id = g_strdup(att->part_index);
                    g_ptr_array_add(bank_proposals, bp);
                }
                g_free(content);
            }
        }
        else if (g_str_has_suffix(att->extracted_path, ".png") || g_str_has_suffix(att->extracted_path, ".jpg") || g_str_has_suffix(att->extracted_path, ".jpeg"))
        {
            char *ocr_text = NULL;
            char *ocr_version = NULL;
            (void) rib_ocr_extract_text(att->extracted_path, &ocr_text,
                &ocr_version, NULL);
            g_free(ocr_version);
            if (ocr_text != NULL)
            {
                BankProposal *bp = bank_proposal_analyze_text(ocr_text, data->evidence_id);
                if (bp != NULL)
                {
                    bp->extraction_id = g_strdup(att->part_index);
                    g_ptr_array_add(bank_proposals, bp);
                }
                g_free(ocr_text);
            }
        }
    }

    background_task_report_progress(task, 1.0, "Analyse EML terminée avec succès.");

    EmlPipelineResult *res = g_new0(EmlPipelineResult, 1);
    res->analysis = analysis;
    res->mime_result = mime_res;
    res->bank_proposals = bank_proposals;
    res->warnings = g_ptr_array_new_with_free_func(g_free);

    if (out_result != NULL)
        *out_result = res;

    return TRUE;
}

BackgroundTask *eml_pipeline_task_new(const char *eml_path,
                                       const char *processed_evidence_dir,
                                       const char *evidence_id)
{
    if (eml_path == NULL || processed_evidence_dir == NULL)
        return NULL;

    EmlPipelineTaskData *data = g_new0(EmlPipelineTaskData, 1);
    data->eml_path = g_strdup(eml_path);
    data->processed_evidence_dir = g_strdup(processed_evidence_dir);
    data->evidence_id = g_strdup(evidence_id);

    BackgroundTask *task = background_task_new(
        "Analyse du message EML et de ses pièces jointes");
    GError *start_error = NULL;
    if (task == NULL || !background_task_start(task, eml_pipeline_task_worker,
            data, eml_pipeline_task_data_free, (GDestroyNotify)
            eml_pipeline_result_free, NULL, NULL, NULL, &start_error))
    {
        if (task != NULL)
            background_task_unref(task);
        else
            eml_pipeline_task_data_free(data);
        g_clear_error(&start_error);
        return NULL;
    }
    return task;
}
