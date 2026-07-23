#include "core/extraction_drop_service.h"
#include "dao/entity_dao.h"
#include "dao/evidence_dao.h"
#include "dao/evidence_entity_dao.h"
#include "database/database.h"
#include "models/entity_record.h"

#include <glib.h>
#include <glib/gstdio.h>

typedef struct
{
    char *root;
    char *extractions;
    char *file_path;
    char *database_path;
    Database *database;
} ExtractionDropFixture;

static void fixture_setup(ExtractionDropFixture *fixture, gconstpointer data)
{
    GError *error = NULL;
    (void) data;
    fixture->root = g_dir_make_tmp("labfy-extraction-drop-XXXXXX", &error);
    g_assert_no_error(error);
    fixture->extractions = g_build_filename(fixture->root,
        "02_Preuves_Traitees", "Extractions", "OSINT", NULL);
    g_assert_cmpint(g_mkdir_with_parents(fixture->extractions, 0700), ==, 0);
    fixture->file_path = g_build_filename(fixture->extractions,
        "resultat.txt", NULL);
    g_assert_true(g_file_set_contents(fixture->file_path,
        "Cible : exemple.test\nAction : test\n\nRésultat public", -1, &error));
    g_assert_no_error(error);
    fixture->database_path = g_build_filename(fixture->root,
        "Enquete.sqlite", NULL);
    g_assert_true(database_initialize(fixture->database_path,
        "Extraction", fixture->root));
    fixture->database = database_open(fixture->database_path);
    g_assert_nonnull(fixture->database);
    g_assert_true(database_migrate_to_latest(fixture->database));
}

static void fixture_teardown(ExtractionDropFixture *fixture,
    gconstpointer data)
{
    (void) data;
    database_close(fixture->database);
    g_remove(fixture->file_path);
    g_remove(fixture->database_path);
    g_rmdir(fixture->extractions);
    {
        char *directory = g_path_get_dirname(fixture->extractions);
        g_rmdir(directory);
        g_free(directory);
    }
    {
        char *directory = g_build_filename(fixture->root,
            "02_Preuves_Traitees", NULL);
        g_rmdir(directory);
        g_free(directory);
    }
    g_rmdir(fixture->root);
    g_free(fixture->database_path);
    g_free(fixture->file_path);
    g_free(fixture->extractions);
    g_free(fixture->root);
}

static char *insert_entity(Database *database)
{
    EntityDao *dao = NULL;
    EntityRecord *record = NULL;
    char *identifier = g_uuid_string_random();
    GError *error = NULL;
    dao = entity_dao_new(database, &error);
    record = entity_record_new(identifier, "other", "Destination",
        "Destination", NULL, 50, "2026-07-23T12:00:00Z",
        "2026-07-23T12:00:00Z", ENTITY_STATUS_ACTIVE, &error);
    g_assert_no_error(error);
    g_assert_true(entity_dao_insert(dao, record, &error));
    g_assert_no_error(error);
    entity_record_free(record);
    entity_dao_free(dao);
    return identifier;
}

static void test_prepare_rejects_outside(ExtractionDropFixture *fixture,
    gconstpointer data)
{
    ExtractionDropInfo *info = NULL;
    char *outside = g_build_filename(fixture->root, "hors.txt", NULL);
    GError *error = NULL;
    (void) data;
    g_assert_true(g_file_set_contents(outside, "hors enquête", -1, &error));
    g_assert_no_error(error);
    info = extraction_drop_service_prepare(fixture->root, outside, &error);
    g_assert_null(info);
    g_assert_error(error, EXTRACTION_DROP_SERVICE_ERROR,
        EXTRACTION_DROP_SERVICE_ERROR_OUTSIDE_INVESTIGATION);
    g_clear_error(&error);
    g_remove(outside);
    g_free(outside);
}

static void test_attach_persists_and_is_idempotent(
    ExtractionDropFixture *fixture, gconstpointer data)
{
    ExtractionDropInfo *info = NULL;
    EvidenceDao *evidence_dao = NULL;
    EvidenceEntityDao *link_dao = NULL;
    GPtrArray *records = NULL;
    char *entity_identifier = NULL;
    gboolean exists = FALSE;
    GError *error = NULL;
    (void) data;

    info = extraction_drop_service_prepare(fixture->root,
        fixture->file_path, &error);
    g_assert_no_error(error);
    g_assert_nonnull(info);
    g_assert_cmpstr(extraction_drop_info_get_source(info), ==,
        "Cible : exemple.test");
    entity_identifier = insert_entity(fixture->database);
    g_assert_true(extraction_drop_service_attach(fixture->database, info,
        entity_identifier, &error));
    g_assert_no_error(error);
    g_assert_true(extraction_drop_service_attach(fixture->database, info,
        entity_identifier, &error));
    g_assert_no_error(error);
    evidence_dao = evidence_dao_new(fixture->database, &error);
    records = evidence_dao_list_all(evidence_dao, &error);
    g_assert_no_error(error);
    g_assert_cmpuint(records->len, ==, 1U);
    link_dao = evidence_entity_dao_new(fixture->database, &error);
    g_assert_true(evidence_entity_dao_exists(link_dao,
        evidence_record_get_identifier(g_ptr_array_index(records, 0U)),
        entity_identifier, &exists, &error));
    g_assert_no_error(error);
    g_assert_true(exists);
    evidence_entity_dao_free(link_dao);
    g_ptr_array_unref(records);
    evidence_dao_free(evidence_dao);
    g_free(entity_identifier);
    extraction_drop_info_free(info);
}

static void test_create_entity_persists(ExtractionDropFixture *fixture,
    gconstpointer data)
{
    ExtractionDropInfo *info = NULL;
    EntityDao *entity_dao = NULL;
    EntityRecord *entity = NULL;
    char *identifier = NULL;
    GError *error = NULL;
    (void) data;
    info = extraction_drop_service_prepare(fixture->root,
        fixture->file_path, &error);
    g_assert_true(extraction_drop_service_create_entity(fixture->database,
        info, &identifier, &error));
    g_assert_no_error(error);
    g_assert_true(g_uuid_string_is_valid(identifier));
    entity_dao = entity_dao_new(fixture->database, &error);
    entity = entity_dao_find_by_identifier(entity_dao, identifier, &error);
    g_assert_no_error(error);
    g_assert_nonnull(entity);
    g_assert_cmpstr(entity_record_get_type_identifier(entity), ==, "other");
    entity_record_free(entity);
    entity_dao_free(entity_dao);
    g_free(identifier);
    extraction_drop_info_free(info);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add("/extraction-drop/outside", ExtractionDropFixture, NULL,
        fixture_setup, test_prepare_rejects_outside, fixture_teardown);
    g_test_add("/extraction-drop/attach", ExtractionDropFixture, NULL,
        fixture_setup, test_attach_persists_and_is_idempotent, fixture_teardown);
    g_test_add("/extraction-drop/create", ExtractionDropFixture, NULL,
        fixture_setup, test_create_entity_persists, fixture_teardown);
    return g_test_run();
}
