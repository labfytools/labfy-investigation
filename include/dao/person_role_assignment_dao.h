#ifndef LABFY_INVESTIGATION_PERSON_ROLE_ASSIGNMENT_DAO_H
#define LABFY_INVESTIGATION_PERSON_ROLE_ASSIGNMENT_DAO_H
#include "database/database.h"
#include "models/person_role_assignment.h"
#include <glib.h>
G_BEGIN_DECLS
typedef struct PersonRoleAssignmentDao PersonRoleAssignmentDao;
PersonRoleAssignmentDao *person_role_assignment_dao_new(
    Database *database, GError **error);
void person_role_assignment_dao_free(PersonRoleAssignmentDao *dao);
gboolean person_role_assignment_dao_insert(PersonRoleAssignmentDao *dao,
    const char *entity_identifier, const PersonRoleAssignmentInput *input,
    GError **error);
gboolean person_role_assignment_dao_insert_all(PersonRoleAssignmentDao *dao,
    const char *entity_identifier, const GPtrArray *inputs, GError **error);
GPtrArray *person_role_assignment_dao_list_by_entity(
    PersonRoleAssignmentDao *dao, const char *entity_identifier,
    GError **error);
G_END_DECLS
#endif
