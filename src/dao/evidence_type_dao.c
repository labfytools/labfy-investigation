/******************************************************************************
 * @file evidence_type_dao.c
 * @brief Lecture des types de preuve depuis SQLite.
 ******************************************************************************/

#include "dao/evidence_type_dao.h"

#include "database/error.h"
#include "database/statement.h"
#include "models/evidence_type.h"

#include <stdint.h>

#include <glib.h>

/**
 * @brief Requête listant les types de preuve.
 */
static const char *const evidence_type_dao_list_all_sql =
    "SELECT "
    "    id, "
    "    code, "
    "    label, "
    "    description "
    "FROM types_preuve "
    "ORDER BY id ASC;";

/**
 * @struct EvidenceTypeDao
 * @brief Représentation interne du DAO.
 */
struct EvidenceTypeDao
{
    Database *database;
};

/**
 * @brief Libère un EvidenceType contenu dans un GPtrArray.
 */
static void evidence_type_dao_destroy_type(
    gpointer user_data
)
{
    EvidenceType *evidence_type = user_data;

    evidence_type_free(
        evidence_type
    );
}

/**
 * @brief Détermine si un message SQLite indique un schéma incompatible.
 */
static gboolean evidence_type_dao_is_schema_error(
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
 * @brief Enregistre une erreur de préparation SQL.
 */
static void evidence_type_dao_set_prepare_error(
    EvidenceTypeDao *evidence_type_dao,
    GError **error
)
{
    const char *database_message = NULL;
    EvidenceTypeDaoError error_code =
        EVIDENCE_TYPE_DAO_ERROR_PREPARE;

    if (error == NULL ||
        *error != NULL)
    {
        return;
    }

    if (evidence_type_dao != NULL)
    {
        database_message =
            database_error_get_message(
                evidence_type_dao->database
            );
    }

    if (evidence_type_dao_is_schema_error(
            database_message
        ))
    {
        error_code =
            EVIDENCE_TYPE_DAO_ERROR_SCHEMA;
    }

    g_set_error(
        error,
        EVIDENCE_TYPE_DAO_ERROR,
        error_code,
        "Impossible de préparer la lecture des types de preuve : %s",
        database_message != NULL
            ? database_message
            : "erreur Database inconnue"
    );
}

/**
 * @brief Enregistre une erreur de lecture SQLite.
 */
static void evidence_type_dao_set_read_error(
    EvidenceTypeDao *evidence_type_dao,
    GError **error,
    const char *context
)
{
    const char *database_message = NULL;

    if (error == NULL ||
        *error != NULL)
    {
        return;
    }

    if (evidence_type_dao != NULL)
    {
        database_message =
            database_error_get_message(
                evidence_type_dao->database
            );
    }

    g_set_error(
        error,
        EVIDENCE_TYPE_DAO_ERROR,
        EVIDENCE_TYPE_DAO_ERROR_READ,
        "%s : %s",
        context != NULL
            ? context
            : "Impossible de lire un type de preuve",
        database_message != NULL
            ? database_message
            : "donnée SQLite invalide"
    );
}

/**
 * @brief Construit un EvidenceType depuis la ligne SQLite courante.
 */
static EvidenceType *evidence_type_dao_read_current_row(
    EvidenceTypeDao *evidence_type_dao,
    DatabaseStatement *statement,
    GError **error
)
{
    EvidenceType *evidence_type = NULL;

    int64_t identifier = 0;

    char *code = NULL;
    char *label = NULL;
    char *description = NULL;

    GError *model_error = NULL;

    if (evidence_type_dao == NULL ||
        statement == NULL)
    {
        g_set_error_literal(
            error,
            EVIDENCE_TYPE_DAO_ERROR,
            EVIDENCE_TYPE_DAO_ERROR_INVALID_ARGUMENT,
            "La ligne du type de preuve ne peut pas être lue."
        );

        return NULL;
    }

    if (!database_statement_column_int64(
            statement,
            0,
            &identifier
        ))
    {
        evidence_type_dao_set_read_error(
            evidence_type_dao,
            error,
            "Impossible de lire l’identifiant du type de preuve"
        );

        goto cleanup;
    }

    if (!database_statement_column_text(
            statement,
            1,
            &code
        ))
    {
        evidence_type_dao_set_read_error(
            evidence_type_dao,
            error,
            "Impossible de lire le code du type de preuve"
        );

        goto cleanup;
    }

    if (!database_statement_column_text(
            statement,
            2,
            &label
        ))
    {
        evidence_type_dao_set_read_error(
            evidence_type_dao,
            error,
            "Impossible de lire le libellé du type de preuve"
        );

        goto cleanup;
    }

    if (!database_statement_column_text(
            statement,
            3,
            &description
        ))
    {
        evidence_type_dao_set_read_error(
            evidence_type_dao,
            error,
            "Impossible de lire la description du type de preuve"
        );

        goto cleanup;
    }

    evidence_type =
        evidence_type_new(
            (gint64) identifier,
            code,
            label,
            description,
            &model_error
        );

    if (evidence_type == NULL)
    {
        g_set_error(
            error,
            EVIDENCE_TYPE_DAO_ERROR,
            EVIDENCE_TYPE_DAO_ERROR_MODEL,
            "Le type de preuve lu depuis SQLite est invalide : %s",
            model_error != NULL
                ? model_error->message
                : "erreur de modèle inconnue"
        );
    }

cleanup:

    g_clear_error(
        &model_error
    );

    g_free(
        description
    );

    g_free(
        label
    );

    g_free(
        code
    );

    return evidence_type;
}

GQuark evidence_type_dao_error_quark(void)
{
    return g_quark_from_static_string(
        "evidence-type-dao-error-quark"
    );
}

EvidenceTypeDao *evidence_type_dao_new(
    Database *database,
    GError **error
)
{
    EvidenceTypeDao *evidence_type_dao = NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (database == NULL)
    {
        g_set_error_literal(
            error,
            EVIDENCE_TYPE_DAO_ERROR,
            EVIDENCE_TYPE_DAO_ERROR_INVALID_ARGUMENT,
            "La connexion Database est obligatoire."
        );

        return NULL;
    }

    evidence_type_dao =
        g_try_new0(
            EvidenceTypeDao,
            1
        );

    if (evidence_type_dao == NULL)
    {
        g_set_error_literal(
            error,
            EVIDENCE_TYPE_DAO_ERROR,
            EVIDENCE_TYPE_DAO_ERROR_MEMORY,
            "Impossible d’allouer le DAO des types de preuve."
        );

        return NULL;
    }

    /*
     * La connexion reste possédée par la session ou le code appelant.
     */
    evidence_type_dao->database =
        database;

    return evidence_type_dao;
}

void evidence_type_dao_free(
    EvidenceTypeDao *evidence_type_dao
)
{
    if (evidence_type_dao == NULL)
    {
        return;
    }

    /*
     * Database est empruntée et ne doit pas être fermée ici.
     */
    evidence_type_dao->database = NULL;

    g_free(
        evidence_type_dao
    );
}

GPtrArray *evidence_type_dao_list_all(
    EvidenceTypeDao *evidence_type_dao,
    GError **error
)
{
    DatabaseStatement *statement = NULL;

    GPtrArray *evidence_types = NULL;

    DatabaseStatementStepResult step_result =
        DATABASE_STATEMENT_STEP_ERROR;

    gboolean success = FALSE;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (evidence_type_dao == NULL ||
        evidence_type_dao->database == NULL)
    {
        g_set_error_literal(
            error,
            EVIDENCE_TYPE_DAO_ERROR,
            EVIDENCE_TYPE_DAO_ERROR_INVALID_ARGUMENT,
            "Le DAO des types de preuve est invalide."
        );

        return NULL;
    }

    evidence_types =
        g_ptr_array_new_with_free_func(
            evidence_type_dao_destroy_type
        );

    if (evidence_types == NULL)
    {
        g_set_error_literal(
            error,
            EVIDENCE_TYPE_DAO_ERROR,
            EVIDENCE_TYPE_DAO_ERROR_MEMORY,
            "Impossible d’allouer la liste des types de preuve."
        );

        return NULL;
    }

    statement =
        database_statement_prepare(
            evidence_type_dao->database,
            evidence_type_dao_list_all_sql
        );

    if (statement == NULL)
    {
        evidence_type_dao_set_prepare_error(
            evidence_type_dao,
            error
        );

        goto cleanup;
    }

    while (TRUE)
    {
        EvidenceType *evidence_type = NULL;

        step_result =
            database_statement_step(
                statement
            );

        if (step_result ==
            DATABASE_STATEMENT_STEP_DONE)
        {
            success = TRUE;
            break;
        }

        if (step_result ==
            DATABASE_STATEMENT_STEP_ERROR)
        {
            evidence_type_dao_set_read_error(
                evidence_type_dao,
                error,
                "Impossible de parcourir les types de preuve"
            );

            break;
        }

        evidence_type =
            evidence_type_dao_read_current_row(
                evidence_type_dao,
                statement,
                error
            );

        if (evidence_type == NULL)
        {
            break;
        }

        g_ptr_array_add(
            evidence_types,
            evidence_type
        );
    }

cleanup:

    database_statement_finalize(
        statement
    );

    if (!success)
    {
        g_ptr_array_unref(
            evidence_types
        );

        return NULL;
    }

    return evidence_types;
}
