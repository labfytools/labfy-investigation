#include "core/person_details_provider.h"
#include "dao/evidence_dao.h"
#include "dao/evidence_entity_dao.h"
#include "dao/identity_traceability_dao.h"
#include "models/evidence_record.h"

GPtrArray *person_details_provider_get_evidences(Database *database, const char *person_identifier, GError **error)
{
    EvidenceEntityDao *link_dao = evidence_entity_dao_new(database, error);
    if (link_dao == NULL) return NULL;
    EvidenceDao *evidence_dao = evidence_dao_new(database, error);
    if (evidence_dao == NULL) {
        evidence_entity_dao_free(link_dao);
        return NULL;
    }
    GPtrArray *identifiers = evidence_entity_dao_list_evidence_identifiers(link_dao, person_identifier, error);
    if (identifiers == NULL) {
        evidence_dao_free(evidence_dao);
        evidence_entity_dao_free(link_dao);
        return NULL;
    }
    GPtrArray *records = g_ptr_array_new_with_free_func((GDestroyNotify)evidence_record_free);
    for (guint index = 0; index < identifiers->len; index++) {
        EvidenceRecord *record = evidence_dao_find_by_identifier(evidence_dao, g_ptr_array_index(identifiers, index), error);
        if (record == NULL) {
            g_ptr_array_unref(records);
            records = NULL;
            break;
        }
        g_ptr_array_add(records, record);
    }
    g_ptr_array_unref(identifiers);
    evidence_dao_free(evidence_dao);
    evidence_entity_dao_free(link_dao);
    return records;
}

GPtrArray *person_details_provider_get_factual_relations(Database *database, const char *person_identifier, GError **error)
{
    IdentityTraceabilityDao *trace_dao = identity_traceability_dao_new(database);
    if (trace_dao == NULL) return NULL;
    GPtrArray *relations = identity_traceability_dao_list_factual_relations_by_person(trace_dao, person_identifier, error);
    identity_traceability_dao_free(trace_dao);
    return relations;
}
