#ifndef LABFY_INVESTIGATION_EVIDENCE_STAGING_TASK_H
#define LABFY_INVESTIGATION_EVIDENCE_STAGING_TASK_H

#include "core/background_task.h"
#include "core/evidence_staging.h"
#include "core/task_manager.h"

G_BEGIN_DECLS
BackgroundTask *evidence_staging_task_start(TaskManager *manager,
    EvidenceStaging *staging, const GPtrArray *source_paths,
    BackgroundTaskCompletionCallback callback, gpointer callback_data,
    GDestroyNotify callback_data_destroy, GError **error);
G_END_DECLS
#endif
