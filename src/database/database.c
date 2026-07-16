/******************************************************************************
 * @file database.c
 * @brief Initialisation de la base SQLite d'une enquête.
 ******************************************************************************/

#include "database/database.h"
#include "database/schema.h"
#include "database/transaction.h"
#include "database/statement.h"

#include "database_internal.h"

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

struct Database
{
    sqlite3 *handle;
    char *database_path;

    bool transaction_active;
    int schema_version;

    DatabaseErrorCode error_code;
    char *error_message;
};

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
 * @brief Insère une paire clé-valeur avec une requête préparée.
 *
 * La requête est réinitialisée après chaque insertion afin de pouvoir
 * être réutilisée.
 */
static bool database_insert_metadata(
    DatabaseStatement *statement,
    const char *key,
    const char *value
)
{
    if (statement == NULL ||
        key == NULL ||
        value == NULL)
    {
        return false;
    }

    if (!database_statement_bind_text(
            statement,
            1,
            key
        ))
    {
        return false;
    }

    if (!database_statement_bind_text(
            statement,
            2,
            value
        ))
    {
        return false;
    }

    if (database_statement_step(statement) !=
        DATABASE_STATEMENT_STEP_DONE)
    {
        return false;
    }

    if (!database_statement_reset(statement))
    {
        return false;
    }

    if (!database_statement_clear_bindings(statement))
    {
        return false;
    }

    return true;
}

/**
 * @brief Insère toutes les métadonnées obligatoires.
 */
static bool database_insert_all_metadata(
    Database *database,
    const char *created_at,
    const char *investigation_uuid
)
{
    DatabaseStatement *statement = NULL;
    bool success = false;

    if (database == NULL ||
        created_at == NULL ||
        investigation_uuid == NULL)
    {
        return false;
    }

    statement = database_statement_prepare(
        database,
        database_insert_metadata_sql
    );

    if (statement == NULL)
    {
        return false;
    }

    success =
        database_insert_metadata(
            statement,
            "schema_version",
            DATABASE_SCHEMA_VERSION
        ) &&
        database_insert_metadata(
            statement,
            "application",
            DATABASE_APPLICATION_NAME
        ) &&
        database_insert_metadata(
            statement,
            "created_at",
            created_at
        ) &&
        database_insert_metadata(
            statement,
            "investigation_uuid",
            investigation_uuid
        );

    database_statement_finalize(statement);

    return success;
}

/**
 * @brief Insère la ligne représentant l'enquête courante.
 */
static bool database_insert_investigation(
    Database *database,
    const char *investigation_uuid,
    const char *investigation_name,
    const char *investigation_root_path,
    const char *created_at
)
{
    DatabaseStatement *statement = NULL;
    bool success = false;

    if (database == NULL ||
        investigation_uuid == NULL ||
        investigation_name == NULL ||
        investigation_root_path == NULL ||
        created_at == NULL)
    {
        return false;
    }

    statement = database_statement_prepare(
        database,
        database_insert_investigation_sql
    );

    if (statement == NULL)
    {
        return false;
    }

    success =
        database_statement_bind_text(
            statement,
            1,
            investigation_uuid
        ) &&
        database_statement_bind_text(
            statement,
            2,
            investigation_name
        ) &&
        database_statement_bind_text(
            statement,
            3,
            investigation_root_path
        ) &&
        database_statement_bind_text(
            statement,
            4,
            created_at
        ) &&
        database_statement_bind_text(
            statement,
            5,
            created_at
        ) &&
        database_statement_step(statement) ==
            DATABASE_STATEMENT_STEP_DONE;

    database_statement_finalize(statement);

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

sqlite3 *database_get_handle(
    Database *database
)
{
    if (database == NULL)
    {
        return NULL;
    }

    return database->handle;
}

bool database_get_transaction_active(
    const Database *database
)
{
    if (database == NULL)
    {
        return false;
    }

    return database->transaction_active;
}

void database_set_transaction_active(
    Database *database,
    bool transaction_active
)
{
    if (database == NULL)
    {
        return;
    }

    database->transaction_active = transaction_active;
}

Database *database_open(
    const char *database_path
)
{
    Database *database = NULL;
    int result = SQLITE_ERROR;

    if (database_path == NULL || database_path[0] == '\0')
    {
        return NULL;
    }

    database = g_new0(Database, 1);

    if (database == NULL)
    {
        return NULL;
    }

    database->database_path = g_strdup(database_path);

    if (database->database_path == NULL)
    {
        g_free(database);
        return NULL;
    }

    result = sqlite3_open_v2(
        database_path,
        &database->handle,
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
            database->handle != NULL
                ? sqlite3_errmsg(database->handle)
                : sqlite3_errstr(result)
        );

        database_close(database);

        return NULL;
    }

    if (!database_execute_sql(
            database->handle,
            "PRAGMA foreign_keys = ON;"
        ))
    {
        database_close(database);
        return NULL;
    }

    database->transaction_active = false;
    database->schema_version = 0;

    return database;
}

bool database_initialize(
    const char *database_path,
    const char *investigation_name,
    const char *investigation_root_path
)
{
    Database *database = NULL;

    char *created_at = NULL;
    char *investigation_uuid = NULL;

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
        goto cleanup;
    }

    database = database_open(
        database_path
    );

    if (database == NULL)
    {
        goto cleanup;
    }

    /*
     * Accès temporaire au handle SQLite.
     *
     * Il reste nécessaire tant que schema_install_v1() et les anciennes
     * fonctions d'insertion n'ont pas encore été migrées vers Database.
     */

    if (!database_transaction_begin(
            database
        ))
    {
        goto cleanup;
    }

    transaction_started = true;

    if (!schema_install_v1(
            database
        ))
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

    if (!database_transaction_commit(
            database
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
        if (!database_transaction_rollback(
                database
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

    database_close(database);

    g_free(investigation_uuid);
    g_free(created_at);

    return success;
}

void database_set_error(
    Database *database,
    DatabaseErrorCode error_code,
    const char *error_message
)
{
    char *error_message_copy = NULL;

    if (database == NULL)
    {
        return;
    }

    if (error_message != NULL)
    {
        error_message_copy = g_strdup(error_message);
    }

    g_free(database->error_message);

    database->error_code = error_code;
    database->error_message = error_message_copy;
}

DatabaseErrorCode database_get_error_code_internal(
    const Database *database
)
{
    if (database == NULL)
    {
        return DATABASE_ERROR_NONE;
    }

    return database->error_code;
}

const char *database_get_error_message_internal(
    const Database *database
)
{
    if (database == NULL)
    {
        return NULL;
    }

    return database->error_message;
}

void database_clear_error_internal(
    Database *database
)
{
    if (database == NULL)
    {
        return;
    }

    g_free(database->error_message);

    database->error_message = NULL;
    database->error_code = DATABASE_ERROR_NONE;
}

void database_close(
    Database *database
)
{
    int result = SQLITE_OK;

    if (database == NULL)
    {
        return;
    }

    if (database->handle != NULL)
    {
        result = sqlite3_close(database->handle);

        if (result != SQLITE_OK)
        {
            g_warning(
                "Impossible de fermer proprement la base '%s' : %s",
                database->database_path != NULL
                    ? database->database_path
                    : "(chemin inconnu)",
                sqlite3_errstr(result)
            );
        }
    }

    g_free(database->error_message);
    g_free(database->database_path);
    g_free(database);
}
