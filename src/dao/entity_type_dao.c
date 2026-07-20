/******************************************************************************
 * @file entity_type_dao.c
 * @brief Lecture des types d'entité depuis SQLite.
 ******************************************************************************/

#include "dao/entity_type_dao.h"

#include "database/error.h"
#include "database/statement.h"
#include "models/entity_type.h"

#include <glib.h>
#include <stdint.h>

/**
 * @brief Requête listant les types d'entité.
 */
static const char *const entity_type_dao_list_all_sql =
    "SELECT "
    "    id, "
    "    code, "
    "    label, "
    "    description "
    "FROM types_entite "
    "ORDER BY id ASC;";

/**
 * @struct EntityTypeDao
 * @brief Représentation interne du DAO.
 */
struct EntityTypeDao
{
    Database *database;
};

/**
 * @brief Libère un EntityType contenu dans un GPtrArray.
 */
static void entity_type_dao_destroy_type(
    gpointer user_data
)
{
    EntityType *entity_type =
        user_data;

    entity_type_free(
        entity_type
    );
}

/**
 * @brief Détermine si un message SQLite indique un schéma incompatible.
 */
static gboolean entity_type_dao_is_schema_error(
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
static void entity_type_dao_set_prepare_error(
    EntityTypeDao *entity_type_dao,
    GError **error
)
{
    const char *database_message =
        NULL;

    EntityTypeDaoError error_code =
        ENTITY_TYPE_DAO_ERROR_PREPARE;

    if (error == NULL ||
        *error != NULL)
    {
        return;
    }

    if (entity_type_dao != NULL)
    {
        database_message =
            database_error_get_message(
                entity_type_dao->database
            );
    }

    if (entity_type_dao_is_schema_error(
            database_message
        ))
    {
        error_code =
            ENTITY_TYPE_DAO_ERROR_SCHEMA;
    }

    g_set_error(
        error,
        ENTITY_TYPE_DAO_ERROR,
        error_code,
        "Impossible de préparer la lecture "
        "des types d'entité : %s",
        database_message != NULL
            ? database_message
            : "erreur Database inconnue"
    );
}

/**
 * @brief Enregistre une erreur de lecture SQLite.
 */
static void entity_type_dao_set_read_error(
    EntityTypeDao *entity_type_dao,
    GError **error,
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

    if (entity_type_dao != NULL)
    {
        database_message =
            database_error_get_message(
                entity_type_dao->database
            );
    }

    g_set_error(
        error,
        ENTITY_TYPE_DAO_ERROR,
        ENTITY_TYPE_DAO_ERROR_READ,
        "%s : %s",
        context != NULL
            ? context
            : "Impossible de lire un type d'entité",
        database_message != NULL
            ? database_message
            : "donnée SQLite invalide"
    );
}

/**
 * @brief Construit un EntityType depuis la ligne SQLite courante.
 */
static EntityType *entity_type_dao_read_current_row(
    EntityTypeDao *entity_type_dao,
    DatabaseStatement *statement,
    GError **error
)
{
    EntityType *entity_type =
        NULL;

    int64_t identifier =
        0;

    char *code =
        NULL;

    char *label =
        NULL;

    char *description =
        NULL;

    GError *model_error =
        NULL;

    if (entity_type_dao == NULL ||
        statement == NULL)
    {
        g_set_error_literal(
            error,
            ENTITY_TYPE_DAO_ERROR,
            ENTITY_TYPE_DAO_ERROR_INVALID_ARGUMENT,
            "La ligne du type d'entité ne peut pas être lue."
        );

        return NULL;
    }

    if (!database_statement_column_int64(
            statement,
            0,
            &identifier
        ))
    {
        entity_type_dao_set_read_error(
            entity_type_dao,
            error,
            "Impossible de lire l'identifiant du type d'entité"
        );

        goto cleanup;
    }

    if (!database_statement_column_text(
            statement,
            1,
            &code
        ))
    {
        entity_type_dao_set_read_error(
            entity_type_dao,
            error,
            "Impossible de lire le code du type d'entité"
        );

        goto cleanup;
    }

    if (!database_statement_column_text(
            statement,
            2,
            &label
        ))
    {
        entity_type_dao_set_read_error(
            entity_type_dao,
            error,
            "Impossible de lire le libellé du type d'entité"
        );

        goto cleanup;
    }

    if (!database_statement_column_text(
            statement,
            3,
            &description
        ))
    {
        entity_type_dao_set_read_error(
            entity_type_dao,
            error,
            "Impossible de lire la description du type d'entité"
        );

        goto cleanup;
    }

    entity_type =
        entity_type_new(
            (gint64) identifier,
            code,
            label,
            description,
            &model_error
        );

    if (entity_type == NULL)
    {
        g_set_error(
            error,
            ENTITY_TYPE_DAO_ERROR,
            ENTITY_TYPE_DAO_ERROR_MODEL,
            "Le type d'entité lu depuis SQLite est invalide : %s",
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

    return entity_type;
}

GQuark entity_type_dao_error_quark(void)
{
    return g_quark_from_static_string(
        "entity-type-dao-error-quark"
    );
}

EntityTypeDao *entity_type_dao_new(
    Database *database,
    GError **error
)
{
    EntityTypeDao *entity_type_dao =
        NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (database == NULL)
    {
        g_set_error_literal(
            error,
            ENTITY_TYPE_DAO_ERROR,
            ENTITY_TYPE_DAO_ERROR_INVALID_ARGUMENT,
            "La connexion Database est obligatoire."
        );

        return NULL;
    }

    entity_type_dao =
        g_try_new0(
            EntityTypeDao,
            1
        );

    if (entity_type_dao == NULL)
    {
        g_set_error_literal(
            error,
            ENTITY_TYPE_DAO_ERROR,
            ENTITY_TYPE_DAO_ERROR_MEMORY,
            "Impossible d'allouer le DAO des types d'entité."
        );

        return NULL;
    }

    /*
     * La connexion reste possédée par la session ou le code appelant.
     */
    entity_type_dao->database =
        database;

    return entity_type_dao;
}

void entity_type_dao_free(
    EntityTypeDao *entity_type_dao
)
{
    if (entity_type_dao == NULL)
    {
        return;
    }

    /*
     * Database est empruntée et ne doit pas être fermée ici.
     */
    entity_type_dao->database =
        NULL;

    g_free(
        entity_type_dao
    );
}

GPtrArray *entity_type_dao_list_all(
    EntityTypeDao *entity_type_dao,
    GError **error
)
{
    DatabaseStatement *statement =
        NULL;

    GPtrArray *entity_types =
        NULL;

    DatabaseStatementStepResult step_result =
        DATABASE_STATEMENT_STEP_ERROR;

    gboolean success =
        FALSE;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (entity_type_dao == NULL ||
        entity_type_dao->database == NULL)
    {
        g_set_error_literal(
            error,
            ENTITY_TYPE_DAO_ERROR,
            ENTITY_TYPE_DAO_ERROR_INVALID_ARGUMENT,
            "Le DAO des types d'entité est invalide."
        );

        return NULL;
    }

    entity_types =
        g_ptr_array_new_with_free_func(
            entity_type_dao_destroy_type
        );

    if (entity_types == NULL)
    {
        g_set_error_literal(
            error,
            ENTITY_TYPE_DAO_ERROR,
            ENTITY_TYPE_DAO_ERROR_MEMORY,
            "Impossible d'allouer la liste des types d'entité."
        );

        return NULL;
    }

    statement =
        database_statement_prepare(
            entity_type_dao->database,
            entity_type_dao_list_all_sql
        );

    if (statement == NULL)
    {
        entity_type_dao_set_prepare_error(
            entity_type_dao,
            error
        );

        goto cleanup;
    }

    while (TRUE)
    {
        EntityType *entity_type =
            NULL;

        step_result =
            database_statement_step(
                statement
            );

        if (step_result ==
            DATABASE_STATEMENT_STEP_DONE)
        {
            success =
                TRUE;

            break;
        }

        if (step_result ==
            DATABASE_STATEMENT_STEP_ERROR)
        {
            entity_type_dao_set_read_error(
                entity_type_dao,
                error,
                "Impossible de parcourir les types d'entité"
            );

            break;
        }

        entity_type =
            entity_type_dao_read_current_row(
                entity_type_dao,
                statement,
                error
            );

        if (entity_type == NULL)
        {
            break;
        }

        g_ptr_array_add(
            entity_types,
            entity_type
        );
    }

cleanup:

    database_statement_finalize(
        statement
    );

    if (!success)
    {
        g_ptr_array_unref(
            entity_types
        );

        return NULL;
    }

    return entity_types;
}
