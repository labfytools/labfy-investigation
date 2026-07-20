/******************************************************************************
 * @file test_entity_type_dao.c
 * @brief Tests du DAO des types d'entité.
 ******************************************************************************/

#include "dao/entity_type_dao.h"

#include "database/database.h"
#include "database/statement.h"
#include "models/entity_type.h"

#include <glib.h>
#include <glib/gstdio.h>

/**
 * @brief Ressources utilisées par un test EntityTypeDao.
 */
typedef struct
{
    char *temporary_directory;
    char *database_path;

    Database *database;
    EntityTypeDao *entity_type_dao;
} TestEntityTypeDaoFixture;

/**
 * @brief Exécute une requête SQL ne retournant aucune ligne.
 */
static void test_entity_type_dao_execute_sql(
    Database *database,
    const char *sql
)
{
    DatabaseStatement *statement =
        NULL;

    g_assert_nonnull(
        database
    );

    g_assert_nonnull(
        sql
    );

    statement =
        database_statement_prepare(
            database,
            sql
        );

    g_assert_nonnull(
        statement
    );

    g_assert_cmpint(
        database_statement_step(
            statement
        ),
        ==,
        DATABASE_STATEMENT_STEP_DONE
    );

    database_statement_finalize(
        statement
    );
}

/**
 * @brief Crée une base temporaire contenant le schéma de l'application.
 */
static TestEntityTypeDaoFixture
test_entity_type_dao_fixture_create(void)
{
    TestEntityTypeDaoFixture fixture =
        {0};

    GError *error =
        NULL;

    fixture.temporary_directory =
        g_dir_make_tmp(
            "labfy-entity-type-dao-test-XXXXXX",
            &error
        );

    g_assert_no_error(
        error
    );

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
            "Enquete_EntityTypeDao",
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

    fixture.entity_type_dao =
        entity_type_dao_new(
            fixture.database,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        fixture.entity_type_dao
    );

    return fixture;
}

/**
 * @brief Libère la fixture et supprime la base temporaire.
 */
static void test_entity_type_dao_fixture_clear(
    TestEntityTypeDaoFixture *fixture
)
{
    g_assert_nonnull(
        fixture
    );

    entity_type_dao_free(
        fixture->entity_type_dao
    );

    database_close(
        fixture->database
    );

    g_assert_cmpint(
        g_remove(
            fixture->database_path
        ),
        ==,
        0
    );

    g_assert_cmpint(
        g_rmdir(
            fixture->temporary_directory
        ),
        ==,
        0
    );

    g_free(
        fixture->database_path
    );

    g_free(
        fixture->temporary_directory
    );

    fixture->entity_type_dao =
        NULL;

    fixture->database =
        NULL;

    fixture->database_path =
        NULL;

    fixture->temporary_directory =
        NULL;
}

/**
 * @brief Vérifie le refus d'une connexion absente.
 */
static void test_entity_type_dao_new_invalid_database(void)
{
    EntityTypeDao *entity_type_dao =
        NULL;

    GError *error =
        NULL;

    entity_type_dao =
        entity_type_dao_new(
            NULL,
            &error
        );

    g_assert_null(
        entity_type_dao
    );

    g_assert_error(
        error,
        ENTITY_TYPE_DAO_ERROR,
        ENTITY_TYPE_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );
}

/**
 * @brief Vérifie le chargement du catalogue initial.
 */
static void test_entity_type_dao_list_all(void)
{
    static const char *const expected_codes[] =
    {
        "email_address",
        "bank_account",
        "facebook_account",
        "instagram_account",
        "identity_document",
        "iban",
        "person",
        "pseudonym",
        "phone_number",
        "website",
        "domain_name",
        "ip_address",
        "organization",
        "other"
    };

    static const char *const expected_labels[] =
    {
        "Adresse email",
        "Compte bancaire",
        "Compte Facebook",
        "Compte Instagram",
        "Document d'identité",
        "IBAN",
        "Personne",
        "Pseudonyme",
        "Numéro de téléphone",
        "Site web",
        "Nom de domaine",
        "Adresse IP",
        "Organisation",
        "Autre"
    };

    TestEntityTypeDaoFixture fixture =
        test_entity_type_dao_fixture_create();

    GPtrArray *entity_types =
        NULL;

    GError *error =
        NULL;

    guint index =
        0;

    entity_types =
        entity_type_dao_list_all(
            fixture.entity_type_dao,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        entity_types
    );

    g_assert_cmpuint(
        entity_types->len,
        ==,
        G_N_ELEMENTS(
            expected_codes
        )
    );

    for (index = 0;
         index < entity_types->len;
         index++)
    {
        const EntityType *entity_type =
            NULL;

        entity_type =
            g_ptr_array_index(
                entity_types,
                index
            );

        g_assert_nonnull(
            entity_type
        );

        /*
         * La requête utilise ORDER BY id ASC.
         */
        g_assert_cmpint(
            entity_type_get_identifier(
                entity_type
            ),
            ==,
            (gint64) index + 1
        );

        g_assert_cmpstr(
            entity_type_get_code(
                entity_type
            ),
            ==,
            expected_codes[index]
        );

        g_assert_cmpstr(
            entity_type_get_label(
                entity_type
            ),
            ==,
            expected_labels[index]
        );

        /*
         * Le catalogue V1 ne renseigne aucune description.
         */
        g_assert_null(
            entity_type_get_description(
                entity_type
            )
        );
    }

    g_ptr_array_unref(
        entity_types
    );

    test_entity_type_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie qu'une table vide retourne un tableau vide.
 */
static void test_entity_type_dao_list_empty(void)
{
    TestEntityTypeDaoFixture fixture =
        test_entity_type_dao_fixture_create();

    GPtrArray *entity_types =
        NULL;

    GError *error =
        NULL;

    test_entity_type_dao_execute_sql(
        fixture.database,
        "DELETE FROM types_entite;"
    );

    entity_types =
        entity_type_dao_list_all(
            fixture.entity_type_dao,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        entity_types
    );

    g_assert_cmpuint(
        entity_types->len,
        ==,
        0
    );

    g_ptr_array_unref(
        entity_types
    );

    test_entity_type_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le signalement d'une table absente.
 */
static void test_entity_type_dao_missing_table(void)
{
    Database *database =
        NULL;

    EntityTypeDao *entity_type_dao =
        NULL;

    GPtrArray *entity_types =
        NULL;

    GError *error =
        NULL;

    database =
        database_open(
            ":memory:"
        );

    g_assert_nonnull(
        database
    );

    entity_type_dao =
        entity_type_dao_new(
            database,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        entity_type_dao
    );

    g_test_expect_message(
        NULL,
        G_LOG_LEVEL_WARNING,
        "*Impossible de préparer la requête SQL*"
    );

    entity_types =
        entity_type_dao_list_all(
            entity_type_dao,
            &error
        );

    g_test_assert_expected_messages();

    g_assert_null(
        entity_types
    );

    g_assert_error(
        error,
        ENTITY_TYPE_DAO_ERROR,
        ENTITY_TYPE_DAO_ERROR_SCHEMA
    );

    g_clear_error(
        &error
    );

    entity_type_dao_free(
        entity_type_dao
    );

    database_close(
        database
    );
}

/**
 * @brief Vérifie le refus d'une ligne produisant un modèle invalide.
 */
static void test_entity_type_dao_invalid_model(void)
{
    TestEntityTypeDaoFixture fixture =
        test_entity_type_dao_fixture_create();

    GPtrArray *entity_types =
        NULL;

    GError *error =
        NULL;

    /*
     * Le schéma interdit NULL mais autorise actuellement
     * une chaîne uniquement composée d'espaces.
     */
    test_entity_type_dao_execute_sql(
        fixture.database,
        "UPDATE types_entite "
        "SET code = '   ' "
        "WHERE id = 1;"
    );

    entity_types =
        entity_type_dao_list_all(
            fixture.entity_type_dao,
            &error
        );

    g_assert_null(
        entity_types
    );

    g_assert_error(
        error,
        ENTITY_TYPE_DAO_ERROR,
        ENTITY_TYPE_DAO_ERROR_MODEL
    );

    g_clear_error(
        &error
    );

    test_entity_type_dao_fixture_clear(
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
        "/entity-type-dao/new-invalid-database",
        test_entity_type_dao_new_invalid_database
    );

    g_test_add_func(
        "/entity-type-dao/list-all",
        test_entity_type_dao_list_all
    );

    g_test_add_func(
        "/entity-type-dao/list-empty",
        test_entity_type_dao_list_empty
    );

    g_test_add_func(
        "/entity-type-dao/missing-table",
        test_entity_type_dao_missing_table
    );

    g_test_add_func(
        "/entity-type-dao/invalid-model",
        test_entity_type_dao_invalid_model
    );

    return g_test_run();
}
