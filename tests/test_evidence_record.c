/******************************************************************************
 * @file test_evidence_record.c
 * @brief Tests du modèle EvidenceRecord.
 ******************************************************************************/

#include "models/evidence_record.h"

#include <glib.h>

#define TEST_EVIDENCE_IDENTIFIER \
    "6e62b9af-2046-4efd-b3b9-29869f816951"

#define TEST_EVIDENCE_SHA256_LOWER \
    "0123456789abcdef" \
    "0123456789abcdef" \
    "0123456789abcdef" \
    "0123456789abcdef"

#define TEST_EVIDENCE_SHA256_UPPER \
    "0123456789ABCDEF" \
    "0123456789ABCDEF" \
    "0123456789ABCDEF" \
    "0123456789ABCDEF"

typedef struct
{
    const char *identifier;
    const char *original_name;
    const char *internal_name;
    const char *relative_path;
    const char *type_identifier;

    guint64 size_bytes;

    const char *sha256;
    const char *imported_at;
    const char *collected_at;
    const char *source;
    const char *description;

    EvidenceIntegrityStatus integrity_status;
} TestEvidenceRecordInput;

static TestEvidenceRecordInput test_evidence_record_valid_input(void)
{
    return (TestEvidenceRecordInput)
    {
        .identifier = TEST_EVIDENCE_IDENTIFIER,
        .original_name = "capture_originale.png",
        .internal_name = "preuve_001.png",
        .relative_path =
            "01_Preuves_Originales/Captures_Ecran/preuve_001.png",
        .type_identifier = "image",
        .size_bytes = 4096,
        .sha256 = TEST_EVIDENCE_SHA256_LOWER,
        .imported_at = "2026-07-18T14:30:00Z",
        .collected_at = "2026-07-17T20:15:30Z",
        .source = "Conversation Instagram",
        .description = "Capture originale reçue par la victime.",
        .integrity_status = EVIDENCE_INTEGRITY_STATUS_VALID
    };
}

static EvidenceRecord *test_evidence_record_create(
    const TestEvidenceRecordInput *input,
    GError **error
)
{
    g_assert_nonnull(
        input
    );

    return evidence_record_new(
        input->identifier,
        input->original_name,
        input->internal_name,
        input->relative_path,
        input->type_identifier,
        input->size_bytes,
        input->sha256,
        input->imported_at,
        input->collected_at,
        input->source,
        input->description,
        input->integrity_status,
        error
    );
}

static void test_evidence_record_assert_invalid(
    const TestEvidenceRecordInput *input,
    EvidenceRecordError expected_error
)
{
    EvidenceRecord *evidence_record = NULL;
    GError *error = NULL;

    evidence_record =
        test_evidence_record_create(
            input,
            &error
        );

    g_assert_null(
        evidence_record
    );

    g_assert_error(
        error,
        EVIDENCE_RECORD_ERROR,
        (gint) expected_error
    );

    g_assert_nonnull(
        error->message
    );

    g_assert_cmpstr(
        error->message,
        !=,
        ""
    );

    g_clear_error(
        &error
    );
}

static void test_evidence_record_valid_full(void)
{
    TestEvidenceRecordInput input =
        test_evidence_record_valid_input();

    EvidenceRecord *evidence_record = NULL;
    GError *error = NULL;

    evidence_record =
        test_evidence_record_create(
            &input,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        evidence_record
    );

    g_assert_cmpstr(
        evidence_record_get_identifier(
            evidence_record
        ),
        ==,
        TEST_EVIDENCE_IDENTIFIER
    );

    g_assert_cmpstr(
        evidence_record_get_original_name(
            evidence_record
        ),
        ==,
        "capture_originale.png"
    );

    g_assert_cmpstr(
        evidence_record_get_internal_name(
            evidence_record
        ),
        ==,
        "preuve_001.png"
    );

    g_assert_cmpstr(
        evidence_record_get_relative_path(
            evidence_record
        ),
        ==,
        "01_Preuves_Originales/Captures_Ecran/preuve_001.png"
    );

    g_assert_cmpstr(
        evidence_record_get_type_identifier(
            evidence_record
        ),
        ==,
        "image"
    );

    g_assert_cmpuint(
        evidence_record_get_size_bytes(
            evidence_record
        ),
        ==,
        4096
    );

    g_assert_cmpstr(
        evidence_record_get_sha256(
            evidence_record
        ),
        ==,
        TEST_EVIDENCE_SHA256_LOWER
    );

    g_assert_cmpstr(
        evidence_record_get_imported_at(
            evidence_record
        ),
        ==,
        "2026-07-18T14:30:00Z"
    );

    g_assert_cmpstr(
        evidence_record_get_collected_at(
            evidence_record
        ),
        ==,
        "2026-07-17T20:15:30Z"
    );

    g_assert_cmpstr(
        evidence_record_get_source(
            evidence_record
        ),
        ==,
        "Conversation Instagram"
    );

    g_assert_cmpstr(
        evidence_record_get_description(
            evidence_record
        ),
        ==,
        "Capture originale reçue par la victime."
    );

    g_assert_cmpint(
        evidence_record_get_integrity_status(
            evidence_record
        ),
        ==,
        EVIDENCE_INTEGRITY_STATUS_VALID
    );

    evidence_record_free(
        evidence_record
    );
}

static void test_evidence_record_valid_optional_null(void)
{
    TestEvidenceRecordInput input =
        test_evidence_record_valid_input();

    EvidenceRecord *evidence_record = NULL;
    GError *error = NULL;

    input.collected_at = NULL;
    input.source = "   ";
    input.description = "\t";

    evidence_record =
        test_evidence_record_create(
            &input,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        evidence_record
    );

    g_assert_null(
        evidence_record_get_collected_at(
            evidence_record
        )
    );

    g_assert_null(
        evidence_record_get_source(
            evidence_record
        )
    );

    g_assert_null(
        evidence_record_get_description(
            evidence_record
        )
    );

    evidence_record_free(
        evidence_record
    );
}

static void test_evidence_record_copies_strings(void)
{
    char identifier[] =
        TEST_EVIDENCE_IDENTIFIER;

    char original_name[] =
        "capture_originale.png";

    char internal_name[] =
        "preuve_001.png";

    char relative_path[] =
        "01_Preuves_Originales/Captures_Ecran/preuve_001.png";

    char type_identifier[] =
        "image";

    char sha256[] =
        TEST_EVIDENCE_SHA256_LOWER;

    char imported_at[] =
        "2026-07-18T14:30:00Z";

    char collected_at[] =
        "2026-07-17T20:15:30Z";

    char source[] =
        "Conversation Instagram";

    char description[] =
        "Capture originale";

    TestEvidenceRecordInput input =
    {
        .identifier = identifier,
        .original_name = original_name,
        .internal_name = internal_name,
        .relative_path = relative_path,
        .type_identifier = type_identifier,
        .size_bytes = 123,
        .sha256 = sha256,
        .imported_at = imported_at,
        .collected_at = collected_at,
        .source = source,
        .description = description,
        .integrity_status = EVIDENCE_INTEGRITY_STATUS_VALID
    };

    EvidenceRecord *evidence_record = NULL;
    GError *error = NULL;

    evidence_record =
        test_evidence_record_create(
            &input,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        evidence_record
    );

    identifier[0] = 'X';
    original_name[0] = 'X';
    internal_name[0] = 'X';
    relative_path[0] = 'X';
    type_identifier[0] = 'X';
    sha256[0] = 'f';
    imported_at[0] = 'X';
    collected_at[0] = 'X';
    source[0] = 'X';
    description[0] = 'X';

    g_assert_cmpstr(
        evidence_record_get_identifier(
            evidence_record
        ),
        ==,
        TEST_EVIDENCE_IDENTIFIER
    );

    g_assert_cmpstr(
        evidence_record_get_original_name(
            evidence_record
        ),
        ==,
        "capture_originale.png"
    );

    g_assert_cmpstr(
        evidence_record_get_internal_name(
            evidence_record
        ),
        ==,
        "preuve_001.png"
    );

    g_assert_cmpstr(
        evidence_record_get_relative_path(
            evidence_record
        ),
        ==,
        "01_Preuves_Originales/Captures_Ecran/preuve_001.png"
    );

    g_assert_cmpstr(
        evidence_record_get_type_identifier(
            evidence_record
        ),
        ==,
        "image"
    );

    g_assert_cmpstr(
        evidence_record_get_sha256(
            evidence_record
        ),
        ==,
        TEST_EVIDENCE_SHA256_LOWER
    );

    g_assert_cmpstr(
        evidence_record_get_imported_at(
            evidence_record
        ),
        ==,
        "2026-07-18T14:30:00Z"
    );

    g_assert_cmpstr(
        evidence_record_get_collected_at(
            evidence_record
        ),
        ==,
        "2026-07-17T20:15:30Z"
    );

    g_assert_cmpstr(
        evidence_record_get_source(
            evidence_record
        ),
        ==,
        "Conversation Instagram"
    );

    g_assert_cmpstr(
        evidence_record_get_description(
            evidence_record
        ),
        ==,
        "Capture originale"
    );

    evidence_record_free(
        evidence_record
    );
}

static void test_evidence_record_trims_text(void)
{
    TestEvidenceRecordInput input =
        test_evidence_record_valid_input();

    EvidenceRecord *evidence_record = NULL;
    GError *error = NULL;

    input.identifier =
        "  6e62b9af-2046-4efd-b3b9-29869f816951  ";

    input.original_name =
        "  capture.png  ";

    input.internal_name =
        "\tpreuve.png\t";

    input.relative_path =
        "  01_Preuves_Originales/preuve.png  ";

    input.type_identifier =
        "  image  ";

    input.sha256 =
        "  0123456789ABCDEF0123456789ABCDEF"
        "0123456789ABCDEF0123456789ABCDEF  ";

    input.imported_at =
        "  2026-07-18T14:30:00Z  ";

    input.collected_at =
        "\t2026-07-17T20:15:30Z\t";

    input.source =
        "  Téléchargement manuel  ";

    input.description =
        "  Capture principale  ";

    evidence_record =
        test_evidence_record_create(
            &input,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        evidence_record
    );

    g_assert_cmpstr(
        evidence_record_get_identifier(
            evidence_record
        ),
        ==,
        TEST_EVIDENCE_IDENTIFIER
    );

    g_assert_cmpstr(
        evidence_record_get_original_name(
            evidence_record
        ),
        ==,
        "capture.png"
    );

    g_assert_cmpstr(
        evidence_record_get_internal_name(
            evidence_record
        ),
        ==,
        "preuve.png"
    );

    g_assert_cmpstr(
        evidence_record_get_relative_path(
            evidence_record
        ),
        ==,
        "01_Preuves_Originales/preuve.png"
    );

    g_assert_cmpstr(
        evidence_record_get_type_identifier(
            evidence_record
        ),
        ==,
        "image"
    );

    g_assert_cmpstr(
        evidence_record_get_sha256(
            evidence_record
        ),
        ==,
        TEST_EVIDENCE_SHA256_LOWER
    );

    g_assert_cmpstr(
        evidence_record_get_source(
            evidence_record
        ),
        ==,
        "Téléchargement manuel"
    );

    g_assert_cmpstr(
        evidence_record_get_description(
            evidence_record
        ),
        ==,
        "Capture principale"
    );

    evidence_record_free(
        evidence_record
    );
}

static void test_evidence_record_invalid_uuid(void)
{
    TestEvidenceRecordInput input =
        test_evidence_record_valid_input();

    input.identifier =
        "identifiant-invalide";

    test_evidence_record_assert_invalid(
        &input,
        EVIDENCE_RECORD_ERROR_INVALID_IDENTIFIER
    );
}

static void test_evidence_record_invalid_original_name(void)
{
    TestEvidenceRecordInput input =
        test_evidence_record_valid_input();

    input.original_name =
        "dossier/capture.png";

    test_evidence_record_assert_invalid(
        &input,
        EVIDENCE_RECORD_ERROR_INVALID_NAME
    );
}

static void test_evidence_record_invalid_internal_name(void)
{
    TestEvidenceRecordInput input =
        test_evidence_record_valid_input();

    input.internal_name =
        "..";

    test_evidence_record_assert_invalid(
        &input,
        EVIDENCE_RECORD_ERROR_INVALID_NAME
    );
}

static void test_evidence_record_absolute_path(void)
{
    TestEvidenceRecordInput input =
        test_evidence_record_valid_input();

    input.relative_path =
        "/tmp/preuve.png";

    test_evidence_record_assert_invalid(
        &input,
        EVIDENCE_RECORD_ERROR_INVALID_PATH
    );
}

static void test_evidence_record_parent_component(void)
{
    TestEvidenceRecordInput input =
        test_evidence_record_valid_input();

    input.relative_path =
        "01_Preuves_Originales/../preuve.png";

    test_evidence_record_assert_invalid(
        &input,
        EVIDENCE_RECORD_ERROR_INVALID_PATH
    );
}

static void test_evidence_record_relative_path(void)
{
    TestEvidenceRecordInput input =
        test_evidence_record_valid_input();

    EvidenceRecord *evidence_record = NULL;
    GError *error = NULL;

    input.relative_path =
        "01_Preuves_Originales/Documents/preuve.pdf";

    evidence_record =
        test_evidence_record_create(
            &input,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        evidence_record
    );

    g_assert_cmpstr(
        evidence_record_get_relative_path(
            evidence_record
        ),
        ==,
        "01_Preuves_Originales/Documents/preuve.pdf"
    );

    evidence_record_free(
        evidence_record
    );
}

static void test_evidence_record_empty_type(void)
{
    TestEvidenceRecordInput input =
        test_evidence_record_valid_input();

    input.type_identifier =
        "   ";

    test_evidence_record_assert_invalid(
        &input,
        EVIDENCE_RECORD_ERROR_INVALID_TYPE
    );
}

static void test_evidence_record_short_sha256(void)
{
    TestEvidenceRecordInput input =
        test_evidence_record_valid_input();

    input.sha256 =
        "0123456789abcdef";

    test_evidence_record_assert_invalid(
        &input,
        EVIDENCE_RECORD_ERROR_INVALID_SHA256
    );
}

static void test_evidence_record_invalid_sha256_character(void)
{
    TestEvidenceRecordInput input =
        test_evidence_record_valid_input();

    char invalid_sha256[] =
        TEST_EVIDENCE_SHA256_LOWER;

    invalid_sha256[63] = 'g';

    input.sha256 =
        invalid_sha256;

    test_evidence_record_assert_invalid(
        &input,
        EVIDENCE_RECORD_ERROR_INVALID_SHA256
    );
}

static void test_evidence_record_uppercase_sha256(void)
{
    TestEvidenceRecordInput input =
        test_evidence_record_valid_input();

    EvidenceRecord *evidence_record = NULL;
    GError *error = NULL;

    input.sha256 =
        TEST_EVIDENCE_SHA256_UPPER;

    evidence_record =
        test_evidence_record_create(
            &input,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        evidence_record
    );

    g_assert_cmpstr(
        evidence_record_get_sha256(
            evidence_record
        ),
        ==,
        TEST_EVIDENCE_SHA256_LOWER
    );

    evidence_record_free(
        evidence_record
    );
}

static void test_evidence_record_invalid_imported_at(void)
{
    TestEvidenceRecordInput input =
        test_evidence_record_valid_input();

    input.imported_at =
        "2026-07-18 14:30:00Z";

    test_evidence_record_assert_invalid(
        &input,
        EVIDENCE_RECORD_ERROR_INVALID_DATE
    );
}

static void test_evidence_record_invalid_collected_at(void)
{
    TestEvidenceRecordInput input =
        test_evidence_record_valid_input();

    input.collected_at =
        "2026-02-30T14:30:00Z";

    test_evidence_record_assert_invalid(
        &input,
        EVIDENCE_RECORD_ERROR_INVALID_DATE
    );
}

static void test_evidence_record_invalid_status(void)
{
    TestEvidenceRecordInput input =
        test_evidence_record_valid_input();

    input.integrity_status =
        (EvidenceIntegrityStatus) 999;

    test_evidence_record_assert_invalid(
        &input,
        EVIDENCE_RECORD_ERROR_INVALID_STATUS
    );
}

static void test_evidence_record_zero_size(void)
{
    TestEvidenceRecordInput input =
        test_evidence_record_valid_input();

    EvidenceRecord *evidence_record = NULL;
    GError *error = NULL;

    input.size_bytes = 0;

    evidence_record =
        test_evidence_record_create(
            &input,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        evidence_record
    );

    g_assert_cmpuint(
        evidence_record_get_size_bytes(
            evidence_record
        ),
        ==,
        0
    );

    evidence_record_free(
        evidence_record
    );
}

static void test_evidence_record_null_accessors(void)
{
    g_assert_null(
        evidence_record_get_identifier(
            NULL
        )
    );

    g_assert_null(
        evidence_record_get_original_name(
            NULL
        )
    );

    g_assert_null(
        evidence_record_get_internal_name(
            NULL
        )
    );

    g_assert_null(
        evidence_record_get_relative_path(
            NULL
        )
    );

    g_assert_null(
        evidence_record_get_type_identifier(
            NULL
        )
    );

    g_assert_cmpuint(
        evidence_record_get_size_bytes(
            NULL
        ),
        ==,
        0
    );

    g_assert_null(
        evidence_record_get_sha256(
            NULL
        )
    );

    g_assert_null(
        evidence_record_get_imported_at(
            NULL
        )
    );

    g_assert_null(
        evidence_record_get_collected_at(
            NULL
        )
    );

    g_assert_null(
        evidence_record_get_source(
            NULL
        )
    );

    g_assert_null(
        evidence_record_get_description(
            NULL
        )
    );

    g_assert_cmpint(
        evidence_record_get_integrity_status(
            NULL
        ),
        ==,
        EVIDENCE_INTEGRITY_STATUS_UNKNOWN
    );
}

static void test_evidence_record_free_null(void)
{
    evidence_record_free(
        NULL
    );
}

static void test_evidence_record_optional_error(void)
{
    TestEvidenceRecordInput input =
        test_evidence_record_valid_input();

    EvidenceRecord *evidence_record = NULL;

    input.identifier =
        "uuid-invalide";

    evidence_record =
        test_evidence_record_create(
            &input,
            NULL
        );

    g_assert_null(
        evidence_record
    );

    input =
        test_evidence_record_valid_input();

    evidence_record =
        test_evidence_record_create(
            &input,
            NULL
        );

    g_assert_nonnull(
        evidence_record
    );

    evidence_record_free(
        evidence_record
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
        "/evidence_record/valid_full",
        test_evidence_record_valid_full
    );

    g_test_add_func(
        "/evidence_record/valid_optional_null",
        test_evidence_record_valid_optional_null
    );

    g_test_add_func(
        "/evidence_record/copies_strings",
        test_evidence_record_copies_strings
    );

    g_test_add_func(
        "/evidence_record/trims_text",
        test_evidence_record_trims_text
    );

    g_test_add_func(
        "/evidence_record/invalid_uuid",
        test_evidence_record_invalid_uuid
    );

    g_test_add_func(
        "/evidence_record/invalid_original_name",
        test_evidence_record_invalid_original_name
    );

    g_test_add_func(
        "/evidence_record/invalid_internal_name",
        test_evidence_record_invalid_internal_name
    );

    g_test_add_func(
        "/evidence_record/absolute_path",
        test_evidence_record_absolute_path
    );

    g_test_add_func(
        "/evidence_record/parent_component",
        test_evidence_record_parent_component
    );

    g_test_add_func(
        "/evidence_record/relative_path",
        test_evidence_record_relative_path
    );

    g_test_add_func(
        "/evidence_record/empty_type",
        test_evidence_record_empty_type
    );

    g_test_add_func(
        "/evidence_record/short_sha256",
        test_evidence_record_short_sha256
    );

    g_test_add_func(
        "/evidence_record/invalid_sha256_character",
        test_evidence_record_invalid_sha256_character
    );

    g_test_add_func(
        "/evidence_record/uppercase_sha256",
        test_evidence_record_uppercase_sha256
    );

    g_test_add_func(
        "/evidence_record/invalid_imported_at",
        test_evidence_record_invalid_imported_at
    );

    g_test_add_func(
        "/evidence_record/invalid_collected_at",
        test_evidence_record_invalid_collected_at
    );

    g_test_add_func(
        "/evidence_record/invalid_status",
        test_evidence_record_invalid_status
    );

    g_test_add_func(
        "/evidence_record/zero_size",
        test_evidence_record_zero_size
    );

    g_test_add_func(
        "/evidence_record/null_accessors",
        test_evidence_record_null_accessors
    );

    g_test_add_func(
        "/evidence_record/free_null",
        test_evidence_record_free_null
    );

    g_test_add_func(
        "/evidence_record/optional_error",
        test_evidence_record_optional_error
    );

    return g_test_run();
}

