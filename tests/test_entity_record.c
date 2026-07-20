/******************************************************************************
 * @file test_entity_record.c
 * @brief Tests unitaires du modèle EntityRecord.
 ******************************************************************************/

#include "models/entity_record.h"

#include <glib.h>

#define TEST_ENTITY_IDENTIFIER \
    "9b6ea3f8-b60b-48ac-908a-94c211f524dd"

typedef struct
{
    const char *identifier;
    const char *type_identifier;
    const char *value;
    const char *label;
    const char *description;

    gint confidence;

    const char *created_at;
    const char *updated_at;

    EntityStatus status;
} TestEntityRecordInput;

static TestEntityRecordInput test_entity_record_valid_input(void)
{
    return (TestEntityRecordInput)
    {
        .identifier = TEST_ENTITY_IDENTIFIER,
        .type_identifier = "email_address",
        .value = "suspect@example.org",
        .label = "Adresse utilisée pour la transaction",
        .description = "Adresse communiquée dans la conversation.",
        .confidence = 80,
        .created_at = "2026-07-20T19:00:00Z",
        .updated_at = "2026-07-20T19:00:00Z",
        .status = ENTITY_STATUS_ACTIVE
    };
}

static EntityRecord *test_entity_record_create(
    const TestEntityRecordInput *input,
    GError **error
)
{
    g_assert_nonnull(
        input
    );

    return entity_record_new(
        input->identifier,
        input->type_identifier,
        input->value,
        input->label,
        input->description,
        input->confidence,
        input->created_at,
        input->updated_at,
        input->status,
        error
    );
}

static void test_entity_record_assert_invalid(
    const TestEntityRecordInput *input,
    EntityRecordError expected_error
)
{
    EntityRecord *entity_record =
        NULL;

    GError *error =
        NULL;

    entity_record =
        test_entity_record_create(
            input,
            &error
        );

    g_assert_null(
        entity_record
    );

    g_assert_error(
        error,
        ENTITY_RECORD_ERROR,
        (gint) expected_error
    );

    g_clear_error(
        &error
    );
}

static void test_entity_record_valid_full(void)
{
    TestEntityRecordInput input =
        test_entity_record_valid_input();

    EntityRecord *entity_record =
        NULL;

    GError *error =
        NULL;

    entity_record =
        test_entity_record_create(
            &input,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        entity_record
    );

    g_assert_cmpstr(
        entity_record_get_identifier(entity_record),
        ==,
        TEST_ENTITY_IDENTIFIER
    );

    g_assert_cmpstr(
        entity_record_get_type_identifier(entity_record),
        ==,
        "email_address"
    );

    g_assert_cmpstr(
        entity_record_get_value(entity_record),
        ==,
        "suspect@example.org"
    );

    g_assert_cmpstr(
        entity_record_get_label(entity_record),
        ==,
        "Adresse utilisée pour la transaction"
    );

    g_assert_cmpstr(
        entity_record_get_description(entity_record),
        ==,
        "Adresse communiquée dans la conversation."
    );

    g_assert_cmpint(
        entity_record_get_confidence(entity_record),
        ==,
        80
    );

    g_assert_cmpstr(
        entity_record_get_created_at(entity_record),
        ==,
        "2026-07-20T19:00:00Z"
    );

    g_assert_cmpstr(
        entity_record_get_updated_at(entity_record),
        ==,
        "2026-07-20T19:00:00Z"
    );

    g_assert_cmpint(
        entity_record_get_status(entity_record),
        ==,
        ENTITY_STATUS_ACTIVE
    );

    entity_record_free(
        entity_record
    );
}

static void test_entity_record_optional_fields(void)
{
    TestEntityRecordInput input =
        test_entity_record_valid_input();

    EntityRecord *entity_record =
        NULL;

    GError *error =
        NULL;

    input.label = "   ";
    input.description = NULL;

    entity_record =
        test_entity_record_create(
            &input,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        entity_record
    );

    g_assert_null(
        entity_record_get_label(
            entity_record
        )
    );

    g_assert_null(
        entity_record_get_description(
            entity_record
        )
    );

    entity_record_free(
        entity_record
    );
}

static void test_entity_record_values_are_trimmed(void)
{
    TestEntityRecordInput input =
        test_entity_record_valid_input();

    EntityRecord *entity_record =
        NULL;

    GError *error =
        NULL;

    input.identifier =
        "  9b6ea3f8-b60b-48ac-908a-94c211f524dd  ";

    input.type_identifier =
        "  iban  ";

    input.value =
        "  FR7612345678901234567890123  ";

    input.label =
        "  Compte destinataire  ";

    input.description =
        "  IBAN communiqué par le vendeur.  ";

    input.created_at =
        "  2026-07-20T19:00:00Z  ";

    input.updated_at =
        "\t2026-07-20T19:00:00Z\t";

    entity_record =
        test_entity_record_create(
            &input,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_cmpstr(
        entity_record_get_type_identifier(entity_record),
        ==,
        "iban"
    );

    g_assert_cmpstr(
        entity_record_get_value(entity_record),
        ==,
        "FR7612345678901234567890123"
    );

    g_assert_cmpstr(
        entity_record_get_label(entity_record),
        ==,
        "Compte destinataire"
    );

    entity_record_free(
        entity_record
    );
}

static void test_entity_record_invalid_uuid(void)
{
    TestEntityRecordInput input =
        test_entity_record_valid_input();

    input.identifier = "uuid-invalide";

    test_entity_record_assert_invalid(
        &input,
        ENTITY_RECORD_ERROR_INVALID_IDENTIFIER
    );
}

static void test_entity_record_missing_type(void)
{
    TestEntityRecordInput input =
        test_entity_record_valid_input();

    input.type_identifier = "   ";

    test_entity_record_assert_invalid(
        &input,
        ENTITY_RECORD_ERROR_INVALID_TYPE
    );
}

static void test_entity_record_empty_value(void)
{
    TestEntityRecordInput input =
        test_entity_record_valid_input();

    input.value = "\t";

    test_entity_record_assert_invalid(
        &input,
        ENTITY_RECORD_ERROR_INVALID_VALUE
    );
}

static void test_entity_record_confidence_below_range(void)
{
    TestEntityRecordInput input =
        test_entity_record_valid_input();

    input.confidence = -1;

    test_entity_record_assert_invalid(
        &input,
        ENTITY_RECORD_ERROR_INVALID_CONFIDENCE
    );
}

static void test_entity_record_confidence_above_range(void)
{
    TestEntityRecordInput input =
        test_entity_record_valid_input();

    input.confidence = 101;

    test_entity_record_assert_invalid(
        &input,
        ENTITY_RECORD_ERROR_INVALID_CONFIDENCE
    );
}

static void test_entity_record_invalid_created_at(void)
{
    TestEntityRecordInput input =
        test_entity_record_valid_input();

    input.created_at = "2026-02-30T19:00:00Z";

    test_entity_record_assert_invalid(
        &input,
        ENTITY_RECORD_ERROR_INVALID_DATE
    );
}

static void test_entity_record_invalid_updated_at(void)
{
    TestEntityRecordInput input =
        test_entity_record_valid_input();

    input.updated_at = "2026-07-20 19:00:00Z";

    test_entity_record_assert_invalid(
        &input,
        ENTITY_RECORD_ERROR_INVALID_DATE
    );
}

static void test_entity_record_valid_statuses(void)
{
    EntityStatus statuses[] =
    {
        ENTITY_STATUS_ACTIVE,
        ENTITY_STATUS_ARCHIVED,
        ENTITY_STATUS_DELETED
    };

    guint status_index =
        0;

    for (status_index = 0;
         status_index < G_N_ELEMENTS(statuses);
         status_index++)
    {
        TestEntityRecordInput input =
            test_entity_record_valid_input();

        EntityRecord *entity_record =
            NULL;

        GError *error =
            NULL;

        input.status =
            statuses[status_index];

        entity_record =
            test_entity_record_create(
                &input,
                &error
            );

        g_assert_no_error(
            error
        );

        g_assert_nonnull(
            entity_record
        );

        g_assert_cmpint(
            entity_record_get_status(entity_record),
            ==,
            statuses[status_index]
        );

        entity_record_free(
            entity_record
        );
    }
}

static void test_entity_record_invalid_status(void)
{
    TestEntityRecordInput input =
        test_entity_record_valid_input();

    input.status = ENTITY_STATUS_UNKNOWN;

    test_entity_record_assert_invalid(
        &input,
        ENTITY_RECORD_ERROR_INVALID_STATUS
    );

    input.status = (EntityStatus) 999;

    test_entity_record_assert_invalid(
        &input,
        ENTITY_RECORD_ERROR_INVALID_STATUS
    );
}

static void test_entity_record_null_accessors(void)
{
    g_assert_null(
        entity_record_get_identifier(NULL)
    );

    g_assert_null(
        entity_record_get_type_identifier(NULL)
    );

    g_assert_null(
        entity_record_get_value(NULL)
    );

    g_assert_null(
        entity_record_get_label(NULL)
    );

    g_assert_null(
        entity_record_get_description(NULL)
    );

    g_assert_cmpint(
        entity_record_get_confidence(NULL),
        ==,
        0
    );

    g_assert_null(
        entity_record_get_created_at(NULL)
    );

    g_assert_null(
        entity_record_get_updated_at(NULL)
    );

    g_assert_cmpint(
        entity_record_get_status(NULL),
        ==,
        ENTITY_STATUS_UNKNOWN
    );

    entity_record_free(
        NULL
    );
}

static void test_entity_record_optional_error(void)
{
    TestEntityRecordInput input =
        test_entity_record_valid_input();

    EntityRecord *entity_record =
        NULL;

    input.identifier =
        "invalide";

    entity_record =
        test_entity_record_create(
            &input,
            NULL
        );

    g_assert_null(
        entity_record
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
        "/entity-record/valid-full",
        test_entity_record_valid_full
    );

    g_test_add_func(
        "/entity-record/optional-fields",
        test_entity_record_optional_fields
    );

    g_test_add_func(
        "/entity-record/values-are-trimmed",
        test_entity_record_values_are_trimmed
    );

    g_test_add_func(
        "/entity-record/invalid-uuid",
        test_entity_record_invalid_uuid
    );

    g_test_add_func(
        "/entity-record/missing-type",
        test_entity_record_missing_type
    );

    g_test_add_func(
        "/entity-record/empty-value",
        test_entity_record_empty_value
    );

    g_test_add_func(
        "/entity-record/confidence-below-range",
        test_entity_record_confidence_below_range
    );

    g_test_add_func(
        "/entity-record/confidence-above-range",
        test_entity_record_confidence_above_range
    );

    g_test_add_func(
        "/entity-record/invalid-created-at",
        test_entity_record_invalid_created_at
    );

    g_test_add_func(
        "/entity-record/invalid-updated-at",
        test_entity_record_invalid_updated_at
    );

    g_test_add_func(
        "/entity-record/valid-statuses",
        test_entity_record_valid_statuses
    );

    g_test_add_func(
        "/entity-record/invalid-status",
        test_entity_record_invalid_status
    );

    g_test_add_func(
        "/entity-record/null-accessors",
        test_entity_record_null_accessors
    );

    g_test_add_func(
        "/entity-record/optional-error",
        test_entity_record_optional_error
    );

    return g_test_run();
}
