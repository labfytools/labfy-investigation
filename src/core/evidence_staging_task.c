#include "core/evidence_staging_task.h"

typedef struct {
    EvidenceStaging *staging;
    GPtrArray *paths;
} StagingTaskRequest;
static void request_free(gpointer data)
{
    StagingTaskRequest *request = data;
    if (request == NULL) return;
    evidence_staging_free(request->staging);
    g_ptr_array_unref(request->paths);
    g_free(request);
}
static gboolean worker(BackgroundTask *task, GCancellable *cancellable,
    gpointer data, gpointer *result, GError **error)
{
    StagingTaskRequest *request = data;
    GPtrArray *prepared = g_ptr_array_new_with_free_func(
        (GDestroyNotify) evidence_staging_result_free);
    for (guint i = 0; i < request->paths->len; i++) {
        EvidenceStagingResult *item;
        if (g_cancellable_set_error_if_cancelled(cancellable, error)) {
            g_ptr_array_unref(prepared);
            return FALSE;
        }
        item = evidence_staging_prepare(request->staging,
            g_ptr_array_index(request->paths, i), cancellable, error);
        if (item == NULL) {
            g_ptr_array_unref(prepared);
            return FALSE;
        }
        g_ptr_array_add(prepared, item);
        background_task_report_progress(task,
            (double) (i + 1) / request->paths->len,
            "Préparation des copies de staging");
    }
    *result = prepared;
    return TRUE;
}
BackgroundTask *evidence_staging_task_start(TaskManager *manager,
    EvidenceStaging *staging, const GPtrArray *source_paths,
    BackgroundTaskCompletionCallback callback, gpointer callback_data,
    GDestroyNotify callback_data_destroy, GError **error)
{
    BackgroundTask *task;
    StagingTaskRequest *request;
    if (manager == NULL || staging == NULL || source_paths == NULL ||
        source_paths->len == 0) return NULL;
    task = background_task_new("Préparation des preuves");
    request = g_new0(StagingTaskRequest, 1);
    request->staging = evidence_staging_ref(staging);
    request->paths = g_ptr_array_new_with_free_func(g_free);
    for (guint i = 0; i < source_paths->len; i++)
        g_ptr_array_add(request->paths,
            g_strdup(g_ptr_array_index((GPtrArray *) source_paths, i)));
    if (task == NULL || !task_manager_add(manager, task, error) ||
        !background_task_start(task, worker, request, request_free,
            (GDestroyNotify) g_ptr_array_unref, callback, callback_data,
            callback_data_destroy, error)) {
        request_free(request);
        background_task_unref(task);
        return NULL;
    }
    return task;
}
