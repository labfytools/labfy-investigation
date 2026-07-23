/******************************************************************************
 * @file schema.c
 * @brief Installation des versions du schéma SQLite.
 ******************************************************************************/

#include "database/schema.h"

#include "database_internal.h"
#include "core/relation_type_normalizer.h"

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

bool schema_install_v7(Database *database)
{
    return schema_execute_file(database, "database/schema_v7.sql",
        "la migration SQLite V7");
}

bool schema_install_v8(Database *database)
{
    return schema_execute_file(database, "database/schema_v8.sql",
        "la migration SQLite V8");
}

static gboolean schema_v9_column_exists(sqlite3 *handle)
{
    sqlite3_stmt *statement = NULL;
    gboolean found = FALSE;
    if (sqlite3_prepare_v2(handle, "PRAGMA table_info(relations);", -1,
            &statement, NULL) != SQLITE_OK) return FALSE;
    while (sqlite3_step(statement) == SQLITE_ROW)
        if (g_strcmp0((const char *) sqlite3_column_text(statement, 1),
                "relation_type_id") == 0) found = TRUE;
    sqlite3_finalize(statement);
    return found;
}

static gboolean schema_v9_migrate_relation_types(Database *database)
{
    sqlite3 *handle = database_get_handle(database);
    sqlite3_stmt *read_statement = NULL;
    sqlite3_stmt *find_statement = NULL;
    sqlite3_stmt *insert_statement = NULL;
    sqlite3_stmt *update_statement = NULL;
    gboolean success = FALSE;

    if (!schema_v9_column_exists(handle) &&
        sqlite3_exec(handle, "ALTER TABLE relations ADD COLUMN "
            "relation_type_id INTEGER REFERENCES relation_types(id) "
            "ON UPDATE CASCADE ON DELETE RESTRICT;", NULL, NULL, NULL) != SQLITE_OK)
        return FALSE;
    if (sqlite3_prepare_v2(handle, "SELECT id,type_relation FROM relations "
            "WHERE relation_type_id IS NULL ORDER BY created_at,id;", -1,
            &read_statement, NULL) != SQLITE_OK ||
        sqlite3_prepare_v2(handle, "SELECT id FROM relation_types WHERE "
            "code=?1 OR normalized_key=?2 ORDER BY code IS NULL LIMIT 1;", -1,
            &find_statement, NULL) != SQLITE_OK ||
        sqlite3_prepare_v2(handle, "INSERT INTO relation_types(code,label,"
            "normalized_key,description,is_system) VALUES(NULL,?1,?2,NULL,0);",
            -1, &insert_statement, NULL) != SQLITE_OK ||
        sqlite3_prepare_v2(handle, "UPDATE relations SET relation_type_id=?1 "
            "WHERE id=?2;", -1, &update_statement, NULL) != SQLITE_OK)
        goto cleanup;

    while (sqlite3_step(read_statement) == SQLITE_ROW)
    {
        const char *relation_id =
            (const char *) sqlite3_column_text(read_statement, 0);
        const char *label =
            (const char *) sqlite3_column_text(read_statement, 1);
        char *key = relation_type_normalize_key(label);
        sqlite3_int64 type_id = 0;
        if (key == NULL) goto cleanup;
        sqlite3_bind_text(find_statement, 1, label, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(find_statement, 2, key, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(find_statement) == SQLITE_ROW)
            type_id = sqlite3_column_int64(find_statement, 0);
        sqlite3_reset(find_statement); sqlite3_clear_bindings(find_statement);
        if (type_id == 0)
        {
            sqlite3_bind_text(insert_statement, 1, label, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insert_statement, 2, key, -1, SQLITE_TRANSIENT);
            if (sqlite3_step(insert_statement) != SQLITE_DONE)
            { g_free(key); goto cleanup; }
            type_id = sqlite3_last_insert_rowid(handle);
            sqlite3_reset(insert_statement);
            sqlite3_clear_bindings(insert_statement);
        }
        sqlite3_bind_int64(update_statement, 1, type_id);
        sqlite3_bind_text(update_statement, 2, relation_id, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(update_statement) != SQLITE_DONE)
        { g_free(key); goto cleanup; }
        sqlite3_reset(update_statement); sqlite3_clear_bindings(update_statement);
        g_free(key);
    }
    if (sqlite3_exec(handle,
            "CREATE UNIQUE INDEX IF NOT EXISTS idx_relations_canonical_type "
            "ON relations(entite_source_id,entite_cible_id,relation_type_id);"
            "CREATE INDEX IF NOT EXISTS idx_relations_relation_type_id "
            "ON relations(relation_type_id);"
            "CREATE TRIGGER IF NOT EXISTS relations_require_canonical_type_insert "
            "BEFORE INSERT ON relations WHEN NEW.relation_type_id IS NULL "
            "BEGIN SELECT RAISE(ABORT,'relation_type_id is required'); END;"
            "CREATE TRIGGER IF NOT EXISTS relations_require_canonical_type_update "
            "BEFORE UPDATE OF relation_type_id ON relations "
            "WHEN NEW.relation_type_id IS NULL "
            "BEGIN SELECT RAISE(ABORT,'relation_type_id is required'); END;",
            NULL, NULL, NULL) != SQLITE_OK)
        goto cleanup;
    {
        sqlite3_stmt *check = NULL;
        if (sqlite3_prepare_v2(handle, "PRAGMA foreign_key_check;", -1,
                &check, NULL) != SQLITE_OK) goto cleanup;
        success = sqlite3_step(check) == SQLITE_DONE;
        sqlite3_finalize(check);
    }
cleanup:
    sqlite3_finalize(update_statement); sqlite3_finalize(insert_statement);
    sqlite3_finalize(find_statement); sqlite3_finalize(read_statement);
    if (!success) database_set_error(database, DATABASE_ERROR_SQLITE,
        sqlite3_errmsg(handle));
    return success;
}

bool schema_install_v9(Database *database)
{
    return schema_execute_file(database, "database/schema_v9.sql",
        "la migration SQLite V9") &&
        schema_v9_migrate_relation_types(database);
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
