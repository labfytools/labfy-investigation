/******************************************************************************
 * @file test_relation_record.c
 * @brief Tests unitaires du modèle RelationRecord.
 ******************************************************************************/

#include "models/relation_record.h"

#include <glib.h>

#define TEST_RELATION_IDENTIFIER \
    "30000000-0000-4000-8000-000000000001"

#define TEST_SOURCE_IDENTIFIER \
    "10000000-0000-4000-8000-000000000001"

#define TEST_TARGET_IDENTIFIER \
    "20000000-0000-4000-8000-000000000001"

typedef struct
{
    const char *identifier;
    const char *source_entity_identifier;
    const char *target_entity_identifier;
    const char *relation_type;
    const char *label;
    const char *justification;

    gint confidence;

    const char *created_at;
    const char *updated_at;

    RelationStatus status;
} TestRelationRecordInput;

static TestRelationRecordInput test_relation_record_valid_input(void)
{
    return (TestRelationRecordInput)
    {
        .identifier = TEST_RELATION_IDENTIFIER,
        .source_entity_identifier = TEST_SOURCE_IDENTIFIER,
        .target_entity_identifier = TEST_TARGET_IDENTIFIER,
        .relation_type = "uses",
        .label = "Utilise cette adresse",
        .justification = "Association observée dans la conversation.",
        .confidence = 80,
        .created_at = "2026-07-20T20:00:00Z",
        .updated_at = "2026-07-20T20:00:00Z",
        .status = RELATION_STATUS_ACTIVE
    };
}

static RelationRecord *test_relation_record_create(
    const TestRelationRecordInput *input,
    GError **error
)
{
    g_assert_nonnull(
        input
    );

    return relation_record_new(
        input->identifier,
        input->source_entity_identifier,
        input->target_entity_identifier,
        input->relation_type,
        input->label,
        input->justification,
        input->confidence,
        input->created_at,
        input->updated_at,
        input->status,
        error
    );
}

static void test_relation_record_assert_invalid(
    const TestRelationRecordInput *input,
    RelationRecordError expected_error
)
{
    RelationRecord *relation_record =
        NULL;

    GError *error =
        NULL;

    relation_record =
        test_relation_record_create(
            input,
            &error
        );

    g_assert_null(
        relation_record
    );

    g_assert_error(
        error,
        RELATION_RECORD_ERROR,
        (gint) expected_error
    );

    g_clear_error(
        &error
    );
}

static void test_relation_record_valid_full(void)
{
    TestRelationRecordInput input =
        test_relation_record_valid_input();

    RelationRecord *relation_record =
        NULL;

    GError *error =
        NULL;

    relation_record =
        test_relation_record_create(
            &input,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        relation_record
    );

    g_assert_cmpstr(
        relation_record_get_identifier(relation_record),
        ==,
        TEST_RELATION_IDENTIFIER
    );

    g_assert_cmpstr(
        relation_record_get_source_entity_identifier(relation_record),
        ==,
        TEST_SOURCE_IDENTIFIER
    );

    g_assert_cmpstr(
        relation_record_get_target_entity_identifier(relation_record),
        ==,
        TEST_TARGET_IDENTIFIER
    );

    g_assert_cmpstr(
        relation_record_get_relation_type(relation_record),
        ==,
        "uses"
    );

    g_assert_cmpstr(
        relation_record_get_label(relation_record),
        ==,
        "Utilise cette adresse"
    );

    g_assert_cmpstr(
        relation_record_get_justification(relation_record),
        ==,
        "Association observée dans la conversation."
    );

    g_assert_cmpint(
        relation_record_get_confidence(relation_record),
        ==,
        80
    );

    g_assert_cmpstr(
        relation_record_get_created_at(relation_record),
        ==,
        "2026-07-20T20:00:00Z"
    );

    g_assert_cmpstr(
        relation_record_get_updated_at(relation_record),
        ==,
        "2026-07-20T20:00:00Z"
    );

    g_assert_cmpint(
        relation_record_get_status(relation_record),
        ==,
        RELATION_STATUS_ACTIVE
    );

    relation_record_free(
        relation_record
    );
}

static void test_relation_record_optional_fields(void)
{
    TestRelationRecordInput input =
        test_relation_record_valid_input();

    RelationRecord *relation_record =
        NULL;

    GError *error =
        NULL;

    input.label = "   ";
    input.justification = NULL;

    relation_record =
        test_relation_record_create(
            &input,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_null(
        relation_record_get_label(
            relation_record
        )
    );

    g_assert_null(
        relation_record_get_justification(
            relation_record
        )
    );

    relation_record_free(
        relation_record
    );
}

static void test_relation_record_values_are_trimmed(void)
{
    TestRelationRecordInput input =
        test_relation_record_valid_input();

    RelationRecord *relation_record =
        NULL;

    GError *error =
        NULL;

    input.identifier =
        "  30000000-0000-4000-8000-000000000001  ";

    input.source_entity_identifier =
        "  10000000-0000-4000-8000-000000000001  ";

    input.target_entity_identifier =
        "\t20000000-0000-4000-8000-000000000001\t";

    input.relation_type =
        "  controls  ";

    input.label =
        "  Contrôle  ";

    input.justification =
        "  Source documentaire.  ";

    input.created_at =
        "  2026-07-20T20:00:00Z  ";

    input.updated_at =
        "\t2026-07-20T20:00:00Z\t";

    relation_record =
        test_relation_record_create(
            &input,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_cmpstr(
        relation_record_get_relation_type(relation_record),
        ==,
        "controls"
    );

    g_assert_cmpstr(
        relation_record_get_label(relation_record),
        ==,
        "Contrôle"
    );

    g_assert_cmpstr(
        relation_record_get_justification(relation_record),
        ==,
        "Source documentaire."
    );

    relation_record_free(
        relation_record
    );
}

static void test_relation_record_invalid_identifier(void)
{
    TestRelationRecordInput input =
        test_relation_record_valid_input();

    input.identifier = "relation-invalide";

    test_relation_record_assert_invalid(
        &input,
        RELATION_RECORD_ERROR_INVALID_IDENTIFIER
    );
}

static void test_relation_record_invalid_source_identifier(void)
{
    TestRelationRecordInput input =
        test_relation_record_valid_input();

    input.source_entity_identifier = "source-invalide";

    test_relation_record_assert_invalid(
        &input,
        RELATION_RECORD_ERROR_INVALID_SOURCE_IDENTIFIER
    );
}

static void test_relation_record_invalid_target_identifier(void)
{
    TestRelationRecordInput input =
        test_relation_record_valid_input();

    input.target_entity_identifier = "cible-invalide";

    test_relation_record_assert_invalid(
        &input,
        RELATION_RECORD_ERROR_INVALID_TARGET_IDENTIFIER
    );
}

static void test_relation_record_identical_entities(void)
{
    TestRelationRecordInput input =
        test_relation_record_valid_input();

    input.target_entity_identifier =
        TEST_SOURCE_IDENTIFIER;

    test_relation_record_assert_invalid(
        &input,
        RELATION_RECORD_ERROR_IDENTICAL_ENTITIES
    );
}

static void test_relation_record_empty_type(void)
{
    TestRelationRecordInput input =
        test_relation_record_valid_input();

    input.relation_type = " \t ";

    test_relation_record_assert_invalid(
        &input,
        RELATION_RECORD_ERROR_INVALID_TYPE
    );
}

static void test_relation_record_confidence_below_range(void)
{
    TestRelationRecordInput input =
        test_relation_record_valid_input();

    input.confidence = -1;

    test_relation_record_assert_invalid(
        &input,
        RELATION_RECORD_ERROR_INVALID_CONFIDENCE
    );
}

static void test_relation_record_confidence_above_range(void)
{
    TestRelationRecordInput input =
        test_relation_record_valid_input();

    input.confidence = 101;

    test_relation_record_assert_invalid(
        &input,
        RELATION_RECORD_ERROR_INVALID_CONFIDENCE
    );
}

static void test_relation_record_invalid_created_at(void)
{
    TestRelationRecordInput input =
        test_relation_record_valid_input();

    input.created_at = "2026-02-30T20:00:00Z";

    test_relation_record_assert_invalid(
        &input,
        RELATION_RECORD_ERROR_INVALID_DATE
    );
}

static void test_relation_record_invalid_updated_at(void)
{
    TestRelationRecordInput input =
        test_relation_record_valid_input();

    input.updated_at = "2026-07-20 20:00:00Z";

    test_relation_record_assert_invalid(
        &input,
        RELATION_RECORD_ERROR_INVALID_DATE
    );
}

static void test_relation_record_valid_statuses(void)
{
    RelationStatus statuses[] =
    {
        RELATION_STATUS_ACTIVE,
        RELATION_STATUS_ARCHIVED,
        RELATION_STATUS_DELETED,
        RELATION_STATUS_DISPUTED
    };

    guint status_index =
        0;

    for (status_index = 0;
         status_index < G_N_ELEMENTS(statuses);
         status_index++)
    {
        TestRelationRecordInput input =
            test_relation_record_valid_input();

        RelationRecord *relation_record =
            NULL;

        GError *error =
            NULL;

        input.status =
            statuses[status_index];

        relation_record =
            test_relation_record_create(
                &input,
                &error
            );

        g_assert_no_error(
            error
        );

        g_assert_nonnull(
            relation_record
        );

        g_assert_cmpint(
            relation_record_get_status(relation_record),
            ==,
            statuses[status_index]
        );

        relation_record_free(
            relation_record
        );
    }
}

static void test_relation_record_invalid_status(void)
{
    TestRelationRecordInput input =
        test_relation_record_valid_input();

    input.status = RELATION_STATUS_UNKNOWN;

    test_relation_record_assert_invalid(
        &input,
        RELATION_RECORD_ERROR_INVALID_STATUS
    );

    input.status = (RelationStatus) 999;

    test_relation_record_assert_invalid(
        &input,
        RELATION_RECORD_ERROR_INVALID_STATUS
    );
}

static void test_relation_record_null_accessors(void)
{
    g_assert_null(
        relation_record_get_identifier(NULL)
    );

    g_assert_null(
        relation_record_get_source_entity_identifier(NULL)
    );

    g_assert_null(
        relation_record_get_target_entity_identifier(NULL)
    );

    g_assert_null(
        relation_record_get_relation_type(NULL)
    );

    g_assert_null(
        relation_record_get_label(NULL)
    );

    g_assert_null(
        relation_record_get_justification(NULL)
    );

    g_assert_cmpint(
        relation_record_get_confidence(NULL),
        ==,
        0
    );

    g_assert_null(
        relation_record_get_created_at(NULL)
    );

    g_assert_null(
        relation_record_get_updated_at(NULL)
    );

    g_assert_cmpint(
        relation_record_get_status(NULL),
        ==,
        RELATION_STATUS_UNKNOWN
    );

    relation_record_free(
        NULL
    );
}

static void test_relation_record_optional_error(void)
{
    TestRelationRecordInput input =
        test_relation_record_valid_input();

    RelationRecord *relation_record =
        NULL;

    input.identifier = "invalide";

    relation_record =
        test_relation_record_create(
            &input,
            NULL
        );

    g_assert_null(
        relation_record
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
        "/relation-record/valid-full",
        test_relation_record_valid_full
    );

    g_test_add_func(
        "/relation-record/optional-fields",
        test_relation_record_optional_fields
    );

    g_test_add_func(
        "/relation-record/values-are-trimmed",
        test_relation_record_values_are_trimmed
    );

    g_test_add_func(
        "/relation-record/invalid-identifier",
        test_relation_record_invalid_identifier
    );

    g_test_add_func(
        "/relation-record/invalid-source-identifier",
        test_relation_record_invalid_source_identifier
    );

    g_test_add_func(
        "/relation-record/invalid-target-identifier",
        test_relation_record_invalid_target_identifier
    );

    g_test_add_func(
        "/relation-record/identical-entities",
        test_relation_record_identical_entities
    );

    g_test_add_func(
        "/relation-record/empty-type",
        test_relation_record_empty_type
    );

    g_test_add_func(
        "/relation-record/confidence-below-range",
        test_relation_record_confidence_below_range
    );

    g_test_add_func(
        "/relation-record/confidence-above-range",
        test_relation_record_confidence_above_range
    );

    g_test_add_func(
        "/relation-record/invalid-created-at",
        test_relation_record_invalid_created_at
    );

    g_test_add_func(
        "/relation-record/invalid-updated-at",
        test_relation_record_invalid_updated_at
    );

    g_test_add_func(
        "/relation-record/valid-statuses",
        test_relation_record_valid_statuses
    );

    g_test_add_func(
        "/relation-record/invalid-status",
        test_relation_record_invalid_status
    );

    g_test_add_func(
        "/relation-record/null-accessors",
        test_relation_record_null_accessors
    );

    g_test_add_func(
        "/relation-record/optional-error",
        test_relation_record_optional_error
    );

    return g_test_run();
}
