#include "core/identity_ocr_workflow.h"

#include "core/evidence_preview.h"
#include "core/identity_field_extractor.h"
#include "core/ocr_analysis.h"

#include <string.h>

static gboolean identity_ocr_workflow_languages_are_valid(
    const char *languages)
{
    char **codes;
    gboolean valid = TRUE;

    if (languages == NULL || languages[0] == '\0')
        return FALSE;
    codes = g_strsplit(languages, "+", -1);
    for (guint index = 0; codes[index] != NULL && valid; index++) {
        const char *code = codes[index];

        valid = strlen(code) == 3;
        for (guint character = 0; valid && code[character] != '\0';
             character++)
            valid = g_ascii_islower(code[character]);
    }
    g_strfreev(codes);
    return valid;
}

static gboolean identity_ocr_workflow_validate_request(
    const IdentityOcrWorkflowRequest *request,
    GError **error)
{
#define REQUIRE_REQUEST_FIELD(condition, message) \
    G_STMT_START { \
        if (!(condition)) { \
            g_set_error_literal(error, G_IO_ERROR, \
                G_IO_ERROR_INVALID_ARGUMENT, (message)); \
            return FALSE; \
        } \
    } G_STMT_END

    REQUIRE_REQUEST_FIELD(request != NULL,
        "La demande OCR d’identité est absente.");
    REQUIRE_REQUEST_FIELD(request->root_path != NULL,
        "Le chemin racine de l’enquête est absent de la demande OCR.");
    REQUIRE_REQUEST_FIELD(request->evidence_identifier != NULL,
        "L’UUID de la preuve est absent de la demande OCR.");
    REQUIRE_REQUEST_FIELD(request->relative_path != NULL,
        "Le chemin relatif de la preuve est absent de la demande OCR.");
    REQUIRE_REQUEST_FIELD(request->expected_sha256 != NULL,
        "Le SHA-256 attendu est absent de la demande OCR.");
    REQUIRE_REQUEST_FIELD(request->mime_type != NULL,
        "Le type MIME de la preuve est absent de la demande OCR.");
    REQUIRE_REQUEST_FIELD(
        identity_ocr_document_type_is_valid(request->document_type),
        "Le type de document d’identité est invalide dans la demande OCR.");
    REQUIRE_REQUEST_FIELD(
        identity_ocr_document_side_is_valid(request->document_side),
        "La face du document d’identité est invalide dans la demande OCR.");
    REQUIRE_REQUEST_FIELD(
        identity_ocr_workflow_languages_are_valid(request->languages),
        "Le code de langue Tesseract est invalide dans la demande OCR "
        "(un code ISO 639-3 comme « fra » ou « eng » est requis).");
    REQUIRE_REQUEST_FIELD(request->preprocessing_profile != NULL,
        "Le profil de prétraitement est absent de la demande OCR.");
    REQUIRE_REQUEST_FIELD(request->tesseract_executable != NULL,
        "Le chemin de l’exécutable Tesseract est absent de la demande OCR.");
    REQUIRE_REQUEST_FIELD(request->page_number > 0,
        "Le numéro de page est invalide dans la demande OCR.");
    REQUIRE_REQUEST_FIELD(request->profile <= IDENTITY_OCR_PREPROCESS_UPSCALE,
        "Le profil de prétraitement est invalide dans la demande OCR.");

#undef REQUIRE_REQUEST_FIELD
    return TRUE;
}

gboolean identity_ocr_workflow_request_is_valid(
    const IdentityOcrWorkflowRequest *request)
{
    return identity_ocr_workflow_validate_request(request, NULL);
}

IdentityOcrRun *identity_ocr_workflow_execute(
    const IdentityOcrWorkflowRequest *request,
    GCancellable *cancellable,
    GError **error)
{
    EvidencePreviewRequest *preview_request = NULL;
    IdentityOcrWorkImage *image = NULL;
    OcrAnalysisResult *ocr = NULL;
    IdentityOcrRun *run = NULL;
    GPtrArray *fields = NULL;
    char *available = NULL;
    char *preview_data = NULL;
    gsize preview_length = 0;

    g_return_val_if_fail(error == NULL || *error == NULL, NULL);
    if (!identity_ocr_workflow_validate_request(request, error))
        return NULL;
    preview_request = evidence_preview_request_new(request->root_path,
        request->evidence_identifier, request->relative_path,
        request->expected_sha256, request->mime_type, request->generation);
    if (preview_request != NULL)
        image = identity_ocr_preprocessor_prepare(preview_request,
            request->page_number, request->profile, cancellable, error);
    evidence_preview_request_free(preview_request);
    if (image == NULL) goto cleanup;
    available = ocr_analysis_list_languages(
        request->tesseract_executable, cancellable, error);
    if (available == NULL) goto cleanup;
    ocr = ocr_analysis_run(request->tesseract_executable, image->path,
        request->languages, cancellable, error);
    if (ocr == NULL) goto cleanup;
    run = identity_ocr_run_new(request->evidence_identifier,
        request->expected_sha256, request->document_type,
        request->document_side, request->page_number, request->languages,
        request->preprocessing_profile);
    if (run == NULL) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
            "Le résultat OCR ne peut pas être représenté.");
        goto cleanup;
    }
    identity_ocr_run_set_outputs(run,
        ocr->execution != NULL ? ocr->execution->version :
            request->tesseract_version,
        available, "tesseract stdout -l LANG ; tesseract stdout -l LANG tsv",
        ocr->text, ocr->tsv);
    if (g_file_get_contents(image->path, &preview_data, &preview_length,
            NULL)) {
        GBytes *preview = g_bytes_new_take(preview_data, preview_length);
        preview_data = NULL;
        identity_ocr_run_set_preview(run, preview);
        g_bytes_unref(preview);
    }
    fields = identity_field_extractor_extract(ocr->text, ocr->tsv,
        image->width, image->height, error);
    if (fields == NULL && error != NULL && *error != NULL) {
        identity_ocr_run_free(run);
        run = NULL;
        goto cleanup;
    }
    while (fields != NULL && fields->len > 0)
        identity_ocr_run_add_field(run,
            g_ptr_array_steal_index(fields, 0));

cleanup:
    g_free(preview_data);
    g_clear_pointer(&fields, g_ptr_array_unref);
    identity_ocr_work_image_free(image);
    ocr_analysis_result_free(ocr);
    g_free(available);
    return run;
}
