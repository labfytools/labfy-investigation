/******************************************************************************
 * @file test_database.c
 * @brief Tests d'intégration du module Database.
 ******************************************************************************/

#include "database/database.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <sqlite3.h>

/**
 * @brief Lit une valeur unique retournée par une requête SQL.
 *
 * @param database Base SQLite ouverte.
 * @param sql      Requête retournant une seule colonne et une seule ligne.
 *
 * @return Une nouvelle chaîne à libérer avec g_free(), ou NULL.
 */
static char *test_database_read_single_text(
    sqlite3 *database,
    const char *sql
)
{
    sqlite3_stmt *statement = NULL;
    const unsigned char *text = NULL;
    char *result_text = NULL;
    int result = SQLITE_ERROR;

    assert(database != NULL);
    assert(sql != NULL);

    result = sqlite3_prepare_v2(
        database,
        sql,
        -1,
        &statement,
        NULL
    );

    assert(result == SQLITE_OK);
    assert(statement != NULL);

    result = sqlite3_step(statement);

    assert(result == SQLITE_ROW);

    text = sqlite3_column_text(
        statement,
        0
    );

    assert(text != NULL);

    result_text = g_strdup(
        (const char *)text
    );

    assert(result_text != NULL);

    result = sqlite3_finalize(statement);

    assert(result == SQLITE_OK);

    return result_text;
}

/**
 * @brief Vérifie qu'une table existe.
 */
static void test_database_assert_table_exists(
    sqlite3 *database,
    const char *table_name
)
{
    sqlite3_stmt *statement = NULL;
    int result = SQLITE_ERROR;

    assert(database != NULL);
    assert(table_name != NULL);

    result = sqlite3_prepare_v2(
        database,
        "SELECT COUNT(*) "
        "FROM sqlite_master "
        "WHERE type = 'table' AND name = ?;",
        -1,
        &statement,
        NULL
    );

    assert(result == SQLITE_OK);
    assert(statement != NULL);

    result = sqlite3_bind_text(
        statement,
        1,
        table_name,
        -1,
        SQLITE_TRANSIENT
    );

    assert(result == SQLITE_OK);

    result = sqlite3_step(statement);

    assert(result == SQLITE_ROW);
    assert(sqlite3_column_int(statement, 0) == 1);

    result = sqlite3_finalize(statement);

    assert(result == SQLITE_OK);
}

/**
 * @brief Vérifie l'initialisation complète d'une base.
 */
static void test_database_initialize_valid_database(void)
{
    char *temporary_directory = NULL;
    char *database_path = NULL;

    char *schema_version = NULL;
    char *application_name = NULL;
    char *created_at = NULL;
    char *investigation_uuid = NULL;
    char *investigation_name = NULL;
    char *investigation_root_path = NULL;
    char *investigation_created_at = NULL;
    char *investigation_id = NULL;
    char *investigation_count = NULL;
    char *investigation_updated_at = NULL;

    sqlite3 *database = NULL;
    GError *error = NULL;
    int result = SQLITE_ERROR;

    temporary_directory = g_dir_make_tmp(
        "labfy-database-test-XXXXXX",
        &error
    );

    assert(temporary_directory != NULL);
    assert(error == NULL);

    database_path = g_build_filename(
        temporary_directory,
        "Enquete.sqlite",
        NULL
    );

    assert(database_path != NULL);

    assert(
        database_initialize(
            database_path,
            "Enquete_Test",
            temporary_directory
        )
    );

    assert(
        g_file_test(
            database_path,
            G_FILE_TEST_IS_REGULAR
        )
    );

    result = sqlite3_open_v2(
        database_path,
        &database,
        SQLITE_OPEN_READONLY,
        NULL
    );

    assert(result == SQLITE_OK);
    assert(database != NULL);

    test_database_assert_table_exists(
        database,
        "metadata"
    );

    test_database_assert_table_exists(
        database,
        "investigation"
    );

    schema_version = test_database_read_single_text(
        database,
        "SELECT value FROM metadata "
        "WHERE key = 'schema_version';"
    );

    application_name = test_database_read_single_text(
        database,
        "SELECT value FROM metadata "
        "WHERE key = 'application';"
    );

    created_at = test_database_read_single_text(
        database,
        "SELECT value FROM metadata "
        "WHERE key = 'created_at';"
    );

    investigation_uuid = test_database_read_single_text(
        database,
        "SELECT value FROM metadata "
        "WHERE key = 'investigation_uuid';"
    );

    investigation_name = test_database_read_single_text(
        database,
        "SELECT name FROM investigation "
        "LIMIT 1;"
    );

    investigation_root_path = test_database_read_single_text(
        database,
        "SELECT root_path FROM investigation "
        "LIMIT 1;"
    );

    investigation_created_at = test_database_read_single_text(
        database,
        "SELECT created_at FROM investigation "
        "LIMIT 1;"
    );

    investigation_updated_at = test_database_read_single_text(
        database,
        "SELECT updated_at FROM investigation "
        "LIMIT 1;"
    );

    investigation_id = test_database_read_single_text(
        database,
        "SELECT id FROM investigation "
        "LIMIT 1;"
    );

    investigation_count = test_database_read_single_text(
        database,
        "SELECT CAST(COUNT(*) AS TEXT) "
        "FROM investigation;"
    );

    assert(strcmp(schema_version, "1") == 0);
    assert(strcmp(application_name, "Labfy Investigation") == 0);

    assert(created_at[0] != '\0');
    assert(investigation_uuid[0] != '\0');

    assert(strcmp(investigation_name, "Enquete_Test") == 0);
    assert(strcmp(investigation_root_path, temporary_directory) == 0);
    assert(strcmp(investigation_created_at, created_at) == 0);

    assert(investigation_updated_at[0] != '\0');

    assert(strcmp(investigation_updated_at,created_at) == 0);

    assert(strcmp(investigation_id,investigation_uuid) == 0
    );

    result = sqlite3_close(database);

    assert(result == SQLITE_OK);

    assert(g_remove(database_path) == 0);
    assert(g_rmdir(temporary_directory) == 0);

    assert(investigation_id[0] != '\0');
    assert(g_uuid_string_is_valid(investigation_id));

    assert(strcmp(investigation_count, "1") == 0);

    g_free(investigation_created_at);
    g_free(investigation_root_path);
    g_free(investigation_name);
    g_free(investigation_uuid);
    g_free(created_at);
    g_free(application_name);
    g_free(schema_version);
    g_free(database_path);
    g_free(temporary_directory);
    g_free(investigation_id);
    g_free(investigation_count);
}

/**
 * @brief Vérifie le refus des paramètres invalides.
 */
static void test_database_initialize_invalid_parameters(void)
{
    assert(
        !database_initialize(
            NULL,
            "Enquete",
            "/tmp/Enquete"
        )
    );

    assert(
        !database_initialize(
            "",
            "Enquete",
            "/tmp/Enquete"
        )
    );

    assert(
        !database_initialize(
            "/tmp/Enquete.sqlite",
            NULL,
            "/tmp/Enquete"
        )
    );

    assert(
        !database_initialize(
            "/tmp/Enquete.sqlite",
            "",
            "/tmp/Enquete"
        )
    );

    assert(
        !database_initialize(
            "/tmp/Enquete.sqlite",
            "Enquete",
            NULL
        )
    );

    assert(
        !database_initialize(
            "/tmp/Enquete.sqlite",
            "Enquete",
            ""
        )
    );
}

/**
 * @brief Vérifie l'échec sur un chemin dont le parent n'existe pas.
 */
static void test_database_initialize_missing_parent(void)
{
    assert(
        !database_initialize(
            "/tmp/labfy-missing-parent/database/Enquete.sqlite",
            "Enquete",
            "/tmp/labfy-missing-parent"
        )
    );
}

/**
 * @brief Vérifie qu'une seconde initialisation échouée est annulée.
 *
 * La première initialisation crée une base valide.
 * La seconde tentative doit échouer sans modifier les données existantes.
 */
static void test_database_initialize_rollback(void)
{
    char *temporary_directory = NULL;
    char *database_path = NULL;

    char *investigation_count = NULL;
    char *metadata_count = NULL;
    char *investigation_name = NULL;
    char *investigation_root_path = NULL;
    char *integrity_result = NULL;

    sqlite3 *database = NULL;
    GError *error = NULL;
    int result = SQLITE_ERROR;

    temporary_directory = g_dir_make_tmp(
        "labfy-database-rollback-test-XXXXXX",
        &error
    );

    assert(temporary_directory != NULL);
    assert(error == NULL);

    database_path = g_build_filename(
        temporary_directory,
        "Enquete.sqlite",
        NULL
    );

    assert(database_path != NULL);

    /*
     * Première initialisation valide.
     */
    assert(
        database_initialize(
            database_path,
            "Enquete_Initiale",
            temporary_directory
        )
    );

    /*
     * La seconde initialisation doit échouer.
     *
     * Elle provoquera une erreur pendant l'installation du schéma
     * ou lors de l'insertion des métadonnées déjà existantes.
     */
    assert(
        !database_initialize(
            database_path,
            "Enquete_Seconde",
            "/tmp/racine-seconde"
        )
    );

    result = sqlite3_open_v2(
        database_path,
        &database,
        SQLITE_OPEN_READONLY,
        NULL
    );

    assert(result == SQLITE_OK);
    assert(database != NULL);

    investigation_count = test_database_read_single_text(
        database,
        "SELECT CAST(COUNT(*) AS TEXT) "
        "FROM investigation;"
    );

    metadata_count = test_database_read_single_text(
        database,
        "SELECT CAST(COUNT(*) AS TEXT) "
        "FROM metadata "
        "WHERE key IN "
        "("
        "    'schema_version',"
        "    'application',"
        "    'created_at',"
        "    'investigation_uuid'"
        ");"
    );

    investigation_name = test_database_read_single_text(
        database,
        "SELECT name "
        "FROM investigation "
        "LIMIT 1;"
    );

    investigation_root_path = test_database_read_single_text(
        database,
        "SELECT root_path "
        "FROM investigation "
        "LIMIT 1;"
    );

    integrity_result = test_database_read_single_text(
        database,
        "PRAGMA integrity_check;"
    );

    /*
     * Aucune seconde enquête ne doit avoir été ajoutée.
     */
    assert(strcmp(investigation_count, "1") == 0);

    /*
     * Les quatre métadonnées initiales doivent toujours être présentes
     * sans duplication.
     */
    assert(strcmp(metadata_count, "4") == 0);

    /*
     * Les données de la première initialisation doivent être intactes.
     */
    assert(
        strcmp(
            investigation_name,
            "Enquete_Initiale"
        ) == 0
    );

    assert(
        strcmp(
            investigation_root_path,
            temporary_directory
        ) == 0
    );

    /*
     * La base doit rester cohérente après le rollback.
     */
    assert(strcmp(integrity_result, "ok") == 0);

    result = sqlite3_close(database);

    assert(result == SQLITE_OK);

    assert(g_remove(database_path) == 0);
    assert(g_rmdir(temporary_directory) == 0);

    g_free(integrity_result);
    g_free(investigation_root_path);
    g_free(investigation_name);
    g_free(metadata_count);
    g_free(investigation_count);
    g_free(database_path);
    g_free(temporary_directory);
}

int main(void)
{
    test_database_initialize_valid_database();
    test_database_initialize_rollback();
    test_database_initialize_invalid_parameters();
    test_database_initialize_missing_parent();

    printf(
        "Database : tous les tests sont valides.\n"
    );

    return 0;
}
