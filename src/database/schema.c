/******************************************************************************
 * @file schema.c
 * @brief Installation du schéma SQLite de Labfy Investigation.
 ******************************************************************************/

#include "database/schema.h"

#include "database_internal.h"

#include <glib.h>
#include <sqlite3.h>

/**
 * @brief Charge le schéma SQL V1 depuis le fichier du projet.
 *
 * @param database Connexion utilisée pour enregistrer une éventuelle erreur.
 *
 * @return Une nouvelle chaîne terminée par zéro, à libérer avec g_free(),
 *         ou NULL en cas d'échec.
 */
static char *schema_load_v1_sql(
    Database *database
)
{
    char *schema_sql = NULL;
    GError *error = NULL;

    if (!g_file_get_contents(
            "database/schema_v1.sql",
            &schema_sql,
            NULL,
            &error
        ))
    {
        database_set_error(
            database,
            DATABASE_ERROR_INVALID_STATE,
            error != NULL
                ? error->message
                : "Impossible de charger le schéma SQLite V1."
        );

        g_warning(
            "Impossible de charger database/schema_v1.sql : %s",
            error != NULL
                ? error->message
                : "erreur inconnue"
        );

        g_clear_error(&error);

        return NULL;
    }

    return schema_sql;
}

bool schema_install_v1(
    Database *database
)
{
    sqlite3 *database_handle = NULL;
    char *schema_sql = NULL;
    char *error_message = NULL;

    int result = SQLITE_ERROR;

    if (database == NULL)
    {
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

    schema_sql = schema_load_v1_sql(
        database
    );

    if (schema_sql == NULL)
    {
        return false;
    }

    result = sqlite3_exec(
        database_handle,
        schema_sql,
        NULL,
        NULL,
        &error_message
    );

    g_free(schema_sql);

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
            "Impossible d'installer le schéma SQLite V1 : %s",
            error_message != NULL
                ? error_message
                : sqlite3_errmsg(database_handle)
        );

        sqlite3_free(error_message);

        return false;
    }

    sqlite3_free(error_message);

    database_clear_error_internal(
        database
    );

    return true;
}
