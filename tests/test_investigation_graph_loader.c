/******************************************************************************
 * @file test_investigation_graph_loader.c
 * @brief Tests du chargement du graphe d'enquête depuis SQLite.
 ******************************************************************************/

#include "core/investigation_graph_loader.h"

#include "dao/entity_dao.h"
#include "dao/relation_dao.h"

#include "database/database.h"
#include "database/statement.h"

#include "models/entity_record.h"
#include "models/investigation_graph_model.h"
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

#define TEST_RELATION_AB_IDENTIFIER \
    "40000000-0000-4000-8000-000000000001"

#define TEST_RELATION_AC_IDENTIFIER \
    "40000000-0000-4000-8000-000000000002"

#define TEST_RELATION_CA_IDENTIFIER \
    "40000000-0000-4000-8000-000000000003"

/**
 * @brief Environnement temporaire d'un test InvestigationGraphLoader.
 */
typedef struct
{
    char *temporary_directory;
    char *database_path;

    Database *database;

    EntityDao *entity_dao;
    RelationDao *relation_dao;

    InvestigationGraphLoader *graph_loader;
} TestInvestigationGraphLoaderFixture;

/**
 * @brief Vérifie le domaine et le code d'une erreur du chargeur.
 */
static void test_graph_loader_assert_error(
    const GError *error,
    InvestigationGraphLoaderError expected_code
)
{
    assert(error != NULL);

    assert(
        error->domain ==
        INVESTIGATION_GRAPH_LOADER_ERROR
    );

    assert(
        error->code ==
        (gint) expected_code
    );

    assert(error->message != NULL);
    assert(error->message[0] != '\0');
}

/**
 * @brief Exécute une instruction SQL ne retournant aucune ligne.
 */
static void test_graph_loader_execute_sql(
    Database *database,
    const char *sql
)
{
    DatabaseStatement *statement =
        NULL;

    assert(database != NULL);
    assert(sql != NULL);

    statement =
        database_statement_prepare(
            database,
            sql
        );

    assert(statement != NULL);

    assert(
        database_statement_step(
            statement
        ) == DATABASE_STATEMENT_STEP_DONE
    );

    database_statement_finalize(
        statement
    );
}

/**
 * @brief Crée une base temporaire et les composants nécessaires.
 */
static TestInvestigationGraphLoaderFixture
test_graph_loader_fixture_create(void)
{
    TestInvestigationGraphLoaderFixture fixture =
        {0};

    GError *error =
        NULL;

    fixture.temporary_directory =
        g_dir_make_tmp(
            "labfy-graph-loader-test-XXXXXX",
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
            "Enquete_GraphLoader",
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

    fixture.graph_loader =
        investigation_graph_loader_new(
            fixture.database,
            &error
        );

    assert(fixture.graph_loader != NULL);
    assert(error == NULL);

    return fixture;
}

/**
 * @brief Libère une fixture et supprime sa base temporaire.
 */
static void test_graph_loader_fixture_clear(
    TestInvestigationGraphLoaderFixture *fixture
)
{
    assert(fixture != NULL);

    investigation_graph_loader_free(
        fixture->graph_loader
    );

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

    fixture->graph_loader =
        NULL;

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
 * @brief Crée une entité de test valide.
 */
static EntityRecord *test_graph_loader_create_entity(
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
            "2026-07-21T21:00:00Z",
            "2026-07-21T21:00:00Z",
            ENTITY_STATUS_ACTIVE,
            &error
        );

    assert(entity_record != NULL);
    assert(error == NULL);

    return entity_record;
}

/**
 * @brief Insère une entité dans SQLite.
 */
static void test_graph_loader_insert_entity(
    TestInvestigationGraphLoaderFixture *fixture,
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
        test_graph_loader_create_entity(
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
 * @brief Insère les trois entités standard.
 */
static void test_graph_loader_insert_standard_entities(
    TestInvestigationGraphLoaderFixture *fixture
)
{
    test_graph_loader_insert_entity(
        fixture,
        TEST_ENTITY_A_IDENTIFIER,
        "Personne A"
    );

    test_graph_loader_insert_entity(
        fixture,
        TEST_ENTITY_B_IDENTIFIER,
        "Personne B"
    );

    test_graph_loader_insert_entity(
        fixture,
        TEST_ENTITY_C_IDENTIFIER,
        "Personne C"
    );
}

/**
 * @brief Crée une relation de test valide.
 */
static RelationRecord *test_graph_loader_create_relation(
    const char *identifier,
    const char *source_identifier,
    const char *target_identifier,
    const char *relation_type
)
{
    RelationRecord *relation_record =
        NULL;

    GError *error =
        NULL;

    relation_record =
        relation_record_new(
            identifier,
            source_identifier,
            target_identifier,
            relation_type,
            NULL,
            "Relation de test.",
            75,
            "2026-07-21T21:10:00Z",
            "2026-07-21T21:10:00Z",
            RELATION_STATUS_ACTIVE,
            &error
        );

    assert(relation_record != NULL);
    assert(error == NULL);

    return relation_record;
}

/**
 * @brief Insère une relation dans SQLite.
 */
static void test_graph_loader_insert_relation(
    TestInvestigationGraphLoaderFixture *fixture,
    const char *identifier,
    const char *source_identifier,
    const char *target_identifier,
    const char *relation_type
)
{
    RelationRecord *relation_record =
        NULL;

    GError *error =
        NULL;

    assert(fixture != NULL);

    relation_record =
        test_graph_loader_create_relation(
            identifier,
            source_identifier,
            target_identifier,
            relation_type
        );

    assert(
        relation_dao_insert(
            fixture->relation_dao,
            relation_record,
            &error
        )
    );

    assert(error == NULL);

    relation_record_free(
        relation_record
    );
}

/**
 * @brief Insère les trois relations standard.
 */
static void test_graph_loader_insert_standard_relations(
    TestInvestigationGraphLoaderFixture *fixture
)
{
    test_graph_loader_insert_relation(
        fixture,
        TEST_RELATION_AB_IDENTIFIER,
        TEST_ENTITY_A_IDENTIFIER,
        TEST_ENTITY_B_IDENTIFIER,
        "uses"
    );

    test_graph_loader_insert_relation(
        fixture,
        TEST_RELATION_AC_IDENTIFIER,
        TEST_ENTITY_A_IDENTIFIER,
        TEST_ENTITY_C_IDENTIFIER,
        "controls"
    );

    test_graph_loader_insert_relation(
        fixture,
        TEST_RELATION_CA_IDENTIFIER,
        TEST_ENTITY_C_IDENTIFIER,
        TEST_ENTITY_A_IDENTIFIER,
        "knows"
    );
}

/**
 * @brief Insère directement une relation incohérente.
 */
static void test_graph_loader_insert_inconsistent_relation(
    TestInvestigationGraphLoaderFixture *fixture,
    const char *identifier,
    const char *source_identifier,
    const char *target_identifier
)
{
    DatabaseStatement *statement =
        NULL;

    assert(fixture != NULL);

    test_graph_loader_execute_sql(
        fixture->database,
        "PRAGMA foreign_keys = OFF;"
    );

    statement =
        database_statement_prepare(
            fixture->database,
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
            identifier
        )
    );

    assert(
        database_statement_bind_text(
            statement,
            2,
            source_identifier
        )
    );

    assert(
        database_statement_bind_text(
            statement,
            3,
            target_identifier
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
            "2026-07-21T21:20:00Z"
        )
    );

    assert(
        database_statement_bind_text(
            statement,
            7,
            "2026-07-21T21:20:00Z"
        )
    );

    assert(
        database_statement_bind_text(
            statement,
            8,
            "active"
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

    test_graph_loader_execute_sql(
        fixture->database,
        "PRAGMA foreign_keys = ON;"
    );
}

/**
 * @brief Vérifie le refus d'une connexion NULL.
 */
static void test_graph_loader_new_null_database(void)
{
    InvestigationGraphLoader *graph_loader =
        NULL;

    GError *error =
        NULL;

    graph_loader =
        investigation_graph_loader_new(
            NULL,
            &error
        );

    assert(graph_loader == NULL);

    test_graph_loader_assert_error(
        error,
        INVESTIGATION_GRAPH_LOADER_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );
}

/**
 * @brief Vérifie le cycle de vie et la connexion empruntée.
 */
static void test_graph_loader_lifecycle(void)
{
    TestInvestigationGraphLoaderFixture fixture =
        test_graph_loader_fixture_create();

    DatabaseStatement *statement =
        NULL;

    investigation_graph_loader_free(
        fixture.graph_loader
    );

    fixture.graph_loader =
        NULL;

    statement =
        database_statement_prepare(
            fixture.database,
            "SELECT 1;"
        );

    assert(statement != NULL);

    assert(
        database_statement_step(
            statement
        ) == DATABASE_STATEMENT_STEP_ROW
    );

    database_statement_finalize(
        statement
    );

    investigation_graph_loader_free(
        NULL
    );

    test_graph_loader_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le chargement d'une base vide.
 */
static void test_graph_loader_load_empty_database(void)
{
    TestInvestigationGraphLoaderFixture fixture =
        test_graph_loader_fixture_create();

    InvestigationGraphModel *graph_model =
        NULL;

    GError *error =
        NULL;

    graph_model =
        investigation_graph_loader_load(
            fixture.graph_loader,
            &error
        );

    assert(graph_model != NULL);
    assert(error == NULL);

    assert(
        investigation_graph_model_get_entity_count(
            graph_model
        ) == 0
    );

    assert(
        investigation_graph_model_get_relation_count(
            graph_model
        ) == 0
    );

    investigation_graph_model_free(
        graph_model
    );

    test_graph_loader_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le chargement d'entités sans relation.
 */
static void test_graph_loader_load_entities_only(void)
{
    TestInvestigationGraphLoaderFixture fixture =
        test_graph_loader_fixture_create();

    InvestigationGraphModel *graph_model =
        NULL;

    GPtrArray *entities =
        NULL;

    GError *error =
        NULL;

    test_graph_loader_insert_standard_entities(
        &fixture
    );

    graph_model =
        investigation_graph_loader_load(
            fixture.graph_loader,
            &error
        );

    assert(graph_model != NULL);
    assert(error == NULL);

    assert(
        investigation_graph_model_get_entity_count(
            graph_model
        ) == 3
    );

    assert(
        investigation_graph_model_get_relation_count(
            graph_model
        ) == 0
    );

    assert(
        investigation_graph_model_find_entity(
            graph_model,
            TEST_ENTITY_A_IDENTIFIER
        ) != NULL
    );

    assert(
        investigation_graph_model_find_entity(
            graph_model,
            TEST_ENTITY_B_IDENTIFIER
        ) != NULL
    );

    entities =
        investigation_graph_model_list_entities(
            graph_model,
            &error
        );

    assert(entities != NULL);
    assert(error == NULL);
    assert(entities->len == 3);

    g_ptr_array_unref(
        entities
    );

    investigation_graph_model_free(
        graph_model
    );

    test_graph_loader_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le chargement complet et les index du graphe.
 */
static void test_graph_loader_load_complete_graph(void)
{
    TestInvestigationGraphLoaderFixture fixture =
        test_graph_loader_fixture_create();

    InvestigationGraphModel *graph_model =
        NULL;

    const RelationRecord *relation_ab =
        NULL;

    GPtrArray *outgoing_a =
        NULL;

    GPtrArray *incoming_a =
        NULL;

    GPtrArray *incident_a =
        NULL;

    GError *error =
        NULL;

    test_graph_loader_insert_standard_entities(
        &fixture
    );

    test_graph_loader_insert_standard_relations(
        &fixture
    );

    graph_model =
        investigation_graph_loader_load(
            fixture.graph_loader,
            &error
        );

    assert(graph_model != NULL);
    assert(error == NULL);

    assert(
        investigation_graph_model_get_entity_count(
            graph_model
        ) == 3
    );

    assert(
        investigation_graph_model_get_relation_count(
            graph_model
        ) == 3
    );

    relation_ab =
        investigation_graph_model_find_relation(
            graph_model,
            TEST_RELATION_AB_IDENTIFIER
        );

    assert(relation_ab != NULL);

    assert(
        strcmp(
            relation_record_get_source_entity_identifier(
                relation_ab
            ),
            TEST_ENTITY_A_IDENTIFIER
        ) == 0
    );

    assert(
        strcmp(
            relation_record_get_target_entity_identifier(
                relation_ab
            ),
            TEST_ENTITY_B_IDENTIFIER
        ) == 0
    );

    outgoing_a =
        investigation_graph_model_list_outgoing_relations(
            graph_model,
            TEST_ENTITY_A_IDENTIFIER,
            &error
        );

    assert(outgoing_a != NULL);
    assert(error == NULL);
    assert(outgoing_a->len == 2);

    incoming_a =
        investigation_graph_model_list_incoming_relations(
            graph_model,
            TEST_ENTITY_A_IDENTIFIER,
            &error
        );

    assert(incoming_a != NULL);
    assert(error == NULL);
    assert(incoming_a->len == 1);

    incident_a =
        investigation_graph_model_list_incident_relations(
            graph_model,
            TEST_ENTITY_A_IDENTIFIER,
            &error
        );

    assert(incident_a != NULL);
    assert(error == NULL);
    assert(incident_a->len == 3);

    g_ptr_array_unref(
        incident_a
    );

    g_ptr_array_unref(
        incoming_a
    );

    g_ptr_array_unref(
        outgoing_a
    );

    investigation_graph_model_free(
        graph_model
    );

    test_graph_loader_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie que deux chargements produisent deux graphes indépendants.
 */
static void test_graph_loader_independent_graphs(void)
{
    TestInvestigationGraphLoaderFixture fixture =
        test_graph_loader_fixture_create();

    InvestigationGraphModel *first_graph =
        NULL;

    InvestigationGraphModel *second_graph =
        NULL;

    const EntityRecord *first_entity =
        NULL;

    const EntityRecord *second_entity =
        NULL;

    const RelationRecord *first_relation =
        NULL;

    const RelationRecord *second_relation =
        NULL;

    GError *error =
        NULL;

    test_graph_loader_insert_standard_entities(
        &fixture
    );

    test_graph_loader_insert_standard_relations(
        &fixture
    );

    first_graph =
        investigation_graph_loader_load(
            fixture.graph_loader,
            &error
        );

    assert(first_graph != NULL);
    assert(error == NULL);

    second_graph =
        investigation_graph_loader_load(
            fixture.graph_loader,
            &error
        );

    assert(second_graph != NULL);
    assert(error == NULL);

    assert(first_graph != second_graph);

    first_entity =
        investigation_graph_model_find_entity(
            first_graph,
            TEST_ENTITY_A_IDENTIFIER
        );

    second_entity =
        investigation_graph_model_find_entity(
            second_graph,
            TEST_ENTITY_A_IDENTIFIER
        );

    assert(first_entity != NULL);
    assert(second_entity != NULL);
    assert(first_entity != second_entity);

    first_relation =
        investigation_graph_model_find_relation(
            first_graph,
            TEST_RELATION_AB_IDENTIFIER
        );

    second_relation =
        investigation_graph_model_find_relation(
            second_graph,
            TEST_RELATION_AB_IDENTIFIER
        );

    assert(first_relation != NULL);
    assert(second_relation != NULL);
    assert(first_relation != second_relation);

    investigation_graph_model_free(
        first_graph
    );

    assert(
        investigation_graph_model_find_entity(
            second_graph,
            TEST_ENTITY_A_IDENTIFIER
        ) != NULL
    );

    assert(
        investigation_graph_model_find_relation(
            second_graph,
            TEST_RELATION_AB_IDENTIFIER
        ) != NULL
    );

    investigation_graph_model_free(
        second_graph
    );

    test_graph_loader_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d'un chargeur NULL et GError facultatif.
 */
static void test_graph_loader_invalid_arguments(void)
{
    InvestigationGraphModel *graph_model =
        NULL;

    GError *error =
        NULL;

    graph_model =
        investigation_graph_loader_load(
            NULL,
            &error
        );

    assert(graph_model == NULL);

    test_graph_loader_assert_error(
        error,
        INVESTIGATION_GRAPH_LOADER_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    graph_model =
        investigation_graph_loader_load(
            NULL,
            NULL
        );

    assert(graph_model == NULL);
}

/**
 * @brief Vérifie l'erreur lorsque la table entites est absente.
 */
static void test_graph_loader_missing_entities_table(void)
{
    Database *database =
        NULL;

    InvestigationGraphLoader *graph_loader =
        NULL;

    InvestigationGraphModel *graph_model =
        NULL;

    GError *error =
        NULL;

    database =
        database_open(
            ":memory:"
        );

    assert(database != NULL);

    graph_loader =
        investigation_graph_loader_new(
            database,
            &error
        );

    assert(graph_loader != NULL);
    assert(error == NULL);

    g_test_expect_message(
        NULL,
        G_LOG_LEVEL_WARNING,
        "*Impossible de préparer la requête SQL*"
    );

    graph_model =
        investigation_graph_loader_load(
            graph_loader,
            &error
        );

    g_test_assert_expected_messages();

    assert(graph_model == NULL);

    test_graph_loader_assert_error(
        error,
        INVESTIGATION_GRAPH_LOADER_ERROR_ENTITY_LOAD
    );

    g_clear_error(
        &error
    );

    investigation_graph_loader_free(
        graph_loader
    );

    database_close(
        database
    );
}

/**
 * @brief Vérifie l'erreur lorsque la table relations est absente.
 */
static void test_graph_loader_missing_relations_table(void)
{
    TestInvestigationGraphLoaderFixture fixture =
        test_graph_loader_fixture_create();

    InvestigationGraphModel *graph_model =
        NULL;

    GError *error =
        NULL;

    test_graph_loader_insert_entity(
        &fixture,
        TEST_ENTITY_A_IDENTIFIER,
        "Personne A"
    );

    test_graph_loader_execute_sql(
        fixture.database,
        "ALTER TABLE relations "
        "RENAME TO relations_missing;"
    );

    g_test_expect_message(
        NULL,
        G_LOG_LEVEL_WARNING,
        "*Impossible de préparer la requête SQL*"
    );

    graph_model =
        investigation_graph_loader_load(
            fixture.graph_loader,
            &error
        );

    g_test_assert_expected_messages();

    assert(graph_model == NULL);

    test_graph_loader_assert_error(
        error,
        INVESTIGATION_GRAPH_LOADER_ERROR_RELATION_LOAD
    );

    g_clear_error(
        &error
    );

    test_graph_loader_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d'une relation dont la source est absente.
 */
static void test_graph_loader_missing_source_entity(void)
{
    TestInvestigationGraphLoaderFixture fixture =
        test_graph_loader_fixture_create();

    InvestigationGraphModel *graph_model =
        NULL;

    GError *error =
        NULL;

    test_graph_loader_insert_entity(
        &fixture,
        TEST_ENTITY_B_IDENTIFIER,
        "Personne B"
    );

    test_graph_loader_insert_inconsistent_relation(
        &fixture,
        TEST_RELATION_AB_IDENTIFIER,
        TEST_ENTITY_A_IDENTIFIER,
        TEST_ENTITY_B_IDENTIFIER
    );

    graph_model =
        investigation_graph_loader_load(
            fixture.graph_loader,
            &error
        );

    assert(graph_model == NULL);

    test_graph_loader_assert_error(
        error,
        INVESTIGATION_GRAPH_LOADER_ERROR_RELATION_TRANSFER
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

    /*
     * Après correction de l'incohérence, le même chargeur doit fonctionner.
     */
    test_graph_loader_insert_entity(
        &fixture,
        TEST_ENTITY_A_IDENTIFIER,
        "Personne A"
    );

    graph_model =
        investigation_graph_loader_load(
            fixture.graph_loader,
            &error
        );

    assert(graph_model != NULL);
    assert(error == NULL);

    assert(
        investigation_graph_model_get_relation_count(
            graph_model
        ) == 1
    );

    investigation_graph_model_free(
        graph_model
    );

    test_graph_loader_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d'une relation dont la cible est absente.
 */
static void test_graph_loader_missing_target_entity(void)
{
    TestInvestigationGraphLoaderFixture fixture =
        test_graph_loader_fixture_create();

    InvestigationGraphModel *graph_model =
        NULL;

    GError *error =
        NULL;

    test_graph_loader_insert_entity(
        &fixture,
        TEST_ENTITY_A_IDENTIFIER,
        "Personne A"
    );

    test_graph_loader_insert_inconsistent_relation(
        &fixture,
        TEST_RELATION_AB_IDENTIFIER,
        TEST_ENTITY_A_IDENTIFIER,
        TEST_ENTITY_B_IDENTIFIER
    );

    graph_model =
        investigation_graph_loader_load(
            fixture.graph_loader,
            &error
        );

    assert(graph_model == NULL);

    test_graph_loader_assert_error(
        error,
        INVESTIGATION_GRAPH_LOADER_ERROR_RELATION_TRANSFER
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

    test_graph_loader_fixture_clear(
        &fixture
    );
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

    test_graph_loader_new_null_database();
    test_graph_loader_lifecycle();
    test_graph_loader_load_empty_database();
    test_graph_loader_load_entities_only();
    test_graph_loader_load_complete_graph();
    test_graph_loader_independent_graphs();
    test_graph_loader_invalid_arguments();
    test_graph_loader_missing_entities_table();
    test_graph_loader_missing_relations_table();
    test_graph_loader_missing_source_entity();
    test_graph_loader_missing_target_entity();

    printf(
        "InvestigationGraphLoader : tous les tests sont valides.\n"
    );

    return 0;
}
