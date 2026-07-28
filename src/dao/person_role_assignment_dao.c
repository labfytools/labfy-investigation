#include "dao/person_role_assignment_dao.h"
#include "database/statement.h"

struct PersonRoleAssignmentDao { Database *database; };
static GQuark role_dao_error(void)
{ return g_quark_from_static_string("person-role-assignment-dao-error"); }

PersonRoleAssignmentDao *person_role_assignment_dao_new(
    Database *database, GError **error)
{
    if (database == NULL) {
        g_set_error_literal(error, role_dao_error(), 1,
            "La connexion SQLite est invalide.");
        return NULL;
    }
    PersonRoleAssignmentDao *dao = g_new0(PersonRoleAssignmentDao, 1);
    dao->database = database;
    return dao;
}
void person_role_assignment_dao_free(PersonRoleAssignmentDao *dao)
{ g_free(dao); }

gboolean person_role_assignment_dao_insert(PersonRoleAssignmentDao *dao,
    const char *entity_identifier, const PersonRoleAssignmentInput *input,
    GError **error)
{
    static const char sql[] =
        "INSERT INTO person_role_assignments(id,entity_id,role_code,"
        "evidence_id,provenance_kind,confidence,notes,created_at,updated_at) "
        "SELECT ?,?,?,?,?,?,?,?,? WHERE EXISTS(SELECT 1 FROM entites e "
        "JOIN types_entite t ON t.id=e.type_id WHERE e.id=? AND t.code='person') "
        "AND (? IS NULL OR EXISTS(SELECT 1 FROM preuves WHERE id=?)) "
        "RETURNING id;";
    DatabaseStatement *statement = NULL;
    GDateTime *now = NULL;
    char *timestamp = NULL, *identifier = NULL;
    gboolean success = FALSE;
    if (dao == NULL || !g_uuid_string_is_valid(entity_identifier) ||
        !person_role_assignment_input_is_valid(input)) goto invalid;
    now = g_date_time_new_now_utc();
    timestamp = now != NULL ? g_date_time_format(now,
        "%Y-%m-%dT%H:%M:%SZ") : NULL;
    identifier = g_uuid_string_random();
    statement = database_statement_prepare(dao->database, sql);
    if (statement == NULL || timestamp == NULL || identifier == NULL ||
        !database_statement_bind_text(statement, 1, identifier) ||
        !database_statement_bind_text(statement, 2, entity_identifier) ||
        !database_statement_bind_text(statement, 3, input->role_code) ||
        !(input->evidence_identifier != NULL
          ? database_statement_bind_text(statement, 4, input->evidence_identifier)
          : database_statement_bind_null(statement, 4)) ||
        !database_statement_bind_text(statement, 5, input->provenance_kind) ||
        !(input->has_confidence
          ? database_statement_bind_int64(statement, 6, input->confidence)
          : database_statement_bind_null(statement, 6)) ||
        !(input->notes != NULL
          ? database_statement_bind_text(statement, 7, input->notes)
          : database_statement_bind_null(statement, 7)) ||
        !database_statement_bind_text(statement, 8, timestamp) ||
        !database_statement_bind_text(statement, 9, timestamp) ||
        !database_statement_bind_text(statement, 10, entity_identifier) ||
        !(input->evidence_identifier != NULL
          ? database_statement_bind_text(statement, 11, input->evidence_identifier)
          : database_statement_bind_null(statement, 11)) ||
        !(input->evidence_identifier != NULL
          ? database_statement_bind_text(statement, 12, input->evidence_identifier)
          : database_statement_bind_null(statement, 12)) ||
        database_statement_step(statement) != DATABASE_STATEMENT_STEP_ROW)
        goto failed;
    success = TRUE;
    goto cleanup;
invalid:
    g_set_error_literal(error, role_dao_error(), 1,
        "L'affectation de rôle est invalide.");
    goto cleanup;
failed:
    g_set_error_literal(error, role_dao_error(), 2,
        "La personne, la preuve ou l'affectation de rôle est invalide.");
cleanup:
    database_statement_finalize(statement);
    g_free(identifier); g_free(timestamp);
    g_clear_pointer(&now, g_date_time_unref);
    return success;
}

gboolean person_role_assignment_dao_insert_all(PersonRoleAssignmentDao *dao,
    const char *entity_identifier, const GPtrArray *inputs, GError **error)
{
    for (guint i = 0; inputs != NULL && i < inputs->len; i++)
        if (!person_role_assignment_dao_insert(dao, entity_identifier,
                g_ptr_array_index((GPtrArray *) inputs, i), error))
            return FALSE;
    return TRUE;
}

GPtrArray *person_role_assignment_dao_list_by_entity(
    PersonRoleAssignmentDao *dao, const char *entity_identifier, GError **error)
{
    static const char sql[] = "SELECT id,role_code,evidence_id,provenance_kind,"
        "confidence,notes,created_at,updated_at FROM person_role_assignments "
        "WHERE entity_id=? ORDER BY created_at,id;";
    DatabaseStatement *s = NULL;
    GPtrArray *result = NULL;
    if (dao == NULL || !g_uuid_string_is_valid(entity_identifier)) goto failed;
    s = database_statement_prepare(dao->database, sql);
    result = g_ptr_array_new_with_free_func(
        (GDestroyNotify) person_role_assignment_record_free);
    if (s == NULL || result == NULL ||
        !database_statement_bind_text(s, 1, entity_identifier)) goto failed;
    while (database_statement_step(s) == DATABASE_STATEMENT_STEP_ROW) {
        PersonRoleAssignmentRecord *r = g_new0(PersonRoleAssignmentRecord, 1);
        int64_t confidence = 0;
        bool confidence_is_null = true;
        if (!database_statement_column_text(s, 0, &r->identifier) ||
            !database_statement_column_text(s, 1, &r->assignment.role_code) ||
            !database_statement_column_text(
                s, 2, &r->assignment.evidence_identifier) ||
            !database_statement_column_text(
                s, 3, &r->assignment.provenance_kind) ||
            !database_statement_column_is_null(s, 4, &confidence_is_null) ||
            !database_statement_column_text(s, 5, &r->assignment.notes) ||
            !database_statement_column_text(s, 6, &r->created_at) ||
            !database_statement_column_text(s, 7, &r->updated_at)) {
            person_role_assignment_record_free(r);
            goto failed;
        }
        r->entity_identifier = g_strdup(entity_identifier);
        r->assignment.has_confidence = !confidence_is_null;
        if (r->assignment.has_confidence &&
            database_statement_column_int64(s, 4, &confidence))
            r->assignment.confidence = (gint) confidence;
        g_ptr_array_add(result, r);
    }
    database_statement_finalize(s);
    return result;
failed:
    database_statement_finalize(s);
    g_clear_pointer(&result, g_ptr_array_unref);
    g_set_error_literal(error, role_dao_error(), 2,
        "Impossible de lire les affectations de rôle.");
    return NULL;
}
