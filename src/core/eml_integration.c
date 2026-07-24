/******************************************************************************
 * @file eml_integration.c
 * @brief Intégration transactionnelle de propositions issues d'un EML.
 ******************************************************************************/
#include "core/eml_integration.h"
#include "core/controlled_vocab.h"
#include "dao/entity_dao.h"
#include "dao/evidence_entity_dao.h"
#include "database/transaction.h"
#include "models/entity_record.h"

EmlEntityProposal *eml_entity_proposal_new(const char *type, const char *value)
{
    return eml_entity_proposal_new_with_metadata(type, value, "proposed",
        "header");
}
EmlEntityProposal *eml_entity_proposal_new_with_metadata(const char *type,
    const char *value, const char *verification_status,
    const char *provenance_kind)
{
    EmlEntityProposal *proposal = NULL;
    if (type == NULL || type[0] == '\0' || value == NULL || value[0] == '\0') return NULL;
    proposal = g_new0(EmlEntityProposal, 1);
    proposal->type_identifier = g_strdup(type); proposal->value = g_strdup(value);
    proposal->verification_status = g_strdup(verification_status != NULL
        ? verification_status : "proposed");
    proposal->provenance_kind = g_strdup(provenance_kind != NULL
        ? provenance_kind : "header");
    if (proposal->type_identifier == NULL || proposal->value == NULL ||
        proposal->verification_status == NULL || proposal->provenance_kind == NULL)
    { eml_entity_proposal_free(proposal); return NULL; }
    return proposal;
}
void eml_entity_proposal_free(EmlEntityProposal *proposal)
{
    if (proposal == NULL) return;
    g_free(proposal->type_identifier); g_free(proposal->value);
    g_free(proposal->verification_status); g_free(proposal->provenance_kind);
    g_free(proposal);
}
/** @brief Recherche une entité existante avec le même type et la même valeur. */
static const EntityRecord *eml_integration_find_existing(const GPtrArray *entities,
    const EmlEntityProposal *proposal)
{
    for (guint i = 0; entities != NULL && i < entities->len; i++)
    {
        const EntityRecord *record = g_ptr_array_index((GPtrArray *) entities, i);
        if (g_strcmp0(entity_record_get_type_identifier(record),
                proposal->type_identifier) == 0 &&
            g_ascii_strcasecmp(entity_record_get_value(record), proposal->value) == 0)
            return record;
    }
    return NULL;
}
gboolean eml_integration_apply(Database *database, const char *evidence_identifier,
    const GPtrArray *proposals, guint *out_created, guint *out_reused, GError **error)
{
    EntityDao *entity_dao = NULL;
    EvidenceEntityDao *link_dao = NULL;
    GPtrArray *entities = NULL;
    GDateTime *now = NULL;
    char *timestamp = NULL;
    guint created = 0;
    guint reused = 0;
    gboolean active = FALSE;
    gboolean success = FALSE;

    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (out_created != NULL)
        *out_created = 0;
    if (out_reused != NULL)
        *out_reused = 0;

    if (database == NULL || evidence_identifier == NULL || proposals == NULL || proposals->len == 0)
    {
        g_set_error_literal(error,
            g_quark_from_static_string("eml-integration-error"), 1,
            "Sélectionnez au moins une proposition EML.");
        return FALSE;
    }
    for (guint i = 0; i < proposals->len; i++)
    {
        const EmlEntityProposal *proposal = g_ptr_array_index(
            (GPtrArray *) proposals, i);
        if (proposal == NULL ||
            !controlled_vocab_is_valid_verification_status(
                proposal->verification_status) ||
            !controlled_vocab_is_valid_provenance_kind(
                proposal->provenance_kind))
        {
            g_set_error_literal(error,
                g_quark_from_static_string("eml-integration-error"), 2,
                "Le statut ou la provenance d’une proposition est invalide.");
            return FALSE;
        }
        if (g_strcmp0(proposal->verification_status, "rejected") == 0 ||
            g_strcmp0(proposal->verification_status, "invalid") == 0)
        {
            g_set_error_literal(error,
                g_quark_from_static_string("eml-integration-error"), 3,
                "Une proposition rejetée ou invalide ne peut pas être intégrée.");
            return FALSE;
        }
    }
    if (!database_transaction_begin(database))
        return FALSE;
    active = TRUE;
    entity_dao = entity_dao_new(database, error);
    link_dao = evidence_entity_dao_new(database, error);
    if (entity_dao == NULL || link_dao == NULL) goto cleanup;
    entities = entity_dao_list_all(entity_dao, error); if (entities == NULL) goto cleanup;
    now = g_date_time_new_now_utc();
    timestamp = now != NULL ? g_date_time_format(now, "%Y-%m-%dT%H:%M:%SZ") : NULL;
    if (timestamp == NULL) goto cleanup;
    for (guint i = 0; i < proposals->len; i++)
    {
        EmlEntityProposal *proposal = g_ptr_array_index((GPtrArray *) proposals, i);
        const EntityRecord *existing = eml_integration_find_existing(entities, proposal);
        const char *identifier = existing != NULL
            ? entity_record_get_identifier(existing) : NULL;
        char *new_identifier = NULL; EntityRecord *new_record = NULL;
        gboolean linked = FALSE;
        if (existing == NULL)
        {
            new_identifier = g_uuid_string_random(); identifier = new_identifier;
            new_record = entity_record_new(identifier, proposal->type_identifier,
                proposal->value, proposal->value,
                "Indicateur extrait des en-têtes de la preuve EML.", 50,
                timestamp, timestamp, ENTITY_STATUS_ACTIVE, error);
            if (new_record == NULL || !entity_dao_insert(entity_dao, new_record, error))
            { entity_record_free(new_record); g_free(new_identifier); goto cleanup; }
            g_ptr_array_add(entities, new_record); new_record = NULL; created++;
        }
        else reused++;
        if (!evidence_entity_dao_exists(link_dao, evidence_identifier,
                identifier, &linked, error) ||
            (!linked && !evidence_entity_dao_link(link_dao,
                evidence_identifier, identifier, error)))
        { g_free(new_identifier); goto cleanup; }
        g_free(new_identifier);
    }
    if (!database_transaction_commit(database)) goto cleanup;
    active = FALSE; success = TRUE;
    if (out_created != NULL) *out_created = created;
    if (out_reused != NULL) *out_reused = reused;
cleanup:
    if (!success && active) database_transaction_rollback(database);
    g_free(timestamp); g_clear_pointer(&now, g_date_time_unref);
    g_clear_pointer(&entities, g_ptr_array_unref);
    evidence_entity_dao_free(link_dao); entity_dao_free(entity_dao);
    return success;
}
