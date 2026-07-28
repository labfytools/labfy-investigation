#include "core/eml_integration.h"
#include "database/database.h"
#include "database/statement.h"
#include <assert.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <stdio.h>

static void execute_done(Database *database, const char *sql)
{
    DatabaseStatement *statement = database_statement_prepare(database, sql);
    assert(statement != NULL);
    assert(database_statement_step(statement) == DATABASE_STATEMENT_STEP_DONE);
    database_statement_finalize(statement);
}

static gint64 count_rows(Database *database, const char *table)
{
    char *sql = g_strdup_printf("SELECT COUNT(*) FROM %s;", table);
    DatabaseStatement *statement = database_statement_prepare(database, sql);
    int64_t count = -1;
    g_free(sql);
    assert(statement != NULL);
    assert(database_statement_step(statement) == DATABASE_STATEMENT_STEP_ROW);
    assert(database_statement_column_int64(statement, 0, &count));
    database_statement_finalize(statement);
    return count;
}
static char *read_observation_identifier(Database *database, const char *role)
{
    DatabaseStatement *statement = database_statement_prepare(database,
        "SELECT id FROM evidence_entity_observations WHERE role=?;");
    char *identifier = NULL;
    assert(statement != NULL && database_statement_bind_text(statement, 1, role));
    assert(database_statement_step(statement) == DATABASE_STATEMENT_STEP_ROW);
    assert(database_statement_column_text(statement, 0, &identifier));
    database_statement_finalize(statement);
    return identifier;
}

int main(void)
{
    GError *error = NULL;
    char *directory = g_dir_make_tmp("labfy-eml-integration-XXXXXX", &error);
    char *path = g_build_filename(directory, "Enquete.sqlite", NULL);
    const char *evidence_id = "10000000-0000-4000-8000-000000000099";
    assert(directory != NULL && error == NULL);
    assert(database_initialize(path, "Synthétique", directory));
    Database *database = database_open(path);
    assert(database != NULL);
    execute_done(database,
        "INSERT INTO preuves(id,name,relative_path,type_id,size_bytes,sha256,"
        "imported_at,updated_at,status,locked,original_name) VALUES("
        "'10000000-0000-4000-8000-000000000099','fixture.eml',"
        "'01_Preuves_Originales/fixture.eml',5,1,"
        "'0000000000000000000000000000000000000000000000000000000000000000',"
        "'2026-07-28T08:00:00Z','2026-07-28T08:00:00Z','active',0,"
        "'fixture.eml');");
    GPtrArray *proposals = g_ptr_array_new_with_free_func(
        (GDestroyNotify) eml_entity_proposal_free);
    EmlEntityProposal *proposal = eml_entity_proposal_new_observation(
        "email_address", "Sender@Example.test", "sender@example.test",
        "from", "from", 1, "confirmed", "header");
    g_ptr_array_add(proposals, proposal);
    guint observations = 0, created = 0, reused = 0;
    assert(eml_integration_apply(database, evidence_id, proposals,
        &observations, &created, &reused, &error));
    assert(error == NULL && observations == 1 && created == 0 && reused == 0);
    assert(count_rows(database, "evidence_entity_observations") == 1);
    assert(count_rows(database, "entites") == 0);
    assert(count_rows(database, "preuve_entites") == 0);
    assert(eml_integration_apply(database, evidence_id, proposals,
        &observations, &created, &reused, &error));
    assert(count_rows(database, "evidence_entity_observations") == 1);
    assert(count_rows(database, "entites") == 0);
    proposal->promote_to_entity = TRUE;
    assert(eml_integration_apply(database, evidence_id, proposals,
        &observations, &created, &reused, &error));
    assert(created == 1 && count_rows(database, "entites") == 1);
    assert(count_rows(database, "preuve_entites") == 1);
    assert(eml_integration_apply(database, evidence_id, proposals,
        &observations, &created, &reused, &error));
    assert(created == 0 && reused == 1 && count_rows(database, "entites") == 1);
    char *from_observation = read_observation_identifier(database, "from");
    gboolean deleted = FALSE, shared = FALSE;
    assert(eml_integration_remove_promotion(database, from_observation,
        &deleted, &shared, &error));
    assert(deleted && !shared);
    assert(count_rows(database, "evidence_entity_observations") == 1);
    assert(count_rows(database, "entites") == 0);
    assert(count_rows(database, "preuve_entites") == 0);
    assert(eml_integration_apply(database, evidence_id, proposals,
        &observations, &created, &reused, &error));
    assert(created == 1);
    EmlEntityProposal *second = eml_entity_proposal_new_observation(
        "email_address", "Sender@Example.test", "sender@example.test",
        "reply_to", "reply-to", 1, "confirmed", "header");
    second->promote_to_entity = TRUE;
    g_ptr_array_add(proposals, second);
    assert(eml_integration_apply(database, evidence_id, proposals,
        &observations, &created, &reused, &error));
    assert(count_rows(database, "evidence_entity_observations") == 2);
    assert(count_rows(database, "entites") == 1);
    assert(eml_integration_remove_promotion(database, from_observation,
        &deleted, &shared, &error));
    assert(!deleted && shared);
    assert(count_rows(database, "evidence_entity_observations") == 2);
    assert(count_rows(database, "entites") == 1);
    assert(count_rows(database, "preuve_entites") == 1);
    execute_done(database,
        "INSERT INTO entites(id,type_id,valeur,label,description,confiance,"
        "created_at,updated_at,status) VALUES("
        "'20000000-0000-4000-8000-000000000099',1,'other@example.test',"
        "'other@example.test',NULL,50,'2026-07-28T08:00:00Z',"
        "'2026-07-28T08:00:00Z','active');");
    execute_done(database,
        "INSERT INTO relations(id,entite_source_id,entite_cible_id,"
        "type_relation,label,justification,confiance,created_at,updated_at,"
        "status,relation_type_id) SELECT "
        "'30000000-0000-4000-8000-000000000099',o.entity_id,"
        "'20000000-0000-4000-8000-000000000099','supports',NULL,NULL,50,"
        "'2026-07-28T08:00:00Z','2026-07-28T08:00:00Z','active',rt.id "
        "FROM evidence_entity_observations o,relation_types rt "
        "WHERE o.role='reply_to' AND rt.code='supports';");
    char *reply_observation = read_observation_identifier(database, "reply_to");
    assert(eml_integration_remove_promotion(database, reply_observation,
        &deleted, &shared, &error));
    assert(!deleted && shared);
    assert(count_rows(database, "relations") == 1);
    assert(count_rows(database, "evidence_entity_observations") == 2);
    g_free(reply_observation);
    g_free(from_observation);
    g_ptr_array_unref(proposals);
    database_close(database);
    assert(g_remove(path) == 0 && g_rmdir(directory) == 0);
    g_free(path); g_free(directory);
    puts("EmlIntegration : tous les tests sont valides.");
    return 0;
}
