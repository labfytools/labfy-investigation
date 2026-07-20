/******************************************************************************
 * @file test_entity_dao.c
 * @brief Tests du DAO des entités OSINT.
 ******************************************************************************/

#include "dao/entity_dao.h"

#include "database/database.h"
#include "database/statement.h"
#include "models/entity_record.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

/**
 * @brief Environnement temporaire d'un test EntityDao.
 */
typedef struct
{
    char *temporary_directory;
    char *database_path;

    Database *database;
    EntityDao *entity_dao;
} TestEntityDaoFixture;

/**
 * @brief Crée une base temporaire contenant le schéma de l'application.
 */
static TestEntityDaoFixture test_entity_dao_fixture_create(void)
{
    TestEntityDaoFixture fixture =
        {0};

    GError *error =
        NULL;

    fixture.temporary_directory =
        g_dir_make_tmp(
            "labfy-entity-dao-test-XXXXXX",
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
            "Enquete_EntityDao",
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

    return fixture;
}

/**
 * @brief Détruit une fixture et ses fichiers temporaires.
 */
static void test_entity_dao_fixture_clear(
    TestEntityDaoFixture *fixture
)
{
    assert(fixture != NULL);

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
 * @brief Crée une entité minimale valide pour les tests.
 */
static EntityRecord *test_entity_dao_create_record(
    const char *identifier,
    const char *type_identifier,
    const char *value,
    const char *created_at,
    EntityStatus status
)
{
    EntityRecord *entity_record =
        NULL;

    GError *error =
        NULL;

    entity_record =
        entity_record_new(
            identifier,
            type_identifier,
            value,
            NULL,
            NULL,
            50,
            created_at,
            created_at,
            status,
            &error
        );

    assert(entity_record != NULL);
    assert(error == NULL);

    return entity_record;
}

/**
 * @brief Retourne le nombre d'entités présentes dans la base de test.
 */
static int64_t test_entity_dao_count_rows(
    Database *database
)
{
    DatabaseStatement *statement =
        NULL;

    int64_t entity_count =
        -1;

    assert(database != NULL);

    statement =
        database_statement_prepare(
            database,
            "SELECT COUNT(*) FROM entites;"
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
            &entity_count
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

    return entity_count;
}

/**
 * @brief Vérifie le refus d'une connexion absente.
 */
static void test_entity_dao_new_null_database(void)
{
    EntityDao *entity_dao =
        NULL;

    GError *error =
        NULL;

    entity_dao =
        entity_dao_new(
            NULL,
            &error
        );

    assert(entity_dao == NULL);
    assert(error != NULL);
    assert(error->domain == ENTITY_DAO_ERROR);

    assert(
        error->code ==
        (gint) ENTITY_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );
}

/**
 * @brief Vérifie la création et la destruction d'un DAO valide.
 */
static void test_entity_dao_new_valid(void)
{
    Database *database =
        NULL;

    EntityDao *entity_dao =
        NULL;

    GError *error =
        NULL;

    database =
        database_open(
            ":memory:"
        );

    assert(database != NULL);

    entity_dao =
        entity_dao_new(
            database,
            &error
        );

    assert(entity_dao != NULL);
    assert(error == NULL);

    /*
     * La destruction du DAO ne doit pas fermer la connexion empruntée.
     */
    entity_dao_free(
        entity_dao
    );

    database_close(
        database
    );
}

/**
 * @brief Vérifie que la destruction accepte NULL.
 */
static void test_entity_dao_free_null(void)
{
    entity_dao_free(
        NULL
    );
}

/**
 * @brief Vérifie l'insertion complète d'une entité valide.
 */
static void test_entity_dao_insert_valid_full(void)
{
    TestEntityDaoFixture fixture =
        test_entity_dao_fixture_create();

    EntityRecord *entity_record =
        NULL;

    DatabaseStatement *statement =
        NULL;

    char *identifier =
        NULL;

    char *type_identifier =
        NULL;

    char *value =
        NULL;

    char *label =
        NULL;

    char *description =
        NULL;

    char *created_at =
        NULL;

    char *updated_at =
        NULL;

    char *status =
        NULL;

    int64_t confidence =
        -1;

    GError *error =
        NULL;

    entity_record =
        entity_record_new(
            "11111111-1111-4111-8111-111111111111",
            "email_address",
            "suspect@example.org",
            "Adresse utilisée pour la transaction",
            "Adresse communiquée dans la conversation.",
            85,
            "2026-07-20T18:00:00Z",
            "2026-07-20T18:30:00Z",
            ENTITY_STATUS_ACTIVE,
            &error
        );

    assert(entity_record != NULL);
    assert(error == NULL);

    assert(
        entity_dao_insert(
            fixture.entity_dao,
            entity_record,
            &error
        )
    );

    assert(error == NULL);

    statement =
        database_statement_prepare(
            fixture.database,
            "SELECT "
            "    entites.id,"
            "    types_entite.code,"
            "    entites.valeur,"
            "    entites.label,"
            "    entites.description,"
            "    entites.confiance,"
            "    entites.created_at,"
            "    entites.updated_at,"
            "    entites.status "
            "FROM entites "
            "INNER JOIN types_entite "
            "ON types_entite.id = entites.type_id "
            "WHERE entites.id = ?;"
        );

    assert(statement != NULL);

    assert(
        database_statement_bind_text(
            statement,
            1,
            "11111111-1111-4111-8111-111111111111"
        )
    );

    assert(
        database_statement_step(
            statement
        ) == DATABASE_STATEMENT_STEP_ROW
    );

    assert(database_statement_column_text(statement, 0, &identifier));
    assert(database_statement_column_text(statement, 1, &type_identifier));
    assert(database_statement_column_text(statement, 2, &value));
    assert(database_statement_column_text(statement, 3, &label));
    assert(database_statement_column_text(statement, 4, &description));
    assert(database_statement_column_int64(statement, 5, &confidence));
    assert(database_statement_column_text(statement, 6, &created_at));
    assert(database_statement_column_text(statement, 7, &updated_at));
    assert(database_statement_column_text(statement, 8, &status));

    assert(
        database_statement_step(
            statement
        ) == DATABASE_STATEMENT_STEP_DONE
    );

    assert(
        strcmp(
            identifier,
            "11111111-1111-4111-8111-111111111111"
        ) == 0
    );

    assert(
        strcmp(
            type_identifier,
            "email_address"
        ) == 0
    );

    assert(
        strcmp(
            value,
            "suspect@example.org"
        ) == 0
    );

    assert(
        strcmp(
            label,
            "Adresse utilisée pour la transaction"
        ) == 0
    );

    assert(
        strcmp(
            description,
            "Adresse communiquée dans la conversation."
        ) == 0
    );

    assert(confidence == 85);

    assert(
        strcmp(
            created_at,
            "2026-07-20T18:00:00Z"
        ) == 0
    );

    assert(
        strcmp(
            updated_at,
            "2026-07-20T18:30:00Z"
        ) == 0
    );

    assert(
        strcmp(
            status,
            "active"
        ) == 0
    );

    database_statement_finalize(
        statement
    );

    g_free(status);
    g_free(updated_at);
    g_free(created_at);
    g_free(description);
    g_free(label);
    g_free(value);
    g_free(type_identifier);
    g_free(identifier);

    entity_record_free(
        entity_record
    );

    test_entity_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie l'insertion avec des champs facultatifs absents.
 */
static void test_entity_dao_insert_optional_null(void)
{
    TestEntityDaoFixture fixture =
        test_entity_dao_fixture_create();

    EntityRecord *entity_record =
        NULL;

    DatabaseStatement *statement =
        NULL;

    bool label_is_null =
        false;

    bool description_is_null =
        false;

    GError *error =
        NULL;

    entity_record =
        test_entity_dao_create_record(
            "22222222-2222-4222-8222-222222222222",
            "iban",
            "FR7612345678901234567890123",
            "2026-07-20T19:00:00Z",
            ENTITY_STATUS_ACTIVE
        );

    assert(
        entity_dao_insert(
            fixture.entity_dao,
            entity_record,
            &error
        )
    );

    assert(error == NULL);

    statement =
        database_statement_prepare(
            fixture.database,
            "SELECT label, description "
            "FROM entites "
            "WHERE id = ?;"
        );

    assert(statement != NULL);

    assert(
        database_statement_bind_text(
            statement,
            1,
            "22222222-2222-4222-8222-222222222222"
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
            &description_is_null
        )
    );

    assert(label_is_null);
    assert(description_is_null);

    assert(
        database_statement_step(
            statement
        ) == DATABASE_STATEMENT_STEP_DONE
    );

    database_statement_finalize(
        statement
    );

    entity_record_free(
        entity_record
    );

    test_entity_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus des arguments d'insertion invalides.
 */
static void test_entity_dao_insert_invalid_arguments(void)
{
    TestEntityDaoFixture fixture =
        test_entity_dao_fixture_create();

    EntityRecord *entity_record =
        NULL;

    GError *error =
        NULL;

    entity_record =
        test_entity_dao_create_record(
            "33333333-3333-4333-8333-333333333333",
            "person",
            "Jean Dupont",
            "2026-07-20T19:10:00Z",
            ENTITY_STATUS_ACTIVE
        );

    assert(
        !entity_dao_insert(
            NULL,
            entity_record,
            &error
        )
    );

    assert(error != NULL);
    assert(error->domain == ENTITY_DAO_ERROR);

    assert(
        error->code ==
        (gint) ENTITY_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    assert(
        !entity_dao_insert(
            fixture.entity_dao,
            NULL,
            &error
        )
    );

    assert(error != NULL);

    assert(
        error->code ==
        (gint) ENTITY_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    entity_record_free(
        entity_record
    );

    test_entity_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d'un UUID déjà utilisé.
 */
static void test_entity_dao_insert_duplicate_identifier(void)
{
    TestEntityDaoFixture fixture =
        test_entity_dao_fixture_create();

    EntityRecord *first_record =
        NULL;

    EntityRecord *second_record =
        NULL;

    GError *error =
        NULL;

    first_record =
        test_entity_dao_create_record(
            "44444444-4444-4444-8444-444444444444",
            "email_address",
            "first@example.org",
            "2026-07-20T19:20:00Z",
            ENTITY_STATUS_ACTIVE
        );

    second_record =
        test_entity_dao_create_record(
            "44444444-4444-4444-8444-444444444444",
            "email_address",
            "second@example.org",
            "2026-07-20T19:25:00Z",
            ENTITY_STATUS_ACTIVE
        );

    assert(
        entity_dao_insert(
            fixture.entity_dao,
            first_record,
            &error
        )
    );

    assert(error == NULL);

    assert(
        !entity_dao_insert(
            fixture.entity_dao,
            second_record,
            &error
        )
    );

    assert(error != NULL);
    assert(error->domain == ENTITY_DAO_ERROR);

    assert(
        error->code ==
        (gint) ENTITY_DAO_ERROR_CONSTRAINT
    );

    assert(
        strstr(
            error->message,
            "identifiant"
        ) != NULL
    );

    assert(
        test_entity_dao_count_rows(
            fixture.database
        ) == 1
    );

    g_clear_error(
        &error
    );

    entity_record_free(second_record);
    entity_record_free(first_record);

    test_entity_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d'un couple type et valeur déjà utilisé.
 */
static void test_entity_dao_insert_duplicate_type_value(void)
{
    TestEntityDaoFixture fixture =
        test_entity_dao_fixture_create();

    EntityRecord *first_record =
        NULL;

    EntityRecord *second_record =
        NULL;

    GError *error =
        NULL;

    first_record =
        test_entity_dao_create_record(
            "55555555-5555-4555-8555-555555555555",
            "phone_number",
            "+33601020304",
            "2026-07-20T19:30:00Z",
            ENTITY_STATUS_ACTIVE
        );

    second_record =
        test_entity_dao_create_record(
            "66666666-6666-4666-8666-666666666666",
            "phone_number",
            "+33601020304",
            "2026-07-20T19:35:00Z",
            ENTITY_STATUS_ACTIVE
        );

    assert(
        entity_dao_insert(
            fixture.entity_dao,
            first_record,
            &error
        )
    );

    assert(error == NULL);

    assert(
        !entity_dao_insert(
            fixture.entity_dao,
            second_record,
            &error
        )
    );

    assert(error != NULL);
    assert(error->domain == ENTITY_DAO_ERROR);

    assert(
        error->code ==
        (gint) ENTITY_DAO_ERROR_CONSTRAINT
    );

    assert(
        strstr(
            error->message,
            "type"
        ) != NULL
    );

    assert(
        test_entity_dao_count_rows(
            fixture.database
        ) == 1
    );

    g_clear_error(
        &error
    );

    entity_record_free(second_record);
    entity_record_free(first_record);

    test_entity_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d'un type absent du catalogue SQLite.
 */
static void test_entity_dao_insert_unknown_type(void)
{
    TestEntityDaoFixture fixture =
        test_entity_dao_fixture_create();

    EntityRecord *entity_record =
        NULL;

    GError *error =
        NULL;

    entity_record =
        test_entity_dao_create_record(
            "77777777-7777-4777-8777-777777777777",
            "unknown_type",
            "Valeur inconnue",
            "2026-07-20T19:40:00Z",
            ENTITY_STATUS_ACTIVE
        );

    assert(
        !entity_dao_insert(
            fixture.entity_dao,
            entity_record,
            &error
        )
    );

    assert(error != NULL);
    assert(error->domain == ENTITY_DAO_ERROR);

    assert(
        error->code ==
        (gint) ENTITY_DAO_ERROR_CONSTRAINT
    );

    assert(
        strstr(
            error->message,
            "type d'entité"
        ) != NULL
    );

    assert(
        test_entity_dao_count_rows(
            fixture.database
        ) == 0
    );

    g_clear_error(
        &error
    );

    entity_record_free(
        entity_record
    );

    test_entity_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie la recherche d'une entité existante.
 */
static void test_entity_dao_find_existing(void)
{
    TestEntityDaoFixture fixture =
        test_entity_dao_fixture_create();

    EntityRecord *inserted_record =
        NULL;

    EntityRecord *loaded_record =
        NULL;

    GError *error =
        NULL;

    inserted_record =
        test_entity_dao_create_record(
            "88888888-8888-4888-8888-888888888888",
            "domain_name",
            "example.org",
            "2026-07-20T20:00:00Z",
            ENTITY_STATUS_ARCHIVED
        );

    assert(
        entity_dao_insert(
            fixture.entity_dao,
            inserted_record,
            &error
        )
    );

    assert(error == NULL);

    loaded_record =
        entity_dao_find_by_identifier(
            fixture.entity_dao,
            "88888888-8888-4888-8888-888888888888",
            &error
        );

    assert(loaded_record != NULL);
    assert(error == NULL);

    assert(
        strcmp(
            entity_record_get_identifier(
                loaded_record
            ),
            "88888888-8888-4888-8888-888888888888"
        ) == 0
    );

    assert(
        strcmp(
            entity_record_get_type_identifier(
                loaded_record
            ),
            "domain_name"
        ) == 0
    );

    assert(
        strcmp(
            entity_record_get_value(
                loaded_record
            ),
            "example.org"
        ) == 0
    );

    assert(
        entity_record_get_confidence(
            loaded_record
        ) == 50
    );

    assert(
        entity_record_get_status(
            loaded_record
        ) == ENTITY_STATUS_ARCHIVED
    );

    entity_record_free(loaded_record);
    entity_record_free(inserted_record);

    test_entity_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie la conversion du statut deleted.
 */
static void test_entity_dao_find_deleted_status(void)
{
    TestEntityDaoFixture fixture =
        test_entity_dao_fixture_create();

    EntityRecord *inserted_record =
        NULL;

    EntityRecord *loaded_record =
        NULL;

    GError *error =
        NULL;

    inserted_record =
        test_entity_dao_create_record(
            "99999999-9999-4999-8999-999999999999",
            "pseudonym",
            "vendeur_76",
            "2026-07-20T20:10:00Z",
            ENTITY_STATUS_DELETED
        );

    assert(
        entity_dao_insert(
            fixture.entity_dao,
            inserted_record,
            &error
        )
    );

    assert(error == NULL);

    loaded_record =
        entity_dao_find_by_identifier(
            fixture.entity_dao,
            "99999999-9999-4999-8999-999999999999",
            &error
        );

    assert(loaded_record != NULL);
    assert(error == NULL);

    assert(
        entity_record_get_status(
            loaded_record
        ) == ENTITY_STATUS_DELETED
    );

    entity_record_free(loaded_record);
    entity_record_free(inserted_record);

    test_entity_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie qu'une entité absente retourne NULL sans erreur.
 */
static void test_entity_dao_find_missing(void)
{
    TestEntityDaoFixture fixture =
        test_entity_dao_fixture_create();

    EntityRecord *entity_record =
        NULL;

    GError *error =
        NULL;

    entity_record =
        entity_dao_find_by_identifier(
            fixture.entity_dao,
            "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",
            &error
        );

    assert(entity_record == NULL);
    assert(error == NULL);

    test_entity_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d'un UUID de recherche invalide.
 */
static void test_entity_dao_find_invalid_identifier(void)
{
    TestEntityDaoFixture fixture =
        test_entity_dao_fixture_create();

    EntityRecord *entity_record =
        NULL;

    GError *error =
        NULL;

    entity_record =
        entity_dao_find_by_identifier(
            fixture.entity_dao,
            "identifiant-invalide",
            &error
        );

    assert(entity_record == NULL);
    assert(error != NULL);
    assert(error->domain == ENTITY_DAO_ERROR);

    assert(
        error->code ==
        (gint) ENTITY_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    test_entity_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le comptage d'une table vide.
 */
static void test_entity_dao_count_empty(void)
{
    TestEntityDaoFixture fixture =
        test_entity_dao_fixture_create();

    guint64 entity_count =
        42U;

    GError *error =
        NULL;

    assert(
        entity_dao_count(
            fixture.entity_dao,
            &entity_count,
            &error
        )
    );

    assert(error == NULL);
    assert(entity_count == 0U);

    test_entity_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le comptage de plusieurs entités.
 */
static void test_entity_dao_count_records(void)
{
    TestEntityDaoFixture fixture =
        test_entity_dao_fixture_create();

    EntityRecord *first_record =
        NULL;

    EntityRecord *second_record =
        NULL;

    guint64 entity_count =
        0U;

    GError *error =
        NULL;

    first_record =
        test_entity_dao_create_record(
            "aaaaaaaa-1111-4111-8111-111111111111",
            "ip_address",
            "192.0.2.10",
            "2026-07-20T20:20:00Z",
            ENTITY_STATUS_ACTIVE
        );

    second_record =
        test_entity_dao_create_record(
            "bbbbbbbb-2222-4222-8222-222222222222",
            "website",
            "https://example.org",
            "2026-07-20T20:25:00Z",
            ENTITY_STATUS_ACTIVE
        );

    assert(entity_dao_insert(fixture.entity_dao, first_record, &error));
    assert(error == NULL);

    assert(entity_dao_insert(fixture.entity_dao, second_record, &error));
    assert(error == NULL);

    assert(
        entity_dao_count(
            fixture.entity_dao,
            &entity_count,
            &error
        )
    );

    assert(error == NULL);
    assert(entity_count == 2U);

    entity_record_free(second_record);
    entity_record_free(first_record);

    test_entity_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus des paramètres de comptage invalides.
 */
static void test_entity_dao_count_invalid_arguments(void)
{
    TestEntityDaoFixture fixture =
        test_entity_dao_fixture_create();

    guint64 entity_count =
        123U;

    GError *error =
        NULL;

    assert(
        !entity_dao_count(
            NULL,
            &entity_count,
            &error
        )
    );

    assert(error != NULL);
    assert(error->domain == ENTITY_DAO_ERROR);

    assert(
        error->code ==
        (gint) ENTITY_DAO_ERROR_INVALID_ARGUMENT
    );

    assert(entity_count == 123U);

    g_clear_error(
        &error
    );

    assert(
        !entity_dao_count(
            fixture.entity_dao,
            NULL,
            &error
        )
    );

    assert(error != NULL);

    assert(
        error->code ==
        (gint) ENTITY_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    test_entity_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le chargement d'une liste vide.
 */
static void test_entity_dao_list_empty(void)
{
    TestEntityDaoFixture fixture =
        test_entity_dao_fixture_create();

    GPtrArray *entity_records =
        NULL;

    GError *error =
        NULL;

    entity_records =
        entity_dao_list_all(
            fixture.entity_dao,
            &error
        );

    assert(entity_records != NULL);
    assert(error == NULL);
    assert(entity_records->len == 0U);

    g_ptr_array_unref(
        entity_records
    );

    test_entity_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le chargement de plusieurs entités dans un ordre stable.
 */
static void test_entity_dao_list_order(void)
{
    TestEntityDaoFixture fixture =
        test_entity_dao_fixture_create();

    EntityRecord *first_inserted_record =
        NULL;

    EntityRecord *second_inserted_record =
        NULL;

    EntityRecord *third_inserted_record =
        NULL;

    const EntityRecord *first_loaded_record =
        NULL;

    const EntityRecord *second_loaded_record =
        NULL;

    const EntityRecord *third_loaded_record =
        NULL;

    GPtrArray *entity_records =
        NULL;

    GError *error =
        NULL;

    third_inserted_record =
        test_entity_dao_create_record(
            "30000000-0000-4000-8000-000000000003",
            "organization",
            "Organisation C",
            "2026-07-20T22:00:00Z",
            ENTITY_STATUS_ACTIVE
        );

    second_inserted_record =
        test_entity_dao_create_record(
            "20000000-0000-4000-8000-000000000002",
            "person",
            "Personne B",
            "2026-07-20T21:00:00Z",
            ENTITY_STATUS_ACTIVE
        );

    first_inserted_record =
        test_entity_dao_create_record(
            "10000000-0000-4000-8000-000000000001",
            "person",
            "Personne A",
            "2026-07-20T21:00:00Z",
            ENTITY_STATUS_ACTIVE
        );

    assert(entity_dao_insert(fixture.entity_dao, third_inserted_record, &error));
    assert(error == NULL);

    assert(entity_dao_insert(fixture.entity_dao, second_inserted_record, &error));
    assert(error == NULL);

    assert(entity_dao_insert(fixture.entity_dao, first_inserted_record, &error));
    assert(error == NULL);

    entity_records =
        entity_dao_list_all(
            fixture.entity_dao,
            &error
        );

    assert(entity_records != NULL);
    assert(error == NULL);
    assert(entity_records->len == 3U);

    first_loaded_record =
        g_ptr_array_index(
            entity_records,
            0U
        );

    second_loaded_record =
        g_ptr_array_index(
            entity_records,
            1U
        );

    third_loaded_record =
        g_ptr_array_index(
            entity_records,
            2U
        );

    assert(first_loaded_record != NULL);
    assert(second_loaded_record != NULL);
    assert(third_loaded_record != NULL);

    assert(
        strcmp(
            entity_record_get_identifier(
                first_loaded_record
            ),
            "10000000-0000-4000-8000-000000000001"
        ) == 0
    );

    assert(
        strcmp(
            entity_record_get_identifier(
                second_loaded_record
            ),
            "20000000-0000-4000-8000-000000000002"
        ) == 0
    );

    assert(
        strcmp(
            entity_record_get_identifier(
                third_loaded_record
            ),
            "30000000-0000-4000-8000-000000000003"
        ) == 0
    );

    g_ptr_array_unref(
        entity_records
    );

    entity_record_free(first_inserted_record);
    entity_record_free(second_inserted_record);
    entity_record_free(third_inserted_record);

    test_entity_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d'un DAO absent lors du chargement.
 */
static void test_entity_dao_list_invalid_arguments(void)
{
    GPtrArray *entity_records =
        NULL;

    GError *error =
        NULL;

    entity_records =
        entity_dao_list_all(
            NULL,
            &error
        );

    assert(entity_records == NULL);
    assert(error != NULL);
    assert(error->domain == ENTITY_DAO_ERROR);

    assert(
        error->code ==
        (gint) ENTITY_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );
}

int main(void)
{
    test_entity_dao_new_null_database();
    test_entity_dao_new_valid();
    test_entity_dao_free_null();
    test_entity_dao_insert_valid_full();
    test_entity_dao_insert_optional_null();
    test_entity_dao_insert_invalid_arguments();
    test_entity_dao_insert_duplicate_identifier();
    test_entity_dao_insert_duplicate_type_value();
    test_entity_dao_insert_unknown_type();
    test_entity_dao_find_existing();
    test_entity_dao_find_deleted_status();
    test_entity_dao_find_missing();
    test_entity_dao_find_invalid_identifier();
    test_entity_dao_count_empty();
    test_entity_dao_count_records();
    test_entity_dao_count_invalid_arguments();
    test_entity_dao_list_empty();
    test_entity_dao_list_order();
    test_entity_dao_list_invalid_arguments();

    printf(
        "EntityDao : tous les tests sont valides.\n"
    );

    return 0;
}
