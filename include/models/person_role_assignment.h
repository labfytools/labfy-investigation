#ifndef LABFY_INVESTIGATION_PERSON_ROLE_ASSIGNMENT_H
#define LABFY_INVESTIGATION_PERSON_ROLE_ASSIGNMENT_H
#include <glib.h>
G_BEGIN_DECLS

typedef struct
{
    char *role_code;
    char *evidence_identifier;
    char *provenance_kind;
    gboolean has_confidence;
    gint confidence;
    char *notes;
} PersonRoleAssignmentInput;

typedef struct
{
    char *identifier;
    char *entity_identifier;
    PersonRoleAssignmentInput assignment;
    char *created_at;
    char *updated_at;
} PersonRoleAssignmentRecord;

gboolean person_role_assignment_role_is_known(const char *role_code);
const char *person_role_assignment_role_label(const char *role_code);
gboolean person_role_assignment_input_is_valid(
    const PersonRoleAssignmentInput *input);
PersonRoleAssignmentInput *person_role_assignment_input_copy(
    const PersonRoleAssignmentInput *input);
void person_role_assignment_input_free(PersonRoleAssignmentInput *input);
void person_role_assignment_record_free(PersonRoleAssignmentRecord *record);

G_END_DECLS
#endif
