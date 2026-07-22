/******************************************************************************
 * @file test_social_account_service.c
 * @brief Tests transactionnels de création des comptes sociaux.
 ******************************************************************************/

#include "core/social_account_service.h"
#include "database/database.h"
#include "database/statement.h"

#include <assert.h>
#include <stdio.h>

#include <glib.h>
#include <glib/gstdio.h>

/** @brief Lit un comptage entier dans la base de test. */
static gint64 test_social_account_count(Database *database, const char *sql)
{
    DatabaseStatement *statement = database_statement_prepare(database, sql);
    int64_t count = -1;
    assert(statement != NULL);
    assert(database_statement_step(statement) == DATABASE_STATEMENT_STEP_ROW);
    assert(database_statement_column_int64(statement, 0, &count));
    database_statement_finalize(statement);
    return count;
}

/** @brief Vérifie la création et le rollback d'un doublon. */
static void test_social_account_create_and_duplicate(void)
{
    char *directory = NULL;
    char *path = NULL;
    char *identifier = NULL;
    Database *database = NULL;
    GError *error = NULL;
    SocialAccountInput input = {
        .platform = "tiktok",
        .profile_url = "https://www.tiktok.com/@profil.test",
        .username = "@profil.test",
        .platform_identifier = "123456789",
        .first_observed_at = "2026-07-22T10:00:00Z",
        .account_state = "active",
        .notes = "URL transmise par la victime.",
        .evidence_identifier = NULL
    };
    directory = g_dir_make_tmp("labfy-social-account-test-XXXXXX", &error);
    assert(directory != NULL && error == NULL);
    path = g_build_filename(directory, "Enquete.sqlite", NULL);
    assert(database_initialize(path, "Enquete_Social", directory));
    database = database_open(path);
    assert(database != NULL);
    assert(database_migrate_to_latest(database));
    assert(social_account_service_create(database, &input, &identifier, &error));
    assert(identifier != NULL && error == NULL);
    assert(test_social_account_count(database, "SELECT COUNT(*) FROM entites;") == 1);
    assert(test_social_account_count(database, "SELECT COUNT(*) FROM comptes_sociaux;") == 1);
    g_clear_pointer(&identifier, g_free);
    assert(!social_account_service_create(database, &input, &identifier, &error));
    assert(error != NULL);
    assert(test_social_account_count(database, "SELECT COUNT(*) FROM entites;") == 1);
    assert(test_social_account_count(database, "SELECT COUNT(*) FROM comptes_sociaux;") == 1);
    g_clear_error(&error);
    database_close(database);
    assert(g_remove(path) == 0);
    assert(g_rmdir(directory) == 0);
    g_free(path);
    g_free(directory);
}

int main(void)
{
    test_social_account_create_and_duplicate();
    puts("SocialAccountService : tous les tests sont valides.");
    return 0;
}
