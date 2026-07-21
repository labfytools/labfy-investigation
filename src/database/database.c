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
#define DATABASE_SCHEMA_VERSION_CURRENT 2

/**
 * @brief Version actuelle sous forme textuelle pour metadata.
 */
#define DATABASE_SCHEMA_VERSION_CURRENT_TEXT "2"

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
            DATABASE_SCHEMA_VERSION_CURRENT_TEXT
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

/**
 * @brief Requête de lecture de la version du schéma.
 */
static const char *const database_select_schema_version_sql =
    "SELECT value "
    "FROM metadata "
    "WHERE key = ?;";

/**
 * @brief Requête de mise à jour de la version du schéma.
 */
static const char *const database_update_schema_version_sql =
    "UPDATE metadata "
    "SET value = ? "
    "WHERE key = ?;";

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

/**
 * @brief Lit et valide la version enregistrée dans metadata.
 */
static bool database_read_schema_version(
    Database *database,
    int *out_schema_version
)
{
    DatabaseStatement *statement = NULL;
    DatabaseStatementStepResult step_result;

    char *version_text = NULL;
    char *end_pointer = NULL;

    gint64 parsed_version = 0;

    bool success = false;

    if (database == NULL ||
        out_schema_version == NULL)
    {
        return false;
    }

    *out_schema_version = 0;

    statement =
        database_statement_prepare(
            database,
            database_select_schema_version_sql
        );

    if (statement == NULL)
    {
        return false;
    }

    if (!database_statement_bind_text(
            statement,
            1,
            "schema_version"
        ))
    {
        goto cleanup;
    }

    step_result =
        database_statement_step(
            statement
        );

    if (step_result == DATABASE_STATEMENT_STEP_ERROR)
    {
        goto cleanup;
    }

    if (step_result == DATABASE_STATEMENT_STEP_DONE)
    {
        database_set_error(
            database,
            DATABASE_ERROR_INVALID_STATE,
            "La version du schéma est absente des métadonnées."
        );

        goto cleanup;
    }

    if (!database_statement_column_text(
            statement,
            0,
            &version_text
        ) ||
        version_text == NULL ||
        version_text[0] == '\0')
    {
        database_set_error(
            database,
            DATABASE_ERROR_INVALID_STATE,
            "La version du schéma est invalide."
        );

        goto cleanup;
    }

    parsed_version =
        g_ascii_strtoll(
            version_text,
            &end_pointer,
            10
        );

    if (end_pointer == version_text ||
        end_pointer == NULL ||
        end_pointer[0] != '\0' ||
        parsed_version < 1 ||
        parsed_version > G_MAXINT)
    {
        database_set_error(
            database,
            DATABASE_ERROR_INVALID_STATE,
            "La version du schéma n’est pas un entier valide."
        );

        goto cleanup;
    }

    /*
     * La clé metadata est primaire, mais on vérifie malgré tout
     * que la requête ne retourne aucune seconde ligne.
     */
    step_result =
        database_statement_step(
            statement
        );

    if (step_result != DATABASE_STATEMENT_STEP_DONE)
    {
        if (step_result == DATABASE_STATEMENT_STEP_ROW)
        {
            database_set_error(
                database,
                DATABASE_ERROR_INVALID_STATE,
                "Plusieurs versions du schéma sont enregistrées."
            );
        }

        goto cleanup;
    }

    *out_schema_version =
        (int) parsed_version;

    database_clear_error_internal(
        database
    );

    success = true;

cleanup:

    g_free(
        version_text
    );

    database_statement_finalize(
        statement
    );

    return success;
}

/**
 * @brief Met à jour la version enregistrée dans metadata.
 */
static bool database_update_schema_version(
    Database *database,
    const char *version_text
)
{
    DatabaseStatement *statement = NULL;
    bool success = false;

    if (database == NULL ||
        version_text == NULL ||
        version_text[0] == '\0')
    {
        return false;
    }

    statement =
        database_statement_prepare(
            database,
            database_update_schema_version_sql
        );

    if (statement == NULL)
    {
        return false;
    }

    success =
        database_statement_bind_text(
            statement,
            1,
            version_text
        ) &&
        database_statement_bind_text(
            statement,
            2,
            "schema_version"
        ) &&
        database_statement_step(
            statement
        ) == DATABASE_STATEMENT_STEP_DONE;

    database_statement_finalize(
        statement
    );

    return success;
}

/**
 * @brief Applique atomiquement la migration du schéma V1 vers V2.
 */
static bool database_migrate_v1_to_v2(
    Database *database
)
{
    bool transaction_started = false;

    if (database == NULL)
    {
        return false;
    }

    if (!database_transaction_begin(
            database
        ))
    {
        return false;
    }

    transaction_started = true;

    if (!schema_install_v2(
            database
        ))
    {
        goto rollback;
    }

    if (!database_update_schema_version(
            database,
            "2"
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

    return true;

rollback:

    if (transaction_started)
    {
        if (!database_transaction_rollback(
                database
            ))
        {
            g_warning(
                "Impossible d’annuler la migration SQLite V1 vers V2."
            );
        }
    }

    return false;
}

/**
 * @brief Garantit atomiquement la présence des extensions du schéma courant.
 */
static bool database_ensure_current_schema(
    Database *database
)
{
    bool transaction_started = false;

    if (database == NULL)
    {
        return false;
    }

    if (!database_transaction_begin(
            database
        ))
    {
        return false;
    }

    transaction_started = true;

    if (!schema_ensure_current(
            database
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

    return true;

rollback:

    if (transaction_started)
    {
        if (!database_transaction_rollback(
                database
            ))
        {
            g_warning(
                "Impossible d’annuler l’installation des extensions "
                "du schéma SQLite courant."
            );
        }
    }

    return false;
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

bool database_migrate_to_latest(
    Database *database
)
{
    int schema_version = 0;

    if (database == NULL)
    {
        return false;
    }

    if (database_get_transaction_active(
            database
        ))
    {
        database_set_error(
            database,
            DATABASE_ERROR_INVALID_STATE,
            "La migration est impossible pendant une transaction active."
        );

        return false;
    }

    if (!database_read_schema_version(
            database,
            &schema_version
        ))
    {
        return false;
    }

    if (schema_version >
        DATABASE_SCHEMA_VERSION_CURRENT)
    {
        database_set_error(
            database,
            DATABASE_ERROR_INVALID_STATE,
            "La base utilise une version de schéma plus récente "
            "que cette version de Labfy Investigation."
        );

        return false;
    }

    while (schema_version <
           DATABASE_SCHEMA_VERSION_CURRENT)
    {
        switch (schema_version)
        {
            case 1:
                if (!database_migrate_v1_to_v2(
                        database
                    ))
                {
                    return false;
                }

                schema_version = 2;
                break;

            default:
                database_set_error(
                    database,
                    DATABASE_ERROR_INVALID_STATE,
                    "La version du schéma ne possède aucune "
                    "migration prise en charge."
                );

                return false;
        }
    }

    if (!database_ensure_current_schema(
            database
        ))
    {
        return false;
    }

    database->schema_version =
        schema_version;

    database_clear_error_internal(
        database
    );

    return true;
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

    /*
     * Une nouvelle base reçoit immédiatement toutes les versions du schéma
     * dans la transaction initiale.
     */
    if (!schema_install_v2(
            database
        ))
    {
        goto rollback;
    }

    if (!schema_ensure_current(
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
