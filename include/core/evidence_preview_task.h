#ifndef LABFY_INVESTIGATION_EVIDENCE_PREVIEW_TASK_H
#define LABFY_INVESTIGATION_EVIDENCE_PREVIEW_TASK_H
#include "core/background_task.h"
#include "core/evidence_preview.h"
#include "core/task_manager.h"
G_BEGIN_DECLS
BackgroundTask *evidence_preview_task_start(TaskManager *manager,
    const EvidencePreviewRequest *request,
    BackgroundTaskCompletionCallback callback, gpointer callback_data,
    GDestroyNotify callback_data_destroy, GError **error);
G_END_DECLS
#endif
