/******************************************************************************
 * @file test_relation_evidence_dao.c
 * @brief Tests du DAO des associations entre relations et preuves.
 ******************************************************************************/

#include "dao/entity_dao.h"
#include "dao/evidence_dao.h"
#include "dao/relation_dao.h"
#include "dao/relation_evidence_dao.h"

#include "database/database.h"
#include "database/statement.h"
#include "models/entity_record.h"
#include "models/evidence_record.h"
#include "models/relation_record.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

#define TEST_ENTITY_A_IDENTIFIER \
    "10000000-0000-4000-8000-000000000001"

#define TEST_ENTITY_B_IDENTIFIER \
    "20000000-0000-4000-8000-000000000001"

#define TEST_ENTITY_C_IDENTIFIER \
    "30000000-0000-4000-8000-000000000001"

/**
 * @brief Environnement temporaire d'un test RelationEvidenceDao.
 */
typedef struct
{
    char *temporary_directory;
    char *database_path;

    Database *database;

    EvidenceDao *evidence_dao;
    EntityDao *entity_dao;
    RelationDao *relation_dao;
    RelationEvidenceDao *relation_evidence_dao;
} TestRelationEvidenceDaoFixture;

/**
 * @brief Vérifie le domaine et le code d'une erreur.
 */
static void test_relation_evidence_dao_assert_error(
    const GError *error,
    RelationEvidenceDaoError expected_code
)
{
    assert(error != NULL);

    assert(
        error->domain ==
        RELATION_EVIDENCE_DAO_ERROR
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
static TestRelationEvidenceDaoFixture
test_relation_evidence_dao_fixture_create(void)
{
    TestRelationEvidenceDaoFixture fixture =
        {0};

    GError *error =
        NULL;

    fixture.temporary_directory =
        g_dir_make_tmp(
            "labfy-relation-evidence-dao-test-XXXXXX",
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
            "Enquete_RelationEvidenceDao",
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

    fixture.relation_dao =
        relation_dao_new(
            fixture.database,
            &error
        );

    assert(fixture.relation_dao != NULL);
    assert(error == NULL);

    fixture.relation_evidence_dao =
        relation_evidence_dao_new(
            fixture.database,
            &error
        );

    assert(fixture.relation_evidence_dao != NULL);
    assert(error == NULL);

    return fixture;
}

/**
 * @brief Libère une fixture et supprime sa base temporaire.
 */
static void test_relation_evidence_dao_fixture_clear(
    TestRelationEvidenceDaoFixture *fixture
)
{
    assert(fixture != NULL);

    relation_evidence_dao_free(
        fixture->relation_evidence_dao
    );

    relation_dao_free(
        fixture->relation_dao
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

    fixture->relation_evidence_dao =
        NULL;

    fixture->relation_dao =
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
 * @brief Crée une entité de test.
 */
static EntityRecord *test_relation_evidence_dao_create_entity(
    const char *identifier,
    const char *value
)
{
    EntityRecord *entity_record =
        NULL;

    GError *error =
        NULL;

    entity_record =
        entity_record_new(
            identifier,
            "person",
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
 * @brief Insère une entité de test.
 */
static void test_relation_evidence_dao_insert_entity(
    TestRelationEvidenceDaoFixture *fixture,
    const char *identifier,
    const char *value
)
{
    EntityRecord *entity_record =
        NULL;

    GError *error =
        NULL;

    entity_record =
        test_relation_evidence_dao_create_entity(
            identifier,
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
 * @brief Insère les entités standard des tests.
 */
static void test_relation_evidence_dao_insert_standard_entities(
    TestRelationEvidenceDaoFixture *fixture
)
{
    test_relation_evidence_dao_insert_entity(
        fixture,
        TEST_ENTITY_A_IDENTIFIER,
        "Personne A"
    );

    test_relation_evidence_dao_insert_entity(
        fixture,
        TEST_ENTITY_B_IDENTIFIER,
        "Personne B"
    );

    test_relation_evidence_dao_insert_entity(
        fixture,
        TEST_ENTITY_C_IDENTIFIER,
        "Personne C"
    );
}

/**
 * @brief Crée une relation de test.
 */
static RelationRecord *test_relation_evidence_dao_create_relation(
    const char *identifier,
    const char *source_identifier,
    const char *target_identifier,
    const char *relation_type
)
{
    RelationRecord *relation_record =
        NULL;

    GError *error =
        NULL;

    relation_record =
        relation_record_new(
            identifier,
            source_identifier,
            target_identifier,
            relation_type,
            NULL,
            NULL,
            75,
            "2026-07-20T20:10:00Z",
            "2026-07-20T20:10:00Z",
            RELATION_STATUS_ACTIVE,
            &error
        );

    assert(relation_record != NULL);
    assert(error == NULL);

    return relation_record;
}

/**
 * @brief Insère une relation de test.
 */
static void test_relation_evidence_dao_insert_relation(
    TestRelationEvidenceDaoFixture *fixture,
    const char *identifier,
    const char *source_identifier,
    const char *target_identifier,
    const char *relation_type
)
{
    RelationRecord *relation_record =
        NULL;

    GError *error =
        NULL;

    relation_record =
        test_relation_evidence_dao_create_relation(
            identifier,
            source_identifier,
            target_identifier,
            relation_type
        );

    assert(
        relation_dao_insert(
            fixture->relation_dao,
            relation_record,
            &error
        )
    );

    assert(error == NULL);

    relation_record_free(
        relation_record
    );
}

/**
 * @brief Crée une preuve de test.
 */
static EvidenceRecord *test_relation_evidence_dao_create_evidence(
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
            "2026-07-20T20:20:00Z",
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
 * @brief Insère une preuve de test.
 */
static void test_relation_evidence_dao_insert_evidence(
    TestRelationEvidenceDaoFixture *fixture,
    const char *identifier
)
{
    EvidenceRecord *evidence_record =
        NULL;

    GError *error =
        NULL;

    evidence_record =
        test_relation_evidence_dao_create_evidence(
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
 * @brief Retourne le nombre d'associations persistées.
 */
static int64_t test_relation_evidence_dao_count_rows(
    Database *database
)
{
    DatabaseStatement *statement =
        NULL;

    int64_t association_count =
        -1;

    statement =
        database_statement_prepare(
            database,
            "SELECT COUNT(*) "
            "FROM relation_preuves;"
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
 * @brief Vérifie qu'une relation existe encore.
 */
static void test_relation_evidence_dao_assert_relation_exists(
    TestRelationEvidenceDaoFixture *fixture,
    const char *identifier
)
{
    RelationRecord *relation_record =
        NULL;

    GError *error =
        NULL;

    relation_record =
        relation_dao_find_by_identifier(
            fixture->relation_dao,
            identifier,
            &error
        );

    assert(relation_record != NULL);
    assert(error == NULL);

    relation_record_free(
        relation_record
    );
}

/**
 * @brief Vérifie qu'une preuve existe encore.
 */
static void test_relation_evidence_dao_assert_evidence_exists(
    TestRelationEvidenceDaoFixture *fixture,
    const char *identifier
)
{
    EvidenceRecord *evidence_record =
        NULL;

    GError *error =
        NULL;

    evidence_record =
        evidence_dao_find_by_identifier(
            fixture->evidence_dao,
            identifier,
            &error
        );

    assert(evidence_record != NULL);
    assert(error == NULL);

    evidence_record_free(
        evidence_record
    );
}

/**
 * @brief Vérifie le refus d'une connexion absente.
 */
static void test_relation_evidence_dao_new_null_database(void)
{
    RelationEvidenceDao *relation_evidence_dao =
        NULL;

    GError *error =
        NULL;

    relation_evidence_dao =
        relation_evidence_dao_new(
            NULL,
            &error
        );

    assert(relation_evidence_dao == NULL);

    test_relation_evidence_dao_assert_error(
        error,
        RELATION_EVIDENCE_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );
}

/**
 * @brief Vérifie la création et la destruction du DAO.
 */
static void test_relation_evidence_dao_lifecycle(void)
{
    Database *database =
        NULL;

    RelationEvidenceDao *relation_evidence_dao =
        NULL;

    GError *error =
        NULL;

    database =
        database_open(
            ":memory:"
        );

    assert(database != NULL);

    relation_evidence_dao =
        relation_evidence_dao_new(
            database,
            &error
        );

    assert(relation_evidence_dao != NULL);
    assert(error == NULL);

    relation_evidence_dao_free(
        relation_evidence_dao
    );

    relation_evidence_dao_free(
        NULL
    );

    database_close(
        database
    );
}

/**
 * @brief Vérifie la création et la détection d'une association.
 */
static void test_relation_evidence_dao_link_and_exists(void)
{
    static const char *const relation_identifier =
        "40000000-0000-4000-8000-000000000001";

    static const char *const evidence_identifier =
        "50000000-0000-4000-8000-000000000001";

    TestRelationEvidenceDaoFixture fixture =
        test_relation_evidence_dao_fixture_create();

    gboolean association_exists =
        TRUE;

    GError *error =
        NULL;

    test_relation_evidence_dao_insert_standard_entities(
        &fixture
    );

    test_relation_evidence_dao_insert_relation(
        &fixture,
        relation_identifier,
        TEST_ENTITY_A_IDENTIFIER,
        TEST_ENTITY_B_IDENTIFIER,
        "uses"
    );

    test_relation_evidence_dao_insert_evidence(
        &fixture,
        evidence_identifier
    );

    assert(
        relation_evidence_dao_exists(
            fixture.relation_evidence_dao,
            relation_identifier,
            evidence_identifier,
            &association_exists,
            &error
        )
    );

    assert(error == NULL);
    assert(!association_exists);

    assert(
        relation_evidence_dao_link(
            fixture.relation_evidence_dao,
            relation_identifier,
            evidence_identifier,
            &error
        )
    );

    assert(error == NULL);

    assert(
        relation_evidence_dao_exists(
            fixture.relation_evidence_dao,
            relation_identifier,
            evidence_identifier,
            &association_exists,
            &error
        )
    );

    assert(error == NULL);
    assert(association_exists);

    assert(
        test_relation_evidence_dao_count_rows(
            fixture.database
        ) == 1
    );

    test_relation_evidence_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d'une association dupliquée.
 */
static void test_relation_evidence_dao_link_duplicate(void)
{
    static const char *const relation_identifier =
        "40000000-0000-4000-8000-000000000002";

    static const char *const evidence_identifier =
        "50000000-0000-4000-8000-000000000002";

    TestRelationEvidenceDaoFixture fixture =
        test_relation_evidence_dao_fixture_create();

    GError *error =
        NULL;

    test_relation_evidence_dao_insert_standard_entities(
        &fixture
    );

    test_relation_evidence_dao_insert_relation(
        &fixture,
        relation_identifier,
        TEST_ENTITY_A_IDENTIFIER,
        TEST_ENTITY_B_IDENTIFIER,
        "controls"
    );

    test_relation_evidence_dao_insert_evidence(
        &fixture,
        evidence_identifier
    );

    assert(
        relation_evidence_dao_link(
            fixture.relation_evidence_dao,
            relation_identifier,
            evidence_identifier,
            &error
        )
    );

    assert(error == NULL);

    assert(
        !relation_evidence_dao_link(
            fixture.relation_evidence_dao,
            relation_identifier,
            evidence_identifier,
            &error
        )
    );

    test_relation_evidence_dao_assert_error(
        error,
        RELATION_EVIDENCE_DAO_ERROR_CONSTRAINT
    );

    assert(
        test_relation_evidence_dao_count_rows(
            fixture.database
        ) == 1
    );

    g_clear_error(
        &error
    );

    test_relation_evidence_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie la clé primaire composée directement dans SQLite.
 */
static void test_relation_evidence_dao_composite_primary_key(void)
{
    static const char *const relation_identifier =
        "40000000-0000-4000-8000-000000000003";

    static const char *const evidence_identifier =
        "50000000-0000-4000-8000-000000000003";

    TestRelationEvidenceDaoFixture fixture =
        test_relation_evidence_dao_fixture_create();

    DatabaseStatement *statement =
        NULL;

    GError *error =
        NULL;

    test_relation_evidence_dao_insert_standard_entities(
        &fixture
    );

    test_relation_evidence_dao_insert_relation(
        &fixture,
        relation_identifier,
        TEST_ENTITY_A_IDENTIFIER,
        TEST_ENTITY_B_IDENTIFIER,
        "knows"
    );

    test_relation_evidence_dao_insert_evidence(
        &fixture,
        evidence_identifier
    );

    assert(
        relation_evidence_dao_link(
            fixture.relation_evidence_dao,
            relation_identifier,
            evidence_identifier,
            &error
        )
    );

    assert(error == NULL);

    statement =
        database_statement_prepare(
            fixture.database,
            "INSERT INTO relation_preuves"
            "("
            "    relation_id,"
            "    preuve_id"
            ")"
            "VALUES"
            "("
            "    ?,"
            "    ?"
            ");"
        );

    assert(statement != NULL);

    assert(
        database_statement_bind_text(
            statement,
            1,
            relation_identifier
        )
    );

    assert(
        database_statement_bind_text(
            statement,
            2,
            evidence_identifier
        )
    );

    g_test_expect_message(
        NULL,
        G_LOG_LEVEL_WARNING,
        "*UNIQUE constraint failed*"
    );

    assert(
        database_statement_step(
            statement
        ) == DATABASE_STATEMENT_STEP_ERROR
    );

    g_test_assert_expected_messages();

    database_statement_finalize(
        statement
    );

    test_relation_evidence_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d'une relation inexistante.
 */
static void test_relation_evidence_dao_link_missing_relation(void)
{
    TestRelationEvidenceDaoFixture fixture =
        test_relation_evidence_dao_fixture_create();

    GError *error =
        NULL;

    test_relation_evidence_dao_insert_evidence(
        &fixture,
        "50000000-0000-4000-8000-000000000004"
    );

    assert(
        !relation_evidence_dao_link(
            fixture.relation_evidence_dao,
            "40000000-0000-4000-8000-000000000004",
            "50000000-0000-4000-8000-000000000004",
            &error
        )
    );

    test_relation_evidence_dao_assert_error(
        error,
        RELATION_EVIDENCE_DAO_ERROR_NOT_FOUND
    );

    g_clear_error(
        &error
    );

    test_relation_evidence_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d'une preuve inexistante.
 */
static void test_relation_evidence_dao_link_missing_evidence(void)
{
    TestRelationEvidenceDaoFixture fixture =
        test_relation_evidence_dao_fixture_create();

    GError *error =
        NULL;

    test_relation_evidence_dao_insert_standard_entities(
        &fixture
    );

    test_relation_evidence_dao_insert_relation(
        &fixture,
        "40000000-0000-4000-8000-000000000005",
        TEST_ENTITY_A_IDENTIFIER,
        TEST_ENTITY_B_IDENTIFIER,
        "uses"
    );

    assert(
        !relation_evidence_dao_link(
            fixture.relation_evidence_dao,
            "40000000-0000-4000-8000-000000000005",
            "50000000-0000-4000-8000-000000000005",
            &error
        )
    );

    test_relation_evidence_dao_assert_error(
        error,
        RELATION_EVIDENCE_DAO_ERROR_NOT_FOUND
    );

    g_clear_error(
        &error
    );

    test_relation_evidence_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie unlink() et la conservation des deux objets.
 */
static void test_relation_evidence_dao_unlink_preserves_objects(void)
{
    static const char *const relation_identifier =
        "40000000-0000-4000-8000-000000000006";

    static const char *const evidence_identifier =
        "50000000-0000-4000-8000-000000000006";

    TestRelationEvidenceDaoFixture fixture =
        test_relation_evidence_dao_fixture_create();

    gboolean association_exists =
        TRUE;

    GError *error =
        NULL;

    test_relation_evidence_dao_insert_standard_entities(
        &fixture
    );

    test_relation_evidence_dao_insert_relation(
        &fixture,
        relation_identifier,
        TEST_ENTITY_A_IDENTIFIER,
        TEST_ENTITY_B_IDENTIFIER,
        "owns"
    );

    test_relation_evidence_dao_insert_evidence(
        &fixture,
        evidence_identifier
    );

    assert(
        relation_evidence_dao_link(
            fixture.relation_evidence_dao,
            relation_identifier,
            evidence_identifier,
            &error
        )
    );

    assert(error == NULL);

    assert(
        relation_evidence_dao_unlink(
            fixture.relation_evidence_dao,
            relation_identifier,
            evidence_identifier,
            &error
        )
    );

    assert(error == NULL);

    assert(
        relation_evidence_dao_exists(
            fixture.relation_evidence_dao,
            relation_identifier,
            evidence_identifier,
            &association_exists,
            &error
        )
    );

    assert(error == NULL);
    assert(!association_exists);

    test_relation_evidence_dao_assert_relation_exists(
        &fixture,
        relation_identifier
    );

    test_relation_evidence_dao_assert_evidence_exists(
        &fixture,
        evidence_identifier
    );

    test_relation_evidence_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus de supprimer une association absente.
 */
static void test_relation_evidence_dao_unlink_missing(void)
{
    TestRelationEvidenceDaoFixture fixture =
        test_relation_evidence_dao_fixture_create();

    GError *error =
        NULL;

    assert(
        !relation_evidence_dao_unlink(
            fixture.relation_evidence_dao,
            "40000000-0000-4000-8000-000000000007",
            "50000000-0000-4000-8000-000000000007",
            &error
        )
    );

    test_relation_evidence_dao_assert_error(
        error,
        RELATION_EVIDENCE_DAO_ERROR_NOT_FOUND
    );

    g_clear_error(
        &error
    );

    test_relation_evidence_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie une relation associée à plusieurs preuves triées.
 */
static void test_relation_evidence_dao_list_evidence_identifiers(void)
{
    static const char *const relation_identifier =
        "40000000-0000-4000-8000-000000000008";

    static const char *const first_evidence_identifier =
        "50000000-0000-4000-8000-000000000001";

    static const char *const second_evidence_identifier =
        "50000000-0000-4000-8000-000000000002";

    static const char *const third_evidence_identifier =
        "50000000-0000-4000-8000-000000000003";

    TestRelationEvidenceDaoFixture fixture =
        test_relation_evidence_dao_fixture_create();

    GPtrArray *identifiers =
        NULL;

    GError *error =
        NULL;

    test_relation_evidence_dao_insert_standard_entities(
        &fixture
    );

    test_relation_evidence_dao_insert_relation(
        &fixture,
        relation_identifier,
        TEST_ENTITY_A_IDENTIFIER,
        TEST_ENTITY_B_IDENTIFIER,
        "uses"
    );

    identifiers =
        relation_evidence_dao_list_evidence_identifiers(
            fixture.relation_evidence_dao,
            relation_identifier,
            &error
        );

    assert(identifiers != NULL);
    assert(error == NULL);
    assert(identifiers->len == 0);

    g_ptr_array_unref(
        identifiers
    );

    test_relation_evidence_dao_insert_evidence(
        &fixture,
        third_evidence_identifier
    );

    test_relation_evidence_dao_insert_evidence(
        &fixture,
        first_evidence_identifier
    );

    test_relation_evidence_dao_insert_evidence(
        &fixture,
        second_evidence_identifier
    );

    assert(
        relation_evidence_dao_link(
            fixture.relation_evidence_dao,
            relation_identifier,
            third_evidence_identifier,
            &error
        )
    );

    assert(error == NULL);

    assert(
        relation_evidence_dao_link(
            fixture.relation_evidence_dao,
            relation_identifier,
            first_evidence_identifier,
            &error
        )
    );

    assert(error == NULL);

    assert(
        relation_evidence_dao_link(
            fixture.relation_evidence_dao,
            relation_identifier,
            second_evidence_identifier,
            &error
        )
    );

    assert(error == NULL);

    identifiers =
        relation_evidence_dao_list_evidence_identifiers(
            fixture.relation_evidence_dao,
            relation_identifier,
            &error
        );

    assert(identifiers != NULL);
    assert(error == NULL);
    assert(identifiers->len == 3);

    assert(
        strcmp(
            g_ptr_array_index(
                identifiers,
                0
            ),
            first_evidence_identifier
        ) == 0
    );

    assert(
        strcmp(
            g_ptr_array_index(
                identifiers,
                1
            ),
            second_evidence_identifier
        ) == 0
    );

    assert(
        strcmp(
            g_ptr_array_index(
                identifiers,
                2
            ),
            third_evidence_identifier
        ) == 0
    );

    g_ptr_array_unref(
        identifiers
    );

    test_relation_evidence_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie une preuve associée à plusieurs relations triées.
 */
static void test_relation_evidence_dao_list_relation_identifiers(void)
{
    static const char *const evidence_identifier =
        "50000000-0000-4000-8000-000000000009";

    static const char *const first_relation_identifier =
        "40000000-0000-4000-8000-000000000001";

    static const char *const second_relation_identifier =
        "40000000-0000-4000-8000-000000000002";

    static const char *const third_relation_identifier =
        "40000000-0000-4000-8000-000000000003";

    TestRelationEvidenceDaoFixture fixture =
        test_relation_evidence_dao_fixture_create();

    GPtrArray *identifiers =
        NULL;

    GError *error =
        NULL;

    test_relation_evidence_dao_insert_standard_entities(
        &fixture
    );

    test_relation_evidence_dao_insert_evidence(
        &fixture,
        evidence_identifier
    );

    identifiers =
        relation_evidence_dao_list_relation_identifiers(
            fixture.relation_evidence_dao,
            evidence_identifier,
            &error
        );

    assert(identifiers != NULL);
    assert(error == NULL);
    assert(identifiers->len == 0);

    g_ptr_array_unref(
        identifiers
    );

    test_relation_evidence_dao_insert_relation(
        &fixture,
        third_relation_identifier,
        TEST_ENTITY_C_IDENTIFIER,
        TEST_ENTITY_A_IDENTIFIER,
        "knows"
    );

    test_relation_evidence_dao_insert_relation(
        &fixture,
        first_relation_identifier,
        TEST_ENTITY_A_IDENTIFIER,
        TEST_ENTITY_B_IDENTIFIER,
        "uses"
    );

    test_relation_evidence_dao_insert_relation(
        &fixture,
        second_relation_identifier,
        TEST_ENTITY_B_IDENTIFIER,
        TEST_ENTITY_C_IDENTIFIER,
        "controls"
    );

    assert(
        relation_evidence_dao_link(
            fixture.relation_evidence_dao,
            third_relation_identifier,
            evidence_identifier,
            &error
        )
    );

    assert(error == NULL);

    assert(
        relation_evidence_dao_link(
            fixture.relation_evidence_dao,
            first_relation_identifier,
            evidence_identifier,
            &error
        )
    );

    assert(error == NULL);

    assert(
        relation_evidence_dao_link(
            fixture.relation_evidence_dao,
            second_relation_identifier,
            evidence_identifier,
            &error
        )
    );

    assert(error == NULL);

    identifiers =
        relation_evidence_dao_list_relation_identifiers(
            fixture.relation_evidence_dao,
            evidence_identifier,
            &error
        );

    assert(identifiers != NULL);
    assert(error == NULL);
    assert(identifiers->len == 3);

    assert(
        strcmp(
            g_ptr_array_index(
                identifiers,
                0
            ),
            first_relation_identifier
        ) == 0
    );

    assert(
        strcmp(
            g_ptr_array_index(
                identifiers,
                1
            ),
            second_relation_identifier
        ) == 0
    );

    assert(
        strcmp(
            g_ptr_array_index(
                identifiers,
                2
            ),
            third_relation_identifier
        ) == 0
    );

    g_ptr_array_unref(
        identifiers
    );

    test_relation_evidence_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie les arguments invalides et GError facultatif.
 */
static void test_relation_evidence_dao_invalid_arguments(void)
{
    TestRelationEvidenceDaoFixture fixture =
        test_relation_evidence_dao_fixture_create();

    GPtrArray *identifiers =
        NULL;

    gboolean association_exists =
        FALSE;

    GError *error =
        NULL;

    assert(
        !relation_evidence_dao_link(
            NULL,
            "40000000-0000-4000-8000-000000000010",
            "50000000-0000-4000-8000-000000000010",
            &error
        )
    );

    test_relation_evidence_dao_assert_error(
        error,
        RELATION_EVIDENCE_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    assert(
        !relation_evidence_dao_link(
            fixture.relation_evidence_dao,
            "relation-invalide",
            "50000000-0000-4000-8000-000000000010",
            &error
        )
    );

    test_relation_evidence_dao_assert_error(
        error,
        RELATION_EVIDENCE_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    assert(
        !relation_evidence_dao_unlink(
            fixture.relation_evidence_dao,
            "40000000-0000-4000-8000-000000000010",
            "preuve-invalide",
            &error
        )
    );

    test_relation_evidence_dao_assert_error(
        error,
        RELATION_EVIDENCE_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    assert(
        !relation_evidence_dao_exists(
            fixture.relation_evidence_dao,
            "40000000-0000-4000-8000-000000000010",
            "50000000-0000-4000-8000-000000000010",
            NULL,
            &error
        )
    );

    test_relation_evidence_dao_assert_error(
        error,
        RELATION_EVIDENCE_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    identifiers =
        relation_evidence_dao_list_evidence_identifiers(
            fixture.relation_evidence_dao,
            "relation-invalide",
            &error
        );

    assert(identifiers == NULL);

    test_relation_evidence_dao_assert_error(
        error,
        RELATION_EVIDENCE_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    identifiers =
        relation_evidence_dao_list_relation_identifiers(
            NULL,
            "50000000-0000-4000-8000-000000000010",
            &error
        );

    assert(identifiers == NULL);

    test_relation_evidence_dao_assert_error(
        error,
        RELATION_EVIDENCE_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    assert(
        !relation_evidence_dao_link(
            fixture.relation_evidence_dao,
            "relation-invalide",
            "preuve-invalide",
            NULL
        )
    );

    assert(
        relation_evidence_dao_exists(
            fixture.relation_evidence_dao,
            "40000000-0000-4000-8000-000000000010",
            "50000000-0000-4000-8000-000000000010",
            &association_exists,
            &error
        )
    );

    assert(error == NULL);
    assert(!association_exists);

    test_relation_evidence_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le signalement d'une table absente.
 */
static void test_relation_evidence_dao_missing_table(void)
{
    Database *database =
        NULL;

    RelationEvidenceDao *relation_evidence_dao =
        NULL;

    GPtrArray *identifiers =
        NULL;

    GError *error =
        NULL;

    database =
        database_open(
            ":memory:"
        );

    assert(database != NULL);

    relation_evidence_dao =
        relation_evidence_dao_new(
            database,
            &error
        );

    assert(relation_evidence_dao != NULL);
    assert(error == NULL);

    g_test_expect_message(
        NULL,
        G_LOG_LEVEL_WARNING,
        "*Impossible de préparer la requête SQL*"
    );

    identifiers =
        relation_evidence_dao_list_evidence_identifiers(
            relation_evidence_dao,
            "40000000-0000-4000-8000-000000000011",
            &error
        );

    g_test_assert_expected_messages();

    assert(identifiers == NULL);

    test_relation_evidence_dao_assert_error(
        error,
        RELATION_EVIDENCE_DAO_ERROR_SCHEMA
    );

    g_clear_error(
        &error
    );

    relation_evidence_dao_free(
        relation_evidence_dao
    );

    database_close(
        database
    );
}

/**
 * @brief Vérifie ON DELETE CASCADE lors de la suppression d'une relation.
 */
static void test_relation_evidence_dao_relation_delete_cascade(void)
{
    static const char *const relation_identifier =
        "40000000-0000-4000-8000-000000000012";

    static const char *const evidence_identifier =
        "50000000-0000-4000-8000-000000000012";

    TestRelationEvidenceDaoFixture fixture =
        test_relation_evidence_dao_fixture_create();

    DatabaseStatement *statement =
        NULL;

    GError *error =
        NULL;

    test_relation_evidence_dao_insert_standard_entities(
        &fixture
    );

    test_relation_evidence_dao_insert_relation(
        &fixture,
        relation_identifier,
        TEST_ENTITY_A_IDENTIFIER,
        TEST_ENTITY_B_IDENTIFIER,
        "uses"
    );

    test_relation_evidence_dao_insert_evidence(
        &fixture,
        evidence_identifier
    );

    assert(
        relation_evidence_dao_link(
            fixture.relation_evidence_dao,
            relation_identifier,
            evidence_identifier,
            &error
        )
    );

    assert(error == NULL);

    statement =
        database_statement_prepare(
            fixture.database,
            "DELETE FROM relations "
            "WHERE id = ?;"
        );

    assert(statement != NULL);

    assert(
        database_statement_bind_text(
            statement,
            1,
            relation_identifier
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

    assert(
        test_relation_evidence_dao_count_rows(
            fixture.database
        ) == 0
    );

    test_relation_evidence_dao_assert_evidence_exists(
        &fixture,
        evidence_identifier
    );

    test_relation_evidence_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie ON DELETE RESTRICT lors de la suppression d'une preuve.
 */
static void test_relation_evidence_dao_evidence_delete_restrict(void)
{
    static const char *const relation_identifier =
        "40000000-0000-4000-8000-000000000013";

    static const char *const evidence_identifier =
        "50000000-0000-4000-8000-000000000013";

    TestRelationEvidenceDaoFixture fixture =
        test_relation_evidence_dao_fixture_create();

    DatabaseStatement *statement =
        NULL;

    GError *error =
        NULL;

    test_relation_evidence_dao_insert_standard_entities(
        &fixture
    );

    test_relation_evidence_dao_insert_relation(
        &fixture,
        relation_identifier,
        TEST_ENTITY_A_IDENTIFIER,
        TEST_ENTITY_B_IDENTIFIER,
        "controls"
    );

    test_relation_evidence_dao_insert_evidence(
        &fixture,
        evidence_identifier
    );

    assert(
        relation_evidence_dao_link(
            fixture.relation_evidence_dao,
            relation_identifier,
            evidence_identifier,
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

    g_test_assert_expected_messages();

    database_statement_finalize(
        statement
    );

    assert(
        test_relation_evidence_dao_count_rows(
            fixture.database
        ) == 1
    );

    test_relation_evidence_dao_assert_relation_exists(
        &fixture,
        relation_identifier
    );

    test_relation_evidence_dao_assert_evidence_exists(
        &fixture,
        evidence_identifier
    );

    test_relation_evidence_dao_fixture_clear(
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

    test_relation_evidence_dao_new_null_database();
    test_relation_evidence_dao_lifecycle();
    test_relation_evidence_dao_link_and_exists();
    test_relation_evidence_dao_link_duplicate();
    test_relation_evidence_dao_composite_primary_key();
    test_relation_evidence_dao_link_missing_relation();
    test_relation_evidence_dao_link_missing_evidence();
    test_relation_evidence_dao_unlink_preserves_objects();
    test_relation_evidence_dao_unlink_missing();
    test_relation_evidence_dao_list_evidence_identifiers();
    test_relation_evidence_dao_list_relation_identifiers();
    test_relation_evidence_dao_invalid_arguments();
    test_relation_evidence_dao_missing_table();
    test_relation_evidence_dao_relation_delete_cascade();
    test_relation_evidence_dao_evidence_delete_restrict();

    printf(
        "RelationEvidenceDao : tous les tests sont valides.\n"
    );

    return 0;
}
