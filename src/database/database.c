/******************************************************************************
 * @file database.c
 * @brief Initialisation de la base SQLite d'une enquête.
 ******************************************************************************/

#include "database/database.h"
#include "database/schema.h"

#include <glib.h>
#include <sqlite3.h>

/**
 * @brief Version actuelle du schéma SQLite.
 */
#define DATABASE_SCHEMA_VERSION "1"

/**
 * @brief Nom de l'application enregistré dans les métadonnées.
 */
#define DATABASE_APPLICATION_NAME "Labfy Investigation"

/**
 * @brief Requête d'insertion d'une métadonnée.
 */
static const char *const database_insert_metadata_sql =
    "INSERT INTO metadata (key, value) VALUES (?, ?);";

/**
 * @brief Requête d'insertion de l'enquête.
 */
static const char *const database_insert_investigation_sql =
    "INSERT INTO investigation"
    "("
    "    id,"
    "    name,"
    "    root_path,"
    "    created_at,"
    "    updated_at"
    ")"
    "VALUES"
    "("
    "    ?,"
    "    ?,"
    "    ?,"
    "    ?,"
    "    ?"
    ");";

/**
 * @brief Vérifie les paramètres publics de l'initialisation.
 */
static bool database_validate_initialize_parameters(
    const char *database_path,
    const char *investigation_name,
    const char *investigation_root_path
)
{
    if (database_path == NULL || database_path[0] == '\0')
    {
        return false;
    }

    if (investigation_name == NULL || investigation_name[0] == '\0')
    {
        return false;
    }

    if (investigation_root_path == NULL ||
        investigation_root_path[0] == '\0')
    {
        return false;
    }

    return true;
}

/**
 * @brief Exécute une requête SQL statique.
 *
 * Cette fonction convient uniquement aux requêtes qui ne contiennent
 * aucune donnée provenant de l'utilisateur.
 */
static bool database_execute_sql(
    sqlite3 *database,
    const char *sql
)
{
    char *error_message = NULL;
    int result = SQLITE_ERROR;

    if (database == NULL || sql == NULL)
    {
        return false;
    }

    result = sqlite3_exec(
        database,
        sql,
        NULL,
        NULL,
        &error_message
    );

    if (result != SQLITE_OK)
    {
        g_warning(
            "Erreur SQLite : %s",
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

/**
 * @brief Insère une paire clé-valeur dans la table metadata.
 */
static bool database_insert_metadata(
    sqlite3 *database,
    sqlite3_stmt *statement,
    const char *key,
    const char *value
)
{
    int result = SQLITE_ERROR;

    if (database == NULL ||
        statement == NULL ||
        key == NULL ||
        value == NULL)
    {
        return false;
    }

    result = sqlite3_bind_text(
        statement,
        1,
        key,
        -1,
        SQLITE_TRANSIENT
    );

    if (result != SQLITE_OK)
    {
        g_warning(
            "Impossible de lier la clé '%s' : %s",
            key,
            sqlite3_errmsg(database)
        );

        return false;
    }

    result = sqlite3_bind_text(
        statement,
        2,
        value,
        -1,
        SQLITE_TRANSIENT
    );

    if (result != SQLITE_OK)
    {
        g_warning(
            "Impossible de lier la valeur de '%s' : %s",
            key,
            sqlite3_errmsg(database)
        );

        return false;
    }

    result = sqlite3_step(statement);

    if (result != SQLITE_DONE)
    {
        g_warning(
            "Impossible d'insérer la métadonnée '%s' : %s",
            key,
            sqlite3_errmsg(database)
        );

        return false;
    }

    result = sqlite3_reset(statement);

    if (result != SQLITE_OK)
    {
        g_warning(
            "Impossible de réinitialiser la requête metadata : %s",
            sqlite3_errmsg(database)
        );

        return false;
    }

    result = sqlite3_clear_bindings(statement);

    if (result != SQLITE_OK)
    {
        g_warning(
            "Impossible d'effacer les paramètres metadata : %s",
            sqlite3_errmsg(database)
        );

        return false;
    }

    return true;
}

/**
 * @brief Insère toutes les métadonnées obligatoires.
 */
static bool database_insert_all_metadata(
    sqlite3 *database,
    const char *created_at,
    const char *investigation_uuid
)
{
    sqlite3_stmt *statement = NULL;
    int result = SQLITE_ERROR;
    bool success = false;

    if (database == NULL ||
        created_at == NULL ||
        investigation_uuid == NULL)
    {
        return false;
    }

    result = sqlite3_prepare_v2(
        database,
        database_insert_metadata_sql,
        -1,
        &statement,
        NULL
    );

    if (result != SQLITE_OK)
    {
        g_warning(
            "Impossible de préparer l'insertion des métadonnées : %s",
            sqlite3_errmsg(database)
        );

        return false;
    }

    success =
        database_insert_metadata(
            database,
            statement,
            "schema_version",
            DATABASE_SCHEMA_VERSION
        ) &&
        database_insert_metadata(
            database,
            statement,
            "application",
            DATABASE_APPLICATION_NAME
        ) &&
        database_insert_metadata(
            database,
            statement,
            "created_at",
            created_at
        ) &&
        database_insert_metadata(
            database,
            statement,
            "investigation_uuid",
            investigation_uuid
        );

    if (sqlite3_finalize(statement) != SQLITE_OK)
    {
        g_warning(
            "Impossible de finaliser la requête metadata : %s",
            sqlite3_errmsg(database)
        );

        return false;
    }

    return success;
}

/**
 * @brief Insère la ligne représentant l'enquête courante.
 */
static bool database_insert_investigation(
    sqlite3 *database,
    const char *investigation_uuid,
    const char *investigation_name,
    const char *investigation_root_path,
    const char *created_at
)
{
    sqlite3_stmt *statement = NULL;
    int result = SQLITE_ERROR;
    bool success = false;

    if (database == NULL ||
        investigation_uuid == NULL ||
        investigation_name == NULL ||
        investigation_root_path == NULL ||
        created_at == NULL)
    {
        return false;
    }

    result = sqlite3_prepare_v2(
        database,
        database_insert_investigation_sql,
        -1,
        &statement,
        NULL
    );

    if (result != SQLITE_OK)
    {
        g_warning(
            "Impossible de préparer l'insertion de l'enquête : %s",
            sqlite3_errmsg(database)
        );

        return false;
    }

    result = sqlite3_bind_text(
        statement,
        1,
        investigation_uuid,
        -1,
        SQLITE_TRANSIENT
    );

    if (result != SQLITE_OK)
    {
        goto cleanup;
    }

    result = sqlite3_bind_text(
        statement,
        2,
        investigation_name,
        -1,
        SQLITE_TRANSIENT
    );

    if (result != SQLITE_OK)
    {
        goto cleanup;
    }

    result = sqlite3_bind_text(
        statement,
        3,
        investigation_root_path,
        -1,
        SQLITE_TRANSIENT
    );

    if (result != SQLITE_OK)
    {
        goto cleanup;
    }

    result = sqlite3_bind_text(
        statement,
        4,
        created_at,
        -1,
        SQLITE_TRANSIENT
    );

    if (result != SQLITE_OK)
    {
        goto cleanup;
    }

    result = sqlite3_bind_text(
        statement,
        5,
        created_at,
        -1,
        SQLITE_TRANSIENT
    );

    if (result != SQLITE_OK)
    {
        goto cleanup;
    }

    result = sqlite3_step(statement);

    if (result != SQLITE_DONE)
    {
        goto cleanup;
    }

    success = true;

cleanup:

    if (!success)
    {
        g_warning(
            "Impossible d'insérer l'enquête : %s",
            sqlite3_errmsg(database)
        );
    }

    if (sqlite3_finalize(statement) != SQLITE_OK)
    {
        g_warning(
            "Impossible de finaliser la requête investigation : %s",
            sqlite3_errmsg(database)
        );

        success = false;
    }

    return success;
}

/**
 * @brief Produit une date UTC au format ISO 8601 attendu.
 *
 * @return Une nouvelle chaîne à libérer avec g_free(), ou NULL.
 */
static char *database_create_utc_timestamp(void)
{
    GDateTime *date_time = NULL;
    char *timestamp = NULL;

    date_time = g_date_time_new_now_utc();

    if (date_time == NULL)
    {
        return NULL;
    }

    timestamp = g_date_time_format(
        date_time,
        "%Y-%m-%dT%H:%M:%SZ"
    );

    g_date_time_unref(date_time);

    return timestamp;
}

bool database_initialize(
    const char *database_path,
    const char *investigation_name,
    const char *investigation_root_path
)
{
    sqlite3 *database = NULL;
    char *created_at = NULL;
    char *investigation_uuid = NULL;

    int result = SQLITE_ERROR;
    bool transaction_started = false;
    bool success = false;

    if (!database_validate_initialize_parameters(
            database_path,
            investigation_name,
            investigation_root_path
        ))
    {
        return false;
    }

    created_at = database_create_utc_timestamp();

    if (created_at == NULL)
    {
        return false;
    }

    investigation_uuid = g_uuid_string_random();

    if (investigation_uuid == NULL)
    {
        g_free(created_at);
        return false;
    }

    result = sqlite3_open_v2(
        database_path,
        &database,
        SQLITE_OPEN_READWRITE |
        SQLITE_OPEN_CREATE |
        SQLITE_OPEN_PRIVATECACHE,
        NULL
    );

    if (result != SQLITE_OK)
    {
        g_warning(
            "Impossible d'ouvrir la base '%s' : %s",
            database_path,
            database != NULL
                ? sqlite3_errmsg(database)
                : sqlite3_errstr(result)
        );

        goto cleanup;
    }

    if (!database_execute_sql(
            database,
            "PRAGMA foreign_keys = ON;"
        ))
    {
        goto cleanup;
    }

    if (!database_execute_sql(
            database,
            "BEGIN IMMEDIATE;"
        ))
    {
        goto cleanup;
    }

    transaction_started = true;

    if (!schema_install_v1(database))
    {
        goto rollback;
    }
    
    if (!database_insert_all_metadata(
            database,
            created_at,
            investigation_uuid
        ))
    {
        goto rollback;
    }

    if (!database_insert_investigation(
            database,
            investigation_uuid,
            investigation_name,
            investigation_root_path,
            created_at
        ))
    {
        goto rollback;
    }

    if (!database_execute_sql(
            database,
            "COMMIT;"
        ))
    {
        goto rollback;
    }

    transaction_started = false;
    success = true;

    goto cleanup;

rollback:

    if (transaction_started)
    {
        if (!database_execute_sql(
                database,
                "ROLLBACK;"
            ))
        {
            g_warning(
                "Échec du rollback de la base '%s'.",
                database_path
            );
        }

        transaction_started = false;
    }

cleanup:

    if (database != NULL)
    {
        result = sqlite3_close(database);

        if (result != SQLITE_OK)
        {
            g_warning(
                "Impossible de fermer proprement la base '%s' : %s",
                database_path,
                sqlite3_errstr(result)
            );

            success = false;
        }
    }

    g_free(investigation_uuid);
    g_free(created_at);

    return success;
}
