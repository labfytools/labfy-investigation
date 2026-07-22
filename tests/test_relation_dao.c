/******************************************************************************
 * @file test_relation_dao.c
 * @brief Tests du DAO des relations entre entités.
 ******************************************************************************/

#include "dao/entity_dao.h"
#include "dao/relation_dao.h"

#include "database/database.h"
#include "database/statement.h"
#include "models/entity_record.h"
#include "models/relation_record.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

#define TEST_ENTITY_A_IDENTIFIER \
    "10000000-0000-4000-8000-000000000001"

#define TEST_ENTITY_B_IDENTIFIER \
    "20000000-0000-4000-8000-000000000001"

#define TEST_ENTITY_C_IDENTIFIER \
    "30000000-0000-4000-8000-000000000001"

/**
 * @brief Environnement temporaire d'un test RelationDao.
 */
typedef struct
{
    char *temporary_directory;
    char *database_path;

    Database *database;
    EntityDao *entity_dao;
    RelationDao *relation_dao;
} TestRelationDaoFixture;

/**
 * @brief Vérifie le domaine et le code d'une erreur RelationDao.
 */
static void test_relation_dao_assert_error(
    const GError *error,
    RelationDaoError expected_code
)
{
    assert(error != NULL);

    assert(
        error->domain ==
        RELATION_DAO_ERROR
    );

    assert(
        error->code ==
        (gint) expected_code
    );

    assert(error->message != NULL);
    assert(error->message[0] != '\0');
}

/**
 * @brief Crée une base temporaire et ses DAO.
 */
static TestRelationDaoFixture test_relation_dao_fixture_create(void)
{
    TestRelationDaoFixture fixture =
        {0};

    GError *error =
        NULL;

    fixture.temporary_directory =
        g_dir_make_tmp(
            "labfy-relation-dao-test-XXXXXX",
            &error
        );

    assert(fixture.temporary_directory != NULL);
    assert(error == NULL);

    fixture.database_path =
        g_build_filename(
            fixture.temporary_directory,
            "Enquete.sqlite",
            NULL
        );

    assert(fixture.database_path != NULL);

    assert(
        database_initialize(
            fixture.database_path,
            "Enquete_RelationDao",
            fixture.temporary_directory
        )
    );

    fixture.database =
        database_open(
            fixture.database_path
        );

    assert(fixture.database != NULL);

    fixture.entity_dao =
        entity_dao_new(
            fixture.database,
            &error
        );

    assert(fixture.entity_dao != NULL);
    assert(error == NULL);

    fixture.relation_dao =
        relation_dao_new(
            fixture.database,
            &error
        );

    assert(fixture.relation_dao != NULL);
    assert(error == NULL);

    return fixture;
}

/**
 * @brief Libère la fixture et supprime sa base temporaire.
 */
static void test_relation_dao_fixture_clear(
    TestRelationDaoFixture *fixture
)
{
    assert(fixture != NULL);

    relation_dao_free(
        fixture->relation_dao
    );

    entity_dao_free(
        fixture->entity_dao
    );

    database_close(
        fixture->database
    );

    assert(
        g_remove(
            fixture->database_path
        ) == 0
    );

    assert(
        g_rmdir(
            fixture->temporary_directory
        ) == 0
    );

    g_free(
        fixture->database_path
    );

    g_free(
        fixture->temporary_directory
    );

    fixture->relation_dao =
        NULL;

    fixture->entity_dao =
        NULL;

    fixture->database =
        NULL;

    fixture->database_path =
        NULL;

    fixture->temporary_directory =
        NULL;
}

/**
 * @brief Crée une entité de test.
 */
static EntityRecord *test_relation_dao_create_entity(
    const char *identifier,
    const char *value
)
{
    EntityRecord *entity_record =
        NULL;

    GError *error =
        NULL;

    entity_record =
        entity_record_new(
            identifier,
            "person",
            value,
            NULL,
            NULL,
            80,
            "2026-07-20T20:00:00Z",
            "2026-07-20T20:00:00Z",
            ENTITY_STATUS_ACTIVE,
            &error
        );

    assert(entity_record != NULL);
    assert(error == NULL);

    return entity_record;
}

/**
 * @brief Insère une entité de test.
 */
static void test_relation_dao_insert_entity(
    TestRelationDaoFixture *fixture,
    const char *identifier,
    const char *value
)
{
    EntityRecord *entity_record =
        NULL;

    GError *error =
        NULL;

    assert(fixture != NULL);

    entity_record =
        test_relation_dao_create_entity(
            identifier,
            value
        );

    assert(
        entity_dao_insert(
            fixture->entity_dao,
            entity_record,
            &error
        )
    );

    assert(error == NULL);

    entity_record_free(
        entity_record
    );
}

/**
 * @brief Insère les trois entités couramment utilisées par les tests.
 */
static void test_relation_dao_insert_standard_entities(
    TestRelationDaoFixture *fixture
)
{
    test_relation_dao_insert_entity(
        fixture,
        TEST_ENTITY_A_IDENTIFIER,
        "Personne A"
    );

    test_relation_dao_insert_entity(
        fixture,
        TEST_ENTITY_B_IDENTIFIER,
        "Personne B"
    );

    test_relation_dao_insert_entity(
        fixture,
        TEST_ENTITY_C_IDENTIFIER,
        "Personne C"
    );
}

/**
 * @brief Crée une relation de test.
 */
static RelationRecord *test_relation_dao_create_relation(
    const char *identifier,
    const char *source_entity_identifier,
    const char *target_entity_identifier,
    const char *relation_type,
    const char *label,
    const char *justification,
    gint confidence,
    const char *created_at,
    const char *updated_at,
    RelationStatus status
)
{
    RelationRecord *relation_record =
        NULL;

    GError *error =
        NULL;

    relation_record =
        relation_record_new(
            identifier,
            source_entity_identifier,
            target_entity_identifier,
            relation_type,
            label,
            justification,
            confidence,
            created_at,
            updated_at,
            status,
            &error
        );

    assert(relation_record != NULL);
    assert(error == NULL);

    return relation_record;
}

/**
 * @brief Insère une relation et vérifie le succès.
 */
static void test_relation_dao_insert_relation(
    TestRelationDaoFixture *fixture,
    RelationRecord *relation_record
)
{
    GError *error =
        NULL;

    assert(fixture != NULL);
    assert(relation_record != NULL);

    assert(
        relation_dao_insert(
            fixture->relation_dao,
            relation_record,
            &error
        )
    );

    assert(error == NULL);
}

/**
 * @brief Retourne le nombre de relations persistées.
 */
static int64_t test_relation_dao_count_rows(
    Database *database
)
{
    DatabaseStatement *statement =
        NULL;

    int64_t relation_count =
        -1;

    assert(database != NULL);

    statement =
        database_statement_prepare(
            database,
            "SELECT COUNT(*) "
            "FROM relations;"
        );

    assert(statement != NULL);

    assert(
        database_statement_step(
            statement
        ) == DATABASE_STATEMENT_STEP_ROW
    );

    assert(
        database_statement_column_int64(
            statement,
            0,
            &relation_count
        )
    );

    assert(
        database_statement_step(
            statement
        ) == DATABASE_STATEMENT_STEP_DONE
    );

    database_statement_finalize(
        statement
    );

    return relation_count;
}

/**
 * @brief Vérifie le refus d'une connexion absente.
 */
static void test_relation_dao_new_null_database(void)
{
    RelationDao *relation_dao =
        NULL;

    GError *error =
        NULL;

    relation_dao =
        relation_dao_new(
            NULL,
            &error
        );

    assert(relation_dao == NULL);

    test_relation_dao_assert_error(
        error,
        RELATION_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );
}

/**
 * @brief Vérifie la création et la destruction du DAO.
 */
static void test_relation_dao_lifecycle(void)
{
    Database *database =
        NULL;

    RelationDao *relation_dao =
        NULL;

    GError *error =
        NULL;

    database =
        database_open(
            ":memory:"
        );

    assert(database != NULL);

    relation_dao =
        relation_dao_new(
            database,
            &error
        );

    assert(relation_dao != NULL);
    assert(error == NULL);

    relation_dao_free(
        relation_dao
    );

    relation_dao_free(
        NULL
    );

    database_close(
        database
    );
}

/**
 * @brief Vérifie l'insertion complète d'une relation.
 */
static void test_relation_dao_insert_valid_full(void)
{
    TestRelationDaoFixture fixture =
        test_relation_dao_fixture_create();

    RelationRecord *relation_record =
        NULL;

    DatabaseStatement *statement =
        NULL;

    char *identifier =
        NULL;

    char *source_identifier =
        NULL;

    char *target_identifier =
        NULL;

    char *relation_type =
        NULL;

    char *label =
        NULL;

    char *justification =
        NULL;

    char *created_at =
        NULL;

    char *updated_at =
        NULL;

    char *status =
        NULL;

    int64_t confidence =
        -1;

    test_relation_dao_insert_standard_entities(
        &fixture
    );

    relation_record =
        test_relation_dao_create_relation(
            "40000000-0000-4000-8000-000000000001",
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_B_IDENTIFIER,
            "uses",
            "Utilise cette identité",
            "Association observée dans une conversation.",
            90,
            "2026-07-20T20:10:00Z",
            "2026-07-20T20:15:00Z",
            RELATION_STATUS_DISPUTED
        );

    test_relation_dao_insert_relation(
        &fixture,
        relation_record
    );

    statement =
        database_statement_prepare(
            fixture.database,
            "SELECT "
            "    id,"
            "    entite_source_id,"
            "    entite_cible_id,"
            "    type_relation,"
            "    label,"
            "    justification,"
            "    confiance,"
            "    created_at,"
            "    updated_at,"
            "    status "
            "FROM relations "
            "WHERE id = ?;"
        );

    assert(statement != NULL);

    assert(
        database_statement_bind_text(
            statement,
            1,
            "40000000-0000-4000-8000-000000000001"
        )
    );

    assert(
        database_statement_step(
            statement
        ) == DATABASE_STATEMENT_STEP_ROW
    );

    assert(
        database_statement_column_text(
            statement,
            0,
            &identifier
        )
    );

    assert(
        database_statement_column_text(
            statement,
            1,
            &source_identifier
        )
    );

    assert(
        database_statement_column_text(
            statement,
            2,
            &target_identifier
        )
    );

    assert(
        database_statement_column_text(
            statement,
            3,
            &relation_type
        )
    );

    assert(
        database_statement_column_text(
            statement,
            4,
            &label
        )
    );

    assert(
        database_statement_column_text(
            statement,
            5,
            &justification
        )
    );

    assert(
        database_statement_column_int64(
            statement,
            6,
            &confidence
        )
    );

    assert(
        database_statement_column_text(
            statement,
            7,
            &created_at
        )
    );

    assert(
        database_statement_column_text(
            statement,
            8,
            &updated_at
        )
    );

    assert(
        database_statement_column_text(
            statement,
            9,
            &status
        )
    );

    assert(
        database_statement_step(
            statement
        ) == DATABASE_STATEMENT_STEP_DONE
    );

    assert(
        strcmp(
            identifier,
            "40000000-0000-4000-8000-000000000001"
        ) == 0
    );

    assert(
        strcmp(
            source_identifier,
            TEST_ENTITY_A_IDENTIFIER
        ) == 0
    );

    assert(
        strcmp(
            target_identifier,
            TEST_ENTITY_B_IDENTIFIER
        ) == 0
    );

    assert(
        strcmp(
            relation_type,
            "uses"
        ) == 0
    );

    assert(
        strcmp(
            label,
            "Utilise cette identité"
        ) == 0
    );

    assert(
        strcmp(
            justification,
            "Association observée dans une conversation."
        ) == 0
    );

    assert(confidence == 90);

    assert(
        strcmp(
            created_at,
            "2026-07-20T20:10:00Z"
        ) == 0
    );

    assert(
        strcmp(
            updated_at,
            "2026-07-20T20:15:00Z"
        ) == 0
    );

    assert(
        strcmp(
            status,
            "disputed"
        ) == 0
    );

    database_statement_finalize(
        statement
    );

    g_free(
        status
    );

    g_free(
        updated_at
    );

    g_free(
        created_at
    );

    g_free(
        justification
    );

    g_free(
        label
    );

    g_free(
        relation_type
    );

    g_free(
        target_identifier
    );

    g_free(
        source_identifier
    );

    g_free(
        identifier
    );

    relation_record_free(
        relation_record
    );

    test_relation_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie l'insertion avec champs facultatifs absents.
 */
static void test_relation_dao_insert_optional_null(void)
{
    TestRelationDaoFixture fixture =
        test_relation_dao_fixture_create();

    RelationRecord *relation_record =
        NULL;

    DatabaseStatement *statement =
        NULL;

    bool label_is_null =
        false;

    bool justification_is_null =
        false;

    test_relation_dao_insert_standard_entities(
        &fixture
    );

    relation_record =
        test_relation_dao_create_relation(
            "40000000-0000-4000-8000-000000000002",
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_B_IDENTIFIER,
            "knows",
            NULL,
            NULL,
            50,
            "2026-07-20T20:20:00Z",
            "2026-07-20T20:20:00Z",
            RELATION_STATUS_ACTIVE
        );

    test_relation_dao_insert_relation(
        &fixture,
        relation_record
    );

    statement =
        database_statement_prepare(
            fixture.database,
            "SELECT "
            "    label,"
            "    justification "
            "FROM relations "
            "WHERE id = ?;"
        );

    assert(statement != NULL);

    assert(
        database_statement_bind_text(
            statement,
            1,
            "40000000-0000-4000-8000-000000000002"
        )
    );

    assert(
        database_statement_step(
            statement
        ) == DATABASE_STATEMENT_STEP_ROW
    );

    assert(
        database_statement_column_is_null(
            statement,
            0,
            &label_is_null
        )
    );

    assert(
        database_statement_column_is_null(
            statement,
            1,
            &justification_is_null
        )
    );

    assert(label_is_null);
    assert(justification_is_null);

    database_statement_finalize(
        statement
    );

    relation_record_free(
        relation_record
    );

    test_relation_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d'un UUID de relation dupliqué.
 */
static void test_relation_dao_insert_duplicate_identifier(void)
{
    TestRelationDaoFixture fixture =
        test_relation_dao_fixture_create();

    RelationRecord *first_record =
        NULL;

    RelationRecord *second_record =
        NULL;

    GError *error =
        NULL;

    test_relation_dao_insert_standard_entities(
        &fixture
    );

    first_record =
        test_relation_dao_create_relation(
            "40000000-0000-4000-8000-000000000003",
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_B_IDENTIFIER,
            "uses",
            NULL,
            NULL,
            80,
            "2026-07-20T20:30:00Z",
            "2026-07-20T20:30:00Z",
            RELATION_STATUS_ACTIVE
        );

    second_record =
        test_relation_dao_create_relation(
            "40000000-0000-4000-8000-000000000003",
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_C_IDENTIFIER,
            "controls",
            NULL,
            NULL,
            70,
            "2026-07-20T20:31:00Z",
            "2026-07-20T20:31:00Z",
            RELATION_STATUS_ACTIVE
        );

    test_relation_dao_insert_relation(
        &fixture,
        first_record
    );

    assert(
        !relation_dao_insert(
            fixture.relation_dao,
            second_record,
            &error
        )
    );

    test_relation_dao_assert_error(
        error,
        RELATION_DAO_ERROR_CONSTRAINT
    );

    assert(
        strstr(
            error->message,
            "identifiant"
        ) != NULL
    );

    assert(
        test_relation_dao_count_rows(
            fixture.database
        ) == 1
    );

    g_clear_error(
        &error
    );

    relation_record_free(
        second_record
    );

    relation_record_free(
        first_record
    );

    test_relation_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d'une entité source inexistante.
 */
static void test_relation_dao_insert_missing_source(void)
{
    TestRelationDaoFixture fixture =
        test_relation_dao_fixture_create();

    RelationRecord *relation_record =
        NULL;

    GError *error =
        NULL;

    test_relation_dao_insert_entity(
        &fixture,
        TEST_ENTITY_B_IDENTIFIER,
        "Personne B"
    );

    relation_record =
        test_relation_dao_create_relation(
            "40000000-0000-4000-8000-000000000004",
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_B_IDENTIFIER,
            "uses",
            NULL,
            NULL,
            80,
            "2026-07-20T20:40:00Z",
            "2026-07-20T20:40:00Z",
            RELATION_STATUS_ACTIVE
        );

    assert(
        !relation_dao_insert(
            fixture.relation_dao,
            relation_record,
            &error
        )
    );

    test_relation_dao_assert_error(
        error,
        RELATION_DAO_ERROR_CONSTRAINT
    );

    assert(
        strstr(
            error->message,
            "source"
        ) != NULL
    );

    g_clear_error(
        &error
    );

    relation_record_free(
        relation_record
    );

    test_relation_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d'une entité cible inexistante.
 */
static void test_relation_dao_insert_missing_target(void)
{
    TestRelationDaoFixture fixture =
        test_relation_dao_fixture_create();

    RelationRecord *relation_record =
        NULL;

    GError *error =
        NULL;

    test_relation_dao_insert_entity(
        &fixture,
        TEST_ENTITY_A_IDENTIFIER,
        "Personne A"
    );

    relation_record =
        test_relation_dao_create_relation(
            "40000000-0000-4000-8000-000000000005",
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_B_IDENTIFIER,
            "uses",
            NULL,
            NULL,
            80,
            "2026-07-20T20:50:00Z",
            "2026-07-20T20:50:00Z",
            RELATION_STATUS_ACTIVE
        );

    assert(
        !relation_dao_insert(
            fixture.relation_dao,
            relation_record,
            &error
        )
    );

    test_relation_dao_assert_error(
        error,
        RELATION_DAO_ERROR_CONSTRAINT
    );

    assert(
        strstr(
            error->message,
            "cible"
        ) != NULL
    );

    g_clear_error(
        &error
    );

    relation_record_free(
        relation_record
    );

    test_relation_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d'un doublon source, cible et type.
 */
static void test_relation_dao_insert_duplicate_triple(void)
{
    TestRelationDaoFixture fixture =
        test_relation_dao_fixture_create();

    RelationRecord *first_record =
        NULL;

    RelationRecord *second_record =
        NULL;

    GError *error =
        NULL;

    test_relation_dao_insert_standard_entities(
        &fixture
    );

    first_record =
        test_relation_dao_create_relation(
            "40000000-0000-4000-8000-000000000006",
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_B_IDENTIFIER,
            "uses",
            "Première relation",
            NULL,
            80,
            "2026-07-20T21:00:00Z",
            "2026-07-20T21:00:00Z",
            RELATION_STATUS_ACTIVE
        );

    second_record =
        test_relation_dao_create_relation(
            "40000000-0000-4000-8000-000000000007",
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_B_IDENTIFIER,
            "uses",
            "Deuxième relation",
            NULL,
            40,
            "2026-07-20T21:01:00Z",
            "2026-07-20T21:01:00Z",
            RELATION_STATUS_DISPUTED
        );

    test_relation_dao_insert_relation(
        &fixture,
        first_record
    );

    assert(
        !relation_dao_insert(
            fixture.relation_dao,
            second_record,
            &error
        )
    );

    test_relation_dao_assert_error(
        error,
        RELATION_DAO_ERROR_CONSTRAINT
    );

    assert(
        strstr(
            error->message,
            "orientée"
        ) != NULL
    );

    assert(
        test_relation_dao_count_rows(
            fixture.database
        ) == 1
    );

    g_clear_error(
        &error
    );

    relation_record_free(
        second_record
    );

    relation_record_free(
        first_record
    );

    test_relation_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie plusieurs types et les deux orientations A-B et B-A.
 */
static void test_relation_dao_orientation_and_multiple_types(void)
{
    TestRelationDaoFixture fixture =
        test_relation_dao_fixture_create();

    RelationRecord *first_record =
        NULL;

    RelationRecord *second_record =
        NULL;

    RelationRecord *reverse_record =
        NULL;

    test_relation_dao_insert_standard_entities(
        &fixture
    );

    first_record =
        test_relation_dao_create_relation(
            "40000000-0000-4000-8000-000000000008",
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_B_IDENTIFIER,
            "uses",
            NULL,
            NULL,
            80,
            "2026-07-20T21:10:00Z",
            "2026-07-20T21:10:00Z",
            RELATION_STATUS_ACTIVE
        );

    second_record =
        test_relation_dao_create_relation(
            "40000000-0000-4000-8000-000000000009",
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_B_IDENTIFIER,
            "controls",
            NULL,
            NULL,
            70,
            "2026-07-20T21:11:00Z",
            "2026-07-20T21:11:00Z",
            RELATION_STATUS_ACTIVE
        );

    reverse_record =
        test_relation_dao_create_relation(
            "40000000-0000-4000-8000-000000000010",
            TEST_ENTITY_B_IDENTIFIER,
            TEST_ENTITY_A_IDENTIFIER,
            "uses",
            NULL,
            NULL,
            60,
            "2026-07-20T21:12:00Z",
            "2026-07-20T21:12:00Z",
            RELATION_STATUS_ACTIVE
        );

    test_relation_dao_insert_relation(
        &fixture,
        first_record
    );

    test_relation_dao_insert_relation(
        &fixture,
        second_record
    );

    test_relation_dao_insert_relation(
        &fixture,
        reverse_record
    );

    assert(
        test_relation_dao_count_rows(
            fixture.database
        ) == 3
    );

    relation_record_free(
        reverse_record
    );

    relation_record_free(
        second_record
    );

    relation_record_free(
        first_record
    );

    test_relation_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie la recherche d'une relation existante.
 */
static void test_relation_dao_find_existing(void)
{
    TestRelationDaoFixture fixture =
        test_relation_dao_fixture_create();

    RelationRecord *inserted_record =
        NULL;

    RelationRecord *loaded_record =
        NULL;

    GError *error =
        NULL;

    test_relation_dao_insert_standard_entities(
        &fixture
    );

    inserted_record =
        test_relation_dao_create_relation(
            "40000000-0000-4000-8000-000000000011",
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_C_IDENTIFIER,
            "knows",
            "Connaît cette personne",
            "Déclaration issue d'un message.",
            65,
            "2026-07-20T21:20:00Z",
            "2026-07-20T21:25:00Z",
            RELATION_STATUS_ARCHIVED
        );

    test_relation_dao_insert_relation(
        &fixture,
        inserted_record
    );

    loaded_record =
        relation_dao_find_by_identifier(
            fixture.relation_dao,
            "40000000-0000-4000-8000-000000000011",
            &error
        );

    assert(loaded_record != NULL);
    assert(error == NULL);

    assert(
        strcmp(
            relation_record_get_identifier(
                loaded_record
            ),
            "40000000-0000-4000-8000-000000000011"
        ) == 0
    );

    assert(
        strcmp(
            relation_record_get_source_entity_identifier(
                loaded_record
            ),
            TEST_ENTITY_A_IDENTIFIER
        ) == 0
    );

    assert(
        strcmp(
            relation_record_get_target_entity_identifier(
                loaded_record
            ),
            TEST_ENTITY_C_IDENTIFIER
        ) == 0
    );

    assert(
        strcmp(
            relation_record_get_relation_type(
                loaded_record
            ),
            "knows"
        ) == 0
    );

    assert(
        relation_record_get_confidence(
            loaded_record
        ) == 65
    );

    assert(
        relation_record_get_status(
            loaded_record
        ) == RELATION_STATUS_ARCHIVED
    );

    relation_record_free(
        loaded_record
    );

    relation_record_free(
        inserted_record
    );

    test_relation_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie qu'une relation absente retourne NULL sans erreur.
 */
static void test_relation_dao_find_missing(void)
{
    TestRelationDaoFixture fixture =
        test_relation_dao_fixture_create();

    RelationRecord *relation_record =
        NULL;

    GError *error =
        NULL;

    relation_record =
        relation_dao_find_by_identifier(
            fixture.relation_dao,
            "40000000-0000-4000-8000-000000000012",
            &error
        );

    assert(relation_record == NULL);
    assert(error == NULL);

    test_relation_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d'un identifiant de recherche invalide.
 */
static void test_relation_dao_find_invalid_identifier(void)
{
    TestRelationDaoFixture fixture =
        test_relation_dao_fixture_create();

    RelationRecord *relation_record =
        NULL;

    GError *error =
        NULL;

    relation_record =
        relation_dao_find_by_identifier(
            fixture.relation_dao,
            "relation-invalide",
            &error
        );

    assert(relation_record == NULL);

    test_relation_dao_assert_error(
        error,
        RELATION_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    test_relation_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le chargement d'une liste vide.
 */
static void test_relation_dao_list_empty(void)
{
    TestRelationDaoFixture fixture =
        test_relation_dao_fixture_create();

    GPtrArray *relations =
        NULL;

    GError *error =
        NULL;

    relations =
        relation_dao_list_all(
            fixture.relation_dao,
            &error
        );

    assert(relations != NULL);
    assert(error == NULL);
    assert(relations->len == 0);

    g_ptr_array_unref(
        relations
    );

    test_relation_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie l'ordre stable de la liste.
 */
static void test_relation_dao_list_order(void)
{
    TestRelationDaoFixture fixture =
        test_relation_dao_fixture_create();

    RelationRecord *first_record =
        NULL;

    RelationRecord *second_record =
        NULL;

    RelationRecord *third_record =
        NULL;

    const RelationRecord *first_loaded =
        NULL;

    const RelationRecord *second_loaded =
        NULL;

    const RelationRecord *third_loaded =
        NULL;

    GPtrArray *relations =
        NULL;

    GError *error =
        NULL;

    test_relation_dao_insert_standard_entities(
        &fixture
    );

    third_record =
        test_relation_dao_create_relation(
            "60000000-0000-4000-8000-000000000003",
            TEST_ENTITY_C_IDENTIFIER,
            TEST_ENTITY_A_IDENTIFIER,
            "knows",
            NULL,
            NULL,
            50,
            "2026-07-20T22:00:00Z",
            "2026-07-20T22:00:00Z",
            RELATION_STATUS_ACTIVE
        );

    second_record =
        test_relation_dao_create_relation(
            "60000000-0000-4000-8000-000000000002",
            TEST_ENTITY_B_IDENTIFIER,
            TEST_ENTITY_C_IDENTIFIER,
            "uses",
            NULL,
            NULL,
            50,
            "2026-07-20T21:50:00Z",
            "2026-07-20T21:50:00Z",
            RELATION_STATUS_ACTIVE
        );

    first_record =
        test_relation_dao_create_relation(
            "60000000-0000-4000-8000-000000000001",
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_B_IDENTIFIER,
            "controls",
            NULL,
            NULL,
            50,
            "2026-07-20T21:50:00Z",
            "2026-07-20T21:50:00Z",
            RELATION_STATUS_ACTIVE
        );

    test_relation_dao_insert_relation(
        &fixture,
        third_record
    );

    test_relation_dao_insert_relation(
        &fixture,
        second_record
    );

    test_relation_dao_insert_relation(
        &fixture,
        first_record
    );

    relations =
        relation_dao_list_all(
            fixture.relation_dao,
            &error
        );

    assert(relations != NULL);
    assert(error == NULL);
    assert(relations->len == 3);

    first_loaded =
        g_ptr_array_index(
            relations,
            0
        );

    second_loaded =
        g_ptr_array_index(
            relations,
            1
        );

    third_loaded =
        g_ptr_array_index(
            relations,
            2
        );

    assert(
        strcmp(
            relation_record_get_identifier(
                first_loaded
            ),
            "60000000-0000-4000-8000-000000000001"
        ) == 0
    );

    assert(
        strcmp(
            relation_record_get_identifier(
                second_loaded
            ),
            "60000000-0000-4000-8000-000000000002"
        ) == 0
    );

    assert(
        strcmp(
            relation_record_get_identifier(
                third_loaded
            ),
            "60000000-0000-4000-8000-000000000003"
        ) == 0
    );

    g_ptr_array_unref(
        relations
    );

    relation_record_free(
        first_record
    );

    relation_record_free(
        second_record
    );

    relation_record_free(
        third_record
    );

    test_relation_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le comptage d'une table vide.
 */
static void test_relation_dao_count_empty(void)
{
    TestRelationDaoFixture fixture =
        test_relation_dao_fixture_create();

    guint64 relation_count =
        42;

    GError *error =
        NULL;

    assert(
        relation_dao_count(
            fixture.relation_dao,
            &relation_count,
            &error
        )
    );

    assert(error == NULL);
    assert(relation_count == 0);

    test_relation_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le comptage de plusieurs relations.
 */
static void test_relation_dao_count_records(void)
{
    TestRelationDaoFixture fixture =
        test_relation_dao_fixture_create();

    RelationRecord *first_record =
        NULL;

    RelationRecord *second_record =
        NULL;

    guint64 relation_count =
        0;

    GError *error =
        NULL;

    test_relation_dao_insert_standard_entities(
        &fixture
    );

    first_record =
        test_relation_dao_create_relation(
            "70000000-0000-4000-8000-000000000001",
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_B_IDENTIFIER,
            "uses",
            NULL,
            NULL,
            50,
            "2026-07-20T22:10:00Z",
            "2026-07-20T22:10:00Z",
            RELATION_STATUS_ACTIVE
        );

    second_record =
        test_relation_dao_create_relation(
            "70000000-0000-4000-8000-000000000002",
            TEST_ENTITY_B_IDENTIFIER,
            TEST_ENTITY_C_IDENTIFIER,
            "controls",
            NULL,
            NULL,
            50,
            "2026-07-20T22:11:00Z",
            "2026-07-20T22:11:00Z",
            RELATION_STATUS_ACTIVE
        );

    test_relation_dao_insert_relation(
        &fixture,
        first_record
    );

    test_relation_dao_insert_relation(
        &fixture,
        second_record
    );

    assert(
        relation_dao_count(
            fixture.relation_dao,
            &relation_count,
            &error
        )
    );

    assert(error == NULL);
    assert(relation_count == 2);

    relation_record_free(
        second_record
    );

    relation_record_free(
        first_record
    );

    test_relation_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie les arguments invalides des opérations publiques.
 */
static void test_relation_dao_invalid_arguments(void)
{
    TestRelationDaoFixture fixture =
        test_relation_dao_fixture_create();

    GPtrArray *relations =
        NULL;

    guint64 relation_count =
        123;

    GError *error =
        NULL;

    assert(
        !relation_dao_insert(
            fixture.relation_dao,
            NULL,
            &error
        )
    );

    test_relation_dao_assert_error(
        error,
        RELATION_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    relations =
        relation_dao_list_all(
            NULL,
            &error
        );

    assert(relations == NULL);

    test_relation_dao_assert_error(
        error,
        RELATION_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    assert(
        !relation_dao_count(
            fixture.relation_dao,
            NULL,
            &error
        )
    );

    test_relation_dao_assert_error(
        error,
        RELATION_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    assert(
        !relation_dao_count(
            NULL,
            &relation_count,
            &error
        )
    );

    test_relation_dao_assert_error(
        error,
        RELATION_DAO_ERROR_INVALID_ARGUMENT
    );

    assert(relation_count == 123);

    g_clear_error(
        &error
    );

    test_relation_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le signalement d'un schéma absent.
 */
static void test_relation_dao_missing_table(void)
{
    Database *database =
        NULL;

    RelationDao *relation_dao =
        NULL;

    GPtrArray *relations =
        NULL;

    GError *error =
        NULL;

    database =
        database_open(
            ":memory:"
        );

    assert(database != NULL);

    relation_dao =
        relation_dao_new(
            database,
            &error
        );

    assert(relation_dao != NULL);
    assert(error == NULL);

    g_test_expect_message(
        NULL,
        G_LOG_LEVEL_WARNING,
        "*Impossible de préparer la requête SQL*"
    );

    relations =
        relation_dao_list_all(
            relation_dao,
            &error
        );

    g_test_assert_expected_messages();

    assert(relations == NULL);

    test_relation_dao_assert_error(
        error,
        RELATION_DAO_ERROR_SCHEMA
    );

    g_clear_error(
        &error
    );

    relation_dao_free(
        relation_dao
    );

    database_close(
        database
    );
}

/**
 * @brief Vérifie la contrainte interdisant une relation réflexive.
 *
 * Le modèle RelationRecord interdit déjà ce cas. Cette insertion directe
 * vérifie que le schéma SQLite constitue également une seconde barrière.
 */
static void test_relation_dao_reflexive_schema_constraint(void)
{
    TestRelationDaoFixture fixture =
        test_relation_dao_fixture_create();

    DatabaseStatement *statement =
        NULL;

    test_relation_dao_insert_entity(
        &fixture,
        TEST_ENTITY_A_IDENTIFIER,
        "Personne A"
    );

    statement =
        database_statement_prepare(
            fixture.database,
            "INSERT INTO relations"
            "("
            "    id,"
            "    entite_source_id,"
            "    entite_cible_id,"
            "    type_relation,"
            "    confiance,"
            "    created_at,"
            "    updated_at,"
            "    status"
            ")"
            "VALUES"
            "("
            "    ?,"
            "    ?,"
            "    ?,"
            "    ?,"
            "    ?,"
            "    ?,"
            "    ?,"
            "    ?"
            ");"
        );

    assert(statement != NULL);

    assert(
        database_statement_bind_text(
            statement,
            1,
            "80000000-0000-4000-8000-000000000001"
        )
    );

    assert(
        database_statement_bind_text(
            statement,
            2,
            TEST_ENTITY_A_IDENTIFIER
        )
    );

    assert(
        database_statement_bind_text(
            statement,
            3,
            TEST_ENTITY_A_IDENTIFIER
        )
    );

    assert(
        database_statement_bind_text(
            statement,
            4,
            "uses"
        )
    );

    assert(
        database_statement_bind_int64(
            statement,
            5,
            50
        )
    );

    assert(
        database_statement_bind_text(
            statement,
            6,
            "2026-07-20T22:20:00Z"
        )
    );

    assert(
        database_statement_bind_text(
            statement,
            7,
            "2026-07-20T22:20:00Z"
        )
    );

    assert(
        database_statement_bind_text(
            statement,
            8,
            "active"
        )
    );

    g_test_expect_message(
        NULL,
        G_LOG_LEVEL_WARNING,
        "*CHECK constraint failed*"
    );

    assert(
        database_statement_step(
            statement
        ) == DATABASE_STATEMENT_STEP_ERROR
    );

    g_test_assert_expected_messages();

    database_statement_finalize(
        statement
    );

    assert(
        test_relation_dao_count_rows(
            fixture.database
        ) == 0
    );

    test_relation_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie les contraintes ON DELETE RESTRICT des deux entités.
 */
static void test_relation_dao_foreign_key_restrict(void)
{
    TestRelationDaoFixture fixture =
        test_relation_dao_fixture_create();

    RelationRecord *relation_record =
        NULL;

    DatabaseStatement *statement =
        NULL;

    test_relation_dao_insert_standard_entities(
        &fixture
    );

    relation_record =
        test_relation_dao_create_relation(
            "90000000-0000-4000-8000-000000000001",
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_B_IDENTIFIER,
            "uses",
            NULL,
            NULL,
            50,
            "2026-07-20T22:30:00Z",
            "2026-07-20T22:30:00Z",
            RELATION_STATUS_ACTIVE
        );

    test_relation_dao_insert_relation(
        &fixture,
        relation_record
    );

    statement =
        database_statement_prepare(
            fixture.database,
            "DELETE FROM entites "
            "WHERE id = ?;"
        );

    assert(statement != NULL);

    assert(
        database_statement_bind_text(
            statement,
            1,
            TEST_ENTITY_A_IDENTIFIER
        )
    );

    g_test_expect_message(
        NULL,
        G_LOG_LEVEL_WARNING,
        "*FOREIGN KEY constraint failed*"
    );

    assert(
        database_statement_step(
            statement
        ) == DATABASE_STATEMENT_STEP_ERROR
    );

    g_test_assert_expected_messages();

    database_statement_finalize(
        statement
    );

    statement =
        database_statement_prepare(
            fixture.database,
            "DELETE FROM entites "
            "WHERE id = ?;"
        );

    assert(statement != NULL);

    assert(
        database_statement_bind_text(
            statement,
            1,
            TEST_ENTITY_B_IDENTIFIER
        )
    );

    g_test_expect_message(
        NULL,
        G_LOG_LEVEL_WARNING,
        "*FOREIGN KEY constraint failed*"
    );

    assert(
        database_statement_step(
            statement
        ) == DATABASE_STATEMENT_STEP_ERROR
    );

    g_test_assert_expected_messages();

    database_statement_finalize(
        statement
    );

    assert(
        test_relation_dao_count_rows(
            fixture.database
        ) == 1
    );

    relation_record_free(
        relation_record
    );

    test_relation_dao_fixture_clear(
        &fixture
    );
}

/** @brief Vérifie la modification des attributs sans changer les extrémités. */
static void test_relation_dao_update_existing(void)
{
    static const char identifier[] =
        "90000000-0000-4000-8000-000000000001";
    TestRelationDaoFixture fixture = test_relation_dao_fixture_create();
    RelationRecord *initial = NULL;
    RelationRecord *updated = NULL;
    RelationRecord *stored = NULL;
    GError *error = NULL;

    test_relation_dao_insert_standard_entities(&fixture);
    initial = test_relation_dao_create_relation(identifier,
        TEST_ENTITY_A_IDENTIFIER, TEST_ENTITY_B_IDENTIFIER, "uses",
        "Ancien libellé", "Ancienne justification", 40,
        "2026-07-20T20:00:00Z", "2026-07-20T20:00:00Z",
        RELATION_STATUS_ACTIVE);
    test_relation_dao_insert_relation(&fixture, initial);
    updated = test_relation_dao_create_relation(identifier,
        TEST_ENTITY_A_IDENTIFIER, TEST_ENTITY_B_IDENTIFIER, "controls",
        "Nouveau libellé", "Nouvelle justification", 90,
        "2026-07-20T20:00:00Z", "2026-07-22T16:00:00Z",
        RELATION_STATUS_ACTIVE);
    assert(relation_dao_update(fixture.relation_dao, updated, &error));
    assert(error == NULL);
    stored = relation_dao_find_by_identifier(fixture.relation_dao,
        identifier, &error);
    assert(stored != NULL && error == NULL);
    assert(strcmp(relation_record_get_relation_type(stored), "controls") == 0);
    assert(strcmp(relation_record_get_label(stored), "Nouveau libellé") == 0);
    assert(relation_record_get_confidence(stored) == 90);
    assert(strcmp(relation_record_get_source_entity_identifier(stored),
        TEST_ENTITY_A_IDENTIFIER) == 0);
    assert(strcmp(relation_record_get_created_at(stored),
        "2026-07-20T20:00:00Z") == 0);
    relation_record_free(stored);
    relation_record_free(updated);
    relation_record_free(initial);
    test_relation_dao_fixture_clear(&fixture);
}

int main(
    int argc,
    char **argv
)
{
    g_test_init(
        &argc,
        &argv,
        NULL
    );

    test_relation_dao_new_null_database();
    test_relation_dao_lifecycle();
    test_relation_dao_insert_valid_full();
    test_relation_dao_insert_optional_null();
    test_relation_dao_insert_duplicate_identifier();
    test_relation_dao_insert_missing_source();
    test_relation_dao_insert_missing_target();
    test_relation_dao_insert_duplicate_triple();
    test_relation_dao_orientation_and_multiple_types();
    test_relation_dao_find_existing();
    test_relation_dao_find_missing();
    test_relation_dao_find_invalid_identifier();
    test_relation_dao_list_empty();
    test_relation_dao_list_order();
    test_relation_dao_count_empty();
    test_relation_dao_count_records();
    test_relation_dao_invalid_arguments();
    test_relation_dao_missing_table();
    test_relation_dao_reflexive_schema_constraint();
    test_relation_dao_foreign_key_restrict();
    test_relation_dao_update_existing();

    printf(
        "RelationDao : tous les tests sont valides.\n"
    );

    return 0;
}
