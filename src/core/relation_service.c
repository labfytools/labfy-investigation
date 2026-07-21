/******************************************************************************
 * @file relation_service.c
 * @brief Orchestration transactionnelle de la création des relations.
 ******************************************************************************/

#include "core/relation_service.h"

#include "dao/relation_dao.h"
#include "dao/relation_evidence_dao.h"
#include "database/error.h"
#include "database/transaction.h"

#include <glib.h>

/**
 * @brief Représentation privée du service.
 *
 * La connexion Database est empruntée.
 * Les deux DAO sont possédés.
 */
struct RelationService
{
    Database *database;

    RelationDao *relation_dao;
    RelationEvidenceDao *relation_evidence_dao;
};

#ifdef RELATION_SERVICE_ENABLE_TEST_HOOKS

#include "core/relation_service_test.h"

static gboolean relation_service_test_commit_failure =
    FALSE;

static gboolean relation_service_test_rollback_failure =
    FALSE;

/**
 * @brief Consomme un point d'injection à usage unique.
 */
static gboolean relation_service_test_take_hook(
    gboolean *hook
)
{
    gboolean enabled =
        FALSE;

    if (hook == NULL)
    {
        return FALSE;
    }

    enabled =
        *hook;

    *hook =
        FALSE;

    return enabled;
}

void relation_service_test_reset_hooks(void)
{
    relation_service_test_commit_failure =
        FALSE;

    relation_service_test_rollback_failure =
        FALSE;
}

void relation_service_test_set_commit_failure(
    gboolean enabled
)
{
    relation_service_test_commit_failure =
        enabled;
}

void relation_service_test_set_rollback_failure(
    gboolean enabled
)
{
    relation_service_test_rollback_failure =
        enabled;
}

#endif

/**
 * @brief Enregistre une erreur littérale du service.
 */
static void relation_service_set_error_literal(
    GError **error,
    RelationServiceError error_code,
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
        RELATION_SERVICE_ERROR,
        error_code,
        message
    );
}

/**
 * @brief Conserve le contexte d'une erreur provenant d'un DAO.
 */
static void relation_service_set_nested_error(
    GError **error,
    RelationServiceError error_code,
    const char *context,
    const GError *nested_error
)
{
    if (error == NULL ||
        *error != NULL)
    {
        return;
    }

    g_set_error(
        error,
        RELATION_SERVICE_ERROR,
        error_code,
        "%s : %s",
        context,
        nested_error != NULL
            ? nested_error->message
            : "erreur inconnue"
    );
}

/**
 * @brief Conserve le contexte de la dernière erreur Database.
 */
static void relation_service_set_database_error(
    GError **error,
    RelationServiceError error_code,
    const char *context,
    const Database *database
)
{
    const char *database_message =
        NULL;

    if (error == NULL ||
        *error != NULL)
    {
        return;
    }

    database_message =
        database_error_get_message(
            database
        );

    g_set_error(
        error,
        RELATION_SERVICE_ERROR,
        error_code,
        "%s : %s",
        context,
        database_message != NULL
            ? database_message
            : "erreur Database inconnue"
    );
}

/**
 * @brief Vérifie les champs obligatoires du modèle.
 */
static gboolean relation_service_record_is_valid(
    const RelationRecord *relation_record
)
{
    if (relation_record == NULL)
    {
        return FALSE;
    }

    if (relation_record_get_identifier(
            relation_record
        ) == NULL ||
        relation_record_get_source_entity_identifier(
            relation_record
        ) == NULL ||
        relation_record_get_target_entity_identifier(
            relation_record
        ) == NULL ||
        relation_record_get_relation_type(
            relation_record
        ) == NULL ||
        relation_record_get_created_at(
            relation_record
        ) == NULL ||
        relation_record_get_updated_at(
            relation_record
        ) == NULL)
    {
        return FALSE;
    }

    return TRUE;
}

/**
 * @brief Vérifie les UUID de preuves avant toute transaction.
 *
 * Le contrôle des doublons est volontairement effectué sans allocation
 * supplémentaire. Les listes de preuves liées à une relation restent
 * normalement courtes et cette validation demeure simple et déterministe.
 */
static gboolean relation_service_evidence_identifiers_are_valid(
    const GPtrArray *evidence_identifiers,
    GError **error
)
{
    guint evidence_index =
        0;

    guint previous_index =
        0;

    if (evidence_identifiers == NULL)
    {
        return TRUE;
    }

    for (evidence_index = 0;
         evidence_index < evidence_identifiers->len;
         evidence_index++)
    {
        const char *evidence_identifier =
            g_ptr_array_index(
                evidence_identifiers,
                evidence_index
            );

        if (evidence_identifier == NULL ||
            !g_uuid_string_is_valid(
                evidence_identifier
            ))
        {
            relation_service_set_error_literal(
                error,
                RELATION_SERVICE_ERROR_INVALID_ARGUMENT,
                "La demande contient un identifiant de preuve invalide."
            );

            return FALSE;
        }

        for (previous_index = 0;
             previous_index < evidence_index;
             previous_index++)
        {
            const char *previous_identifier =
                g_ptr_array_index(
                    evidence_identifiers,
                    previous_index
                );

            if (g_strcmp0(
                    evidence_identifier,
                    previous_identifier
                ) == 0)
            {
                relation_service_set_error_literal(
                    error,
                    RELATION_SERVICE_ERROR_INVALID_ARGUMENT,
                    "La demande contient plusieurs fois la même preuve."
                );

                return FALSE;
            }
        }
    }

    return TRUE;
}

/**
 * @brief Valide la transaction, sauf panne simulée par un test.
 */
static gboolean relation_service_commit_transaction(
    Database *database
)
{
#ifdef RELATION_SERVICE_ENABLE_TEST_HOOKS

    if (relation_service_test_take_hook(
            &relation_service_test_commit_failure
        ))
    {
        database_error_set(
            database,
            DATABASE_ERROR_SQLITE,
            "Échec de COMMIT simulé par RelationService."
        );

        return FALSE;
    }

#endif

    return database_transaction_commit(
        database
    );
}

/**
 * @brief Annule la transaction, sauf panne simulée par un test.
 */
static gboolean relation_service_execute_rollback(
    Database *database
)
{
#ifdef RELATION_SERVICE_ENABLE_TEST_HOOKS

    if (relation_service_test_take_hook(
            &relation_service_test_rollback_failure
        ))
    {
        /*
         * La vraie transaction est tout de même annulée afin que le test
         * ne laisse pas la connexion dans un état inutilisable.
         */
        (void) database_transaction_rollback(
            database
        );

        database_error_set(
            database,
            DATABASE_ERROR_SQLITE,
            "Échec de ROLLBACK simulé par RelationService."
        );

        return FALSE;
    }

#endif

    return database_transaction_rollback(
        database
    );
}

/**
 * @brief Annule la transaction et conserve l'erreur métier initiale.
 */
static gboolean relation_service_rollback_transaction(
    Database *database,
    GError **error
)
{
    const char *database_message =
        NULL;

    char *primary_error_message =
        NULL;

    if (database == NULL)
    {
        return FALSE;
    }

    if (!database_transaction_is_active(
            database
        ))
    {
        return TRUE;
    }

    if (relation_service_execute_rollback(
            database
        ))
    {
        return TRUE;
    }

    database_message =
        database_error_get_message(
            database
        );

    if (error == NULL)
    {
        g_warning(
            "Impossible d'annuler la transaction SQLite : %s",
            database_message != NULL
                ? database_message
                : "erreur inconnue"
        );

        return FALSE;
    }

    if (*error != NULL)
    {
        primary_error_message =
            g_strdup(
                (*error)->message
            );

        g_clear_error(
            error
        );
    }

    g_set_error(
        error,
        RELATION_SERVICE_ERROR,
        RELATION_SERVICE_ERROR_TRANSACTION_ROLLBACK,
        "Impossible d'annuler la transaction SQLite : %s. "
        "Cause initiale : %s",
        database_message != NULL
            ? database_message
            : "erreur inconnue",
        primary_error_message != NULL
            ? primary_error_message
            : "erreur inconnue"
    );

    g_free(
        primary_error_message
    );

    return FALSE;
}

GQuark relation_service_error_quark(void)
{
    return g_quark_from_static_string(
        "relation-service-error-quark"
    );
}

RelationService *relation_service_new(
    Database *database,
    GError **error
)
{
    RelationService *relation_service =
        NULL;

    RelationDao *relation_dao =
        NULL;

    RelationEvidenceDao *relation_evidence_dao =
        NULL;

    GError *dao_error =
        NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (database == NULL)
    {
        relation_service_set_error_literal(
            error,
            RELATION_SERVICE_ERROR_INVALID_ARGUMENT,
            "La connexion Database est obligatoire."
        );

        return NULL;
    }

    relation_dao =
        relation_dao_new(
            database,
            &dao_error
        );

    if (relation_dao == NULL)
    {
        relation_service_set_nested_error(
            error,
            RELATION_SERVICE_ERROR_MEMORY,
            "Impossible de créer le DAO des relations",
            dao_error
        );

        g_clear_error(
            &dao_error
        );

        return NULL;
    }

    relation_evidence_dao =
        relation_evidence_dao_new(
            database,
            &dao_error
        );

    if (relation_evidence_dao == NULL)
    {
        relation_service_set_nested_error(
            error,
            RELATION_SERVICE_ERROR_MEMORY,
            "Impossible de créer le DAO des associations relation-preuve",
            dao_error
        );

        g_clear_error(
            &dao_error
        );

        relation_dao_free(
            relation_dao
        );

        return NULL;
    }

    relation_service =
        g_try_new0(
            RelationService,
            1
        );

    if (relation_service == NULL)
    {
        relation_evidence_dao_free(
            relation_evidence_dao
        );

        relation_dao_free(
            relation_dao
        );

        relation_service_set_error_literal(
            error,
            RELATION_SERVICE_ERROR_MEMORY,
            "Impossible d'allouer le service de gestion des relations."
        );

        return NULL;
    }

    relation_service->database =
        database;

    relation_service->relation_dao =
        relation_dao;

    relation_service->relation_evidence_dao =
        relation_evidence_dao;

    return relation_service;
}

void relation_service_free(
    RelationService *relation_service
)
{
    if (relation_service == NULL)
    {
        return;
    }

    relation_evidence_dao_free(
        relation_service->relation_evidence_dao
    );

    relation_dao_free(
        relation_service->relation_dao
    );

    relation_service->relation_evidence_dao =
        NULL;

    relation_service->relation_dao =
        NULL;

    relation_service->database =
        NULL;

    g_free(
        relation_service
    );
}

gboolean relation_service_create(
    RelationService *relation_service,
    const RelationRecord *relation_record,
    const GPtrArray *evidence_identifiers,
    GError **error
)
{
    GError *dao_error =
        NULL;

    guint evidence_index =
        0;

    gboolean transaction_started =
        FALSE;

    gboolean success =
        FALSE;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (relation_service == NULL ||
        relation_service->database == NULL ||
        relation_service->relation_dao == NULL ||
        relation_service->relation_evidence_dao == NULL)
    {
        relation_service_set_error_literal(
            error,
            RELATION_SERVICE_ERROR_INVALID_ARGUMENT,
            "Le service de gestion des relations est invalide."
        );

        return FALSE;
    }

    if (!relation_service_record_is_valid(
            relation_record
        ))
    {
        relation_service_set_error_literal(
            error,
            RELATION_SERVICE_ERROR_INVALID_ARGUMENT,
            "Le modèle de relation est invalide."
        );

        return FALSE;
    }

    if (!relation_service_evidence_identifiers_are_valid(
            evidence_identifiers,
            error
        ))
    {
        return FALSE;
    }

    if (!database_transaction_begin(
            relation_service->database
        ))
    {
        relation_service_set_database_error(
            error,
            RELATION_SERVICE_ERROR_TRANSACTION_BEGIN,
            "Impossible de démarrer la transaction SQLite",
            relation_service->database
        );

        return FALSE;
    }

    transaction_started =
        TRUE;

    if (!relation_dao_insert(
            relation_service->relation_dao,
            relation_record,
            &dao_error
        ))
    {
        relation_service_set_nested_error(
            error,
            RELATION_SERVICE_ERROR_RELATION_INSERT,
            "Impossible d'enregistrer la relation",
            dao_error
        );

        goto cleanup;
    }

    g_clear_error(
        &dao_error
    );

    if (evidence_identifiers != NULL)
    {
        for (evidence_index = 0;
             evidence_index < evidence_identifiers->len;
             evidence_index++)
        {
            const char *evidence_identifier =
                g_ptr_array_index(
                    evidence_identifiers,
                    evidence_index
                );

            if (!relation_evidence_dao_link(
                    relation_service->relation_evidence_dao,
                    relation_record_get_identifier(
                        relation_record
                    ),
                    evidence_identifier,
                    &dao_error
                ))
            {
                if (error != NULL &&
                    *error == NULL)
                {
                    g_set_error(
                        error,
                        RELATION_SERVICE_ERROR,
                        RELATION_SERVICE_ERROR_EVIDENCE_LINK,
                        "Impossible d'associer la preuve %s "
                        "à la relation : %s",
                        evidence_identifier,
                        dao_error != NULL
                            ? dao_error->message
                            : "erreur DAO inconnue"
                    );
                }

                goto cleanup;
            }

            g_clear_error(
                &dao_error
            );
        }
    }

    if (!relation_service_commit_transaction(
            relation_service->database
        ))
    {
        relation_service_set_database_error(
            error,
            RELATION_SERVICE_ERROR_TRANSACTION_COMMIT,
            "Impossible de valider la transaction SQLite",
            relation_service->database
        );

        goto cleanup;
    }

    transaction_started =
        FALSE;

    success =
        TRUE;

cleanup:

    g_clear_error(
        &dao_error
    );

    if (!success &&
        transaction_started)
    {
        (void) relation_service_rollback_transaction(
            relation_service->database,
            error
        );

        transaction_started =
            FALSE;
    }

    return success;
}

