#ifndef LABFY_PERSON_FACTUAL_RELATION_EDITOR_H
#define LABFY_PERSON_FACTUAL_RELATION_EDITOR_H

#include <gtk/gtk.h>
#include "core/person_creation_coordinator.h"
#include "models/identity_ocr.h"

G_BEGIN_DECLS

typedef struct PersonFactualRelationEditor PersonFactualRelationEditor;

PersonFactualRelationEditor *person_factual_relation_editor_new(void);

GtkWidget *person_factual_relation_editor_get_widget(
    PersonFactualRelationEditor *editor);

void person_factual_relation_editor_set_available_evidence(
    PersonFactualRelationEditor *editor, 
    GtkStringList *evidence_labels, 
    GPtrArray *evidence_identifiers);

void person_factual_relation_editor_set_available_ocr_runs(
    PersonFactualRelationEditor *editor, 
    GPtrArray *ocr_runs);

gboolean person_factual_relation_editor_collect_relations(
    PersonFactualRelationEditor *editor, GPtrArray **relations,
    GError **error);
guint person_factual_relation_editor_get_count(
    const PersonFactualRelationEditor *editor);
gboolean person_factual_relation_editor_validate(
    PersonFactualRelationEditor *editor, GError **error);
void person_factual_relation_editor_append_summary(
    PersonFactualRelationEditor *editor, GString *summary);

void person_factual_relation_editor_free(PersonFactualRelationEditor *editor);

G_END_DECLS

#endif
