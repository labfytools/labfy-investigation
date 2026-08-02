#include "core/person_creation_task.h"
#include "models/person_ocr_projection.h"

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
    GPtrArray *ocr_runs;
    GPtrArray *factual_relations;
    GPtrArray *ocr_projections;
};
PersonCreationTaskRequest *person_creation_task_request_new(
    const char *database_path, const char *root,
    const PersonEntityInput *person,
    const PersonEvidenceSelection *selection, const GPtrArray *ocr_runs,
    const GPtrArray *factual_relations,const GPtrArray *ocr_projections)
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
    request->ocr_runs = g_ptr_array_new_with_free_func(
        (GDestroyNotify) identity_ocr_run_free);
    for (guint i = 0; ocr_runs != NULL && i < ocr_runs->len; i++)
        g_ptr_array_add(request->ocr_runs,
            identity_ocr_run_copy(g_ptr_array_index((GPtrArray *) ocr_runs,i)));

    request->factual_relations = NULL;
    if (factual_relations != NULL) {
        request->factual_relations = g_ptr_array_new_with_free_func(g_free);
        for (guint i = 0; i < factual_relations->len; i++) {
            PersonCreationFactualRelationInput *orig = g_ptr_array_index((GPtrArray *) factual_relations, i);
            PersonCreationFactualRelationInput *copy_rel = g_new0(PersonCreationFactualRelationInput, 1);
            copy_rel->evidence_selection_identifier = g_strdup(orig->evidence_selection_identifier);
            copy_rel->ocr_run_identifier = g_strdup(orig->ocr_run_identifier);
            copy_rel->relation_type = g_strdup(orig->relation_type);
            copy_rel->factual_note = g_strdup(orig->factual_note);
            g_ptr_array_add(request->factual_relations, copy_rel);
        }
    }
    request->ocr_projections=g_ptr_array_new_with_free_func(
        (GDestroyNotify)person_ocr_field_projection_free);
    for(guint i=0;ocr_projections&&i<ocr_projections->len;i++)
        g_ptr_array_add(request->ocr_projections,person_ocr_field_projection_copy(
            g_ptr_array_index((GPtrArray*)ocr_projections,i)));

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
    g_ptr_array_unref(request->ocr_runs);
    if (request->factual_relations) {
        for (guint i = 0; i < request->factual_relations->len; i++) {
            PersonCreationFactualRelationInput *rel = g_ptr_array_index(request->factual_relations, i);
            g_free((char*)rel->evidence_selection_identifier);
            g_free((char*)rel->ocr_run_identifier);
            g_free((char*)rel->relation_type);
            g_free((char*)rel->factual_note);
        }
        g_ptr_array_unref(request->factual_relations);
    }
    g_ptr_array_unref(request->ocr_projections);
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
    if ((request->factual_relations != NULL && request->factual_relations->len > 0)||request->ocr_projections->len>0) {
        PersonCreationCoordinatorOptions options = {0};
        options.factual_relations = request->factual_relations;
        options.ocr_projections = request->ocr_projections;
        *result = person_creation_coordinator_execute_with_options(database, request->root,
            &request->person, request->selection, request->ocr_runs, &options,
            cancellable, error);
    } else {
        *result = person_creation_coordinator_execute(database, request->root,
            &request->person, request->selection, request->ocr_runs,
            cancellable, error);
    }
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
        request->root, &request->person, request->selection,
        request->ocr_runs, request->factual_relations,request->ocr_projections);
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
