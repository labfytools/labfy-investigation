/******************************************************************************
 * @file investigation_dao.c
 * @brief Chargement de l'enquête persistée.
 ******************************************************************************/

#include "dao/investigation_dao.h"

#include "database/error.h"
#include "database/statement.h"

#include <glib.h>

/**
 * @brief Requête de chargement de l'enquête.
 */
static const char *const investigation_dao_load_sql =
    "SELECT "
    "    id, "
    "    name, "
    "    root_path, "
    "    created_at, "
    "    updated_at "
    "FROM investigation;";

/**
 * @brief Vérifie qu'une valeur obligatoire est présente.
 */
static bool investigation_dao_text_is_valid(
    const char *text
)
{
    return text != NULL && text[0] != '\0';
}

InvestigationRecord *investigation_dao_load(
    Database *database
)
{
    DatabaseStatement *statement = NULL;
    InvestigationRecord *record = NULL;

    char *id = NULL;
    char *name = NULL;
    char *root_path = NULL;
    char *created_at = NULL;
    char *updated_at = NULL;

    DatabaseStatementStepResult step_result =
        DATABASE_STATEMENT_STEP_ERROR;

    if (database == NULL)
    {
        return NULL;
    }

    statement = database_statement_prepare(
        database,
        investigation_dao_load_sql
    );

    if (statement == NULL)
    {
        goto cleanup;
    }

    step_result = database_statement_step(
        statement
    );

    if (step_result == DATABASE_STATEMENT_STEP_ERROR)
    {
        goto cleanup;
    }

    if (step_result == DATABASE_STATEMENT_STEP_DONE)
    {
        database_error_set(
            database,
            DATABASE_ERROR_INVALID_STATE,
            "La table investigation ne contient aucune ligne."
        );

        goto cleanup;
    }

    if (!database_statement_column_text(
            statement,
            0,
            &id
        ) ||
        !database_statement_column_text(
            statement,
            1,
            &name
        ) ||
        !database_statement_column_text(
            statement,
            2,
            &root_path
        ) ||
        !database_statement_column_text(
            statement,
            3,
            &created_at
        ) ||
        !database_statement_column_text(
            statement,
            4,
            &updated_at
        ))
    {
        database_error_set(
            database,
            DATABASE_ERROR_INVALID_STATE,
            "Impossible de lire les colonnes de l'enquête."
        );

        goto cleanup;
    }

    if (!investigation_dao_text_is_valid(id) ||
        !investigation_dao_text_is_valid(name) ||
        !investigation_dao_text_is_valid(root_path) ||
        !investigation_dao_text_is_valid(created_at) ||
        !investigation_dao_text_is_valid(updated_at))
    {
        database_error_set(
            database,
            DATABASE_ERROR_INVALID_STATE,
            "L'enquête contient une colonne obligatoire invalide."
        );

        goto cleanup;
    }

    record = investigation_record_new(
        id,
        name,
        root_path,
        created_at,
        updated_at
    );

    if (record == NULL)
    {
        database_error_set(
            database,
            DATABASE_ERROR_MEMORY,
            "Impossible d'allouer le modèle de l'enquête."
        );

        goto cleanup;
    }

    /*
     * Une seconde ligne indiquerait une base incohérente.
     */
    step_result = database_statement_step(
        statement
    );

    if (step_result == DATABASE_STATEMENT_STEP_ERROR)
    {
        investigation_record_free(record);
        record = NULL;

        goto cleanup;
    }

    if (step_result == DATABASE_STATEMENT_STEP_ROW)
    {
        database_error_set(
            database,
            DATABASE_ERROR_INVALID_STATE,
            "La table investigation contient plusieurs lignes."
        );

        investigation_record_free(record);
        record = NULL;

        goto cleanup;
    }

    database_error_clear(database);

cleanup:

    g_free(updated_at);
    g_free(created_at);
    g_free(root_path);
    g_free(name);
    g_free(id);

    database_statement_finalize(statement);

    return record;
}
