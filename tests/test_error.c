/******************************************************************************
 * @file test_error.c
 * @brief Tests de l'infrastructure d'erreurs Database.
 ******************************************************************************/

#include "database/database.h"
#include "database/error.h"
#include "database/statement.h"

#include <assert.h>
#include <stdio.h>

/**
 * @brief Vérifie l'état initial d'une connexion.
 */
static void test_initial_error_state(void)
{
    Database *database = NULL;

    database = database_open(":memory:");

    assert(database != NULL);

    assert(
        database_error_get_code(database) ==
        DATABASE_ERROR_NONE
    );

    assert(
        database_error_get_message(database) == NULL
    );

    database_close(database);
}

/**
 * @brief Vérifie l'enregistrement d'une erreur SQLite.
 */
static void test_statement_error(void)
{
    Database *database = NULL;
    DatabaseStatement *statement = NULL;
    const char *error_message = NULL;

    database = database_open(":memory:");

    assert(database != NULL);

    statement = database_statement_prepare(
        database,
        "SELECT FROM;"
    );

    assert(statement == NULL);

    assert(
        database_error_get_code(database) ==
        DATABASE_ERROR_SQLITE
    );

    error_message = database_error_get_message(database);

    assert(error_message != NULL);
    assert(error_message[0] != '\0');

    database_error_clear(database);

    assert(
        database_error_get_code(database) ==
        DATABASE_ERROR_NONE
    );

    assert(
        database_error_get_message(database) == NULL
    );

    database_close(database);
}

/**
 * @brief Vérifie qu'une opération valide efface l'erreur précédente.
 */
static void test_success_clears_previous_error(void)
{
    Database *database = NULL;
    DatabaseStatement *statement = NULL;

    database = database_open(":memory:");

    assert(database != NULL);

    statement = database_statement_prepare(
        database,
        "SELECT FROM;"
    );

    assert(statement == NULL);

    assert(
        database_error_get_code(database) ==
        DATABASE_ERROR_SQLITE
    );

    statement = database_statement_prepare(
        database,
        "SELECT 1;"
    );

    assert(statement != NULL);

    assert(
        database_error_get_code(database) ==
        DATABASE_ERROR_NONE
    );

    assert(
        database_error_get_message(database) == NULL
    );

    database_statement_finalize(statement);
    database_close(database);
}

int main(void)
{
    test_initial_error_state();
    test_statement_error();
    test_success_clears_previous_error();

    printf(
        "DatabaseError : tous les tests sont valides.\n"
    );

    return 0;
}
