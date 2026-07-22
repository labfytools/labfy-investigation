/******************************************************************************
 * @file test_graph_node_position_dao.c
 * @brief Tests du DAO des positions persistées du graphe.
 ******************************************************************************/

#include "dao/graph_node_position_dao.h"

#include "database/database.h"
#include "database/statement.h"
#include "models/graph_node_position.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

/**
 * @brief Environnement temporaire d'un test GraphNodePositionDao.
 */
typedef struct
{
    char *temporary_directory;
    char *database_path;

    Database *database;
    GraphNodePositionDao *position_dao;
} TestGraphNodePositionDaoFixture;

/**
 * @brief Crée une base temporaire complète et son DAO.
 */
static TestGraphNodePositionDaoFixture
test_graph_node_position_dao_fixture_create(void)
{
    TestGraphNodePositionDaoFixture fixture =
        {0};

    GError *error =
        NULL;

    fixture.temporary_directory =
        g_dir_make_tmp(
            "labfy-graph-node-position-dao-test-XXXXXX",
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        fixture.temporary_directory
    );

    fixture.database_path =
        g_build_filename(
            fixture.temporary_directory,
            "Enquete.sqlite",
            NULL
        );

    g_assert_nonnull(
        fixture.database_path
    );

    g_assert_true(
        database_initialize(
            fixture.database_path,
            "Enquete_GraphNodePositionDao",
            fixture.temporary_directory
        )
    );

    fixture.database =
        database_open(
            fixture.database_path
        );

    g_assert_nonnull(
        fixture.database
    );

    fixture.position_dao =
        graph_node_position_dao_new(
            fixture.database,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        fixture.position_dao
    );

    return fixture;
}

/**
 * @brief Détruit une fixture et ses fichiers temporaires.
 */
static void test_graph_node_position_dao_fixture_clear(
    TestGraphNodePositionDaoFixture *fixture
)
{
    g_assert_nonnull(
        fixture
    );

    graph_node_position_dao_free(
        fixture->position_dao
    );

    database_close(
        fixture->database
    );

    g_assert_cmpint(
        g_remove(
            fixture->database_path
        ),
        ==,
        0
    );

    g_assert_cmpint(
        g_rmdir(
            fixture->temporary_directory
        ),
        ==,
        0
    );

    g_clear_pointer(
        &fixture->database_path,
        g_free
    );

    g_clear_pointer(
        &fixture->temporary_directory,
        g_free
    );

    fixture->position_dao =
        NULL;

    fixture->database =
        NULL;
}

/**
 * @brief Insère une entité minimale à laquelle rattacher une position.
 */
static void test_graph_node_position_dao_insert_entity(
    Database *database,
    const char *node_identifier,
    const char *entity_value
)
{
    DatabaseStatement *statement =
        NULL;

    g_assert_nonnull(
        database
    );

    g_assert_nonnull(
        node_identifier
    );

    g_assert_nonnull(
        entity_value
    );

    statement =
        database_statement_prepare(
            database,
            "INSERT INTO entites"
            "("
            "    id,"
            "    type_id,"
            "    valeur,"
            "    confiance,"
            "    created_at,"
            "    updated_at,"
            "    status"
            ")"
            "VALUES"
            "("
            "    ?,"
            "    14,"
            "    ?,"
            "    50,"
            "    '2026-07-22T06:00:00Z',"
            "    '2026-07-22T06:00:00Z',"
            "    'active'"
            ");"
        );

    g_assert_nonnull(
        statement
    );

    g_assert_true(
        database_statement_bind_text(
            statement,
            1,
            node_identifier
        )
    );

    g_assert_true(
        database_statement_bind_text(
            statement,
            2,
            entity_value
        )
    );

    g_assert_cmpint(
        database_statement_step(
            statement
        ),
        ==,
        DATABASE_STATEMENT_STEP_DONE
    );

    database_statement_finalize(
        statement
    );
}

/**
 * @brief Retourne le nombre de positions présentes dans SQLite.
 */
static int64_t test_graph_node_position_dao_count_rows(
    Database *database
)
{
    DatabaseStatement *statement =
        NULL;

    int64_t position_count =
        -1;

    g_assert_nonnull(
        database
    );

    statement =
        database_statement_prepare(
            database,
            "SELECT COUNT(*) "
            "FROM graph_layout_positions;"
        );

    g_assert_nonnull(
        statement
    );

    g_assert_cmpint(
        database_statement_step(
            statement
        ),
        ==,
        DATABASE_STATEMENT_STEP_ROW
    );

    g_assert_true(
        database_statement_column_int64(
            statement,
            0,
            &position_count
        )
    );

    g_assert_cmpint(
        database_statement_step(
            statement
        ),
        ==,
        DATABASE_STATEMENT_STEP_DONE
    );

    database_statement_finalize(
        statement
    );

    return position_count;
}

static void test_graph_node_position_dao_new_null_database(void)
{
    GraphNodePositionDao *position_dao =
        NULL;

    GError *error =
        NULL;

    position_dao =
        graph_node_position_dao_new(
            NULL,
            &error
        );

    g_assert_null(
        position_dao
    );

    g_assert_error(
        error,
        GRAPH_NODE_POSITION_DAO_ERROR,
        GRAPH_NODE_POSITION_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );
}

static void test_graph_node_position_dao_new_valid(void)
{
    Database *database =
        NULL;

    GraphNodePositionDao *position_dao =
        NULL;

    GError *error =
        NULL;

    database =
        database_open(
            ":memory:"
        );

    g_assert_nonnull(
        database
    );

    position_dao =
        graph_node_position_dao_new(
            database,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        position_dao
    );

    graph_node_position_dao_free(
        position_dao
    );

    database_close(
        database
    );
}

static void test_graph_node_position_dao_free_null(void)
{
    graph_node_position_dao_free(
        NULL
    );
}

static void test_graph_node_position_dao_list_empty(void)
{
    TestGraphNodePositionDaoFixture fixture =
        test_graph_node_position_dao_fixture_create();

    GPtrArray *positions =
        NULL;

    GError *error =
        NULL;

    positions =
        graph_node_position_dao_list_all(
            fixture.position_dao,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        positions
    );

    g_assert_cmpuint(
        positions->len,
        ==,
        0
    );

    g_ptr_array_unref(
        positions
    );

    test_graph_node_position_dao_fixture_clear(
        &fixture
    );
}

static void test_graph_node_position_dao_list_invalid_arguments(void)
{
    GPtrArray *positions =
        NULL;

    GError *error =
        NULL;

    positions =
        graph_node_position_dao_list_all(
            NULL,
            &error
        );

    g_assert_null(
        positions
    );

    g_assert_error(
        error,
        GRAPH_NODE_POSITION_DAO_ERROR,
        GRAPH_NODE_POSITION_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );
}

static void test_graph_node_position_dao_upsert_insert(void)
{
    TestGraphNodePositionDaoFixture fixture =
        test_graph_node_position_dao_fixture_create();

    const GraphNodePosition *position =
        NULL;

    GPtrArray *positions =
        NULL;

    GError *error =
        NULL;

    const char *node_identifier =
        "10000000-0000-4000-8000-000000000001";

    /* Un nœud générique peut être une relation et n'a pas à exister dans
     * la table entites. */

    g_assert_true(
        graph_node_position_dao_upsert(
            fixture.position_dao,
            node_identifier,
            120.5,
            -42.25,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_cmpint(
        test_graph_node_position_dao_count_rows(
            fixture.database
        ),
        ==,
        1
    );

    positions =
        graph_node_position_dao_list_all(
            fixture.position_dao,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        positions
    );

    g_assert_cmpuint(
        positions->len,
        ==,
        1
    );

    position =
        g_ptr_array_index(
            positions,
            0
        );

    g_assert_nonnull(
        position
    );

    g_assert_cmpstr(
        graph_node_position_get_node_identifier(
            position
        ),
        ==,
        node_identifier
    );

    g_assert_cmpfloat(
        graph_node_position_get_x(
            position
        ),
        ==,
        120.5
    );

    g_assert_cmpfloat(
        graph_node_position_get_y(
            position
        ),
        ==,
        -42.25
    );

    g_assert_nonnull(
        graph_node_position_get_updated_at(
            position
        )
    );

    g_assert_cmpuint(
        strlen(
            graph_node_position_get_updated_at(
                position
            )
        ),
        ==,
        20
    );

    g_ptr_array_unref(
        positions
    );

    test_graph_node_position_dao_fixture_clear(
        &fixture
    );
}

static void test_graph_node_position_dao_upsert_update(void)
{
    TestGraphNodePositionDaoFixture fixture =
        test_graph_node_position_dao_fixture_create();

    const GraphNodePosition *position =
        NULL;

    GPtrArray *positions =
        NULL;

    GError *error =
        NULL;

    const char *node_identifier =
        "20000000-0000-4000-8000-000000000002";

    test_graph_node_position_dao_insert_entity(
        fixture.database,
        node_identifier,
        "entite-position-2"
    );

    g_assert_true(
        graph_node_position_dao_upsert(
            fixture.position_dao,
            node_identifier,
            10.0,
            20.0,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        graph_node_position_dao_upsert(
            fixture.position_dao,
            node_identifier,
            -300.75,
            450.5,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_cmpint(
        test_graph_node_position_dao_count_rows(
            fixture.database
        ),
        ==,
        1
    );

    positions =
        graph_node_position_dao_list_all(
            fixture.position_dao,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_cmpuint(
        positions->len,
        ==,
        1
    );

    position =
        g_ptr_array_index(
            positions,
            0
        );

    g_assert_cmpfloat(
        graph_node_position_get_x(
            position
        ),
        ==,
        -300.75
    );

    g_assert_cmpfloat(
        graph_node_position_get_y(
            position
        ),
        ==,
        450.5
    );

    g_ptr_array_unref(
        positions
    );

    test_graph_node_position_dao_fixture_clear(
        &fixture
    );
}

static void test_graph_node_position_dao_upsert_invalid_arguments(void)
{
    TestGraphNodePositionDaoFixture fixture =
        test_graph_node_position_dao_fixture_create();

    GError *error =
        NULL;

    g_assert_false(
        graph_node_position_dao_upsert(
            NULL,
            "30000000-0000-4000-8000-000000000003",
            1.0,
            2.0,
            &error
        )
    );

    g_assert_error(
        error,
        GRAPH_NODE_POSITION_DAO_ERROR,
        GRAPH_NODE_POSITION_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    g_assert_false(
        graph_node_position_dao_upsert(
            fixture.position_dao,
            "identifiant-invalide",
            1.0,
            2.0,
            &error
        )
    );

    g_assert_error(
        error,
        GRAPH_NODE_POSITION_DAO_ERROR,
        GRAPH_NODE_POSITION_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    g_assert_false(
        graph_node_position_dao_upsert(
            fixture.position_dao,
            "30000000-0000-4000-8000-000000000003",
            NAN,
            2.0,
            &error
        )
    );

    g_assert_error(
        error,
        GRAPH_NODE_POSITION_DAO_ERROR,
        GRAPH_NODE_POSITION_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    g_assert_false(
        graph_node_position_dao_upsert(
            fixture.position_dao,
            "30000000-0000-4000-8000-000000000003",
            1.0,
            INFINITY,
            &error
        )
    );

    g_assert_error(
        error,
        GRAPH_NODE_POSITION_DAO_ERROR,
        GRAPH_NODE_POSITION_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    test_graph_node_position_dao_fixture_clear(
        &fixture
    );
}

static void test_graph_node_position_dao_list_order(void)
{
    TestGraphNodePositionDaoFixture fixture =
        test_graph_node_position_dao_fixture_create();

    const GraphNodePosition *first_position =
        NULL;

    const GraphNodePosition *second_position =
        NULL;

    GPtrArray *positions =
        NULL;

    GError *error =
        NULL;

    const char *first_identifier =
        "40000000-0000-4000-8000-000000000004";

    const char *second_identifier =
        "50000000-0000-4000-8000-000000000005";

    test_graph_node_position_dao_insert_entity(
        fixture.database,
        first_identifier,
        "entite-position-4"
    );

    test_graph_node_position_dao_insert_entity(
        fixture.database,
        second_identifier,
        "entite-position-5"
    );

    g_assert_true(
        graph_node_position_dao_upsert(
            fixture.position_dao,
            second_identifier,
            50.0,
            60.0,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        graph_node_position_dao_upsert(
            fixture.position_dao,
            first_identifier,
            40.0,
            45.0,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    positions =
        graph_node_position_dao_list_all(
            fixture.position_dao,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_cmpuint(
        positions->len,
        ==,
        2
    );

    first_position =
        g_ptr_array_index(
            positions,
            0
        );

    second_position =
        g_ptr_array_index(
            positions,
            1
        );

    g_assert_cmpstr(
        graph_node_position_get_node_identifier(
            first_position
        ),
        ==,
        first_identifier
    );

    g_assert_cmpstr(
        graph_node_position_get_node_identifier(
            second_position
        ),
        ==,
        second_identifier
    );

    g_ptr_array_unref(
        positions
    );

    test_graph_node_position_dao_fixture_clear(
        &fixture
    );
}

static void test_graph_node_position_dao_delete_existing(void)
{
    TestGraphNodePositionDaoFixture fixture =
        test_graph_node_position_dao_fixture_create();

    GError *error =
        NULL;

    const char *node_identifier =
        "60000000-0000-4000-8000-000000000006";

    test_graph_node_position_dao_insert_entity(
        fixture.database,
        node_identifier,
        "entite-position-6"
    );

    g_assert_true(
        graph_node_position_dao_upsert(
            fixture.position_dao,
            node_identifier,
            60.0,
            70.0,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        graph_node_position_dao_delete(
            fixture.position_dao,
            node_identifier,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_cmpint(
        test_graph_node_position_dao_count_rows(
            fixture.database
        ),
        ==,
        0
    );

    test_graph_node_position_dao_fixture_clear(
        &fixture
    );
}

static void test_graph_node_position_dao_delete_missing(void)
{
    TestGraphNodePositionDaoFixture fixture =
        test_graph_node_position_dao_fixture_create();

    GError *error =
        NULL;

    g_assert_true(
        graph_node_position_dao_delete(
            fixture.position_dao,
            "70000000-0000-4000-8000-000000000007",
            &error
        )
    );

    g_assert_no_error(
        error
    );

    test_graph_node_position_dao_fixture_clear(
        &fixture
    );
}

static void test_graph_node_position_dao_delete_invalid_arguments(void)
{
    TestGraphNodePositionDaoFixture fixture =
        test_graph_node_position_dao_fixture_create();

    GError *error =
        NULL;

    g_assert_false(
        graph_node_position_dao_delete(
            NULL,
            "80000000-0000-4000-8000-000000000008",
            &error
        )
    );

    g_assert_error(
        error,
        GRAPH_NODE_POSITION_DAO_ERROR,
        GRAPH_NODE_POSITION_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    g_assert_false(
        graph_node_position_dao_delete(
            fixture.position_dao,
            "identifiant-invalide",
            &error
        )
    );

    g_assert_error(
        error,
        GRAPH_NODE_POSITION_DAO_ERROR,
        GRAPH_NODE_POSITION_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    test_graph_node_position_dao_fixture_clear(
        &fixture
    );
}

static void test_graph_node_position_dao_delete_all_populated(void)
{
    TestGraphNodePositionDaoFixture fixture =
        test_graph_node_position_dao_fixture_create();

    GError *error =
        NULL;

    const char *first_identifier =
        "90000000-0000-4000-8000-000000000009";

    const char *second_identifier =
        "a0000000-0000-4000-8000-00000000000a";

    test_graph_node_position_dao_insert_entity(
        fixture.database,
        first_identifier,
        "entite-position-9"
    );

    test_graph_node_position_dao_insert_entity(
        fixture.database,
        second_identifier,
        "entite-position-10"
    );

    g_assert_true(
        graph_node_position_dao_upsert(
            fixture.position_dao,
            first_identifier,
            90.0,
            91.0,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_true(
        graph_node_position_dao_upsert(
            fixture.position_dao,
            second_identifier,
            100.0,
            101.0,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_cmpint(
        test_graph_node_position_dao_count_rows(
            fixture.database
        ),
        ==,
        2
    );

    g_assert_true(
        graph_node_position_dao_delete_all(
            fixture.position_dao,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_cmpint(
        test_graph_node_position_dao_count_rows(
            fixture.database
        ),
        ==,
        0
    );

    test_graph_node_position_dao_fixture_clear(
        &fixture
    );
}

static void test_graph_node_position_dao_delete_all_empty(void)
{
    TestGraphNodePositionDaoFixture fixture =
        test_graph_node_position_dao_fixture_create();

    GError *error =
        NULL;

    g_assert_true(
        graph_node_position_dao_delete_all(
            fixture.position_dao,
            &error
        )
    );

    g_assert_no_error(
        error
    );

    g_assert_cmpint(
        test_graph_node_position_dao_count_rows(
            fixture.database
        ),
        ==,
        0
    );

    test_graph_node_position_dao_fixture_clear(
        &fixture
    );
}

static void test_graph_node_position_dao_delete_all_invalid_arguments(void)
{
    GError *error =
        NULL;

    g_assert_false(
        graph_node_position_dao_delete_all(
            NULL,
            &error
        )
    );

    g_assert_error(
        error,
        GRAPH_NODE_POSITION_DAO_ERROR,
        GRAPH_NODE_POSITION_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
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

    g_test_add_func(
        "/graph-node-position-dao/new-null-database",
        test_graph_node_position_dao_new_null_database
    );

    g_test_add_func(
        "/graph-node-position-dao/new-valid",
        test_graph_node_position_dao_new_valid
    );

    g_test_add_func(
        "/graph-node-position-dao/free-null",
        test_graph_node_position_dao_free_null
    );

    g_test_add_func(
        "/graph-node-position-dao/list-empty",
        test_graph_node_position_dao_list_empty
    );

    g_test_add_func(
        "/graph-node-position-dao/list-invalid-arguments",
        test_graph_node_position_dao_list_invalid_arguments
    );

    g_test_add_func(
        "/graph-node-position-dao/upsert-insert",
        test_graph_node_position_dao_upsert_insert
    );

    g_test_add_func(
        "/graph-node-position-dao/upsert-update",
        test_graph_node_position_dao_upsert_update
    );

    g_test_add_func(
        "/graph-node-position-dao/upsert-invalid-arguments",
        test_graph_node_position_dao_upsert_invalid_arguments
    );

    g_test_add_func(
        "/graph-node-position-dao/list-order",
        test_graph_node_position_dao_list_order
    );

    g_test_add_func(
        "/graph-node-position-dao/delete-existing",
        test_graph_node_position_dao_delete_existing
    );

    g_test_add_func(
        "/graph-node-position-dao/delete-missing",
        test_graph_node_position_dao_delete_missing
    );

    g_test_add_func(
        "/graph-node-position-dao/delete-invalid-arguments",
        test_graph_node_position_dao_delete_invalid_arguments
    );

    g_test_add_func(
        "/graph-node-position-dao/delete-all-populated",
        test_graph_node_position_dao_delete_all_populated
    );

    g_test_add_func(
        "/graph-node-position-dao/delete-all-empty",
        test_graph_node_position_dao_delete_all_empty
    );

    g_test_add_func(
        "/graph-node-position-dao/delete-all-invalid-arguments",
        test_graph_node_position_dao_delete_all_invalid_arguments
    );

    return g_test_run();
}
