/******************************************************************************
 * @file graph_node_position_dao.c
 * @brief Persistance des positions des nœuds du graphe dans SQLite.
 ******************************************************************************/

#include "dao/graph_node_position_dao.h"

#include "database/error.h"
#include "database/statement.h"

#include <math.h>
#include <string.h>

/**
 * @struct GraphNodePositionDao
 * @brief Représentation privée du DAO.
 *
 * La connexion Database est empruntée.
 */
struct GraphNodePositionDao
{
    Database *database;
};

/**
 * @brief Requête de chargement de toutes les positions.
 */
static const char *const graph_node_position_dao_list_all_sql =
    "SELECT "
    "    node_id,"
    "    x,"
    "    y,"
    "    updated_at "
    "FROM graph_layout_positions "
    "ORDER BY node_id ASC;";

/**
 * @brief Requête d'insertion ou de mise à jour d'une position.
 */
static const char *const graph_node_position_dao_upsert_sql =
    "INSERT INTO graph_layout_positions"
    "("
    "    node_id,"
    "    x,"
    "    y,"
    "    updated_at"
    ")"
    "VALUES"
    "("
    "    ?,"
    "    ?,"
    "    ?,"
    "    ?"
    ")"
    "ON CONFLICT(node_id)"
    "DO UPDATE SET "
    "    x = excluded.x,"
    "    y = excluded.y,"
    "    updated_at = excluded.updated_at;";

/**
 * @brief Requête de suppression d'une position.
 */
static const char *const graph_node_position_dao_delete_sql =
    "DELETE FROM graph_layout_positions "
    "WHERE node_id = ?;";

/**
 * @brief Requête de suppression de toutes les positions.
 */
static const char *const graph_node_position_dao_delete_all_sql =
    "DELETE FROM graph_layout_positions;";

/**
 * @brief Enregistre une erreur littérale.
 */
static void graph_node_position_dao_set_error_literal(
    GError **error,
    GraphNodePositionDaoError error_code,
    const char *error_message
)
{
    if (error == NULL)
    {
        return;
    }

    g_set_error_literal(
        error,
        GRAPH_NODE_POSITION_DAO_ERROR,
        error_code,
        error_message
    );
}

/**
 * @brief Transforme la dernière erreur Database en erreur du DAO.
 */
static void graph_node_position_dao_set_database_error(
    GraphNodePositionDao *position_dao,
    GError **error,
    GraphNodePositionDaoError error_code,
    const char *context
)
{
    const char *database_error_message =
        NULL;

    if (error == NULL)
    {
        return;
    }

    if (position_dao != NULL &&
        position_dao->database != NULL)
    {
        database_error_message =
            database_error_get_message(
                position_dao->database
            );
    }

    if (database_error_message != NULL &&
        database_error_message[0] != '\0')
    {
        g_set_error(
            error,
            GRAPH_NODE_POSITION_DAO_ERROR,
            error_code,
            "%s : %s",
            context,
            database_error_message
        );

        return;
    }

    g_set_error_literal(
        error,
        GRAPH_NODE_POSITION_DAO_ERROR,
        error_code,
        context
    );
}

/**
 * @brief Génère une date UTC au format attendu par le modèle.
 *
 * @return Nouvelle chaîne à libérer avec g_free(), ou NULL.
 */
static char *graph_node_position_dao_create_utc_timestamp(void)
{
    GDateTime *date_time =
        NULL;

    char *timestamp =
        NULL;

    date_time =
        g_date_time_new_now_utc();

    if (date_time == NULL)
    {
        return NULL;
    }

    timestamp =
        g_date_time_format(
            date_time,
            "%Y-%m-%dT%H:%M:%SZ"
        );

    g_date_time_unref(
        date_time
    );

    return timestamp;
}

/**
 * @brief Libère une position contenue dans un GPtrArray.
 */
static void graph_node_position_dao_record_destroy(
    gpointer data
)
{
    graph_node_position_free(
        data
    );
}

/**
 * @brief Construit une position depuis la ligne SQLite courante.
 */
static GraphNodePosition *
graph_node_position_dao_read_current_record(
    GraphNodePositionDao *position_dao,
    DatabaseStatement *statement,
    GError **error
)
{
    GraphNodePosition *position =
        NULL;

    char *node_identifier =
        NULL;

    char *updated_at =
        NULL;

    double x =
        0.0;

    double y =
        0.0;

    GError *model_error =
        NULL;

    if (position_dao == NULL ||
        position_dao->database == NULL ||
        statement == NULL)
    {
        graph_node_position_dao_set_error_literal(
            error,
            GRAPH_NODE_POSITION_DAO_ERROR_INVALID_ARGUMENT,
            "Impossible de lire une position avec des paramètres invalides."
        );

        return NULL;
    }

    if (!database_statement_column_text(
            statement,
            0,
            &node_identifier
        ) ||
        !database_statement_column_double(
            statement,
            1,
            &x
        ) ||
        !database_statement_column_double(
            statement,
            2,
            &y
        ) ||
        !database_statement_column_text(
            statement,
            3,
            &updated_at
        ))
    {
        graph_node_position_dao_set_error_literal(
            error,
            GRAPH_NODE_POSITION_DAO_ERROR_READ,
            "Impossible de lire les colonnes de la position du nœud."
        );

        goto cleanup;
    }

    position =
        graph_node_position_new(
            node_identifier,
            x,
            y,
            updated_at,
            &model_error
        );

    if (position == NULL)
    {
        if (model_error != NULL)
        {
            g_set_error(
                error,
                GRAPH_NODE_POSITION_DAO_ERROR,
                GRAPH_NODE_POSITION_DAO_ERROR_MODEL,
                "La position persistée est invalide : %s",
                model_error->message
            );
        }
        else
        {
            graph_node_position_dao_set_error_literal(
                error,
                GRAPH_NODE_POSITION_DAO_ERROR_MODEL,
                "La position persistée ne peut pas être reconstruite."
            );
        }
    }

cleanup:

    g_clear_error(
        &model_error
    );

    g_free(
        updated_at
    );

    g_free(
        node_identifier
    );

    return position;
}

/**
 * @brief Convertit une erreur SQLite d'UPSERT en erreur du DAO.
 */
static void graph_node_position_dao_set_upsert_error(
    GraphNodePositionDao *position_dao,
    GError **error
)
{
    const char *database_error_message =
        NULL;

    if (position_dao != NULL &&
        position_dao->database != NULL)
    {
        database_error_message =
            database_error_get_message(
                position_dao->database
            );
    }

    if (database_error_message != NULL &&
        (
            strstr(
                database_error_message,
                "constraint"
            ) != NULL ||
            strstr(
                database_error_message,
                "CONSTRAINT"
            ) != NULL ||
            strstr(
                database_error_message,
                "FOREIGN KEY"
            ) != NULL
        ))
    {
        graph_node_position_dao_set_database_error(
            position_dao,
            error,
            GRAPH_NODE_POSITION_DAO_ERROR_CONSTRAINT,
            "La position viole une contrainte SQLite"
        );

        return;
    }

    graph_node_position_dao_set_database_error(
        position_dao,
        error,
        GRAPH_NODE_POSITION_DAO_ERROR_EXECUTE,
        "Impossible d'enregistrer la position du nœud"
    );
}

GQuark graph_node_position_dao_error_quark(void)
{
    return g_quark_from_static_string(
        "graph-node-position-dao-error-quark"
    );
}

GraphNodePositionDao *graph_node_position_dao_new(
    Database *database,
    GError **error
)
{
    GraphNodePositionDao *position_dao =
        NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (database == NULL)
    {
        graph_node_position_dao_set_error_literal(
            error,
            GRAPH_NODE_POSITION_DAO_ERROR_INVALID_ARGUMENT,
            "La connexion Database est absente."
        );

        return NULL;
    }

    position_dao =
        g_try_new0(
            GraphNodePositionDao,
            1
        );

    if (position_dao == NULL)
    {
        graph_node_position_dao_set_error_literal(
            error,
            GRAPH_NODE_POSITION_DAO_ERROR_MEMORY,
            "Impossible d'allouer le DAO des positions de nœuds."
        );

        return NULL;
    }

    position_dao->database =
        database;

    return position_dao;
}

void graph_node_position_dao_free(
    GraphNodePositionDao *position_dao
)
{
    if (position_dao == NULL)
    {
        return;
    }

    position_dao->database =
        NULL;

    g_free(
        position_dao
    );
}

GPtrArray *graph_node_position_dao_list_all(
    GraphNodePositionDao *position_dao,
    GError **error
)
{
    DatabaseStatement *statement =
        NULL;

    GPtrArray *positions =
        NULL;

    GraphNodePosition *position =
        NULL;

    DatabaseStatementStepResult step_result =
        DATABASE_STATEMENT_STEP_ERROR;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (position_dao == NULL ||
        position_dao->database == NULL)
    {
        graph_node_position_dao_set_error_literal(
            error,
            GRAPH_NODE_POSITION_DAO_ERROR_INVALID_ARGUMENT,
            "Le DAO des positions de nœuds est invalide."
        );

        return NULL;
    }

    positions =
        g_ptr_array_new_with_free_func(
            graph_node_position_dao_record_destroy
        );

    if (positions == NULL)
    {
        graph_node_position_dao_set_error_literal(
            error,
            GRAPH_NODE_POSITION_DAO_ERROR_MEMORY,
            "Impossible d'allouer la liste des positions de nœuds."
        );

        return NULL;
    }

    statement =
        database_statement_prepare(
            position_dao->database,
            graph_node_position_dao_list_all_sql
        );

    if (statement == NULL)
    {
        graph_node_position_dao_set_database_error(
            position_dao,
            error,
            GRAPH_NODE_POSITION_DAO_ERROR_PREPARE,
            "Impossible de préparer le chargement des positions"
        );

        goto error;
    }

    while (TRUE)
    {
        step_result =
            database_statement_step(
                statement
            );

        if (step_result ==
            DATABASE_STATEMENT_STEP_DONE)
        {
            break;
        }

        if (step_result ==
            DATABASE_STATEMENT_STEP_ERROR)
        {
            graph_node_position_dao_set_database_error(
                position_dao,
                error,
                GRAPH_NODE_POSITION_DAO_ERROR_EXECUTE,
                "Impossible d'exécuter le chargement des positions"
            );

            goto error;
        }

        position =
            graph_node_position_dao_read_current_record(
                position_dao,
                statement,
                error
            );

        if (position == NULL)
        {
            goto error;
        }

        g_ptr_array_add(
            positions,
            position
        );

        /*
         * Le tableau devient propriétaire du modèle.
         */
        position =
            NULL;
    }

    database_statement_finalize(
        statement
    );

    return positions;

error:

    graph_node_position_free(
        position
    );

    database_statement_finalize(
        statement
    );

    g_ptr_array_unref(
        positions
    );

    return NULL;
}

gboolean graph_node_position_dao_upsert(
    GraphNodePositionDao *position_dao,
    const char *node_identifier,
    double x,
    double y,
    GError **error
)
{
    DatabaseStatement *statement =
        NULL;

    char *updated_at =
        NULL;

    gboolean success =
        FALSE;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (position_dao == NULL ||
        position_dao->database == NULL ||
        node_identifier == NULL ||
        !g_uuid_string_is_valid(
            node_identifier
        ) ||
        !isfinite(x) ||
        !isfinite(y))
    {
        graph_node_position_dao_set_error_literal(
            error,
            GRAPH_NODE_POSITION_DAO_ERROR_INVALID_ARGUMENT,
            "Les paramètres de la position du nœud sont invalides."
        );

        return FALSE;
    }

    updated_at =
        graph_node_position_dao_create_utc_timestamp();

    if (updated_at == NULL)
    {
        graph_node_position_dao_set_error_literal(
            error,
            GRAPH_NODE_POSITION_DAO_ERROR_MEMORY,
            "Impossible de générer la date de modification de la position."
        );

        return FALSE;
    }

    statement =
        database_statement_prepare(
            position_dao->database,
            graph_node_position_dao_upsert_sql
        );

    if (statement == NULL)
    {
        graph_node_position_dao_set_database_error(
            position_dao,
            error,
            GRAPH_NODE_POSITION_DAO_ERROR_PREPARE,
            "Impossible de préparer l'enregistrement de la position"
        );

        goto cleanup;
    }

    if (!database_statement_bind_text(
            statement,
            1,
            node_identifier
        ) ||
        !database_statement_bind_double(
            statement,
            2,
            x
        ) ||
        !database_statement_bind_double(
            statement,
            3,
            y
        ) ||
        !database_statement_bind_text(
            statement,
            4,
            updated_at
        ))
    {
        graph_node_position_dao_set_database_error(
            position_dao,
            error,
            GRAPH_NODE_POSITION_DAO_ERROR_BIND,
            "Impossible de lier les données de la position"
        );

        goto cleanup;
    }

    if (database_statement_step(
            statement
        ) != DATABASE_STATEMENT_STEP_DONE)
    {
        graph_node_position_dao_set_upsert_error(
            position_dao,
            error
        );

        goto cleanup;
    }

    success =
        TRUE;

cleanup:

    database_statement_finalize(
        statement
    );

    g_free(
        updated_at
    );

    return success;
}

gboolean graph_node_position_dao_delete(
    GraphNodePositionDao *position_dao,
    const char *node_identifier,
    GError **error
)
{
    DatabaseStatement *statement =
        NULL;

    gboolean success =
        FALSE;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (position_dao == NULL ||
        position_dao->database == NULL ||
        node_identifier == NULL ||
        !g_uuid_string_is_valid(
            node_identifier
        ))
    {
        graph_node_position_dao_set_error_literal(
            error,
            GRAPH_NODE_POSITION_DAO_ERROR_INVALID_ARGUMENT,
            "L'identifiant du nœud est invalide."
        );

        return FALSE;
    }

    statement =
        database_statement_prepare(
            position_dao->database,
            graph_node_position_dao_delete_sql
        );

    if (statement == NULL)
    {
        graph_node_position_dao_set_database_error(
            position_dao,
            error,
            GRAPH_NODE_POSITION_DAO_ERROR_PREPARE,
            "Impossible de préparer la suppression de la position"
        );

        goto cleanup;
    }

    if (!database_statement_bind_text(
            statement,
            1,
            node_identifier
        ))
    {
        graph_node_position_dao_set_database_error(
            position_dao,
            error,
            GRAPH_NODE_POSITION_DAO_ERROR_BIND,
            "Impossible de lier l'identifiant du nœud"
        );

        goto cleanup;
    }

    if (database_statement_step(
            statement
        ) != DATABASE_STATEMENT_STEP_DONE)
    {
        graph_node_position_dao_set_database_error(
            position_dao,
            error,
            GRAPH_NODE_POSITION_DAO_ERROR_EXECUTE,
            "Impossible de supprimer la position du nœud"
        );

        goto cleanup;
    }

    success =
        TRUE;

cleanup:

    database_statement_finalize(
        statement
    );

    return success;
}

gboolean graph_node_position_dao_delete_all(
    GraphNodePositionDao *position_dao,
    GError **error
)
{
    DatabaseStatement *statement =
        NULL;

    gboolean success =
        FALSE;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (position_dao == NULL ||
        position_dao->database == NULL)
    {
        graph_node_position_dao_set_error_literal(
            error,
            GRAPH_NODE_POSITION_DAO_ERROR_INVALID_ARGUMENT,
            "Le DAO des positions de nœuds est invalide."
        );

        return FALSE;
    }

    statement =
        database_statement_prepare(
            position_dao->database,
            graph_node_position_dao_delete_all_sql
        );

    if (statement == NULL)
    {
        graph_node_position_dao_set_database_error(
            position_dao,
            error,
            GRAPH_NODE_POSITION_DAO_ERROR_PREPARE,
            "Impossible de préparer la suppression des positions"
        );

        goto cleanup;
    }

    if (database_statement_step(
            statement
        ) != DATABASE_STATEMENT_STEP_DONE)
    {
        graph_node_position_dao_set_database_error(
            position_dao,
            error,
            GRAPH_NODE_POSITION_DAO_ERROR_EXECUTE,
            "Impossible de supprimer les positions du graphe"
        );

        goto cleanup;
    }

    success =
        TRUE;

cleanup:

    database_statement_finalize(
        statement
    );

    return success;
}
