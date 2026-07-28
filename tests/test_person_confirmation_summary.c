#include "core/person_confirmation_summary.h"
#include <glib.h>

static EvidenceRecord *record(void)
{
    EvidenceRecord *evidence = evidence_record_new(
        "10000000-0000-4000-8000-000000000010",
        "SPECIMEN-confirmation.png", "specimen.png",
        "01_Preuves_Originales/specimen.png", "screenshot", 42,
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "2026-07-28T10:00:00Z", NULL, NULL, "Description SPECIMEN",
        EVIDENCE_INTEGRITY_STATUS_VALID, NULL);
    evidence_record_set_display_metadata(
        evidence, "Capture d’écran", "image/png");
    return evidence;
}

static void test_full_summary(void)
{
    EvidenceRecord *evidence = record();
    GPtrArray *roles = g_ptr_array_new();
    g_ptr_array_add(roles, "Victime");
    g_ptr_array_add(roles, "Témoin");
    char *summary = person_confirmation_summary_build(
        "Personne SPECIMEN", "Nom SPECIMEN", "Pseudo", "Présumé", 55,
        "Notes synthétiques", roles, evidence);
    const char *values[] = {
        "Personne SPECIMEN", "Nom SPECIMEN", "Pseudo", "Présumé", "55 %",
        "Victime", "Témoin", "SPECIMEN-confirmation.png",
        "Capture d’écran", "image/png", "42", "2026-07-28T10:00:00Z",
        "aaaaaaaaaaaa…", "Valide", "Description SPECIMEN",
        "Aucune écriture n’a encore été effectuée"
    };
    for (guint i = 0; i < G_N_ELEMENTS(values); i++)
        g_assert_nonnull(strstr(summary, values[i]));
    g_free(summary);
    g_ptr_array_unref(roles);
    evidence_record_free(evidence);
}

static void test_optional_summary(void)
{
    char *summary = person_confirmation_summary_build(
        "SPECIMEN", "", NULL, "Inconnu", 0, "", NULL, NULL);
    g_assert_nonnull(strstr(summary, "Nom déclaré : Non renseigné"));
    g_assert_nonnull(strstr(summary, "Pseudonyme : Non renseigné"));
    g_assert_nonnull(strstr(summary, "Notes factuelles : Aucune"));
    g_assert_nonnull(strstr(summary, "Aucun rôle sélectionné"));
    g_assert_nonnull(strstr(summary,
        "Aucune preuve principale associée"));
    g_free(summary);
}
static void test_multiple_summary(void)
{
    EvidenceRecord *evidence = record();
    PersonEvidenceSelection *selection = person_evidence_selection_new();
    GPtrArray *roles = g_ptr_array_new();
    char *summary;
    g_ptr_array_add(roles, "Témoin");
    g_assert_true(person_evidence_selection_add_existing(
        selection, evidence, NULL));
    g_assert_true(person_evidence_selection_add_staged(selection,
        "/tmp/SPECIMEN-source.pdf", "/tmp/SPECIMEN-stage.pdf",
        "SPECIMEN-document.pdf", "application/pdf", "document", 100,
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
        NULL, "2026-07-28T10:00:00Z", NULL));
    summary = person_confirmation_summary_build_multiple(
        "Personne SPECIMEN", NULL, NULL, "Présumé", 40, NULL,
        roles, selection);
    g_assert_nonnull(strstr(summary, "SPECIMEN-confirmation.png"));
    g_assert_nonnull(strstr(summary, "SPECIMEN-document.pdf"));
    g_assert_nonnull(strstr(summary, "1 nouvelle(s) preuve(s)"));
    g_assert_nonnull(strstr(summary, "1 preuve(s) existante(s)"));
    g_assert_nonnull(strstr(summary, "Aucun OCR"));
    g_free(summary);
    g_ptr_array_unref(roles);
    person_evidence_selection_free(selection);
    evidence_record_free(evidence);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/person-confirmation/full", test_full_summary);
    g_test_add_func("/person-confirmation/optional", test_optional_summary);
    g_test_add_func("/person-confirmation/multiple", test_multiple_summary);
    return g_test_run();
}
