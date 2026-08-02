#include "core/document_identity_misuse_service.h"
#include "dao/identity_ocr_dao.h"
#include "dao/identity_traceability_dao.h"
#include "database/transaction.h"
static GQuark domain(void){return g_quark_from_static_string("document-identity-misuse-service");}
static const DocumentIdentityMisuseStatus statuses[]={
 {"indeterminate","Hypothèse d’usurpation indéterminée","Aucune conclusion humaine sur un usage abusif d’identité.",FALSE},
 {"presumed","Document présumé utilisé dans une usurpation d’identité","Hypothèse humaine prudente nécessitant une justification.",TRUE},
 {"confirmed","Utilisation dans une usurpation d’identité confirmée","Conclusion humaine documentée, jamais déduite automatiquement.",TRUE}};
const DocumentIdentityMisuseStatus *document_identity_misuse_service_statuses(gsize*c)
{if(c)*c=G_N_ELEMENTS(statuses);return statuses;}
const DocumentIdentityMisuseStatus *document_identity_misuse_service_status(const char*code)
{for(guint i=0;i<G_N_ELEMENTS(statuses);i++)if(g_strcmp0(code,statuses[i].code)==0)return &statuses[i];return NULL;}
GPtrArray *document_identity_misuse_service_list_runs(Database*d,const char*e,GError**error)
{IdentityOcrDao*dao=identity_ocr_dao_new(d);if(!dao)return NULL;GPtrArray*a=identity_ocr_dao_list_runs_by_evidence(dao,e,error);identity_ocr_dao_free(dao);return a;}
GPtrArray *document_identity_misuse_service_history(Database*d,const char*e,GError**error)
{IdentityTraceabilityDao*dao=identity_traceability_dao_new(d);if(!dao)return NULL;GPtrArray*a=identity_traceability_dao_list_identity_misuse(dao,e,error);identity_traceability_dao_free(dao);return a;}
gboolean document_identity_misuse_service_add(Database*d,const char*evidence,
 const char*status,const char*justification,const char*run,
 DocumentIdentityMisuseAssessment**created,GError**error)
{
 if(created)*created=NULL;
 const DocumentIdentityMisuseStatus*entry=
  document_identity_misuse_service_status(status);char*j=justification?g_strdup(justification):NULL;if(j)g_strstrip(j);
 if(!d||!g_uuid_string_is_valid(evidence)||!entry||(entry->requires_justification&&(!j||!*j))){
  g_free(j);g_set_error_literal(error,domain(),1,"Une justification est requise pour cette évaluation.");return FALSE;}
 IdentityTraceabilityDao*dao=identity_traceability_dao_new(d);GError*local=NULL;
 if(!database_transaction_begin(d)){identity_traceability_dao_free(dao);g_free(j);g_set_error_literal(error,domain(),2,"Impossible de démarrer la transaction d’usage d’identité.");return FALSE;}
 DocumentIdentityMisuseAssessment*previous=identity_traceability_dao_current_identity_misuse(dao,evidence,&local);
 if(local){database_transaction_rollback(d);g_propagate_error(error,local);identity_traceability_dao_free(dao);g_free(j);return FALSE;}
 char*id=g_uuid_string_random();GDateTime*now=g_date_time_new_now_utc();char*at=g_date_time_format(now,"%Y-%m-%dT%H:%M:%SZ");
 DocumentIdentityMisuseAssessment*a=document_identity_misuse_assessment_new(id,evidence,run,status,j&&*j?j:NULL,at,
  previous?document_identity_misuse_assessment_get_identifier(previous):NULL);
 gboolean ok=a&&identity_traceability_dao_insert_identity_misuse(dao,a,&local)&&database_transaction_commit(d);
 if(!ok&&database_transaction_is_active(d))database_transaction_rollback(d);
 if(!ok&&!local)g_set_error_literal(&local,domain(),2,"Impossible d’enregistrer l’évaluation d’usage d’identité.");
 if(ok&&created)*created=document_identity_misuse_assessment_copy(a);else if(local)g_propagate_error(error,local);
 document_identity_misuse_assessment_free(a);document_identity_misuse_assessment_free(previous);identity_traceability_dao_free(dao);
 g_date_time_unref(now);g_free(at);g_free(id);g_free(j);return ok;
}
