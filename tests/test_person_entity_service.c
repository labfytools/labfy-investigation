/******************************************************************************
 * @file test_person_entity_service.c
 * @brief Tests transactionnels de création d'une personne.
 ******************************************************************************/
#include "core/person_entity_service.h"
#include "database/database.h"
#include "dao/entity_dao.h"
#include "models/entity_record.h"
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
    entity_record_free(record); entity_dao_free(dao); database_close(database);
    assert(g_remove(path) == 0); assert(g_rmdir(directory) == 0);
    g_free(identifier); g_free(path); g_free(directory);
}
int main(void)
{
    test_person_entity_create();
    puts("PersonEntityService : tous les tests sont valides."); return 0;
}
