#ifndef LABFY_EVIDENCE_IDENTITY_IMPORT_TASK_H
#define LABFY_EVIDENCE_IDENTITY_IMPORT_TASK_H

#include "core/background_task.h"
#include "core/task_manager.h"
#include "models/identity_ocr.h"
#include "core/person_creation_coordinator.h"

G_BEGIN_DECLS

typedef struct EvidenceIdentityImportTaskRequest
    EvidenceIdentityImportTaskRequest;

EvidenceIdentityImportTaskRequest *
evidence_identity_import_task_request_new(
    const char *database_path, const char *investigation_root_path,
    const char *source_path, const char *person_identifier,
    const char *type_identifier, const char *collected_at,
    const char *source, const char *description,
    const IdentityOcrRun *run);
EvidenceIdentityImportTaskRequest *
evidence_identity_import_task_request_new_existing(
    const char *database_path, const char *investigation_root_path,
    const char *evidence_identifier, const char *person_identifier,
    const IdentityOcrRun *run);
EvidenceIdentityImportTaskRequest *
evidence_identity_import_task_request_new_review(
    const char *database_path, const char *investigation_root_path,
    const char *evidence_identifier, const IdentityOcrRun *run);
void evidence_identity_import_task_request_free(
    EvidenceIdentityImportTaskRequest *request);
void evidence_identity_import_task_request_set_session_check(
    EvidenceIdentityImportTaskRequest *request,
    PersonCreationSessionCheck session_check,
    gpointer session_check_data);
BackgroundTask *evidence_identity_import_task_start(
    TaskManager *manager,
    const EvidenceIdentityImportTaskRequest *request,
    BackgroundTaskCompletionCallback callback,
    gpointer callback_data, GDestroyNotify callback_data_destroy,
    GError **error);

G_END_DECLS

#endif
