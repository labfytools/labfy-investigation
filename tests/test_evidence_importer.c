/******************************************************************************
 * @file test_evidence_importer.c
 * @brief Tests de l’import transactionnel des preuves.
 ******************************************************************************/

#include "core/evidence_importer.h"

#include "dao/evidence_dao.h"
#include "database/database.h"
#include "database/transaction.h"
#include "models/evidence_record.h"
#include "core/evidence_importer_test.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

/**
 * @brief Dossier relatif utilisé par les tests.
 */
#define TEST_EVIDENCE_RELATIVE_DIRECTORY \
    "01_Preuves_Originales/Documents"

/**
 * @brief Environnement temporaire d’un test EvidenceImporter.
 */
typedef struct
{
    char *root_directory;
    char *database_path;

    char *source_directory;
    char *evidence_directory;
    char *destination_directory;

    Database *database;
    EvidenceDao *evidence_dao;
    EvidenceImporter *evidence_importer;
} TestEvidenceImporterFixture;

/**
 * @brief Vérifie grossièrement le format UTC strict du modèle.
 */
static void test_evidence_importer_assert_utc_timestamp(
    const char *timestamp
)
{
    assert(timestamp != NULL);

    assert(
        strlen(
            timestamp
        ) == 20
    );

    assert(timestamp[4] == '-');
    assert(timestamp[7] == '-');
    assert(timestamp[10] == 'T');
    assert(timestamp[13] == ':');
    assert(timestamp[16] == ':');
    assert(timestamp[19] == 'Z');
}

/**
 * @brief Crée une enquête temporaire complète.
 */
static TestEvidenceImporterFixture
test_evidence_importer_fixture_create(void)
{
    TestEvidenceImporterFixture fixture = {0};

    GError *error = NULL;

    fixture.root_directory =
        g_dir_make_tmp(
            "labfy-evidence-importer-test-XXXXXX",
            &error
        );

    assert(fixture.root_directory != NULL);
    assert(error == NULL);

    fixture.database_path =
        g_build_filename(
            fixture.root_directory,
            "Enquete.sqlite",
            NULL
        );

    fixture.source_directory =
        g_build_filename(
            fixture.root_directory,
            "sources",
            NULL
        );

    fixture.evidence_directory =
        g_build_filename(
            fixture.root_directory,
            "01_Preuves_Originales",
            NULL
        );

    fixture.destination_directory =
        g_build_filename(
            fixture.evidence_directory,
            "Documents",
            NULL
        );

    assert(fixture.database_path != NULL);
    assert(fixture.source_directory != NULL);
    assert(fixture.evidence_directory != NULL);
    assert(fixture.destination_directory != NULL);

    assert(
        g_mkdir(
            fixture.source_directory,
            0700
        ) == 0
    );

    assert(
        g_mkdir(
            fixture.evidence_directory,
            0700
        ) == 0
    );

    assert(
        g_mkdir(
            fixture.destination_directory,
            0700
        ) == 0
    );

    assert(
        database_initialize(
            fixture.database_path,
            "Enquete_EvidenceImporter",
            fixture.root_directory
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

    fixture.evidence_importer =
        evidence_importer_new(
            fixture.database,
            &error
        );

    assert(fixture.evidence_importer != NULL);
    assert(error == NULL);

    return fixture;
}

/**
 * @brief Détruit une fixture dont les preuves ont déjà été supprimées.
 */
static void test_evidence_importer_fixture_clear(
    TestEvidenceImporterFixture *fixture
)
{
    assert(fixture != NULL);

    evidence_importer_free(
        fixture->evidence_importer
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
            fixture->destination_directory
        ) == 0
    );

    assert(
        g_rmdir(
            fixture->evidence_directory
        ) == 0
    );

    assert(
        g_rmdir(
            fixture->source_directory
        ) == 0
    );

    assert(
        g_rmdir(
            fixture->root_directory
        ) == 0
    );

    g_free(
        fixture->destination_directory
    );

    g_free(
        fixture->evidence_directory
    );

    g_free(
        fixture->source_directory
    );

    g_free(
        fixture->database_path
    );

    g_free(
        fixture->root_directory
    );

    fixture->evidence_importer = NULL;
    fixture->evidence_dao = NULL;
    fixture->database = NULL;

    fixture->destination_directory = NULL;
    fixture->evidence_directory = NULL;
    fixture->source_directory = NULL;
    fixture->database_path = NULL;
    fixture->root_directory = NULL;
}

/**
 * @brief Vérifie le refus d’une connexion absente.
 */
static void test_evidence_importer_new_null_database(void)
{
    EvidenceImporter *evidence_importer = NULL;

    GError *error = NULL;

    evidence_importer =
        evidence_importer_new(
            NULL,
            &error
        );

    assert(evidence_importer == NULL);
    assert(error != NULL);

    assert(
        error->domain ==
        EVIDENCE_IMPORTER_ERROR
    );

    assert(
        error->code ==
        (gint) EVIDENCE_IMPORTER_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    evidence_importer_free(
        NULL
    );
}

/**
 * @brief Vérifie la création et la libération d’un importeur valide.
 */
static void test_evidence_importer_new_valid(void)
{
    TestEvidenceImporterFixture fixture =
        test_evidence_importer_fixture_create();

    /*
     * La création de la fixture prouve que l’importeur et le DAO
     * peuvent emprunter simultanément la même connexion.
     */
    assert(fixture.evidence_importer != NULL);
    assert(fixture.evidence_dao != NULL);
    assert(fixture.database != NULL);

    assert(
        !database_transaction_is_active(
            fixture.database
        )
    );

    test_evidence_importer_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie un import complet : fichier, modèle et SQLite.
 */
static void test_evidence_importer_import_valid_text_file(void)
{
    TestEvidenceImporterFixture fixture =
        test_evidence_importer_fixture_create();

    EvidenceImportRequest request = {0};

    EvidenceRecord *evidence_record = NULL;
    EvidenceRecord *loaded_record = NULL;

    char *source_path = NULL;
    char *destination_path = NULL;
    char *expected_relative_path = NULL;

    char *source_contents = NULL;
    char *destination_contents = NULL;

    const char *identifier = NULL;
    const char *internal_name = NULL;

    gsize source_contents_size = 0;
    gsize destination_contents_size = 0;

    guint64 evidence_count = 0;

    GError *error = NULL;

    source_path =
        g_build_filename(
            fixture.source_directory,
            "Preuve Originale.TXT",
            NULL
        );

    assert(source_path != NULL);

    assert(
        g_file_set_contents(
            source_path,
            "abc",
            3,
            &error
        )
    );

    assert(error == NULL);

    request.source_path =
        source_path;

    request.destination_directory =
        fixture.destination_directory;

    request.relative_directory =
        TEST_EVIDENCE_RELATIVE_DIRECTORY;

    request.type_identifier =
        "document";

    request.collected_at =
        "2026-07-18T20:30:00Z";

    request.source =
        "Fichier communiqué directement par la victime.";

    request.description =
        "Première preuve importée par le service transactionnel.";

    evidence_record =
        evidence_importer_import(
            fixture.evidence_importer,
            &request,
            NULL,
            &error
        );

    assert(error == NULL);
    assert(evidence_record != NULL);

    /*
     * Le commit doit être terminé avant le retour.
     */
    assert(
        !database_transaction_is_active(
            fixture.database
        )
    );

    identifier =
        evidence_record_get_identifier(
            evidence_record
        );

    internal_name =
        evidence_record_get_internal_name(
            evidence_record
        );

    assert(identifier != NULL);
    assert(internal_name != NULL);

    assert(
        g_uuid_string_is_valid(
            identifier
        )
    );

    assert(
        strcmp(
            evidence_record_get_original_name(
                evidence_record
            ),
            "Preuve Originale.TXT"
        ) == 0
    );

    /*
     * L’extension est normalisée en minuscules.
     */
    assert(
        g_str_has_prefix(
            internal_name,
            identifier
        )
    );

    assert(
        g_str_has_suffix(
            internal_name,
            ".txt"
        )
    );

    destination_path =
        g_build_filename(
            fixture.destination_directory,
            internal_name,
            NULL
        );

    expected_relative_path =
        g_build_filename(
            TEST_EVIDENCE_RELATIVE_DIRECTORY,
            internal_name,
            NULL
        );

    assert(destination_path != NULL);
    assert(expected_relative_path != NULL);

    assert(
        strcmp(
            evidence_record_get_relative_path(
                evidence_record
            ),
            expected_relative_path
        ) == 0
    );

    assert(
        strcmp(
            evidence_record_get_type_identifier(
                evidence_record
            ),
            "document"
        ) == 0
    );

    assert(
        evidence_record_get_size_bytes(
            evidence_record
        ) == 3
    );

    assert(
        strcmp(
            evidence_record_get_sha256(
                evidence_record
            ),
            "ba7816bf8f01cfea414140de5dae2223"
            "b00361a396177a9cb410ff61f20015ad"
        ) == 0
    );

    test_evidence_importer_assert_utc_timestamp(
        evidence_record_get_imported_at(
            evidence_record
        )
    );

    assert(
        strcmp(
            evidence_record_get_collected_at(
                evidence_record
            ),
            request.collected_at
        ) == 0
    );

    assert(
        strcmp(
            evidence_record_get_source(
                evidence_record
            ),
            request.source
        ) == 0
    );

    assert(
        strcmp(
            evidence_record_get_description(
                evidence_record
            ),
            request.description
        ) == 0
    );

    assert(
        evidence_record_get_integrity_status(
            evidence_record
        ) ==
        EVIDENCE_INTEGRITY_STATUS_VALID
    );

    /*
     * La copie validée doit exister après le commit.
     */
    assert(
        g_file_test(
            destination_path,
            G_FILE_TEST_IS_REGULAR
        )
    );

    assert(
        g_file_get_contents(
            source_path,
            &source_contents,
            &source_contents_size,
            &error
        )
    );

    assert(error == NULL);

    assert(
        g_file_get_contents(
            destination_path,
            &destination_contents,
            &destination_contents_size,
            &error
        )
    );

    assert(error == NULL);

    assert(source_contents_size == 3);
    assert(destination_contents_size == 3);

    assert(
        memcmp(
            source_contents,
            "abc",
            3
        ) == 0
    );

    assert(
        memcmp(
            destination_contents,
            source_contents,
            3
        ) == 0
    );

    /*
     * La ligne SQLite doit exister exactement une fois.
     */
    assert(
        evidence_dao_count(
            fixture.evidence_dao,
            &evidence_count,
            &error
        )
    );

    assert(error == NULL);
    assert(evidence_count == 1);

    loaded_record =
        evidence_dao_find_by_identifier(
            fixture.evidence_dao,
            identifier,
            &error
        );

    assert(error == NULL);
    assert(loaded_record != NULL);

    assert(
        strcmp(
            evidence_record_get_identifier(
                loaded_record
            ),
            identifier
        ) == 0
    );

    assert(
        strcmp(
            evidence_record_get_original_name(
                loaded_record
            ),
            evidence_record_get_original_name(
                evidence_record
            )
        ) == 0
    );

    assert(
        strcmp(
            evidence_record_get_internal_name(
                loaded_record
            ),
            internal_name
        ) == 0
    );

    assert(
        strcmp(
            evidence_record_get_relative_path(
                loaded_record
            ),
            expected_relative_path
        ) == 0
    );

    assert(
        evidence_record_get_size_bytes(
            loaded_record
        ) ==
        evidence_record_get_size_bytes(
            evidence_record
        )
    );

    assert(
        strcmp(
            evidence_record_get_sha256(
                loaded_record
            ),
            evidence_record_get_sha256(
                evidence_record
            )
        ) == 0
    );

    assert(
        strcmp(
            evidence_record_get_imported_at(
                loaded_record
            ),
            evidence_record_get_imported_at(
                evidence_record
            )
        ) == 0
    );

    assert(
        evidence_record_get_integrity_status(
            loaded_record
        ) ==
        EVIDENCE_INTEGRITY_STATUS_VALID
    );

    evidence_record_free(
        loaded_record
    );

    evidence_record_free(
        evidence_record
    );

    g_free(
        destination_contents
    );

    g_free(
        source_contents
    );

    assert(
        g_remove(
            destination_path
        ) == 0
    );

    assert(
        g_remove(
            source_path
        ) == 0
    );

    g_free(
        expected_relative_path
    );

    g_free(
        destination_path
    );

    g_free(
        source_path
    );

    test_evidence_importer_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie l’import d’un fichier vide sans métadonnées facultatives.
 */
static void test_evidence_importer_import_empty_optional_null(void)
{
    TestEvidenceImporterFixture fixture =
        test_evidence_importer_fixture_create();

    EvidenceImportRequest request = {0};

    EvidenceRecord *evidence_record = NULL;
    EvidenceRecord *loaded_record = NULL;

    char *source_path = NULL;
    char *destination_path = NULL;

    const char *identifier = NULL;
    const char *internal_name = NULL;

    guint64 evidence_count = 0;

    GError *error = NULL;

    source_path =
        g_build_filename(
            fixture.source_directory,
            "preuve_vide",
            NULL
        );

    assert(source_path != NULL);

    assert(
        g_file_set_contents(
            source_path,
            "",
            0,
            &error
        )
    );

    assert(error == NULL);

    request.source_path =
        source_path;

    request.destination_directory =
        fixture.destination_directory;

    request.relative_directory =
        TEST_EVIDENCE_RELATIVE_DIRECTORY;

    request.type_identifier =
        "document";

    request.collected_at = NULL;
    request.source = NULL;
    request.description = NULL;

    evidence_record =
        evidence_importer_import(
            fixture.evidence_importer,
            &request,
            NULL,
            &error
        );

    assert(error == NULL);
    assert(evidence_record != NULL);

    identifier =
        evidence_record_get_identifier(
            evidence_record
        );

    internal_name =
        evidence_record_get_internal_name(
            evidence_record
        );

    assert(identifier != NULL);
    assert(internal_name != NULL);

    /*
     * Le fichier original ne possède pas d’extension :
     * le nom interne doit être exactement l’UUID.
     */
    assert(
        strcmp(
            internal_name,
            identifier
        ) == 0
    );

    assert(
        evidence_record_get_size_bytes(
            evidence_record
        ) == 0
    );

    assert(
        strcmp(
            evidence_record_get_sha256(
                evidence_record
            ),
            "e3b0c44298fc1c149afbf4c8996fb924"
            "27ae41e4649b934ca495991b7852b855"
        ) == 0
    );

    assert(
        evidence_record_get_collected_at(
            evidence_record
        ) == NULL
    );

    assert(
        evidence_record_get_source(
            evidence_record
        ) == NULL
    );

    assert(
        evidence_record_get_description(
            evidence_record
        ) == NULL
    );

    destination_path =
        g_build_filename(
            fixture.destination_directory,
            internal_name,
            NULL
        );

    assert(destination_path != NULL);

    assert(
        g_file_test(
            destination_path,
            G_FILE_TEST_IS_REGULAR
        )
    );

    assert(
        evidence_dao_count(
            fixture.evidence_dao,
            &evidence_count,
            &error
        )
    );

    assert(error == NULL);
    assert(evidence_count == 1);

    loaded_record =
        evidence_dao_find_by_identifier(
            fixture.evidence_dao,
            identifier,
            &error
        );

    assert(error == NULL);
    assert(loaded_record != NULL);

    assert(
        evidence_record_get_collected_at(
            loaded_record
        ) == NULL
    );

    assert(
        evidence_record_get_source(
            loaded_record
        ) == NULL
    );

    assert(
        evidence_record_get_description(
            loaded_record
        ) == NULL
    );

    evidence_record_free(
        loaded_record
    );

    evidence_record_free(
        evidence_record
    );

    assert(
        g_remove(
            destination_path
        ) == 0
    );

    assert(
        g_remove(
            source_path
        ) == 0
    );

    g_free(
        destination_path
    );

    g_free(
        source_path
    );

    test_evidence_importer_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie l’import exact d’un fichier binaire.
 */
static void test_evidence_importer_import_binary_file(void)
{
    TestEvidenceImporterFixture fixture =
        test_evidence_importer_fixture_create();

    static const guint8 binary_data[] =
    {
        0x00,
        0xFF,
        0x80,
        0x41,
        0x00,
        0x7F,
        0x10
    };

    EvidenceImportRequest request = {0};

    EvidenceRecord *evidence_record = NULL;

    char *source_path = NULL;
    char *destination_path = NULL;
    char *destination_contents = NULL;

    const char *internal_name = NULL;

    gsize destination_contents_size = 0;

    GError *error = NULL;

    source_path =
        g_build_filename(
            fixture.source_directory,
            "donnees.BIN",
            NULL
        );

    assert(source_path != NULL);

    assert(
        g_file_set_contents(
            source_path,
            (const char *) binary_data,
            (gssize) sizeof(binary_data),
            &error
        )
    );

    assert(error == NULL);

    request.source_path =
        source_path;

    request.destination_directory =
        fixture.destination_directory;

    request.relative_directory =
        TEST_EVIDENCE_RELATIVE_DIRECTORY;

    request.type_identifier =
        "document";

    evidence_record =
        evidence_importer_import(
            fixture.evidence_importer,
            &request,
            NULL,
            &error
        );

    assert(error == NULL);
    assert(evidence_record != NULL);

    internal_name =
        evidence_record_get_internal_name(
            evidence_record
        );

    assert(internal_name != NULL);

    assert(
        g_str_has_suffix(
            internal_name,
            ".bin"
        )
    );

    assert(
        evidence_record_get_size_bytes(
            evidence_record
        ) ==
        sizeof(binary_data)
    );

    assert(
        strcmp(
            evidence_record_get_sha256(
                evidence_record
            ),
            "c90d3a7754f84abc867e780e5a0c0f8d"
            "a5c663bd1766ad89e538536a3763f367"
        ) == 0
    );

    destination_path =
        g_build_filename(
            fixture.destination_directory,
            internal_name,
            NULL
        );

    assert(destination_path != NULL);

    assert(
        g_file_get_contents(
            destination_path,
            &destination_contents,
            &destination_contents_size,
            &error
        )
    );

    assert(error == NULL);

    assert(
        destination_contents_size ==
        sizeof(binary_data)
    );

    assert(
        memcmp(
            destination_contents,
            binary_data,
            sizeof(binary_data)
        ) == 0
    );

    assert(
        !database_transaction_is_active(
            fixture.database
        )
    );

    evidence_record_free(
        evidence_record
    );

    g_free(
        destination_contents
    );

    assert(
        g_remove(
            destination_path
        ) == 0
    );

    assert(
        g_remove(
            source_path
        ) == 0
    );

    g_free(
        destination_path
    );

    g_free(
        source_path
    );

    test_evidence_importer_fixture_clear(
        &fixture
    );
}

/**
 * @brief Compte les entrées présentes dans le dossier de destination.
 */
static guint test_evidence_importer_count_destination_entries(
    const TestEvidenceImporterFixture *fixture
)
{
    GDir *directory = NULL;

    const char *entry_name = NULL;

    guint entry_count = 0;

    GError *error = NULL;

    assert(fixture != NULL);
    assert(fixture->destination_directory != NULL);

    directory =
        g_dir_open(
            fixture->destination_directory,
            0,
            &error
        );

    assert(directory != NULL);
    assert(error == NULL);

    while ((entry_name = g_dir_read_name(directory)) != NULL)
    {
        assert(entry_name[0] != '\0');

        entry_count++;
    }

    g_dir_close(
        directory
    );

    return entry_count;
}

/**
 * @brief Vérifie qu’une annulation préalable ne crée aucune preuve.
 */
static void test_evidence_importer_cancelled_before_copy(void)
{
    TestEvidenceImporterFixture fixture =
        test_evidence_importer_fixture_create();

    EvidenceImportRequest request = {0};

    EvidenceRecord *evidence_record = NULL;

    GCancellable *cancellable = NULL;

    char *source_path = NULL;

    guint64 evidence_count = 42;

    GError *error = NULL;

    source_path =
        g_build_filename(
            fixture.source_directory,
            "preuve_annulee.txt",
            NULL
        );

    assert(source_path != NULL);

    assert(
        g_file_set_contents(
            source_path,
            "preuve annulee",
            -1,
            &error
        )
    );

    assert(error == NULL);

    request.source_path =
        source_path;

    request.destination_directory =
        fixture.destination_directory;

    request.relative_directory =
        TEST_EVIDENCE_RELATIVE_DIRECTORY;

    request.type_identifier =
        "document";

    cancellable =
        g_cancellable_new();

    assert(cancellable != NULL);

    g_cancellable_cancel(
        cancellable
    );

    evidence_record =
        evidence_importer_import(
            fixture.evidence_importer,
            &request,
            cancellable,
            &error
        );

    assert(evidence_record == NULL);
    assert(error != NULL);

    assert(
        error->domain ==
        EVIDENCE_IMPORTER_ERROR
    );

    assert(
        error->code ==
        (gint) EVIDENCE_IMPORTER_ERROR_CANCELLED
    );

    assert(
        test_evidence_importer_count_destination_entries(
            &fixture
        ) == 0
    );

    assert(
        evidence_dao_count(
            fixture.evidence_dao,
            &evidence_count,
            NULL
        )
    );

    assert(evidence_count == 0);

    assert(
        !database_transaction_is_active(
            fixture.database
        )
    );

    g_clear_error(
        &error
    );

    g_object_unref(
        cancellable
    );

    assert(
        g_remove(
            source_path
        ) == 0
    );

    g_free(
        source_path
    );

    test_evidence_importer_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le rollback et le nettoyage pour un type inconnu.
 */
static void test_evidence_importer_unknown_type_cleanup(void)
{
    TestEvidenceImporterFixture fixture =
        test_evidence_importer_fixture_create();

    EvidenceImportRequest request = {0};

    EvidenceRecord *evidence_record = NULL;

    char *source_path = NULL;

    guint64 evidence_count = 42;

    GError *error = NULL;

    source_path =
        g_build_filename(
            fixture.source_directory,
            "preuve_type_inconnu.txt",
            NULL
        );

    assert(source_path != NULL);

    assert(
        g_file_set_contents(
            source_path,
            "contenu de preuve",
            -1,
            &error
        )
    );

    assert(error == NULL);

    request.source_path =
        source_path;

    request.destination_directory =
        fixture.destination_directory;

    request.relative_directory =
        TEST_EVIDENCE_RELATIVE_DIRECTORY;

    request.type_identifier =
        "type_totalement_inconnu";

    evidence_record =
        evidence_importer_import(
            fixture.evidence_importer,
            &request,
            NULL,
            &error
        );

    assert(evidence_record == NULL);
    assert(error != NULL);

    assert(
        error->domain ==
        EVIDENCE_IMPORTER_ERROR
    );

    assert(
        error->code ==
        (gint) EVIDENCE_IMPORTER_ERROR_DATABASE
    );

    assert(
        strstr(
            error->message,
            "type de preuve"
        ) != NULL
    );

    /*
     * La copie créée avant l’insertion doit avoir été supprimée.
     */
    assert(
        test_evidence_importer_count_destination_entries(
            &fixture
        ) == 0
    );

    assert(
        evidence_dao_count(
            fixture.evidence_dao,
            &evidence_count,
            NULL
        )
    );

    assert(evidence_count == 0);

    assert(
        !database_transaction_is_active(
            fixture.database
        )
    );

    /*
     * Le fichier source ne doit jamais être supprimé ou modifié.
     */
    assert(
        g_file_test(
            source_path,
            G_FILE_TEST_IS_REGULAR
        )
    );

    g_clear_error(
        &error
    );

    assert(
        g_remove(
            source_path
        ) == 0
    );

    g_free(
        source_path
    );

    test_evidence_importer_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie que l’importeur respecte une transaction appelante.
 */
static void test_evidence_importer_transaction_already_active(void)
{
    TestEvidenceImporterFixture fixture =
        test_evidence_importer_fixture_create();

    EvidenceImportRequest request = {0};

    EvidenceRecord *evidence_record = NULL;

    char *source_path = NULL;

    guint64 evidence_count = 42;

    GError *error = NULL;

    source_path =
        g_build_filename(
            fixture.source_directory,
            "preuve_transaction.txt",
            NULL
        );

    assert(source_path != NULL);

    assert(
        g_file_set_contents(
            source_path,
            "transaction externe",
            -1,
            &error
        )
    );

    assert(error == NULL);

    request.source_path =
        source_path;

    request.destination_directory =
        fixture.destination_directory;

    request.relative_directory =
        TEST_EVIDENCE_RELATIVE_DIRECTORY;

    request.type_identifier =
        "document";

    assert(
        database_transaction_begin(
            fixture.database
        )
    );

    assert(
        database_transaction_is_active(
            fixture.database
        )
    );

    evidence_record =
        evidence_importer_import(
            fixture.evidence_importer,
            &request,
            NULL,
            &error
        );

    assert(evidence_record == NULL);
    assert(error != NULL);

    assert(
        error->domain ==
        EVIDENCE_IMPORTER_ERROR
    );

    assert(
        error->code ==
        (gint) EVIDENCE_IMPORTER_ERROR_DATABASE
    );

    /*
     * L’importeur ne possède pas cette transaction :
     * il ne doit donc pas l’annuler.
     */
    assert(
        database_transaction_is_active(
            fixture.database
        )
    );

    assert(
        test_evidence_importer_count_destination_entries(
            &fixture
        ) == 0
    );

    assert(
        evidence_dao_count(
            fixture.evidence_dao,
            &evidence_count,
            NULL
        )
    );

    assert(evidence_count == 0);

    g_clear_error(
        &error
    );

    assert(
        database_transaction_rollback(
            fixture.database
        )
    );

    assert(
        !database_transaction_is_active(
            fixture.database
        )
    );

    assert(
        g_remove(
            source_path
        ) == 0
    );

    g_free(
        source_path
    );

    test_evidence_importer_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le nettoyage lorsque le modèle refuse les métadonnées.
 */
static void test_evidence_importer_invalid_collected_at_cleanup(void)
{
    TestEvidenceImporterFixture fixture =
        test_evidence_importer_fixture_create();

    EvidenceImportRequest request = {0};

    EvidenceRecord *evidence_record = NULL;

    char *source_path = NULL;

    guint64 evidence_count = 42;

    GError *error = NULL;

    source_path =
        g_build_filename(
            fixture.source_directory,
            "preuve_date_invalide.txt",
            NULL
        );

    assert(source_path != NULL);

    assert(
        g_file_set_contents(
            source_path,
            "preuve avec date invalide",
            -1,
            &error
        )
    );

    assert(error == NULL);

    request.source_path =
        source_path;

    request.destination_directory =
        fixture.destination_directory;

    request.relative_directory =
        TEST_EVIDENCE_RELATIVE_DIRECTORY;

    request.type_identifier =
        "document";

    /*
     * EvidenceRecord exige une date UTC stricte.
     */
    request.collected_at =
        "18/07/2026 20:30";

    request.source =
        "Fichier de test.";

    request.description =
        "La création du modèle doit échouer.";

    evidence_record =
        evidence_importer_import(
            fixture.evidence_importer,
            &request,
            NULL,
            &error
        );

    assert(evidence_record == NULL);
    assert(error != NULL);

    assert(
        error->domain ==
        EVIDENCE_IMPORTER_ERROR
    );

    assert(
        error->code ==
        (gint) EVIDENCE_IMPORTER_ERROR_MODEL
    );

    /*
     * La copie a eu lieu avant la création du modèle,
     * mais elle doit avoir été supprimée.
     */
    assert(
        test_evidence_importer_count_destination_entries(
            &fixture
        ) == 0
    );

    assert(
        evidence_dao_count(
            fixture.evidence_dao,
            &evidence_count,
            NULL
        )
    );

    assert(evidence_count == 0);

    assert(
        !database_transaction_is_active(
            fixture.database
        )
    );

    /*
     * Le fichier source reste intact.
     */
    assert(
        g_file_test(
            source_path,
            G_FILE_TEST_IS_REGULAR
        )
    );

    g_clear_error(
        &error
    );

    assert(
        g_remove(
            source_path
        ) == 0
    );

    g_free(
        source_path
    );

    test_evidence_importer_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie la conservation d’un nom original complexe.
 */
static void test_evidence_importer_complex_original_name(void)
{
    TestEvidenceImporterFixture fixture =
        test_evidence_importer_fixture_create();

    EvidenceImportRequest request = {0};

    EvidenceRecord *evidence_record = NULL;

    char *source_path = NULL;
    char *destination_path = NULL;

    const char *identifier = NULL;
    const char *internal_name = NULL;

    guint64 evidence_count = 0;

    GError *error = NULL;

    source_path =
        g_build_filename(
            fixture.source_directory,
            "Capture été.Version Finale.PDF",
            NULL
        );

    assert(source_path != NULL);

    assert(
        g_file_set_contents(
            source_path,
            "contenu du document",
            -1,
            &error
        )
    );

    assert(error == NULL);

    request.source_path =
        source_path;

    request.destination_directory =
        fixture.destination_directory;

    request.relative_directory =
        TEST_EVIDENCE_RELATIVE_DIRECTORY;

    request.type_identifier =
        "document";

    evidence_record =
        evidence_importer_import(
            fixture.evidence_importer,
            &request,
            NULL,
            &error
        );

    assert(error == NULL);
    assert(evidence_record != NULL);

    identifier =
        evidence_record_get_identifier(
            evidence_record
        );

    internal_name =
        evidence_record_get_internal_name(
            evidence_record
        );

    assert(identifier != NULL);
    assert(internal_name != NULL);

    /*
     * Le nom fourni par l’utilisateur doit être conservé
     * exactement dans le modèle.
     */
    assert(
        strcmp(
            evidence_record_get_original_name(
                evidence_record
            ),
            "Capture été.Version Finale.PDF"
        ) == 0
    );

    /*
     * Le nom interne ne conserve que l’UUID et
     * l’extension finale normalisée.
     */
    assert(
        g_str_has_prefix(
            internal_name,
            identifier
        )
    );

    assert(
        g_str_has_suffix(
            internal_name,
            ".pdf"
        )
    );

    assert(
        strlen(
            internal_name
        ) ==
        strlen(identifier) + strlen(".pdf")
    );

    assert(
        strchr(
            internal_name,
            ' '
        ) == NULL
    );

    assert(
        strchr(
            internal_name,
            '/'
        ) == NULL
    );

    assert(
        strchr(
            internal_name,
            '\\'
        ) == NULL
    );

    destination_path =
        g_build_filename(
            fixture.destination_directory,
            internal_name,
            NULL
        );

    assert(destination_path != NULL);

    assert(
        g_file_test(
            destination_path,
            G_FILE_TEST_IS_REGULAR
        )
    );

    assert(
        evidence_dao_count(
            fixture.evidence_dao,
            &evidence_count,
            &error
        )
    );

    assert(error == NULL);
    assert(evidence_count == 1);

    assert(
        !database_transaction_is_active(
            fixture.database
        )
    );

    evidence_record_free(
        evidence_record
    );

    assert(
        g_remove(
            destination_path
        ) == 0
    );

    assert(
        g_remove(
            source_path
        ) == 0
    );

    g_free(
        destination_path
    );

    g_free(
        source_path
    );

    test_evidence_importer_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d’un chemin relatif sortant de l’enquête.
 */
static void test_evidence_importer_invalid_relative_directory(void)
{
    TestEvidenceImporterFixture fixture =
        test_evidence_importer_fixture_create();

    EvidenceImportRequest request = {0};

    EvidenceRecord *evidence_record = NULL;

    char *source_path = NULL;

    guint64 evidence_count = 42;

    GError *error = NULL;

    source_path =
        g_build_filename(
            fixture.source_directory,
            "preuve.txt",
            NULL
        );

    assert(source_path != NULL);

    assert(
        g_file_set_contents(
            source_path,
            "contenu",
            -1,
            &error
        )
    );

    assert(error == NULL);

    request.source_path =
        source_path;

    request.destination_directory =
        fixture.destination_directory;

    request.relative_directory =
        "../../hors_enquete";

    request.type_identifier =
        "document";

    evidence_record =
        evidence_importer_import(
            fixture.evidence_importer,
            &request,
            NULL,
            &error
        );

    assert(evidence_record == NULL);
    assert(error != NULL);

    assert(
        error->domain ==
        EVIDENCE_IMPORTER_ERROR
    );

    assert(
        error->code ==
        (gint) EVIDENCE_IMPORTER_ERROR_INVALID_ARGUMENT
    );

    assert(
        test_evidence_importer_count_destination_entries(
            &fixture
        ) == 0
    );

    assert(
        evidence_dao_count(
            fixture.evidence_dao,
            &evidence_count,
            NULL
        )
    );

    assert(evidence_count == 0);

    assert(
        !database_transaction_is_active(
            fixture.database
        )
    );

    g_clear_error(
        &error
    );

    assert(
        g_remove(
            source_path
        ) == 0
    );

    g_free(
        source_path
    );

    test_evidence_importer_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le refus d’un fichier source inexistant.
 */
static void test_evidence_importer_missing_source(void)
{
    TestEvidenceImporterFixture fixture =
        test_evidence_importer_fixture_create();

    EvidenceImportRequest request = {0};

    EvidenceRecord *evidence_record = NULL;

    char *source_path = NULL;

    guint64 evidence_count = 42;

    GError *error = NULL;

    source_path =
        g_build_filename(
            fixture.source_directory,
            "preuve_absente.txt",
            NULL
        );

    assert(source_path != NULL);

    assert(
        !g_file_test(
            source_path,
            G_FILE_TEST_EXISTS
        )
    );

    request.source_path =
        source_path;

    request.destination_directory =
        fixture.destination_directory;

    request.relative_directory =
        TEST_EVIDENCE_RELATIVE_DIRECTORY;

    request.type_identifier =
        "document";

    evidence_record =
        evidence_importer_import(
            fixture.evidence_importer,
            &request,
            NULL,
            &error
        );

    assert(evidence_record == NULL);
    assert(error != NULL);

    assert(
        error->domain ==
        EVIDENCE_IMPORTER_ERROR
    );

    assert(
        error->code ==
        (gint) EVIDENCE_IMPORTER_ERROR_COPY
    );

    assert(
        strstr(
            error->message,
            "Impossible de copier"
        ) != NULL
    );

    assert(
        test_evidence_importer_count_destination_entries(
            &fixture
        ) == 0
    );

    assert(
        evidence_dao_count(
            fixture.evidence_dao,
            &evidence_count,
            NULL
        )
    );

    assert(evidence_count == 0);

    assert(
        !database_transaction_is_active(
            fixture.database
        )
    );

    g_clear_error(
        &error
    );

    g_free(
        source_path
    );

    test_evidence_importer_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie l’annulation après la création de la copie.
 */
static void test_evidence_importer_cancelled_after_copy_cleanup(void)
{
    TestEvidenceImporterFixture fixture =
        test_evidence_importer_fixture_create();

    EvidenceImportRequest request = {0};

    EvidenceRecord *evidence_record = NULL;

    GCancellable *cancellable = NULL;

    char *source_path = NULL;

    guint64 evidence_count = 42;

    GError *error = NULL;

    evidence_importer_test_reset_hooks();

    source_path =
        g_build_filename(
            fixture.source_directory,
            "preuve_annulee_apres_copie.txt",
            NULL
        );

    assert(source_path != NULL);

    assert(
        g_file_set_contents(
            source_path,
            "annulation après copie",
            -1,
            &error
        )
    );

    assert(error == NULL);

    request.source_path =
        source_path;

    request.destination_directory =
        fixture.destination_directory;

    request.relative_directory =
        TEST_EVIDENCE_RELATIVE_DIRECTORY;

    request.type_identifier =
        "document";

    cancellable =
        g_cancellable_new();

    assert(cancellable != NULL);

    evidence_importer_test_set_cancel_after_copy(
        TRUE
    );

    evidence_record =
        evidence_importer_import(
            fixture.evidence_importer,
            &request,
            cancellable,
            &error
        );

    assert(evidence_record == NULL);
    assert(error != NULL);

    assert(
        error->domain ==
        EVIDENCE_IMPORTER_ERROR
    );

    assert(
        error->code ==
        (gint) EVIDENCE_IMPORTER_ERROR_CANCELLED
    );

    /*
     * Le fichier a été copié avant l’annulation,
     * puis supprimé par la compensation.
     */
    assert(
        test_evidence_importer_count_destination_entries(
            &fixture
        ) == 0
    );

    assert(
        evidence_dao_count(
            fixture.evidence_dao,
            &evidence_count,
            NULL
        )
    );

    assert(evidence_count == 0);

    assert(
        !database_transaction_is_active(
            fixture.database
        )
    );

    assert(
        g_file_test(
            source_path,
            G_FILE_TEST_IS_REGULAR
        )
    );

    evidence_importer_test_reset_hooks();

    g_clear_error(
        &error
    );

    g_object_unref(
        cancellable
    );

    assert(
        g_remove(
            source_path
        ) == 0
    );

    g_free(
        source_path
    );

    test_evidence_importer_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le rollback lorsque le COMMIT SQLite échoue.
 */
static void test_evidence_importer_commit_failure_cleanup(void)
{
    TestEvidenceImporterFixture fixture =
        test_evidence_importer_fixture_create();

    EvidenceImportRequest request = {0};

    EvidenceRecord *evidence_record = NULL;

    char *source_path = NULL;

    guint64 evidence_count = 42;

    GError *error = NULL;

    evidence_importer_test_reset_hooks();

    source_path =
        g_build_filename(
            fixture.source_directory,
            "preuve_commit_echec.txt",
            NULL
        );

    assert(source_path != NULL);

    assert(
        g_file_set_contents(
            source_path,
            "échec artificiel du commit",
            -1,
            &error
        )
    );

    assert(error == NULL);

    request.source_path =
        source_path;

    request.destination_directory =
        fixture.destination_directory;

    request.relative_directory =
        TEST_EVIDENCE_RELATIVE_DIRECTORY;

    request.type_identifier =
        "document";

    request.source =
        "Test du rollback après échec du commit.";

    evidence_importer_test_set_commit_failure(
        TRUE
    );

    evidence_record =
        evidence_importer_import(
            fixture.evidence_importer,
            &request,
            NULL,
            &error
        );

    assert(evidence_record == NULL);
    assert(error != NULL);

    assert(
        error->domain ==
        EVIDENCE_IMPORTER_ERROR
    );

    assert(
        error->code ==
        (gint) EVIDENCE_IMPORTER_ERROR_DATABASE
    );

    assert(
        strstr(
            error->message,
            "COMMIT simulé"
        ) != NULL
    );

    /*
     * L’insertion a eu lieu dans la transaction,
     * mais le rollback doit l’avoir annulée.
     */
    assert(
        evidence_dao_count(
            fixture.evidence_dao,
            &evidence_count,
            NULL
        )
    );

    assert(evidence_count == 0);

    /*
     * La copie doit également avoir été supprimée.
     */
    assert(
        test_evidence_importer_count_destination_entries(
            &fixture
        ) == 0
    );

    assert(
        !database_transaction_is_active(
            fixture.database
        )
    );

    /*
     * Le fichier source reste intact.
     */
    assert(
        g_file_test(
            source_path,
            G_FILE_TEST_IS_REGULAR
        )
    );

    evidence_importer_test_reset_hooks();

    g_clear_error(
        &error
    );

    assert(
        g_remove(
            source_path
        ) == 0
    );

    g_free(
        source_path
    );

    test_evidence_importer_fixture_clear(
        &fixture
    );
}

/**
 * @brief Vérifie le signalement d’une copie orpheline.
 */
static void test_evidence_importer_cleanup_failure_reports_orphan(void)
{
    TestEvidenceImporterFixture fixture =
        test_evidence_importer_fixture_create();

    EvidenceImportRequest request = {0};

    EvidenceRecord *evidence_record = NULL;

    GDir *destination_directory = NULL;

    const char *orphan_name = NULL;

    char *source_path = NULL;
    char *orphan_path = NULL;

    guint64 evidence_count = 42;

    GError *error = NULL;
    GError *directory_error = NULL;

    evidence_importer_test_reset_hooks();

    source_path =
        g_build_filename(
            fixture.source_directory,
            "preuve_nettoyage_echec.txt",
            NULL
        );

    assert(source_path != NULL);

    assert(
        g_file_set_contents(
            source_path,
            "échec artificiel du nettoyage",
            -1,
            &error
        )
    );

    assert(error == NULL);

    request.source_path =
        source_path;

    request.destination_directory =
        fixture.destination_directory;

    request.relative_directory =
        TEST_EVIDENCE_RELATIVE_DIRECTORY;

    /*
     * Le type inconnu provoque l’échec après la copie.
     */
    request.type_identifier =
        "type_inconnu_pour_nettoyage";

    evidence_importer_test_set_cleanup_failure(
        TRUE
    );

    evidence_record =
        evidence_importer_import(
            fixture.evidence_importer,
            &request,
            NULL,
            &error
        );

    assert(evidence_record == NULL);
    assert(error != NULL);

    assert(
        error->domain ==
        EVIDENCE_IMPORTER_ERROR
    );

    assert(
        error->code ==
        (gint) EVIDENCE_IMPORTER_ERROR_ROLLBACK
    );

    assert(
        strstr(
            error->message,
            "n’a pas pu être supprimée"
        ) != NULL
    );

    assert(
        strstr(
            error->message,
            "Cause initiale"
        ) != NULL
    );

    /*
     * L’insertion SQLite a été annulée.
     */
    assert(
        evidence_dao_count(
            fixture.evidence_dao,
            &evidence_count,
            NULL
        )
    );

    assert(evidence_count == 0);

    assert(
        !database_transaction_is_active(
            fixture.database
        )
    );

    /*
     * Le hook a empêché la suppression :
     * une copie orpheline doit donc être présente.
     */
    assert(
        test_evidence_importer_count_destination_entries(
            &fixture
        ) == 1
    );

    destination_directory =
        g_dir_open(
            fixture.destination_directory,
            0,
            &directory_error
        );

    assert(destination_directory != NULL);
    assert(directory_error == NULL);

    orphan_name =
        g_dir_read_name(
            destination_directory
        );

    assert(orphan_name != NULL);
    assert(orphan_name[0] != '\0');

    orphan_path =
        g_build_filename(
            fixture.destination_directory,
            orphan_name,
            NULL
        );

    assert(orphan_path != NULL);

    assert(
        g_file_test(
            orphan_path,
            G_FILE_TEST_IS_REGULAR
        )
    );

    /*
     * Le message doit permettre de localiser le fichier orphelin.
     */
    assert(
        strstr(
            error->message,
            orphan_path
        ) != NULL
    );

    g_dir_close(
        destination_directory
    );

    destination_directory = NULL;

    /*
     * On désactive le hook avant de nettoyer manuellement
     * l’environnement du test.
     */
    evidence_importer_test_reset_hooks();

    g_clear_error(
        &error
    );

    assert(
        g_remove(
            orphan_path
        ) == 0
    );

    assert(
        g_remove(
            source_path
        ) == 0
    );

    g_free(
        orphan_path
    );

    g_free(
        source_path
    );

    test_evidence_importer_fixture_clear(
        &fixture
    );
}

int main(void)
{
    test_evidence_importer_new_null_database();
    test_evidence_importer_new_valid();
    test_evidence_importer_import_valid_text_file();
    test_evidence_importer_import_empty_optional_null();
    test_evidence_importer_import_binary_file();
    test_evidence_importer_cancelled_before_copy();
    test_evidence_importer_unknown_type_cleanup();
    test_evidence_importer_transaction_already_active();
    test_evidence_importer_invalid_collected_at_cleanup();
    test_evidence_importer_complex_original_name();
    test_evidence_importer_invalid_relative_directory();
    test_evidence_importer_missing_source();
    test_evidence_importer_cancelled_after_copy_cleanup();
    test_evidence_importer_commit_failure_cleanup();
    test_evidence_importer_cleanup_failure_reports_orphan();

    printf(
        "EvidenceImporter : premiers tests transactionnels valides.\n"
    );

    return 0;
}
