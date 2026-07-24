/******************************************************************************
 * @file test_controlled_vocab.c
 * @brief Tests unitaires du vocabulaire contrôlé.
 ******************************************************************************/
#include "core/controlled_vocab.h"
#include <glib.h>

static void test_verification_statuses(void)
{
    size_t count = 0;
    const ControlledVocabItem *items = controlled_vocab_get_items(CONTROLLED_VOCAB_VERIFICATION_STATUS, &count);
    g_assert_nonnull(items);
    g_assert_cmpuint(count, ==, 5);

    g_assert_true(controlled_vocab_is_valid_verification_status("proposed"));
    g_assert_true(controlled_vocab_is_valid_verification_status("confirmed"));
    g_assert_true(controlled_vocab_is_valid_verification_status("rejected"));
    g_assert_true(controlled_vocab_is_valid_verification_status("conflicted"));
    g_assert_true(controlled_vocab_is_valid_verification_status("invalid"));

    g_assert_false(controlled_vocab_is_valid_verification_status("unknown_status"));
    g_assert_false(controlled_vocab_is_valid_verification_status(NULL));
    g_assert_false(controlled_vocab_is_valid_verification_status(""));

    g_assert_cmpstr(controlled_vocab_get_label(CONTROLLED_VOCAB_VERIFICATION_STATUS, "proposed"), ==, "Proposé");
    g_assert_cmpstr(controlled_vocab_get_code_from_label(CONTROLLED_VOCAB_VERIFICATION_STATUS, "Proposé"), ==, "proposed");
}

static void test_provenance_kinds(void)
{
    size_t count = 0;
    const ControlledVocabItem *items = controlled_vocab_get_items(CONTROLLED_VOCAB_PROVENANCE_KIND, &count);
    g_assert_nonnull(items);
    g_assert_cmpuint(count, ==, 6);

    g_assert_true(controlled_vocab_is_valid_provenance_kind("observed"));
    g_assert_true(controlled_vocab_is_valid_provenance_kind("ocr"));
    g_assert_true(controlled_vocab_is_valid_provenance_kind("header"));
    g_assert_true(controlled_vocab_is_valid_provenance_kind("metadata"));
    g_assert_true(controlled_vocab_is_valid_provenance_kind("derived"));
    g_assert_true(controlled_vocab_is_valid_provenance_kind("manual"));

    g_assert_false(controlled_vocab_is_valid_provenance_kind("invalid_kind"));
}

static void test_email_roles(void)
{
    size_t count = 0;
    const ControlledVocabItem *items = controlled_vocab_get_items(CONTROLLED_VOCAB_EMAIL_ROLE, &count);
    g_assert_nonnull(items);
    g_assert_cmpuint(count, ==, 9);

    g_assert_true(controlled_vocab_is_valid_email_role("from"));
    g_assert_true(controlled_vocab_is_valid_email_role("reply_to"));
    g_assert_false(controlled_vocab_is_valid_email_role("custom_role"));
}

static void test_smtp_roles(void)
{
    size_t count = 0;
    const ControlledVocabItem *items = controlled_vocab_get_items(CONTROLLED_VOCAB_SMTP_ROLE, &count);
    g_assert_nonnull(items);
    g_assert_cmpuint(count, ==, 5);

    g_assert_true(controlled_vocab_is_valid_smtp_role("declared_source"));
    g_assert_true(controlled_vocab_is_valid_smtp_role("smtp_relay"));
    g_assert_false(controlled_vocab_is_valid_smtp_role("fraudster_ip"));
}

static void test_value_types(void)
{
    size_t count = 0;
    const ControlledVocabItem *items = controlled_vocab_get_items(CONTROLLED_VOCAB_VALUE_TYPE, &count);
    g_assert_nonnull(items);
    g_assert_cmpuint(count, ==, 14);

    g_assert_true(controlled_vocab_is_valid_value_type("iban"));
    g_assert_true(controlled_vocab_is_valid_value_type("email"));
    g_assert_false(controlled_vocab_is_valid_value_type("invalid_type"));
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/controlled-vocab/verification-status", test_verification_statuses);
    g_test_add_func("/controlled-vocab/provenance-kinds", test_provenance_kinds);
    g_test_add_func("/controlled-vocab/email-roles", test_email_roles);
    g_test_add_func("/controlled-vocab/smtp-roles", test_smtp_roles);
    g_test_add_func("/controlled-vocab/value-types", test_value_types);
    return g_test_run();
}
