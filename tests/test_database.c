/******************************************************************************
 * @file test_database.c
 * @brief Tests d'intégration du module Database.
 ******************************************************************************/

#include "database/database.h"
#include "database/error.h"

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
 * @brief Exécute une ou plusieurs instructions SQL de test.
 */
static void test_database_execute_sql(
    sqlite3 *database,
    const char *sql
)
{
    char *error_message = NULL;
    int result = SQLITE_ERROR;

    assert(database != NULL);
    assert(sql != NULL);
    assert(sql[0] != '\0');

    result = sqlite3_exec(
        database,
        sql,
        NULL,
        NULL,
        &error_message
    );

    if (result != SQLITE_OK)
    {
        fprintf(
            stderr,
            "Erreur SQL de test : %s\n",
            error_message != NULL
                ? error_message
                : sqlite3_errmsg(database)
        );
    }

    assert(result == SQLITE_OK);

    sqlite3_free(
        error_message
    );
}

/**
 * @brief Crée une base V1 contenant une ancienne preuve.
 *
 * La base est construite directement avec schema_v1.sql afin de tester
 * le véritable chemin de migration, sans passer par database_initialize()
 * qui crée désormais directement une base V2.
 */
static void test_database_create_v1_database(
    const char *database_path
)
{
    sqlite3 *database = NULL;

    char *schema_sql = NULL;

    GError *error = NULL;

    int result = SQLITE_ERROR;

    assert(database_path != NULL);
    assert(database_path[0] != '\0');

    result = sqlite3_open_v2(
        database_path,
        &database,
        SQLITE_OPEN_READWRITE |
        SQLITE_OPEN_CREATE,
        NULL
    );

    assert(result == SQLITE_OK);
    assert(database != NULL);

    assert(
        g_file_get_contents(
            "database/schema_v1.sql",
            &schema_sql,
            NULL,
            &error
        )
    );

    assert(error == NULL);
    assert(schema_sql != NULL);

    test_database_execute_sql(
        database,
        "BEGIN IMMEDIATE;"
    );

    test_database_execute_sql(
        database,
        schema_sql
    );

    test_database_execute_sql(
        database,
        "INSERT INTO metadata"
        "("
        "    key,"
        "    value"
        ")"
        "VALUES"
        "    ('schema_version', '1'),"
        "    ('application', 'Labfy Investigation'),"
        "    ('created_at', '2026-07-18T10:00:00Z'),"
        "    ("
        "        'investigation_uuid',"
        "        '11111111-1111-4111-8111-111111111111'"
        "    );"
    );

    /*
     * Cette ligne utilise uniquement les colonnes du schéma V1.
     *
     * La migration devra conserver la ligne et copier name dans
     * original_name.
     */
    test_database_execute_sql(
        database,
        "INSERT INTO preuves"
        "("
        "    id,"
        "    name,"
        "    relative_path,"
        "    type_id,"
        "    size_bytes,"
        "    sha256,"
        "    mime_type,"
        "    description,"
        "    commentaire,"
        "    categorie_id,"
        "    file_created_at,"
        "    imported_at,"
        "    updated_at,"
        "    status,"
        "    locked"
        ")"
        "VALUES"
        "("
        "    '22222222-2222-4222-8222-222222222222',"
        "    'ancienne_capture.png',"
        "    '01_Preuves_Originales/ancienne_capture.png',"
        "    (SELECT id FROM types_preuve "
        "     WHERE code = 'screenshot'),"
        "    128,"
        "    '0123456789abcdef0123456789abcdef"
             "0123456789abcdef0123456789abcdef',"
        "    'image/png',"
        "    'Preuve créée avec le schéma V1.',"
        "    NULL,"
        "    NULL,"
        "    NULL,"
        "    '2026-07-18T10:00:00Z',"
        "    '2026-07-18T10:00:00Z',"
        "    'active',"
        "    1"
        ");"
    );

    test_database_execute_sql(
        database,
        "COMMIT;"
    );

    g_free(
        schema_sql
    );

    result = sqlite3_close(
        database
    );

    assert(result == SQLITE_OK);
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
 * @brief Vérifie qu’une colonne existe dans une table.
 */
static void test_database_assert_column_exists(
    sqlite3 *database,
    const char *table_name,
    const char *column_name
)
{
    sqlite3_stmt *statement = NULL;
    int result = SQLITE_ERROR;

    assert(database != NULL);
    assert(table_name != NULL);
    assert(column_name != NULL);

    result = sqlite3_prepare_v2(
        database,
        "SELECT COUNT(*) "
        "FROM pragma_table_info(?) "
        "WHERE name = ?;",
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

    result = sqlite3_bind_text(
        statement,
        2,
        column_name,
        -1,
        SQLITE_TRANSIENT
    );

    assert(result == SQLITE_OK);

    result = sqlite3_step(
        statement
    );

    assert(result == SQLITE_ROW);

    assert(
        sqlite3_column_int(
            statement,
            0
        ) == 1
    );

    result = sqlite3_finalize(
        statement
    );

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
    test_database_assert_table_exists(
        database,
        "preuves"
    );

    test_database_assert_column_exists(
        database,
        "preuves",
        "original_name"
    );

    test_database_assert_column_exists(
        database,
        "preuves",
        "collected_at"
    );

    test_database_assert_column_exists(
        database,
        "preuves",
        "source"
    );

    test_database_assert_column_exists(
        database,
        "preuves",
        "integrity_status"
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

    assert(strcmp(schema_version, "7") == 0);
    test_database_assert_table_exists(database, "osint_executions");
    test_database_assert_table_exists(database, "osint_execution_entities");
    test_database_assert_table_exists(database, "osint_execution_relations");
    test_database_assert_table_exists(database, "comptes_sociaux");
    test_database_assert_table_exists(database, "person_roles");
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

/**
 * @brief Vérifie la migration V1 vers la version courante sans perte.
 */
static void test_database_migrate_v1_to_v2(void)
{
    char *temporary_directory = NULL;
    char *database_path = NULL;

    char *schema_version = NULL;
    char *original_name = NULL;
    char *internal_name = NULL;
    char *relative_path = NULL;
    char *integrity_status = NULL;
    char *evidence_count = NULL;
    char *v2_column_count = NULL;
    char *migration_index_count = NULL;
    char *migration_trigger_count = NULL;

    Database *database_context = NULL;
    sqlite3 *database = NULL;

    GError *error = NULL;

    int result = SQLITE_ERROR;

    temporary_directory =
        g_dir_make_tmp(
            "labfy-database-migration-test-XXXXXX",
            &error
        );

    assert(temporary_directory != NULL);
    assert(error == NULL);

    database_path =
        g_build_filename(
            temporary_directory,
            "Enquete.sqlite",
            NULL
        );

    assert(database_path != NULL);

    test_database_create_v1_database(
        database_path
    );

    database_context =
        database_open(
            database_path
        );

    assert(database_context != NULL);

    /*
     * Premier appel : migrations réelles de V1 vers la version courante.
     */
    assert(
        database_migrate_to_latest(
            database_context
        )
    );

    assert(
        database_error_get_code(
            database_context
        ) == DATABASE_ERROR_NONE
    );

    /*
     * Second appel : la base est déjà à jour.
     *
     * Aucun ALTER TABLE, index ou trigger ne doit être rejoué.
     */
    assert(
        database_migrate_to_latest(
            database_context
        )
    );

    assert(
        database_error_get_code(
            database_context
        ) == DATABASE_ERROR_NONE
    );

    database_close(
        database_context
    );

    result = sqlite3_open_v2(
        database_path,
        &database,
        SQLITE_OPEN_READONLY,
        NULL
    );

    assert(result == SQLITE_OK);
    assert(database != NULL);

    schema_version =
        test_database_read_single_text(
            database,
            "SELECT value "
            "FROM metadata "
            "WHERE key = 'schema_version';"
        );

    evidence_count =
        test_database_read_single_text(
            database,
            "SELECT CAST(COUNT(*) AS TEXT) "
            "FROM preuves;"
        );

    internal_name =
        test_database_read_single_text(
            database,
            "SELECT name "
            "FROM preuves "
            "WHERE id = "
            "'22222222-2222-4222-8222-222222222222';"
        );

    original_name =
        test_database_read_single_text(
            database,
            "SELECT original_name "
            "FROM preuves "
            "WHERE id = "
            "'22222222-2222-4222-8222-222222222222';"
        );

    relative_path =
        test_database_read_single_text(
            database,
            "SELECT relative_path "
            "FROM preuves "
            "WHERE id = "
            "'22222222-2222-4222-8222-222222222222';"
        );

    integrity_status =
        test_database_read_single_text(
            database,
            "SELECT CAST(integrity_status AS TEXT) "
            "FROM preuves "
            "WHERE id = "
            "'22222222-2222-4222-8222-222222222222';"
        );

    v2_column_count =
        test_database_read_single_text(
            database,
            "SELECT CAST(COUNT(*) AS TEXT) "
            "FROM pragma_table_info('preuves') "
            "WHERE name IN"
            "("
            "    'original_name',"
            "    'collected_at',"
            "    'source',"
            "    'integrity_status'"
            ");"
        );

    migration_index_count =
        test_database_read_single_text(
            database,
            "SELECT CAST(COUNT(*) AS TEXT) "
            "FROM sqlite_master "
            "WHERE type = 'index' "
            "AND name = 'idx_preuves_imported_at';"
        );

    migration_trigger_count =
        test_database_read_single_text(
            database,
            "SELECT CAST(COUNT(*) AS TEXT) "
            "FROM sqlite_master "
            "WHERE type = 'trigger' "
            "AND name IN"
            "("
            "    'preuves_v2_validate_insert',"
            "    'preuves_v2_validate_update'"
            ");"
        );

    assert(
        strcmp(
            schema_version,
            "6"
        ) == 0
    );

    /*
     * La ligne V1 doit être conservée.
     */
    assert(
        strcmp(
            evidence_count,
            "1"
        ) == 0
    );

    assert(
        strcmp(
            internal_name,
            "ancienne_capture.png"
        ) == 0
    );

    /*
     * Le backfill V2 doit recopier name dans original_name.
     */
    assert(
        strcmp(
            original_name,
            "ancienne_capture.png"
        ) == 0
    );

    assert(
        strcmp(
            relative_path,
            "01_Preuves_Originales/ancienne_capture.png"
        ) == 0
    );

    /*
     * Une ancienne preuve n’ayant jamais été contrôlée reçoit UNKNOWN.
     */
    assert(
        strcmp(
            integrity_status,
            "0"
        ) == 0
    );

    /*
     * Les quatre colonnes ne doivent exister qu’une fois chacune.
     */
    assert(
        strcmp(
            v2_column_count,
            "4"
        ) == 0
    );

    assert(
        strcmp(
            migration_index_count,
            "1"
        ) == 0
    );

    assert(
        strcmp(
            migration_trigger_count,
            "2"
        ) == 0
    );

    result = sqlite3_close(
        database
    );

    assert(result == SQLITE_OK);

    assert(
        g_remove(
            database_path
        ) == 0
    );

    assert(
        g_rmdir(
            temporary_directory
        ) == 0
    );

    g_free(
        migration_trigger_count
    );

    g_free(
        migration_index_count
    );

    g_free(
        v2_column_count
    );

    g_free(
        evidence_count
    );

    g_free(
        integrity_status
    );

    g_free(
        relative_path
    );

    g_free(
        internal_name
    );

    g_free(
        original_name
    );

    g_free(
        schema_version
    );

    g_free(
        database_path
    );

    g_free(
        temporary_directory
    );
}

/**
 * @brief Vérifie qu’une migration V2 échouée est entièrement annulée.
 */
static void test_database_migration_rollback(void)
{
    char *temporary_directory = NULL;
    char *database_path = NULL;

    char *schema_version = NULL;
    char *original_name_column_count = NULL;
    char *collected_at_column_count = NULL;
    char *source_column_count = NULL;
    char *integrity_status_column_count = NULL;
    char *evidence_count = NULL;
    char *integrity_result = NULL;

    Database *database_context = NULL;
    sqlite3 *database = NULL;

    GError *error = NULL;

    int result = SQLITE_ERROR;

    temporary_directory =
        g_dir_make_tmp(
            "labfy-database-migration-rollback-test-XXXXXX",
            &error
        );

    assert(temporary_directory != NULL);
    assert(error == NULL);

    database_path =
        g_build_filename(
            temporary_directory,
            "Enquete.sqlite",
            NULL
        );

    assert(database_path != NULL);

    test_database_create_v1_database(
        database_path
    );

    result = sqlite3_open_v2(
        database_path,
        &database,
        SQLITE_OPEN_READWRITE,
        NULL
    );

    assert(result == SQLITE_OK);
    assert(database != NULL);

    /*
     * La migration V2 essaiera de créer un index portant ce nom.
     * Le conflit doit provoquer un échec après les ALTER TABLE.
     */
    test_database_execute_sql(
        database,
        "CREATE INDEX idx_preuves_imported_at "
        "ON preuves(name);"
    );

    result = sqlite3_close(
        database
    );

    assert(result == SQLITE_OK);

    database = NULL;

    database_context =
        database_open(
            database_path
        );

    assert(database_context != NULL);

    assert(
        !database_migrate_to_latest(
            database_context
        )
    );

    assert(
        database_error_get_code(
            database_context
        ) != DATABASE_ERROR_NONE
    );

    database_close(
        database_context
    );

    result = sqlite3_open_v2(
        database_path,
        &database,
        SQLITE_OPEN_READONLY,
        NULL
    );

    assert(result == SQLITE_OK);
    assert(database != NULL);

    schema_version =
        test_database_read_single_text(
            database,
            "SELECT value "
            "FROM metadata "
            "WHERE key = 'schema_version';"
        );

    original_name_column_count =
        test_database_read_single_text(
            database,
            "SELECT CAST(COUNT(*) AS TEXT) "
            "FROM pragma_table_info('preuves') "
            "WHERE name = 'original_name';"
        );

    collected_at_column_count =
        test_database_read_single_text(
            database,
            "SELECT CAST(COUNT(*) AS TEXT) "
            "FROM pragma_table_info('preuves') "
            "WHERE name = 'collected_at';"
        );

    source_column_count =
        test_database_read_single_text(
            database,
            "SELECT CAST(COUNT(*) AS TEXT) "
            "FROM pragma_table_info('preuves') "
            "WHERE name = 'source';"
        );

    integrity_status_column_count =
        test_database_read_single_text(
            database,
            "SELECT CAST(COUNT(*) AS TEXT) "
            "FROM pragma_table_info('preuves') "
            "WHERE name = 'integrity_status';"
        );

    evidence_count =
        test_database_read_single_text(
            database,
            "SELECT CAST(COUNT(*) AS TEXT) "
            "FROM preuves;"
        );

    integrity_result =
        test_database_read_single_text(
            database,
            "PRAGMA integrity_check;"
        );

    /*
     * La version ne doit pas avancer lorsque le SQL V2 échoue.
     */
    assert(
        strcmp(
            schema_version,
            "1"
        ) == 0
    );

    /*
     * Les ALTER TABLE précédant l’erreur doivent avoir été annulés.
     */
    assert(
        strcmp(
            original_name_column_count,
            "0"
        ) == 0
    );

    assert(
        strcmp(
            collected_at_column_count,
            "0"
        ) == 0
    );

    assert(
        strcmp(
            source_column_count,
            "0"
        ) == 0
    );

    assert(
        strcmp(
            integrity_status_column_count,
            "0"
        ) == 0
    );

    /*
     * La preuve V1 doit rester intacte.
     */
    assert(
        strcmp(
            evidence_count,
            "1"
        ) == 0
    );

    assert(
        strcmp(
            integrity_result,
            "ok"
        ) == 0
    );

    result = sqlite3_close(
        database
    );

    assert(result == SQLITE_OK);

    assert(
        g_remove(
            database_path
        ) == 0
    );

    assert(
        g_rmdir(
            temporary_directory
        ) == 0
    );

    g_free(
        integrity_result
    );

    g_free(
        evidence_count
    );

    g_free(
        integrity_status_column_count
    );

    g_free(
        source_column_count
    );

    g_free(
        collected_at_column_count
    );

    g_free(
        original_name_column_count
    );

    g_free(
        schema_version
    );

    g_free(
        database_path
    );

    g_free(
        temporary_directory
    );
}

int main(void)
{
    test_database_initialize_valid_database();
    test_database_initialize_rollback();

    test_database_migrate_v1_to_v2();
    test_database_migration_rollback();

    test_database_initialize_invalid_parameters();
    test_database_initialize_missing_parent();

    printf(
        "Database : tous les tests sont valides.\n"
    );

    return 0;
}
