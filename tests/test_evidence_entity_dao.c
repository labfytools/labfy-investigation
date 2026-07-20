/******************************************************************************
 * @file test_evidence_entity_dao.c
 * @brief Tests du DAO des associations entre preuves et entités.
 ******************************************************************************/

#include "dao/entity_dao.h"
#include "dao/evidence_dao.h"
#include "dao/evidence_entity_dao.h"

#include "database/database.h"
#include "database/statement.h"
#include "models/entity_record.h"
#include "models/evidence_record.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

/**
 * @brief Environnement temporaire d'un test EvidenceEntityDao.
 */
typedef struct
{
    char *temporary_directory;
    char *database_path;

    Database *database;

    EvidenceDao *evidence_dao;
    EntityDao *entity_dao;
    EvidenceEntityDao *evidence_entity_dao;
} TestEvidenceEntityDaoFixture;

/**
 * @brief Vérifie le domaine et le code d'une erreur.
 */
static void test_evidence_entity_dao_assert_error(
    const GError *error,
    EvidenceEntityDaoError expected_code
)
{
    assert(error != NULL);

    assert(
        error->domain ==
        EVIDENCE_ENTITY_DAO_ERROR
    );

    assert(
        error->code ==
        (gint) expected_code
    );

    assert(error->message != NULL);
    assert(error->message[0] != '\0');
}

/**
 * @brief Crée une base temporaire et les DAO nécessaires.
 */
static TestEvidenceEntityDaoFixture
test_evidence_entity_dao_fixture_create(void)
{
    TestEvidenceEntityDaoFixture fixture =
        {0};

    GError *error =
        NULL;

    fixture.temporary_directory =
        g_dir_make_tmp(
            "labfy-evidence-entity-dao-test-XXXXXX",
            &error
        );

    assert(fixture.temporary_directory != NULL);
    assert(error == NULL);

    fixture.database_path =
        g_build_filename(
            fixture.temporary_directory,
            "Enquete.sqlite",
            NULL
        );

    assert(fixture.database_path != NULL);

    assert(
        database_initialize(
            fixture.database_path,
            "Enquete_EvidenceEntityDao",
            fixture.temporary_directory
        )
    );

    fixture.database =
        database_open(
            fixture.database_path
        );

    assert(fixture.database != NULL);

    fixture.evidence_dao =
        evidence_dao_new(
            fixture.database,
            &error
        );

    assert(fixture.evidence_dao != NULL);
    assert(error == NULL);

    fixture.entity_dao =
        entity_dao_new(
            fixture.database,
            &error
        );

    assert(fixture.entity_dao != NULL);
    assert(error == NULL);

    fixture.evidence_entity_dao =
        evidence_entity_dao_new(
            fixture.database,
            &error
        );

    assert(fixture.evidence_entity_dao != NULL);
    assert(error == NULL);

    return fixture;
}

/**
 * @brief Libère la fixture et supprime sa base temporaire.
 */
static void test_evidence_entity_dao_fixture_clear(
    TestEvidenceEntityDaoFixture *fixture
)
{
    assert(fixture != NULL);

    evidence_entity_dao_free(
        fixture->evidence_entity_dao
    );

    entity_dao_free(
        fixture->entity_dao
    );

    evidence_dao_free(
        fixture->evidence_dao
    );

    database_close(
        fixture->database
    );

    assert(
        g_remove(
            fixture->database_path
        ) == 0
    );

    assert(
        g_rmdir(
            fixture->temporary_directory
        ) == 0
    );

    g_free(
        fixture->database_path
    );

    g_free(
        fixture->temporary_directory
    );

    fixture->evidence_entity_dao =
        NULL;

    fixture->entity_dao =
        NULL;

    fixture->evidence_dao =
        NULL;

    fixture->database =
        NULL;

    fixture->database_path =
        NULL;

    fixture->temporary_directory =
        NULL;
}

/**
 * @brief Crée une preuve valide destinée aux tests.
 */
static EvidenceRecord *test_evidence_entity_dao_create_evidence(
    const char *identifier
)
{
    EvidenceRecord *evidence_record =
        NULL;

    char *internal_name =
        NULL;

    char *relative_path =
        NULL;

    GError *error =
        NULL;

    assert(identifier != NULL);

    internal_name =
        g_strdup_printf(
            "%s.bin",
            identifier
        );

    relative_path =
        g_strdup_printf(
            "01_Preuves_Originales/Documents/%s.bin",
            identifier
        );

    assert(internal_name != NULL);
    assert(relative_path != NULL);

    evidence_record =
        evidence_record_new(
            identifier,
            "preuve.bin",
            internal_name,
            relative_path,
            "document",
            128,
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            "2026-07-20T20:00:00Z",
            NULL,
            NULL,
            NULL,
            EVIDENCE_INTEGRITY_STATUS_UNKNOWN,
            &error
        );

    assert(evidence_record != NULL);
    assert(error == NULL);

    g_free(
        relative_path
    );

    g_free(
        internal_name
    );

    return evidence_record;
}

/**
 * @brief Crée une entité valide destinée aux tests.
 */
static EntityRecord *test_evidence_entity_dao_create_entity(
    const char *identifier,
    const char *type_identifier,
    const char *value
)
{
    EntityRecord *entity_record =
        NULL;

    GError *error =
        NULL;

    assert(identifier != NULL);
    assert(type_identifier != NULL);
    assert(value != NULL);

    entity_record =
        entity_record_new(
            identifier,
            type_identifier,
            value,
            NULL,
            NULL,
            80,
            "2026-07-20T20:00:00Z",
            "2026-07-20T20:00:00Z",
            ENTITY_STATUS_ACTIVE,
            &error
        );

    assert(entity_record != NULL);
    assert(error == NULL);

    return entity_record;
}

/**
 * @brief Insère une preuve de test.
 */
static void test_evidence_entity_dao_insert_evidence(
    TestEvidenceEntityDaoFixture *fixture,
    const char *identifier
)
{
    EvidenceRecord *evidence_record =
        NULL;

    GError *error =
        NULL;

    assert(fixture != NULL);

    evidence_record =
        test_evidence_entity_dao_create_evidence(
            identifier
        );

    assert(
        evidence_dao_insert(
            fixture->evidence_dao,
            evidence_record,
            &error
        )
    );

    assert(error == NULL);

    evidence_record_free(
        evidence_record
    );
}

/**
 * @brief Insère une entité de test.
 */
static void test_evidence_entity_dao_insert_entity(
    TestEvidenceEntityDaoFixture *fixture,
    const char *identifier,
    const char *type_identifier,
    const char *value
)
{
    EntityRecord *entity_record =
        NULL;

    GError *error =
        NULL;

    assert(fixture != NULL);

    entity_record =
        test_evidence_entity_dao_create_entity(
            identifier,
            type_identifier,
            value
        );

    assert(
        entity_dao_insert(
            fixture->entity_dao,
            entity_record,
            &error
        )
    );

    assert(error == NULL);

    entity_record_free(
        entity_record
    );
}

/**
 * @brief Retourne le nombre d'associations persistées.
 */
static int64_t test_evidence_entity_dao_count_rows(
    Database *database
)
{
    DatabaseStatement *statement =
        NULL;

    int64_t association_count =
        -1;

    assert(database != NULL);

    statement =
        database_statement_prepare(
            database,
            "SELECT COUNT(*) "
            "FROM preuve_entites;"
        );

    assert(statement != NULL);

    assert(
        database_statement_step(
            statement
        ) == DATABASE_STATEMENT_STEP_ROW
    );

    assert(
        database_statement_column_int64(
            statement,
            0,
            &association_count
        )
    );

    assert(
        database_statement_step(
            statement
        ) == DATABASE_STATEMENT_STEP_DONE
    );

    database_statement_finalize(
        statement
    );

    return association_count;
}

/**
 * @brief Vérifie le refus d'une connexion absente.
 */
static void test_evidence_entity_dao_new_null_database(void)
{
    EvidenceEntityDao *evidence_entity_dao =
        NULL;

    GError *error =
        NULL;

    evidence_entity_dao =
        evidence_entity_dao_new(
            NULL,
            &error
        );

    assert(evidence_entity_dao == NULL);

    test_evidence_entity_dao_assert_error(
        error,
        EVIDENCE_ENTITY_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );
}

/**
 * @brief Vérifie la création et la destruction du DAO.
 */
static void test_evidence_entity_dao_lifecycle(void)
{
    Database *database =
        NULL;

    EvidenceEntityDao *evidence_entity_dao =
        NULL;

    GError *error =
        NULL;

    database =
        database_open(
            ":memory:"
        );

    assert(database != NULL);

    evidence_entity_dao =
        evidence_entity_dao_new(
            database,
            &error
        );

    assert(evidence_entity_dao != NULL);
    assert(error == NULL);

    evidence_entity_dao_free(
        evidence_entity_dao
    );

    evidence_entity_dao_free(
        NULL
    );

    database_close(
        database
    );
}

/**
 * @brief Vérifie la création d'une association valide.
 */
static void test_evidence_entity_dao_link_valid(void)
{
    static const char *const evidence_identifier =
        "10000000-0000-4000-8000-000000000001";

    static const char *const entity_identifier =
        "20000000-0000-4000-8000-000000000001";

    TestEvidenceEntityDaoFixture fixture =
        test_evidence_entity_dao_fixture_create();

    gboolean association_exists =
        FALSE;

    GError *error =
        NULL;

    test_evidence_entity_dao_insert_evidence(
        &fixture,
        evidence_identifier
    );

    test_evidence_entity_dao_insert_entity(
        &fixture,
        entity_identifier,
        "email_address",
        "contact@example.org"
    );

    assert(
        evidence_entity_dao_link(
            fixture.evidence_entity_dao,
            evidence_identifier,
            entity_identifier,
            &error
        )
    );

    assert(error == NULL);

    assert(
        test_evidence_entity_dao_count_rows(
            fixture.database
        ) == 1
    );

    assert(
        evidence_entity_dao_exists(
            fixture.evidence_entity_dao,
            evidence_identifier,
            entity_identifier,
            &association_exists,
            &error
        )
    );

    assert(error == NULL);
    assert(association_exists);

    test_evidence_entity_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d'une association dupliquée.
 */
static void test_evidence_entity_dao_link_duplicate(void)
{
    static const char *const evidence_identifier =
        "10000000-0000-4000-8000-000000000002";

    static const char *const entity_identifier =
        "20000000-0000-4000-8000-000000000002";

    TestEvidenceEntityDaoFixture fixture =
        test_evidence_entity_dao_fixture_create();

    GError *error =
        NULL;

    test_evidence_entity_dao_insert_evidence(
        &fixture,
        evidence_identifier
    );

    test_evidence_entity_dao_insert_entity(
        &fixture,
        entity_identifier,
        "iban",
        "FR7612345678901234567890123"
    );

    assert(
        evidence_entity_dao_link(
            fixture.evidence_entity_dao,
            evidence_identifier,
            entity_identifier,
            &error
        )
    );

    assert(error == NULL);

    assert(
        !evidence_entity_dao_link(
            fixture.evidence_entity_dao,
            evidence_identifier,
            entity_identifier,
            &error
        )
    );

    test_evidence_entity_dao_assert_error(
        error,
        EVIDENCE_ENTITY_DAO_ERROR_CONSTRAINT
    );

    assert(
        test_evidence_entity_dao_count_rows(
            fixture.database
        ) == 1
    );

    g_clear_error(
        &error
    );

    test_evidence_entity_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d'une preuve inexistante.
 */
static void test_evidence_entity_dao_link_missing_evidence(void)
{
    static const char *const evidence_identifier =
        "10000000-0000-4000-8000-000000000003";

    static const char *const entity_identifier =
        "20000000-0000-4000-8000-000000000003";

    TestEvidenceEntityDaoFixture fixture =
        test_evidence_entity_dao_fixture_create();

    GError *error =
        NULL;

    test_evidence_entity_dao_insert_entity(
        &fixture,
        entity_identifier,
        "person",
        "Personne inconnue"
    );

    assert(
        !evidence_entity_dao_link(
            fixture.evidence_entity_dao,
            evidence_identifier,
            entity_identifier,
            &error
        )
    );

    test_evidence_entity_dao_assert_error(
        error,
        EVIDENCE_ENTITY_DAO_ERROR_NOT_FOUND
    );

    assert(
        test_evidence_entity_dao_count_rows(
            fixture.database
        ) == 0
    );

    g_clear_error(
        &error
    );

    test_evidence_entity_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d'une entité inexistante.
 */
static void test_evidence_entity_dao_link_missing_entity(void)
{
    static const char *const evidence_identifier =
        "10000000-0000-4000-8000-000000000004";

    static const char *const entity_identifier =
        "20000000-0000-4000-8000-000000000004";

    TestEvidenceEntityDaoFixture fixture =
        test_evidence_entity_dao_fixture_create();

    GError *error =
        NULL;

    test_evidence_entity_dao_insert_evidence(
        &fixture,
        evidence_identifier
    );

    assert(
        !evidence_entity_dao_link(
            fixture.evidence_entity_dao,
            evidence_identifier,
            entity_identifier,
            &error
        )
    );

    test_evidence_entity_dao_assert_error(
        error,
        EVIDENCE_ENTITY_DAO_ERROR_NOT_FOUND
    );

    assert(
        test_evidence_entity_dao_count_rows(
            fixture.database
        ) == 0
    );

    g_clear_error(
        &error
    );

    test_evidence_entity_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie les résultats vrai et faux de exists().
 */
static void test_evidence_entity_dao_exists(void)
{
    static const char *const evidence_identifier =
        "10000000-0000-4000-8000-000000000005";

    static const char *const entity_identifier =
        "20000000-0000-4000-8000-000000000005";

    TestEvidenceEntityDaoFixture fixture =
        test_evidence_entity_dao_fixture_create();

    gboolean association_exists =
        TRUE;

    GError *error =
        NULL;

    test_evidence_entity_dao_insert_evidence(
        &fixture,
        evidence_identifier
    );

    test_evidence_entity_dao_insert_entity(
        &fixture,
        entity_identifier,
        "domain_name",
        "example.org"
    );

    assert(
        evidence_entity_dao_exists(
            fixture.evidence_entity_dao,
            evidence_identifier,
            entity_identifier,
            &association_exists,
            &error
        )
    );

    assert(error == NULL);
    assert(!association_exists);

    assert(
        evidence_entity_dao_link(
            fixture.evidence_entity_dao,
            evidence_identifier,
            entity_identifier,
            &error
        )
    );

    assert(error == NULL);

    assert(
        evidence_entity_dao_exists(
            fixture.evidence_entity_dao,
            evidence_identifier,
            entity_identifier,
            &association_exists,
            &error
        )
    );

    assert(error == NULL);
    assert(association_exists);

    test_evidence_entity_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie la suppression d'une association.
 */
static void test_evidence_entity_dao_unlink_valid(void)
{
    static const char *const evidence_identifier =
        "10000000-0000-4000-8000-000000000006";

    static const char *const entity_identifier =
        "20000000-0000-4000-8000-000000000006";

    TestEvidenceEntityDaoFixture fixture =
        test_evidence_entity_dao_fixture_create();

    gboolean association_exists =
        TRUE;

    GError *error =
        NULL;

    test_evidence_entity_dao_insert_evidence(
        &fixture,
        evidence_identifier
    );

    test_evidence_entity_dao_insert_entity(
        &fixture,
        entity_identifier,
        "phone_number",
        "+33601020304"
    );

    assert(
        evidence_entity_dao_link(
            fixture.evidence_entity_dao,
            evidence_identifier,
            entity_identifier,
            &error
        )
    );

    assert(error == NULL);

    assert(
        evidence_entity_dao_unlink(
            fixture.evidence_entity_dao,
            evidence_identifier,
            entity_identifier,
            &error
        )
    );

    assert(error == NULL);

    assert(
        evidence_entity_dao_exists(
            fixture.evidence_entity_dao,
            evidence_identifier,
            entity_identifier,
            &association_exists,
            &error
        )
    );

    assert(error == NULL);
    assert(!association_exists);

    assert(
        test_evidence_entity_dao_count_rows(
            fixture.database
        ) == 0
    );

    test_evidence_entity_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le signalement d'une association absente.
 */
static void test_evidence_entity_dao_unlink_missing(void)
{
    TestEvidenceEntityDaoFixture fixture =
        test_evidence_entity_dao_fixture_create();

    GError *error =
        NULL;

    assert(
        !evidence_entity_dao_unlink(
            fixture.evidence_entity_dao,
            "10000000-0000-4000-8000-000000000007",
            "20000000-0000-4000-8000-000000000007",
            &error
        )
    );

    test_evidence_entity_dao_assert_error(
        error,
        EVIDENCE_ENTITY_DAO_ERROR_NOT_FOUND
    );

    g_clear_error(
        &error
    );

    test_evidence_entity_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie la liste vide et l'ordre des entités d'une preuve.
 */
static void test_evidence_entity_dao_list_entities(void)
{
    static const char *const evidence_identifier =
        "10000000-0000-4000-8000-000000000008";

    static const char *const first_entity_identifier =
        "20000000-0000-4000-8000-000000000001";

    static const char *const second_entity_identifier =
        "20000000-0000-4000-8000-000000000002";

    static const char *const third_entity_identifier =
        "20000000-0000-4000-8000-000000000003";

    TestEvidenceEntityDaoFixture fixture =
        test_evidence_entity_dao_fixture_create();

    GPtrArray *entity_identifiers =
        NULL;

    GError *error =
        NULL;

    test_evidence_entity_dao_insert_evidence(
        &fixture,
        evidence_identifier
    );

    entity_identifiers =
        evidence_entity_dao_list_entity_identifiers(
            fixture.evidence_entity_dao,
            evidence_identifier,
            &error
        );

    assert(entity_identifiers != NULL);
    assert(error == NULL);
    assert(entity_identifiers->len == 0);

    g_ptr_array_unref(
        entity_identifiers
    );

    test_evidence_entity_dao_insert_entity(
        &fixture,
        third_entity_identifier,
        "pseudonym",
        "pseudo-trois"
    );

    test_evidence_entity_dao_insert_entity(
        &fixture,
        first_entity_identifier,
        "pseudonym",
        "pseudo-un"
    );

    test_evidence_entity_dao_insert_entity(
        &fixture,
        second_entity_identifier,
        "pseudonym",
        "pseudo-deux"
    );

    assert(
        evidence_entity_dao_link(
            fixture.evidence_entity_dao,
            evidence_identifier,
            third_entity_identifier,
            &error
        )
    );

    assert(error == NULL);

    assert(
        evidence_entity_dao_link(
            fixture.evidence_entity_dao,
            evidence_identifier,
            first_entity_identifier,
            &error
        )
    );

    assert(error == NULL);

    assert(
        evidence_entity_dao_link(
            fixture.evidence_entity_dao,
            evidence_identifier,
            second_entity_identifier,
            &error
        )
    );

    assert(error == NULL);

    entity_identifiers =
        evidence_entity_dao_list_entity_identifiers(
            fixture.evidence_entity_dao,
            evidence_identifier,
            &error
        );

    assert(entity_identifiers != NULL);
    assert(error == NULL);
    assert(entity_identifiers->len == 3);

    assert(
        strcmp(
            g_ptr_array_index(
                entity_identifiers,
                0
            ),
            first_entity_identifier
        ) == 0
    );

    assert(
        strcmp(
            g_ptr_array_index(
                entity_identifiers,
                1
            ),
            second_entity_identifier
        ) == 0
    );

    assert(
        strcmp(
            g_ptr_array_index(
                entity_identifiers,
                2
            ),
            third_entity_identifier
        ) == 0
    );

    g_ptr_array_unref(
        entity_identifiers
    );

    test_evidence_entity_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie l'ordre des preuves liées à une entité.
 */
static void test_evidence_entity_dao_list_evidence(void)
{
    static const char *const entity_identifier =
        "20000000-0000-4000-8000-000000000009";

    static const char *const first_evidence_identifier =
        "10000000-0000-4000-8000-000000000001";

    static const char *const second_evidence_identifier =
        "10000000-0000-4000-8000-000000000002";

    static const char *const third_evidence_identifier =
        "10000000-0000-4000-8000-000000000003";

    TestEvidenceEntityDaoFixture fixture =
        test_evidence_entity_dao_fixture_create();

    GPtrArray *evidence_identifiers =
        NULL;

    GError *error =
        NULL;

    test_evidence_entity_dao_insert_entity(
        &fixture,
        entity_identifier,
        "organization",
        "Organisation de test"
    );

    test_evidence_entity_dao_insert_evidence(
        &fixture,
        third_evidence_identifier
    );

    test_evidence_entity_dao_insert_evidence(
        &fixture,
        first_evidence_identifier
    );

    test_evidence_entity_dao_insert_evidence(
        &fixture,
        second_evidence_identifier
    );

    assert(
        evidence_entity_dao_link(
            fixture.evidence_entity_dao,
            third_evidence_identifier,
            entity_identifier,
            &error
        )
    );

    assert(error == NULL);

    assert(
        evidence_entity_dao_link(
            fixture.evidence_entity_dao,
            first_evidence_identifier,
            entity_identifier,
            &error
        )
    );

    assert(error == NULL);

    assert(
        evidence_entity_dao_link(
            fixture.evidence_entity_dao,
            second_evidence_identifier,
            entity_identifier,
            &error
        )
    );

    assert(error == NULL);

    evidence_identifiers =
        evidence_entity_dao_list_evidence_identifiers(
            fixture.evidence_entity_dao,
            entity_identifier,
            &error
        );

    assert(evidence_identifiers != NULL);
    assert(error == NULL);
    assert(evidence_identifiers->len == 3);

    assert(
        strcmp(
            g_ptr_array_index(
                evidence_identifiers,
                0
            ),
            first_evidence_identifier
        ) == 0
    );

    assert(
        strcmp(
            g_ptr_array_index(
                evidence_identifiers,
                1
            ),
            second_evidence_identifier
        ) == 0
    );

    assert(
        strcmp(
            g_ptr_array_index(
                evidence_identifiers,
                2
            ),
            third_evidence_identifier
        ) == 0
    );

    g_ptr_array_unref(
        evidence_identifiers
    );

    test_evidence_entity_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie les arguments invalides des opérations publiques.
 */
static void test_evidence_entity_dao_invalid_arguments(void)
{
    TestEvidenceEntityDaoFixture fixture =
        test_evidence_entity_dao_fixture_create();

    GPtrArray *identifiers =
        NULL;

    gboolean association_exists =
        FALSE;

    GError *error =
        NULL;

    assert(
        !evidence_entity_dao_link(
            NULL,
            "10000000-0000-4000-8000-000000000010",
            "20000000-0000-4000-8000-000000000010",
            &error
        )
    );

    test_evidence_entity_dao_assert_error(
        error,
        EVIDENCE_ENTITY_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    assert(
        !evidence_entity_dao_link(
            fixture.evidence_entity_dao,
            "preuve-invalide",
            "20000000-0000-4000-8000-000000000010",
            &error
        )
    );

    test_evidence_entity_dao_assert_error(
        error,
        EVIDENCE_ENTITY_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    assert(
        !evidence_entity_dao_unlink(
            fixture.evidence_entity_dao,
            "10000000-0000-4000-8000-000000000010",
            "entite-invalide",
            &error
        )
    );

    test_evidence_entity_dao_assert_error(
        error,
        EVIDENCE_ENTITY_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    assert(
        !evidence_entity_dao_exists(
            fixture.evidence_entity_dao,
            "10000000-0000-4000-8000-000000000010",
            "20000000-0000-4000-8000-000000000010",
            NULL,
            &error
        )
    );

    test_evidence_entity_dao_assert_error(
        error,
        EVIDENCE_ENTITY_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    identifiers =
        evidence_entity_dao_list_entity_identifiers(
            fixture.evidence_entity_dao,
            "preuve-invalide",
            &error
        );

    assert(identifiers == NULL);

    test_evidence_entity_dao_assert_error(
        error,
        EVIDENCE_ENTITY_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    identifiers =
        evidence_entity_dao_list_evidence_identifiers(
            NULL,
            "20000000-0000-4000-8000-000000000010",
            &error
        );

    assert(identifiers == NULL);

    test_evidence_entity_dao_assert_error(
        error,
        EVIDENCE_ENTITY_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    /*
     * Un résultat faux est parfaitement valide lorsque l'association
     * n'existe pas.
     */
    assert(
        evidence_entity_dao_exists(
            fixture.evidence_entity_dao,
            "10000000-0000-4000-8000-000000000010",
            "20000000-0000-4000-8000-000000000010",
            &association_exists,
            &error
        )
    );

    assert(error == NULL);
    assert(!association_exists);

    test_evidence_entity_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie les contraintes ON DELETE RESTRICT.
 */
static void test_evidence_entity_dao_foreign_key_restrict(void)
{
    static const char *const evidence_identifier =
        "10000000-0000-4000-8000-000000000011";

    static const char *const entity_identifier =
        "20000000-0000-4000-8000-000000000011";

    TestEvidenceEntityDaoFixture fixture =
        test_evidence_entity_dao_fixture_create();

    DatabaseStatement *statement =
        NULL;

    GError *error =
        NULL;

    test_evidence_entity_dao_insert_evidence(
        &fixture,
        evidence_identifier
    );

    test_evidence_entity_dao_insert_entity(
        &fixture,
        entity_identifier,
        "website",
        "https://example.org/"
    );

    assert(
        evidence_entity_dao_link(
            fixture.evidence_entity_dao,
            evidence_identifier,
            entity_identifier,
            &error
        )
    );

    assert(error == NULL);

    statement =
        database_statement_prepare(
            fixture.database,
            "DELETE FROM preuves "
            "WHERE id = ?;"
        );

    assert(statement != NULL);

    assert(
        database_statement_bind_text(
            statement,
            1,
            evidence_identifier
        )
    );

    g_test_expect_message(
        NULL,
        G_LOG_LEVEL_WARNING,
        "*FOREIGN KEY constraint failed*"   
    );

    assert(
        database_statement_step(
            statement
        ) == DATABASE_STATEMENT_STEP_ERROR
    );

    database_statement_finalize(
        statement
    );

    statement =
        database_statement_prepare(
            fixture.database,
            "DELETE FROM entites "
            "WHERE id = ?;"
        );

    assert(statement != NULL);

    assert(
        database_statement_bind_text(
            statement,
            1,
            entity_identifier
        )
    );

    g_test_expect_message(
        NULL,
        G_LOG_LEVEL_WARNING,
        "*FOREIGN KEY constraint failed*"
    );
    
    assert(
        database_statement_step(
            statement
        ) == DATABASE_STATEMENT_STEP_ERROR
    );

    database_statement_finalize(
        statement
    );

    assert(
        test_evidence_entity_dao_count_rows(
            fixture.database
        ) == 1
    );

    test_evidence_entity_dao_fixture_clear(
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
    test_evidence_entity_dao_new_null_database();
    test_evidence_entity_dao_lifecycle();
    test_evidence_entity_dao_link_valid();
    test_evidence_entity_dao_link_duplicate();
    test_evidence_entity_dao_link_missing_evidence();
    test_evidence_entity_dao_link_missing_entity();
    test_evidence_entity_dao_exists();
    test_evidence_entity_dao_unlink_valid();
    test_evidence_entity_dao_unlink_missing();
    test_evidence_entity_dao_list_entities();
    test_evidence_entity_dao_list_evidence();
    test_evidence_entity_dao_invalid_arguments();
    test_evidence_entity_dao_foreign_key_restrict();

    printf(
        "EvidenceEntityDao : tous les tests sont valides.\n"
    );

    return 0;
}
