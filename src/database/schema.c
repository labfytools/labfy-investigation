/******************************************************************************
 * @file schema.c
 * @brief Installation du schéma SQLite de Labfy Investigation.
 ******************************************************************************/

#include "database/schema.h"

#include <glib.h>

/**
 * @brief Charge le schéma SQL V1 depuis les ressources intégrées.
 *
 * @return Une nouvelle chaîne terminée par zéro, à libérer avec g_free(),
 *         ou NULL en cas d'échec.
 */
static char *schema_load_v1_sql(void)
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
    sqlite3 *database
)
{
    char *schema_sql = NULL;
    char *error_message = NULL;

    int result = SQLITE_ERROR;

    if (database == NULL)
    {
        return false;
    }

    schema_sql = schema_load_v1_sql();

    if (schema_sql == NULL)
    {
        return false;
    }

    result = sqlite3_exec(
        database,
        schema_sql,
        NULL,
        NULL,
        &error_message
    );

    g_free(schema_sql);

    if (result != SQLITE_OK)
    {
        g_warning(
            "Impossible d'installer le schéma SQLite V1 : %s",
            error_message != NULL
                ? error_message
                : sqlite3_errmsg(database)
        );

        sqlite3_free(error_message);

        return false;
    }

    sqlite3_free(error_message);

    return true;
}
