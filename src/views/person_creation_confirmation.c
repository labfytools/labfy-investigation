#include "views/person_creation_confirmation.h"

static const char *displayed_value(const IdentityFieldObservation *field)
{
    const char *corrected = identity_field_observation_get_corrected_value(field);
    return corrected != NULL ? corrected
        : identity_field_observation_get_raw_value(field);
}

static void append_ocr_review(GString *summary, const GPtrArray *runs)
{
    g_string_append(summary,
        "\n\nCorrection OCR enregistrée\n"
        "La transcription corrigée reste attachée à la preuve et distincte "
        "des données appliquées à la personne. L’OCR peut contenir des "
        "erreurs : l’authenticité n’est pas établie.");
    if (runs == NULL || runs->len == 0) {
        g_string_append(summary, "\nAucune correction OCR enregistrée.");
        return;
    }
    for (guint run_index = 0; run_index < runs->len; run_index++) {
        IdentityOcrRun *run = g_ptr_array_index((GPtrArray *) runs, run_index);
        g_string_append_printf(summary,
            "\n• %s — %s — page %u — Tesseract %s — %s",
            identity_ocr_run_get_document_type(run),
            identity_ocr_run_get_document_side(run),
            identity_ocr_run_get_page(run),
            identity_ocr_run_get_version(run) != NULL
                ? identity_ocr_run_get_version(run) : "version inconnue",
            identity_ocr_run_get_languages(run));
        if (identity_ocr_run_get_factual_notes(run) != NULL)
            g_string_append_printf(summary, "\n  Notes factuelles : %s",
                identity_ocr_run_get_factual_notes(run));
        const GPtrArray *fields = identity_ocr_run_get_fields(run);
        for (guint field_index = 0; fields != NULL && field_index < fields->len;
             field_index++) {
            IdentityFieldObservation *field =
                g_ptr_array_index((GPtrArray *) fields, field_index);
            IdentityReviewStatus review =
                identity_field_observation_get_status(field);
            if (review == IDENTITY_REVIEW_ACCEPTED ||
                review == IDENTITY_REVIEW_MODIFIED)
                g_string_append_printf(summary,
                    "\n  - %s : %s (brut : %s ; origine : %s)",
                    identity_field_observation_get_code(field),
                    displayed_value(field),
                    identity_field_observation_get_raw_value(field) != NULL
                        ? identity_field_observation_get_raw_value(field)
                        : "absente",
                    identity_field_observation_get_origin(field));
        }
    }
}

void person_creation_confirmation_append_sections(
    GString *summary, const GPtrArray *ocr_runs,
    PersonOcrProjectionEditor *projection_editor,
    PersonFactualRelationEditor *relation_editor)
{
    if (summary == NULL)
        return;
    append_ocr_review(summary, ocr_runs);
    person_ocr_projection_editor_append_summary(projection_editor, summary);
    person_factual_relation_editor_append_summary(relation_editor, summary);
}
