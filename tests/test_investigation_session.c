/******************************************************************************
 * @file test_investigation_session.c
 * @brief Tests du contexte représentant une enquête ouverte.
 ******************************************************************************/

#include "core/investigation_session.h"

#include "core/investigation_project.h"
#include "dao/investigation_dao.h"
#include "database/database.h"
#include "models/investigation_record.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

static gboolean test_remove_path_recursively(
    const char *path
)
{
    GDir *directory = NULL;
    const char *entry_name = NULL;

    if (path == NULL)
    {
        return FALSE;
    }

    if (!g_file_test(
            path,
            G_FILE_TEST_EXISTS
        ))
    {
        return TRUE;
    }

    if (!g_file_test(
            path,
            G_FILE_TEST_IS_DIR
        ))
    {
        return g_remove(path) == 0;
    }

    directory = g_dir_open(
        path,
        0,
        NULL
    );

    if (directory == NULL)
    {
        return FALSE;
    }

    while ((entry_name = g_dir_read_name(directory)) != NULL)
    {
        char *entry_path = NULL;
        gboolean removed = FALSE;

        entry_path = g_build_filename(
            path,
            entry_name,
            NULL
        );

        if (entry_path == NULL)
        {
            g_dir_close(directory);
            return FALSE;
        }

        removed = test_remove_path_recursively(
            entry_path
        );

        g_free(entry_path);

        if (!removed)
        {
            g_dir_close(directory);
            return FALSE;
        }
    }

    g_dir_close(directory);

    return g_rmdir(path) == 0;
}

static void test_assert_session_error(
    const GError *error,
    InvestigationSessionError expected_code
)
{
    assert(error != NULL);
    assert(error->domain == INVESTIGATION_SESSION_ERROR);
    assert(error->code == (gint) expected_code);
    assert(error->message != NULL);
    assert(error->message[0] != '\0');
}

static void test_open_valid_session(void)
{
    char *temporary_parent = NULL;
    char *investigation_path = NULL;
    char *canonical_root_path = NULL;

    InvestigationSession *session = NULL;
    const InvestigationProject *project = NULL;
    const InvestigationRecord *record = NULL;
    InvestigationRecord *reloaded_record = NULL;
    Database *database = NULL;

    const char *record_id = NULL;
    const char *reloaded_record_id = NULL;

    GError *error = NULL;

    temporary_parent = g_dir_make_tmp(
        "labfy-investigation-session-valid-XXXXXX",
        &error
    );

    assert(temporary_parent != NULL);
    assert(error == NULL);

    investigation_path = investigation_project_create(
        temporary_parent,
        "Enquete_Session"
    );

    assert(investigation_path != NULL);

    canonical_root_path = g_canonicalize_filename(
        investigation_path,
        NULL
    );

    assert(canonical_root_path != NULL);

    session = investigation_session_open(
        investigation_path,
        &error
    );

    assert(session != NULL);
    assert(error == NULL);

    project = investigation_session_get_project(
        session
    );

    database = investigation_session_get_database(
        session
    );

    record = investigation_session_get_record(
        session
    );

    assert(project != NULL);
    assert(database != NULL);
    assert(record != NULL);

    assert(
        strcmp(
            investigation_project_get_root_path(project),
            canonical_root_path
        ) == 0
    );

    assert(
        strcmp(
            investigation_record_get_name(record),
            "Enquete_Session"
        ) == 0
    );

    assert(
        strcmp(
            investigation_record_get_root_path(record),
            canonical_root_path
        ) == 0
    );

    record_id = investigation_record_get_id(
        record
    );

    assert(record_id != NULL);
    assert(g_uuid_string_is_valid(record_id));

    assert(
        investigation_record_get_created_at(record) != NULL
    );

    assert(
        investigation_record_get_created_at(record)[0] != '\0'
    );

    assert(
        investigation_record_get_updated_at(record) != NULL
    );

    assert(
        investigation_record_get_updated_at(record)[0] != '\0'
    );

    reloaded_record = investigation_dao_load(
        database
    );

    assert(reloaded_record != NULL);

    reloaded_record_id = investigation_record_get_id(
        reloaded_record
    );

    assert(reloaded_record_id != NULL);

    assert(
        strcmp(
            record_id,
            reloaded_record_id
        ) == 0
    );

    investigation_record_free(reloaded_record);
    investigation_session_close(session);

    assert(
        test_remove_path_recursively(
            temporary_parent
        )
    );

    g_free(canonical_root_path);
    g_free(investigation_path);
    g_free(temporary_parent);
}

static void test_open_invalid_parameters(void)
{
    InvestigationSession *session = NULL;
    GError *error = NULL;

    session = investigation_session_open(
        NULL,
        &error
    );

    assert(session == NULL);

    test_assert_session_error(
        error,
        INVESTIGATION_SESSION_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(&error);

    session = investigation_session_open(
        "",
        &error
    );

    assert(session == NULL);

    test_assert_session_error(
        error,
        INVESTIGATION_SESSION_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(&error);

    assert(
        investigation_session_open(
            NULL,
            NULL
        ) == NULL
    );

    assert(
        investigation_session_open(
            "",
            NULL
        ) == NULL
    );
}

static void test_open_missing_root(void)
{
    char *temporary_parent = NULL;
    char *missing_root_path = NULL;

    InvestigationSession *session = NULL;

    GError *error = NULL;

    temporary_parent = g_dir_make_tmp(
        "labfy-investigation-session-missing-root-XXXXXX",
        &error
    );

    assert(temporary_parent != NULL);
    assert(error == NULL);

    missing_root_path = g_build_filename(
        temporary_parent,
        "does-not-exist",
        NULL
    );

    assert(missing_root_path != NULL);

    session = investigation_session_open(
        missing_root_path,
        &error
    );

    assert(session == NULL);

    test_assert_session_error(
        error,
        INVESTIGATION_SESSION_ERROR_ROOT_NOT_FOUND
    );

    g_clear_error(&error);

    assert(
        !g_file_test(
            missing_root_path,
            G_FILE_TEST_EXISTS
        )
    );

    assert(
        test_remove_path_recursively(
            temporary_parent
        )
    );

    g_free(missing_root_path);
    g_free(temporary_parent);
}

static void test_open_root_is_file(void)
{
    char *temporary_parent = NULL;
    char *file_path = NULL;

    InvestigationSession *session = NULL;

    GError *error = NULL;

    temporary_parent = g_dir_make_tmp(
        "labfy-investigation-session-root-file-XXXXXX",
        &error
    );

    assert(temporary_parent != NULL);
    assert(error == NULL);

    file_path = g_build_filename(
        temporary_parent,
        "not-a-directory.txt",
        NULL
    );

    assert(file_path != NULL);

    assert(
        g_file_set_contents(
            file_path,
            "test\n",
            -1,
            &error
        )
    );

    assert(error == NULL);

    session = investigation_session_open(
        file_path,
        &error
    );

    assert(session == NULL);

    test_assert_session_error(
        error,
        INVESTIGATION_SESSION_ERROR_ROOT_NOT_FOUND
    );

    g_clear_error(&error);

    assert(
        test_remove_path_recursively(
            temporary_parent
        )
    );

    g_free(file_path);
    g_free(temporary_parent);
}

static void test_open_missing_database(void)
{
    char *investigation_root = NULL;
    char *database_directory = NULL;
    char *database_path = NULL;

    InvestigationSession *session = NULL;

    GError *error = NULL;

    investigation_root = g_dir_make_tmp(
        "labfy-investigation-session-missing-database-XXXXXX",
        &error
    );

    assert(investigation_root != NULL);
    assert(error == NULL);

    database_directory = g_build_filename(
        investigation_root,
        "00_BaseDeDonnees",
        NULL
    );

    assert(database_directory != NULL);

    assert(
        g_mkdir(
            database_directory,
            0700
        ) == 0
    );

    database_path = g_build_filename(
        database_directory,
        "Enquete.sqlite",
        NULL
    );

    assert(database_path != NULL);

    assert(
        !g_file_test(
            database_path,
            G_FILE_TEST_EXISTS
        )
    );

    session = investigation_session_open(
        investigation_root,
        &error
    );

    assert(session == NULL);

    test_assert_session_error(
        error,
        INVESTIGATION_SESSION_ERROR_DATABASE_NOT_FOUND
    );

    g_clear_error(&error);

    assert(
        !g_file_test(
            database_path,
            G_FILE_TEST_EXISTS
        )
    );

    assert(
        test_remove_path_recursively(
            investigation_root
        )
    );

    g_free(database_path);
    g_free(database_directory);
    g_free(investigation_root);
}

static void test_open_invalid_database(void)
{
    char *investigation_root = NULL;
    char *database_directory = NULL;
    char *database_path = NULL;

    InvestigationSession *session = NULL;

    GError *error = NULL;

    investigation_root = g_dir_make_tmp(
        "labfy-investigation-session-invalid-database-XXXXXX",
        &error
    );

    assert(investigation_root != NULL);
    assert(error == NULL);

    database_directory = g_build_filename(
        investigation_root,
        "00_BaseDeDonnees",
        NULL
    );

    assert(database_directory != NULL);

    assert(
        g_mkdir(
            database_directory,
            0700
        ) == 0
    );

    database_path = g_build_filename(
        database_directory,
        "Enquete.sqlite",
        NULL
    );

    assert(database_path != NULL);

    assert(
        g_file_set_contents(
            database_path,
            "",
            0,
            &error
        )
    );

    assert(error == NULL);

    session = investigation_session_open(
        investigation_root,
        &error
    );

    assert(session == NULL);

    test_assert_session_error(
        error,
        INVESTIGATION_SESSION_ERROR_DATABASE
    );

    g_clear_error(&error);

    assert(
        test_remove_path_recursively(
            investigation_root
        )
    );

    g_free(database_path);
    g_free(database_directory);
    g_free(investigation_root);
}

static void test_open_root_mismatch(void)
{
    char *investigation_root = NULL;
    char *database_directory = NULL;
    char *database_path = NULL;
    char *different_root_path = NULL;

    InvestigationSession *session = NULL;

    GError *error = NULL;

    investigation_root = g_dir_make_tmp(
        "labfy-investigation-session-root-mismatch-XXXXXX",
        &error
    );

    assert(investigation_root != NULL);
    assert(error == NULL);

    database_directory = g_build_filename(
        investigation_root,
        "00_BaseDeDonnees",
        NULL
    );

    assert(database_directory != NULL);

    assert(
        g_mkdir(
            database_directory,
            0700
        ) == 0
    );

    database_path = g_build_filename(
        database_directory,
        "Enquete.sqlite",
        NULL
    );

    assert(database_path != NULL);

    different_root_path = g_build_filename(
        investigation_root,
        "autre-emplacement",
        NULL
    );

    assert(different_root_path != NULL);

    assert(
        database_initialize(
            database_path,
            "Enquete_Incoherente",
            different_root_path
        )
    );

    session = investigation_session_open(
        investigation_root,
        &error
    );

    assert(session == NULL);

    test_assert_session_error(
        error,
        INVESTIGATION_SESSION_ERROR_ROOT_MISMATCH
    );

    g_clear_error(&error);

    assert(
        test_remove_path_recursively(
            investigation_root
        )
    );

    g_free(different_root_path);
    g_free(database_path);
    g_free(database_directory);
    g_free(investigation_root);
}

static void test_null_session_accessors(void)
{
    assert(
        investigation_session_get_project(NULL) == NULL
    );

    assert(
        investigation_session_get_record(NULL) == NULL
    );

    assert(
        investigation_session_get_database(NULL) == NULL
    );

    investigation_session_close(NULL);
}

int main(void)
{
    test_open_valid_session();
    test_open_invalid_parameters();
    test_open_missing_root();
    test_open_root_is_file();
    test_open_missing_database();
    test_open_invalid_database();
    test_open_root_mismatch();
    test_null_session_accessors();

    printf(
        "InvestigationSession : tous les tests sont valides.\n"
    );

    return 0;
}
