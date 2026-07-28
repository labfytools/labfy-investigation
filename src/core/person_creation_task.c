#include "core/person_creation_task.h"

struct PersonCreationTaskRequest {
    char *database_path;
    char *root;
    PersonEntityInput person;
    char *designation;
    char *declared_name;
    char *pseudonym;
    char *status;
    char *notes;
    GPtrArray *roles;
    PersonEvidenceSelection *selection;
};
PersonCreationTaskRequest *person_creation_task_request_new(
    const char *database_path, const char *root,
    const PersonEntityInput *person,
    const PersonEvidenceSelection *selection)
{
    PersonCreationTaskRequest *request;
    if (database_path == NULL || root == NULL || person == NULL) return NULL;
    request = g_new0(PersonCreationTaskRequest, 1);
    request->database_path = g_strdup(database_path);
    request->root = g_strdup(root);
    request->designation = g_strdup(person->designation);
    request->declared_name = g_strdup(person->declared_name);
    request->pseudonym = g_strdup(person->pseudonym);
    request->status = g_strdup(person->identification_status);
    request->notes = g_strdup(person->notes);
    request->roles = g_ptr_array_new_with_free_func(
        (GDestroyNotify) person_role_assignment_input_free);
    for (guint i = 0; person->role_assignments != NULL &&
         i < person->role_assignments->len; i++)
        g_ptr_array_add(request->roles,
            person_role_assignment_input_copy(g_ptr_array_index(
                (GPtrArray *) person->role_assignments, i)));
    request->person = *person;
    request->person.designation = request->designation;
    request->person.declared_name = request->declared_name;
    request->person.pseudonym = request->pseudonym;
    request->person.identification_status = request->status;
    request->person.notes = request->notes;
    request->person.evidence_identifier = NULL;
    request->person.role_assignments = request->roles;
    request->selection = person_evidence_selection_copy(selection);
    return request;
}
void person_creation_task_request_free(PersonCreationTaskRequest *request)
{
    if (request == NULL) return;
    g_free(request->database_path); g_free(request->root);
    g_free(request->designation); g_free(request->declared_name);
    g_free(request->pseudonym); g_free(request->status);
    g_free(request->notes); g_ptr_array_unref(request->roles);
    person_evidence_selection_free(request->selection);
    g_free(request);
}
static gboolean worker(BackgroundTask *task, GCancellable *cancellable,
    gpointer data, gpointer *result, GError **error)
{
    PersonCreationTaskRequest *request = data;
    Database *database;
    (void) task;
    database = database_open(request->database_path);
    if (database == NULL) {
        g_set_error_literal(error,
            g_quark_from_static_string("person-creation-task-error"), 1,
            "Impossible d’ouvrir la base de l’enquête.");
        return FALSE;
    }
    *result = person_creation_coordinator_execute(database, request->root,
        &request->person, request->selection, cancellable, error);
    database_close(database);
    return *result != NULL;
}
BackgroundTask *person_creation_task_start(TaskManager *manager,
    const PersonCreationTaskRequest *request,
    BackgroundTaskCompletionCallback callback, gpointer callback_data,
    GDestroyNotify callback_data_destroy, GError **error)
{
    BackgroundTask *task;
    PersonCreationTaskRequest *copy;
    if (manager == NULL || request == NULL) return NULL;
    task = background_task_new("Création de la personne et import des preuves");
    copy = person_creation_task_request_new(request->database_path,
        request->root, &request->person, request->selection);
    if (task == NULL || copy == NULL ||
        !task_manager_add(manager, task, error) ||
        !background_task_start(task, worker, copy,
            (GDestroyNotify) person_creation_task_request_free,
            (GDestroyNotify) person_creation_coordinator_result_free,
            callback, callback_data, callback_data_destroy, error)) {
        person_creation_task_request_free(copy);
        background_task_unref(task);
        return NULL;
    }
    return task;
}
