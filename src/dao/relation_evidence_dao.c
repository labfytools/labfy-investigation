/******************************************************************************
 * @file relation_evidence_dao.c
 * @brief Persistance des associations entre relations et preuves.
 ******************************************************************************/

#include "dao/relation_evidence_dao.h"

#include "database/error.h"
#include "database/statement.h"

#include <glib.h>

/**
 * @brief Représentation privée du DAO.
 *
 * La connexion Database est empruntée.
 */
struct RelationEvidenceDao
{
    Database *database;
};

/**
 * @brief Requête vérifiant l'existence d'une relation.
 */
static const char *const relation_evidence_dao_relation_exists_sql =
    "SELECT 1 "
    "FROM relations "
    "WHERE id = ? "
    "LIMIT 1;";

/**
 * @brief Requête vérifiant l'existence d'une preuve.
 */
static const char *const relation_evidence_dao_evidence_exists_sql =
    "SELECT 1 "
    "FROM preuves "
    "WHERE id = ? "
    "LIMIT 1;";

/**
 * @brief Requête vérifiant l'existence d'une association.
 */
static const char *const relation_evidence_dao_association_exists_sql =
    "SELECT 1 "
    "FROM relation_preuves "
    "WHERE relation_id = ? "
    "AND preuve_id = ? "
    "LIMIT 1;";

/**
 * @brief Requête créant une association.
 */
static const char *const relation_evidence_dao_link_sql =
    "INSERT INTO relation_preuves"
    "("
    "    relation_id,"
    "    preuve_id"
    ")"
    "VALUES"
    "("
    "    ?,"
    "    ?"
    ");";

/**
 * @brief Requête supprimant une association.
 */
static const char *const relation_evidence_dao_unlink_sql =
    "DELETE FROM relation_preuves "
    "WHERE relation_id = ? "
    "AND preuve_id = ?;";

/**
 * @brief Requête listant les preuves liées à une relation.
 */
static const char *const
relation_evidence_dao_list_evidence_identifiers_sql =
    "SELECT preuve_id "
    "FROM relation_preuves "
    "WHERE relation_id = ? "
    "ORDER BY preuve_id ASC;";

/**
 * @brief Requête listant les relations liées à une preuve.
 */
static const char *const
relation_evidence_dao_list_relation_identifiers_sql =
    "SELECT relation_id "
    "FROM relation_preuves "
    "WHERE preuve_id = ? "
    "ORDER BY relation_id ASC;";

/**
 * @brief Enregistre une erreur littérale du DAO.
 */
static void relation_evidence_dao_set_error_literal(
    GError **error,
    RelationEvidenceDaoError error_code,
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
        RELATION_EVIDENCE_DAO_ERROR,
        error_code,
        message
    );
}

/**
 * @brief Détermine si SQLite signale un schéma incompatible.
 */
static gboolean relation_evidence_dao_is_schema_error(
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
static gboolean relation_evidence_dao_is_constraint_error(
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
static void relation_evidence_dao_set_database_error(
    RelationEvidenceDao *relation_evidence_dao,
    GError **error,
    RelationEvidenceDaoError error_code,
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

    if (relation_evidence_dao != NULL &&
        relation_evidence_dao->database != NULL)
    {
        database_message =
            database_error_get_message(
                relation_evidence_dao->database
            );
    }

    if (error_code ==
            RELATION_EVIDENCE_DAO_ERROR_PREPARE &&
        relation_evidence_dao_is_schema_error(
            database_message
        ))
    {
        error_code =
            RELATION_EVIDENCE_DAO_ERROR_SCHEMA;
    }

    if (database_message != NULL &&
        database_message[0] != '\0')
    {
        g_set_error(
            error,
            RELATION_EVIDENCE_DAO_ERROR,
            error_code,
            "%s : %s",
            context,
            database_message
        );

        return;
    }

    g_set_error_literal(
        error,
        RELATION_EVIDENCE_DAO_ERROR,
        error_code,
        context
    );
}

/**
 * @brief Vérifie le DAO et les deux UUID d'une association.
 */
static gboolean relation_evidence_dao_validate_association(
    RelationEvidenceDao *relation_evidence_dao,
    const char *relation_identifier,
    const char *evidence_identifier,
    GError **error
)
{
    if (relation_evidence_dao == NULL ||
        relation_evidence_dao->database == NULL)
    {
        relation_evidence_dao_set_error_literal(
            error,
            RELATION_EVIDENCE_DAO_ERROR_INVALID_ARGUMENT,
            "Le DAO des associations relation-preuve est invalide."
        );

        return FALSE;
    }

    if (relation_identifier == NULL ||
        !g_uuid_string_is_valid(
            relation_identifier
        ))
    {
        relation_evidence_dao_set_error_literal(
            error,
            RELATION_EVIDENCE_DAO_ERROR_INVALID_ARGUMENT,
            "L'identifiant de relation est invalide."
        );

        return FALSE;
    }

    if (evidence_identifier == NULL ||
        !g_uuid_string_is_valid(
            evidence_identifier
        ))
    {
        relation_evidence_dao_set_error_literal(
            error,
            RELATION_EVIDENCE_DAO_ERROR_INVALID_ARGUMENT,
            "L'identifiant de preuve est invalide."
        );

        return FALSE;
    }

    return TRUE;
}

/**
 * @brief Vérifie le DAO et un UUID utilisé pour une liste.
 */
static gboolean relation_evidence_dao_validate_list_request(
    RelationEvidenceDao *relation_evidence_dao,
    const char *identifier,
    const char *invalid_identifier_message,
    GError **error
)
{
    if (relation_evidence_dao == NULL ||
        relation_evidence_dao->database == NULL)
    {
        relation_evidence_dao_set_error_literal(
            error,
            RELATION_EVIDENCE_DAO_ERROR_INVALID_ARGUMENT,
            "Le DAO des associations relation-preuve est invalide."
        );

        return FALSE;
    }

    if (identifier == NULL ||
        !g_uuid_string_is_valid(
            identifier
        ))
    {
        relation_evidence_dao_set_error_literal(
            error,
            RELATION_EVIDENCE_DAO_ERROR_INVALID_ARGUMENT,
            invalid_identifier_message
        );

        return FALSE;
    }

    return TRUE;
}

/**
 * @brief Exécute une requête SELECT 1 avec un ou deux paramètres.
 */
static gboolean relation_evidence_dao_query_exists(
    RelationEvidenceDao *relation_evidence_dao,
    const char *sql,
    const char *first_value,
    const char *second_value,
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

    if (relation_evidence_dao == NULL ||
        relation_evidence_dao->database == NULL ||
        sql == NULL ||
        first_value == NULL ||
        out_exists == NULL)
    {
        relation_evidence_dao_set_error_literal(
            error,
            RELATION_EVIDENCE_DAO_ERROR_INVALID_ARGUMENT,
            "Les paramètres de la recherche d'existence sont invalides."
        );

        return FALSE;
    }

    *out_exists =
        FALSE;

    statement =
        database_statement_prepare(
            relation_evidence_dao->database,
            sql
        );

    if (statement == NULL)
    {
        relation_evidence_dao_set_database_error(
            relation_evidence_dao,
            error,
            RELATION_EVIDENCE_DAO_ERROR_PREPARE,
            "Impossible de préparer la recherche d'existence"
        );

        goto cleanup;
    }

    if (!database_statement_bind_text(
            statement,
            1,
            first_value
        ))
    {
        relation_evidence_dao_set_database_error(
            relation_evidence_dao,
            error,
            RELATION_EVIDENCE_DAO_ERROR_BIND,
            "Impossible de lier le premier identifiant"
        );

        goto cleanup;
    }

    if (second_value != NULL &&
        !database_statement_bind_text(
            statement,
            2,
            second_value
        ))
    {
        relation_evidence_dao_set_database_error(
            relation_evidence_dao,
            error,
            RELATION_EVIDENCE_DAO_ERROR_BIND,
            "Impossible de lier le second identifiant"
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
        relation_evidence_dao_set_database_error(
            relation_evidence_dao,
            error,
            RELATION_EVIDENCE_DAO_ERROR_EXECUTE,
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
 * @brief Exécute une modification à deux UUID.
 */
static gboolean relation_evidence_dao_execute_pair_statement(
    RelationEvidenceDao *relation_evidence_dao,
    const char *sql,
    const char *relation_identifier,
    const char *evidence_identifier,
    const char *prepare_context,
    const char *bind_context,
    const char *execute_context,
    GError **error
)
{
    DatabaseStatement *statement =
        NULL;

    DatabaseStatementStepResult step_result =
        DATABASE_STATEMENT_STEP_ERROR;

    const char *database_message =
        NULL;

    gboolean success =
        FALSE;

    statement =
        database_statement_prepare(
            relation_evidence_dao->database,
            sql
        );

    if (statement == NULL)
    {
        relation_evidence_dao_set_database_error(
            relation_evidence_dao,
            error,
            RELATION_EVIDENCE_DAO_ERROR_PREPARE,
            prepare_context
        );

        goto cleanup;
    }

    if (!database_statement_bind_text(
            statement,
            1,
            relation_identifier
        ) ||
        !database_statement_bind_text(
            statement,
            2,
            evidence_identifier
        ))
    {
        relation_evidence_dao_set_database_error(
            relation_evidence_dao,
            error,
            RELATION_EVIDENCE_DAO_ERROR_BIND,
            bind_context
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
        database_message =
            database_error_get_message(
                relation_evidence_dao->database
            );

        if (relation_evidence_dao_is_constraint_error(
                database_message
            ))
        {
            relation_evidence_dao_set_database_error(
                relation_evidence_dao,
                error,
                RELATION_EVIDENCE_DAO_ERROR_CONSTRAINT,
                execute_context
            );
        }
        else
        {
            relation_evidence_dao_set_database_error(
                relation_evidence_dao,
                error,
                RELATION_EVIDENCE_DAO_ERROR_EXECUTE,
                execute_context
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

/**
 * @brief Charge une liste d'UUID depuis une requête à un paramètre.
 */
static GPtrArray *relation_evidence_dao_list_identifiers(
    RelationEvidenceDao *relation_evidence_dao,
    const char *sql,
    const char *identifier,
    const char *prepare_context,
    const char *bind_context,
    const char *execute_context,
    GError **error
)
{
    DatabaseStatement *statement =
        NULL;

    GPtrArray *identifiers =
        NULL;

    DatabaseStatementStepResult step_result =
        DATABASE_STATEMENT_STEP_ERROR;

    identifiers =
        g_ptr_array_new_with_free_func(
            g_free
        );

    if (identifiers == NULL)
    {
        relation_evidence_dao_set_error_literal(
            error,
            RELATION_EVIDENCE_DAO_ERROR_MEMORY,
            "Impossible d'allouer la liste d'identifiants."
        );

        return NULL;
    }

    statement =
        database_statement_prepare(
            relation_evidence_dao->database,
            sql
        );

    if (statement == NULL)
    {
        relation_evidence_dao_set_database_error(
            relation_evidence_dao,
            error,
            RELATION_EVIDENCE_DAO_ERROR_PREPARE,
            prepare_context
        );

        goto failure;
    }

    if (!database_statement_bind_text(
            statement,
            1,
            identifier
        ))
    {
        relation_evidence_dao_set_database_error(
            relation_evidence_dao,
            error,
            RELATION_EVIDENCE_DAO_ERROR_BIND,
            bind_context
        );

        goto failure;
    }

    while (TRUE)
    {
        char *loaded_identifier =
            NULL;

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
            relation_evidence_dao_set_database_error(
                relation_evidence_dao,
                error,
                RELATION_EVIDENCE_DAO_ERROR_EXECUTE,
                execute_context
            );

            goto failure;
        }

        if (!database_statement_column_text(
                statement,
                0,
                &loaded_identifier
            ))
        {
            relation_evidence_dao_set_database_error(
                relation_evidence_dao,
                error,
                RELATION_EVIDENCE_DAO_ERROR_READ,
                "Impossible de lire un identifiant associé"
            );

            g_free(
                loaded_identifier
            );

            goto failure;
        }

        if (loaded_identifier == NULL ||
            !g_uuid_string_is_valid(
                loaded_identifier
            ))
        {
            relation_evidence_dao_set_error_literal(
                error,
                RELATION_EVIDENCE_DAO_ERROR_READ,
                "La base contient un identifiant associé invalide."
            );

            g_free(
                loaded_identifier
            );

            goto failure;
        }

        g_ptr_array_add(
            identifiers,
            loaded_identifier
        );
    }

    database_statement_finalize(
        statement
    );

    return identifiers;

failure:

    database_statement_finalize(
        statement
    );

    g_ptr_array_unref(
        identifiers
    );

    return NULL;
}

GQuark relation_evidence_dao_error_quark(void)
{
    return g_quark_from_static_string(
        "relation-evidence-dao-error-quark"
    );
}

RelationEvidenceDao *relation_evidence_dao_new(
    Database *database,
    GError **error
)
{
    RelationEvidenceDao *relation_evidence_dao =
        NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (database == NULL)
    {
        relation_evidence_dao_set_error_literal(
            error,
            RELATION_EVIDENCE_DAO_ERROR_INVALID_ARGUMENT,
            "La connexion Database est obligatoire."
        );

        return NULL;
    }

    relation_evidence_dao =
        g_try_new0(
            RelationEvidenceDao,
            1
        );

    if (relation_evidence_dao == NULL)
    {
        relation_evidence_dao_set_error_literal(
            error,
            RELATION_EVIDENCE_DAO_ERROR_MEMORY,
            "Impossible d'allouer le DAO des associations relation-preuve."
        );

        return NULL;
    }

    relation_evidence_dao->database =
        database;

    return relation_evidence_dao;
}

void relation_evidence_dao_free(
    RelationEvidenceDao *relation_evidence_dao
)
{
    if (relation_evidence_dao == NULL)
    {
        return;
    }

    relation_evidence_dao->database =
        NULL;

    g_free(
        relation_evidence_dao
    );
}

gboolean relation_evidence_dao_link(
    RelationEvidenceDao *relation_evidence_dao,
    const char *relation_identifier,
    const char *evidence_identifier,
    GError **error
)
{
    gboolean relation_exists =
        FALSE;

    gboolean evidence_exists =
        FALSE;

    gboolean association_exists =
        FALSE;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (!relation_evidence_dao_validate_association(
            relation_evidence_dao,
            relation_identifier,
            evidence_identifier,
            error
        ))
    {
        return FALSE;
    }

    if (!relation_evidence_dao_query_exists(
            relation_evidence_dao,
            relation_evidence_dao_relation_exists_sql,
            relation_identifier,
            NULL,
            &relation_exists,
            error
        ))
    {
        return FALSE;
    }

    if (!relation_exists)
    {
        relation_evidence_dao_set_error_literal(
            error,
            RELATION_EVIDENCE_DAO_ERROR_NOT_FOUND,
            "La relation à associer n'existe pas."
        );

        return FALSE;
    }

    if (!relation_evidence_dao_query_exists(
            relation_evidence_dao,
            relation_evidence_dao_evidence_exists_sql,
            evidence_identifier,
            NULL,
            &evidence_exists,
            error
        ))
    {
        return FALSE;
    }

    if (!evidence_exists)
    {
        relation_evidence_dao_set_error_literal(
            error,
            RELATION_EVIDENCE_DAO_ERROR_NOT_FOUND,
            "La preuve à associer n'existe pas."
        );

        return FALSE;
    }

    if (!relation_evidence_dao_query_exists(
            relation_evidence_dao,
            relation_evidence_dao_association_exists_sql,
            relation_identifier,
            evidence_identifier,
            &association_exists,
            error
        ))
    {
        return FALSE;
    }

    if (association_exists)
    {
        relation_evidence_dao_set_error_literal(
            error,
            RELATION_EVIDENCE_DAO_ERROR_CONSTRAINT,
            "Cette relation est déjà associée à cette preuve."
        );

        return FALSE;
    }

    return relation_evidence_dao_execute_pair_statement(
        relation_evidence_dao,
        relation_evidence_dao_link_sql,
        relation_identifier,
        evidence_identifier,
        "Impossible de préparer la création de l'association",
        "Impossible de lier les identifiants de l'association",
        "Impossible de créer l'association",
        error
    );
}

gboolean relation_evidence_dao_unlink(
    RelationEvidenceDao *relation_evidence_dao,
    const char *relation_identifier,
    const char *evidence_identifier,
    GError **error
)
{
    gboolean association_exists =
        FALSE;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (!relation_evidence_dao_validate_association(
            relation_evidence_dao,
            relation_identifier,
            evidence_identifier,
            error
        ))
    {
        return FALSE;
    }

    if (!relation_evidence_dao_query_exists(
            relation_evidence_dao,
            relation_evidence_dao_association_exists_sql,
            relation_identifier,
            evidence_identifier,
            &association_exists,
            error
        ))
    {
        return FALSE;
    }

    if (!association_exists)
    {
        relation_evidence_dao_set_error_literal(
            error,
            RELATION_EVIDENCE_DAO_ERROR_NOT_FOUND,
            "L'association entre la relation et la preuve n'existe pas."
        );

        return FALSE;
    }

    return relation_evidence_dao_execute_pair_statement(
        relation_evidence_dao,
        relation_evidence_dao_unlink_sql,
        relation_identifier,
        evidence_identifier,
        "Impossible de préparer la suppression de l'association",
        "Impossible de lier les identifiants de l'association",
        "Impossible de supprimer l'association",
        error
    );
}

gboolean relation_evidence_dao_exists(
    RelationEvidenceDao *relation_evidence_dao,
    const char *relation_identifier,
    const char *evidence_identifier,
    gboolean *out_exists,
    GError **error
)
{
    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (out_exists == NULL)
    {
        relation_evidence_dao_set_error_literal(
            error,
            RELATION_EVIDENCE_DAO_ERROR_INVALID_ARGUMENT,
            "La destination du résultat d'existence est obligatoire."
        );

        return FALSE;
    }

    if (!relation_evidence_dao_validate_association(
            relation_evidence_dao,
            relation_identifier,
            evidence_identifier,
            error
        ))
    {
        return FALSE;
    }

    return relation_evidence_dao_query_exists(
        relation_evidence_dao,
        relation_evidence_dao_association_exists_sql,
        relation_identifier,
        evidence_identifier,
        out_exists,
        error
    );
}

GPtrArray *relation_evidence_dao_list_evidence_identifiers(
    RelationEvidenceDao *relation_evidence_dao,
    const char *relation_identifier,
    GError **error
)
{
    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (!relation_evidence_dao_validate_list_request(
            relation_evidence_dao,
            relation_identifier,
            "L'identifiant de relation est invalide.",
            error
        ))
    {
        return NULL;
    }

    return relation_evidence_dao_list_identifiers(
        relation_evidence_dao,
        relation_evidence_dao_list_evidence_identifiers_sql,
        relation_identifier,
        "Impossible de préparer la liste des preuves associées",
        "Impossible de lier l'identifiant de relation",
        "Impossible de charger les preuves associées",
        error
    );
}

GPtrArray *relation_evidence_dao_list_relation_identifiers(
    RelationEvidenceDao *relation_evidence_dao,
    const char *evidence_identifier,
    GError **error
)
{
    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (!relation_evidence_dao_validate_list_request(
            relation_evidence_dao,
            evidence_identifier,
            "L'identifiant de preuve est invalide.",
            error
        ))
    {
        return NULL;
    }

    return relation_evidence_dao_list_identifiers(
        relation_evidence_dao,
        relation_evidence_dao_list_relation_identifiers_sql,
        evidence_identifier,
        "Impossible de préparer la liste des relations associées",
        "Impossible de lier l'identifiant de preuve",
        "Impossible de charger les relations associées",
        error
    );
}
