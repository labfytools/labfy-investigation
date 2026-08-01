#ifndef LABFY_IDENTITY_TRACEABILITY_DAO_H
#define LABFY_IDENTITY_TRACEABILITY_DAO_H
#include "database/database.h"
#include "models/identity_traceability.h"
#include <glib.h>
G_BEGIN_DECLS
typedef struct IdentityTraceabilityDao IdentityTraceabilityDao;
IdentityTraceabilityDao *identity_traceability_dao_new(Database *database);
void identity_traceability_dao_free(IdentityTraceabilityDao *dao);
gboolean identity_traceability_dao_insert_authenticity(
 IdentityTraceabilityDao *dao,const DocumentAuthenticityAssessment *assessment,
 GError **error);
DocumentAuthenticityAssessment *identity_traceability_dao_find_authenticity(
 IdentityTraceabilityDao *dao,const char *identifier,GError **error);
GPtrArray *identity_traceability_dao_list_authenticity(
 IdentityTraceabilityDao *dao,const char *evidence_identifier,GError **error);
DocumentAuthenticityAssessment *identity_traceability_dao_current_authenticity(
 IdentityTraceabilityDao *dao,const char *evidence_identifier,GError **error);
gboolean identity_traceability_dao_insert_factual_relation(
 IdentityTraceabilityDao *dao,const PersonEvidenceFactualRelation *relation,
 GError **error);
GPtrArray *identity_traceability_dao_list_factual_relations(
 IdentityTraceabilityDao *dao,const char *evidence_identifier,GError **error);
GPtrArray *identity_traceability_dao_list_factual_relations_by_evidence(
 IdentityTraceabilityDao *dao,const char *evidence_identifier,GError **error);
GPtrArray *identity_traceability_dao_list_factual_relations_by_person(
 IdentityTraceabilityDao *dao,const char *person_identifier,GError **error);
GPtrArray *identity_traceability_dao_list_roles(
 IdentityTraceabilityDao *dao,gboolean include_inactive,GError **error);
GPtrArray *identity_traceability_dao_list_identification_statuses(
 IdentityTraceabilityDao *dao,gboolean include_inactive,GError **error);
G_END_DECLS
#endif
