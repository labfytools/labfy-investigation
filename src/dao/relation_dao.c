/******************************************************************************
 * @file relation_dao.c
 * @brief Persistance des relations entre entités dans SQLite.
 ******************************************************************************/

#include "dao/relation_dao.h"

#include "database/error.h"
#include "database/statement.h"

#include <glib.h>
#include <stdint.h>

/**
 * @brief Représentation privée du DAO.
 *
 * La connexion Database est empruntée.
 */
struct RelationDao
{
    Database *database;
};

/**
 * @brief Requête d'insertion d'une relation.
 */
static const char *const relation_dao_insert_sql =
    "INSERT INTO relations"
    "("
    "    id,"
    "    entite_source_id,"
    "    entite_cible_id,"
    "    type_relation,"
    "    label,"
    "    justification,"
    "    confiance,"
    "    created_at,"
    "    updated_at,"
    "    status"
    ")"
    "VALUES"
    "("
    "    ?,"
    "    ?,"
    "    ?,"
    "    ?,"
    "    ?,"
    "    ?,"
    "    ?,"
    "    ?,"
    "    ?,"
    "    ?"
    ");";

/** @brief Requête de modification des attributs d'une relation. */
static const char *const relation_dao_update_sql =
    "UPDATE relations SET "
    "type_relation = ?, label = ?, justification = ?, confiance = ?, "
    "updated_at = ?, status = ? WHERE id = ?;";

/** @brief Requête de suppression d'une relation. */
static const char *const relation_dao_delete_sql =
    "DELETE FROM relations WHERE id = ?;";

/**
 * @brief Requête de recherche par UUID.
 */
static const char *const relation_dao_find_by_identifier_sql =
    "SELECT "
    "    id,"
    "    entite_source_id,"
    "    entite_cible_id,"
    "    type_relation,"
    "    label,"
    "    justification,"
    "    confiance,"
    "    created_at,"
    "    updated_at,"
    "    status "
    "FROM relations "
    "WHERE id = ?;";

/**
 * @brief Requête de chargement de toutes les relations.
 */
static const char *const relation_dao_list_all_sql =
    "SELECT "
    "    id,"
    "    entite_source_id,"
    "    entite_cible_id,"
    "    type_relation,"
    "    label,"
    "    justification,"
    "    confiance,"
    "    created_at,"
    "    updated_at,"
    "    status "
    "FROM relations "
    "ORDER BY "
    "    created_at ASC,"
    "    id ASC;";

/**
 * @brief Requête de comptage.
 */
static const char *const relation_dao_count_sql =
    "SELECT COUNT(*) "
    "FROM relations;";

/**
 * @brief Requête de détection d'un UUID de relation.
 */
static const char *const relation_dao_identifier_exists_sql =
    "SELECT 1 "
    "FROM relations "
    "WHERE id = ? "
    "LIMIT 1;";

/**
 * @brief Requête de détection d'une entité.
 */
static const char *const relation_dao_entity_exists_sql =
    "SELECT 1 "
    "FROM entites "
    "WHERE id = ? "
    "LIMIT 1;";

/**
 * @brief Requête de détection d'un doublon orienté.
 */
static const char *const relation_dao_duplicate_exists_sql =
    "SELECT 1 "
    "FROM relations "
    "WHERE entite_source_id = ? "
    "AND entite_cible_id = ? "
    "AND type_relation = ? "
    "LIMIT 1;";

/**
 * @brief Enregistre une erreur littérale.
 */
static void relation_dao_set_error_literal(
    GError **error,
    RelationDaoError error_code,
    const char *message
)
{
    if (error == NULL ||
        *error != NULL)
    {
        return;
    }

    g_set_error_literal(
        error,
        RELATION_DAO_ERROR,
        error_code,
        message
    );
}

/**
 * @brief Détermine si SQLite signale un schéma incompatible.
 */
static gboolean relation_dao_is_schema_error(
    const char *database_message
)
{
    if (database_message == NULL)
    {
        return FALSE;
    }

    return g_strstr_len(
               database_message,
               -1,
               "no such table"
           ) != NULL ||
           g_strstr_len(
               database_message,
               -1,
               "no such column"
           ) != NULL;
}

/**
 * @brief Détermine si SQLite signale une contrainte.
 */
static gboolean relation_dao_is_constraint_error(
    const char *database_message
)
{
    if (database_message == NULL)
    {
        return FALSE;
    }

    return g_strstr_len(
               database_message,
               -1,
               "constraint"
           ) != NULL ||
           g_strstr_len(
               database_message,
               -1,
               "CONSTRAINT"
           ) != NULL;
}

/**
 * @brief Transforme la dernière erreur Database en erreur du DAO.
 */
static void relation_dao_set_database_error(
    RelationDao *relation_dao,
    GError **error,
    RelationDaoError error_code,
    const char *context
)
{
    const char *database_message =
        NULL;

    if (error == NULL ||
        *error != NULL)
    {
        return;
    }

    if (relation_dao != NULL &&
        relation_dao->database != NULL)
    {
        database_message =
            database_error_get_message(
                relation_dao->database
            );
    }

    if (error_code == RELATION_DAO_ERROR_PREPARE &&
        relation_dao_is_schema_error(
            database_message
        ))
    {
        error_code =
            RELATION_DAO_ERROR_SCHEMA;
    }

    if (database_message != NULL &&
        database_message[0] != '\0')
    {
        g_set_error(
            error,
            RELATION_DAO_ERROR,
            error_code,
            "%s : %s",
            context,
            database_message
        );

        return;
    }

    g_set_error_literal(
        error,
        RELATION_DAO_ERROR,
        error_code,
        context
    );
}

/**
 * @brief Convertit un statut métier en texte SQLite.
 */
static const char *relation_dao_status_to_text(
    RelationStatus status
)
{
    switch (status)
    {
        case RELATION_STATUS_ACTIVE:
            return "active";

        case RELATION_STATUS_ARCHIVED:
            return "archived";

        case RELATION_STATUS_DELETED:
            return "deleted";

        case RELATION_STATUS_DISPUTED:
            return "disputed";

        case RELATION_STATUS_UNKNOWN:
        default:
            return NULL;
    }
}

/**
 * @brief Convertit un statut SQLite en statut métier.
 */
static gboolean relation_dao_status_from_text(
    const char *status_text,
    RelationStatus *out_status
)
{
    if (status_text == NULL ||
        out_status == NULL)
    {
        return FALSE;
    }

    if (g_strcmp0(
            status_text,
            "active"
        ) == 0)
    {
        *out_status =
            RELATION_STATUS_ACTIVE;

        return TRUE;
    }

    if (g_strcmp0(
            status_text,
            "archived"
        ) == 0)
    {
        *out_status =
            RELATION_STATUS_ARCHIVED;

        return TRUE;
    }

    if (g_strcmp0(
            status_text,
            "deleted"
        ) == 0)
    {
        *out_status =
            RELATION_STATUS_DELETED;

        return TRUE;
    }

    if (g_strcmp0(
            status_text,
            "disputed"
        ) == 0)
    {
        *out_status =
            RELATION_STATUS_DISPUTED;

        return TRUE;
    }

    return FALSE;
}

/**
 * @brief Lie un texte facultatif ou SQL NULL.
 */
static gboolean relation_dao_bind_optional_text(
    DatabaseStatement *statement,
    int parameter_index,
    const char *value
)
{
    if (value == NULL)
    {
        return database_statement_bind_null(
            statement,
            parameter_index
        );
    }

    return database_statement_bind_text(
        statement,
        parameter_index,
        value
    );
}

/**
 * @brief Exécute une recherche d'existence avec un paramètre.
 */
static gboolean relation_dao_value_exists(
    RelationDao *relation_dao,
    const char *sql,
    const char *value,
    gboolean *out_exists,
    GError **error
)
{
    DatabaseStatement *statement =
        NULL;

    DatabaseStatementStepResult step_result =
        DATABASE_STATEMENT_STEP_ERROR;

    gboolean success =
        FALSE;

    if (relation_dao == NULL ||
        relation_dao->database == NULL ||
        sql == NULL ||
        value == NULL ||
        out_exists == NULL)
    {
        relation_dao_set_error_literal(
            error,
            RELATION_DAO_ERROR_INVALID_ARGUMENT,
            "Les paramètres de la recherche d'existence sont invalides."
        );

        return FALSE;
    }

    *out_exists =
        FALSE;

    statement =
        database_statement_prepare(
            relation_dao->database,
            sql
        );

    if (statement == NULL)
    {
        relation_dao_set_database_error(
            relation_dao,
            error,
            RELATION_DAO_ERROR_PREPARE,
            "Impossible de préparer la recherche d'existence"
        );

        goto cleanup;
    }

    if (!database_statement_bind_text(
            statement,
            1,
            value
        ))
    {
        relation_dao_set_database_error(
            relation_dao,
            error,
            RELATION_DAO_ERROR_BIND,
            "Impossible de lier la valeur recherchée"
        );

        goto cleanup;
    }

    step_result =
        database_statement_step(
            statement
        );

    if (step_result ==
        DATABASE_STATEMENT_STEP_ERROR)
    {
        relation_dao_set_database_error(
            relation_dao,
            error,
            RELATION_DAO_ERROR_EXECUTE,
            "Impossible d'exécuter la recherche d'existence"
        );

        goto cleanup;
    }

    *out_exists =
        step_result ==
        DATABASE_STATEMENT_STEP_ROW;

    success =
        TRUE;

cleanup:

    database_statement_finalize(
        statement
    );

    return success;
}

/**
 * @brief Vérifie l'existence d'un doublon orienté.
 */
static gboolean relation_dao_duplicate_exists(
    RelationDao *relation_dao,
    const char *source_entity_identifier,
    const char *target_entity_identifier,
    const char *relation_type,
    gboolean *out_exists,
    GError **error
)
{
    DatabaseStatement *statement =
        NULL;

    DatabaseStatementStepResult step_result =
        DATABASE_STATEMENT_STEP_ERROR;

    gboolean success =
        FALSE;

    if (relation_dao == NULL ||
        relation_dao->database == NULL ||
        source_entity_identifier == NULL ||
        target_entity_identifier == NULL ||
        relation_type == NULL ||
        out_exists == NULL)
    {
        relation_dao_set_error_literal(
            error,
            RELATION_DAO_ERROR_INVALID_ARGUMENT,
            "Les paramètres de détection du doublon sont invalides."
        );

        return FALSE;
    }

    *out_exists =
        FALSE;

    statement =
        database_statement_prepare(
            relation_dao->database,
            relation_dao_duplicate_exists_sql
        );

    if (statement == NULL)
    {
        relation_dao_set_database_error(
            relation_dao,
            error,
            RELATION_DAO_ERROR_PREPARE,
            "Impossible de préparer la recherche du doublon"
        );

        goto cleanup;
    }

    if (!database_statement_bind_text(
            statement,
            1,
            source_entity_identifier
        ) ||
        !database_statement_bind_text(
            statement,
            2,
            target_entity_identifier
        ) ||
        !database_statement_bind_text(
            statement,
            3,
            relation_type
        ))
    {
        relation_dao_set_database_error(
            relation_dao,
            error,
            RELATION_DAO_ERROR_BIND,
            "Impossible de lier les données du doublon"
        );

        goto cleanup;
    }

    step_result =
        database_statement_step(
            statement
        );

    if (step_result ==
        DATABASE_STATEMENT_STEP_ERROR)
    {
        relation_dao_set_database_error(
            relation_dao,
            error,
            RELATION_DAO_ERROR_EXECUTE,
            "Impossible d'exécuter la recherche du doublon"
        );

        goto cleanup;
    }

    *out_exists =
        step_result ==
        DATABASE_STATEMENT_STEP_ROW;

    success =
        TRUE;

cleanup:

    database_statement_finalize(
        statement
    );

    return success;
}

/**
 * @brief Construit un RelationRecord depuis la ligne courante.
 */
static RelationRecord *relation_dao_read_current_record(
    RelationDao *relation_dao,
    DatabaseStatement *statement,
    GError **error
)
{
    RelationRecord *relation_record =
        NULL;

    char *identifier =
        NULL;

    char *source_entity_identifier =
        NULL;

    char *target_entity_identifier =
        NULL;

    char *relation_type =
        NULL;

    char *label =
        NULL;

    char *justification =
        NULL;

    char *created_at =
        NULL;

    char *updated_at =
        NULL;

    char *status_text =
        NULL;

    int64_t confidence_value =
        -1;

    RelationStatus status =
        RELATION_STATUS_UNKNOWN;

    GError *model_error =
        NULL;

    if (relation_dao == NULL ||
        relation_dao->database == NULL ||
        statement == NULL)
    {
        relation_dao_set_error_literal(
            error,
            RELATION_DAO_ERROR_INVALID_ARGUMENT,
            "Impossible de lire une relation avec des paramètres invalides."
        );

        return NULL;
    }

    if (!database_statement_column_text(
            statement,
            0,
            &identifier
        ) ||
        !database_statement_column_text(
            statement,
            1,
            &source_entity_identifier
        ) ||
        !database_statement_column_text(
            statement,
            2,
            &target_entity_identifier
        ) ||
        !database_statement_column_text(
            statement,
            3,
            &relation_type
        ) ||
        !database_statement_column_text(
            statement,
            4,
            &label
        ) ||
        !database_statement_column_text(
            statement,
            5,
            &justification
        ) ||
        !database_statement_column_int64(
            statement,
            6,
            &confidence_value
        ) ||
        !database_statement_column_text(
            statement,
            7,
            &created_at
        ) ||
        !database_statement_column_text(
            statement,
            8,
            &updated_at
        ) ||
        !database_statement_column_text(
            statement,
            9,
            &status_text
        ))
    {
        relation_dao_set_error_literal(
            error,
            RELATION_DAO_ERROR_READ,
            "Impossible de lire les colonnes de la relation."
        );

        goto cleanup;
    }

    if (confidence_value < 0 ||
        confidence_value > 100)
    {
        relation_dao_set_error_literal(
            error,
            RELATION_DAO_ERROR_RANGE,
            "La relation persistée possède une confiance invalide."
        );

        goto cleanup;
    }

    if (!relation_dao_status_from_text(
            status_text,
            &status
        ))
    {
        relation_dao_set_error_literal(
            error,
            RELATION_DAO_ERROR_MODEL,
            "La relation persistée possède un statut invalide."
        );

        goto cleanup;
    }

    relation_record =
        relation_record_new(
            identifier,
            source_entity_identifier,
            target_entity_identifier,
            relation_type,
            label,
            justification,
            (gint) confidence_value,
            created_at,
            updated_at,
            status,
            &model_error
        );

    if (relation_record == NULL)
    {
        if (model_error != NULL)
        {
            g_set_error(
                error,
                RELATION_DAO_ERROR,
                RELATION_DAO_ERROR_MODEL,
                "La relation persistée est invalide : %s",
                model_error->message
            );
        }
        else
        {
            relation_dao_set_error_literal(
                error,
                RELATION_DAO_ERROR_MODEL,
                "La relation persistée ne peut pas être reconstruite."
            );
        }
    }

cleanup:

    g_clear_error(
        &model_error
    );

    g_free(
        status_text
    );

    g_free(
        updated_at
    );

    g_free(
        created_at
    );

    g_free(
        justification
    );

    g_free(
        label
    );

    g_free(
        relation_type
    );

    g_free(
        target_entity_identifier
    );

    g_free(
        source_entity_identifier
    );

    g_free(
        identifier
    );

    return relation_record;
}

/**
 * @brief Libère un modèle contenu dans un GPtrArray.
 */
static void relation_dao_record_destroy(
    gpointer user_data
)
{
    relation_record_free(
        user_data
    );
}

GQuark relation_dao_error_quark(void)
{
    return g_quark_from_static_string(
        "relation-dao-error-quark"
    );
}

RelationDao *relation_dao_new(
    Database *database,
    GError **error
)
{
    RelationDao *relation_dao =
        NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (database == NULL)
    {
        relation_dao_set_error_literal(
            error,
            RELATION_DAO_ERROR_INVALID_ARGUMENT,
            "La connexion Database est obligatoire."
        );

        return NULL;
    }

    relation_dao =
        g_try_new0(
            RelationDao,
            1
        );

    if (relation_dao == NULL)
    {
        relation_dao_set_error_literal(
            error,
            RELATION_DAO_ERROR_MEMORY,
            "Impossible d'allouer le DAO des relations."
        );

        return NULL;
    }

    relation_dao->database =
        database;

    return relation_dao;
}

void relation_dao_free(
    RelationDao *relation_dao
)
{
    if (relation_dao == NULL)
    {
        return;
    }

    relation_dao->database =
        NULL;

    g_free(
        relation_dao
    );
}

gboolean relation_dao_insert(
    RelationDao *relation_dao,
    const RelationRecord *relation_record,
    GError **error
)
{
    DatabaseStatement *statement =
        NULL;

    const char *identifier =
        NULL;

    const char *source_entity_identifier =
        NULL;

    const char *target_entity_identifier =
        NULL;

    const char *relation_type =
        NULL;

    const char *label =
        NULL;

    const char *justification =
        NULL;

    const char *created_at =
        NULL;

    const char *updated_at =
        NULL;

    const char *status_text =
        NULL;

    const char *database_message =
        NULL;

    gint confidence =
        0;

    gboolean value_exists =
        FALSE;

    gboolean success =
        FALSE;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (relation_dao == NULL ||
        relation_dao->database == NULL ||
        relation_record == NULL)
    {
        relation_dao_set_error_literal(
            error,
            RELATION_DAO_ERROR_INVALID_ARGUMENT,
            "Le DAO ou la relation est absent."
        );

        return FALSE;
    }

    identifier =
        relation_record_get_identifier(
            relation_record
        );

    source_entity_identifier =
        relation_record_get_source_entity_identifier(
            relation_record
        );

    target_entity_identifier =
        relation_record_get_target_entity_identifier(
            relation_record
        );

    relation_type =
        relation_record_get_relation_type(
            relation_record
        );

    label =
        relation_record_get_label(
            relation_record
        );

    justification =
        relation_record_get_justification(
            relation_record
        );

    confidence =
        relation_record_get_confidence(
            relation_record
        );

    created_at =
        relation_record_get_created_at(
            relation_record
        );

    updated_at =
        relation_record_get_updated_at(
            relation_record
        );

    status_text =
        relation_dao_status_to_text(
            relation_record_get_status(
                relation_record
            )
        );

    if (identifier == NULL ||
        source_entity_identifier == NULL ||
        target_entity_identifier == NULL ||
        relation_type == NULL ||
        created_at == NULL ||
        updated_at == NULL ||
        status_text == NULL)
    {
        relation_dao_set_error_literal(
            error,
            RELATION_DAO_ERROR_INVALID_ARGUMENT,
            "La relation contient une valeur obligatoire absente."
        );

        return FALSE;
    }

    if (!g_uuid_string_is_valid(
            identifier
        ) ||
        !g_uuid_string_is_valid(
            source_entity_identifier
        ) ||
        !g_uuid_string_is_valid(
            target_entity_identifier
        ))
    {
        relation_dao_set_error_literal(
            error,
            RELATION_DAO_ERROR_INVALID_ARGUMENT,
            "La relation contient un UUID invalide."
        );

        return FALSE;
    }

    if (g_strcmp0(
            source_entity_identifier,
            target_entity_identifier
        ) == 0)
    {
        relation_dao_set_error_literal(
            error,
            RELATION_DAO_ERROR_CONSTRAINT,
            "Une relation ne peut pas relier une entité à elle-même."
        );

        return FALSE;
    }

    if (confidence < 0 ||
        confidence > 100)
    {
        relation_dao_set_error_literal(
            error,
            RELATION_DAO_ERROR_RANGE,
            "Le niveau de confiance dépasse les limites autorisées."
        );

        return FALSE;
    }

    if (!relation_dao_value_exists(
            relation_dao,
            relation_dao_identifier_exists_sql,
            identifier,
            &value_exists,
            error
        ))
    {
        return FALSE;
    }

    if (value_exists)
    {
        relation_dao_set_error_literal(
            error,
            RELATION_DAO_ERROR_CONSTRAINT,
            "Une relation possède déjà cet identifiant."
        );

        return FALSE;
    }

    if (!relation_dao_value_exists(
            relation_dao,
            relation_dao_entity_exists_sql,
            source_entity_identifier,
            &value_exists,
            error
        ))
    {
        return FALSE;
    }

    if (!value_exists)
    {
        relation_dao_set_error_literal(
            error,
            RELATION_DAO_ERROR_CONSTRAINT,
            "L'entité source n'existe pas."
        );

        return FALSE;
    }

    if (!relation_dao_value_exists(
            relation_dao,
            relation_dao_entity_exists_sql,
            target_entity_identifier,
            &value_exists,
            error
        ))
    {
        return FALSE;
    }

    if (!value_exists)
    {
        relation_dao_set_error_literal(
            error,
            RELATION_DAO_ERROR_CONSTRAINT,
            "L'entité cible n'existe pas."
        );

        return FALSE;
    }

    if (!relation_dao_duplicate_exists(
            relation_dao,
            source_entity_identifier,
            target_entity_identifier,
            relation_type,
            &value_exists,
            error
        ))
    {
        return FALSE;
    }

    if (value_exists)
    {
        relation_dao_set_error_literal(
            error,
            RELATION_DAO_ERROR_CONSTRAINT,
            "Cette relation orientée existe déjà pour ce type."
        );

        return FALSE;
    }

    statement =
        database_statement_prepare(
            relation_dao->database,
            relation_dao_insert_sql
        );

    if (statement == NULL)
    {
        relation_dao_set_database_error(
            relation_dao,
            error,
            RELATION_DAO_ERROR_PREPARE,
            "Impossible de préparer l'insertion de la relation"
        );

        goto cleanup;
    }

    if (!database_statement_bind_text(
            statement,
            1,
            identifier
        ) ||
        !database_statement_bind_text(
            statement,
            2,
            source_entity_identifier
        ) ||
        !database_statement_bind_text(
            statement,
            3,
            target_entity_identifier
        ) ||
        !database_statement_bind_text(
            statement,
            4,
            relation_type
        ) ||
        !relation_dao_bind_optional_text(
            statement,
            5,
            label
        ) ||
        !relation_dao_bind_optional_text(
            statement,
            6,
            justification
        ) ||
        !database_statement_bind_int64(
            statement,
            7,
            (int64_t) confidence
        ) ||
        !database_statement_bind_text(
            statement,
            8,
            created_at
        ) ||
        !database_statement_bind_text(
            statement,
            9,
            updated_at
        ) ||
        !database_statement_bind_text(
            statement,
            10,
            status_text
        ))
    {
        relation_dao_set_database_error(
            relation_dao,
            error,
            RELATION_DAO_ERROR_BIND,
            "Impossible de lier les données de la relation"
        );

        goto cleanup;
    }

    if (database_statement_step(
            statement
        ) != DATABASE_STATEMENT_STEP_DONE)
    {
        database_message =
            database_error_get_message(
                relation_dao->database
            );

        if (relation_dao_is_constraint_error(
                database_message
            ))
        {
            relation_dao_set_database_error(
                relation_dao,
                error,
                RELATION_DAO_ERROR_CONSTRAINT,
                "La relation viole une contrainte SQLite"
            );
        }
        else
        {
            relation_dao_set_database_error(
                relation_dao,
                error,
                RELATION_DAO_ERROR_EXECUTE,
                "Impossible d'insérer la relation"
            );
        }

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

gboolean relation_dao_update(RelationDao *relation_dao,
    const RelationRecord *relation_record, GError **error)
{
    DatabaseStatement *statement = NULL;
    const char *status_text = NULL;
    gboolean success = FALSE;

    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    if (relation_dao == NULL || relation_record == NULL)
    {
        relation_dao_set_error_literal(error,
            RELATION_DAO_ERROR_INVALID_ARGUMENT,
            "La relation à modifier est invalide.");
        return FALSE;
    }
    status_text = relation_dao_status_to_text(
        relation_record_get_status(relation_record));
    statement = database_statement_prepare(relation_dao->database,
        relation_dao_update_sql);
    if (statement == NULL)
    {
        relation_dao_set_database_error(relation_dao, error,
            RELATION_DAO_ERROR_PREPARE,
            "Impossible de préparer la modification de la relation");
        return FALSE;
    }
    if (!database_statement_bind_text(statement, 1,
            relation_record_get_relation_type(relation_record)) ||
        !relation_dao_bind_optional_text(statement, 2,
            relation_record_get_label(relation_record)) ||
        !relation_dao_bind_optional_text(statement, 3,
            relation_record_get_justification(relation_record)) ||
        !database_statement_bind_int64(statement, 4,
            relation_record_get_confidence(relation_record)) ||
        !database_statement_bind_text(statement, 5,
            relation_record_get_updated_at(relation_record)) ||
        status_text == NULL ||
        !database_statement_bind_text(statement, 6, status_text) ||
        !database_statement_bind_text(statement, 7,
            relation_record_get_identifier(relation_record)))
    {
        relation_dao_set_database_error(relation_dao, error,
            RELATION_DAO_ERROR_BIND,
            "Impossible de lier la modification de la relation");
        goto cleanup;
    }
    if (database_statement_step(statement) != DATABASE_STATEMENT_STEP_DONE)
    {
        relation_dao_set_database_error(relation_dao, error,
            RELATION_DAO_ERROR_EXECUTE,
            "Impossible de modifier la relation");
        goto cleanup;
    }
    success = TRUE;
cleanup:
    database_statement_finalize(statement);
    return success;
}

gboolean relation_dao_delete(RelationDao *relation_dao,
    const char *identifier, GError **error)
{
    DatabaseStatement *statement = NULL;
    gboolean success = FALSE;
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    if (relation_dao == NULL || relation_dao->database == NULL ||
        identifier == NULL || !g_uuid_string_is_valid(identifier))
    {
        relation_dao_set_error_literal(error,
            RELATION_DAO_ERROR_INVALID_ARGUMENT,
            "Les paramètres de suppression de la relation sont invalides.");
        return FALSE;
    }
    statement = database_statement_prepare(relation_dao->database,
        relation_dao_delete_sql);
    if (statement == NULL)
    {
        relation_dao_set_database_error(relation_dao, error,
            RELATION_DAO_ERROR_PREPARE,
            "Impossible de préparer la suppression de la relation");
        goto cleanup;
    }
    if (!database_statement_bind_text(statement, 1, identifier))
    {
        relation_dao_set_database_error(relation_dao, error,
            RELATION_DAO_ERROR_BIND,
            "Impossible de lier l'identifiant de la relation");
        goto cleanup;
    }
    if (database_statement_step(statement) != DATABASE_STATEMENT_STEP_DONE)
    {
        relation_dao_set_database_error(relation_dao, error,
            RELATION_DAO_ERROR_EXECUTE,
            "Impossible de supprimer la relation");
        goto cleanup;
    }
    success = TRUE;
cleanup:
    database_statement_finalize(statement);
    return success;
}

RelationRecord *relation_dao_find_by_identifier(
    RelationDao *relation_dao,
    const char *identifier,
    GError **error
)
{
    DatabaseStatement *statement =
        NULL;

    RelationRecord *relation_record =
        NULL;

    DatabaseStatementStepResult step_result =
        DATABASE_STATEMENT_STEP_ERROR;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (relation_dao == NULL ||
        relation_dao->database == NULL ||
        identifier == NULL ||
        !g_uuid_string_is_valid(
            identifier
        ))
    {
        relation_dao_set_error_literal(
            error,
            RELATION_DAO_ERROR_INVALID_ARGUMENT,
            "L'identifiant de relation est invalide."
        );

        return NULL;
    }

    statement =
        database_statement_prepare(
            relation_dao->database,
            relation_dao_find_by_identifier_sql
        );

    if (statement == NULL)
    {
        relation_dao_set_database_error(
            relation_dao,
            error,
            RELATION_DAO_ERROR_PREPARE,
            "Impossible de préparer la recherche de la relation"
        );

        goto cleanup;
    }

    if (!database_statement_bind_text(
            statement,
            1,
            identifier
        ))
    {
        relation_dao_set_database_error(
            relation_dao,
            error,
            RELATION_DAO_ERROR_BIND,
            "Impossible de lier l'identifiant de la relation"
        );

        goto cleanup;
    }

    step_result =
        database_statement_step(
            statement
        );

    if (step_result ==
        DATABASE_STATEMENT_STEP_ERROR)
    {
        relation_dao_set_database_error(
            relation_dao,
            error,
            RELATION_DAO_ERROR_EXECUTE,
            "Impossible d'exécuter la recherche de la relation"
        );

        goto cleanup;
    }

    if (step_result ==
        DATABASE_STATEMENT_STEP_DONE)
    {
        goto cleanup;
    }

    relation_record =
        relation_dao_read_current_record(
            relation_dao,
            statement,
            error
        );

    if (relation_record == NULL)
    {
        goto cleanup;
    }

    step_result =
        database_statement_step(
            statement
        );

    if (step_result !=
        DATABASE_STATEMENT_STEP_DONE)
    {
        if (step_result ==
            DATABASE_STATEMENT_STEP_ROW)
        {
            relation_dao_set_error_literal(
                error,
                RELATION_DAO_ERROR_SCHEMA,
                "Plusieurs relations possèdent le même identifiant."
            );
        }
        else
        {
            relation_dao_set_database_error(
                relation_dao,
                error,
                RELATION_DAO_ERROR_EXECUTE,
                "Impossible de terminer la lecture de la relation"
            );
        }

        relation_record_free(
            relation_record
        );

        relation_record =
            NULL;
    }

cleanup:

    database_statement_finalize(
        statement
    );

    return relation_record;
}

GPtrArray *relation_dao_list_all(
    RelationDao *relation_dao,
    GError **error
)
{
    DatabaseStatement *statement =
        NULL;

    GPtrArray *relation_records =
        NULL;

    RelationRecord *relation_record =
        NULL;

    DatabaseStatementStepResult step_result =
        DATABASE_STATEMENT_STEP_ERROR;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (relation_dao == NULL ||
        relation_dao->database == NULL)
    {
        relation_dao_set_error_literal(
            error,
            RELATION_DAO_ERROR_INVALID_ARGUMENT,
            "Le DAO des relations est invalide."
        );

        return NULL;
    }

    relation_records =
        g_ptr_array_new_with_free_func(
            relation_dao_record_destroy
        );

    if (relation_records == NULL)
    {
        relation_dao_set_error_literal(
            error,
            RELATION_DAO_ERROR_MEMORY,
            "Impossible d'allouer la liste des relations."
        );

        return NULL;
    }

    statement =
        database_statement_prepare(
            relation_dao->database,
            relation_dao_list_all_sql
        );

    if (statement == NULL)
    {
        relation_dao_set_database_error(
            relation_dao,
            error,
            RELATION_DAO_ERROR_PREPARE,
            "Impossible de préparer le chargement des relations"
        );

        goto failure;
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
            relation_dao_set_database_error(
                relation_dao,
                error,
                RELATION_DAO_ERROR_EXECUTE,
                "Impossible d'exécuter le chargement des relations"
            );

            goto failure;
        }

        relation_record =
            relation_dao_read_current_record(
                relation_dao,
                statement,
                error
            );

        if (relation_record == NULL)
        {
            goto failure;
        }

        g_ptr_array_add(
            relation_records,
            relation_record
        );

        relation_record =
            NULL;
    }

    database_statement_finalize(
        statement
    );

    return relation_records;

failure:

    relation_record_free(
        relation_record
    );

    database_statement_finalize(
        statement
    );

    g_ptr_array_unref(
        relation_records
    );

    return NULL;
}

gboolean relation_dao_count(
    RelationDao *relation_dao,
    guint64 *out_count,
    GError **error
)
{
    DatabaseStatement *statement =
        NULL;

    DatabaseStatementStepResult step_result =
        DATABASE_STATEMENT_STEP_ERROR;

    int64_t count_value =
        -1;

    gboolean success =
        FALSE;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (relation_dao == NULL ||
        relation_dao->database == NULL ||
        out_count == NULL)
    {
        relation_dao_set_error_literal(
            error,
            RELATION_DAO_ERROR_INVALID_ARGUMENT,
            "Les paramètres du comptage des relations sont invalides."
        );

        return FALSE;
    }

    *out_count =
        0;

    statement =
        database_statement_prepare(
            relation_dao->database,
            relation_dao_count_sql
        );

    if (statement == NULL)
    {
        relation_dao_set_database_error(
            relation_dao,
            error,
            RELATION_DAO_ERROR_PREPARE,
            "Impossible de préparer le comptage des relations"
        );

        goto cleanup;
    }

    step_result =
        database_statement_step(
            statement
        );

    if (step_result ==
        DATABASE_STATEMENT_STEP_ERROR)
    {
        relation_dao_set_database_error(
            relation_dao,
            error,
            RELATION_DAO_ERROR_EXECUTE,
            "Impossible d'exécuter le comptage des relations"
        );

        goto cleanup;
    }

    if (step_result !=
        DATABASE_STATEMENT_STEP_ROW)
    {
        relation_dao_set_error_literal(
            error,
            RELATION_DAO_ERROR_READ,
            "Le comptage des relations n'a retourné aucune ligne."
        );

        goto cleanup;
    }

    if (!database_statement_column_int64(
            statement,
            0,
            &count_value
        ))
    {
        relation_dao_set_error_literal(
            error,
            RELATION_DAO_ERROR_READ,
            "Impossible de lire le nombre de relations."
        );

        goto cleanup;
    }

    if (count_value < 0)
    {
        relation_dao_set_error_literal(
            error,
            RELATION_DAO_ERROR_RANGE,
            "Le nombre de relations retourné par SQLite est invalide."
        );

        goto cleanup;
    }

    step_result =
        database_statement_step(
            statement
        );

    if (step_result !=
        DATABASE_STATEMENT_STEP_DONE)
    {
        if (step_result ==
            DATABASE_STATEMENT_STEP_ROW)
        {
            relation_dao_set_error_literal(
                error,
                RELATION_DAO_ERROR_SCHEMA,
                "Le comptage des relations a retourné plusieurs lignes."
            );
        }
        else
        {
            relation_dao_set_database_error(
                relation_dao,
                error,
                RELATION_DAO_ERROR_EXECUTE,
                "Impossible de terminer le comptage des relations"
            );
        }

        goto cleanup;
    }

    *out_count =
        (guint64) count_value;

    success =
        TRUE;

cleanup:

    database_statement_finalize(
        statement
    );

    return success;
}
