/******************************************************************************
 * @file eml_processing.c
 * @brief Préparation vérifiée d'une copie de travail EML.
 ******************************************************************************/
#include "core/eml_processing.h"
#include "core/evidence_copy.h"
#include "core/file_hash.h"
#include <string.h>
struct EmlProcessingResult { char *copy_path; EmlAnalysis *analysis; };
EmlProcessingResult *eml_processing_prepare(const char *root,
    const EvidenceRecord *record, GError **error)
{
    EmlProcessingResult *result = NULL;
    EvidenceCopyResult *copy = NULL;
    const char *identifier = evidence_record_get_identifier(record);
    const char *relative = evidence_record_get_relative_path(record);
    const char *expected_hash = evidence_record_get_sha256(record);
    char *source = NULL, *directory = NULL, *name = NULL, *destination = NULL;
    char *existing_hash = NULL; guint64 existing_size = 0;
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);
    if (root == NULL || identifier == NULL || relative == NULL || expected_hash == NULL)
    { g_set_error_literal(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
        "La preuve EML ou la racine de l'enquête est invalide."); return NULL; }
    source = g_build_filename(root, relative, NULL);
    directory = g_build_filename(root, "02_Preuves_Traitees", "Extractions", NULL);
    name = g_strdup_printf("%s_email_headers.eml", identifier);
    destination = g_build_filename(directory, name, NULL);
    if (g_file_test(destination, G_FILE_TEST_EXISTS))
    {
        if (!file_hash_compute_sha256(destination, NULL, &existing_hash,
                &existing_size, error) || strcmp(existing_hash, expected_hash) != 0)
        {
            if (error != NULL && *error == NULL) g_set_error_literal(error,
                G_FILE_ERROR, G_FILE_ERROR_FAILED,
                "La copie EML existante ne correspond pas à la preuve originale.");
            goto cleanup;
        }
    }
    else
    {
        copy = evidence_copy_file(source, directory, name, NULL, error);
        if (copy == NULL || strcmp(evidence_copy_result_get_sha256(copy),
                expected_hash) != 0)
        {
            if (error != NULL && *error == NULL) g_set_error_literal(error,
                G_FILE_ERROR, G_FILE_ERROR_FAILED,
                "La copie EML ne correspond pas à l'empreinte enregistrée.");
            goto cleanup;
        }
    }
    result = g_new0(EmlProcessingResult, 1);
    result->copy_path = g_strdup(destination);
    result->analysis = eml_analyzer_analyze_file(destination, error);
    if (result->copy_path == NULL || result->analysis == NULL)
    { eml_processing_result_free(result); result = NULL; }
cleanup:
    evidence_copy_result_free(copy); g_free(existing_hash);
    g_free(source); g_free(directory); g_free(name); g_free(destination);
    return result;
}
void eml_processing_result_free(EmlProcessingResult *result)
{
    if (result == NULL) return;
    eml_analysis_free(result->analysis); g_free(result->copy_path); g_free(result);
}
const char *eml_processing_result_get_copy_path(const EmlProcessingResult *result)
{ return result != NULL ? result->copy_path : NULL; }
const EmlAnalysis *eml_processing_result_get_analysis(const EmlProcessingResult *result)
{ return result != NULL ? result->analysis : NULL; }
