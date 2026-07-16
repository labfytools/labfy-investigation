/******************************************************************************
 * @file test_investigation_dao.c
 * @brief Tests du DAO de l'enquête.
 ******************************************************************************/

#include "dao/investigation_dao.h"

#include "database/database.h"
#include "database/error.h"
#include "database/statement.h"
#include "models/investigation_record.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

/**
 * @brief Crée une table investigation minimale pour les tests du DAO.
 */
static void test_investigation_dao_create_table(
    Database *database
)
{
    DatabaseStatement *statement = NULL;

    assert(database != NULL);

    statement = database_statement_prepare(
        database,
        "CREATE TABLE investigation"
        "("
        "    id TEXT NOT NULL,"
        "    name TEXT NOT NULL,"
        "    root_path TEXT NOT NULL,"
        "    created_at TEXT NOT NULL,"
        "    updated_at TEXT NOT NULL"
        ");"
    );

    assert(statement != NULL);

    assert(
        database_statement_step(statement) ==
        DATABASE_STATEMENT_STEP_DONE
    );

    database_statement_finalize(statement);
}

/**
 * @brief Insère une ligne dans la table investigation de test.
 */
static void test_investigation_dao_insert_row(
    Database *database,
    const char *id,
    const char *name
)
{
    DatabaseStatement *statement = NULL;

    assert(database != NULL);
    assert(id != NULL);
    assert(name != NULL);

    statement = database_statement_prepare(
        database,
        "INSERT INTO investigation"
        "("
        "    id,"
        "    name,"
        "    root_path,"
        "    created_at,"
        "    updated_at"
        ")"
        "VALUES (?, ?, ?, ?, ?);"
    );

    assert(statement != NULL);

    assert(
        database_statement_bind_text(
            statement,
            1,
            id
        )
    );

    assert(
        database_statement_bind_text(
            statement,
            2,
            name
        )
    );

    assert(
        database_statement_bind_text(
            statement,
            3,
            "/tmp/enquete-dao"
        )
    );

    assert(
        database_statement_bind_text(
            statement,
            4,
            "2026-07-16T12:00:00Z"
        )
    );

    assert(
        database_statement_bind_text(
            statement,
            5,
            "2026-07-16T12:00:00Z"
        )
    );

    assert(
        database_statement_step(statement) ==
        DATABASE_STATEMENT_STEP_DONE
    );

    database_statement_finalize(statement);
}

/**
 * @brief Vérifie le refus d'une table investigation vide.
 */
static void test_investigation_dao_load_empty_table(void)
{
    Database *database = NULL;
    InvestigationRecord *record = NULL;
    const char *error_message = NULL;

    database = database_open(":memory:");

    assert(database != NULL);

    test_investigation_dao_create_table(database);

    record = investigation_dao_load(database);

    assert(record == NULL);

    assert(
        database_error_get_code(database) ==
        DATABASE_ERROR_INVALID_STATE
    );

    error_message = database_error_get_message(database);

    assert(error_message != NULL);
    assert(error_message[0] != '\0');

    database_close(database);
}

/**
 * @brief Vérifie le refus de plusieurs enquêtes dans la même base.
 */
static void test_investigation_dao_load_multiple_rows(void)
{
    Database *database = NULL;
    InvestigationRecord *record = NULL;
    const char *error_message = NULL;

    database = database_open(":memory:");

    assert(database != NULL);

    test_investigation_dao_create_table(database);

    test_investigation_dao_insert_row(
        database,
        "11111111-1111-4111-8111-111111111111",
        "Premiere_Enquete"
    );

    test_investigation_dao_insert_row(
        database,
        "22222222-2222-4222-8222-222222222222",
        "Seconde_Enquete"
    );

    record = investigation_dao_load(database);

    assert(record == NULL);

    assert(
        database_error_get_code(database) ==
        DATABASE_ERROR_INVALID_STATE
    );

    error_message = database_error_get_message(database);

    assert(error_message != NULL);
    assert(error_message[0] != '\0');

    database_close(database);
}

/**
 * @brief Vérifie le chargement d'une enquête valide.
 */
static void test_investigation_dao_load_valid(void)
{
    char *temporary_directory = NULL;
    char *database_path = NULL;

    Database *database = NULL;
    InvestigationRecord *record = NULL;

    const char *id = NULL;
    const char *created_at = NULL;
    const char *updated_at = NULL;

    GError *error = NULL;

    temporary_directory = g_dir_make_tmp(
        "labfy-investigation-dao-test-XXXXXX",
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
            "Enquete_DAO",
            temporary_directory
        )
    );

    database = database_open(
        database_path
    );

    assert(database != NULL);

    record = investigation_dao_load(
        database
    );

    assert(record != NULL);

    id = investigation_record_get_id(record);
    created_at = investigation_record_get_created_at(record);
    updated_at = investigation_record_get_updated_at(record);

    assert(id != NULL);
    assert(g_uuid_string_is_valid(id));

    assert(
        strcmp(
            investigation_record_get_name(record),
            "Enquete_DAO"
        ) == 0
    );

    assert(
        strcmp(
            investigation_record_get_root_path(record),
            temporary_directory
        ) == 0
    );

    assert(created_at != NULL);
    assert(created_at[0] != '\0');

    assert(updated_at != NULL);
    assert(updated_at[0] != '\0');

    assert(
        strcmp(
            created_at,
            updated_at
        ) == 0
    );

    assert(
        database_error_get_code(database) ==
        DATABASE_ERROR_NONE
    );

    investigation_record_free(record);
    database_close(database);

    assert(g_remove(database_path) == 0);
    assert(g_rmdir(temporary_directory) == 0);

    g_free(database_path);
    g_free(temporary_directory);
}

/**
 * @brief Vérifie le refus d'une connexion absente.
 */
static void test_investigation_dao_load_null_database(void)
{
    assert(
        investigation_dao_load(NULL) == NULL
    );
}

/**
 * @brief Vérifie le refus d'une enquête contenant une valeur obligatoire vide.
 */
static void test_investigation_dao_load_invalid_row(void)
{
    Database *database = NULL;
    InvestigationRecord *record = NULL;
    const char *error_message = NULL;

    database = database_open(":memory:");

    assert(database != NULL);

    test_investigation_dao_create_table(database);

    /*
     * La colonne name est NOT NULL, mais une chaîne vide reste autorisée
     * par SQLite. Le DAO doit la considérer comme invalide.
     */
    test_investigation_dao_insert_row(
        database,
        "33333333-3333-4333-8333-333333333333",
        ""
    );

    record = investigation_dao_load(database);

    assert(record == NULL);

    assert(
        database_error_get_code(database) ==
        DATABASE_ERROR_INVALID_STATE
    );

    error_message = database_error_get_message(database);

    assert(error_message != NULL);
    assert(error_message[0] != '\0');

    database_close(database);
}

int main(void)
{
    test_investigation_dao_load_valid();
    test_investigation_dao_load_null_database();
    test_investigation_dao_load_empty_table();
    test_investigation_dao_load_multiple_rows();
    test_investigation_dao_load_invalid_row();

    printf(
        "InvestigationDao : tous les tests sont valides.\n"
    );

    return 0;
}
