/******************************************************************************
 * @file eml_processing.h
 * @brief Préparation vérifiée d'une copie de travail EML.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_EML_PROCESSING_H
#define LABFY_INVESTIGATION_EML_PROCESSING_H
#include "core/eml_analyzer.h"
#include "models/evidence_record.h"
#include <glib.h>
G_BEGIN_DECLS
/** @brief Résultat opaque possédant la copie et son analyse. */
typedef struct EmlProcessingResult EmlProcessingResult;
/**
 * @brief Copie une preuve vers les extractions puis analyse uniquement la copie.
 * @param investigation_root Racine canonique de l'enquête.
 * @param evidence_record Preuve EML empruntée.
 * @param error Destination facultative d'une erreur.
 * @return Nouveau résultat, ou NULL.
 */
EmlProcessingResult *eml_processing_prepare(const char *investigation_root,
    const EvidenceRecord *evidence_record, GError **error);
/** @brief Libère le résultat sans supprimer la copie vérifiée. */
void eml_processing_result_free(EmlProcessingResult *result);
/** @brief Retourne le chemin de la copie de travail. */
const char *eml_processing_result_get_copy_path(const EmlProcessingResult *result);
/** @brief Retourne l'analyse empruntée de la copie. */
const EmlAnalysis *eml_processing_result_get_analysis(const EmlProcessingResult *result);
G_END_DECLS
#endif
