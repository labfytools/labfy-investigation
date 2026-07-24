/******************************************************************************
 * @file test_bank_proposal.c
 * @brief Tests unitaires du modèle bancaire et de l'analyse d'IBAN / RIB / BIC.
 ******************************************************************************/
#include "core/bank_proposal.h"
#include <glib.h>

static void test_iban_validation(void)
{
    /* IBAN français synthétique avec MOD-97 et RIB valide */
    const char *valid_fr = "FR4830002005500000000000052";
    g_assert_true(bank_proposal_validate_iban(valid_fr));



    /* IBAN invalide (mauvaise clé) */
    const char *invalid_fr = "FR0030002005500000000000021";
    g_assert_false(bank_proposal_validate_iban(invalid_fr));

    /* IBAN trop court */
    g_assert_false(bank_proposal_validate_iban("FR763000"));
}

static void test_bic_validation(void)
{
    g_assert_true(bank_proposal_validate_bic("BNPAFRPPXXX"));
    g_assert_true(bank_proposal_validate_bic("BNPAFRPP"));
    g_assert_false(bank_proposal_validate_bic("BNPAFR"));
    g_assert_false(bank_proposal_validate_bic("INVALID BIC!"));
}

static void test_french_rib_derivation(void)
{
    const char *valid_fr = "FR4830002005500000000000052";
    BankProposal *proposal = bank_proposal_analyze_text(valid_fr, "evidence-123");
    g_assert_nonnull(proposal);
    g_assert_true(proposal->is_iban_valid);
    g_assert_true(proposal->is_derived_bban);
    g_assert_cmpstr(proposal->country_code, ==, "FR");
    g_assert_cmpstr(proposal->bank_code, ==, "30002");
    g_assert_cmpstr(proposal->branch_code, ==, "00550");
    g_assert_cmpstr(proposal->account_number, ==, "00000000000");
    g_assert_cmpstr(proposal->rib_key, ==, "52");
    g_assert_cmpstr(proposal->evidence_id, ==, "evidence-123");
    g_assert_cmpstr(proposal->verification_status, ==, "proposed");

    bank_proposal_free(proposal);
}


int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/bank-proposal/iban-validation", test_iban_validation);
    g_test_add_func("/bank-proposal/bic-validation", test_bic_validation);
    g_test_add_func("/bank-proposal/french-rib-derivation", test_french_rib_derivation);
    return g_test_run();
}
