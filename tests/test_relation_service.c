/******************************************************************************
 * @file test_relation_service.c
 * @brief Tests transactionnels du service de gestion des relations.
 ******************************************************************************/

#include "core/relation_service.h"
#include "core/relation_service_test.h"

#include "dao/entity_dao.h"
#include "dao/evidence_dao.h"
#include "dao/relation_dao.h"
#include "dao/relation_evidence_dao.h"

#include "database/database.h"
#include "database/statement.h"
#include "database/transaction.h"

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
 * @brief Environnement temporaire d'un test RelationService.
 */
typedef struct
{
    char *temporary_directory;
    char *database_path;

    Database *database;

    EntityDao *entity_dao;
    EvidenceDao *evidence_dao;
    RelationDao *relation_dao;
    RelationEvidenceDao *relation_evidence_dao;

    RelationService *relation_service;
} TestRelationServiceFixture;

/**
 * @brief Vérifie le domaine et le code d'une erreur RelationService.
 */
static void test_relation_service_assert_error(
    const GError *error,
    RelationServiceError expected_code
)
{
    assert(error != NULL);

    assert(
        error->domain ==
        RELATION_SERVICE_ERROR
    );

    assert(
        error->code ==
        (gint) expected_code
    );

    assert(error->message != NULL);
    assert(error->message[0] != '\0');
}

/**
 * @brief Crée une base temporaire et les composants nécessaires.
 */
static TestRelationServiceFixture
test_relation_service_fixture_create(void)
{
    TestRelationServiceFixture fixture =
        {0};

    GError *error =
        NULL;

    fixture.temporary_directory =
        g_dir_make_tmp(
            "labfy-relation-service-test-XXXXXX",
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
            "Enquete_RelationService",
            fixture.temporary_directory
        )
    );

    fixture.database =
        database_open(
            fixture.database_path
        );

    assert(fixture.database != NULL);

    fixture.entity_dao =
        entity_dao_new(
            fixture.database,
            &error
        );

    assert(fixture.entity_dao != NULL);
    assert(error == NULL);

    fixture.evidence_dao =
        evidence_dao_new(
            fixture.database,
            &error
        );

    assert(fixture.evidence_dao != NULL);
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

    fixture.relation_service =
        relation_service_new(
            fixture.database,
            &error
        );

    assert(fixture.relation_service != NULL);
    assert(error == NULL);

    relation_service_test_reset_hooks();

    return fixture;
}

/**
 * @brief Libère la fixture et supprime la base temporaire.
 */
static void test_relation_service_fixture_clear(
    TestRelationServiceFixture *fixture
)
{
    assert(fixture != NULL);

    relation_service_test_reset_hooks();

    relation_service_free(
        fixture->relation_service
    );

    relation_evidence_dao_free(
        fixture->relation_evidence_dao
    );

    relation_dao_free(
        fixture->relation_dao
    );

    evidence_dao_free(
        fixture->evidence_dao
    );

    entity_dao_free(
        fixture->entity_dao
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

    fixture->relation_service =
        NULL;

    fixture->relation_evidence_dao =
        NULL;

    fixture->relation_dao =
        NULL;

    fixture->evidence_dao =
        NULL;

    fixture->entity_dao =
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
static EntityRecord *test_relation_service_create_entity(
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
            "2026-07-21T18:00:00Z",
            "2026-07-21T18:00:00Z",
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
static void test_relation_service_insert_entity(
    TestRelationServiceFixture *fixture,
    const char *identifier,
    const char *value
)
{
    EntityRecord *entity_record =
        NULL;

    GError *error =
        NULL;

    entity_record =
        test_relation_service_create_entity(
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
 * @brief Insère les entités standard.
 */
static void test_relation_service_insert_standard_entities(
    TestRelationServiceFixture *fixture
)
{
    test_relation_service_insert_entity(
        fixture,
        TEST_ENTITY_A_IDENTIFIER,
        "Personne A"
    );

    test_relation_service_insert_entity(
        fixture,
        TEST_ENTITY_B_IDENTIFIER,
        "Personne B"
    );

    test_relation_service_insert_entity(
        fixture,
        TEST_ENTITY_C_IDENTIFIER,
        "Personne C"
    );
}

/**
 * @brief Crée une preuve possédant des valeurs uniques.
 */
static EvidenceRecord *test_relation_service_create_evidence(
    const char *identifier
)
{
    EvidenceRecord *evidence_record =
        NULL;

    char *internal_name =
        NULL;

    char *relative_path =
        NULL;

    char sha256[65] =
        {0};

    char hash_character =
        'a';

    GError *error =
        NULL;

    assert(identifier != NULL);

    hash_character =
        identifier[strlen(identifier) - 1U];

    if (!g_ascii_isxdigit(
            hash_character
        ))
    {
        hash_character =
            'a';
    }

    memset(
        sha256,
        hash_character,
        64U
    );

    sha256[64] =
        '\0';

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
            sha256,
            "2026-07-21T18:10:00Z",
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
static void test_relation_service_insert_evidence(
    TestRelationServiceFixture *fixture,
    const char *identifier
)
{
    EvidenceRecord *evidence_record =
        NULL;

    GError *error =
        NULL;

    evidence_record =
        test_relation_service_create_evidence(
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
 * @brief Crée une relation de test.
 */
static RelationRecord *test_relation_service_create_relation(
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
            "Relation créée par un test transactionnel.",
            75,
            "2026-07-21T18:20:00Z",
            "2026-07-21T18:20:00Z",
            RELATION_STATUS_ACTIVE,
            &error
        );

    assert(relation_record != NULL);
    assert(error == NULL);

    return relation_record;
}

/**
 * @brief Crée un tableau possédant des copies d'UUID.
 */
static GPtrArray *test_relation_service_create_identifier_array(
    const char *first_identifier,
    const char *second_identifier,
    const char *third_identifier
)
{
    GPtrArray *identifiers =
        NULL;

    identifiers =
        g_ptr_array_new_with_free_func(
            g_free
        );

    assert(identifiers != NULL);

    if (first_identifier != NULL)
    {
        g_ptr_array_add(
            identifiers,
            g_strdup(
                first_identifier
            )
        );
    }

    if (second_identifier != NULL)
    {
        g_ptr_array_add(
            identifiers,
            g_strdup(
                second_identifier
            )
        );
    }

    if (third_identifier != NULL)
    {
        g_ptr_array_add(
            identifiers,
            g_strdup(
                third_identifier
            )
        );
    }

    return identifiers;
}

/**
 * @brief Compte les lignes correspondant à un UUID.
 */
static int64_t test_relation_service_count_by_identifier(
    Database *database,
    const char *sql,
    const char *identifier
)
{
    DatabaseStatement *statement =
        NULL;

    int64_t row_count =
        -1;

    statement =
        database_statement_prepare(
            database,
            sql
        );

    assert(statement != NULL);

    assert(
        database_statement_bind_text(
            statement,
            1,
            identifier
        )
    );

    assert(
        database_statement_step(
            statement
        ) == DATABASE_STATEMENT_STEP_ROW
    );

    assert(
        database_statement_column_int64(
            statement,
            0,
            &row_count
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

    return row_count;
}

/**
 * @brief Compte une relation précise.
 */
static int64_t test_relation_service_count_relation(
    Database *database,
    const char *relation_identifier
)
{
    return test_relation_service_count_by_identifier(
        database,
        "SELECT COUNT(*) "
        "FROM relations "
        "WHERE id = ?;",
        relation_identifier
    );
}

/**
 * @brief Compte les associations d'une relation.
 */
static int64_t test_relation_service_count_associations(
    Database *database,
    const char *relation_identifier
)
{
    return test_relation_service_count_by_identifier(
        database,
        "SELECT COUNT(*) "
        "FROM relation_preuves "
        "WHERE relation_id = ?;",
        relation_identifier
    );
}

/**
 * @brief Vérifie qu'une preuve existe encore.
 */
static void test_relation_service_assert_evidence_exists(
    TestRelationServiceFixture *fixture,
    const char *evidence_identifier
)
{
    EvidenceRecord *evidence_record =
        NULL;

    GError *error =
        NULL;

    evidence_record =
        evidence_dao_find_by_identifier(
            fixture->evidence_dao,
            evidence_identifier,
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
static void test_relation_service_new_null_database(void)
{
    RelationService *relation_service =
        NULL;

    GError *error =
        NULL;

    relation_service =
        relation_service_new(
            NULL,
            &error
        );

    assert(relation_service == NULL);

    test_relation_service_assert_error(
        error,
        RELATION_SERVICE_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );
}

/**
 * @brief Vérifie la création et la destruction du service.
 */
static void test_relation_service_lifecycle(void)
{
    TestRelationServiceFixture fixture =
        test_relation_service_fixture_create();

    DatabaseStatement *statement =
        NULL;

    relation_service_free(
        fixture.relation_service
    );

    fixture.relation_service =
        NULL;

    /*
     * La connexion doit rester exploitable après la destruction du service.
     */
    statement =
        database_statement_prepare(
            fixture.database,
            "SELECT 1;"
        );

    assert(statement != NULL);

    assert(
        database_statement_step(
            statement
        ) == DATABASE_STATEMENT_STEP_ROW
    );

    database_statement_finalize(
        statement
    );

    relation_service_free(
        NULL
    );

    test_relation_service_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie la création sans tableau de preuves.
 */
static void test_relation_service_create_without_array(void)
{
    static const char *const relation_identifier =
        "40000000-0000-4000-8000-000000000001";

    TestRelationServiceFixture fixture =
        test_relation_service_fixture_create();

    RelationRecord *relation_record =
        NULL;

    GError *error =
        NULL;

    test_relation_service_insert_standard_entities(
        &fixture
    );

    relation_record =
        test_relation_service_create_relation(
            relation_identifier,
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_B_IDENTIFIER,
            "uses"
        );

    assert(
        relation_service_create(
            fixture.relation_service,
            relation_record,
            NULL,
            &error
        )
    );

    assert(error == NULL);

    assert(
        test_relation_service_count_relation(
            fixture.database,
            relation_identifier
        ) == 1
    );

    assert(
        test_relation_service_count_associations(
            fixture.database,
            relation_identifier
        ) == 0
    );

    relation_record_free(
        relation_record
    );

    test_relation_service_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie la création avec un tableau vide.
 */
static void test_relation_service_create_with_empty_array(void)
{
    static const char *const relation_identifier =
        "40000000-0000-4000-8000-000000000002";

    TestRelationServiceFixture fixture =
        test_relation_service_fixture_create();

    RelationRecord *relation_record =
        NULL;

    GPtrArray *evidence_identifiers =
        NULL;

    GError *error =
        NULL;

    test_relation_service_insert_standard_entities(
        &fixture
    );

    relation_record =
        test_relation_service_create_relation(
            relation_identifier,
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_B_IDENTIFIER,
            "controls"
        );

    evidence_identifiers =
        g_ptr_array_new_with_free_func(
            g_free
        );

    assert(evidence_identifiers != NULL);

    assert(
        relation_service_create(
            fixture.relation_service,
            relation_record,
            evidence_identifiers,
            &error
        )
    );

    assert(error == NULL);
    assert(evidence_identifiers->len == 0);

    assert(
        test_relation_service_count_relation(
            fixture.database,
            relation_identifier
        ) == 1
    );

    g_ptr_array_unref(
        evidence_identifiers
    );

    relation_record_free(
        relation_record
    );

    test_relation_service_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie la création avec une preuve.
 */
static void test_relation_service_create_with_one_evidence(void)
{
    static const char *const relation_identifier =
        "40000000-0000-4000-8000-000000000003";

    static const char *const evidence_identifier =
        "50000000-0000-4000-8000-000000000001";

    TestRelationServiceFixture fixture =
        test_relation_service_fixture_create();

    RelationRecord *relation_record =
        NULL;

    GPtrArray *evidence_identifiers =
        NULL;

    gboolean association_exists =
        FALSE;

    GError *error =
        NULL;

    test_relation_service_insert_standard_entities(
        &fixture
    );

    test_relation_service_insert_evidence(
        &fixture,
        evidence_identifier
    );

    relation_record =
        test_relation_service_create_relation(
            relation_identifier,
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_B_IDENTIFIER,
            "owns"
        );

    evidence_identifiers =
        test_relation_service_create_identifier_array(
            evidence_identifier,
            NULL,
            NULL
        );

    assert(
        relation_service_create(
            fixture.relation_service,
            relation_record,
            evidence_identifiers,
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

    assert(evidence_identifiers->len == 1);

    assert(
        strcmp(
            g_ptr_array_index(
                evidence_identifiers,
                0
            ),
            evidence_identifier
        ) == 0
    );

    g_ptr_array_unref(
        evidence_identifiers
    );

    relation_record_free(
        relation_record
    );

    test_relation_service_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie la création avec plusieurs preuves.
 */
static void test_relation_service_create_with_multiple_evidence(void)
{
    static const char *const relation_identifier =
        "40000000-0000-4000-8000-000000000004";

    static const char *const first_evidence_identifier =
        "50000000-0000-4000-8000-000000000002";

    static const char *const second_evidence_identifier =
        "50000000-0000-4000-8000-000000000003";

    static const char *const third_evidence_identifier =
        "50000000-0000-4000-8000-000000000004";

    TestRelationServiceFixture fixture =
        test_relation_service_fixture_create();

    RelationRecord *relation_record =
        NULL;

    GPtrArray *requested_identifiers =
        NULL;

    GPtrArray *loaded_identifiers =
        NULL;

    GError *error =
        NULL;

    test_relation_service_insert_standard_entities(
        &fixture
    );

    test_relation_service_insert_evidence(
        &fixture,
        first_evidence_identifier
    );

    test_relation_service_insert_evidence(
        &fixture,
        second_evidence_identifier
    );

    test_relation_service_insert_evidence(
        &fixture,
        third_evidence_identifier
    );

    relation_record =
        test_relation_service_create_relation(
            relation_identifier,
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_C_IDENTIFIER,
            "knows"
        );

    requested_identifiers =
        test_relation_service_create_identifier_array(
            third_evidence_identifier,
            first_evidence_identifier,
            second_evidence_identifier
        );

    assert(
        relation_service_create(
            fixture.relation_service,
            relation_record,
            requested_identifiers,
            &error
        )
    );

    assert(error == NULL);

    loaded_identifiers =
        relation_evidence_dao_list_evidence_identifiers(
            fixture.relation_evidence_dao,
            relation_identifier,
            &error
        );

    assert(loaded_identifiers != NULL);
    assert(error == NULL);
    assert(loaded_identifiers->len == 3);

    assert(
        strcmp(
            g_ptr_array_index(
                loaded_identifiers,
                0
            ),
            first_evidence_identifier
        ) == 0
    );

    assert(
        strcmp(
            g_ptr_array_index(
                loaded_identifiers,
                1
            ),
            second_evidence_identifier
        ) == 0
    );

    assert(
        strcmp(
            g_ptr_array_index(
                loaded_identifiers,
                2
            ),
            third_evidence_identifier
        ) == 0
    );

    g_ptr_array_unref(
        loaded_identifiers
    );

    g_ptr_array_unref(
        requested_identifiers
    );

    relation_record_free(
        relation_record
    );

    test_relation_service_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie les arguments publics invalides.
 */
static void test_relation_service_invalid_arguments(void)
{
    TestRelationServiceFixture fixture =
        test_relation_service_fixture_create();

    RelationRecord *relation_record =
        NULL;

    GPtrArray *identifiers =
        NULL;

    GError *error =
        NULL;

    test_relation_service_insert_standard_entities(
        &fixture
    );

    relation_record =
        test_relation_service_create_relation(
            "40000000-0000-4000-8000-000000000005",
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_B_IDENTIFIER,
            "uses"
        );

    assert(
        !relation_service_create(
            NULL,
            relation_record,
            NULL,
            &error
        )
    );

    test_relation_service_assert_error(
        error,
        RELATION_SERVICE_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    assert(
        !relation_service_create(
            fixture.relation_service,
            NULL,
            NULL,
            &error
        )
    );

    test_relation_service_assert_error(
        error,
        RELATION_SERVICE_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    identifiers =
        g_ptr_array_new();

    assert(identifiers != NULL);

    g_ptr_array_add(
        identifiers,
        NULL
    );

    assert(
        !relation_service_create(
            fixture.relation_service,
            relation_record,
            identifiers,
            &error
        )
    );

    test_relation_service_assert_error(
        error,
        RELATION_SERVICE_ERROR_INVALID_ARGUMENT
    );

    assert(
        test_relation_service_count_relation(
            fixture.database,
            relation_record_get_identifier(
                relation_record
            )
        ) == 0
    );

    g_clear_error(
        &error
    );

    g_ptr_array_unref(
        identifiers
    );

    relation_record_free(
        relation_record
    );

    test_relation_service_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d'un UUID de preuve invalide avant transaction.
 */
static void test_relation_service_invalid_evidence_identifier(void)
{
    static const char *const relation_identifier =
        "40000000-0000-4000-8000-000000000006";

    TestRelationServiceFixture fixture =
        test_relation_service_fixture_create();

    RelationRecord *relation_record =
        NULL;

    GPtrArray *identifiers =
        NULL;

    GError *error =
        NULL;

    test_relation_service_insert_standard_entities(
        &fixture
    );

    relation_record =
        test_relation_service_create_relation(
            relation_identifier,
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_B_IDENTIFIER,
            "uses"
        );

    identifiers =
        test_relation_service_create_identifier_array(
            "preuve-invalide",
            NULL,
            NULL
        );

    assert(
        !relation_service_create(
            fixture.relation_service,
            relation_record,
            identifiers,
            &error
        )
    );

    test_relation_service_assert_error(
        error,
        RELATION_SERVICE_ERROR_INVALID_ARGUMENT
    );

    assert(
        test_relation_service_count_relation(
            fixture.database,
            relation_identifier
        ) == 0
    );

    assert(
        !database_transaction_is_active(
            fixture.database
        )
    );

    g_clear_error(
        &error
    );

    g_ptr_array_unref(
        identifiers
    );

    relation_record_free(
        relation_record
    );

    test_relation_service_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus des doublons avant transaction.
 */
static void test_relation_service_duplicate_evidence_request(void)
{
    static const char *const relation_identifier =
        "40000000-0000-4000-8000-000000000007";

    static const char *const evidence_identifier =
        "50000000-0000-4000-8000-000000000005";

    TestRelationServiceFixture fixture =
        test_relation_service_fixture_create();

    RelationRecord *relation_record =
        NULL;

    GPtrArray *identifiers =
        NULL;

    GError *error =
        NULL;

    test_relation_service_insert_standard_entities(
        &fixture
    );

    test_relation_service_insert_evidence(
        &fixture,
        evidence_identifier
    );

    relation_record =
        test_relation_service_create_relation(
            relation_identifier,
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_B_IDENTIFIER,
            "controls"
        );

    identifiers =
        test_relation_service_create_identifier_array(
            evidence_identifier,
            evidence_identifier,
            NULL
        );

    assert(
        !relation_service_create(
            fixture.relation_service,
            relation_record,
            identifiers,
            &error
        )
    );

    test_relation_service_assert_error(
        error,
        RELATION_SERVICE_ERROR_INVALID_ARGUMENT
    );

    assert(
        strstr(
            error->message,
            "plusieurs fois"
        ) != NULL
    );

    assert(
        test_relation_service_count_relation(
            fixture.database,
            relation_identifier
        ) == 0
    );

    assert(
        !database_transaction_is_active(
            fixture.database
        )
    );

    g_clear_error(
        &error
    );

    g_ptr_array_unref(
        identifiers
    );

    relation_record_free(
        relation_record
    );

    test_relation_service_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le rollback lorsque l'UUID de relation existe déjà.
 */
static void test_relation_service_duplicate_relation_rollback(void)
{
    static const char *const relation_identifier =
        "40000000-0000-4000-8000-000000000008";

    TestRelationServiceFixture fixture =
        test_relation_service_fixture_create();

    RelationRecord *existing_record =
        NULL;

    RelationRecord *duplicate_record =
        NULL;

    GError *error =
        NULL;

    test_relation_service_insert_standard_entities(
        &fixture
    );

    existing_record =
        test_relation_service_create_relation(
            relation_identifier,
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_B_IDENTIFIER,
            "uses"
        );

    assert(
        relation_dao_insert(
            fixture.relation_dao,
            existing_record,
            &error
        )
    );

    assert(error == NULL);

    duplicate_record =
        test_relation_service_create_relation(
            relation_identifier,
            TEST_ENTITY_B_IDENTIFIER,
            TEST_ENTITY_C_IDENTIFIER,
            "controls"
        );

    assert(
        !relation_service_create(
            fixture.relation_service,
            duplicate_record,
            NULL,
            &error
        )
    );

    test_relation_service_assert_error(
        error,
        RELATION_SERVICE_ERROR_RELATION_INSERT
    );

    assert(
        test_relation_service_count_relation(
            fixture.database,
            relation_identifier
        ) == 1
    );

    assert(
        !database_transaction_is_active(
            fixture.database
        )
    );

    g_clear_error(
        &error
    );

    relation_record_free(
        duplicate_record
    );

    relation_record_free(
        existing_record
    );

    test_relation_service_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie l'atomicité lorsqu'une seconde preuve est absente.
 */
static void test_relation_service_missing_second_evidence_rollback(void)
{
    static const char *const relation_identifier =
        "40000000-0000-4000-8000-000000000009";

    static const char *const existing_evidence_identifier =
        "50000000-0000-4000-8000-000000000006";

    static const char *const missing_evidence_identifier =
        "50000000-0000-4000-8000-000000000007";

    TestRelationServiceFixture fixture =
        test_relation_service_fixture_create();

    RelationRecord *relation_record =
        NULL;

    GPtrArray *identifiers =
        NULL;

    GError *error =
        NULL;

    test_relation_service_insert_standard_entities(
        &fixture
    );

    test_relation_service_insert_evidence(
        &fixture,
        existing_evidence_identifier
    );

    relation_record =
        test_relation_service_create_relation(
            relation_identifier,
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_C_IDENTIFIER,
            "owns"
        );

    identifiers =
        test_relation_service_create_identifier_array(
            existing_evidence_identifier,
            missing_evidence_identifier,
            NULL
        );

    assert(
        !relation_service_create(
            fixture.relation_service,
            relation_record,
            identifiers,
            &error
        )
    );

    test_relation_service_assert_error(
        error,
        RELATION_SERVICE_ERROR_EVIDENCE_LINK
    );

    assert(
        test_relation_service_count_relation(
            fixture.database,
            relation_identifier
        ) == 0
    );

    assert(
        test_relation_service_count_associations(
            fixture.database,
            relation_identifier
        ) == 0
    );

    test_relation_service_assert_evidence_exists(
        &fixture,
        existing_evidence_identifier
    );

    assert(
        !database_transaction_is_active(
            fixture.database
        )
    );

    g_clear_error(
        &error
    );

    g_ptr_array_unref(
        identifiers
    );

    relation_record_free(
        relation_record
    );

    test_relation_service_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie que la connexion reste utilisable après un rollback.
 */
static void test_relation_service_recovery_after_failure(void)
{
    static const char *const failed_relation_identifier =
        "40000000-0000-4000-8000-000000000010";

    static const char *const valid_relation_identifier =
        "40000000-0000-4000-8000-000000000011";

    static const char *const existing_evidence_identifier =
        "50000000-0000-4000-8000-000000000008";

    static const char *const missing_evidence_identifier =
        "50000000-0000-4000-8000-000000000009";

    TestRelationServiceFixture fixture =
        test_relation_service_fixture_create();

    RelationRecord *failed_record =
        NULL;

    RelationRecord *valid_record =
        NULL;

    GPtrArray *failed_identifiers =
        NULL;

    GPtrArray *valid_identifiers =
        NULL;

    GError *error =
        NULL;

    test_relation_service_insert_standard_entities(
        &fixture
    );

    test_relation_service_insert_evidence(
        &fixture,
        existing_evidence_identifier
    );

    failed_record =
        test_relation_service_create_relation(
            failed_relation_identifier,
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_B_IDENTIFIER,
            "uses"
        );

    failed_identifiers =
        test_relation_service_create_identifier_array(
            missing_evidence_identifier,
            NULL,
            NULL
        );

    assert(
        !relation_service_create(
            fixture.relation_service,
            failed_record,
            failed_identifiers,
            &error
        )
    );

    test_relation_service_assert_error(
        error,
        RELATION_SERVICE_ERROR_EVIDENCE_LINK
    );

    g_clear_error(
        &error
    );

    valid_record =
        test_relation_service_create_relation(
            valid_relation_identifier,
            TEST_ENTITY_B_IDENTIFIER,
            TEST_ENTITY_C_IDENTIFIER,
            "controls"
        );

    valid_identifiers =
        test_relation_service_create_identifier_array(
            existing_evidence_identifier,
            NULL,
            NULL
        );

    assert(
        relation_service_create(
            fixture.relation_service,
            valid_record,
            valid_identifiers,
            &error
        )
    );

    assert(error == NULL);

    assert(
        test_relation_service_count_relation(
            fixture.database,
            failed_relation_identifier
        ) == 0
    );

    assert(
        test_relation_service_count_relation(
            fixture.database,
            valid_relation_identifier
        ) == 1
    );

    assert(
        test_relation_service_count_associations(
            fixture.database,
            valid_relation_identifier
        ) == 1
    );

    g_ptr_array_unref(
        valid_identifiers
    );

    g_ptr_array_unref(
        failed_identifiers
    );

    relation_record_free(
        valid_record
    );

    relation_record_free(
        failed_record
    );

    test_relation_service_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie l'échec du démarrage d'une transaction imbriquée.
 */
static void test_relation_service_transaction_begin_failure(void)
{
    static const char *const relation_identifier =
        "40000000-0000-4000-8000-000000000012";

    TestRelationServiceFixture fixture =
        test_relation_service_fixture_create();

    RelationRecord *relation_record =
        NULL;

    GError *error =
        NULL;

    test_relation_service_insert_standard_entities(
        &fixture
    );

    relation_record =
        test_relation_service_create_relation(
            relation_identifier,
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_B_IDENTIFIER,
            "uses"
        );

    assert(
        database_transaction_begin(
            fixture.database
        )
    );

    assert(
        !relation_service_create(
            fixture.relation_service,
            relation_record,
            NULL,
            &error
        )
    );

    test_relation_service_assert_error(
        error,
        RELATION_SERVICE_ERROR_TRANSACTION_BEGIN
    );

    assert(
        test_relation_service_count_relation(
            fixture.database,
            relation_identifier
        ) == 0
    );

    g_clear_error(
        &error
    );

    assert(
        database_transaction_rollback(
            fixture.database
        )
    );

    relation_record_free(
        relation_record
    );

    test_relation_service_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le rollback après un échec simulé du COMMIT.
 */
static void test_relation_service_commit_failure_rollback(void)
{
    static const char *const relation_identifier =
        "40000000-0000-4000-8000-000000000013";

    static const char *const evidence_identifier =
        "50000000-0000-4000-8000-00000000000a";

    TestRelationServiceFixture fixture =
        test_relation_service_fixture_create();

    RelationRecord *relation_record =
        NULL;

    GPtrArray *identifiers =
        NULL;

    GError *error =
        NULL;

    test_relation_service_insert_standard_entities(
        &fixture
    );

    test_relation_service_insert_evidence(
        &fixture,
        evidence_identifier
    );

    relation_record =
        test_relation_service_create_relation(
            relation_identifier,
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_B_IDENTIFIER,
            "uses"
        );

    identifiers =
        test_relation_service_create_identifier_array(
            evidence_identifier,
            NULL,
            NULL
        );

    relation_service_test_set_commit_failure(
        TRUE
    );

    assert(
        !relation_service_create(
            fixture.relation_service,
            relation_record,
            identifiers,
            &error
        )
    );

    test_relation_service_assert_error(
        error,
        RELATION_SERVICE_ERROR_TRANSACTION_COMMIT
    );

    assert(
        test_relation_service_count_relation(
            fixture.database,
            relation_identifier
        ) == 0
    );

    assert(
        test_relation_service_count_associations(
            fixture.database,
            relation_identifier
        ) == 0
    );

    assert(
        !database_transaction_is_active(
            fixture.database
        )
    );

    g_clear_error(
        &error
    );

    g_ptr_array_unref(
        identifiers
    );

    relation_record_free(
        relation_record
    );

    test_relation_service_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le signalement explicite d'un échec de rollback.
 */
static void test_relation_service_rollback_failure_reported(void)
{
    static const char *const relation_identifier =
        "40000000-0000-4000-8000-000000000014";

    static const char *const missing_evidence_identifier =
        "50000000-0000-4000-8000-00000000000b";

    TestRelationServiceFixture fixture =
        test_relation_service_fixture_create();

    RelationRecord *relation_record =
        NULL;

    GPtrArray *identifiers =
        NULL;

    GError *error =
        NULL;

    test_relation_service_insert_standard_entities(
        &fixture
    );

    relation_record =
        test_relation_service_create_relation(
            relation_identifier,
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_C_IDENTIFIER,
            "controls"
        );

    identifiers =
        test_relation_service_create_identifier_array(
            missing_evidence_identifier,
            NULL,
            NULL
        );

    relation_service_test_set_rollback_failure(
        TRUE
    );

    assert(
        !relation_service_create(
            fixture.relation_service,
            relation_record,
            identifiers,
            &error
        )
    );

    test_relation_service_assert_error(
        error,
        RELATION_SERVICE_ERROR_TRANSACTION_ROLLBACK
    );

    assert(
        strstr(
            error->message,
            "Cause initiale"
        ) != NULL
    );

    assert(
        strstr(
            error->message,
            "ROLLBACK"
        ) != NULL
    );

    /*
     * Le point d'injection signale l'échec, mais effectue réellement
     * l'annulation afin de préserver l'isolation du test.
     */
    assert(
        test_relation_service_count_relation(
            fixture.database,
            relation_identifier
        ) == 0
    );

    assert(
        !database_transaction_is_active(
            fixture.database
        )
    );

    g_clear_error(
        &error
    );

    g_ptr_array_unref(
        identifiers
    );

    relation_record_free(
        relation_record
    );

    test_relation_service_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie qu'un GError facultatif n'est pas obligatoire.
 */
static void test_relation_service_optional_error(void)
{
    static const char *const relation_identifier =
        "40000000-0000-4000-8000-000000000015";

    TestRelationServiceFixture fixture =
        test_relation_service_fixture_create();

    RelationRecord *relation_record =
        NULL;

    test_relation_service_insert_standard_entities(
        &fixture
    );

    relation_record =
        test_relation_service_create_relation(
            relation_identifier,
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_B_IDENTIFIER,
            "knows"
        );

    assert(
        relation_service_create(
            fixture.relation_service,
            relation_record,
            NULL,
            NULL
        )
    );

    assert(
        test_relation_service_count_relation(
            fixture.database,
            relation_identifier
        ) == 1
    );

    assert(
        !relation_service_create(
            NULL,
            relation_record,
            NULL,
            NULL
        )
    );

    relation_record_free(
        relation_record
    );

    test_relation_service_fixture_clear(
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

    test_relation_service_new_null_database();
    test_relation_service_lifecycle();
    test_relation_service_create_without_array();
    test_relation_service_create_with_empty_array();
    test_relation_service_create_with_one_evidence();
    test_relation_service_create_with_multiple_evidence();
    test_relation_service_invalid_arguments();
    test_relation_service_invalid_evidence_identifier();
    test_relation_service_duplicate_evidence_request();
    test_relation_service_duplicate_relation_rollback();
    test_relation_service_missing_second_evidence_rollback();
    test_relation_service_recovery_after_failure();
    test_relation_service_transaction_begin_failure();
    test_relation_service_commit_failure_rollback();
    test_relation_service_rollback_failure_reported();
    test_relation_service_optional_error();

    printf(
        "RelationService : tous les tests sont valides.\n"
    );

    return 0;
}
