#include "core/observation_review_service.h"
#include "database/statement.h"

#include <glib.h>

#define EVIDENCE_A "10000000-0000-4000-8000-000000000001"
#define EVIDENCE_B "10000000-0000-4000-8000-000000000002"
#define OBSERVATION_A "20000000-0000-4000-8000-000000000001"
#define OBSERVATION_B "20000000-0000-4000-8000-000000000002"
#define EXTRACTION_A "30000000-0000-4000-8000-000000000001"
#define ENTITY_SHARED "40000000-0000-4000-8000-000000000001"
#define NOW "2026-09-27T08:00:00Z"

typedef struct {
    Database *database;
    ObservationReviewService *service;
} Fixture;

static void execute_sql(Database *database, const char *sql)
{
    DatabaseStatement *statement = database_statement_prepare(database, sql);
    g_assert_nonnull(statement);
    g_assert_cmpint(database_statement_step(statement), ==,
        DATABASE_STATEMENT_STEP_DONE);
    database_statement_finalize(statement);
}

static gint64 query_int(Database *database, const char *sql)
{
    DatabaseStatement *statement = database_statement_prepare(database, sql);
    int64_t value = -1;
    g_assert_nonnull(statement);
    g_assert_cmpint(database_statement_step(statement), ==,
        DATABASE_STATEMENT_STEP_ROW);
    g_assert_true(database_statement_column_int64(statement, 0, &value));
    database_statement_finalize(statement);
    return value;
}

static char *query_text(Database *database, const char *sql)
{
    DatabaseStatement *statement = database_statement_prepare(database, sql);
    char *value = NULL;
    g_assert_nonnull(statement);
    g_assert_cmpint(database_statement_step(statement), ==,
        DATABASE_STATEMENT_STEP_ROW);
    g_assert_true(database_statement_column_text(statement, 0, &value));
    database_statement_finalize(statement);
    return value;
}

static void create_schema(Database *database)
{
    execute_sql(database, "CREATE TABLE preuves(id TEXT PRIMARY KEY);");
    execute_sql(database, "CREATE TABLE extractions(id TEXT PRIMARY KEY);");
    execute_sql(database, "CREATE TABLE types_entite(id INTEGER PRIMARY KEY,"
        "code TEXT UNIQUE,label TEXT);");
    execute_sql(database, "INSERT INTO types_entite VALUES"
        "(1,'email_address','Adresse email'),"
        "(11,'domain_name','Nom de domaine'),(12,'ip_address','Adresse IP');");
    execute_sql(database, "CREATE TABLE entites(id TEXT PRIMARY KEY,type_id INTEGER "
        "NOT NULL,valeur TEXT NOT NULL,label TEXT,description TEXT,confiance INTEGER "
        "NOT NULL DEFAULT 50,created_at TEXT NOT NULL,updated_at TEXT NOT NULL,"
        "status TEXT NOT NULL DEFAULT 'active',UNIQUE(type_id,valeur),"
        "FOREIGN KEY(type_id) REFERENCES types_entite(id));");
    execute_sql(database, "CREATE TABLE preuve_entites(preuve_id TEXT NOT NULL,"
        "entite_id TEXT NOT NULL,PRIMARY KEY(preuve_id,entite_id),"
        "FOREIGN KEY(preuve_id) REFERENCES preuves(id),"
        "FOREIGN KEY(entite_id) REFERENCES entites(id));");
    execute_sql(database, "CREATE TABLE preuve_entite_sources(id TEXT PRIMARY KEY,"
        "preuve_id TEXT NOT NULL,entite_id TEXT NOT NULL,source_kind TEXT NOT NULL,"
        "source_uuid TEXT,created_at TEXT NOT NULL,UNIQUE(preuve_id,entite_id,"
        "source_kind,source_uuid),FOREIGN KEY(preuve_id,entite_id) REFERENCES "
        "preuve_entites(preuve_id,entite_id) ON DELETE CASCADE);");
    execute_sql(database, "CREATE TABLE evidence_entity_observations("
        "id TEXT PRIMARY KEY,evidence_id TEXT NOT NULL,entity_id TEXT,"
        "entity_type TEXT NOT NULL,value_raw TEXT NOT NULL,value_normalized TEXT,"
        "value_corrected TEXT,role TEXT NOT NULL,provenance_kind TEXT NOT NULL,"
        "source_header TEXT NOT NULL,occurrence INTEGER NOT NULL,"
        "verification_status TEXT NOT NULL,extraction_id TEXT,warning TEXT,"
        "observed_at TEXT NOT NULL,integrated_at TEXT NOT NULL,promoted_at TEXT,"
        "promotion_kind TEXT,FOREIGN KEY(evidence_id) REFERENCES preuves(id),"
        "FOREIGN KEY(entity_id) REFERENCES entites(id),"
        "FOREIGN KEY(extraction_id) REFERENCES extractions(id));");
    execute_sql(database, "CREATE TABLE journal(id TEXT PRIMARY KEY,event_time TEXT "
        "NOT NULL,action TEXT NOT NULL,objet_type TEXT,objet_id TEXT,resultat TEXT "
        "NOT NULL,details TEXT,acteur TEXT,created_at TEXT NOT NULL);");
    execute_sql(database, "INSERT INTO preuves VALUES('" EVIDENCE_A "');");
    execute_sql(database, "INSERT INTO preuves VALUES('" EVIDENCE_B "');");
    execute_sql(database, "INSERT INTO extractions VALUES('" EXTRACTION_A "');");
    execute_sql(database, "INSERT INTO evidence_entity_observations(id,evidence_id,"
        "entity_type,value_raw,value_normalized,role,provenance_kind,source_header,"
        "occurrence,verification_status,extraction_id,observed_at,integrated_at) "
        "VALUES('" OBSERVATION_A "','" EVIDENCE_A "','email_address',"
        "' Alice@Example.Test ','alice@example.test','from','eml','from',1,"
        "'proposed','" EXTRACTION_A "','" NOW "','" NOW "');");
    execute_sql(database, "INSERT INTO evidence_entity_observations(id,evidence_id,"
        "entity_type,value_raw,value_normalized,role,provenance_kind,source_header,"
        "occurrence,verification_status,observed_at,integrated_at) VALUES("
        "'" OBSERVATION_B "','" EVIDENCE_B "','domain_name','Example.Test',"
        "'example.test','host','eml','received',1,'proposed','" NOW "','" NOW "');");
}

static void fixture_setup(Fixture *fixture, gconstpointer unused)
{
    GError *error = NULL;
    (void) unused;
    fixture->database = database_open(":memory:");
    g_assert_nonnull(fixture->database);
    create_schema(fixture->database);
    fixture->service = observation_review_service_new(fixture->database, &error);
    g_assert_no_error(error); g_assert_nonnull(fixture->service);
}

static void fixture_teardown(Fixture *fixture, gconstpointer unused)
{
    (void) unused;
    observation_review_service_free(fixture->service);
    database_close(fixture->database);
}

static ObservationReviewRequest request_base(const char *operation,
    guint64 revision, ObservationReviewAction action)
{
    ObservationReviewRequest request = {
        .operation_identifier = operation,
        .evidence_identifier = EVIDENCE_A,
        .observation_identifier = OBSERVATION_A,
        .expected_revision = revision,
        .action = action,
        .author = "analyste-local",
        .reason = "Revue humaine du spécimen",
        .occurred_at = NOW
    };
    return request;
}

static void test_decision_and_correction_preserve_source(Fixture *fixture,
    gconstpointer unused)
{
    ObservationReviewResult result = {0}; GError *error = NULL;
    ObservationReviewRequest decide = request_base(
        "50000000-0000-4000-8000-000000000001", 0,
        OBSERVATION_REVIEW_ACTION_DECIDE);
    ObservationReviewRequest correct = request_base(
        "50000000-0000-4000-8000-000000000002", 1,
        OBSERVATION_REVIEW_ACTION_CORRECT);
    char *value = NULL;
    (void) unused;
    decide.verification_status = "confirmed";
    g_assert_true(observation_review_service_apply(fixture->service, &decide,
        &result, &error));
    g_assert_no_error(error); g_assert_cmpuint(result.revision, ==, 1);
    observation_review_result_clear(&result);
    correct.corrected_value = "alice+review@example.test";
    g_assert_true(observation_review_service_apply(fixture->service, &correct,
        &result, &error));
    g_assert_no_error(error); g_assert_cmpuint(result.revision, ==, 2);
    value = query_text(fixture->database,
        "SELECT value_raw||'|'||value_normalized||'|'||extraction_id||'|'||"
        "verification_status||'|'||value_corrected FROM "
        "evidence_entity_observations WHERE id='" OBSERVATION_A "';");
    g_assert_cmpstr(value, ==, " Alice@Example.Test |alice@example.test|"
        EXTRACTION_A "|confirmed|alice+review@example.test");
    g_free(value);
    g_assert_cmpint(query_int(fixture->database,
        "SELECT COUNT(*) FROM journal;"), ==, 2);
    gboolean accepted = FALSE;
    g_assert_true(observation_review_service_validate_replay(fixture->service,
        EVIDENCE_A, OBSERVATION_A, EXTRACTION_A, "email_address",
        " Alice@Example.Test ", "alice@example.test", "from", "eml",
        "from", 1, &accepted, &error));
    g_assert_true(accepted);
    g_assert_true(observation_review_service_validate_replay(fixture->service,
        EVIDENCE_A, OBSERVATION_A, EXTRACTION_A, "email_address",
        "valeur brute altérée", "alice@example.test", "from", "eml",
        "from", 1, &accepted, &error));
    g_assert_false(accepted);
    observation_review_result_clear(&result);

    const char *states[] = {"rejected", "conflicted", "proposed"};
    const char *operations[] = {
        "50000000-0000-4000-8000-000000000010",
        "50000000-0000-4000-8000-000000000011",
        "50000000-0000-4000-8000-000000000012"
    };
    for (guint index = 0; index < G_N_ELEMENTS(states); index++) {
        ObservationReviewRequest transition = request_base(operations[index],
            2U + index, OBSERVATION_REVIEW_ACTION_DECIDE);
        transition.verification_status = states[index];
        g_assert_true(observation_review_service_apply(fixture->service,
            &transition, &result, &error));
        g_assert_no_error(error);
        observation_review_result_clear(&result);
    }
    value = query_text(fixture->database, "SELECT verification_status FROM "
        "evidence_entity_observations WHERE id='" OBSERVATION_A "';");
    g_assert_cmpstr(value, ==, "proposed"); g_free(value);
}

static void test_foreign_observation_is_rejected(Fixture *fixture,
    gconstpointer unused)
{
    ObservationReviewResult result = {0}; GError *error = NULL;
    ObservationReviewRequest request = request_base(
        "50000000-0000-4000-8000-000000000003", 0,
        OBSERVATION_REVIEW_ACTION_DECIDE);
    (void) unused;
    request.observation_identifier = OBSERVATION_B;
    request.verification_status = "rejected";
    g_assert_false(observation_review_service_apply(fixture->service, &request,
        &result, &error));
    g_assert_error(error, OBSERVATION_REVIEW_ERROR,
        OBSERVATION_REVIEW_ERROR_NOT_FOUND);
    g_clear_error(&error);
    g_assert_cmpint(query_int(fixture->database,
        "SELECT COUNT(*) FROM journal;"), ==, 0);
}

static void test_revision_and_idempotence(Fixture *fixture,
    gconstpointer unused)
{
    ObservationReviewResult result = {0}; GError *error = NULL;
    ObservationReviewRequest first = request_base(
        "50000000-0000-4000-8000-000000000004", 0,
        OBSERVATION_REVIEW_ACTION_DECIDE);
    ObservationReviewRequest stale = request_base(
        "50000000-0000-4000-8000-000000000005", 0,
        OBSERVATION_REVIEW_ACTION_DECIDE);
    (void) unused;
    first.verification_status = "confirmed";
    stale.verification_status = "rejected";
    g_assert_true(observation_review_service_apply(fixture->service, &first,
        &result, &error));
    observation_review_result_clear(&result);
    g_assert_true(observation_review_service_apply(fixture->service, &first,
        &result, &error));
    g_assert_true(result.replayed); g_assert_cmpuint(result.revision, ==, 1);
    observation_review_result_clear(&result);
    g_assert_false(observation_review_service_apply(fixture->service, &stale,
        &result, &error));
    g_assert_error(error, OBSERVATION_REVIEW_ERROR,
        OBSERVATION_REVIEW_ERROR_CONFLICT); g_clear_error(&error);
    first.reason = "Intention divergente";
    g_assert_false(observation_review_service_apply(fixture->service, &first,
        &result, &error));
    g_assert_error(error, OBSERVATION_REVIEW_ERROR,
        OBSERVATION_REVIEW_ERROR_CONFLICT); g_clear_error(&error);
    g_assert_cmpint(query_int(fixture->database,
        "SELECT COUNT(*) FROM journal;"), ==, 1);
}

static void test_journal_failure_rolls_back(Fixture *fixture,
    gconstpointer unused)
{
    ObservationReviewResult result = {0}; GError *error = NULL;
    ObservationReviewRequest request = request_base(
        "50000000-0000-4000-8000-000000000006", 0,
        OBSERVATION_REVIEW_ACTION_DECIDE);
    char *status;
    (void) unused;
    request.verification_status = "conflicted";
    execute_sql(fixture->database, "CREATE TRIGGER reject_review_journal BEFORE "
        "INSERT ON journal BEGIN SELECT RAISE(ABORT,'SPECIMEN journal failure'); END;");
    g_test_expect_message(NULL, G_LOG_LEVEL_WARNING,
        "*SPECIMEN journal failure*");
    g_assert_false(observation_review_service_apply(fixture->service, &request,
        &result, &error));
    g_test_assert_expected_messages();
    g_assert_error(error, OBSERVATION_REVIEW_ERROR,
        OBSERVATION_REVIEW_ERROR_DATABASE); g_clear_error(&error);
    status = query_text(fixture->database, "SELECT verification_status FROM "
        "evidence_entity_observations WHERE id='" OBSERVATION_A "';");
    g_assert_cmpstr(status, ==, "proposed"); g_free(status);
    g_assert_cmpint(query_int(fixture->database,
        "SELECT COUNT(*) FROM journal;"), ==, 0);
}

static void test_promotion_create_attach_and_withdraw(Fixture *fixture,
    gconstpointer unused)
{
    ObservationReviewResult result = {0}; GError *error = NULL;
    ObservationReviewRequest create = request_base(
        "50000000-0000-4000-8000-000000000007", 0,
        OBSERVATION_REVIEW_ACTION_PROMOTE_CREATE);
    ObservationReviewRequest withdraw = request_base(
        "50000000-0000-4000-8000-000000000008", 1,
        OBSERVATION_REVIEW_ACTION_WITHDRAW_PROMOTION);
    ObservationReviewRequest attach = request_base(
        "50000000-0000-4000-8000-000000000009", 2,
        OBSERVATION_REVIEW_ACTION_PROMOTE_ATTACH);
    char *created_id = NULL;
    (void) unused;
    g_assert_true(observation_review_service_apply(fixture->service, &create,
        &result, &error));
    g_assert_true(result.entity_created); g_assert_nonnull(result.entity_identifier);
    created_id = g_strdup(result.entity_identifier);
    observation_review_result_clear(&result);
    /* Une source manuelle partage le lien : le retrait ne doit pas le supprimer. */
    execute_sql(fixture->database, "INSERT INTO preuve_entite_sources VALUES("
        "'60000000-0000-4000-8000-000000000001','" EVIDENCE_A "',(SELECT "
        "entity_id FROM evidence_entity_observations WHERE id='" OBSERVATION_A "'),"
        "'manual',NULL,'" NOW "');");
    g_assert_true(observation_review_service_apply(fixture->service, &withdraw,
        &result, &error));
    g_assert_cmpint(query_int(fixture->database,
        "SELECT COUNT(*) FROM preuve_entites;"), ==, 1);
    g_assert_cmpint(query_int(fixture->database,
        "SELECT COUNT(*) FROM entites;"), ==, 1);
    observation_review_result_clear(&result);
    attach.entity_identifier = created_id;
    g_assert_true(observation_review_service_apply(fixture->service, &attach,
        &result, &error));
    g_assert_false(result.entity_created);
    g_assert_cmpstr(result.entity_identifier, ==, created_id);
    g_assert_cmpint(query_int(fixture->database,
        "SELECT COUNT(*) FROM evidence_entity_observations WHERE id='"
        OBSERVATION_A "';"), ==, 1);
    g_assert_cmpint(query_int(fixture->database,
        "SELECT COUNT(*) FROM preuve_entite_sources;"), ==, 2);
    observation_review_result_clear(&result); g_free(created_id);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add("/observation-review/decision-correction", Fixture, NULL,
        fixture_setup, test_decision_and_correction_preserve_source,
        fixture_teardown);
    g_test_add("/observation-review/foreign-observation", Fixture, NULL,
        fixture_setup, test_foreign_observation_is_rejected, fixture_teardown);
    g_test_add("/observation-review/revision-idempotence", Fixture, NULL,
        fixture_setup, test_revision_and_idempotence, fixture_teardown);
    g_test_add("/observation-review/journal-rollback", Fixture, NULL,
        fixture_setup, test_journal_failure_rolls_back, fixture_teardown);
    g_test_add("/observation-review/promotion-withdraw", Fixture, NULL,
        fixture_setup, test_promotion_create_attach_and_withdraw,
        fixture_teardown);
    return g_test_run();
}
