/******************************************************************************
 * @file test_error.c
 * @brief Tests de l'infrastructure d'erreurs Database.
 ******************************************************************************/

#include "database/database.h"
#include "database/error.h"
#include "database/statement.h"

#include <assert.h>
#include <stdio.h>
#include <stdint.h>

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

/**
 * @brief Vérifie les erreurs produites par un binding texte.
 */
static void test_statement_bind_error(void)
{
    Database *database = NULL;
    DatabaseStatement *statement = NULL;
    const char *error_message = NULL;

    database = database_open(":memory:");

    assert(database != NULL);

    statement = database_statement_prepare(
        database,
        "SELECT ?;"
    );

    assert(statement != NULL);

    /*
     * La requête ne possède qu'un seul paramètre.
     */
    assert(
        !database_statement_bind_text(
            statement,
            2,
            "hors limite"
        )
    );

    assert(
        database_error_get_code(database) ==
        DATABASE_ERROR_SQLITE
    );

    error_message = database_error_get_message(
        database
    );

    assert(error_message != NULL);
    assert(error_message[0] != '\0');

    /*
     * Un binding valide efface l'erreur précédente.
     */
    assert(
        database_statement_bind_text(
            statement,
            1,
            "valeur valide"
        )
    );

    assert(
        database_error_get_code(database) ==
        DATABASE_ERROR_NONE
    );

    /*
     * Un indice invalide appartient aux erreurs d'argument.
     */
    assert(
        !database_statement_bind_text(
            statement,
            0,
            "valeur"
        )
    );

    assert(
        database_error_get_code(database) ==
        DATABASE_ERROR_INVALID_ARGUMENT
    );

    database_statement_finalize(statement);
    database_close(database);
}

/**
 * @brief Vérifie les erreurs produites par les bindings entier et NULL.
 */
static void test_statement_other_bind_errors(void)
{
    Database *database = NULL;
    DatabaseStatement *statement = NULL;

    database = database_open(":memory:");

    assert(database != NULL);

    statement = database_statement_prepare(
        database,
        "SELECT ?;"
    );

    assert(statement != NULL);

    /*
     * Indice hors limite : erreur SQLite.
     */
    assert(
        !database_statement_bind_int64(
            statement,
            2,
            INT64_C(42)
        )
    );

    assert(
        database_error_get_code(database) ==
        DATABASE_ERROR_SQLITE
    );

    /*
     * Un binding valide efface l'erreur précédente.
     */
    assert(
        database_statement_bind_int64(
            statement,
            1,
            INT64_C(42)
        )
    );

    assert(
        database_error_get_code(database) ==
        DATABASE_ERROR_NONE
    );

    /*
     * Indice invalide : erreur d'argument.
     */
    assert(
        !database_statement_bind_int64(
            statement,
            0,
            INT64_C(42)
        )
    );

    assert(
        database_error_get_code(database) ==
        DATABASE_ERROR_INVALID_ARGUMENT
    );

    /*
     * Le binding NULL valide efface l'erreur précédente.
     */
    assert(
        database_statement_bind_null(
            statement,
            1
        )
    );

    assert(
        database_error_get_code(database) ==
        DATABASE_ERROR_NONE
    );

    /*
     * Indice hors limite : erreur SQLite.
     */
    assert(
        !database_statement_bind_null(
            statement,
            2
        )
    );

    assert(
        database_error_get_code(database) ==
        DATABASE_ERROR_SQLITE
    );

    /*
     * Indice invalide : erreur d'argument.
     */
    assert(
        !database_statement_bind_null(
            statement,
            0
        )
    );

    assert(
        database_error_get_code(database) ==
        DATABASE_ERROR_INVALID_ARGUMENT
    );

    database_statement_finalize(statement);
    database_close(database);
}

/**
 * @brief Vérifie l'enregistrement d'une erreur pendant sqlite3_step().
 */
static void test_statement_step_error(void)
{
    Database *database = NULL;
    DatabaseStatement *statement = NULL;
    const char *error_message = NULL;

    database = database_open(":memory:");

    assert(database != NULL);

    statement = database_statement_prepare(
        database,
        "CREATE TABLE step_error_test"
        "("
        "    value TEXT NOT NULL UNIQUE"
        ");"
    );

    assert(statement != NULL);

    assert(
        database_statement_step(statement) ==
        DATABASE_STATEMENT_STEP_DONE
    );

    database_statement_finalize(statement);

    /*
     * Première insertion valide.
     */
    statement = database_statement_prepare(
        database,
        "INSERT INTO step_error_test (value) "
        "VALUES (?);"
    );

    assert(statement != NULL);

    assert(
        database_statement_bind_text(
            statement,
            1,
            "valeur unique"
        )
    );

    assert(
        database_statement_step(statement) ==
        DATABASE_STATEMENT_STEP_DONE
    );

    database_statement_finalize(statement);

    /*
     * La seconde insertion viole la contrainte UNIQUE.
     * La préparation et le binding réussissent, mais step() échoue.
     */
    statement = database_statement_prepare(
        database,
        "INSERT INTO step_error_test (value) "
        "VALUES (?);"
    );

    assert(statement != NULL);

    assert(
        database_statement_bind_text(
            statement,
            1,
            "valeur unique"
        )
    );

    assert(
        database_statement_step(statement) ==
        DATABASE_STATEMENT_STEP_ERROR
    );

    assert(
        database_error_get_code(database) ==
        DATABASE_ERROR_SQLITE
    );

    error_message = database_error_get_message(
        database
    );

    assert(error_message != NULL);
    assert(error_message[0] != '\0');

    database_statement_finalize(statement);

    /*
     * Une opération valide ultérieure efface l'erreur.
     */
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
        database_statement_step(statement) ==
        DATABASE_STATEMENT_STEP_ROW
    );

    assert(
        database_error_get_code(database) ==
        DATABASE_ERROR_NONE
    );

    database_statement_finalize(statement);
    database_close(database);
}

int main(void)
{
    test_initial_error_state();
    test_statement_error();
    test_success_clears_previous_error();
    test_statement_bind_error();
    test_statement_other_bind_errors();
    test_statement_step_error();

    printf(
        "DatabaseError : tous les tests sont valides.\n"
    );

    return 0;
}
