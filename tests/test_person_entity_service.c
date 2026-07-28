/******************************************************************************
 * @file test_person_entity_service.c
 * @brief Tests transactionnels de création d'une personne.
 ******************************************************************************/
#include "core/person_entity_service.h"
#include "database/database.h"
#include "dao/entity_dao.h"
#include "dao/person_role_assignment_dao.h"
#include "models/entity_record.h"
#include "database/statement.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
/** @brief Vérifie la création d'une personne présumée. */
static void test_person_entity_create(void)
{
    char *directory = NULL, *path = NULL, *identifier = NULL;
    Database *database = NULL;
    EntityDao *dao = NULL;
    EntityRecord *record = NULL;
    GError *error = NULL;
    PersonEntityInput input = {
        .designation = "Personne présumée liée aux comptes",
        .declared_name = NULL, .pseudonym = "vendeur_test",
        .identification_status = "suspected",
        .notes = "Identité non confirmée.", .confidence = 30,
        .evidence_identifier = NULL};
    PersonRoleAssignmentInput victim = {
        .role_code = "victim", .provenance_kind = "manual",
        .has_confidence = TRUE, .confidence = 70};
    PersonRoleAssignmentInput witness = {
        .role_code = "witness", .provenance_kind = "manual"};
    GPtrArray *roles = g_ptr_array_new();
    g_ptr_array_add(roles, &victim);
    g_ptr_array_add(roles, &witness);
    input.role_assignments = roles;
    directory = g_dir_make_tmp("labfy-person-service-test-XXXXXX", &error);
    assert(directory != NULL && error == NULL);
    path = g_build_filename(directory, "Enquete.sqlite", NULL);
    assert(database_initialize(path, "Enquete_Personne", directory));
    database = database_open(path); assert(database != NULL);
    assert(database_migrate_to_latest(database));
    assert(person_entity_service_create(database, &input, &identifier, &error));
    dao = entity_dao_new(database, &error);
    record = entity_dao_find_by_identifier(dao, identifier, &error);
    assert(record != NULL && error == NULL);
    assert(strcmp(entity_record_get_type_identifier(record), "person") == 0);
    assert(entity_record_get_confidence(record) == 30);
    PersonRoleAssignmentDao *role_dao =
        person_role_assignment_dao_new(database, &error);
    GPtrArray *saved_roles = person_role_assignment_dao_list_by_entity(
        role_dao, identifier, &error);
    assert(saved_roles != NULL && saved_roles->len == 2);
    g_ptr_array_unref(saved_roles);
    person_role_assignment_dao_free(role_dao);
    assert(entity_record_get_person_role(record) == PERSON_ROLE_UNCATEGORIZED);
    entity_record_free(record); record = NULL;
    assert(person_entity_service_update_role(database, identifier,
        PERSON_ROLE_VICTIM, &error));
    record = entity_dao_find_by_identifier(dao, identifier, &error);
    assert(record != NULL && error == NULL);
    assert(entity_record_get_person_role(record) == PERSON_ROLE_VICTIM);
    entity_record_free(record); record = NULL;
    assert(person_entity_service_update_role(database, identifier,
        PERSON_ROLE_IMPERSONATED_IDENTITY, &error));
    record = entity_dao_find_by_identifier(dao, identifier, &error);
    assert(record != NULL && error == NULL);
    assert(entity_record_get_person_role(record) ==
        PERSON_ROLE_IMPERSONATED_IDENTITY);
    assert(strcmp(person_role_to_code(PERSON_ROLE_IMPERSONATED_IDENTITY),
        "impersonated_identity") == 0);
    assert(strcmp(person_role_get_label(PERSON_ROLE_IMPERSONATED_IDENTITY),
        "Identité usurpée") == 0);
    assert(person_entity_service_update_display_name(database, identifier,
        "Vraie identité", &error));
    entity_record_free(record); record = NULL;
    record = entity_dao_find_by_identifier(dao, identifier, &error);
    assert(record != NULL && error == NULL);
    assert(strcmp(entity_record_get_label(record), "Vraie identité") == 0);
    entity_record_free(record); entity_dao_free(dao); database_close(database);
    assert(g_remove(path) == 0); assert(g_rmdir(directory) == 0);
    g_ptr_array_unref(roles);
    g_free(identifier); g_free(path); g_free(directory);
}

static gint64 count_rows(Database *database, const char *sql)
{
    DatabaseStatement *statement = database_statement_prepare(database, sql);
    int64_t count = -1;
    assert(statement != NULL);
    assert(database_statement_step(statement) == DATABASE_STATEMENT_STEP_ROW);
    assert(database_statement_column_int64(statement, 0, &count));
    database_statement_finalize(statement);
    return count;
}

static void test_person_entity_rollbacks(void)
{
    GError *error = NULL;
    char *directory = g_dir_make_tmp("labfy-person-rollback-XXXXXX", &error);
    char *path = g_build_filename(directory, "Enquete.sqlite", NULL);
    Database *database;
    PersonRoleAssignmentInput first = {
        .role_code="victim", .provenance_kind="manual"};
    PersonRoleAssignmentInput duplicate = {
        .role_code="victim", .provenance_kind="manual"};
    GPtrArray *roles = g_ptr_array_new();
    PersonEntityInput input = {
        .designation="Personne rollback SPECIMEN",
        .identification_status="unknown", .confidence=20,
        .role_assignments=roles};
    g_assert_true(database_initialize(path, "Rollback synthétique", directory));
    database = database_open(path);
    g_assert_true(database_migrate_to_latest(database));
    g_ptr_array_add(roles, &first); g_ptr_array_add(roles, &duplicate);
    g_assert_false(person_entity_service_create(
        database, &input, NULL, &error));
    g_clear_error(&error);
    g_assert_cmpint(count_rows(database,
        "SELECT COUNT(*) FROM entites WHERE label='Personne rollback SPECIMEN';"),
        ==, 0);
    g_assert_cmpint(count_rows(database,
        "SELECT COUNT(*) FROM person_role_assignments;"), ==, 0);
    g_ptr_array_set_size(roles, 1);
    input.evidence_identifier =
        "10000000-0000-4000-8000-000000000999";
    first.evidence_identifier = NULL;
    g_assert_false(person_entity_service_create(
        database, &input, NULL, &error));
    g_clear_error(&error);
    g_assert_cmpint(count_rows(database,
        "SELECT COUNT(*) FROM entites WHERE label='Personne rollback SPECIMEN';"),
        ==, 0);
    g_assert_cmpint(count_rows(database,
        "SELECT COUNT(*) FROM person_role_assignments;"), ==, 0);
    g_assert_cmpint(count_rows(database,
        "SELECT COUNT(*) FROM preuve_entites;"), ==, 0);
    g_assert_cmpint(count_rows(database,
        "SELECT COUNT(*) FROM preuve_entite_sources;"), ==, 0);
    g_ptr_array_unref(roles); database_close(database);
    g_assert_cmpint(g_remove(path), ==, 0);
    g_assert_cmpint(g_rmdir(directory), ==, 0);
    g_free(path); g_free(directory);
}
int main(void)
{
    test_person_entity_create();
    test_person_entity_rollbacks();
    puts("PersonEntityService : tous les tests sont valides."); return 0;
}
