#include "models/person_role_assignment.h"
#include <string.h>

typedef struct { const char *code; const char *label; } PersonRoleDefinition;
static const PersonRoleDefinition roles[] = {
    {"alleged_author", "Auteur présumé"},
    {"presented_identity", "Identité présentée"},
    {"potentially_impersonated_identity", "Identité potentiellement usurpée"},
    {"victim", "Victime"},
    {"witness", "Témoin"},
    {"declared_bank_holder", "Titulaire bancaire déclaré"},
    {"intermediary", "Intermédiaire"},
    {"mentioned_person", "Personne citée"},
    {"other", "Autre"},
    /* Codes V6 conservés pour la lecture et la migration historique. */
    {"uncategorized", "Non catégorisée"},
    {"alleged_scammer", "Escroc présumé (historique)"},
    {"suspect", "Suspect (historique)"},
    {"related_person", "Personne liée (historique)"},
    {"impersonated_identity", "Identité usurpée (historique)"}
};

gboolean person_role_assignment_role_is_known(const char *role_code)
{
    if (role_code == NULL) return FALSE;
    for (guint i = 0; i < G_N_ELEMENTS(roles); i++)
        if (g_str_equal(role_code, roles[i].code)) return TRUE;
    return FALSE;
}

const char *person_role_assignment_role_label(const char *role_code)
{
    for (guint i = 0; role_code != NULL && i < G_N_ELEMENTS(roles); i++)
        if (g_str_equal(role_code, roles[i].code)) return roles[i].label;
    return NULL;
}

gboolean person_role_assignment_input_is_valid(
    const PersonRoleAssignmentInput *input)
{
    return input != NULL &&
        person_role_assignment_role_is_known(input->role_code) &&
        (input->evidence_identifier == NULL ||
         g_uuid_string_is_valid(input->evidence_identifier)) &&
        input->provenance_kind != NULL &&
        (g_str_equal(input->provenance_kind, "manual") ||
         g_str_equal(input->provenance_kind, "legacy_manual")) &&
        (!input->has_confidence ||
         (input->confidence >= 0 && input->confidence <= 100));
}

PersonRoleAssignmentInput *person_role_assignment_input_copy(
    const PersonRoleAssignmentInput *input)
{
    PersonRoleAssignmentInput *copy;
    if (input == NULL) return NULL;
    copy = g_new0(PersonRoleAssignmentInput, 1);
    copy->role_code = g_strdup(input->role_code);
    copy->evidence_identifier = g_strdup(input->evidence_identifier);
    copy->provenance_kind = g_strdup(input->provenance_kind);
    copy->has_confidence = input->has_confidence;
    copy->confidence = input->confidence;
    copy->notes = g_strdup(input->notes);
    return copy;
}

void person_role_assignment_input_free(PersonRoleAssignmentInput *input)
{
    if (input == NULL) return;
    g_free(input->role_code);
    g_free(input->evidence_identifier);
    g_free(input->provenance_kind);
    g_free(input->notes);
    g_free(input);
}

void person_role_assignment_record_free(PersonRoleAssignmentRecord *record)
{
    if (record == NULL) return;
    g_free(record->identifier);
    g_free(record->entity_identifier);
    g_free(record->assignment.role_code);
    g_free(record->assignment.evidence_identifier);
    g_free(record->assignment.provenance_kind);
    g_free(record->assignment.notes);
    g_free(record->created_at);
    g_free(record->updated_at);
    g_free(record);
}
