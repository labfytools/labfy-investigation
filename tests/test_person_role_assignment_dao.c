#include "dao/person_role_assignment_dao.h"
#include "dao/entity_dao.h"
#include "dao/evidence_dao.h"
#include "database/transaction.h"
#include <glib.h>
#include <glib/gstdio.h>

static EntityRecord *make_person(const char *id)
{
    GError *error = NULL;
    EntityRecord *r = entity_record_new(id, "person", id,
        "Personne SPECIMEN", NULL, 50, "2026-07-28T10:00:00Z",
        "2026-07-28T10:00:00Z", ENTITY_STATUS_ACTIVE, &error);
    g_assert_no_error(error); return r;
}
static EvidenceRecord *make_evidence(const char *id, const char *name)
{
    GError *error = NULL;
    EvidenceRecord *r = evidence_record_new(id, name, name,
        name, "document", 1,
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "2026-07-28T10:00:00Z", NULL, NULL, NULL,
        EVIDENCE_INTEGRITY_STATUS_UNKNOWN, &error);
    g_assert_no_error(error); return r;
}
static void test_dao_roles(void)
{
    GError *error = NULL;
    char *dir = g_dir_make_tmp("labfy-role-dao-XXXXXX", &error);
    char *path = g_build_filename(dir, "Enquete.sqlite", NULL);
    Database *db; EntityDao *entities; EvidenceDao *evidences;
    PersonRoleAssignmentDao *roles; EntityRecord *person;
    EvidenceRecord *evidence;
    const char *person_id = "20000000-0000-4000-8000-000000000109";
    const char *evidence_id = "10000000-0000-4000-8000-000000000109";
    const char *evidence_id_2 = "10000000-0000-4000-8000-000000000110";
    g_assert_true(database_initialize(path, "Rôles synthétiques", dir));
    db = database_open(path); g_assert_true(database_migrate_to_latest(db));
    entities = entity_dao_new(db, &error); evidences = evidence_dao_new(db, &error);
    roles = person_role_assignment_dao_new(db, &error);
    person = make_person(person_id); g_assert_true(entity_dao_insert(entities, person, &error));
    evidence = make_evidence(evidence_id, "specimen.bin");
    g_assert_true(evidence_dao_insert(evidences, evidence, &error));
    EvidenceRecord *evidence_2 = make_evidence(
        evidence_id_2, "specimen-2.bin");
    g_assert_true(evidence_dao_insert(evidences, evidence_2, &error));
    PersonRoleAssignmentInput without = {
        .role_code="victim", .provenance_kind="manual",
        .has_confidence=TRUE, .confidence=0};
    PersonRoleAssignmentInput with = {
        .role_code="witness", .evidence_identifier=(char *) evidence_id,
        .provenance_kind="manual", .has_confidence=TRUE, .confidence=100};
    g_assert_true(person_role_assignment_dao_insert(
        roles, person_id, &without, &error));
    g_assert_true(person_role_assignment_dao_insert(
        roles, person_id, &with, &error));
    PersonRoleAssignmentInput same_other_proof = with;
    same_other_proof.evidence_identifier = (char *) evidence_id_2;
    g_assert_true(person_role_assignment_dao_insert(
        roles, person_id, &same_other_proof, &error));
    GPtrArray *saved = person_role_assignment_dao_list_by_entity(
        roles, person_id, &error);
    g_assert_cmpuint(saved->len, ==, 3);
    g_ptr_array_unref(saved);
    PersonRoleAssignmentInput invalid = {
        .role_code="unknown-code", .provenance_kind="manual"};
    g_assert_false(person_role_assignment_dao_insert(
        roles, person_id, &invalid, &error)); g_clear_error(&error);
    invalid.role_code="victim"; invalid.has_confidence=TRUE; invalid.confidence=-1;
    g_assert_false(person_role_assignment_dao_insert(
        roles, person_id, &invalid, &error)); g_clear_error(&error);
    invalid.confidence=101;
    g_assert_false(person_role_assignment_dao_insert(
        roles, person_id, &invalid, &error)); g_clear_error(&error);
    with.evidence_identifier="10000000-0000-4000-8000-000000000999";
    g_assert_false(person_role_assignment_dao_insert(
        roles, person_id, &with, &error)); g_clear_error(&error);
    with.evidence_identifier=(char *) evidence_id;
    g_assert_false(person_role_assignment_dao_insert(roles,
        "20000000-0000-4000-8000-000000000999", &with, &error));
    g_clear_error(&error);
    const char *rollback_person =
        "20000000-0000-4000-8000-000000000110";
    EntityRecord *person_2 = make_person(rollback_person);
    g_assert_true(entity_dao_insert(entities, person_2, &error));
    GPtrArray *batch = g_ptr_array_new();
    PersonRoleAssignmentInput batch_valid = {
        .role_code="intermediary", .provenance_kind="manual"};
    PersonRoleAssignmentInput batch_invalid = {
        .role_code="not-known", .provenance_kind="manual"};
    g_ptr_array_add(batch, &batch_valid); g_ptr_array_add(batch, &batch_invalid);
    g_assert_true(database_transaction_begin(db));
    g_assert_false(person_role_assignment_dao_insert_all(
        roles, rollback_person, batch, &error));
    g_clear_error(&error);
    g_assert_true(database_transaction_rollback(db));
    saved = person_role_assignment_dao_list_by_entity(
        roles, rollback_person, &error);
    g_assert_cmpuint(saved->len, ==, 0);
    g_ptr_array_unref(saved); g_ptr_array_unref(batch);
    evidence_record_free(evidence_2); entity_record_free(person_2);
    evidence_record_free(evidence); entity_record_free(person);
    person_role_assignment_dao_free(roles); evidence_dao_free(evidences);
    entity_dao_free(entities); database_close(db);
    g_assert_cmpint(g_remove(path), ==, 0); g_assert_cmpint(g_rmdir(dir), ==, 0);
    g_free(path); g_free(dir);
}
int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/person-role-assignment-dao/direct", test_dao_roles);
    return g_test_run();
}
