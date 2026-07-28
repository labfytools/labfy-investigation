#include "core/evidence_preview_task.h"

static EvidencePreviewRequest *copy_request(const EvidencePreviewRequest *r)
{
    return evidence_preview_request_new(r->investigation_root_path,
        r->evidence_identifier, r->relative_path, r->expected_sha256,
        r->mime_type, r->request_generation);
}
static gboolean worker(BackgroundTask *task, GCancellable *cancellable,
    gpointer data, gpointer *result, GError **error)
{
    (void) task;
    *result = evidence_preview_load(data, cancellable, error);
    return *result != NULL;
}
BackgroundTask *evidence_preview_task_start(TaskManager *manager,
    const EvidencePreviewRequest *request,
    BackgroundTaskCompletionCallback callback, gpointer callback_data,
    GDestroyNotify callback_data_destroy, GError **error)
{
    BackgroundTask *task = NULL;
    EvidencePreviewRequest *copy = NULL;
    if (manager == NULL || request == NULL) return NULL;
    task = background_task_new("Aperçu de la preuve");
    copy = copy_request(request);
    if (task == NULL || copy == NULL ||
        !task_manager_add(manager, task, error) ||
        !background_task_start(task, worker, copy,
            (GDestroyNotify) evidence_preview_request_free,
            (GDestroyNotify) evidence_preview_result_free,
            callback, callback_data, callback_data_destroy, error)) {
        evidence_preview_request_free(copy);
        background_task_unref(task);
        return NULL;
    }
    return task;
}
