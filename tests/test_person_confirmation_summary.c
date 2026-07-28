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

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/person-confirmation/full", test_full_summary);
    g_test_add_func("/person-confirmation/optional", test_optional_summary);
    return g_test_run();
}
