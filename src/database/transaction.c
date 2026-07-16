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

    if (database == NULL ||
        sql == NULL ||
        sql[0] == '\0')
    {
        return false;
    }

    database_handle = database_get_handle(database);

    if (database_handle == NULL)
    {
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
    if (database == NULL ||
        database_get_transaction_active(database))
    {
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

    return true;
}

bool database_transaction_commit(
    Database *database
)
{
    if (database == NULL ||
        !database_get_transaction_active(database))
    {
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

    return true;
}

bool database_transaction_rollback(
    Database *database
)
{
    if (database == NULL ||
        !database_get_transaction_active(database))
    {
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

    return true;
}
