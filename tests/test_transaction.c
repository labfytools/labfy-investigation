/******************************************************************************
 * @file test_transaction.c
 * @brief Tests du module DatabaseTransaction.
 ******************************************************************************/

#include "database/database.h"
#include "database/statement.h"
#include "database/transaction.h"
#include "database/error.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

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

/**
 * @brief Vérifie l'enregistrement des erreurs de transaction.
 */
static void test_transaction_error_state(void)
{
    Database *database = NULL;
    const char *error_message = NULL;

    database = database_open(":memory:");

    assert(database != NULL);

    /*
     * Aucun commit n'est possible sans transaction active.
     */
    assert(
        !database_transaction_commit(database)
    );

    assert(
        database_error_get_code(database) ==
        DATABASE_ERROR_INVALID_STATE
    );

    error_message = database_error_get_message(
        database
    );

    assert(error_message != NULL);
    assert(error_message[0] != '\0');

    /*
     * Un début de transaction valide efface l'erreur précédente.
     */
    assert(
        database_transaction_begin(database)
    );

    assert(
        database_error_get_code(database) ==
        DATABASE_ERROR_NONE
    );

    /*
     * Une transaction imbriquée est refusée.
     */
    assert(
        !database_transaction_begin(database)
    );

    assert(
        database_error_get_code(database) ==
        DATABASE_ERROR_INVALID_STATE
    );

    database_error_clear(database);

    assert(
        database_transaction_rollback(database)
    );

    assert(
        database_error_get_code(database) ==
        DATABASE_ERROR_NONE
    );

    /*
     * Un second rollback est impossible.
     */
    assert(
        !database_transaction_rollback(database)
    );

    assert(
        database_error_get_code(database) ==
        DATABASE_ERROR_INVALID_STATE
    );

    database_close(database);
}

/**
 * @brief Vérifie l’exposition publique de l’état de transaction.
 */
static void test_transaction_active_state(void)
{
    Database *database = NULL;

    const char *error_message = NULL;

    database =
        database_open(
            ":memory:"
        );

    assert(database != NULL);

    /*
     * Une connexion nouvelle ne possède aucune transaction active.
     */
    assert(
        !database_transaction_is_active(
            database
        )
    );

    /*
     * Une connexion NULL est considérée comme inactive.
     */
    assert(
        !database_transaction_is_active(
            NULL
        )
    );

    /*
     * La consultation de l’état ne doit pas modifier l’erreur courante.
     */
    database_error_set(
        database,
        DATABASE_ERROR_INVALID_ARGUMENT,
        "erreur témoin"
    );

    assert(
        !database_transaction_is_active(
            database
        )
    );

    assert(
        database_error_get_code(
            database
        ) ==
        DATABASE_ERROR_INVALID_ARGUMENT
    );

    error_message =
        database_error_get_message(
            database
        );

    assert(error_message != NULL);

    assert(
        strcmp(
            error_message,
            "erreur témoin"
        ) == 0
    );

    database_error_clear(
        database
    );

    /*
     * L’état devient actif après BEGIN.
     */
    assert(
        database_transaction_begin(
            database
        )
    );

    assert(
        database_transaction_is_active(
            database
        )
    );

    /*
     * Un simple contrôle ne doit pas terminer la transaction.
     */
    assert(
        database_transaction_is_active(
            database
        )
    );

    /*
     * L’état redevient inactif après COMMIT.
     */
    assert(
        database_transaction_commit(
            database
        )
    );

    assert(
        !database_transaction_is_active(
            database
        )
    );

    /*
     * Même vérification avec un ROLLBACK.
     */
    assert(
        database_transaction_begin(
            database
        )
    );

    assert(
        database_transaction_is_active(
            database
        )
    );

    assert(
        database_transaction_rollback(
            database
        )
    );

    assert(
        !database_transaction_is_active(
            database
        )
    );

    database_close(
        database
    );
}

int main(void)
{
    test_transaction_commit();
    test_transaction_rollback();
    test_invalid_transaction_states();
    test_transaction_error_state();
    test_transaction_active_state();

    printf(
        "DatabaseTransaction : tous les tests sont valides.\n"
    );

    return 0;
}
