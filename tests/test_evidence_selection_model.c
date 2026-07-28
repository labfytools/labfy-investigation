#include "models/evidence_selection_model.h"
#include <glib.h>

static EvidenceRecord *record(const char *id, const char *name,
    const char *type, const char *label, const char *description)
{
    GError *error = NULL;
    EvidenceRecord *r = evidence_record_new(id, name, "synthetic.bin",
        "01_Preuves_Originales/synthetic.bin", type, 12,
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "2026-07-28T10:00:00Z", NULL, NULL, description,
        EVIDENCE_INTEGRITY_STATUS_UNKNOWN, &error);
    g_assert_no_error(error);
    evidence_record_set_display_metadata(r, label, "image/png");
    return r;
}
static void test_search_filter_selection(void)
{
    GPtrArray *records = g_ptr_array_new_with_free_func(
        (GDestroyNotify) evidence_record_free);
    g_ptr_array_add(records, record(
        "10000000-0000-4000-8000-000000000001", "SPECIMEN Alpha.PNG",
        "screenshot", "Capture d'écran", "Écran de démonstration"));
    g_ptr_array_add(records, record(
        "10000000-0000-4000-8000-000000000002", "specimen-beta.jpg",
        "photo", "Photographie", "Portrait entièrement synthétique"));
    EvidenceSelectionModel *model = evidence_selection_model_new(records);
    GPtrArray *visible = evidence_selection_model_list_visible(model);
    g_assert_cmpuint(visible->len, ==, 2); g_ptr_array_unref(visible);
    g_assert_true(evidence_selection_model_select(model,
        "10000000-0000-4000-8000-000000000001"));
    evidence_selection_model_set_query(model, "ÉCRAN");
    visible = evidence_selection_model_list_visible(model);
    g_assert_cmpuint(visible->len, ==, 1); g_ptr_array_unref(visible);
    g_assert_nonnull(evidence_selection_model_get_selected(model));
    evidence_selection_model_set_type(model, "photo");
    g_assert_null(evidence_selection_model_get_selected(model));
    visible = evidence_selection_model_list_visible(model);
    g_assert_cmpuint(visible->len, ==, 0); g_ptr_array_unref(visible);
    evidence_selection_model_set_query(model, "");
    visible = evidence_selection_model_list_visible(model);
    g_assert_cmpuint(visible->len, ==, 1); g_ptr_array_unref(visible);
    evidence_selection_model_set_type(model, NULL);
    evidence_selection_model_set_query(model, "capture");
    visible = evidence_selection_model_list_visible(model);
    g_assert_cmpuint(visible->len, ==, 1); g_ptr_array_unref(visible);
    evidence_selection_model_free(model);
    g_ptr_array_unref(records);
}

static void test_owned_records_survive_source(void)
{
    GPtrArray *records = g_ptr_array_new_with_free_func(
        (GDestroyNotify) evidence_record_free);
    g_ptr_array_add(records, record(
        "10000000-0000-4000-8000-000000000003",
        "SPECIMEN durée de vie.png", "screenshot",
        "Capture d’écran", NULL));
    EvidenceSelectionModel *model = evidence_selection_model_new(records);
    g_ptr_array_unref(records);
    evidence_selection_model_set_query(model, "d");
    GPtrArray *visible = evidence_selection_model_list_visible(model);
    g_assert_cmpuint(visible->len, ==, 1);
    g_ptr_array_unref(visible);
    evidence_selection_model_free(model);
}

static void test_null_utf8_and_successive_queries(void)
{
    GPtrArray *records = g_ptr_array_new_with_free_func(
        (GDestroyNotify) evidence_record_free);
    EvidenceRecord *without_optional = record(
        "10000000-0000-4000-8000-000000000004",
        "SPECIMEN été.png", "screenshot", "Capture française", NULL);
    evidence_record_set_display_metadata(
        without_optional, "Capture française", NULL);
    g_ptr_array_add(records, without_optional);
    EvidenceSelectionModel *model = evidence_selection_model_new(records);
    const char *queries[] = {
        NULL, "", "é", "ÉTÉ", "capture", "screenshot",
        "image/png", "不存在", "a", "ab", "abc", NULL
    };
    for (guint i = 0; i < G_N_ELEMENTS(queries); i++) {
        evidence_selection_model_set_query(model, queries[i]);
        GPtrArray *visible = evidence_selection_model_list_visible(model);
        g_assert_nonnull(visible);
        g_ptr_array_unref(visible);
    }
    char *long_query = g_strnfill(4096, 'x');
    evidence_selection_model_set_query(model, long_query);
    GPtrArray *visible = evidence_selection_model_list_visible(model);
    g_assert_cmpuint(visible->len, ==, 0);
    g_ptr_array_unref(visible);
    g_free(long_query);
    evidence_selection_model_free(model);
    g_ptr_array_unref(records);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/evidence-selection/search-filter",
        test_search_filter_selection);
    g_test_add_func("/evidence-selection/owned-records",
        test_owned_records_survive_source);
    g_test_add_func("/evidence-selection/null-utf8-successive",
        test_null_utf8_and_successive_queries);
    return g_test_run();
}
