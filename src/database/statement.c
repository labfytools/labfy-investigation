/******************************************************************************
 * @file statement.c
 * @brief Encapsulation des requêtes préparées SQLite.
 ******************************************************************************/

#include "database/statement.h"

#include "database_internal.h"

#include <glib.h>
#include <sqlite3.h>

/**
 * @brief Représentation privée d'une requête préparée.
 */
struct DatabaseStatement
{
    Database *database;
    sqlite3_stmt *handle;
};

DatabaseStatement *database_statement_prepare(
    Database *database,
    const char *sql
)
{
    DatabaseStatement *statement = NULL;
    sqlite3 *database_handle = NULL;
    int result = SQLITE_ERROR;

    if (database == NULL)
    {
        return NULL;
    }

    if (sql == NULL || sql[0] == '\0')
    {
        database_set_error(
            database,
            DATABASE_ERROR_INVALID_ARGUMENT,
            "La requête SQL est absente."
        );

        return NULL;
    }

    database_handle = database_get_handle(database);

    if (database_handle == NULL)
    {
        database_set_error(
            database,
            DATABASE_ERROR_INVALID_STATE,
            "La connexion SQLite est absente."
        );

        return NULL;
    }

    statement = g_new0(DatabaseStatement, 1);

    if (statement == NULL)
    {
        database_set_error(
            database,
            DATABASE_ERROR_MEMORY,
            "Impossible d'allouer la requête préparée."
        );

        return NULL;
    }

    result = sqlite3_prepare_v2(
        database_handle,
        sql,
        -1,
        &statement->handle,
        NULL
    );

    if (result != SQLITE_OK)
    {
        database_set_error(
            database,
            DATABASE_ERROR_SQLITE,
            sqlite3_errmsg(database_handle)
        );

        g_warning(
            "Impossible de préparer la requête SQL : %s",
            sqlite3_errmsg(database_handle)
        );

        g_free(statement);

        return NULL;
    }

    statement->database = database;

    database_clear_error_internal(database);

    return statement;
}

bool database_statement_bind_text(
    DatabaseStatement *statement,
    int index,
    const char *value
)
{
    sqlite3 *database_handle = NULL;
    const char *error_message = NULL;
    int result = SQLITE_ERROR;

    if (statement == NULL)
    {
        return false;
    }

    if (statement->handle == NULL ||
        index <= 0 ||
        value == NULL)
    {
        database_set_error(
            statement->database,
            DATABASE_ERROR_INVALID_ARGUMENT,
            "Paramètres invalides pour la liaison d'un texte."
        );

        return false;
    }

    result = sqlite3_bind_text(
        statement->handle,
        index,
        value,
        -1,
        SQLITE_TRANSIENT
    );

    if (result != SQLITE_OK)
    {
        database_handle = database_get_handle(
            statement->database
        );

        error_message =
            database_handle != NULL
                ? sqlite3_errmsg(database_handle)
                : sqlite3_errstr(result);

        database_set_error(
            statement->database,
            DATABASE_ERROR_SQLITE,
            error_message
        );

        g_warning(
            "Impossible de lier le paramètre texte %d : %s",
            index,
            error_message
        );

        return false;
    }

    database_clear_error_internal(
        statement->database
    );

    return true;
}

bool database_statement_bind_int64(
    DatabaseStatement *statement,
    int index,
    int64_t value
)
{
    sqlite3 *database_handle = NULL;
    const char *error_message = NULL;
    int result = SQLITE_ERROR;

    if (statement == NULL)
    {
        return false;
    }

    if (statement->handle == NULL ||
        index <= 0)
    {
        database_set_error(
            statement->database,
            DATABASE_ERROR_INVALID_ARGUMENT,
            "Paramètres invalides pour la liaison d'un entier."
        );

        return false;
    }

    result = sqlite3_bind_int64(
        statement->handle,
        index,
        value
    );

    if (result != SQLITE_OK)
    {
        database_handle = database_get_handle(
            statement->database
        );

        error_message =
            database_handle != NULL
                ? sqlite3_errmsg(database_handle)
                : sqlite3_errstr(result);

        database_set_error(
            statement->database,
            DATABASE_ERROR_SQLITE,
            error_message
        );

        g_warning(
            "Impossible de lier le paramètre entier %d : %s",
            index,
            error_message
        );

        return false;
    }

    database_clear_error_internal(
        statement->database
    );

    return true;
}

bool database_statement_bind_double(
    DatabaseStatement *statement,
    int index,
    double value
)
{
    sqlite3 *database_handle =
        NULL;

    const char *error_message =
        NULL;

    int result =
        SQLITE_ERROR;

    if (statement == NULL)
    {
        return false;
    }

    if (statement->handle == NULL ||
        index <= 0)
    {
        database_set_error(
            statement->database,
            DATABASE_ERROR_INVALID_ARGUMENT,
            "Paramètres invalides pour la liaison d'un nombre réel."
        );

        return false;
    }

    result =
        sqlite3_bind_double(
            statement->handle,
            index,
            value
        );

    if (result != SQLITE_OK)
    {
        database_handle =
            database_get_handle(
                statement->database
            );

        error_message =
            database_handle != NULL
                ? sqlite3_errmsg(database_handle)
                : sqlite3_errstr(result);

        database_set_error(
            statement->database,
            DATABASE_ERROR_SQLITE,
            error_message
        );

        g_warning(
            "Impossible de lier le paramètre réel %d : %s",
            index,
            error_message
        );

        return false;
    }

    database_clear_error_internal(
        statement->database
    );

    return true;
}

bool database_statement_bind_null(
    DatabaseStatement *statement,
    int index
)
{
    sqlite3 *database_handle = NULL;
    const char *error_message = NULL;
    int result = SQLITE_ERROR;

    if (statement == NULL)
    {
        return false;
    }

    if (statement->handle == NULL ||
        index <= 0)
    {
        database_set_error(
            statement->database,
            DATABASE_ERROR_INVALID_ARGUMENT,
            "Paramètres invalides pour la liaison de NULL."
        );

        return false;
    }

    result = sqlite3_bind_null(
        statement->handle,
        index
    );

    if (result != SQLITE_OK)
    {
        database_handle = database_get_handle(
            statement->database
        );

        error_message =
            database_handle != NULL
                ? sqlite3_errmsg(database_handle)
                : sqlite3_errstr(result);

        database_set_error(
            statement->database,
            DATABASE_ERROR_SQLITE,
            error_message
        );

        g_warning(
            "Impossible de lier le paramètre NULL %d : %s",
            index,
            error_message
        );

        return false;
    }

    database_clear_error_internal(
        statement->database
    );

    return true;
}

DatabaseStatementStepResult database_statement_step(
    DatabaseStatement *statement
)
{
    sqlite3 *database_handle = NULL;
    const char *error_message = NULL;
    int result = SQLITE_ERROR;

    if (statement == NULL)
    {
        return DATABASE_STATEMENT_STEP_ERROR;
    }

    if (statement->handle == NULL)
    {
        database_set_error(
            statement->database,
            DATABASE_ERROR_INVALID_STATE,
            "La requête préparée SQLite est absente."
        );

        return DATABASE_STATEMENT_STEP_ERROR;
    }

    result = sqlite3_step(
        statement->handle
    );

    if (result == SQLITE_ROW)
    {
        database_clear_error_internal(
            statement->database
        );

        return DATABASE_STATEMENT_STEP_ROW;
    }

    if (result == SQLITE_DONE)
    {
        database_clear_error_internal(
            statement->database
        );

        return DATABASE_STATEMENT_STEP_DONE;
    }

    database_handle = database_get_handle(
        statement->database
    );

    error_message =
        database_handle != NULL
            ? sqlite3_errmsg(database_handle)
            : sqlite3_errstr(result);

    database_set_error(
        statement->database,
        DATABASE_ERROR_SQLITE,
        error_message
    );

    g_warning(
        "Impossible d'exécuter la requête SQL : %s",
        error_message
    );

    return DATABASE_STATEMENT_STEP_ERROR;
}

bool database_statement_reset(
    DatabaseStatement *statement
)
{
    sqlite3 *database_handle = NULL;
    int result = SQLITE_ERROR;

    if (statement == NULL ||
        statement->handle == NULL)
    {
        return false;
    }

    result = sqlite3_reset(
        statement->handle
    );

    if (result != SQLITE_OK)
    {
        database_handle = database_get_handle(
            statement->database
        );

        g_warning(
            "Impossible de réinitialiser la requête SQL : %s",
            database_handle != NULL
                ? sqlite3_errmsg(database_handle)
                : sqlite3_errstr(result)
        );

        return false;
    }

    return true;
}

bool database_statement_clear_bindings(
    DatabaseStatement *statement
)
{
    sqlite3 *database_handle = NULL;
    int result = SQLITE_ERROR;

    if (statement == NULL ||
        statement->handle == NULL)
    {
        return false;
    }

    result = sqlite3_clear_bindings(
        statement->handle
    );

    if (result != SQLITE_OK)
    {
        database_handle = database_get_handle(
            statement->database
        );

        g_warning(
            "Impossible d'effacer les paramètres de la requête SQL : %s",
            database_handle != NULL
                ? sqlite3_errmsg(database_handle)
                : sqlite3_errstr(result)
        );

        return false;
    }

    return true;
}

bool database_statement_column_is_null(
    DatabaseStatement *statement,
    int column_index,
    bool *is_null
)
{
    int column_count = 0;

    if (statement == NULL ||
        statement->handle == NULL ||
        is_null == NULL)
    {
        return false;
    }

    column_count = sqlite3_column_count(
        statement->handle
    );

    if (column_index < 0 ||
        column_index >= column_count)
    {
        return false;
    }

    *is_null =
        sqlite3_column_type(
            statement->handle,
            column_index
        ) == SQLITE_NULL;

    return true;
}

bool database_statement_column_double(
    DatabaseStatement *statement,
    int column_index,
    double *value
)
{
    int column_count =
        0;

    int column_type =
        SQLITE_NULL;

    if (statement == NULL ||
        statement->handle == NULL ||
        value == NULL)
    {
        return false;
    }

    column_count =
        sqlite3_column_count(
            statement->handle
        );

    if (column_index < 0 ||
        column_index >= column_count)
    {
        return false;
    }

    column_type =
        sqlite3_column_type(
            statement->handle,
            column_index
        );

    if (column_type != SQLITE_FLOAT &&
        column_type != SQLITE_INTEGER)
    {
        return false;
    }

    *value =
        sqlite3_column_double(
            statement->handle,
            column_index
        );

    return true;
}

bool database_statement_column_text(
    DatabaseStatement *statement,
    int column_index,
    char **value
)
{
    const unsigned char *column_text = NULL;
    int column_count = 0;
    int text_length = 0;

    if (statement == NULL ||
        statement->handle == NULL ||
        value == NULL)
    {
        return false;
    }

    *value = NULL;

    column_count = sqlite3_column_count(
        statement->handle
    );

    if (column_index < 0 ||
        column_index >= column_count)
    {
        return false;
    }

    if (sqlite3_column_type(
            statement->handle,
            column_index
        ) == SQLITE_NULL)
    {
        return true;
    }

    column_text = sqlite3_column_text(
        statement->handle,
        column_index
    );

    if (column_text == NULL)
    {
        return false;
    }

    text_length = sqlite3_column_bytes(
        statement->handle,
        column_index
    );

    *value = g_strndup(
        (const char *) column_text,
        (gsize) text_length
    );

    if (*value == NULL)
    {
        return false;
    }

    return true;
}

bool database_statement_column_int64(
    DatabaseStatement *statement,
    int column_index,
    int64_t *value
)
{
    int column_count = 0;
    int column_type = SQLITE_NULL;

    if (statement == NULL ||
        statement->handle == NULL ||
        value == NULL)
    {
        return false;
    }

    column_count = sqlite3_column_count(
        statement->handle
    );

    if (column_index < 0 ||
        column_index >= column_count)
    {
        return false;
    }

    column_type = sqlite3_column_type(
        statement->handle,
        column_index
    );

    if (column_type != SQLITE_INTEGER)
    {
        return false;
    }

    *value = sqlite3_column_int64(
        statement->handle,
        column_index
    );

    return true;
}

void database_statement_finalize(
    DatabaseStatement *statement
)
{
    if (statement == NULL)
    {
        return;
    }

    if (statement->handle != NULL)
    {
        /*
         * sqlite3_finalize() peut retourner l'erreur produite par le dernier
         * sqlite3_step(), même si la requête est correctement détruite.
         *
         * Cette erreur est déjà enregistrée par database_statement_step().
         */
        sqlite3_finalize(
            statement->handle
        );

        statement->handle = NULL;
    }

    g_free(statement);
}
