#ifndef LABFY_INVESTIGATION_PERSON_CREATION_TASK_H
#define LABFY_INVESTIGATION_PERSON_CREATION_TASK_H

#include "core/background_task.h"
#include "core/person_creation_coordinator.h"
#include "core/task_manager.h"

G_BEGIN_DECLS
typedef struct PersonCreationTaskRequest PersonCreationTaskRequest;
PersonCreationTaskRequest *person_creation_task_request_new(
    const char *database_path, const char *investigation_root_path,
    const PersonEntityInput *person,
    const PersonEvidenceSelection *selection, const GPtrArray *ocr_runs);
void person_creation_task_request_free(PersonCreationTaskRequest *request);
BackgroundTask *person_creation_task_start(TaskManager *manager,
    const PersonCreationTaskRequest *request,
    BackgroundTaskCompletionCallback callback, gpointer callback_data,
    GDestroyNotify callback_data_destroy, GError **error);
G_END_DECLS
#endif
