/******************************************************************************
 * @file error.c
 * @brief Gestion des erreurs de la couche Database.
 ******************************************************************************/

#include "database/error.h"

#include "database_internal.h"
#include <stddef.h>

DatabaseErrorCode database_error_get_code(
    const Database *database
)
{
    return database_get_error_code_internal(database);
}

const char *database_error_get_message(
    const Database *database
)
{
    return database_get_error_message_internal(database);
}

void database_error_set(
    Database *database,
    DatabaseErrorCode error_code,
    const char *error_message
)
{
    database_set_error(
        database,
        error_code,
        error_message
    );
}

void database_error_clear(
    Database *database
)
{
    database_clear_error_internal(database);
}

