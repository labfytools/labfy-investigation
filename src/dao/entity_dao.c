/******************************************************************************
 * @file entity_dao.c
 * @brief Persistance des entités OSINT dans SQLite.
 ******************************************************************************/

#include "dao/entity_dao.h"

#include "database/error.h"
#include "database/statement.h"

#include <glib.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief Représentation privée du DAO.
 *
 * La connexion Database est empruntée.
 */
struct EntityDao
{
    Database *database;
};

/**
 * @brief Requête d'insertion d'une entité.
 */
static const char *const entity_dao_insert_sql =
    "INSERT INTO entites"
    "("
    "    id,"
    "    type_id,"
    "    valeur,"
    "    label,"
    "    description,"
    "    confiance,"
    "    created_at,"
    "    updated_at,"
    "    status"
    ")"
    "VALUES"
    "("
    "    ?,"
    "    ("
    "        SELECT id "
    "        FROM types_entite "
    "        WHERE code = ?"
    "    ),"
    "    ?,"
    "    ?,"
    "    ?,"
    "    ?,"
    "    ?,"
    "    ?,"
    "    ?"
    ");";

/**
 * @brief Requête de recherche par UUID.
 */
static const char *const entity_dao_find_by_identifier_sql =
    "SELECT "
    "    entites.id,"
    "    types_entite.code,"
    "    entites.valeur,"
    "    entites.label,"
    "    entites.description,"
    "    entites.confiance,"
    "    entites.created_at,"
    "    entites.updated_at,"
    "    entites.status "
    "FROM entites "
    "LEFT JOIN types_entite "
    "ON types_entite.id = entites.type_id "
    "WHERE entites.id = ?;";

/**
 * @brief Requête de chargement de toutes les entités.
 */
static const char *const entity_dao_list_all_sql =
    "SELECT "
    "    entites.id,"
    "    types_entite.code,"
    "    entites.valeur,"
    "    entites.label,"
    "    entites.description,"
    "    entites.confiance,"
    "    entites.created_at,"
    "    entites.updated_at,"
    "    entites.status "
    "FROM entites "
    "LEFT JOIN types_entite "
    "ON types_entite.id = entites.type_id "
    "ORDER BY "
    "    entites.created_at ASC,"
    "    entites.id ASC;";

/**
 * @brief Requête de comptage.
 */
static const char *const entity_dao_count_sql =
    "SELECT COUNT(*) "
    "FROM entites;";

/**
 * @brief Requête de détection d'un UUID existant.
 */
static const char *const entity_dao_identifier_exists_sql =
    "SELECT 1 "
    "FROM entites "
    "WHERE id = ? "
    "LIMIT 1;";

/**
 * @brief Requête de détection d'un type.
 */
static const char *const entity_dao_type_exists_sql =
    "SELECT 1 "
    "FROM types_entite "
    "WHERE code = ? "
    "LIMIT 1;";

/**
 * @brief Requête de détection du couple type et valeur.
 */
static const char *const entity_dao_type_value_exists_sql =
    "SELECT 1 "
    "FROM entites "
    "INNER JOIN types_entite "
    "ON types_entite.id = entites.type_id "
    "WHERE types_entite.code = ? "
    "AND entites.valeur = ? "
    "LIMIT 1;";

/**
 * @brief Enregistre une erreur littérale.
 */
static void entity_dao_set_error_literal(
    GError **error,
    EntityDaoError error_code,
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
        ENTITY_DAO_ERROR,
        error_code,
        message
    );
}

/**
 * @brief Détermine si SQLite signale un schéma incompatible.
 */
static gboolean entity_dao_is_schema_error(
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
 * @brief Transforme la dernière erreur Database en GError du DAO.
 */
static void entity_dao_set_database_error(
    EntityDao *entity_dao,
    GError **error,
    EntityDaoError error_code,
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

    if (entity_dao != NULL &&
        entity_dao->database != NULL)
    {
        database_message =
            database_error_get_message(
                entity_dao->database
            );
    }

    if (error_code == ENTITY_DAO_ERROR_PREPARE &&
        entity_dao_is_schema_error(
            database_message
        ))
    {
        error_code =
            ENTITY_DAO_ERROR_SCHEMA;
    }

    if (database_message != NULL &&
        database_message[0] != '\0')
    {
        g_set_error(
            error,
            ENTITY_DAO_ERROR,
            error_code,
            "%s : %s",
            context,
            database_message
        );

        return;
    }

    g_set_error_literal(
        error,
        ENTITY_DAO_ERROR,
        error_code,
        context
    );
}

/**
 * @brief Convertit un statut métier en texte SQLite.
 */
static const char *entity_dao_status_to_text(
    EntityStatus status
)
{
    switch (status)
    {
        case ENTITY_STATUS_ACTIVE:
            return "active";

        case ENTITY_STATUS_ARCHIVED:
            return "archived";

        case ENTITY_STATUS_DELETED:
            return "deleted";

        case ENTITY_STATUS_UNKNOWN:
        default:
            return NULL;
    }
}

/**
 * @brief Convertit un statut SQLite en statut métier.
 */
static gboolean entity_dao_status_from_text(
    const char *status_text,
    EntityStatus *out_status
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
            ENTITY_STATUS_ACTIVE;

        return TRUE;
    }

    if (g_strcmp0(
            status_text,
            "archived"
        ) == 0)
    {
        *out_status =
            ENTITY_STATUS_ARCHIVED;

        return TRUE;
    }

    if (g_strcmp0(
            status_text,
            "deleted"
        ) == 0)
    {
        *out_status =
            ENTITY_STATUS_DELETED;

        return TRUE;
    }

    return FALSE;
}

/**
 * @brief Lie un texte facultatif ou SQL NULL.
 */
static gboolean entity_dao_bind_optional_text(
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
 * @brief Exécute une recherche d'existence avec une valeur.
 */
static gboolean entity_dao_value_exists(
    EntityDao *entity_dao,
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

    if (entity_dao == NULL ||
        entity_dao->database == NULL ||
        sql == NULL ||
        value == NULL ||
        out_exists == NULL)
    {
        entity_dao_set_error_literal(
            error,
            ENTITY_DAO_ERROR_INVALID_ARGUMENT,
            "Les paramètres de la recherche d'existence sont invalides."
        );

        return FALSE;
    }

    *out_exists =
        FALSE;

    statement =
        database_statement_prepare(
            entity_dao->database,
            sql
        );

    if (statement == NULL)
    {
        entity_dao_set_database_error(
            entity_dao,
            error,
            ENTITY_DAO_ERROR_PREPARE,
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
        entity_dao_set_database_error(
            entity_dao,
            error,
            ENTITY_DAO_ERROR_BIND,
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
        entity_dao_set_database_error(
            entity_dao,
            error,
            ENTITY_DAO_ERROR_EXECUTE,
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
 * @brief Recherche un couple type et valeur existant.
 */
static gboolean entity_dao_type_value_exists(
    EntityDao *entity_dao,
    const char *type_identifier,
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

    if (entity_dao == NULL ||
        entity_dao->database == NULL ||
        type_identifier == NULL ||
        value == NULL ||
        out_exists == NULL)
    {
        entity_dao_set_error_literal(
            error,
            ENTITY_DAO_ERROR_INVALID_ARGUMENT,
            "Les paramètres de détection du doublon sont invalides."
        );

        return FALSE;
    }

    *out_exists =
        FALSE;

    statement =
        database_statement_prepare(
            entity_dao->database,
            entity_dao_type_value_exists_sql
        );

    if (statement == NULL)
    {
        entity_dao_set_database_error(
            entity_dao,
            error,
            ENTITY_DAO_ERROR_PREPARE,
            "Impossible de préparer la recherche du doublon"
        );

        goto cleanup;
    }

    if (!database_statement_bind_text(
            statement,
            1,
            type_identifier
        ) ||
        !database_statement_bind_text(
            statement,
            2,
            value
        ))
    {
        entity_dao_set_database_error(
            entity_dao,
            error,
            ENTITY_DAO_ERROR_BIND,
            "Impossible de lier le type et la valeur"
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
        entity_dao_set_database_error(
            entity_dao,
            error,
            ENTITY_DAO_ERROR_EXECUTE,
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
 * @brief Convertit une erreur SQLite d'insertion.
 */
static void entity_dao_set_insert_error(
    EntityDao *entity_dao,
    GError **error
)
{
    const char *database_message =
        NULL;

    if (entity_dao != NULL &&
        entity_dao->database != NULL)
    {
        database_message =
            database_error_get_message(
                entity_dao->database
            );
    }

    if (database_message != NULL &&
        strstr(
            database_message,
            "entites.id"
        ) != NULL)
    {
        entity_dao_set_error_literal(
            error,
            ENTITY_DAO_ERROR_CONSTRAINT,
            "Une entité possède déjà cet identifiant."
        );

        return;
    }

    if (database_message != NULL &&
        strstr(
            database_message,
            "entites.type_id"
        ) != NULL &&
        strstr(
            database_message,
            "entites.valeur"
        ) != NULL)
    {
        entity_dao_set_error_literal(
            error,
            ENTITY_DAO_ERROR_CONSTRAINT,
            "Une entité de ce type utilise déjà cette valeur."
        );

        return;
    }

    if (database_message != NULL &&
        (
            strstr(
                database_message,
                "constraint"
            ) != NULL ||
            strstr(
                database_message,
                "CONSTRAINT"
            ) != NULL
        ))
    {
        entity_dao_set_database_error(
            entity_dao,
            error,
            ENTITY_DAO_ERROR_CONSTRAINT,
            "L'entité viole une contrainte SQLite"
        );

        return;
    }

    entity_dao_set_database_error(
        entity_dao,
        error,
        ENTITY_DAO_ERROR_EXECUTE,
        "Impossible d'insérer l'entité"
    );
}

/**
 * @brief Construit un EntityRecord depuis la ligne courante.
 */
static EntityRecord *entity_dao_read_current_record(
    EntityDao *entity_dao,
    DatabaseStatement *statement,
    GError **error
)
{
    EntityRecord *entity_record =
        NULL;

    char *identifier =
        NULL;

    char *type_identifier =
        NULL;

    char *value =
        NULL;

    char *label =
        NULL;

    char *description =
        NULL;

    char *created_at =
        NULL;

    char *updated_at =
        NULL;

    char *status_text =
        NULL;

    int64_t confidence_value =
        -1;

    EntityStatus status =
        ENTITY_STATUS_UNKNOWN;

    GError *model_error =
        NULL;

    if (entity_dao == NULL ||
        entity_dao->database == NULL ||
        statement == NULL)
    {
        entity_dao_set_error_literal(
            error,
            ENTITY_DAO_ERROR_INVALID_ARGUMENT,
            "Impossible de lire une entité avec des paramètres invalides."
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
            &type_identifier
        ) ||
        !database_statement_column_text(
            statement,
            2,
            &value
        ) ||
        !database_statement_column_text(
            statement,
            3,
            &label
        ) ||
        !database_statement_column_text(
            statement,
            4,
            &description
        ) ||
        !database_statement_column_int64(
            statement,
            5,
            &confidence_value
        ) ||
        !database_statement_column_text(
            statement,
            6,
            &created_at
        ) ||
        !database_statement_column_text(
            statement,
            7,
            &updated_at
        ) ||
        !database_statement_column_text(
            statement,
            8,
            &status_text
        ))
    {
        entity_dao_set_error_literal(
            error,
            ENTITY_DAO_ERROR_READ,
            "Impossible de lire les colonnes de l'entité."
        );

        goto cleanup;
    }

    if (confidence_value < 0 ||
        confidence_value > 100)
    {
        entity_dao_set_error_literal(
            error,
            ENTITY_DAO_ERROR_RANGE,
            "L'entité persistée possède un niveau de confiance invalide."
        );

        goto cleanup;
    }

    if (!entity_dao_status_from_text(
            status_text,
            &status
        ))
    {
        entity_dao_set_error_literal(
            error,
            ENTITY_DAO_ERROR_MODEL,
            "L'entité persistée possède un statut invalide."
        );

        goto cleanup;
    }

    entity_record =
        entity_record_new(
            identifier,
            type_identifier,
            value,
            label,
            description,
            (gint) confidence_value,
            created_at,
            updated_at,
            status,
            &model_error
        );

    if (entity_record == NULL)
    {
        if (model_error != NULL)
        {
            g_set_error(
                error,
                ENTITY_DAO_ERROR,
                ENTITY_DAO_ERROR_MODEL,
                "L'entité persistée est invalide : %s",
                model_error->message
            );
        }
        else
        {
            entity_dao_set_error_literal(
                error,
                ENTITY_DAO_ERROR_MODEL,
                "L'entité persistée ne peut pas être reconstruite."
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
        description
    );

    g_free(
        label
    );

    g_free(
        value
    );

    g_free(
        type_identifier
    );

    g_free(
        identifier
    );

    return entity_record;
}

/**
 * @brief Libère un modèle contenu dans un GPtrArray.
 */
static void entity_dao_record_destroy(
    gpointer user_data
)
{
    entity_record_free(
        user_data
    );
}

GQuark entity_dao_error_quark(void)
{
    return g_quark_from_static_string(
        "entity-dao-error-quark"
    );
}

EntityDao *entity_dao_new(
    Database *database,
    GError **error
)
{
    EntityDao *entity_dao =
        NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (database == NULL)
    {
        entity_dao_set_error_literal(
            error,
            ENTITY_DAO_ERROR_INVALID_ARGUMENT,
            "La connexion Database est absente."
        );

        return NULL;
    }

    entity_dao =
        g_try_new0(
            EntityDao,
            1
        );

    if (entity_dao == NULL)
    {
        entity_dao_set_error_literal(
            error,
            ENTITY_DAO_ERROR_MEMORY,
            "Impossible d'allouer le DAO des entités."
        );

        return NULL;
    }

    entity_dao->database =
        database;

    return entity_dao;
}

void entity_dao_free(
    EntityDao *entity_dao
)
{
    if (entity_dao == NULL)
    {
        return;
    }

    entity_dao->database =
        NULL;

    g_free(
        entity_dao
    );
}

gboolean entity_dao_insert(
    EntityDao *entity_dao,
    const EntityRecord *entity_record,
    GError **error
)
{
    DatabaseStatement *statement =
        NULL;

    const char *identifier =
        NULL;

    const char *type_identifier =
        NULL;

    const char *value =
        NULL;

    const char *label =
        NULL;

    const char *description =
        NULL;

    const char *created_at =
        NULL;

    const char *updated_at =
        NULL;

    const char *status_text =
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

    if (entity_dao == NULL ||
        entity_dao->database == NULL ||
        entity_record == NULL)
    {
        entity_dao_set_error_literal(
            error,
            ENTITY_DAO_ERROR_INVALID_ARGUMENT,
            "Le DAO ou l'entité est absent."
        );

        return FALSE;
    }

    identifier =
        entity_record_get_identifier(
            entity_record
        );

    type_identifier =
        entity_record_get_type_identifier(
            entity_record
        );

    value =
        entity_record_get_value(
            entity_record
        );

    label =
        entity_record_get_label(
            entity_record
        );

    description =
        entity_record_get_description(
            entity_record
        );

    confidence =
        entity_record_get_confidence(
            entity_record
        );

    created_at =
        entity_record_get_created_at(
            entity_record
        );

    updated_at =
        entity_record_get_updated_at(
            entity_record
        );

    status_text =
        entity_dao_status_to_text(
            entity_record_get_status(
                entity_record
            )
        );

    if (identifier == NULL ||
        type_identifier == NULL ||
        value == NULL ||
        created_at == NULL ||
        updated_at == NULL ||
        status_text == NULL)
    {
        entity_dao_set_error_literal(
            error,
            ENTITY_DAO_ERROR_INVALID_ARGUMENT,
            "L'entité contient une valeur obligatoire absente."
        );

        return FALSE;
    }

    if (confidence < 0 ||
        confidence > 100)
    {
        entity_dao_set_error_literal(
            error,
            ENTITY_DAO_ERROR_RANGE,
            "Le niveau de confiance dépasse les limites autorisées."
        );

        return FALSE;
    }

    if (!entity_dao_value_exists(
            entity_dao,
            entity_dao_identifier_exists_sql,
            identifier,
            &value_exists,
            error
        ))
    {
        return FALSE;
    }

    if (value_exists)
    {
        entity_dao_set_error_literal(
            error,
            ENTITY_DAO_ERROR_CONSTRAINT,
            "Une entité possède déjà cet identifiant."
        );

        return FALSE;
    }

    if (!entity_dao_value_exists(
            entity_dao,
            entity_dao_type_exists_sql,
            type_identifier,
            &value_exists,
            error
        ))
    {
        return FALSE;
    }

    if (!value_exists)
    {
        entity_dao_set_error_literal(
            error,
            ENTITY_DAO_ERROR_CONSTRAINT,
            "Le type d'entité n'existe pas dans le catalogue SQLite."
        );

        return FALSE;
    }

    if (!entity_dao_type_value_exists(
            entity_dao,
            type_identifier,
            value,
            &value_exists,
            error
        ))
    {
        return FALSE;
    }

    if (value_exists)
    {
        entity_dao_set_error_literal(
            error,
            ENTITY_DAO_ERROR_CONSTRAINT,
            "Une entité de ce type utilise déjà cette valeur."
        );

        return FALSE;
    }

    statement =
        database_statement_prepare(
            entity_dao->database,
            entity_dao_insert_sql
        );

    if (statement == NULL)
    {
        entity_dao_set_database_error(
            entity_dao,
            error,
            ENTITY_DAO_ERROR_PREPARE,
            "Impossible de préparer l'insertion de l'entité"
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
            type_identifier
        ) ||
        !database_statement_bind_text(
            statement,
            3,
            value
        ) ||
        !entity_dao_bind_optional_text(
            statement,
            4,
            label
        ) ||
        !entity_dao_bind_optional_text(
            statement,
            5,
            description
        ) ||
        !database_statement_bind_int64(
            statement,
            6,
            (int64_t) confidence
        ) ||
        !database_statement_bind_text(
            statement,
            7,
            created_at
        ) ||
        !database_statement_bind_text(
            statement,
            8,
            updated_at
        ) ||
        !database_statement_bind_text(
            statement,
            9,
            status_text
        ))
    {
        entity_dao_set_database_error(
            entity_dao,
            error,
            ENTITY_DAO_ERROR_BIND,
            "Impossible de lier les données de l'entité"
        );

        goto cleanup;
    }

    if (database_statement_step(
            statement
        ) != DATABASE_STATEMENT_STEP_DONE)
    {
        entity_dao_set_insert_error(
            entity_dao,
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

    return success;
}

EntityRecord *entity_dao_find_by_identifier(
    EntityDao *entity_dao,
    const char *identifier,
    GError **error
)
{
    DatabaseStatement *statement =
        NULL;

    EntityRecord *entity_record =
        NULL;

    DatabaseStatementStepResult step_result =
        DATABASE_STATEMENT_STEP_ERROR;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (entity_dao == NULL ||
        entity_dao->database == NULL ||
        identifier == NULL ||
        !g_uuid_string_is_valid(
            identifier
        ))
    {
        entity_dao_set_error_literal(
            error,
            ENTITY_DAO_ERROR_INVALID_ARGUMENT,
            "L'identifiant d'entité est invalide."
        );

        return NULL;
    }

    statement =
        database_statement_prepare(
            entity_dao->database,
            entity_dao_find_by_identifier_sql
        );

    if (statement == NULL)
    {
        entity_dao_set_database_error(
            entity_dao,
            error,
            ENTITY_DAO_ERROR_PREPARE,
            "Impossible de préparer la recherche de l'entité"
        );

        goto cleanup;
    }

    if (!database_statement_bind_text(
            statement,
            1,
            identifier
        ))
    {
        entity_dao_set_database_error(
            entity_dao,
            error,
            ENTITY_DAO_ERROR_BIND,
            "Impossible de lier l'identifiant de l'entité"
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
        entity_dao_set_database_error(
            entity_dao,
            error,
            ENTITY_DAO_ERROR_EXECUTE,
            "Impossible d'exécuter la recherche de l'entité"
        );

        goto cleanup;
    }

    if (step_result ==
        DATABASE_STATEMENT_STEP_DONE)
    {
        goto cleanup;
    }

    entity_record =
        entity_dao_read_current_record(
            entity_dao,
            statement,
            error
        );

    if (entity_record == NULL)
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
            entity_dao_set_error_literal(
                error,
                ENTITY_DAO_ERROR_SCHEMA,
                "Plusieurs entités possèdent le même identifiant."
            );
        }
        else
        {
            entity_dao_set_database_error(
                entity_dao,
                error,
                ENTITY_DAO_ERROR_EXECUTE,
                "Impossible de terminer la lecture de l'entité"
            );
        }

        entity_record_free(
            entity_record
        );

        entity_record =
            NULL;
    }

cleanup:

    database_statement_finalize(
        statement
    );

    return entity_record;
}

GPtrArray *entity_dao_list_all(
    EntityDao *entity_dao,
    GError **error
)
{
    DatabaseStatement *statement =
        NULL;

    GPtrArray *entity_records =
        NULL;

    EntityRecord *entity_record =
        NULL;

    DatabaseStatementStepResult step_result =
        DATABASE_STATEMENT_STEP_ERROR;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (entity_dao == NULL ||
        entity_dao->database == NULL)
    {
        entity_dao_set_error_literal(
            error,
            ENTITY_DAO_ERROR_INVALID_ARGUMENT,
            "Le DAO des entités est invalide."
        );

        return NULL;
    }

    entity_records =
        g_ptr_array_new_with_free_func(
            entity_dao_record_destroy
        );

    if (entity_records == NULL)
    {
        entity_dao_set_error_literal(
            error,
            ENTITY_DAO_ERROR_MEMORY,
            "Impossible d'allouer la liste des entités."
        );

        return NULL;
    }

    statement =
        database_statement_prepare(
            entity_dao->database,
            entity_dao_list_all_sql
        );

    if (statement == NULL)
    {
        entity_dao_set_database_error(
            entity_dao,
            error,
            ENTITY_DAO_ERROR_PREPARE,
            "Impossible de préparer le chargement des entités"
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
            entity_dao_set_database_error(
                entity_dao,
                error,
                ENTITY_DAO_ERROR_EXECUTE,
                "Impossible d'exécuter le chargement des entités"
            );

            goto error;
        }

        entity_record =
            entity_dao_read_current_record(
                entity_dao,
                statement,
                error
            );

        if (entity_record == NULL)
        {
            goto error;
        }

        g_ptr_array_add(
            entity_records,
            entity_record
        );

        entity_record =
            NULL;
    }

    database_statement_finalize(
        statement
    );

    return entity_records;

error:

    entity_record_free(
        entity_record
    );

    database_statement_finalize(
        statement
    );

    g_ptr_array_unref(
        entity_records
    );

    return NULL;
}

gboolean entity_dao_count(
    EntityDao *entity_dao,
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

    if (entity_dao == NULL ||
        entity_dao->database == NULL ||
        out_count == NULL)
    {
        entity_dao_set_error_literal(
            error,
            ENTITY_DAO_ERROR_INVALID_ARGUMENT,
            "Les paramètres du comptage des entités sont invalides."
        );

        return FALSE;
    }

    *out_count =
        0;

    statement =
        database_statement_prepare(
            entity_dao->database,
            entity_dao_count_sql
        );

    if (statement == NULL)
    {
        entity_dao_set_database_error(
            entity_dao,
            error,
            ENTITY_DAO_ERROR_PREPARE,
            "Impossible de préparer le comptage des entités"
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
        entity_dao_set_database_error(
            entity_dao,
            error,
            ENTITY_DAO_ERROR_EXECUTE,
            "Impossible d'exécuter le comptage des entités"
        );

        goto cleanup;
    }

    if (step_result !=
        DATABASE_STATEMENT_STEP_ROW)
    {
        entity_dao_set_error_literal(
            error,
            ENTITY_DAO_ERROR_READ,
            "Le comptage des entités n'a retourné aucune ligne."
        );

        goto cleanup;
    }

    if (!database_statement_column_int64(
            statement,
            0,
            &count_value
        ))
    {
        entity_dao_set_error_literal(
            error,
            ENTITY_DAO_ERROR_READ,
            "Impossible de lire le nombre d'entités."
        );

        goto cleanup;
    }

    if (count_value < 0)
    {
        entity_dao_set_error_literal(
            error,
            ENTITY_DAO_ERROR_RANGE,
            "Le nombre d'entités retourné par SQLite est invalide."
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
            entity_dao_set_error_literal(
                error,
                ENTITY_DAO_ERROR_SCHEMA,
                "Le comptage des entités a retourné plusieurs lignes."
            );
        }
        else
        {
            entity_dao_set_database_error(
                entity_dao,
                error,
                ENTITY_DAO_ERROR_EXECUTE,
                "Impossible de terminer le comptage des entités"
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
