/******************************************************************************
 * @file evidence_entity_dao.c
 * @brief Persistance des associations entre preuves et entités.
 ******************************************************************************/

#include "dao/evidence_entity_dao.h"

#include "database/error.h"
#include "database/statement.h"

#include <glib.h>

/**
 * @brief Représentation privée du DAO.
 *
 * La connexion Database est empruntée.
 */
struct EvidenceEntityDao
{
    Database *database;
};

/**
 * @brief Requête vérifiant l'existence d'une preuve.
 */
static const char *const evidence_entity_dao_evidence_exists_sql =
    "SELECT 1 "
    "FROM preuves "
    "WHERE id = ? "
    "LIMIT 1;";

/**
 * @brief Requête vérifiant l'existence d'une entité.
 */
static const char *const evidence_entity_dao_entity_exists_sql =
    "SELECT 1 "
    "FROM entites "
    "WHERE id = ? "
    "LIMIT 1;";

/**
 * @brief Requête vérifiant l'existence d'une association.
 */
static const char *const evidence_entity_dao_association_exists_sql =
    "SELECT 1 "
    "FROM preuve_entites "
    "WHERE preuve_id = ? "
    "AND entite_id = ? "
    "LIMIT 1;";

/**
 * @brief Requête créant une association.
 */
static const char *const evidence_entity_dao_link_sql =
    "INSERT INTO preuve_entites"
    "("
    "    preuve_id,"
    "    entite_id"
    ")"
    "VALUES"
    "("
    "    ?,"
    "    ?"
    ");";

/**
 * @brief Requête supprimant une association.
 */
static const char *const evidence_entity_dao_unlink_sql =
    "DELETE FROM preuve_entites "
    "WHERE preuve_id = ? "
    "AND entite_id = ?;";

/**
 * @brief Requête listant les entités liées à une preuve.
 */
static const char *const
evidence_entity_dao_list_entity_identifiers_sql =
    "SELECT entite_id "
    "FROM preuve_entites "
    "WHERE preuve_id = ? "
    "ORDER BY entite_id ASC;";

/**
 * @brief Requête listant les preuves liées à une entité.
 */
static const char *const
evidence_entity_dao_list_evidence_identifiers_sql =
    "SELECT preuve_id "
    "FROM preuve_entites "
    "WHERE entite_id = ? "
    "ORDER BY preuve_id ASC;";

/**
 * @brief Enregistre une erreur littérale du DAO.
 */
static void evidence_entity_dao_set_error_literal(
    GError **error,
    EvidenceEntityDaoError error_code,
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
        EVIDENCE_ENTITY_DAO_ERROR,
        error_code,
        message
    );
}

/**
 * @brief Détermine si SQLite signale un schéma incompatible.
 */
static gboolean evidence_entity_dao_is_schema_error(
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
static gboolean evidence_entity_dao_is_constraint_error(
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
static void evidence_entity_dao_set_database_error(
    EvidenceEntityDao *evidence_entity_dao,
    GError **error,
    EvidenceEntityDaoError error_code,
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

    if (evidence_entity_dao != NULL &&
        evidence_entity_dao->database != NULL)
    {
        database_message =
            database_error_get_message(
                evidence_entity_dao->database
            );
    }

    if (error_code ==
            EVIDENCE_ENTITY_DAO_ERROR_PREPARE &&
        evidence_entity_dao_is_schema_error(
            database_message
        ))
    {
        error_code =
            EVIDENCE_ENTITY_DAO_ERROR_SCHEMA;
    }

    if (database_message != NULL &&
        database_message[0] != '\0')
    {
        g_set_error(
            error,
            EVIDENCE_ENTITY_DAO_ERROR,
            error_code,
            "%s : %s",
            context,
            database_message
        );

        return;
    }

    g_set_error_literal(
        error,
        EVIDENCE_ENTITY_DAO_ERROR,
        error_code,
        context
    );
}

/**
 * @brief Vérifie le DAO et les deux UUID d'une association.
 */
static gboolean evidence_entity_dao_validate_association(
    EvidenceEntityDao *evidence_entity_dao,
    const char *evidence_identifier,
    const char *entity_identifier,
    GError **error
)
{
    if (evidence_entity_dao == NULL ||
        evidence_entity_dao->database == NULL)
    {
        evidence_entity_dao_set_error_literal(
            error,
            EVIDENCE_ENTITY_DAO_ERROR_INVALID_ARGUMENT,
            "Le DAO des associations preuve-entité est invalide."
        );

        return FALSE;
    }

    if (evidence_identifier == NULL ||
        !g_uuid_string_is_valid(
            evidence_identifier
        ))
    {
        evidence_entity_dao_set_error_literal(
            error,
            EVIDENCE_ENTITY_DAO_ERROR_INVALID_ARGUMENT,
            "L'identifiant de preuve est invalide."
        );

        return FALSE;
    }

    if (entity_identifier == NULL ||
        !g_uuid_string_is_valid(
            entity_identifier
        ))
    {
        evidence_entity_dao_set_error_literal(
            error,
            EVIDENCE_ENTITY_DAO_ERROR_INVALID_ARGUMENT,
            "L'identifiant d'entité est invalide."
        );

        return FALSE;
    }

    return TRUE;
}

/**
 * @brief Vérifie le DAO et un UUID utilisé pour une liste.
 */
static gboolean evidence_entity_dao_validate_list_request(
    EvidenceEntityDao *evidence_entity_dao,
    const char *identifier,
    const char *invalid_identifier_message,
    GError **error
)
{
    if (evidence_entity_dao == NULL ||
        evidence_entity_dao->database == NULL)
    {
        evidence_entity_dao_set_error_literal(
            error,
            EVIDENCE_ENTITY_DAO_ERROR_INVALID_ARGUMENT,
            "Le DAO des associations preuve-entité est invalide."
        );

        return FALSE;
    }

    if (identifier == NULL ||
        !g_uuid_string_is_valid(
            identifier
        ))
    {
        evidence_entity_dao_set_error_literal(
            error,
            EVIDENCE_ENTITY_DAO_ERROR_INVALID_ARGUMENT,
            invalid_identifier_message
        );

        return FALSE;
    }

    return TRUE;
}

/**
 * @brief Exécute une requête SELECT 1 avec un ou deux paramètres.
 */
static gboolean evidence_entity_dao_query_exists(
    EvidenceEntityDao *evidence_entity_dao,
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

    if (evidence_entity_dao == NULL ||
        evidence_entity_dao->database == NULL ||
        sql == NULL ||
        first_value == NULL ||
        out_exists == NULL)
    {
        evidence_entity_dao_set_error_literal(
            error,
            EVIDENCE_ENTITY_DAO_ERROR_INVALID_ARGUMENT,
            "Les paramètres de la recherche d'existence sont invalides."
        );

        return FALSE;
    }

    *out_exists =
        FALSE;

    statement =
        database_statement_prepare(
            evidence_entity_dao->database,
            sql
        );

    if (statement == NULL)
    {
        evidence_entity_dao_set_database_error(
            evidence_entity_dao,
            error,
            EVIDENCE_ENTITY_DAO_ERROR_PREPARE,
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
        evidence_entity_dao_set_database_error(
            evidence_entity_dao,
            error,
            EVIDENCE_ENTITY_DAO_ERROR_BIND,
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
        evidence_entity_dao_set_database_error(
            evidence_entity_dao,
            error,
            EVIDENCE_ENTITY_DAO_ERROR_BIND,
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
        evidence_entity_dao_set_database_error(
            evidence_entity_dao,
            error,
            EVIDENCE_ENTITY_DAO_ERROR_EXECUTE,
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
static gboolean evidence_entity_dao_execute_pair_statement(
    EvidenceEntityDao *evidence_entity_dao,
    const char *sql,
    const char *evidence_identifier,
    const char *entity_identifier,
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
            evidence_entity_dao->database,
            sql
        );

    if (statement == NULL)
    {
        evidence_entity_dao_set_database_error(
            evidence_entity_dao,
            error,
            EVIDENCE_ENTITY_DAO_ERROR_PREPARE,
            prepare_context
        );

        goto cleanup;
    }

    if (!database_statement_bind_text(
            statement,
            1,
            evidence_identifier
        ) ||
        !database_statement_bind_text(
            statement,
            2,
            entity_identifier
        ))
    {
        evidence_entity_dao_set_database_error(
            evidence_entity_dao,
            error,
            EVIDENCE_ENTITY_DAO_ERROR_BIND,
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
                evidence_entity_dao->database
            );

        if (evidence_entity_dao_is_constraint_error(
                database_message
            ))
        {
            evidence_entity_dao_set_database_error(
                evidence_entity_dao,
                error,
                EVIDENCE_ENTITY_DAO_ERROR_CONSTRAINT,
                execute_context
            );
        }
        else
        {
            evidence_entity_dao_set_database_error(
                evidence_entity_dao,
                error,
                EVIDENCE_ENTITY_DAO_ERROR_EXECUTE,
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
static GPtrArray *evidence_entity_dao_list_identifiers(
    EvidenceEntityDao *evidence_entity_dao,
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
        evidence_entity_dao_set_error_literal(
            error,
            EVIDENCE_ENTITY_DAO_ERROR_MEMORY,
            "Impossible d'allouer la liste d'identifiants."
        );

        return NULL;
    }

    statement =
        database_statement_prepare(
            evidence_entity_dao->database,
            sql
        );

    if (statement == NULL)
    {
        evidence_entity_dao_set_database_error(
            evidence_entity_dao,
            error,
            EVIDENCE_ENTITY_DAO_ERROR_PREPARE,
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
        evidence_entity_dao_set_database_error(
            evidence_entity_dao,
            error,
            EVIDENCE_ENTITY_DAO_ERROR_BIND,
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
            evidence_entity_dao_set_database_error(
                evidence_entity_dao,
                error,
                EVIDENCE_ENTITY_DAO_ERROR_EXECUTE,
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
            evidence_entity_dao_set_database_error(
                evidence_entity_dao,
                error,
                EVIDENCE_ENTITY_DAO_ERROR_READ,
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
            evidence_entity_dao_set_error_literal(
                error,
                EVIDENCE_ENTITY_DAO_ERROR_READ,
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

GQuark evidence_entity_dao_error_quark(void)
{
    return g_quark_from_static_string(
        "evidence-entity-dao-error-quark"
    );
}

EvidenceEntityDao *evidence_entity_dao_new(
    Database *database,
    GError **error
)
{
    EvidenceEntityDao *evidence_entity_dao =
        NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (database == NULL)
    {
        evidence_entity_dao_set_error_literal(
            error,
            EVIDENCE_ENTITY_DAO_ERROR_INVALID_ARGUMENT,
            "La connexion Database est obligatoire."
        );

        return NULL;
    }

    evidence_entity_dao =
        g_try_new0(
            EvidenceEntityDao,
            1
        );

    if (evidence_entity_dao == NULL)
    {
        evidence_entity_dao_set_error_literal(
            error,
            EVIDENCE_ENTITY_DAO_ERROR_MEMORY,
            "Impossible d'allouer le DAO des associations preuve-entité."
        );

        return NULL;
    }

    evidence_entity_dao->database =
        database;

    return evidence_entity_dao;
}

void evidence_entity_dao_free(
    EvidenceEntityDao *evidence_entity_dao
)
{
    if (evidence_entity_dao == NULL)
    {
        return;
    }

    evidence_entity_dao->database =
        NULL;

    g_free(
        evidence_entity_dao
    );
}

gboolean evidence_entity_dao_link(
    EvidenceEntityDao *evidence_entity_dao,
    const char *evidence_identifier,
    const char *entity_identifier,
    GError **error
)
{
    gboolean evidence_exists =
        FALSE;

    gboolean entity_exists =
        FALSE;

    gboolean association_exists =
        FALSE;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (!evidence_entity_dao_validate_association(
            evidence_entity_dao,
            evidence_identifier,
            entity_identifier,
            error
        ))
    {
        return FALSE;
    }

    if (!evidence_entity_dao_query_exists(
            evidence_entity_dao,
            evidence_entity_dao_evidence_exists_sql,
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
        evidence_entity_dao_set_error_literal(
            error,
            EVIDENCE_ENTITY_DAO_ERROR_NOT_FOUND,
            "La preuve à associer n'existe pas."
        );

        return FALSE;
    }

    if (!evidence_entity_dao_query_exists(
            evidence_entity_dao,
            evidence_entity_dao_entity_exists_sql,
            entity_identifier,
            NULL,
            &entity_exists,
            error
        ))
    {
        return FALSE;
    }

    if (!entity_exists)
    {
        evidence_entity_dao_set_error_literal(
            error,
            EVIDENCE_ENTITY_DAO_ERROR_NOT_FOUND,
            "L'entité à associer n'existe pas."
        );

        return FALSE;
    }

    if (!evidence_entity_dao_query_exists(
            evidence_entity_dao,
            evidence_entity_dao_association_exists_sql,
            evidence_identifier,
            entity_identifier,
            &association_exists,
            error
        ))
    {
        return FALSE;
    }

    if (association_exists)
    {
        evidence_entity_dao_set_error_literal(
            error,
            EVIDENCE_ENTITY_DAO_ERROR_CONSTRAINT,
            "Cette preuve est déjà associée à cette entité."
        );

        return FALSE;
    }

    return evidence_entity_dao_execute_pair_statement(
        evidence_entity_dao,
        evidence_entity_dao_link_sql,
        evidence_identifier,
        entity_identifier,
        "Impossible de préparer la création de l'association",
        "Impossible de lier les identifiants de l'association",
        "Impossible de créer l'association",
        error
    );
}

gboolean evidence_entity_dao_unlink(
    EvidenceEntityDao *evidence_entity_dao,
    const char *evidence_identifier,
    const char *entity_identifier,
    GError **error
)
{
    gboolean association_exists =
        FALSE;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (!evidence_entity_dao_validate_association(
            evidence_entity_dao,
            evidence_identifier,
            entity_identifier,
            error
        ))
    {
        return FALSE;
    }

    if (!evidence_entity_dao_query_exists(
            evidence_entity_dao,
            evidence_entity_dao_association_exists_sql,
            evidence_identifier,
            entity_identifier,
            &association_exists,
            error
        ))
    {
        return FALSE;
    }

    if (!association_exists)
    {
        evidence_entity_dao_set_error_literal(
            error,
            EVIDENCE_ENTITY_DAO_ERROR_NOT_FOUND,
            "L'association entre la preuve et l'entité n'existe pas."
        );

        return FALSE;
    }

    return evidence_entity_dao_execute_pair_statement(
        evidence_entity_dao,
        evidence_entity_dao_unlink_sql,
        evidence_identifier,
        entity_identifier,
        "Impossible de préparer la suppression de l'association",
        "Impossible de lier les identifiants de l'association",
        "Impossible de supprimer l'association",
        error
    );
}

gboolean evidence_entity_dao_exists(
    EvidenceEntityDao *evidence_entity_dao,
    const char *evidence_identifier,
    const char *entity_identifier,
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
        evidence_entity_dao_set_error_literal(
            error,
            EVIDENCE_ENTITY_DAO_ERROR_INVALID_ARGUMENT,
            "La destination du résultat d'existence est obligatoire."
        );

        return FALSE;
    }

    if (!evidence_entity_dao_validate_association(
            evidence_entity_dao,
            evidence_identifier,
            entity_identifier,
            error
        ))
    {
        return FALSE;
    }

    return evidence_entity_dao_query_exists(
        evidence_entity_dao,
        evidence_entity_dao_association_exists_sql,
        evidence_identifier,
        entity_identifier,
        out_exists,
        error
    );
}

GPtrArray *evidence_entity_dao_list_entity_identifiers(
    EvidenceEntityDao *evidence_entity_dao,
    const char *evidence_identifier,
    GError **error
)
{
    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (!evidence_entity_dao_validate_list_request(
            evidence_entity_dao,
            evidence_identifier,
            "L'identifiant de preuve est invalide.",
            error
        ))
    {
        return NULL;
    }

    return evidence_entity_dao_list_identifiers(
        evidence_entity_dao,
        evidence_entity_dao_list_entity_identifiers_sql,
        evidence_identifier,
        "Impossible de préparer la liste des entités associées",
        "Impossible de lier l'identifiant de preuve",
        "Impossible de charger les entités associées",
        error
    );
}

GPtrArray *evidence_entity_dao_list_evidence_identifiers(
    EvidenceEntityDao *evidence_entity_dao,
    const char *entity_identifier,
    GError **error
)
{
    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (!evidence_entity_dao_validate_list_request(
            evidence_entity_dao,
            entity_identifier,
            "L'identifiant d'entité est invalide.",
            error
        ))
    {
        return NULL;
    }

    return evidence_entity_dao_list_identifiers(
        evidence_entity_dao,
        evidence_entity_dao_list_evidence_identifiers_sql,
        entity_identifier,
        "Impossible de préparer la liste des preuves associées",
        "Impossible de lier l'identifiant d'entité",
        "Impossible de charger les preuves associées",
        error
    );
}
