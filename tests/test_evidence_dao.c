/******************************************************************************
 * @file test_evidence_dao.c
 * @brief Tests du DAO des preuves numériques.
 ******************************************************************************/

#include "dao/evidence_dao.h"

#include "database/database.h"
#include "database/statement.h"
#include "models/evidence_record.h"

#include <assert.h>
#include <stdio.h>

#include <glib.h>
#include <glib/gstdio.h>

/**
 * @brief Vérifie le refus d’une connexion absente.
 */
static void test_evidence_dao_new_null_database(void)
{
    EvidenceDao *evidence_dao = NULL;
    GError *error = NULL;

    evidence_dao = evidence_dao_new(
        NULL,
        &error
    );

    assert(evidence_dao == NULL);
    assert(error != NULL);

    assert(
        error->domain ==
        EVIDENCE_DAO_ERROR
    );

    assert(
        error->code ==
        (gint) EVIDENCE_DAO_ERROR_INVALID_ARGUMENT
    );

    assert(error->message != NULL);
    assert(error->message[0] != '\0');

    g_clear_error(
        &error
    );
}

/**
 * @brief Environnement temporaire d’un test EvidenceDao.
 */
typedef struct
{
    char *temporary_directory;
    char *database_path;

    Database *database;
    EvidenceDao *evidence_dao;
} TestEvidenceDaoFixture;

static TestEvidenceDaoFixture test_evidence_dao_fixture_create(void);

static void test_evidence_dao_fixture_clear(
    TestEvidenceDaoFixture *fixture
);

/**
 * @brief Retourne le nombre de preuves présentes dans la base de test.
 */
static int64_t test_evidence_dao_count_rows(
    Database *database
)
{
    DatabaseStatement *statement = NULL;
    int64_t evidence_count = -1;

    assert(database != NULL);

    statement =
        database_statement_prepare(
            database,
            "SELECT COUNT(*) FROM preuves;"
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
            &evidence_count
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

    return evidence_count;
}

/**
 * @brief Vérifie le refus d’un identifiant déjà utilisé.
 */
static void test_evidence_dao_insert_duplicate_identifier(void)
{
    TestEvidenceDaoFixture fixture =
        test_evidence_dao_fixture_create();

    EvidenceRecord *first_record = NULL;
    EvidenceRecord *second_record = NULL;

    GError *error = NULL;

    first_record =
        evidence_record_new(
            "55555555-5555-4555-8555-555555555555",
            "capture_1.png",
            "capture_001.png",
            "01_Preuves_Originales/Captures_Ecran/capture_001.png",
            "screenshot",
            100,
            "dddddddddddddddddddddddddddddddd"
            "dddddddddddddddddddddddddddddddd",
            "2026-07-18T18:00:00Z",
            NULL,
            NULL,
            NULL,
            EVIDENCE_INTEGRITY_STATUS_UNKNOWN,
            &error
        );

    assert(first_record != NULL);
    assert(error == NULL);

    second_record =
        evidence_record_new(
            "55555555-5555-4555-8555-555555555555",
            "capture_2.png",
            "capture_002.png",
            "01_Preuves_Originales/Captures_Ecran/capture_002.png",
            "screenshot",
            200,
            "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
            "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee",
            "2026-07-18T18:05:00Z",
            NULL,
            NULL,
            NULL,
            EVIDENCE_INTEGRITY_STATUS_UNKNOWN,
            &error
        );

    assert(second_record != NULL);
    assert(error == NULL);

    assert(
        evidence_dao_insert(
            fixture.evidence_dao,
            first_record,
            &error
        )
    );

    assert(error == NULL);

    assert(
        !evidence_dao_insert(
            fixture.evidence_dao,
            second_record,
            &error
        )
    );

    assert(error != NULL);
    assert(error->domain == EVIDENCE_DAO_ERROR);

    assert(
        error->code ==
        (gint) EVIDENCE_DAO_ERROR_CONSTRAINT
    );

    assert(
        strstr(
            error->message,
            "identifiant"
        ) != NULL
    );

    assert(
        test_evidence_dao_count_rows(
            fixture.database
        ) == 1
    );

    g_clear_error(
        &error
    );

    evidence_record_free(
        second_record
    );

    evidence_record_free(
        first_record
    );

    test_evidence_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d’un chemin relatif déjà utilisé.
 */
static void test_evidence_dao_insert_duplicate_relative_path(void)
{
    TestEvidenceDaoFixture fixture =
        test_evidence_dao_fixture_create();

    EvidenceRecord *first_record = NULL;
    EvidenceRecord *second_record = NULL;

    GError *error = NULL;

    const char *shared_relative_path =
        "01_Preuves_Originales/Documents/document_commun.pdf";

    first_record =
        evidence_record_new(
            "66666666-6666-4666-8666-666666666666",
            "document_1.pdf",
            "document_commun.pdf",
            shared_relative_path,
            "document",
            300,
            "ffffffffffffffffffffffffffffffff"
            "ffffffffffffffffffffffffffffffff",
            "2026-07-18T18:10:00Z",
            NULL,
            NULL,
            NULL,
            EVIDENCE_INTEGRITY_STATUS_UNKNOWN,
            &error
        );

    assert(first_record != NULL);
    assert(error == NULL);

    second_record =
        evidence_record_new(
            "77777777-7777-4777-8777-777777777777",
            "document_2.pdf",
            "document_commun.pdf",
            shared_relative_path,
            "document",
            400,
            "11111111111111111111111111111111"
            "11111111111111111111111111111111",
            "2026-07-18T18:15:00Z",
            NULL,
            NULL,
            NULL,
            EVIDENCE_INTEGRITY_STATUS_UNKNOWN,
            &error
        );

    assert(second_record != NULL);
    assert(error == NULL);

    assert(
        evidence_dao_insert(
            fixture.evidence_dao,
            first_record,
            &error
        )
    );

    assert(error == NULL);

    assert(
        !evidence_dao_insert(
            fixture.evidence_dao,
            second_record,
            &error
        )
    );

    assert(error != NULL);
    assert(error->domain == EVIDENCE_DAO_ERROR);

    assert(
        error->code ==
        (gint) EVIDENCE_DAO_ERROR_CONSTRAINT
    );

    assert(
        strstr(
            error->message,
            "chemin relatif"
        ) != NULL
    );

    assert(
        test_evidence_dao_count_rows(
            fixture.database
        ) == 1
    );

    g_clear_error(
        &error
    );

    evidence_record_free(
        second_record
    );

    evidence_record_free(
        first_record
    );

    test_evidence_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d’un type absent du catalogue SQLite.
 */
static void test_evidence_dao_insert_unknown_type(void)
{
    TestEvidenceDaoFixture fixture =
        test_evidence_dao_fixture_create();

    EvidenceRecord *evidence_record = NULL;

    GError *error = NULL;

    evidence_record =
        evidence_record_new(
            "88888888-8888-4888-8888-888888888888",
            "preuve.bin",
            "preuve_001.bin",
            "01_Preuves_Originales/Autres/preuve_001.bin",
            "type_inconnu",
            500,
            "22222222222222222222222222222222"
            "22222222222222222222222222222222",
            "2026-07-18T18:20:00Z",
            NULL,
            NULL,
            NULL,
            EVIDENCE_INTEGRITY_STATUS_UNKNOWN,
            &error
        );

    assert(evidence_record != NULL);
    assert(error == NULL);

    assert(
        !evidence_dao_insert(
            fixture.evidence_dao,
            evidence_record,
            &error
        )
    );

    assert(error != NULL);
    assert(error->domain == EVIDENCE_DAO_ERROR);

    assert(
        error->code ==
        (gint) EVIDENCE_DAO_ERROR_CONSTRAINT
    );

    assert(
        strstr(
            error->message,
            "type de preuve"
        ) != NULL
    );

    assert(
        test_evidence_dao_count_rows(
            fixture.database
        ) == 0
    );

    g_clear_error(
        &error
    );

    evidence_record_free(
        evidence_record
    );

    test_evidence_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Crée une base V2 temporaire et son DAO.
 */
static TestEvidenceDaoFixture test_evidence_dao_fixture_create(void)
{
    TestEvidenceDaoFixture fixture = {0};

    GError *error = NULL;

    fixture.temporary_directory =
        g_dir_make_tmp(
            "labfy-evidence-dao-test-XXXXXX",
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
            "Enquete_EvidenceDao",
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

    return fixture;
}

/**
 * @brief Crée une preuve destinée aux tests de statut d'intégrité.
 */
static EvidenceRecord *test_evidence_dao_create_integrity_record(
    const char *identifier,
    EvidenceIntegrityStatus integrity_status
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
            "preuve_integrite.bin",
            internal_name,
            relative_path,
            "document",
            128,
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            "2026-07-20T10:00:00Z",
            NULL,
            NULL,
            NULL,
            integrity_status,
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
 * @brief Détruit une fixture et ses fichiers temporaires.
 */
static void test_evidence_dao_fixture_clear(
    TestEvidenceDaoFixture *fixture
)
{
    assert(fixture != NULL);

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

    fixture->evidence_dao = NULL;
    fixture->database = NULL;
    fixture->database_path = NULL;
    fixture->temporary_directory = NULL;
}

/**
 * @brief Vérifie la création et la libération d’un DAO valide.
 */
static void test_evidence_dao_new_valid(void)
{
    Database *database = NULL;
    EvidenceDao *evidence_dao = NULL;
    GError *error = NULL;

    database = database_open(
        ":memory:"
    );

    assert(database != NULL);

    evidence_dao = evidence_dao_new(
        database,
        &error
    );

    assert(evidence_dao != NULL);
    assert(error == NULL);

    /*
     * La libération du DAO ne doit pas fermer Database.
     */
    evidence_dao_free(
        evidence_dao
    );

    database_close(
        database
    );
}

/**
 * @brief Vérifie que la destruction accepte NULL.
 */
static void test_evidence_dao_free_null(void)
{
    evidence_dao_free(
        NULL
    );
}

/**
 * @brief Vérifie l’insertion complète d’une preuve valide.
 */
static void test_evidence_dao_insert_valid_full(void)
{
    TestEvidenceDaoFixture fixture =
        test_evidence_dao_fixture_create();

    EvidenceRecord *evidence_record = NULL;
    DatabaseStatement *statement = NULL;

    char *identifier = NULL;
    char *original_name = NULL;
    char *internal_name = NULL;
    char *relative_path = NULL;
    char *type_identifier = NULL;
    char *sha256 = NULL;
    char *imported_at = NULL;
    char *collected_at = NULL;
    char *source = NULL;
    char *description = NULL;

    int64_t size_bytes = -1;
    int64_t integrity_status = -1;

    GError *error = NULL;

    evidence_record =
        evidence_record_new(
            "11111111-1111-4111-8111-111111111111",
            "Capture Instagram.png",
            "capture_instagram_001.png",
            "01_Preuves_Originales/Captures_Ecran/"
            "capture_instagram_001.png",
            "screenshot",
            4096,
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            "2026-07-18T16:00:00Z",
            "2026-07-17T20:30:00Z",
            "Compte Instagram communiqué par la victime.",
            "Capture originale de la conversation.",
            EVIDENCE_INTEGRITY_STATUS_VALID,
            &error
        );

    assert(evidence_record != NULL);
    assert(error == NULL);

    assert(
        evidence_dao_insert(
            fixture.evidence_dao,
            evidence_record,
            &error
        )
    );

    assert(error == NULL);

    statement =
        database_statement_prepare(
            fixture.database,
            "SELECT "
            "    preuves.id,"
            "    preuves.original_name,"
            "    preuves.name,"
            "    preuves.relative_path,"
            "    types_preuve.code,"
            "    preuves.size_bytes,"
            "    preuves.sha256,"
            "    preuves.imported_at,"
            "    preuves.collected_at,"
            "    preuves.source,"
            "    preuves.description,"
            "    preuves.integrity_status "
            "FROM preuves "
            "INNER JOIN types_preuve "
            "ON types_preuve.id = preuves.type_id "
            "WHERE preuves.id = ?;"
        );

    assert(statement != NULL);

    assert(
        database_statement_bind_text(
            statement,
            1,
            "11111111-1111-4111-8111-111111111111"
        )
    );

    assert(
        database_statement_step(
            statement
        ) == DATABASE_STATEMENT_STEP_ROW
    );

    assert(
        database_statement_column_text(
            statement,
            0,
            &identifier
        )
    );

    assert(
        database_statement_column_text(
            statement,
            1,
            &original_name
        )
    );

    assert(
        database_statement_column_text(
            statement,
            2,
            &internal_name
        )
    );

    assert(
        database_statement_column_text(
            statement,
            3,
            &relative_path
        )
    );

    assert(
        database_statement_column_text(
            statement,
            4,
            &type_identifier
        )
    );

    assert(
        database_statement_column_int64(
            statement,
            5,
            &size_bytes
        )
    );

    assert(
        database_statement_column_text(
            statement,
            6,
            &sha256
        )
    );

    assert(
        database_statement_column_text(
            statement,
            7,
            &imported_at
        )
    );

    assert(
        database_statement_column_text(
            statement,
            8,
            &collected_at
        )
    );

    assert(
        database_statement_column_text(
            statement,
            9,
            &source
        )
    );

    assert(
        database_statement_column_text(
            statement,
            10,
            &description
        )
    );

    assert(
        database_statement_column_int64(
            statement,
            11,
            &integrity_status
        )
    );

    /*
     * L’identifiant est unique : aucune seconde ligne ne doit exister.
     */
    assert(
        database_statement_step(
            statement
        ) == DATABASE_STATEMENT_STEP_DONE
    );

    assert(
        strcmp(
            identifier,
            "11111111-1111-4111-8111-111111111111"
        ) == 0
    );

    assert(
        strcmp(
            original_name,
            "Capture Instagram.png"
        ) == 0
    );

    assert(
        strcmp(
            internal_name,
            "capture_instagram_001.png"
        ) == 0
    );

    assert(
        strcmp(
            relative_path,
            "01_Preuves_Originales/Captures_Ecran/"
            "capture_instagram_001.png"
        ) == 0
    );

    assert(
        strcmp(
            type_identifier,
            "screenshot"
        ) == 0
    );

    assert(size_bytes == 4096);

    assert(
        strcmp(
            sha256,
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        ) == 0
    );

    assert(
        strcmp(
            imported_at,
            "2026-07-18T16:00:00Z"
        ) == 0
    );

    assert(
        strcmp(
            collected_at,
            "2026-07-17T20:30:00Z"
        ) == 0
    );

    assert(
        strcmp(
            source,
            "Compte Instagram communiqué par la victime."
        ) == 0
    );

    assert(
        strcmp(
            description,
            "Capture originale de la conversation."
        ) == 0
    );

    assert(
        integrity_status ==
        EVIDENCE_INTEGRITY_STATUS_VALID
    );

    database_statement_finalize(
        statement
    );

    g_free(description);
    g_free(source);
    g_free(collected_at);
    g_free(imported_at);
    g_free(sha256);
    g_free(type_identifier);
    g_free(relative_path);
    g_free(internal_name);
    g_free(original_name);
    g_free(identifier);

    evidence_record_free(
        evidence_record
    );

    test_evidence_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie l’insertion d’une preuve dont les champs facultatifs
 *        sont absents.
 */
static void test_evidence_dao_insert_optional_null(void)
{
    TestEvidenceDaoFixture fixture =
        test_evidence_dao_fixture_create();

    EvidenceRecord *evidence_record = NULL;
    DatabaseStatement *statement = NULL;

    bool collected_at_is_null = false;
    bool source_is_null = false;
    bool description_is_null = false;

    GError *error = NULL;

    evidence_record =
        evidence_record_new(
            "22222222-2222-4222-8222-222222222222",
            "document.pdf",
            "document_001.pdf",
            "01_Preuves_Originales/Documents/document_001.pdf",
            "document",
            2048,
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
            "2026-07-18T16:30:00Z",
            NULL,
            NULL,
            NULL,
            EVIDENCE_INTEGRITY_STATUS_UNKNOWN,
            &error
        );

    assert(evidence_record != NULL);
    assert(error == NULL);

    assert(
        evidence_dao_insert(
            fixture.evidence_dao,
            evidence_record,
            &error
        )
    );

    assert(error == NULL);

    statement =
        database_statement_prepare(
            fixture.database,
            "SELECT "
            "    collected_at,"
            "    source,"
            "    description "
            "FROM preuves "
            "WHERE id = ?;"
        );

    assert(statement != NULL);

    assert(
        database_statement_bind_text(
            statement,
            1,
            "22222222-2222-4222-8222-222222222222"
        )
    );

    assert(
        database_statement_step(
            statement
        ) == DATABASE_STATEMENT_STEP_ROW
    );

    assert(
        database_statement_column_is_null(
            statement,
            0,
            &collected_at_is_null
        )
    );

    assert(
        database_statement_column_is_null(
            statement,
            1,
            &source_is_null
        )
    );

    assert(
        database_statement_column_is_null(
            statement,
            2,
            &description_is_null
        )
    );

    assert(collected_at_is_null);
    assert(source_is_null);
    assert(description_is_null);

    assert(
        database_statement_step(
            statement
        ) == DATABASE_STATEMENT_STEP_DONE
    );

    database_statement_finalize(
        statement
    );

    evidence_record_free(
        evidence_record
    );

    test_evidence_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie que deux preuves distinctes peuvent partager le même hash.
 *
 * Un même fichier peut être présent à plusieurs emplacements ou provenir
 * de plusieurs collectes. Le SHA-256 ne doit donc pas être unique.
 */
static void test_evidence_dao_insert_duplicate_sha256(void)
{
    TestEvidenceDaoFixture fixture =
        test_evidence_dao_fixture_create();

    EvidenceRecord *first_record = NULL;
    EvidenceRecord *second_record = NULL;

    DatabaseStatement *statement = NULL;

    int64_t evidence_count = -1;

    GError *error = NULL;

    const char *shared_sha256 =
        "cccccccccccccccccccccccccccccccc"
        "cccccccccccccccccccccccccccccccc";

    first_record =
        evidence_record_new(
            "33333333-3333-4333-8333-333333333333",
            "capture_1.png",
            "capture_001.png",
            "01_Preuves_Originales/Captures_Ecran/capture_001.png",
            "screenshot",
            1024,
            shared_sha256,
            "2026-07-18T17:00:00Z",
            NULL,
            NULL,
            NULL,
            EVIDENCE_INTEGRITY_STATUS_UNKNOWN,
            &error
        );

    assert(first_record != NULL);
    assert(error == NULL);

    second_record =
        evidence_record_new(
            "44444444-4444-4444-8444-444444444444",
            "capture_2.png",
            "capture_002.png",
            "01_Preuves_Originales/Captures_Ecran/capture_002.png",
            "screenshot",
            1024,
            shared_sha256,
            "2026-07-18T17:05:00Z",
            NULL,
            NULL,
            NULL,
            EVIDENCE_INTEGRITY_STATUS_UNKNOWN,
            &error
        );

    assert(second_record != NULL);
    assert(error == NULL);

    assert(
        evidence_dao_insert(
            fixture.evidence_dao,
            first_record,
            &error
        )
    );

    assert(error == NULL);

    assert(
        evidence_dao_insert(
            fixture.evidence_dao,
            second_record,
            &error
        )
    );

    assert(error == NULL);

    statement =
        database_statement_prepare(
            fixture.database,
            "SELECT COUNT(*) "
            "FROM preuves "
            "WHERE sha256 = ?;"
        );

    assert(statement != NULL);

    assert(
        database_statement_bind_text(
            statement,
            1,
            shared_sha256
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
            &evidence_count
        )
    );

    assert(evidence_count == 2);

    assert(
        database_statement_step(
            statement
        ) == DATABASE_STATEMENT_STEP_DONE
    );

    database_statement_finalize(
        statement
    );

    evidence_record_free(
        second_record
    );

    evidence_record_free(
        first_record
    );

    test_evidence_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie la recherche d’une preuve existante.
 */
static void test_evidence_dao_find_existing(void)
{
    TestEvidenceDaoFixture fixture =
        test_evidence_dao_fixture_create();

    EvidenceRecord *inserted_record = NULL;
    EvidenceRecord *loaded_record = NULL;

    GError *error = NULL;

    inserted_record =
        evidence_record_new(
            "99999999-9999-4999-8999-999999999999",
            "conversation.png",
            "conversation_001.png",
            "01_Preuves_Originales/Captures_Ecran/"
            "conversation_001.png",
            "screenshot",
            8192,
            "33333333333333333333333333333333"
            "33333333333333333333333333333333",
            "2026-07-18T19:00:00Z",
            "2026-07-17T21:00:00Z",
            "Conversation Instagram.",
            "Capture conservée sans modification.",
            EVIDENCE_INTEGRITY_STATUS_VALID,
            &error
        );

    assert(inserted_record != NULL);
    assert(error == NULL);

    assert(
        evidence_dao_insert(
            fixture.evidence_dao,
            inserted_record,
            &error
        )
    );

    assert(error == NULL);

    loaded_record =
        evidence_dao_find_by_identifier(
            fixture.evidence_dao,
            "99999999-9999-4999-8999-999999999999",
            &error
        );

    assert(loaded_record != NULL);
    assert(error == NULL);

    assert(
        strcmp(
            evidence_record_get_identifier(
                loaded_record
            ),
            "99999999-9999-4999-8999-999999999999"
        ) == 0
    );

    assert(
        strcmp(
            evidence_record_get_original_name(
                loaded_record
            ),
            "conversation.png"
        ) == 0
    );

    assert(
        strcmp(
            evidence_record_get_type_identifier(
                loaded_record
            ),
            "screenshot"
        ) == 0
    );

    assert(
        evidence_record_get_size_bytes(
            loaded_record
        ) == 8192
    );

    assert(
        evidence_record_get_integrity_status(
            loaded_record
        ) == EVIDENCE_INTEGRITY_STATUS_VALID
    );

    evidence_record_free(
        loaded_record
    );

    evidence_record_free(
        inserted_record
    );

    test_evidence_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie qu’une preuve absente retourne NULL sans erreur.
 */
static void test_evidence_dao_find_missing(void)
{
    TestEvidenceDaoFixture fixture =
        test_evidence_dao_fixture_create();

    EvidenceRecord *evidence_record = NULL;
    GError *error = NULL;

    evidence_record =
        evidence_dao_find_by_identifier(
            fixture.evidence_dao,
            "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa",
            &error
        );

    assert(evidence_record == NULL);
    assert(error == NULL);

    test_evidence_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d’un identifiant de recherche invalide.
 */
static void test_evidence_dao_find_invalid_identifier(void)
{
    TestEvidenceDaoFixture fixture =
        test_evidence_dao_fixture_create();

    EvidenceRecord *evidence_record = NULL;
    GError *error = NULL;

    evidence_record =
        evidence_dao_find_by_identifier(
            fixture.evidence_dao,
            "identifiant-invalide",
            &error
        );

    assert(evidence_record == NULL);
    assert(error != NULL);
    assert(error->domain == EVIDENCE_DAO_ERROR);

    assert(
        error->code ==
        (gint) EVIDENCE_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    test_evidence_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le comptage d’une table de preuves vide.
 */
static void test_evidence_dao_count_empty(void)
{
    TestEvidenceDaoFixture fixture =
        test_evidence_dao_fixture_create();

    guint64 evidence_count = 42;

    GError *error = NULL;

    assert(
        evidence_dao_count(
            fixture.evidence_dao,
            &evidence_count,
            &error
        )
    );

    assert(error == NULL);
    assert(evidence_count == 0);

    test_evidence_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le comptage de plusieurs preuves.
 */
static void test_evidence_dao_count_records(void)
{
    TestEvidenceDaoFixture fixture =
        test_evidence_dao_fixture_create();

    EvidenceRecord *first_record = NULL;
    EvidenceRecord *second_record = NULL;

    guint64 evidence_count = 0;

    GError *error = NULL;

    first_record =
        evidence_record_new(
            "aaaaaaaa-1111-4111-8111-111111111111",
            "preuve_1.png",
            "preuve_001.png",
            "01_Preuves_Originales/Captures_Ecran/preuve_001.png",
            "screenshot",
            1000,
            "44444444444444444444444444444444"
            "44444444444444444444444444444444",
            "2026-07-18T19:30:00Z",
            NULL,
            NULL,
            NULL,
            EVIDENCE_INTEGRITY_STATUS_UNKNOWN,
            &error
        );

    assert(first_record != NULL);
    assert(error == NULL);

    second_record =
        evidence_record_new(
            "bbbbbbbb-2222-4222-8222-222222222222",
            "preuve_2.pdf",
            "preuve_002.pdf",
            "01_Preuves_Originales/Documents/preuve_002.pdf",
            "document",
            2000,
            "55555555555555555555555555555555"
            "55555555555555555555555555555555",
            "2026-07-18T19:35:00Z",
            NULL,
            NULL,
            NULL,
            EVIDENCE_INTEGRITY_STATUS_UNKNOWN,
            &error
        );

    assert(second_record != NULL);
    assert(error == NULL);

    assert(
        evidence_dao_insert(
            fixture.evidence_dao,
            first_record,
            &error
        )
    );

    assert(error == NULL);

    assert(
        evidence_dao_insert(
            fixture.evidence_dao,
            second_record,
            &error
        )
    );

    assert(error == NULL);

    assert(
        evidence_dao_count(
            fixture.evidence_dao,
            &evidence_count,
            &error
        )
    );

    assert(error == NULL);
    assert(evidence_count == 2);

    evidence_record_free(
        second_record
    );

    evidence_record_free(
        first_record
    );

    test_evidence_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus des paramètres de comptage invalides.
 */
static void test_evidence_dao_count_invalid_arguments(void)
{
    TestEvidenceDaoFixture fixture =
        test_evidence_dao_fixture_create();

    guint64 evidence_count = 123;

    GError *error = NULL;

    assert(
        !evidence_dao_count(
            NULL,
            &evidence_count,
            &error
        )
    );

    assert(error != NULL);
    assert(error->domain == EVIDENCE_DAO_ERROR);

    assert(
        error->code ==
        (gint) EVIDENCE_DAO_ERROR_INVALID_ARGUMENT
    );

    /*
     * Le DAO absent empêche la fonction d’accéder à la destination.
     */
    assert(evidence_count == 123);

    g_clear_error(
        &error
    );

    assert(
        !evidence_dao_count(
            fixture.evidence_dao,
            NULL,
            &error
        )
    );

    assert(error != NULL);

    assert(
        error->code ==
        (gint) EVIDENCE_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    test_evidence_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le chargement d’une liste vide.
 */
static void test_evidence_dao_list_empty(void)
{
    TestEvidenceDaoFixture fixture =
        test_evidence_dao_fixture_create();

    GPtrArray *evidence_records = NULL;
    GError *error = NULL;

    evidence_records =
        evidence_dao_list_all(
            fixture.evidence_dao,
            &error
        );

    assert(evidence_records != NULL);
    assert(error == NULL);
    assert(evidence_records->len == 0);

    g_ptr_array_unref(
        evidence_records
    );

    test_evidence_dao_fixture_clear(
        &fixture
    );
}
/**
 * @brief Vérifie le chargement de plusieurs preuves dans un ordre stable.
 */
static void test_evidence_dao_list_order(void)
{
    TestEvidenceDaoFixture fixture =
        test_evidence_dao_fixture_create();

    EvidenceRecord *first_inserted_record = NULL;
    EvidenceRecord *second_inserted_record = NULL;
    EvidenceRecord *third_inserted_record = NULL;

    const EvidenceRecord *first_loaded_record = NULL;
    const EvidenceRecord *second_loaded_record = NULL;
    const EvidenceRecord *third_loaded_record = NULL;

    GPtrArray *evidence_records = NULL;
    GError *error = NULL;

    /*
     * Cette preuve est insérée en premier, mais sa date est la plus tardive.
     */
    third_inserted_record =
        evidence_record_new(
            "30000000-0000-4000-8000-000000000003",
            "preuve_tardive.pdf",
            "preuve_tardive.pdf",
            "01_Preuves_Originales/Documents/preuve_tardive.pdf",
            "document",
            3000,
            "66666666666666666666666666666666"
            "66666666666666666666666666666666",
            "2026-07-18T21:00:00Z",
            NULL,
            NULL,
            NULL,
            EVIDENCE_INTEGRITY_STATUS_UNKNOWN,
            &error
        );

    assert(third_inserted_record != NULL);
    assert(error == NULL);

    second_inserted_record =
        evidence_record_new(
            "20000000-0000-4000-8000-000000000002",
            "preuve_b.png",
            "preuve_b.png",
            "01_Preuves_Originales/Captures_Ecran/preuve_b.png",
            "screenshot",
            2000,
            "77777777777777777777777777777777"
            "77777777777777777777777777777777",
            "2026-07-18T20:00:00Z",
            NULL,
            NULL,
            NULL,
            EVIDENCE_INTEGRITY_STATUS_UNKNOWN,
            &error
        );

    assert(second_inserted_record != NULL);
    assert(error == NULL);

    first_inserted_record =
        evidence_record_new(
            "10000000-0000-4000-8000-000000000001",
            "preuve_a.png",
            "preuve_a.png",
            "01_Preuves_Originales/Captures_Ecran/preuve_a.png",
            "screenshot",
            1000,
            "88888888888888888888888888888888"
            "88888888888888888888888888888888",
            "2026-07-18T20:00:00Z",
            NULL,
            NULL,
            NULL,
            EVIDENCE_INTEGRITY_STATUS_UNKNOWN,
            &error
        );

    assert(first_inserted_record != NULL);
    assert(error == NULL);

    assert(
        evidence_dao_insert(
            fixture.evidence_dao,
            third_inserted_record,
            &error
        )
    );

    assert(error == NULL);

    assert(
        evidence_dao_insert(
            fixture.evidence_dao,
            second_inserted_record,
            &error
        )
    );

    assert(error == NULL);

    assert(
        evidence_dao_insert(
            fixture.evidence_dao,
            first_inserted_record,
            &error
        )
    );

    assert(error == NULL);

    evidence_records =
        evidence_dao_list_all(
            fixture.evidence_dao,
            &error
        );

    assert(evidence_records != NULL);
    assert(error == NULL);
    assert(evidence_records->len == 3);

    first_loaded_record =
        g_ptr_array_index(
            evidence_records,
            0
        );

    second_loaded_record =
        g_ptr_array_index(
            evidence_records,
            1
        );

    third_loaded_record =
        g_ptr_array_index(
            evidence_records,
            2
        );

    assert(first_loaded_record != NULL);
    assert(second_loaded_record != NULL);
    assert(third_loaded_record != NULL);

    /*
     * Les deux premières preuves partagent la même date.
     * Leur identifiant sert donc de second critère de tri.
     */
    assert(
        strcmp(
            evidence_record_get_identifier(
                first_loaded_record
            ),
            "10000000-0000-4000-8000-000000000001"
        ) == 0
    );

    assert(
        strcmp(
            evidence_record_get_identifier(
                second_loaded_record
            ),
            "20000000-0000-4000-8000-000000000002"
        ) == 0
    );

    assert(
        strcmp(
            evidence_record_get_identifier(
                third_loaded_record
            ),
            "30000000-0000-4000-8000-000000000003"
        ) == 0
    );

    /*
     * Le tableau libère lui-même les trois modèles chargés.
     */
    g_ptr_array_unref(
        evidence_records
    );

    evidence_record_free(
        first_inserted_record
    );

    evidence_record_free(
        second_inserted_record
    );

    evidence_record_free(
        third_inserted_record
    );

    test_evidence_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d’un DAO absent.
 */
static void test_evidence_dao_list_invalid_arguments(void)
{
    GPtrArray *evidence_records = NULL;
    GError *error = NULL;

    evidence_records =
        evidence_dao_list_all(
            NULL,
            &error
        );

    assert(evidence_records == NULL);
    assert(error != NULL);
    assert(error->domain == EVIDENCE_DAO_ERROR);

    assert(
        error->code ==
        (gint) EVIDENCE_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );
}

/**
 * @brief Vérifie la mise à jour d'une preuve vers VALID.
 */
static void test_evidence_dao_update_integrity_valid(void)
{
    TestEvidenceDaoFixture fixture =
        test_evidence_dao_fixture_create();

    EvidenceRecord *inserted_record =
        NULL;

    EvidenceRecord *loaded_record =
        NULL;

    GError *error =
        NULL;

    const char *identifier =
        "12345678-1111-4111-8111-111111111111";

    inserted_record =
        test_evidence_dao_create_integrity_record(
            identifier,
            EVIDENCE_INTEGRITY_STATUS_UNKNOWN
        );

    assert(
        evidence_dao_insert(
            fixture.evidence_dao,
            inserted_record,
            &error
        )
    );

    assert(error == NULL);

    assert(
        evidence_dao_update_integrity_status(
            fixture.evidence_dao,
            identifier,
            EVIDENCE_INTEGRITY_STATUS_VALID,
            &error
        )
    );

    assert(error == NULL);

    loaded_record =
        evidence_dao_find_by_identifier(
            fixture.evidence_dao,
            identifier,
            &error
        );

    assert(loaded_record != NULL);
    assert(error == NULL);

    assert(
        evidence_record_get_integrity_status(
            loaded_record
        ) == EVIDENCE_INTEGRITY_STATUS_VALID
    );

    evidence_record_free(
        loaded_record
    );

    evidence_record_free(
        inserted_record
    );

    test_evidence_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie la mise à jour d'une preuve vers MODIFIED.
 */
static void test_evidence_dao_update_integrity_modified(void)
{
    TestEvidenceDaoFixture fixture =
        test_evidence_dao_fixture_create();

    EvidenceRecord *inserted_record =
        NULL;

    EvidenceRecord *loaded_record =
        NULL;

    GError *error =
        NULL;

    const char *identifier =
        "12345678-2222-4222-8222-222222222222";

    inserted_record =
        test_evidence_dao_create_integrity_record(
            identifier,
            EVIDENCE_INTEGRITY_STATUS_VALID
        );

    assert(
        evidence_dao_insert(
            fixture.evidence_dao,
            inserted_record,
            &error
        )
    );

    assert(error == NULL);

    assert(
        evidence_dao_update_integrity_status(
            fixture.evidence_dao,
            identifier,
            EVIDENCE_INTEGRITY_STATUS_MODIFIED,
            &error
        )
    );

    assert(error == NULL);

    loaded_record =
        evidence_dao_find_by_identifier(
            fixture.evidence_dao,
            identifier,
            &error
        );

    assert(loaded_record != NULL);
    assert(error == NULL);

    assert(
        evidence_record_get_integrity_status(
            loaded_record
        ) == EVIDENCE_INTEGRITY_STATUS_MODIFIED
    );

    evidence_record_free(
        loaded_record
    );

    evidence_record_free(
        inserted_record
    );

    test_evidence_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le signalement d'une preuve inexistante.
 */
static void test_evidence_dao_update_integrity_missing(void)
{
    TestEvidenceDaoFixture fixture =
        test_evidence_dao_fixture_create();

    GError *error =
        NULL;

    assert(
        !evidence_dao_update_integrity_status(
            fixture.evidence_dao,
            "12345678-3333-4333-8333-333333333333",
            EVIDENCE_INTEGRITY_STATUS_VALID,
            &error
        )
    );

    assert(error != NULL);
    assert(error->domain == EVIDENCE_DAO_ERROR);

    assert(
        error->code ==
        (gint) EVIDENCE_DAO_ERROR_NOT_FOUND
    );

    g_clear_error(
        &error
    );

    test_evidence_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d'un statut hors de l'énumération.
 */
static void test_evidence_dao_update_integrity_invalid_status(void)
{
    TestEvidenceDaoFixture fixture =
        test_evidence_dao_fixture_create();

    GError *error =
        NULL;

    assert(
        !evidence_dao_update_integrity_status(
            fixture.evidence_dao,
            "12345678-4444-4444-8444-444444444444",
            (EvidenceIntegrityStatus)
                (EVIDENCE_INTEGRITY_STATUS_ERROR + 1),
            &error
        )
    );

    assert(error != NULL);
    assert(error->domain == EVIDENCE_DAO_ERROR);

    assert(
        error->code ==
        (gint) EVIDENCE_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    test_evidence_dao_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus des arguments invalides.
 */
static void test_evidence_dao_update_integrity_invalid_arguments(void)
{
    TestEvidenceDaoFixture fixture =
        test_evidence_dao_fixture_create();

    GError *error =
        NULL;

    assert(
        !evidence_dao_update_integrity_status(
            NULL,
            "12345678-5555-4555-8555-555555555555",
            EVIDENCE_INTEGRITY_STATUS_VALID,
            &error
        )
    );

    assert(error != NULL);

    assert(
        error->code ==
        (gint) EVIDENCE_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    assert(
        !evidence_dao_update_integrity_status(
            fixture.evidence_dao,
            NULL,
            EVIDENCE_INTEGRITY_STATUS_VALID,
            &error
        )
    );

    assert(error != NULL);

    assert(
        error->code ==
        (gint) EVIDENCE_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    assert(
        !evidence_dao_update_integrity_status(
            fixture.evidence_dao,
            "identifiant-invalide",
            EVIDENCE_INTEGRITY_STATUS_VALID,
            &error
        )
    );

    assert(error != NULL);

    assert(
        error->code ==
        (gint) EVIDENCE_DAO_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    test_evidence_dao_fixture_clear(
        &fixture
    );
}

int main(void)
{
    test_evidence_dao_new_null_database();
    test_evidence_dao_new_valid();
    test_evidence_dao_free_null();
    test_evidence_dao_insert_valid_full();
    test_evidence_dao_insert_optional_null();
    test_evidence_dao_insert_duplicate_sha256();
    test_evidence_dao_insert_duplicate_identifier();
    test_evidence_dao_insert_duplicate_relative_path();
    test_evidence_dao_insert_unknown_type();
    test_evidence_dao_find_existing();
    test_evidence_dao_find_missing();
    test_evidence_dao_find_invalid_identifier();
    test_evidence_dao_count_empty();
    test_evidence_dao_count_records();
    test_evidence_dao_count_invalid_arguments();
    test_evidence_dao_list_empty();
    test_evidence_dao_list_order();
    test_evidence_dao_list_invalid_arguments();
    test_evidence_dao_update_integrity_valid();
    test_evidence_dao_update_integrity_modified();
    test_evidence_dao_update_integrity_missing();
    test_evidence_dao_update_integrity_invalid_status();
    test_evidence_dao_update_integrity_invalid_arguments();

    printf(
        "EvidenceDao : tests de construction valides.\n"
    );

    return 0;
}
