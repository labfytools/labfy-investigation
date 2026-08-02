#ifndef LABFY_DOCUMENT_IDENTITY_MISUSE_SERVICE_H
#define LABFY_DOCUMENT_IDENTITY_MISUSE_SERVICE_H
#include "database/database.h"
#include "models/identity_traceability.h"
#include <glib.h>
G_BEGIN_DECLS
typedef struct {const char*code,*label,*description;gboolean requires_justification;} DocumentIdentityMisuseStatus;
const DocumentIdentityMisuseStatus *document_identity_misuse_service_statuses(gsize *count);
const DocumentIdentityMisuseStatus *document_identity_misuse_service_status(const char *code);
GPtrArray *document_identity_misuse_service_list_runs(Database *database,const char *evidence_identifier,GError **error);
GPtrArray *document_identity_misuse_service_history(Database *database,const char *evidence_identifier,GError **error);
gboolean document_identity_misuse_service_add(Database *database,const char *evidence_identifier,
 const char *status,const char *justification,const char *ocr_run_identifier,
 DocumentIdentityMisuseAssessment **created,GError **error);
G_END_DECLS
#endif
