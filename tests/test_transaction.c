/******************************************************************************
 * @file test_transaction.c
 * @brief Tests du module DatabaseTransaction.
 ******************************************************************************/

#include "database/database.h"
#include "database/statement.h"
#include "database/transaction.h"

#include <assert.h>
#include <stdio.h>

/**
 * @brief Crée la table utilisée par les tests.
 */
static void test_create_table(
    Database *database
)
{
    DatabaseStatement *statement = NULL;

    statement = database_statement_prepare(
        database,
        "CREATE TABLE transaction_test"
        "("
        "    value TEXT NOT NULL"
        ");"
    );

    assert(statement != NULL);

    assert(
        database_statement_step(statement) ==
        DATABASE_STATEMENT_STEP_DONE
    );

    database_statement_finalize(statement);
}

/**
 * @brief Insère une valeur dans la table de test.
 */
static void test_insert_value(
    Database *database,
    const char *value
)
{
    DatabaseStatement *statement = NULL;

    statement = database_statement_prepare(
        database,
        "INSERT INTO transaction_test (value)"
        " VALUES (?);"
    );

    assert(statement != NULL);

    assert(
        database_statement_bind_text(
            statement,
            1,
            value
        )
    );

    assert(
        database_statement_step(statement) ==
        DATABASE_STATEMENT_STEP_DONE
    );

    database_statement_finalize(statement);
}

/**
 * @brief Compte les lignes de la table de test.
 */
static int64_t test_count_values(
    Database *database
)
{
    DatabaseStatement *statement = NULL;
    int64_t count = INT64_C(0);

    statement = database_statement_prepare(
        database,
        "SELECT COUNT(*) FROM transaction_test;"
    );

    assert(statement != NULL);

    assert(
        database_statement_step(statement) ==
        DATABASE_STATEMENT_STEP_ROW
    );

    assert(
        database_statement_column_int64(
            statement,
            0,
            &count
        )
    );

    database_statement_finalize(statement);

    return count;
}

/**
 * @brief Vérifie qu'un commit conserve les modifications.
 */
static void test_transaction_commit(void)
{
    Database *database = NULL;

    database = database_open(":memory:");

    assert(database != NULL);

    test_create_table(database);

    assert(
        database_transaction_begin(database)
    );

    test_insert_value(
        database,
        "valeur validée"
    );

    assert(
        database_transaction_commit(database)
    );

    assert(
        test_count_values(database) ==
        INT64_C(1)
    );

    database_close(database);
}

/**
 * @brief Vérifie qu'un rollback annule les modifications.
 */
static void test_transaction_rollback(void)
{
    Database *database = NULL;

    database = database_open(":memory:");

    assert(database != NULL);

    test_create_table(database);

    assert(
        database_transaction_begin(database)
    );

    test_insert_value(
        database,
        "valeur annulée"
    );

    assert(
        database_transaction_rollback(database)
    );

    assert(
        test_count_values(database) ==
        INT64_C(0)
    );

    database_close(database);
}

/**
 * @brief Vérifie le refus des états de transaction invalides.
 */
static void test_invalid_transaction_states(void)
{
    Database *database = NULL;

    database = database_open(":memory:");

    assert(database != NULL);

    assert(
        !database_transaction_commit(database)
    );

    assert(
        !database_transaction_rollback(database)
    );

    assert(
        database_transaction_begin(database)
    );

    assert(
        !database_transaction_begin(database)
    );

    assert(
        database_transaction_rollback(database)
    );

    assert(
        !database_transaction_rollback(database)
    );

    assert(
        !database_transaction_begin(NULL)
    );

    assert(
        !database_transaction_commit(NULL)
    );

    assert(
        !database_transaction_rollback(NULL)
    );

    database_close(database);
}

int main(void)
{
    test_transaction_commit();
    test_transaction_rollback();
    test_invalid_transaction_states();

    printf(
        "DatabaseTransaction : tous les tests sont valides.\n"
    );

    return 0;
}
