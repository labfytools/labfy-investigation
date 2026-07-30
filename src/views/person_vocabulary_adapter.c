#include "views/person_vocabulary_adapter.h"
#include "dao/identity_traceability_dao.h"
#include "models/person_role_assignment.h"

struct PersonVocabularyAdapter {
    GPtrArray *roles;
    GPtrArray *statuses;
};

PersonVocabularyAdapter *person_vocabulary_adapter_new(
    Database *database, GError **error)
{
    IdentityTraceabilityDao *dao = identity_traceability_dao_new(database);
    PersonVocabularyAdapter *adapter = NULL;
    if (dao == NULL) return NULL;
    adapter = g_new0(PersonVocabularyAdapter, 1);
    adapter->roles = identity_traceability_dao_list_roles(
        dao, FALSE, error);
    if (adapter->roles != NULL)
        adapter->statuses =
            identity_traceability_dao_list_identification_statuses(
                dao, FALSE, error);
    identity_traceability_dao_free(dao);
    if (adapter->roles == NULL || adapter->statuses == NULL) {
        person_vocabulary_adapter_free(adapter);
        return NULL;
    }
    return adapter;
}

void person_vocabulary_adapter_free(PersonVocabularyAdapter *adapter)
{
    if (adapter == NULL) return;
    g_clear_pointer(&adapter->roles, g_ptr_array_unref);
    g_clear_pointer(&adapter->statuses, g_ptr_array_unref);
    g_free(adapter);
}

const GPtrArray *person_vocabulary_adapter_get_roles(
    const PersonVocabularyAdapter *adapter)
{ return adapter != NULL ? adapter->roles : NULL; }

const GPtrArray *person_vocabulary_adapter_get_statuses(
    const PersonVocabularyAdapter *adapter)
{ return adapter != NULL ? adapter->statuses : NULL; }

static GtkStringList *create_labels(const GPtrArray *entries)
{
    GtkStringList *labels = gtk_string_list_new(NULL);
    for (guint i = 0; entries != NULL && i < entries->len; i++) {
        PersonRoleVocabularyEntry *entry = g_ptr_array_index(
            (GPtrArray *) entries, i);
        gtk_string_list_append(labels, entry->label);
    }
    return labels;
}

GtkStringList *person_vocabulary_adapter_create_role_labels(
    const PersonVocabularyAdapter *adapter)
{ return create_labels(person_vocabulary_adapter_get_roles(adapter)); }

GtkStringList *person_vocabulary_adapter_create_status_labels(
    const PersonVocabularyAdapter *adapter)
{ return create_labels(person_vocabulary_adapter_get_statuses(adapter)); }

const char *person_vocabulary_adapter_status_code(
    const PersonVocabularyAdapter *adapter, guint index)
{
    const GPtrArray *statuses =
        person_vocabulary_adapter_get_statuses(adapter);
    return statuses != NULL && index < statuses->len
        ? ((IdentificationStatusVocabularyEntry *) g_ptr_array_index(
            (GPtrArray *) statuses, index))->code : NULL;
}

const char *person_vocabulary_adapter_status_label(
    const PersonVocabularyAdapter *adapter, guint index)
{
    const GPtrArray *statuses =
        person_vocabulary_adapter_get_statuses(adapter);
    return statuses != NULL && index < statuses->len
        ? ((IdentificationStatusVocabularyEntry *) g_ptr_array_index(
            (GPtrArray *) statuses, index))->label : "Non renseigné";
}

GPtrArray *person_vocabulary_adapter_selected_role_labels(
    const GPtrArray *buttons)
{
    GPtrArray *labels = g_ptr_array_new();
    for (guint i = 0; buttons != NULL && i < buttons->len; i++) {
        GtkCheckButton *button = g_ptr_array_index(
            (GPtrArray *) buttons, i);
        if (gtk_check_button_get_active(button))
            g_ptr_array_add(labels, (gpointer)
                gtk_check_button_get_label(button));
    }
    return labels;
}

GPtrArray *person_vocabulary_adapter_create_role_buttons(
    const PersonVocabularyAdapter *adapter, GtkBox *container)
{
    const GPtrArray *roles = person_vocabulary_adapter_get_roles(adapter);
    GPtrArray *buttons = g_ptr_array_new();
    for (guint i = 0; roles != NULL && i < roles->len; i++) {
        PersonRoleVocabularyEntry *entry =
            g_ptr_array_index((GPtrArray *) roles, i);
        GtkCheckButton *button = GTK_CHECK_BUTTON(
            gtk_check_button_new_with_label(entry->label));
        gtk_widget_set_tooltip_text(GTK_WIDGET(button), entry->description);
        g_ptr_array_add(buttons, button);
        gtk_box_append(container, GTK_WIDGET(button));
    }
    return buttons;
}

GPtrArray *person_vocabulary_adapter_build_role_assignments(
    const PersonVocabularyAdapter *adapter, const GPtrArray *buttons,
    const char *evidence_identifier)
{
    const GPtrArray *roles = person_vocabulary_adapter_get_roles(adapter);
    GPtrArray *assignments = g_ptr_array_new_with_free_func(
        (GDestroyNotify) person_role_assignment_input_free);
    for (guint i = 0; roles != NULL && buttons != NULL &&
         i < roles->len && i < buttons->len; i++) {
        if (!gtk_check_button_get_active(g_ptr_array_index(
                (GPtrArray *) buttons, i))) continue;
        PersonRoleVocabularyEntry *entry =
            g_ptr_array_index((GPtrArray *) roles, i);
        PersonRoleAssignmentInput input = {
            .role_code = entry->code,
            .evidence_identifier = (char *) evidence_identifier,
            .provenance_kind = "manual"
        };
        g_ptr_array_add(assignments,
            person_role_assignment_input_copy(&input));
    }
    return assignments;
}

gboolean person_vocabulary_adapter_justification_valid(
    const PersonRoleVocabularyEntry *entry, const char *justification)
{
    char *copy;
    gboolean valid;
    if (entry == NULL) return FALSE;
    if (!entry->requires_justification) return TRUE;
    copy = g_strdup(justification);
    if (copy != NULL) g_strstrip(copy);
    valid = copy != NULL && copy[0] != '\0' &&
        g_utf8_validate(copy, -1, NULL);
    g_free(copy);
    return valid;
}
