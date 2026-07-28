/******************************************************************************
 * @file eml_pipeline_task.c
 * @brief Pipeline d'analyse asynchrone complète pour fichier EML.
 ******************************************************************************/
#include "core/eml_pipeline_task.h"
#include "core/file_hash.h"
#include <gio/gio.h>
#include <glib.h>
#include <string.h>

typedef struct
{
    char *eml_path;
    char *processed_evidence_dir;
    char *evidence_id;
    char *exiftool;
    char *tesseract;
    char *pdfinfo;
    char *pdftotext;
    char *pdftoppm;
    guint document_analysis_limit;
} EmlPipelineTaskData;

static void eml_pipeline_task_data_free(gpointer user_data)
{
    EmlPipelineTaskData *data = user_data;
    if (data == NULL)
        return;
    g_free(data->eml_path);
    g_free(data->processed_evidence_dir);
    g_free(data->evidence_id);
    g_free(data->exiftool);
    g_free(data->tesseract);
    g_free(data->pdfinfo);
    g_free(data->pdftotext);
    g_free(data->pdftoppm);
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
    if (res->document_analyses != NULL)
        g_ptr_array_unref(res->document_analyses);
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

    EmlMimeResult *mime_res = eml_mime_extract_attachments_cancellable(
        data->eml_path,
        target_dir,
        cancellable,
        error
    );
    g_free(target_dir);

    if (mime_res == NULL)
    {
        if (g_cancellable_is_cancelled(cancellable))
        {
            eml_analysis_free(analysis);
            return FALSE;
        }

        /* Si l'extraction MIME échoue, on conserve quand même l'analyse des en-têtes (résultat partiel) */
        g_clear_error(error);
        mime_res = g_new0(EmlMimeResult, 1);
        mime_res->attachments = g_ptr_array_new_with_free_func((GDestroyNotify) eml_attachment_free);
        mime_res->warnings = g_ptr_array_new_with_free_func(g_free);
        g_ptr_array_add(mime_res->warnings, g_strdup("L'extraction MIME a échoué ou ne contient aucune pièce jointe."));
    }

    background_task_report_progress(task, 0.65,
        "Analyse des pièces jointes avec ExifTool…");
    GPtrArray *bank_proposals = g_ptr_array_new_with_free_func((GDestroyNotify) bank_proposal_free);
    GPtrArray *document_analyses = g_ptr_array_new_with_free_func(
        (GDestroyNotify) document_file_analysis_free);
    GPtrArray *pipeline_warnings =
        g_ptr_array_new_with_free_func(g_free);
    guint skipped_document_analyses = 0;

    for (guint i = 0; mime_res->attachments != NULL && i < mime_res->attachments->len; i++)
    {
        if (g_cancellable_is_cancelled(cancellable))
        {
            eml_analysis_free(analysis);
            eml_mime_result_free(mime_res);
            g_ptr_array_unref(bank_proposals);
            g_ptr_array_unref(document_analyses);
            g_ptr_array_unref(pipeline_warnings);
            g_set_error_literal(
                error,
                G_IO_ERROR,
                G_IO_ERROR_CANCELLED,
                "L'analyse EML a été annulée."
            );
            return FALSE;
        }

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
        else if (ocr_analysis_mime_is_compatible(att->detected_mime) ||
                 g_strcmp0(att->detected_mime, "application/pdf") == 0 ||
                 g_strcmp0(att->content_type, "application/pdf") == 0)
        {
            background_task_report_progress(task, 0.78,
                g_strcmp0(att->detected_mime, "application/pdf") == 0
                    ? "Extraction du texte PDF ou OCR…"
                    : "Reconnaissance OCR de la pièce jointe…");
            if (document_analyses->len >= data->document_analysis_limit)
            {
                skipped_document_analyses++;
                continue;
            }
            DocumentAnalysisTools tools = {
                .exiftool = data->exiftool,
                .tesseract = data->tesseract,
                .pdfinfo = data->pdfinfo,
                .pdftotext = data->pdftotext,
                .pdftoppm = data->pdftoppm
            };
            GError *analysis_error = NULL;
            DocumentFileAnalysis *document =
                document_file_analysis_run(&tools, att->extracted_path,
                    att->content_type, att->detected_mime, TRUE,
                    "fra+eng", cancellable, &analysis_error);
            if (document == NULL &&
                analysis_error != NULL &&
                g_error_matches(analysis_error, G_IO_ERROR,
                    G_IO_ERROR_CANCELLED))
            {
                g_propagate_error(error, analysis_error);
                eml_analysis_free(analysis);
                eml_mime_result_free(mime_res);
                g_ptr_array_unref(bank_proposals);
                g_ptr_array_unref(document_analyses);
                g_ptr_array_unref(pipeline_warnings);
                return FALSE;
            }
            g_clear_error(&analysis_error);
            if (document == NULL)
                continue;
            g_ptr_array_add(document_analyses, document);
            const char *ocr_text = document->ocr != NULL
                ? document->ocr->text
                : NULL;
            if (ocr_text != NULL)
            {
                BankProposal *bp = bank_proposal_analyze_text(ocr_text, data->evidence_id);
                if (bp != NULL)
                {
                    bp->extraction_id = g_strdup(att->part_index);
                    g_ptr_array_add(bank_proposals, bp);
                }
            }
        }
    }

    background_task_report_progress(task, 0.92,
        "Détection des données bancaires terminée.");
    background_task_report_progress(task, 0.98,
        "Préparation du dialogue de révision…");

    EmlPipelineResult *res = g_new0(EmlPipelineResult, 1);
    res->analysis = analysis;
    res->mime_result = mime_res;
    res->bank_proposals = bank_proposals;
    res->document_analyses = document_analyses;
    res->warnings = pipeline_warnings;
    res->skipped_document_analyses = skipped_document_analyses;
    res->state = skipped_document_analyses > 0
        ? DOCUMENT_ANALYSIS_STATE_PARTIAL
        : DOCUMENT_ANALYSIS_STATE_SUCCESS;
    if (skipped_document_analyses > 0)
        g_ptr_array_add(res->warnings, g_strdup_printf(
            "La limite d'analyses documentaires est atteinte : "
            "%u fichier(s) n'ont pas été analysés.",
            skipped_document_analyses));

    if (out_result != NULL)
        *out_result = res;

    background_task_report_progress(task, 1.0,
        "Analyse EML terminée avec succès.");
    return TRUE;
}

BackgroundTask *eml_pipeline_task_new(const char *eml_path,
                                       const char *processed_evidence_dir,
                                       const char *evidence_id)
{
    DocumentAnalysisTools tools = {
        .exiftool = "exiftool",
        .tesseract = "tesseract",
        .pdfinfo = "pdfinfo",
        .pdftotext = "pdftotext",
        .pdftoppm = "pdftoppm"
    };
    return eml_pipeline_task_new_with_tools(eml_path,
        processed_evidence_dir, evidence_id, &tools);
}

BackgroundTask *eml_pipeline_task_new_with_tools(
    const char *eml_path,
    const char *processed_evidence_dir,
    const char *evidence_id,
    const DocumentAnalysisTools *tools)
{
    return eml_pipeline_task_new_with_tools_and_limit(
        eml_path, processed_evidence_dir, evidence_id, tools,
        DOCUMENT_ANALYSIS_MAX_PIPELINE_ITEMS);
}

BackgroundTask *eml_pipeline_task_new_with_tools_and_limit(
    const char *eml_path,
    const char *processed_evidence_dir,
    const char *evidence_id,
    const DocumentAnalysisTools *tools,
    guint document_analysis_limit)
{
    if (eml_path == NULL || processed_evidence_dir == NULL || tools == NULL)
        return NULL;
    if (document_analysis_limit == 0)
        return NULL;

    EmlPipelineTaskData *data = g_new0(EmlPipelineTaskData, 1);
    data->eml_path = g_strdup(eml_path);
    data->processed_evidence_dir = g_strdup(processed_evidence_dir);
    data->evidence_id = g_strdup(evidence_id);
    data->exiftool = g_strdup(tools->exiftool);
    data->tesseract = g_strdup(tools->tesseract);
    data->pdfinfo = g_strdup(tools->pdfinfo);
    data->pdftotext = g_strdup(tools->pdftotext);
    data->pdftoppm = g_strdup(tools->pdftoppm);
    data->document_analysis_limit = document_analysis_limit;

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

BackgroundTask *eml_pipeline_task_start(
    const char *eml_path,
    const char *staging_directory,
    const char *evidence_id,
    const DocumentAnalysisTools *tools,
    BackgroundTaskCompletionCallback completion_callback,
    gpointer completion_data,
    GDestroyNotify completion_data_destroy)
{
    if (eml_path == NULL || staging_directory == NULL || tools == NULL)
        return NULL;
    EmlPipelineTaskData *data = g_new0(EmlPipelineTaskData, 1);
    data->eml_path = g_strdup(eml_path);
    data->processed_evidence_dir = g_strdup(staging_directory);
    data->evidence_id = g_strdup(evidence_id);
    data->exiftool = g_strdup(tools->exiftool);
    data->tesseract = g_strdup(tools->tesseract);
    data->pdfinfo = g_strdup(tools->pdfinfo);
    data->pdftotext = g_strdup(tools->pdftotext);
    data->pdftoppm = g_strdup(tools->pdftoppm);
    data->document_analysis_limit = DOCUMENT_ANALYSIS_MAX_PIPELINE_ITEMS;
    BackgroundTask *task = background_task_new(
        "Analyse complète de l’e-mail");
    GError *error = NULL;
    if (task == NULL || !background_task_start(task, eml_pipeline_task_worker,
            data, eml_pipeline_task_data_free,
            (GDestroyNotify) eml_pipeline_result_free,
            completion_callback, completion_data, completion_data_destroy,
            &error))
    {
        if (task != NULL)
            background_task_unref(task);
        else
            eml_pipeline_task_data_free(data);
        g_clear_error(&error);
        return NULL;
    }
    return task;
}
