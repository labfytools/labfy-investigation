/******************************************************************************
 * @file evidence_dao.c
 * @brief Persistance des preuves numériques dans SQLite.
 ******************************************************************************/

#include "dao/evidence_dao.h"

#include "database/error.h"
#include "database/statement.h"

#include <glib.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief Représentation privée du DAO des preuves.
 *
 * La connexion Database est empruntée.
 */
struct EvidenceDao
{
    Database *database;
};

/**
 * @brief Requête d’insertion d’une preuve.
 *
 * updated_at reçoit initialement la même date que imported_at.
 *
 * Les colonnes historiques non représentées par EvidenceRecord utilisent
 * leurs valeurs par défaut ou SQL NULL.
 */
static const char *const evidence_dao_insert_sql =
    "INSERT INTO preuves"
    "("
    "    id,"
    "    original_name,"
    "    name,"
    "    relative_path,"
    "    type_id,"
    "    size_bytes,"
    "    sha256,"
    "    description,"
    "    imported_at,"
    "    updated_at,"
    "    source,"
    "    collected_at,"
    "    integrity_status"
    ")"
    "VALUES"
    "("
    "    ?,"
    "    ?,"
    "    ?,"
    "    ?,"
    "    ("
    "        SELECT id "
    "        FROM types_preuve "
    "        WHERE code = ?"
    "    ),"
    "    ?,"
    "    ?,"
    "    ?,"
    "    ?,"
    "    ?,"
    "    ?,"
    "    ?,"
    "    ?"
    ");";

/**
 * @brief Requête de recherche d’une preuve par son identifiant.
 */
static const char *const evidence_dao_find_by_identifier_sql =
    "SELECT "
    "    preuves.id,"
    "    preuves.original_name,"
    "    preuves.name,"
    "    preuves.relative_path,"
    "    types_preuve.code,"
    "    preuves.size_bytes,"
    "    preuves.sha256,"
    "    preuves.imported_at,"
    "    preuves.collected_at,"
    "    preuves.source,"
    "    preuves.description,"
    "    preuves.integrity_status "
    "FROM preuves "
    "LEFT JOIN types_preuve "
    "ON types_preuve.id = preuves.type_id "
    "WHERE preuves.id = ?;";

/**
 * @brief Requête de détection d’un identifiant existant.
 */
static const char *const evidence_dao_identifier_exists_sql =
    "SELECT 1 "
    "FROM preuves "
    "WHERE id = ? "
    "LIMIT 1;";

/**
 * @brief Requête de détection d’un chemin existant.
 */
static const char *const evidence_dao_relative_path_exists_sql =
    "SELECT 1 "
    "FROM preuves "
    "WHERE relative_path = ? "
    "LIMIT 1;";

/**
 * @brief Requête de détection d’un type de preuve.
 */
static const char *const evidence_dao_type_exists_sql =
    "SELECT 1 "
    "FROM types_preuve "
    "WHERE code = ? "
    "LIMIT 1;";

  /**
 * @brief Requête de comptage des preuves.
 */
static const char *const evidence_dao_count_sql =
    "SELECT COUNT(*) "
    "FROM preuves;";

    /**
 * @brief Requête de chargement de toutes les preuves.
 */
static const char *const evidence_dao_list_all_sql =
    "SELECT "
    "    preuves.id,"
    "    preuves.original_name,"
    "    preuves.name,"
    "    preuves.relative_path,"
    "    types_preuve.code,"
    "    preuves.size_bytes,"
    "    preuves.sha256,"
    "    preuves.imported_at,"
    "    preuves.collected_at,"
    "    preuves.source,"
    "    preuves.description,"
    "    preuves.integrity_status "
    "FROM preuves "
    "LEFT JOIN types_preuve "
    "ON types_preuve.id = preuves.type_id "
    "ORDER BY "
    "    preuves.imported_at ASC,"
    "    preuves.id ASC;";

/**
 * @brief Requête de mise à jour du statut d'intégrité.
 */
static const char *const evidence_dao_update_integrity_status_sql =
    "UPDATE preuves "
    "SET integrity_status = ? "
    "WHERE id = ?;";

/** @brief Requête de mise à jour des métadonnées éditables. */
static const char *const evidence_dao_update_metadata_sql =
    "UPDATE preuves SET type_id=(SELECT id FROM types_preuve WHERE code=?),"
    "relative_path=?,source=?,description=?,updated_at=? WHERE id=?;";

/**
 * @brief Enregistre une erreur littérale.
 */
static void evidence_dao_set_error_literal(
    GError **error,
    EvidenceDaoError error_code,
    const char *error_message
)
{
    if (error == NULL)
    {
        return;
    }

    g_set_error_literal(
        error,
        EVIDENCE_DAO_ERROR,
        error_code,
        error_message
    );
}

/**
 * @brief Transforme la dernière erreur Database en GError du DAO.
 */
static void evidence_dao_set_database_error(
    EvidenceDao *evidence_dao,
    GError **error,
    EvidenceDaoError error_code,
    const char *context
)
{
    const char *database_error_message = NULL;

    if (error == NULL)
    {
        return;
    }

    if (evidence_dao != NULL &&
        evidence_dao->database != NULL)
    {
        database_error_message =
            database_error_get_message(
                evidence_dao->database
            );
    }

    if (database_error_message != NULL &&
        database_error_message[0] != '\0')
    {
        g_set_error(
            error,
            EVIDENCE_DAO_ERROR,
            error_code,
            "%s : %s",
            context,
            database_error_message
        );

        return;
    }

    g_set_error_literal(
        error,
        EVIDENCE_DAO_ERROR,
        error_code,
        context
    );
}

/**
 * @brief Exécute une requête SELECT d’existence.
 */
static gboolean evidence_dao_value_exists(
    EvidenceDao *evidence_dao,
    const char *sql,
    const char *value,
    gboolean *out_exists,
    GError **error
)
{
    DatabaseStatement *statement = NULL;

    DatabaseStatementStepResult step_result =
        DATABASE_STATEMENT_STEP_ERROR;

    gboolean success = FALSE;

    if (evidence_dao == NULL ||
        evidence_dao->database == NULL ||
        sql == NULL ||
        value == NULL ||
        out_exists == NULL)
    {
        evidence_dao_set_error_literal(
            error,
            EVIDENCE_DAO_ERROR_INVALID_ARGUMENT,
            "Les paramètres de la recherche d’existence sont invalides."
        );

        return FALSE;
    }

    *out_exists = FALSE;

    statement =
        database_statement_prepare(
            evidence_dao->database,
            sql
        );

    if (statement == NULL)
    {
        evidence_dao_set_database_error(
            evidence_dao,
            error,
            EVIDENCE_DAO_ERROR_PREPARE,
            "Impossible de préparer la recherche d’existence"
        );

        goto cleanup;
    }

    if (!database_statement_bind_text(
            statement,
            1,
            value
        ))
    {
        evidence_dao_set_database_error(
            evidence_dao,
            error,
            EVIDENCE_DAO_ERROR_BIND,
            "Impossible de lier la valeur recherchée"
        );

        goto cleanup;
    }

    step_result =
        database_statement_step(
            statement
        );

    if (step_result == DATABASE_STATEMENT_STEP_ERROR)
    {
        evidence_dao_set_database_error(
            evidence_dao,
            error,
            EVIDENCE_DAO_ERROR_EXECUTE,
            "Impossible d’exécuter la recherche d’existence"
        );

        goto cleanup;
    }

    *out_exists =
        step_result == DATABASE_STATEMENT_STEP_ROW;

    success = TRUE;

cleanup:

    database_statement_finalize(
        statement
    );

    return success;
}

/**
 * @brief Lie un texte facultatif ou SQL NULL.
 */
static gboolean evidence_dao_bind_optional_text(
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
 * @brief Convertit une erreur d’insertion SQLite en erreur DAO.
 */
static void evidence_dao_set_insert_error(
    EvidenceDao *evidence_dao,
    GError **error
)
{
    const char *database_error_message = NULL;

    if (evidence_dao != NULL &&
        evidence_dao->database != NULL)
    {
        database_error_message =
            database_error_get_message(
                evidence_dao->database
            );
    }

    if (database_error_message != NULL &&
        strstr(
            database_error_message,
            "preuves.id"
        ) != NULL)
    {
        evidence_dao_set_error_literal(
            error,
            EVIDENCE_DAO_ERROR_CONSTRAINT,
            "Une preuve possède déjà cet identifiant."
        );

        return;
    }

    if (database_error_message != NULL &&
        strstr(
            database_error_message,
            "preuves.relative_path"
        ) != NULL)
    {
        evidence_dao_set_error_literal(
            error,
            EVIDENCE_DAO_ERROR_CONSTRAINT,
            "Une preuve utilise déjà ce chemin relatif."
        );

        return;
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
            ) != NULL
        ))
    {
        evidence_dao_set_database_error(
            evidence_dao,
            error,
            EVIDENCE_DAO_ERROR_CONSTRAINT,
            "La preuve viole une contrainte SQLite"
        );

        return;
    }

    evidence_dao_set_database_error(
        evidence_dao,
        error,
        EVIDENCE_DAO_ERROR_EXECUTE,
        "Impossible d’insérer la preuve"
    );
}

/**
 * @brief Construit un EvidenceRecord depuis la ligne SQLite courante.
 */
static EvidenceRecord *evidence_dao_read_current_record(
    EvidenceDao *evidence_dao,
    DatabaseStatement *statement,
    GError **error
)
{
    EvidenceRecord *evidence_record = NULL;

    char *identifier = NULL;
    char *original_name = NULL;
    char *internal_name = NULL;
    char *relative_path = NULL;
    char *type_identifier = NULL;
    char *sha256 = NULL;
    char *imported_at = NULL;
    char *collected_at = NULL;
    char *source = NULL;
    char *description = NULL;

    int64_t size_bytes = -1;
    int64_t integrity_status_value = -1;

    GError *model_error = NULL;

    if (evidence_dao == NULL ||
        evidence_dao->database == NULL ||
        statement == NULL)
    {
        evidence_dao_set_error_literal(
            error,
            EVIDENCE_DAO_ERROR_INVALID_ARGUMENT,
            "Impossible de lire une preuve avec des paramètres invalides."
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
            &original_name
        ) ||
        !database_statement_column_text(
            statement,
            2,
            &internal_name
        ) ||
        !database_statement_column_text(
            statement,
            3,
            &relative_path
        ) ||
        !database_statement_column_text(
            statement,
            4,
            &type_identifier
        ) ||
        !database_statement_column_int64(
            statement,
            5,
            &size_bytes
        ) ||
        !database_statement_column_text(
            statement,
            6,
            &sha256
        ) ||
        !database_statement_column_text(
            statement,
            7,
            &imported_at
        ) ||
        !database_statement_column_text(
            statement,
            8,
            &collected_at
        ) ||
        !database_statement_column_text(
            statement,
            9,
            &source
        ) ||
        !database_statement_column_text(
            statement,
            10,
            &description
        ) ||
        !database_statement_column_int64(
            statement,
            11,
            &integrity_status_value
        ))
    {
        evidence_dao_set_error_literal(
            error,
            EVIDENCE_DAO_ERROR_READ,
            "Impossible de lire les colonnes de la preuve."
        );

        goto cleanup;
    }

    if (size_bytes < 0)
    {
        evidence_dao_set_error_literal(
            error,
            EVIDENCE_DAO_ERROR_RANGE,
            "La preuve persistée possède une taille négative."
        );

        goto cleanup;
    }

    if (integrity_status_value <
            EVIDENCE_INTEGRITY_STATUS_UNKNOWN ||
        integrity_status_value >
            EVIDENCE_INTEGRITY_STATUS_ERROR)
    {
        evidence_dao_set_error_literal(
            error,
            EVIDENCE_DAO_ERROR_MODEL,
            "La preuve persistée possède un état d’intégrité invalide."
        );

        goto cleanup;
    }

    evidence_record =
        evidence_record_new(
            identifier,
            original_name,
            internal_name,
            relative_path,
            type_identifier,
            (guint64) size_bytes,
            sha256,
            imported_at,
            collected_at,
            source,
            description,
            (EvidenceIntegrityStatus) integrity_status_value,
            &model_error
        );

    if (evidence_record == NULL)
    {
        if (model_error != NULL)
        {
            g_set_error(
                error,
                EVIDENCE_DAO_ERROR,
                EVIDENCE_DAO_ERROR_MODEL,
                "La preuve persistée est invalide : %s",
                model_error->message
            );
        }
        else
        {
            evidence_dao_set_error_literal(
                error,
                EVIDENCE_DAO_ERROR_MODEL,
                "La preuve persistée ne peut pas être reconstruite."
            );
        }
    }

cleanup:

    g_clear_error(
        &model_error
    );

    g_free(
        description
    );

    g_free(
        source
    );

    g_free(
        collected_at
    );

    g_free(
        imported_at
    );

    g_free(
        sha256
    );

    g_free(
        type_identifier
    );

    g_free(
        relative_path
    );

    g_free(
        internal_name
    );

    g_free(
        original_name
    );

    g_free(
        identifier
    );

    return evidence_record;
}

/**
 * @brief Libère un EvidenceRecord contenu dans un GPtrArray.
 */
static void evidence_dao_record_destroy(
    gpointer data
)
{
    evidence_record_free(
        data
    );
}

GQuark evidence_dao_error_quark(void)
{
    return g_quark_from_static_string(
        "evidence-dao-error-quark"
    );
}

EvidenceDao *evidence_dao_new(
    Database *database,
    GError **error
)
{
    EvidenceDao *evidence_dao = NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (database == NULL)
    {
        evidence_dao_set_error_literal(
            error,
            EVIDENCE_DAO_ERROR_INVALID_ARGUMENT,
            "La connexion Database est absente."
        );

        return NULL;
    }

    evidence_dao =
        g_try_new0(
            EvidenceDao,
            1
        );

    if (evidence_dao == NULL)
    {
        evidence_dao_set_error_literal(
            error,
            EVIDENCE_DAO_ERROR_MEMORY,
            "Impossible d’allouer le DAO des preuves."
        );

        return NULL;
    }

    evidence_dao->database = database;

    return evidence_dao;
}

void evidence_dao_free(
    EvidenceDao *evidence_dao
)
{
    if (evidence_dao == NULL)
    {
        return;
    }

    evidence_dao->database = NULL;

    g_free(
        evidence_dao
    );
}

gboolean evidence_dao_insert(
    EvidenceDao *evidence_dao,
    const EvidenceRecord *evidence_record,
    GError **error
)
{
    DatabaseStatement *statement = NULL;

    const char *identifier = NULL;
    const char *original_name = NULL;
    const char *internal_name = NULL;
    const char *relative_path = NULL;
    const char *type_identifier = NULL;
    const char *sha256 = NULL;
    const char *imported_at = NULL;
    const char *collected_at = NULL;
    const char *source = NULL;
    const char *description = NULL;

    guint64 size_bytes = 0;

    EvidenceIntegrityStatus integrity_status =
        EVIDENCE_INTEGRITY_STATUS_UNKNOWN;

    gboolean value_exists = FALSE;
    gboolean success = FALSE;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (evidence_dao == NULL ||
        evidence_dao->database == NULL ||
        evidence_record == NULL)
    {
        evidence_dao_set_error_literal(
            error,
            EVIDENCE_DAO_ERROR_INVALID_ARGUMENT,
            "Le DAO ou la preuve est absent."
        );

        return FALSE;
    }

    identifier =
        evidence_record_get_identifier(
            evidence_record
        );

    original_name =
        evidence_record_get_original_name(
            evidence_record
        );

    internal_name =
        evidence_record_get_internal_name(
            evidence_record
        );

    relative_path =
        evidence_record_get_relative_path(
            evidence_record
        );

    type_identifier =
        evidence_record_get_type_identifier(
            evidence_record
        );

    size_bytes =
        evidence_record_get_size_bytes(
            evidence_record
        );

    sha256 =
        evidence_record_get_sha256(
            evidence_record
        );

    imported_at =
        evidence_record_get_imported_at(
            evidence_record
        );

    collected_at =
        evidence_record_get_collected_at(
            evidence_record
        );

    source =
        evidence_record_get_source(
            evidence_record
        );

    description =
        evidence_record_get_description(
            evidence_record
        );

    integrity_status =
        evidence_record_get_integrity_status(
            evidence_record
        );

    if (identifier == NULL ||
        original_name == NULL ||
        internal_name == NULL ||
        relative_path == NULL ||
        type_identifier == NULL ||
        sha256 == NULL ||
        imported_at == NULL)
    {
        evidence_dao_set_error_literal(
            error,
            EVIDENCE_DAO_ERROR_INVALID_ARGUMENT,
            "La preuve contient une valeur obligatoire absente."
        );

        return FALSE;
    }

    /*
     * SQLite stocke les INTEGER dans un entier signé sur 64 bits.
     */
    if (size_bytes > (guint64) G_MAXINT64)
    {
        evidence_dao_set_error_literal(
            error,
            EVIDENCE_DAO_ERROR_RANGE,
            "La taille de la preuve dépasse la capacité de SQLite."
        );

        return FALSE;
    }

    if (!evidence_dao_value_exists(
            evidence_dao,
            evidence_dao_identifier_exists_sql,
            identifier,
            &value_exists,
            error
        ))
    {
        return FALSE;
    }

    if (value_exists)
    {
        evidence_dao_set_error_literal(
            error,
            EVIDENCE_DAO_ERROR_CONSTRAINT,
            "Une preuve possède déjà cet identifiant."
        );

        return FALSE;
    }

    if (!evidence_dao_value_exists(
            evidence_dao,
            evidence_dao_relative_path_exists_sql,
            relative_path,
            &value_exists,
            error
        ))
    {
        return FALSE;
    }

    if (value_exists)
    {
        evidence_dao_set_error_literal(
            error,
            EVIDENCE_DAO_ERROR_CONSTRAINT,
            "Une preuve utilise déjà ce chemin relatif."
        );

        return FALSE;
    }

    if (!evidence_dao_value_exists(
            evidence_dao,
            evidence_dao_type_exists_sql,
            type_identifier,
            &value_exists,
            error
        ))
    {
        return FALSE;
    }

    if (!value_exists)
    {
        evidence_dao_set_error_literal(
            error,
            EVIDENCE_DAO_ERROR_CONSTRAINT,
            "Le type de preuve n’existe pas dans le catalogue SQLite."
        );

        return FALSE;
    }

    statement =
        database_statement_prepare(
            evidence_dao->database,
            evidence_dao_insert_sql
        );

    if (statement == NULL)
    {
        evidence_dao_set_database_error(
            evidence_dao,
            error,
            EVIDENCE_DAO_ERROR_PREPARE,
            "Impossible de préparer l’insertion de la preuve"
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
            original_name
        ) ||
        !database_statement_bind_text(
            statement,
            3,
            internal_name
        ) ||
        !database_statement_bind_text(
            statement,
            4,
            relative_path
        ) ||
        !database_statement_bind_text(
            statement,
            5,
            type_identifier
        ) ||
        !database_statement_bind_int64(
            statement,
            6,
            (int64_t) size_bytes
        ) ||
        !database_statement_bind_text(
            statement,
            7,
            sha256
        ) ||
        !evidence_dao_bind_optional_text(
            statement,
            8,
            description
        ) ||
        !database_statement_bind_text(
            statement,
            9,
            imported_at
        ) ||
        !database_statement_bind_text(
            statement,
            10,
            imported_at
        ) ||
        !evidence_dao_bind_optional_text(
            statement,
            11,
            source
        ) ||
        !evidence_dao_bind_optional_text(
            statement,
            12,
            collected_at
        ) ||
        !database_statement_bind_int64(
            statement,
            13,
            (int64_t) integrity_status
        ))
    {
        evidence_dao_set_database_error(
            evidence_dao,
            error,
            EVIDENCE_DAO_ERROR_BIND,
            "Impossible de lier les données de la preuve"
        );

        goto cleanup;
    }

    if (database_statement_step(
            statement
        ) != DATABASE_STATEMENT_STEP_DONE)
    {
        evidence_dao_set_insert_error(
            evidence_dao,
            error
        );

        goto cleanup;
    }

    success = TRUE;

cleanup:

    database_statement_finalize(
        statement
    );

    return success;
}

EvidenceRecord *evidence_dao_find_by_identifier(
    EvidenceDao *evidence_dao,
    const char *identifier,
    GError **error
)
{
    DatabaseStatement *statement = NULL;
    EvidenceRecord *evidence_record = NULL;

    DatabaseStatementStepResult step_result =
        DATABASE_STATEMENT_STEP_ERROR;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (evidence_dao == NULL ||
        evidence_dao->database == NULL ||
        identifier == NULL ||
        !g_uuid_string_is_valid(identifier))
    {
        evidence_dao_set_error_literal(
            error,
            EVIDENCE_DAO_ERROR_INVALID_ARGUMENT,
            "L’identifiant de preuve est invalide."
        );

        return NULL;
    }

    statement =
        database_statement_prepare(
            evidence_dao->database,
            evidence_dao_find_by_identifier_sql
        );

    if (statement == NULL)
    {
        evidence_dao_set_database_error(
            evidence_dao,
            error,
            EVIDENCE_DAO_ERROR_PREPARE,
            "Impossible de préparer la recherche de la preuve"
        );

        goto cleanup;
    }

    if (!database_statement_bind_text(
            statement,
            1,
            identifier
        ))
    {
        evidence_dao_set_database_error(
            evidence_dao,
            error,
            EVIDENCE_DAO_ERROR_BIND,
            "Impossible de lier l’identifiant de la preuve"
        );

        goto cleanup;
    }

    step_result =
        database_statement_step(
            statement
        );

    if (step_result == DATABASE_STATEMENT_STEP_ERROR)
    {
        evidence_dao_set_database_error(
            evidence_dao,
            error,
            EVIDENCE_DAO_ERROR_EXECUTE,
            "Impossible d’exécuter la recherche de la preuve"
        );

        goto cleanup;
    }

    /*
     * Une preuve absente n’est pas une erreur.
     */
    if (step_result == DATABASE_STATEMENT_STEP_DONE)
    {
        goto cleanup;
    }

    evidence_record =
        evidence_dao_read_current_record(
            evidence_dao,
            statement,
            error
        );

    if (evidence_record == NULL)
    {
        goto cleanup;
    }

    /*
     * L’identifiant est une clé primaire, mais on vérifie tout de même
     * que la requête ne fournit aucune seconde ligne.
     */
    step_result =
        database_statement_step(
            statement
        );

    if (step_result != DATABASE_STATEMENT_STEP_DONE)
    {
        if (step_result == DATABASE_STATEMENT_STEP_ROW)
        {
            evidence_dao_set_error_literal(
                error,
                EVIDENCE_DAO_ERROR_SCHEMA,
                "Plusieurs preuves possèdent le même identifiant."
            );
        }
        else
        {
            evidence_dao_set_database_error(
                evidence_dao,
                error,
                EVIDENCE_DAO_ERROR_EXECUTE,
                "Impossible de terminer la lecture de la preuve"
            );
        }

        evidence_record_free(
            evidence_record
        );

        evidence_record = NULL;
    }

cleanup:

    database_statement_finalize(
        statement
    );

    return evidence_record;
}

gboolean evidence_dao_update_integrity_status(
    EvidenceDao *evidence_dao,
    const char *identifier,
    EvidenceIntegrityStatus integrity_status,
    GError **error
)
{
    DatabaseStatement *statement =
        NULL;

    gboolean identifier_exists =
        FALSE;

    gboolean success =
        FALSE;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (evidence_dao == NULL ||
        evidence_dao->database == NULL ||
        identifier == NULL ||
        !g_uuid_string_is_valid(
            identifier
        ))
    {
        evidence_dao_set_error_literal(
            error,
            EVIDENCE_DAO_ERROR_INVALID_ARGUMENT,
            "Le DAO ou l'identifiant de preuve est invalide."
        );

        return FALSE;
    }

    if (integrity_status <
            EVIDENCE_INTEGRITY_STATUS_UNKNOWN ||
        integrity_status >
            EVIDENCE_INTEGRITY_STATUS_ERROR)
    {
        evidence_dao_set_error_literal(
            error,
            EVIDENCE_DAO_ERROR_INVALID_ARGUMENT,
            "Le statut d'intégrité demandé est invalide."
        );

        return FALSE;
    }

    /*
     * Cette vérification permet de distinguer une preuve absente
     * d'une mise à jour SQLite ayant réellement échoué.
     */
    if (!evidence_dao_value_exists(
            evidence_dao,
            evidence_dao_identifier_exists_sql,
            identifier,
            &identifier_exists,
            error
        ))
    {
        return FALSE;
    }

    if (!identifier_exists)
    {
        evidence_dao_set_error_literal(
            error,
            EVIDENCE_DAO_ERROR_NOT_FOUND,
            "La preuve à mettre à jour n'existe pas."
        );

        return FALSE;
    }

    statement =
        database_statement_prepare(
            evidence_dao->database,
            evidence_dao_update_integrity_status_sql
        );

    if (statement == NULL)
    {
        evidence_dao_set_database_error(
            evidence_dao,
            error,
            EVIDENCE_DAO_ERROR_PREPARE,
            "Impossible de préparer la mise à jour "
            "du statut d'intégrité"
        );

        goto cleanup;
    }

    if (!database_statement_bind_int64(
            statement,
            1,
            (int64_t) integrity_status
        ) ||
        !database_statement_bind_text(
            statement,
            2,
            identifier
        ))
    {
        evidence_dao_set_database_error(
            evidence_dao,
            error,
            EVIDENCE_DAO_ERROR_BIND,
            "Impossible de lier les paramètres "
            "du statut d'intégrité"
        );

        goto cleanup;
    }

    if (database_statement_step(
            statement
        ) != DATABASE_STATEMENT_STEP_DONE)
    {
        evidence_dao_set_database_error(
            evidence_dao,
            error,
            EVIDENCE_DAO_ERROR_EXECUTE,
            "Impossible de mettre à jour "
            "le statut d'intégrité"
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

gboolean evidence_dao_update_metadata(
    EvidenceDao *evidence_dao, const char *identifier,
    const char *type_identifier, const char *relative_path,
    const char *source, const char *description, const char *updated_at,
    GError **error
)
{
    DatabaseStatement *statement = NULL;
    gboolean success = FALSE;
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    if (evidence_dao == NULL || evidence_dao->database == NULL ||
        identifier == NULL || !g_uuid_string_is_valid(identifier) ||
        type_identifier == NULL || type_identifier[0] == '\0' ||
        relative_path == NULL || relative_path[0] == '\0' ||
        g_path_is_absolute(relative_path) || updated_at == NULL ||
        strlen(updated_at) != 20U)
    {
        evidence_dao_set_error_literal(error,
            EVIDENCE_DAO_ERROR_INVALID_ARGUMENT,
            "Les métadonnées de reclassement sont invalides.");
        return FALSE;
    }
    statement = database_statement_prepare(
        evidence_dao->database, evidence_dao_update_metadata_sql);
    if (statement == NULL) goto cleanup;
    success = database_statement_bind_text(statement, 1, type_identifier) &&
        database_statement_bind_text(statement, 2, relative_path) &&
        (source != NULL ? database_statement_bind_text(statement, 3, source)
                        : database_statement_bind_null(statement, 3)) &&
        (description != NULL
            ? database_statement_bind_text(statement, 4, description)
            : database_statement_bind_null(statement, 4)) &&
        database_statement_bind_text(statement, 5, updated_at) &&
        database_statement_bind_text(statement, 6, identifier) &&
        database_statement_step(statement) == DATABASE_STATEMENT_STEP_DONE;
cleanup:
    if (!success) evidence_dao_set_database_error(evidence_dao, error,
        EVIDENCE_DAO_ERROR_EXECUTE,
        "Impossible de mettre à jour les métadonnées de la preuve");
    database_statement_finalize(statement);
    return success;
}

gboolean evidence_dao_count(
    EvidenceDao *evidence_dao,
    guint64 *out_count,
    GError **error
)
{
    DatabaseStatement *statement = NULL;

    DatabaseStatementStepResult step_result =
        DATABASE_STATEMENT_STEP_ERROR;

    int64_t count_value = -1;

    gboolean success = FALSE;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (evidence_dao == NULL ||
        evidence_dao->database == NULL ||
        out_count == NULL)
    {
        evidence_dao_set_error_literal(
            error,
            EVIDENCE_DAO_ERROR_INVALID_ARGUMENT,
            "Les paramètres du comptage des preuves sont invalides."
        );

        return FALSE;
    }

    /*
     * Une erreur ne doit jamais laisser une ancienne valeur dans
     * la destination fournie par l’appelant.
     */
    *out_count = 0;

    statement =
        database_statement_prepare(
            evidence_dao->database,
            evidence_dao_count_sql
        );

    if (statement == NULL)
    {
        evidence_dao_set_database_error(
            evidence_dao,
            error,
            EVIDENCE_DAO_ERROR_PREPARE,
            "Impossible de préparer le comptage des preuves"
        );

        goto cleanup;
    }

    step_result =
        database_statement_step(
            statement
        );

    if (step_result == DATABASE_STATEMENT_STEP_ERROR)
    {
        evidence_dao_set_database_error(
            evidence_dao,
            error,
            EVIDENCE_DAO_ERROR_EXECUTE,
            "Impossible d’exécuter le comptage des preuves"
        );

        goto cleanup;
    }

    /*
     * COUNT(*) doit toujours produire exactement une ligne.
     */
    if (step_result != DATABASE_STATEMENT_STEP_ROW)
    {
        evidence_dao_set_error_literal(
            error,
            EVIDENCE_DAO_ERROR_READ,
            "Le comptage des preuves n’a retourné aucune ligne."
        );

        goto cleanup;
    }

    if (!database_statement_column_int64(
            statement,
            0,
            &count_value
        ))
    {
        evidence_dao_set_error_literal(
            error,
            EVIDENCE_DAO_ERROR_READ,
            "Impossible de lire le nombre de preuves."
        );

        goto cleanup;
    }

    if (count_value < 0)
    {
        evidence_dao_set_error_literal(
            error,
            EVIDENCE_DAO_ERROR_RANGE,
            "Le nombre de preuves retourné par SQLite est invalide."
        );

        goto cleanup;
    }

    step_result =
        database_statement_step(
            statement
        );

    if (step_result != DATABASE_STATEMENT_STEP_DONE)
    {
        if (step_result == DATABASE_STATEMENT_STEP_ROW)
        {
            evidence_dao_set_error_literal(
                error,
                EVIDENCE_DAO_ERROR_SCHEMA,
                "Le comptage des preuves a retourné plusieurs lignes."
            );
        }
        else
        {
            evidence_dao_set_database_error(
                evidence_dao,
                error,
                EVIDENCE_DAO_ERROR_EXECUTE,
                "Impossible de terminer le comptage des preuves"
            );
        }

        goto cleanup;
    }

    *out_count =
        (guint64) count_value;

    success = TRUE;

cleanup:

    database_statement_finalize(
        statement
    );

    return success;
}

GPtrArray *evidence_dao_list_all(
    EvidenceDao *evidence_dao,
    GError **error
)
{
    DatabaseStatement *statement = NULL;

    GPtrArray *evidence_records = NULL;
    EvidenceRecord *evidence_record = NULL;

    DatabaseStatementStepResult step_result =
        DATABASE_STATEMENT_STEP_ERROR;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (evidence_dao == NULL ||
        evidence_dao->database == NULL)
    {
        evidence_dao_set_error_literal(
            error,
            EVIDENCE_DAO_ERROR_INVALID_ARGUMENT,
            "Le DAO des preuves est invalide."
        );

        return NULL;
    }

    evidence_records =
        g_ptr_array_new_with_free_func(
            evidence_dao_record_destroy
        );

    if (evidence_records == NULL)
    {
        evidence_dao_set_error_literal(
            error,
            EVIDENCE_DAO_ERROR_MEMORY,
            "Impossible d’allouer la liste des preuves."
        );

        return NULL;
    }

    statement =
        database_statement_prepare(
            evidence_dao->database,
            evidence_dao_list_all_sql
        );

    if (statement == NULL)
    {
        evidence_dao_set_database_error(
            evidence_dao,
            error,
            EVIDENCE_DAO_ERROR_PREPARE,
            "Impossible de préparer le chargement des preuves"
        );

        goto error;
    }

    while (true)
    {
        step_result =
            database_statement_step(
                statement
            );

        if (step_result == DATABASE_STATEMENT_STEP_DONE)
        {
            break;
        }

        if (step_result == DATABASE_STATEMENT_STEP_ERROR)
        {
            evidence_dao_set_database_error(
                evidence_dao,
                error,
                EVIDENCE_DAO_ERROR_EXECUTE,
                "Impossible d’exécuter le chargement des preuves"
            );

            goto error;
        }

        evidence_record =
            evidence_dao_read_current_record(
                evidence_dao,
                statement,
                error
            );

        if (evidence_record == NULL)
        {
            goto error;
        }

        g_ptr_array_add(
            evidence_records,
            evidence_record
        );

        /*
         * Le tableau devient propriétaire du modèle.
         */
        evidence_record = NULL;
    }

    database_statement_finalize(
        statement
    );

    return evidence_records;

error:

    evidence_record_free(
        evidence_record
    );

    database_statement_finalize(
        statement
    );

    g_ptr_array_unref(
        evidence_records
    );

    return NULL;
}
