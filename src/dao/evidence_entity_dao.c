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
    "INSERT OR IGNORE INTO preuve_entites"
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

static void evidence_entity_dao_set_database_error(
    EvidenceEntityDao *evidence_entity_dao, GError **error,
    EvidenceEntityDaoError error_code, const char *context);

static gboolean evidence_entity_dao_execute_source_statement(
    EvidenceEntityDao *dao, const char *sql, const char *evidence_identifier,
    const char *entity_identifier, const char *source_kind,
    const char *source_uuid, const char *created_at, GError **error)
{
    DatabaseStatement *statement = database_statement_prepare(dao->database, sql);
    gboolean success = statement != NULL &&
        database_statement_bind_text(statement, 1, evidence_identifier) &&
        database_statement_bind_text(statement, 2, entity_identifier) &&
        database_statement_bind_text(statement, 3, source_kind);
    if (success && source_uuid != NULL)
        success = database_statement_bind_text(statement, 4, source_uuid);
    if (success && created_at != NULL)
        success = database_statement_bind_text(statement, 5, created_at);
    if (success)
        success = database_statement_step(statement) ==
            DATABASE_STATEMENT_STEP_DONE;
    if (!success)
        evidence_entity_dao_set_database_error(dao, error,
            statement == NULL ? EVIDENCE_ENTITY_DAO_ERROR_PREPARE :
                EVIDENCE_ENTITY_DAO_ERROR_EXECUTE,
            "Impossible de modifier la provenance de l'association");
    database_statement_finalize(statement);
    return success;
}

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

    GDateTime *now = NULL;
    char *created_at = NULL;
    gboolean success = FALSE;

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

    now = g_date_time_new_now_utc();
    created_at = now != NULL
        ? g_date_time_format(now, "%Y-%m-%dT%H:%M:%SZ") : NULL;
    if (created_at == NULL)
        evidence_entity_dao_set_error_literal(error,
            EVIDENCE_ENTITY_DAO_ERROR_MEMORY,
            "Impossible de dater l'association preuve-entité.");
    else
        success = evidence_entity_dao_add_source(evidence_entity_dao,
            evidence_identifier, entity_identifier, "manual", NULL,
            created_at, error);
    g_free(created_at);
    g_clear_pointer(&now, g_date_time_unref);
    return success;
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

    return evidence_entity_dao_remove_source(evidence_entity_dao,
        evidence_identifier, entity_identifier, "manual", NULL, NULL, error);
}

gboolean evidence_entity_dao_add_source(
    EvidenceEntityDao *dao, const char *evidence_identifier,
    const char *entity_identifier, const char *source_kind,
    const char *source_uuid, const char *created_at, GError **error)
{
    static const char *insert_sql =
        "INSERT OR IGNORE INTO preuve_entite_sources"
        "(id,preuve_id,entite_id,source_kind,source_uuid,created_at)"
        "VALUES(?,?,?,?,?,?);";
    char *source_identifier = NULL;
    DatabaseStatement *statement = NULL;
    gboolean success = FALSE;
    if (!evidence_entity_dao_validate_association(dao, evidence_identifier,
            entity_identifier, error) ||
        source_kind == NULL || created_at == NULL ||
        (g_strcmp0(source_kind, "eml_observation") == 0 &&
         (source_uuid == NULL || !g_uuid_string_is_valid(source_uuid))))
        return FALSE;
    if (!evidence_entity_dao_execute_pair_statement(dao,
            evidence_entity_dao_link_sql, evidence_identifier,
            entity_identifier, "Impossible de préparer l'association",
            "Impossible de lier l'association",
            "Impossible de matérialiser l'association", error))
        return FALSE;
    source_identifier = g_uuid_string_random();
    statement = database_statement_prepare(dao->database, insert_sql);
    success = statement != NULL &&
        database_statement_bind_text(statement, 1, source_identifier) &&
        database_statement_bind_text(statement, 2, evidence_identifier) &&
        database_statement_bind_text(statement, 3, entity_identifier) &&
        database_statement_bind_text(statement, 4, source_kind);
    if (success && source_uuid != NULL)
        success = database_statement_bind_text(statement, 5, source_uuid);
    if (success)
        success = database_statement_bind_text(statement, 6, created_at) &&
            database_statement_step(statement) == DATABASE_STATEMENT_STEP_DONE;
    if (!success)
        evidence_entity_dao_set_database_error(dao, error,
            statement == NULL ? EVIDENCE_ENTITY_DAO_ERROR_PREPARE :
                EVIDENCE_ENTITY_DAO_ERROR_EXECUTE,
            "Impossible d'enregistrer la provenance de l'association");
    database_statement_finalize(statement);
    g_free(source_identifier);
    return success;
}

gboolean evidence_entity_dao_remove_source(
    EvidenceEntityDao *dao, const char *evidence_identifier,
    const char *entity_identifier, const char *source_kind,
    const char *source_uuid, gboolean *out_link_removed, GError **error)
{
    static const char *delete_source_sql =
        "DELETE FROM preuve_entite_sources WHERE preuve_id=?1 AND entite_id=?2 "
        "AND source_kind=?3 AND COALESCE(source_uuid,'')=COALESCE(?4,'');";
    static const char *count_sql =
        "SELECT COUNT(*) FROM preuve_entite_sources "
        "WHERE preuve_id=?1 AND entite_id=?2;";
    DatabaseStatement *statement = NULL;
    gint64 count = 0;
    gboolean success = FALSE;
    if (out_link_removed != NULL) *out_link_removed = FALSE;
    if (!evidence_entity_dao_validate_association(dao, evidence_identifier,
            entity_identifier, error) || source_kind == NULL)
        return FALSE;
    if (!evidence_entity_dao_execute_source_statement(dao, delete_source_sql,
            evidence_identifier, entity_identifier, source_kind, source_uuid,
            NULL, error))
        return FALSE;
    statement = database_statement_prepare(dao->database, count_sql);
    success = statement != NULL &&
        database_statement_bind_text(statement, 1, evidence_identifier) &&
        database_statement_bind_text(statement, 2, entity_identifier) &&
        database_statement_step(statement) == DATABASE_STATEMENT_STEP_ROW &&
        database_statement_column_int64(statement, 0, &count);
    database_statement_finalize(statement);
    if (!success)
    {
        evidence_entity_dao_set_database_error(dao, error,
            EVIDENCE_ENTITY_DAO_ERROR_EXECUTE,
            "Impossible de vérifier les provenances de l'association");
        return FALSE;
    }
    if (count == 0)
    {
        success = evidence_entity_dao_execute_pair_statement(dao,
            evidence_entity_dao_unlink_sql, evidence_identifier,
            entity_identifier, "Impossible de préparer le détachement",
            "Impossible de lier le détachement",
            "Impossible de supprimer l'association sans provenance", error);
        if (success && out_link_removed != NULL) *out_link_removed = TRUE;
        return success;
    }
    return TRUE;
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

gboolean evidence_entity_dao_add_observation(EvidenceEntityDao *dao,
    const char *evidence_identifier, const char *entity_type,
    const char *value_raw,
    const char *value_normalized, const char *role,
    const char *provenance_kind, const char *source_header,
    guint occurrence, const char *verification_status,
    const char *created_at, char **out_observation_identifier, GError **error)
{
    static const char *sql =
        "INSERT OR IGNORE INTO evidence_entity_observations("
        "id,evidence_id,entity_type,value_raw,value_normalized,role,"
        "provenance_kind,source_header,occurrence,verification_status,"
        "observed_at,integrated_at) VALUES(?,?,?,?,?,?,?,?,?,?,?,?);";
    static const char *find_sql =
        "SELECT id FROM evidence_entity_observations WHERE evidence_id=? "
        "AND entity_type=? AND value_normalized=? AND role=? "
        "AND source_header=? AND occurrence=? AND provenance_kind=? "
        "AND extraction_id IS NULL LIMIT 1;";
    DatabaseStatement *statement = NULL;
    char *new_identifier = NULL;
    gboolean success = FALSE;
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    if (out_observation_identifier != NULL) *out_observation_identifier = NULL;
    if (!evidence_entity_dao_validate_list_request(dao, evidence_identifier,
            "L'identifiant de preuve est invalide.", error) ||
        entity_type == NULL ||
        value_raw == NULL || value_normalized == NULL || role == NULL ||
        source_header == NULL || occurrence == 0 || created_at == NULL)
        return FALSE;
    new_identifier = g_uuid_string_random();
    statement = database_statement_prepare(dao->database, sql);
    if (statement == NULL) goto cleanup;
    success =
        database_statement_bind_text(statement, 1, new_identifier) &&
        database_statement_bind_text(statement, 2, evidence_identifier) &&
        database_statement_bind_text(statement, 3, entity_type) &&
        database_statement_bind_text(statement, 4, value_raw) &&
        database_statement_bind_text(statement, 5, value_normalized) &&
        database_statement_bind_text(statement, 6, role) &&
        database_statement_bind_text(statement, 7, provenance_kind) &&
        database_statement_bind_text(statement, 8, source_header) &&
        database_statement_bind_int64(statement, 9, occurrence) &&
        database_statement_bind_text(statement, 10, verification_status) &&
        database_statement_bind_text(statement, 11, created_at) &&
        database_statement_bind_text(statement, 12, created_at) &&
        database_statement_step(statement) == DATABASE_STATEMENT_STEP_DONE;
    database_statement_finalize(statement); statement = NULL;
    if (!success) goto cleanup;
    statement = database_statement_prepare(dao->database, find_sql);
    success = statement != NULL &&
        database_statement_bind_text(statement, 1, evidence_identifier) &&
        database_statement_bind_text(statement, 2, entity_type) &&
        database_statement_bind_text(statement, 3, value_normalized) &&
        database_statement_bind_text(statement, 4, role) &&
        database_statement_bind_text(statement, 5, source_header) &&
        database_statement_bind_int64(statement, 6, occurrence) &&
        database_statement_bind_text(statement, 7, provenance_kind) &&
        database_statement_step(statement) == DATABASE_STATEMENT_STEP_ROW &&
        database_statement_column_text(statement, 0,
            out_observation_identifier);
cleanup:
    if (!success)
        evidence_entity_dao_set_database_error(dao, error,
            EVIDENCE_ENTITY_DAO_ERROR_EXECUTE,
            "Impossible d’enregistrer l’observation preuve-entité");
    database_statement_finalize(statement);
    g_free(new_identifier);
    return success;
}

gboolean evidence_entity_dao_promote_observation(EvidenceEntityDao *dao,
    const char *observation_identifier, const char *entity_identifier,
    const char *promoted_at, const char *promotion_kind, GError **error)
{
    static const char *sql =
        "UPDATE evidence_entity_observations SET entity_id=?,promoted_at=?,"
        "promotion_kind=? WHERE id=? AND (entity_id IS NULL OR entity_id=?);";
    DatabaseStatement *statement = NULL;
    gboolean success = FALSE;
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    if (dao == NULL || !g_uuid_string_is_valid(observation_identifier) ||
        !g_uuid_string_is_valid(entity_identifier) || promoted_at == NULL ||
        (g_strcmp0(promotion_kind, "created") != 0 &&
         g_strcmp0(promotion_kind, "reused") != 0)) return FALSE;
    statement = database_statement_prepare(dao->database, sql);
    success = statement != NULL &&
        database_statement_bind_text(statement, 1, entity_identifier) &&
        database_statement_bind_text(statement, 2, promoted_at) &&
        database_statement_bind_text(statement, 3, promotion_kind) &&
        database_statement_bind_text(statement, 4, observation_identifier) &&
        database_statement_bind_text(statement, 5, entity_identifier) &&
        database_statement_step(statement) == DATABASE_STATEMENT_STEP_DONE;
    if (!success)
        evidence_entity_dao_set_database_error(dao, error,
            EVIDENCE_ENTITY_DAO_ERROR_EXECUTE,
            "Impossible de promouvoir l’observation");
    database_statement_finalize(statement);
    return success;
}

char *evidence_entity_dao_format_observations(EvidenceEntityDao *dao,
    const char *evidence_identifier, GError **error)
{
    static const char *sql =
        "SELECT COALESCE(o.value_normalized,o.value_raw),o.entity_type,o.role,"
        "o.source_header,o.occurrence,o.provenance_kind,o.verification_status,"
        "o.integrated_at,o.entity_id,o.promotion_kind "
        "FROM evidence_entity_observations o WHERE o.evidence_id=? "
        "ORDER BY o.source_header,o.occurrence,1,o.role;";
    DatabaseStatement *statement = NULL;
    GString *text = NULL;
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);
    if (!evidence_entity_dao_validate_list_request(dao, evidence_identifier,
            "L'identifiant de preuve est invalide.", error)) return NULL;
    statement = database_statement_prepare(dao->database, sql);
    if (statement == NULL ||
        !database_statement_bind_text(statement, 1, evidence_identifier))
        goto failure;
    text = g_string_new(NULL);
    for (;;)
    {
        DatabaseStatementStepResult step = database_statement_step(statement);
        if (step == DATABASE_STATEMENT_STEP_DONE) break;
        if (step != DATABASE_STATEMENT_STEP_ROW) goto failure;
        char *value = NULL, *type = NULL, *role = NULL, *header = NULL;
        char *provenance = NULL, *status = NULL, *date = NULL;
        char *entity = NULL, *promotion = NULL; int64_t occurrence = 0;
        if (!database_statement_column_text(statement, 0, &value) ||
            !database_statement_column_text(statement, 1, &type) ||
            !database_statement_column_text(statement, 2, &role) ||
            !database_statement_column_text(statement, 3, &header) ||
            !database_statement_column_int64(statement, 4, &occurrence) ||
            !database_statement_column_text(statement, 5, &provenance) ||
            !database_statement_column_text(statement, 6, &status) ||
            !database_statement_column_text(statement, 7, &date) ||
            !database_statement_column_text(statement, 8, &entity) ||
            !database_statement_column_text(statement, 9, &promotion))
        { g_free(value); g_free(type); g_free(role); g_free(header);
          g_free(provenance); g_free(status); g_free(date); g_free(entity);
          g_free(promotion); goto failure; }
        g_string_append_printf(text, "%s%s — %s — rôle : %s — origine : %s"
            " #%lld — provenance : %s — validation : %s — date : %s — "
            "graphe : %s%s%s", text->len > 0 ? "\n" : "",
            value, type, role, header, (long long) occurrence, provenance,
            status, date, entity != NULL ? "promue" : "non ajoutée",
            promotion != NULL ? " (" : "", promotion != NULL ? promotion : "");
        if (promotion != NULL) g_string_append_c(text, ')');
        g_free(value); g_free(type); g_free(role); g_free(header);
        g_free(provenance); g_free(status); g_free(date); g_free(entity);
        g_free(promotion);
    }
    database_statement_finalize(statement);
    return g_string_free(text, FALSE);
failure:
    evidence_entity_dao_set_database_error(dao, error,
        EVIDENCE_ENTITY_DAO_ERROR_EXECUTE,
        "Impossible de charger les observations preuve-entité");
    database_statement_finalize(statement);
    if (text != NULL) g_string_free(text, TRUE);
    return NULL;
}

GPtrArray *evidence_entity_dao_list_observations(EvidenceEntityDao *dao,
    const char *evidence_identifier, GError **error)
{
    static const char *sql =
        "SELECT id,COALESCE(value_normalized,value_raw),entity_type,role,"
        "source_header,occurrence,provenance_kind,verification_status,"
        "integrated_at,entity_id,promotion_kind "
        "FROM evidence_entity_observations WHERE evidence_id=? "
        "ORDER BY source_header,occurrence,2,role;";
    DatabaseStatement *statement = NULL;
    GPtrArray *items = NULL;
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);
    if (!evidence_entity_dao_validate_list_request(dao, evidence_identifier,
            "L'identifiant de preuve est invalide.", error)) return NULL;
    statement = database_statement_prepare(dao->database, sql);
    if (statement == NULL ||
        !database_statement_bind_text(statement, 1, evidence_identifier))
        goto failure;
    items = g_ptr_array_new_with_free_func(
        (GDestroyNotify) evidence_observation_free);
    for (;;)
    {
        DatabaseStatementStepResult step = database_statement_step(statement);
        if (step == DATABASE_STATEMENT_STEP_DONE) break;
        if (step != DATABASE_STATEMENT_STEP_ROW) goto failure;
        EvidenceObservation *item = g_new0(EvidenceObservation, 1);
        int64_t occurrence = 0;
        if (!database_statement_column_text(statement, 0, &item->identifier) ||
            !database_statement_column_text(statement, 1, &item->value) ||
            !database_statement_column_text(statement, 2, &item->type_identifier) ||
            !database_statement_column_text(statement, 3, &item->role) ||
            !database_statement_column_text(statement, 4, &item->source_header) ||
            !database_statement_column_int64(statement, 5, &occurrence) ||
            !database_statement_column_text(statement, 6, &item->provenance_kind) ||
            !database_statement_column_text(statement, 7, &item->verification_status) ||
            !database_statement_column_text(statement, 8, &item->integrated_at) ||
            !database_statement_column_text(statement, 9, &item->entity_identifier) ||
            !database_statement_column_text(statement, 10, &item->promotion_kind))
        { evidence_observation_free(item); goto failure; }
        item->occurrence = (guint) occurrence;
        g_ptr_array_add(items, item);
    }
    database_statement_finalize(statement);
    return items;
failure:
    evidence_entity_dao_set_database_error(dao, error,
        EVIDENCE_ENTITY_DAO_ERROR_EXECUTE,
        "Impossible de charger toutes les observations de la preuve");
    database_statement_finalize(statement);
    g_clear_pointer(&items, g_ptr_array_unref);
    return NULL;
}
