/******************************************************************************
 * @file test_evidence_type_dao.c
 * @brief Tests du DAO des types de preuve.
 ******************************************************************************/

#include "dao/evidence_type_dao.h"

#include "database/database.h"
#include "database/statement.h"
#include "models/evidence_type.h"

#include <glib.h>
#include <glib/gstdio.h>

/**
 * @brief Ressources utilisées par un test EvidenceTypeDao.
 */
typedef struct
{
    char *temporary_directory;
    char *database_path;

    Database *database;
    EvidenceTypeDao *evidence_type_dao;
} TestEvidenceTypeDaoFixture;

/**
 * @brief Exécute une requête SQL ne retournant aucune ligne.
 */
static void test_evidence_type_dao_execute_sql(
    Database *database,
    const char *sql
)
{
    DatabaseStatement *statement = NULL;

    g_assert_nonnull(database);
    g_assert_nonnull(sql);

    statement =
        database_statement_prepare(
            database,
            sql
        );

    g_assert_nonnull(statement);

    g_assert_cmpint(
        database_statement_step(statement),
        ==,
        DATABASE_STATEMENT_STEP_DONE
    );

    database_statement_finalize(
        statement
    );
}

/**
 * @brief Crée une base temporaire contenant le schéma de l’application.
 */
static TestEvidenceTypeDaoFixture
test_evidence_type_dao_fixture_create(void)
{
    TestEvidenceTypeDaoFixture fixture = {0};

    GError *error = NULL;

    fixture.temporary_directory =
        g_dir_make_tmp(
            "labfy-evidence-type-dao-test-XXXXXX",
            &error
        );

    g_assert_no_error(error);
    g_assert_nonnull(
        fixture.temporary_directory
    );

    fixture.database_path =
        g_build_filename(
            fixture.temporary_directory,
            "Enquete.sqlite",
            NULL
        );

    g_assert_nonnull(
        fixture.database_path
    );

    g_assert_true(
        database_initialize(
            fixture.database_path,
            "Enquete_EvidenceTypeDao",
            fixture.temporary_directory
        )
    );

    fixture.database =
        database_open(
            fixture.database_path
        );

    g_assert_nonnull(
        fixture.database
    );

    fixture.evidence_type_dao =
        evidence_type_dao_new(
            fixture.database,
            &error
        );

    g_assert_no_error(error);

    g_assert_nonnull(
        fixture.evidence_type_dao
    );

    return fixture;
}

/**
 * @brief Libère la fixture et supprime la base temporaire.
 */
static void test_evidence_type_dao_fixture_clear(
    TestEvidenceTypeDaoFixture *fixture
)
{
    g_assert_nonnull(fixture);

    evidence_type_dao_free(
        fixture->evidence_type_dao
    );

    database_close(
        fixture->database
    );

    g_assert_cmpint(
        g_remove(fixture->database_path),
        ==,
        0
    );

    g_assert_cmpint(
        g_rmdir(fixture->temporary_directory),
        ==,
        0
    );

    g_free(
        fixture->database_path
    );

    g_free(
        fixture->temporary_directory
    );

    fixture->evidence_type_dao = NULL;
    fixture->database = NULL;
    fixture->database_path = NULL;
    fixture->temporary_directory = NULL;
}

/**
 * @brief Vérifie le refus d’une connexion absente.
 */
static void test_evidence_type_dao_new_invalid_database(void)
{
    EvidenceTypeDao *evidence_type_dao = NULL;

    GError *error = NULL;

    evidence_type_dao =
        evidence_type_dao_new(
            NULL,
            &error
        );

    g_assert_null(
        evidence_type_dao
    );

    g_assert_error(
        error,
        EVIDENCE_TYPE_DAO_ERROR,
        EVIDENCE_TYPE_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );
}

/**
 * @brief Vérifie le chargement du catalogue initial.
 */
static void test_evidence_type_dao_list_all(void)
{
    static const char *const expected_codes[] =
    {
        "screenshot",
        "photo",
        "video",
        "document",
        "email",
        "archive",
        "audio",
        "text",
        "other"
    };

    static const char *const expected_labels[] =
    {
        "Capture d'écran",
        "Photographie",
        "Vidéo",
        "Document",
        "Courrier électronique",
        "Archive",
        "Audio",
        "Texte",
        "Autre"
    };

    TestEvidenceTypeDaoFixture fixture =
        test_evidence_type_dao_fixture_create();

    GPtrArray *evidence_types = NULL;

    GError *error = NULL;

    guint index = 0;

    evidence_types =
        evidence_type_dao_list_all(
            fixture.evidence_type_dao,
            &error
        );

    g_assert_no_error(error);
    g_assert_nonnull(evidence_types);

    g_assert_cmpuint(
        evidence_types->len,
        ==,
        G_N_ELEMENTS(expected_codes)
    );

    for (index = 0;
         index < evidence_types->len;
         index++)
    {
        const EvidenceType *evidence_type =
            g_ptr_array_index(
                evidence_types,
                index
            );

        g_assert_nonnull(
            evidence_type
        );

        /*
         * La requête utilise ORDER BY id ASC.
         */
        g_assert_cmpint(
            evidence_type_get_identifier(
                evidence_type
            ),
            ==,
            (gint64) index + 1
        );

        g_assert_cmpstr(
            evidence_type_get_code(
                evidence_type
            ),
            ==,
            expected_codes[index]
        );

        g_assert_cmpstr(
            evidence_type_get_label(
                evidence_type
            ),
            ==,
            expected_labels[index]
        );
    }

    g_ptr_array_unref(
        evidence_types
    );

    test_evidence_type_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie qu’une table vide retourne un tableau vide.
 */
static void test_evidence_type_dao_list_empty(void)
{
    TestEvidenceTypeDaoFixture fixture =
        test_evidence_type_dao_fixture_create();

    GPtrArray *evidence_types = NULL;

    GError *error = NULL;

    test_evidence_type_dao_execute_sql(
        fixture.database,
        "DELETE FROM types_preuve;"
    );

    evidence_types =
        evidence_type_dao_list_all(
            fixture.evidence_type_dao,
            &error
        );

    g_assert_no_error(error);
    g_assert_nonnull(evidence_types);

    g_assert_cmpuint(
        evidence_types->len,
        ==,
        0
    );

    g_ptr_array_unref(
        evidence_types
    );

    test_evidence_type_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le signalement d’une table absente.
 */
static void test_evidence_type_dao_missing_table(void)
{
    Database *database = NULL;

    EvidenceTypeDao *evidence_type_dao = NULL;

    GPtrArray *evidence_types = NULL;

    GError *error = NULL;

    database =
        database_open(
            ":memory:"
        );

    g_assert_nonnull(database);

    evidence_type_dao =
        evidence_type_dao_new(
            database,
            &error
        );

    g_assert_no_error(error);
    g_assert_nonnull(evidence_type_dao);

    /*
     * DatabaseStatement journalise l’échec de préparation SQLite.
     */
    g_test_expect_message(
        NULL,
        G_LOG_LEVEL_WARNING,
        "*Impossible de préparer la requête SQL*"
    );

    evidence_types =
        evidence_type_dao_list_all(
            evidence_type_dao,
            &error
        );

    g_test_assert_expected_messages();

    g_assert_null(evidence_types);

    g_assert_error(
        error,
        EVIDENCE_TYPE_DAO_ERROR,
        EVIDENCE_TYPE_DAO_ERROR_SCHEMA
    );

    g_clear_error(
        &error
    );

    evidence_type_dao_free(
        evidence_type_dao
    );

    database_close(
        database
    );
}

/**
 * @brief Vérifie le refus d’une ligne produisant un modèle invalide.
 */
static void test_evidence_type_dao_invalid_model(void)
{
    TestEvidenceTypeDaoFixture fixture =
        test_evidence_type_dao_fixture_create();

    GPtrArray *evidence_types = NULL;

    GError *error = NULL;

    /*
     * La contrainte SQLite interdit NULL mais pas une chaîne
     * uniquement composée d’espaces.
     *
     * EvidenceType doit normaliser puis refuser cette valeur.
     */
    test_evidence_type_dao_execute_sql(
        fixture.database,
        "UPDATE types_preuve "
        "SET code = '   ' "
        "WHERE id = 1;"
    );

    evidence_types =
        evidence_type_dao_list_all(
            fixture.evidence_type_dao,
            &error
        );

    g_assert_null(evidence_types);

    g_assert_error(
        error,
        EVIDENCE_TYPE_DAO_ERROR,
        EVIDENCE_TYPE_DAO_ERROR_MODEL
    );

    g_clear_error(
        &error
    );

    test_evidence_type_dao_fixture_clear(
        &fixture
    );
}

int main(
    int argc,
    char **argv
)
{
    g_test_init(
        &argc,
        &argv,
        NULL
    );

    g_test_add_func(
        "/evidence-type-dao/new-invalid-database",
        test_evidence_type_dao_new_invalid_database
    );

    g_test_add_func(
        "/evidence-type-dao/list-all",
        test_evidence_type_dao_list_all
    );

    g_test_add_func(
        "/evidence-type-dao/list-empty",
        test_evidence_type_dao_list_empty
    );

    g_test_add_func(
        "/evidence-type-dao/missing-table",
        test_evidence_type_dao_missing_table
    );

    g_test_add_func(
        "/evidence-type-dao/invalid-model",
        test_evidence_type_dao_invalid_model
    );

    return g_test_run();
}
