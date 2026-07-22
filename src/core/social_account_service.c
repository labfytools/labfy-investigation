/******************************************************************************
 * @file social_account_service.c
 * @brief Création transactionnelle d'entités de comptes sociaux.
 ******************************************************************************/

#include "core/social_account_service.h"

#include "dao/entity_dao.h"
#include "dao/evidence_entity_dao.h"
#include "database/statement.h"
#include "database/transaction.h"
#include "models/entity_record.h"

#include <string.h>

/** @brief Domaine d'erreur privé du service. */
#define SOCIAL_ACCOUNT_SERVICE_ERROR social_account_service_error_quark()

/** @brief Retourne le domaine d'erreur du service. */
static GQuark social_account_service_error_quark(void)
{
    return g_quark_from_static_string("social-account-service-error");
}

/** @brief Associe une plateforme au type d'entité correspondant. */
static const char *social_account_service_entity_type(const char *platform)
{
    if (g_strcmp0(platform, "tiktok") == 0) return "tiktok_account";
    if (g_strcmp0(platform, "instagram") == 0) return "instagram_account";
    if (g_strcmp0(platform, "facebook") == 0) return "facebook_account";
    if (g_strcmp0(platform, "x") == 0) return "x_account";
    if (g_strcmp0(platform, "telegram") == 0) return "telegram_account";
    if (g_strcmp0(platform, "other") == 0) return "social_account";
    return NULL;
}

/** @brief Indique si l'état du compte est pris en charge. */
static gboolean social_account_service_state_is_valid(const char *state)
{
    return g_strcmp0(state, "active") == 0 ||
           g_strcmp0(state, "private") == 0 ||
           g_strcmp0(state, "suspended") == 0 ||
           g_strcmp0(state, "deleted") == 0 ||
           g_strcmp0(state, "unknown") == 0;
}

/** @brief Lie un texte ou SQL NULL à une requête. */
static gboolean social_account_service_bind_optional(
    DatabaseStatement *statement, int index, const char *value)
{
    return value != NULL && value[0] != '\0'
        ? database_statement_bind_text(statement, index, value)
        : database_statement_bind_null(statement, index);
}

gboolean social_account_service_create(
    Database *database,
    const SocialAccountInput *input,
    char **out_entity_identifier,
    GError **error)
{
    static const char *const insert_sql =
        "INSERT INTO comptes_sociaux"
        "(entite_id, plateforme, url_profil, pseudonyme, "
        "identifiant_plateforme, premiere_observation, etat_compte, notes) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    EntityDao *entity_dao = NULL;
    EvidenceEntityDao *link_dao = NULL;
    DatabaseStatement *statement = NULL;
    EntityRecord *record = NULL;
    GDateTime *parsed_date = NULL;
    char *identifier = NULL;
    char *now_text = NULL;
    const char *type = NULL;
    gboolean transaction_active = FALSE;
    gboolean success = FALSE;

    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    if (out_entity_identifier != NULL) *out_entity_identifier = NULL;
    type = input != NULL
        ? social_account_service_entity_type(input->platform) : NULL;
    parsed_date = input != NULL && input->first_observed_at != NULL
        ? g_date_time_new_from_iso8601(input->first_observed_at, NULL) : NULL;
    if (database == NULL || input == NULL || type == NULL ||
        input->profile_url == NULL ||
        (!g_str_has_prefix(input->profile_url, "https://") &&
         !g_str_has_prefix(input->profile_url, "http://")) ||
        input->username == NULL || input->username[0] == '\0' ||
        parsed_date == NULL ||
        !social_account_service_state_is_valid(input->account_state))
    {
        g_set_error_literal(error, SOCIAL_ACCOUNT_SERVICE_ERROR, 1,
            "Les informations du compte social sont invalides.");
        goto cleanup;
    }
    identifier = g_uuid_string_random();
    {
        GDateTime *now = g_date_time_new_now_utc();
        now_text = now != NULL ? g_date_time_format(now, "%Y-%m-%dT%H:%M:%SZ") : NULL;
        g_clear_pointer(&now, g_date_time_unref);
    }
    if (identifier == NULL || now_text == NULL ||
        !database_transaction_begin(database)) goto cleanup;
    transaction_active = TRUE;
    entity_dao = entity_dao_new(database, error);
    record = entity_record_new(identifier, type, input->profile_url,
        input->username, input->notes, 50, now_text, now_text,
        ENTITY_STATUS_ACTIVE, error);
    if (entity_dao == NULL || record == NULL ||
        !entity_dao_insert(entity_dao, record, error)) goto cleanup;
    statement = database_statement_prepare(database, insert_sql);
    if (statement == NULL ||
        !database_statement_bind_text(statement, 1, identifier) ||
        !database_statement_bind_text(statement, 2, input->platform) ||
        !database_statement_bind_text(statement, 3, input->profile_url) ||
        !database_statement_bind_text(statement, 4, input->username) ||
        !social_account_service_bind_optional(statement, 5, input->platform_identifier) ||
        !database_statement_bind_text(statement, 6, input->first_observed_at) ||
        !database_statement_bind_text(statement, 7, input->account_state) ||
        !social_account_service_bind_optional(statement, 8, input->notes) ||
        database_statement_step(statement) != DATABASE_STATEMENT_STEP_DONE)
    {
        if (error != NULL && *error == NULL)
            g_set_error_literal(error, SOCIAL_ACCOUNT_SERVICE_ERROR, 2,
                "Impossible d'enregistrer la fiche du compte social.");
        goto cleanup;
    }
    if (input->evidence_identifier != NULL &&
        input->evidence_identifier[0] != '\0')
    {
        link_dao = evidence_entity_dao_new(database, error);
        if (link_dao == NULL || !evidence_entity_dao_link(
                link_dao, input->evidence_identifier, identifier, error))
            goto cleanup;
    }
    if (!database_transaction_commit(database)) goto cleanup;
    transaction_active = FALSE;
    success = TRUE;
    if (out_entity_identifier != NULL)
        *out_entity_identifier = g_strdup(identifier);

cleanup:
    if (!success && transaction_active)
        database_transaction_rollback(database);
    database_statement_finalize(statement);
    evidence_entity_dao_free(link_dao);
    entity_record_free(record);
    entity_dao_free(entity_dao);
    g_clear_pointer(&parsed_date, g_date_time_unref);
    g_free(now_text);
    g_free(identifier);
    return success;
}
