/******************************************************************************
 * @file eml_pipeline_task.h
 * @brief Pipeline d'analyse asynchrone complète pour fichier EML (En-têtes, MIME, OCR, Banque).
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_EML_PIPELINE_TASK_H
#define LABFY_INVESTIGATION_EML_PIPELINE_TASK_H

#include "core/background_task.h"
#include "core/bank_proposal.h"
#include "core/eml_analyzer.h"
#include "core/eml_mime_extractor.h"
#include "core/document_file_analysis.h"

G_BEGIN_DECLS

/** @brief Résultat global d'un pipeline d'analyse EML. */
typedef struct EmlPipelineResult
{
    EmlAnalysis *analysis;      /**< Analyse des en-têtes EML */
    EmlMimeResult *mime_result; /**< Pièces jointes extraites */
    GPtrArray *bank_proposals;  /**< Tableau de BankProposal* */
    GPtrArray *document_analyses; /**< Tableau de DocumentFileAnalysis* */
    GPtrArray *warnings;        /**< Avertissements globaux */
} EmlPipelineResult;

/** @brief Libère un résultat EmlPipelineResult. */
void eml_pipeline_result_free(EmlPipelineResult *result);

/**
 * @brief Crée une nouvelle tâche asynchrone d'analyse EML complète.
 * @param eml_path Chemin du fichier .eml source.
 * @param processed_evidence_dir Dossier racine 02_Preuves_Traitees.
 * @param evidence_id UUID de la preuve EML.
 * @return Nouvelle BackgroundTask, ou NULL.
 */
BackgroundTask *eml_pipeline_task_new(const char *eml_path,
                                       const char *processed_evidence_dir,
                                       const char *evidence_id);

BackgroundTask *eml_pipeline_task_new_with_tools(
    const char *eml_path,
    const char *processed_evidence_dir,
    const char *evidence_id,
    const DocumentAnalysisTools *tools
);

G_END_DECLS

#endif /* LABFY_INVESTIGATION_EML_PIPELINE_TASK_H */
