#ifndef LABFY_PERSON_VOCABULARY_ADAPTER_H
#define LABFY_PERSON_VOCABULARY_ADAPTER_H
#include "database/database.h"
#include "models/identity_traceability.h"
#include <gtk/gtk.h>
G_BEGIN_DECLS
typedef struct PersonVocabularyAdapter PersonVocabularyAdapter;
PersonVocabularyAdapter *person_vocabulary_adapter_new(
    Database *database, GError **error);
void person_vocabulary_adapter_free(PersonVocabularyAdapter *adapter);
const GPtrArray *person_vocabulary_adapter_get_roles(
    const PersonVocabularyAdapter *adapter);
const GPtrArray *person_vocabulary_adapter_get_statuses(
    const PersonVocabularyAdapter *adapter);
GtkStringList *person_vocabulary_adapter_create_role_labels(
    const PersonVocabularyAdapter *adapter);
GtkStringList *person_vocabulary_adapter_create_status_labels(
    const PersonVocabularyAdapter *adapter);
GtkStringList *person_vocabulary_adapter_create_relation_labels(void);
const char *person_vocabulary_adapter_relation_code(guint index);
const char *person_vocabulary_adapter_relation_label(const char *code);
const char *person_vocabulary_adapter_relation_description(const char *code);
const char *person_vocabulary_adapter_status_code(
    const PersonVocabularyAdapter *adapter, guint index);
const char *person_vocabulary_adapter_status_label(
    const PersonVocabularyAdapter *adapter, guint index);
GPtrArray *person_vocabulary_adapter_selected_role_labels(
    const GPtrArray *buttons);
GPtrArray *person_vocabulary_adapter_create_role_buttons(
    const PersonVocabularyAdapter *adapter, GtkBox *container);
GPtrArray *person_vocabulary_adapter_build_role_assignments(
    const PersonVocabularyAdapter *adapter, const GPtrArray *buttons,
    const char *evidence_identifier);
gboolean person_vocabulary_adapter_justification_valid(
    const PersonRoleVocabularyEntry *entry, const char *justification);
G_END_DECLS
#endif
