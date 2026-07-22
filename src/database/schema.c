/******************************************************************************
 * @file schema.c
 * @brief Installation des versions du schéma SQLite.
 ******************************************************************************/

#include "database/schema.h"

#include "database_internal.h"

#include <glib.h>
#include <sqlite3.h>

/**
 * @brief Charge un fichier SQL du projet.
 *
 * @param database Connexion recevant les erreurs.
 * @param schema_path Chemin du fichier SQL.
 * @param schema_name Nom utilisé dans les diagnostics.
 *
 * @return Nouvelle chaîne SQL, ou NULL.
 */
static char *schema_load_sql(
    Database *database,
    const char *schema_path,
    const char *schema_name
)
{
    char *schema_sql = NULL;
    GError *error = NULL;

    if (database == NULL ||
        schema_path == NULL ||
        schema_path[0] == '\0' ||
        schema_name == NULL ||
        schema_name[0] == '\0')
    {
        return NULL;
    }

    if (!g_file_get_contents(
            schema_path,
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
                : "Impossible de charger le schéma SQLite."
        );

        g_warning(
            "Impossible de charger %s depuis '%s' : %s",
            schema_name,
            schema_path,
            error != NULL
                ? error->message
                : "erreur inconnue"
        );

        g_clear_error(
            &error
        );

        return NULL;
    }

    return schema_sql;
}

/**
 * @brief Exécute le contenu d’un fichier de schéma SQL.
 *
 * La transaction reste sous la responsabilité du code appelant.
 */
static bool schema_execute_file(
    Database *database,
    const char *schema_path,
    const char *schema_name
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

    database_handle =
        database_get_handle(
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

    schema_sql =
        schema_load_sql(
            database,
            schema_path,
            schema_name
        );

    if (schema_sql == NULL)
    {
        return false;
    }

    result =
        sqlite3_exec(
            database_handle,
            schema_sql,
            NULL,
            NULL,
            &error_message
        );

    g_free(
        schema_sql
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
            "Impossible d’installer %s : %s",
            schema_name,
            error_message != NULL
                ? error_message
                : sqlite3_errmsg(database_handle)
        );

        sqlite3_free(
            error_message
        );

        return false;
    }

    sqlite3_free(
        error_message
    );

    database_clear_error_internal(
        database
    );

    return true;
}

bool schema_install_v1(
    Database *database
)
{
    return schema_execute_file(
        database,
        "database/schema_v1.sql",
        "le schéma SQLite V1"
    );
}

bool schema_install_v2(
    Database *database
)
{
    return schema_execute_file(
        database,
        "database/schema_v2.sql",
        "la migration SQLite V2"
    );
}

bool schema_install_v3(
    Database *database
)
{
    return schema_execute_file(
        database,
        "database/schema_v3.sql",
        "la migration SQLite V3"
    );
}

bool schema_install_v4(
    Database *database
)
{
    return schema_execute_file(
        database,
        "database/schema_v4.sql",
        "la migration SQLite V4"
    );
}

bool schema_install_v5(Database *database)
{
    return schema_execute_file(database, "database/schema_v5.sql",
        "la migration SQLite V5");
}

bool schema_install_v6(Database *database)
{
    return schema_execute_file(database, "database/schema_v6.sql",
        "la migration SQLite V6");
}

bool schema_ensure_current(
    Database *database
)
{
    return schema_execute_file(
        database,
        "database/schema_current.sql",
        "les extensions du schéma SQLite courant"
    );
}
