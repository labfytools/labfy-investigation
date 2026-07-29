#include "models/person_evidence_selection.h"
#include <glib.h>
#include <string.h>

static EvidenceRecord *record(const char *identifier, const char *name,
    const char *sha)
{
    EvidenceRecord *record = evidence_record_new(identifier, name, name,
        "01_Preuves_Originales/specimen.png", "screenshot", 12, sha,
        "2026-07-28T10:00:00Z", NULL, NULL, "SPECIMEN",
        EVIDENCE_INTEGRITY_STATUS_VALID, NULL);
    evidence_record_set_display_metadata(record, "Capture", "image/png");
    return record;
}
static void test_collection(void)
{
    PersonEvidenceSelection *selection = person_evidence_selection_new();
    EvidenceRecord *first = record(
        "10000000-0000-4000-8000-000000000001", "SPECIMEN-1.png",
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    EvidenceRecord *second = record(
        "10000000-0000-4000-8000-000000000002", "SPECIMEN-2.png",
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    GError *error = NULL;
    g_assert_cmpuint(person_evidence_selection_get_count(selection), ==, 0);
    g_assert_true(person_evidence_selection_add_existing(
        selection, first, &error));
    g_assert_true(person_evidence_selection_add_existing(
        selection, second, &error));
    g_assert_true(person_evidence_selection_add_staged(selection,
        "/tmp/SPECIMEN-source.jpg", "/tmp/SPECIMEN-stage.jpg",
        "SPECIMEN.jpg", "image/jpeg", "photo", 24,
        "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",
        "description", "2026-07-28T10:00:00Z", &error));
    g_assert_cmpuint(person_evidence_selection_get_count(selection), ==, 3);
    g_assert_cmpstr(person_evidence_selection_item_get_original_name(
        person_evidence_selection_get(selection, 0)), ==, "SPECIMEN-1.png");
    g_assert_true(person_evidence_selection_set_active(selection,
        person_evidence_selection_item_get_identifier(
            person_evidence_selection_get(selection, 2))));
    g_assert_cmpstr(person_evidence_selection_item_get_original_name(
        person_evidence_selection_get_active(selection)), ==, "SPECIMEN.jpg");
    g_assert_true(person_evidence_selection_set_type(selection,
        person_evidence_selection_item_get_identifier(
            person_evidence_selection_get(selection, 2)), "document"));
    g_assert_true(person_evidence_selection_remove(selection,
        person_evidence_selection_item_get_identifier(
            person_evidence_selection_get(selection, 1))));
    g_assert_cmpuint(person_evidence_selection_get_count(selection), ==, 2);
    g_assert_true(person_evidence_selection_is_confirmable(selection));
    evidence_record_free(first); evidence_record_free(second);
    person_evidence_selection_free(selection);
}
static void test_duplicates(void)
{
    PersonEvidenceSelection *selection = person_evidence_selection_new();
    EvidenceRecord *first = record(
        "10000000-0000-4000-8000-000000000001", "SPECIMEN.png",
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    GError *error = NULL;
    g_assert_true(person_evidence_selection_add_existing(
        selection, first, &error));
    g_assert_false(person_evidence_selection_add_existing(
        selection, first, &error));
    g_assert_error(error,
        g_quark_from_static_string("person-evidence-selection-error"), 2);
    g_clear_error(&error);
    g_assert_false(person_evidence_selection_add_staged(selection,
        "/tmp/SPECIMEN-copy", "/tmp/SPECIMEN-stage", "copy",
        "image/png", "photo", 12,
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        NULL, "2026-07-28T10:00:00Z", &error));
    g_assert_nonnull(strstr(error->message, "déjà"));
    g_clear_error(&error);
    g_assert_true(person_evidence_selection_remove(selection,
        person_evidence_selection_item_get_identifier(
            person_evidence_selection_get(selection, 0))));
    g_assert_true(person_evidence_selection_add_existing(
        selection, first, &error));
    evidence_record_free(first);
    person_evidence_selection_free(selection);
}
static void test_deep_copy_survives_sources(void)
{
    PersonEvidenceSelection *source = person_evidence_selection_new();
    EvidenceRecord *incomplete = record(
        "10000000-0000-4000-8000-000000000003", "SPECIMEN-NULL.jpg",
        "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd");
    evidence_record_set_display_metadata(incomplete, NULL, NULL);
    g_assert_true(person_evidence_selection_add_existing(
        source, incomplete, NULL));
    g_assert_true(person_evidence_selection_set_type(source,
        person_evidence_selection_item_get_identifier(
            person_evidence_selection_get(source, 0)), "email"));
    PersonEvidenceSelection *copy = person_evidence_selection_copy(source);
    evidence_record_free(incomplete);
    person_evidence_selection_free(source);
    g_assert_cmpuint(person_evidence_selection_get_count(copy), ==, 1);
    const PersonEvidenceSelectionItem *item =
        person_evidence_selection_get(copy, 0);
    g_assert_cmpstr(person_evidence_selection_item_get_original_name(item),
        ==, "SPECIMEN-NULL.jpg");
    g_assert_cmpstr(person_evidence_selection_item_get_type_identifier(item),
        ==, "email");
    g_assert_true(person_evidence_selection_remove(copy,
        person_evidence_selection_item_get_identifier(item)));
    person_evidence_selection_free(copy);
}
static void test_staged_copy_keeps_selection_identity(void)
{
    PersonEvidenceSelection *source = person_evidence_selection_new();
    g_assert_true(person_evidence_selection_add_staged(source,
        "/tmp/SPECIMEN-source.png", "/tmp/SPECIMEN-stage.png",
        "SPECIMEN.png", "image/png", "identity", 24,
        "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee",
        "SPECIMEN synthétique", "2026-07-28T10:00:00Z", NULL));
    const PersonEvidenceSelectionItem *source_item =
        person_evidence_selection_get(source, 0);
    char *selection_identifier = g_strdup(
        person_evidence_selection_item_get_identifier(source_item));
    g_assert_true(person_evidence_selection_set_active(
        source, selection_identifier));

    PersonEvidenceSelection *copy = person_evidence_selection_copy(source);
    const PersonEvidenceSelectionItem *copied_item =
        person_evidence_selection_get(copy, 0);
    g_assert_cmpstr(person_evidence_selection_item_get_identifier(copied_item),
        ==, selection_identifier);
    g_assert_cmpstr(person_evidence_selection_item_get_identifier(
        person_evidence_selection_get_active(copy)), ==,
        selection_identifier);
    g_assert_cmpint(person_evidence_selection_item_get_origin(copied_item),
        ==, PERSON_EVIDENCE_ORIGIN_STAGED);
    g_assert_cmpint(person_evidence_selection_item_get_state(copied_item),
        ==, PERSON_EVIDENCE_STATE_READY);

    g_free(selection_identifier);
    person_evidence_selection_free(copy);
    person_evidence_selection_free(source);
}
int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/person-evidence/collection", test_collection);
    g_test_add_func("/person-evidence/duplicates", test_duplicates);
    g_test_add_func("/person-evidence/deep-copy",
        test_deep_copy_survives_sources);
    g_test_add_func("/person-evidence/staged-copy-selection-identity",
        test_staged_copy_keeps_selection_identity);
    return g_test_run();
}
