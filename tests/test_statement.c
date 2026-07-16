/******************************************************************************
 * @file test_statement.c
 * @brief Tests du module DatabaseStatement.
 ******************************************************************************/

#include "database/database.h"
#include "database/statement.h"

#include <assert.h>
#include <stdio.h>
#include <glib.h>

/**
 * @brief Vérifie la préparation et la finalisation d'une requête valide.
 */
static void test_prepare_valid_statement(void)
{
    Database *database = NULL;
    DatabaseStatement *statement = NULL;

    database = database_open(":memory:");

    assert(database != NULL);

    statement = database_statement_prepare(
        database,
        "SELECT 1;"
    );

    assert(statement != NULL);

    database_statement_finalize(statement);
    database_close(database);
}

/**
 * @brief Vérifie qu'une requête SQL invalide est refusée.
 */
static void test_prepare_invalid_statement(void)
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

    database_close(database);
}

/**
 * @brief Vérifie le traitement des paramètres invalides.
 */
static void test_invalid_parameters(void)
{
    Database *database = NULL;

    database = database_open(":memory:");

    assert(database != NULL);

    assert(
        database_statement_prepare(
            NULL,
            "SELECT 1;"
        ) == NULL
    );

    assert(
        database_statement_prepare(
            database,
            NULL
        ) == NULL
    );

    assert(
        database_statement_prepare(
            database,
            ""
        ) == NULL
    );

    /*
     * La fonction doit accepter NULL sans provoquer d'erreur.
     */
    database_statement_finalize(NULL);

    database_close(database);
}

/**
 * @brief Vérifie la liaison valide d'un paramètre texte.
 */
static void test_bind_valid_text(void)
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

    assert(
        database_statement_bind_text(
            statement,
            1,
            "Labfy Investigation"
        )
    );

    database_statement_finalize(statement);
    database_close(database);
}

/**
 * @brief Vérifie le refus des paramètres invalides.
 */
static void test_bind_invalid_text(void)
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

    assert(
        !database_statement_bind_text(
            NULL,
            1,
            "test"
        )
    );

    assert(
        !database_statement_bind_text(
            statement,
            0,
            "test"
        )
    );

    assert(
        !database_statement_bind_text(
            statement,
            1,
            NULL
        )
    );

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

    database_statement_finalize(statement);
    database_close(database);
}

/**
 * @brief Vérifie l'exécution d'une requête retournant une ligne.
 */
static void test_step_select_statement(void)
{
    Database *database = NULL;
    DatabaseStatement *statement = NULL;

    database = database_open(":memory:");

    assert(database != NULL);

    statement = database_statement_prepare(
        database,
        "SELECT 1;"
    );

    assert(statement != NULL);

    assert(
        database_statement_step(statement) ==
        DATABASE_STATEMENT_STEP_ROW
    );

    assert(
        database_statement_step(statement) ==
        DATABASE_STATEMENT_STEP_DONE
    );

    database_statement_finalize(statement);
    database_close(database);
}

/**
 * @brief Vérifie l'exécution d'une requête sans ligne de résultat.
 */
static void test_step_non_query_statement(void)
{
    Database *database = NULL;
    DatabaseStatement *statement = NULL;

    database = database_open(":memory:");

    assert(database != NULL);

    statement = database_statement_prepare(
        database,
        "CREATE TABLE test_step"
        "("
        "    id INTEGER PRIMARY KEY"
        ");"
    );

    assert(statement != NULL);

    assert(
        database_statement_step(statement) ==
        DATABASE_STATEMENT_STEP_DONE
    );

    database_statement_finalize(statement);
    database_close(database);
}

/**
 * @brief Vérifie le traitement d'une requête absente.
 */
static void test_step_invalid_statement(void)
{
    assert(
        database_statement_step(NULL) ==
        DATABASE_STATEMENT_STEP_ERROR
    );
}

/**
 * @brief Vérifie qu'une requête peut être réexécutée après reset.
 */
static void test_reset_statement(void)
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

    assert(
        database_statement_bind_text(
            statement,
            1,
            "Labfy Investigation"
        )
    );

    assert(
        database_statement_step(statement) ==
        DATABASE_STATEMENT_STEP_ROW
    );

    assert(
        database_statement_step(statement) ==
        DATABASE_STATEMENT_STEP_DONE
    );

    assert(
        database_statement_reset(statement)
    );

    /*
     * Le paramètre texte est toujours lié après le reset.
     */
    assert(
        database_statement_step(statement) ==
        DATABASE_STATEMENT_STEP_ROW
    );

    assert(
        database_statement_step(statement) ==
        DATABASE_STATEMENT_STEP_DONE
    );

    database_statement_finalize(statement);
    database_close(database);
}

/**
 * @brief Vérifie le refus d'une requête absente.
 */
static void test_reset_invalid_statement(void)
{
    assert(
        !database_statement_reset(NULL)
    );
}

/**
 * @brief Vérifie l'effacement des paramètres d'une requête préparée.
 */
static void test_clear_bindings(void)
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

    assert(
        database_statement_bind_text(
            statement,
            1,
            "Première valeur"
        )
    );

    assert(
        database_statement_clear_bindings(
            statement
        )
    );

    /*
     * Le paramètre peut recevoir une nouvelle valeur après effacement.
     */
    assert(
        database_statement_bind_text(
            statement,
            1,
            "Nouvelle valeur"
        )
    );

    assert(
        database_statement_step(statement) ==
        DATABASE_STATEMENT_STEP_ROW
    );

    assert(
        database_statement_step(statement) ==
        DATABASE_STATEMENT_STEP_DONE
    );

    database_statement_finalize(statement);
    database_close(database);
}

/**
 * @brief Vérifie le refus d'une requête absente.
 */
static void test_clear_bindings_invalid_statement(void)
{
    assert(
        !database_statement_clear_bindings(NULL)
    );
}

/**
 * @brief Vérifie la détection des colonnes SQL NULL.
 */
static void test_column_is_null(void)
{
    Database *database = NULL;
    DatabaseStatement *statement = NULL;
    bool is_null = false;

    database = database_open(":memory:");

    assert(database != NULL);

    statement = database_statement_prepare(
        database,
        "SELECT NULL, 'Labfy Investigation';"
    );

    assert(statement != NULL);

    assert(
        database_statement_step(statement) ==
        DATABASE_STATEMENT_STEP_ROW
    );

    assert(
        database_statement_column_is_null(
            statement,
            0,
            &is_null
        )
    );

    assert(is_null);

    assert(
        database_statement_column_is_null(
            statement,
            1,
            &is_null
        )
    );

    assert(!is_null);

    assert(
        !database_statement_column_is_null(
            NULL,
            0,
            &is_null
        )
    );

    assert(
        !database_statement_column_is_null(
            statement,
            -1,
            &is_null
        )
    );

    assert(
        !database_statement_column_is_null(
            statement,
            2,
            &is_null
        )
    );

    assert(
        !database_statement_column_is_null(
            statement,
            0,
            NULL
        )
    );

    database_statement_finalize(statement);
    database_close(database);
}

/**
 * @brief Vérifie la lecture sécurisée des colonnes texte.
 */
static void test_column_text(void)
{
    Database *database = NULL;
    DatabaseStatement *statement = NULL;
    char *value = NULL;

    database = database_open(":memory:");

    assert(database != NULL);

    statement = database_statement_prepare(
        database,
        "SELECT 'Labfy Investigation', NULL;"
    );

    assert(statement != NULL);

    assert(
        database_statement_step(statement) ==
        DATABASE_STATEMENT_STEP_ROW
    );

    assert(
        database_statement_column_text(
            statement,
            0,
            &value
        )
    );

    assert(value != NULL);
    assert(
        g_strcmp0(
            value,
            "Labfy Investigation"
        ) == 0
    );

    g_free(value);
    value = NULL;

    /*
     * Une colonne SQL NULL produit un pointeur NULL,
     * mais la lecture reste valide.
     */
    assert(
        database_statement_column_text(
            statement,
            1,
            &value
        )
    );

    assert(value == NULL);

    assert(
        !database_statement_column_text(
            NULL,
            0,
            &value
        )
    );

    assert(
        !database_statement_column_text(
            statement,
            -1,
            &value
        )
    );

    assert(
        !database_statement_column_text(
            statement,
            2,
            &value
        )
    );

    assert(
        !database_statement_column_text(
            statement,
            0,
            NULL
        )
    );

    database_statement_finalize(statement);
    database_close(database);
}

/**
 * @brief Vérifie la lecture stricte des colonnes entières.
 */
static void test_column_int64(void)
{
    Database *database = NULL;
    DatabaseStatement *statement = NULL;
    int64_t value = INT64_C(0);

    database = database_open(":memory:");

    assert(database != NULL);

    statement = database_statement_prepare(
        database,
        "SELECT 42, NULL, '42';"
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
            &value
        )
    );

    assert(value == INT64_C(42));

    /*
     * SQL NULL n'est pas un entier valide.
     */
    assert(
        !database_statement_column_int64(
            statement,
            1,
            &value
        )
    );

    /*
     * Le texte '42' n'est pas accepté comme entier.
     */
    assert(
        !database_statement_column_int64(
            statement,
            2,
            &value
        )
    );

    assert(
        !database_statement_column_int64(
            NULL,
            0,
            &value
        )
    );

    assert(
        !database_statement_column_int64(
            statement,
            -1,
            &value
        )
    );

    assert(
        !database_statement_column_int64(
            statement,
            3,
            &value
        )
    );

    assert(
        !database_statement_column_int64(
            statement,
            0,
            NULL
        )
    );

    database_statement_finalize(statement);
    database_close(database);
}

int main(void)
{
    test_prepare_valid_statement();
    test_prepare_invalid_statement();
    test_invalid_parameters();
    test_bind_valid_text();
    test_bind_invalid_text();
    test_step_select_statement();
    test_step_non_query_statement();
    test_step_invalid_statement();
    test_reset_statement();
    test_reset_invalid_statement();
    test_clear_bindings();
    test_clear_bindings_invalid_statement();
    test_column_is_null();
    test_column_text();
    test_column_int64();

    printf(
        "DatabaseStatement : tous les tests sont valides.\n"
    );

    return 0;
}
