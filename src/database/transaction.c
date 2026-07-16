/******************************************************************************
 * @file transaction.c
 * @brief Gestion des transactions SQLite.
 ******************************************************************************/

#include "database/transaction.h"

#include "database_internal.h"

#include <glib.h>
#include <sqlite3.h>

/**
 * @brief Exécute une commande SQL de transaction.
 */
static bool database_transaction_execute(
    Database *database,
    const char *sql
)
{
    sqlite3 *database_handle = NULL;
    char *error_message = NULL;
    int result = SQLITE_ERROR;

    if (database == NULL)
    {
        return false;
    }

    if (sql == NULL || sql[0] == '\0')
    {
        database_set_error(
            database,
            DATABASE_ERROR_INVALID_ARGUMENT,
            "La commande SQL de transaction est absente."
        );

        return false;
    }

    database_handle = database_get_handle(
        database
    );

    if (database_handle == NULL)
    {
        database_set_error(
            database,
            DATABASE_ERROR_INVALID_STATE,
            "La connexion SQLite est absente."
        );

        return false;
    }

    result = sqlite3_exec(
        database_handle,
        sql,
        NULL,
        NULL,
        &error_message
    );

    if (result != SQLITE_OK)
    {
        database_set_error(
            database,
            DATABASE_ERROR_SQLITE,
            error_message != NULL
                ? error_message
                : sqlite3_errmsg(database_handle)
        );

        g_warning(
            "Impossible d'exécuter la transaction SQL : %s",
            error_message != NULL
                ? error_message
                : sqlite3_errmsg(database_handle)
        );

        sqlite3_free(error_message);

        return false;
    }

    sqlite3_free(error_message);

    return true;
}

bool database_transaction_begin(
    Database *database
)
{
    if (database == NULL)
    {
        return false;
    }

    if (database_get_transaction_active(database))
    {
        database_set_error(
            database,
            DATABASE_ERROR_INVALID_STATE,
            "Une transaction est déjà active."
        );

        return false;
    }

    if (!database_transaction_execute(
            database,
            "BEGIN IMMEDIATE;"
        ))
    {
        return false;
    }

    database_set_transaction_active(
        database,
        true
    );

    database_clear_error_internal(
        database
    );

    return true;
}

bool database_transaction_commit(
    Database *database
)
{
    if (database == NULL)
    {
        return false;
    }

    if (!database_get_transaction_active(database))
    {
        database_set_error(
            database,
            DATABASE_ERROR_INVALID_STATE,
            "Aucune transaction active à valider."
        );

        return false;
    }

    if (!database_transaction_execute(
            database,
            "COMMIT;"
        ))
    {
        return false;
    }

    database_set_transaction_active(
        database,
        false
    );

    database_clear_error_internal(
        database
    );

    return true;
}

bool database_transaction_rollback(
    Database *database
)
{
    if (database == NULL)
    {
        return false;
    }

    if (!database_get_transaction_active(database))
    {
        database_set_error(
            database,
            DATABASE_ERROR_INVALID_STATE,
            "Aucune transaction active à annuler."
        );

        return false;
    }

    if (!database_transaction_execute(
            database,
            "ROLLBACK;"
        ))
    {
        return false;
    }

    database_set_transaction_active(
        database,
        false
    );

    /*
     * On n'efface pas automatiquement l'erreur précédente.
     *
     * Un rollback est souvent exécuté à la suite d'une autre erreur.
     * Cette erreur initiale doit rester disponible pour le diagnostic.
     */

    return true;
}
