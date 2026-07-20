/******************************************************************************
 * @file test_evidence_type.c
 * @brief Tests unitaires du modèle EvidenceType.
 ******************************************************************************/

#include "models/evidence_type.h"

#include <glib.h>

/**
 * @brief Vérifie la création d’un type valide.
 */
static void test_evidence_type_new_valid(void)
{
    EvidenceType *evidence_type = NULL;
    GError *error = NULL;

    evidence_type = evidence_type_new(
        4,
        "document",
        "Document",
        "Document numérique ou numérisé.",
        &error
    );

    g_assert_no_error(error);
    g_assert_nonnull(evidence_type);

    g_assert_cmpint(
        evidence_type_get_identifier(evidence_type),
        ==,
        4
    );

    g_assert_cmpstr(
        evidence_type_get_code(evidence_type),
        ==,
        "document"
    );

    g_assert_cmpstr(
        evidence_type_get_label(evidence_type),
        ==,
        "Document"
    );

    g_assert_cmpstr(
        evidence_type_get_description(evidence_type),
        ==,
        "Document numérique ou numérisé."
    );

    evidence_type_free(evidence_type);
}

/**
 * @brief Vérifie la suppression des espaces périphériques.
 */
static void test_evidence_type_values_are_trimmed(void)
{
    EvidenceType *evidence_type = NULL;
    GError *error = NULL;

    evidence_type = evidence_type_new(
        1,
        "  screenshot  ",
        "  Capture d’écran  ",
        "  Image capturée depuis un écran.  ",
        &error
    );

    g_assert_no_error(error);
    g_assert_nonnull(evidence_type);

    g_assert_cmpstr(
        evidence_type_get_code(evidence_type),
        ==,
        "screenshot"
    );

    g_assert_cmpstr(
        evidence_type_get_label(evidence_type),
        ==,
        "Capture d’écran"
    );

    g_assert_cmpstr(
        evidence_type_get_description(evidence_type),
        ==,
        "Image capturée depuis un écran."
    );

    evidence_type_free(evidence_type);
}

/**
 * @brief Vérifie qu’une description absente reste facultative.
 */
static void test_evidence_type_description_is_optional(void)
{
    EvidenceType *evidence_type = NULL;
    GError *error = NULL;

    evidence_type = evidence_type_new(
        9,
        "other",
        "Autre",
        NULL,
        &error
    );

    g_assert_no_error(error);
    g_assert_nonnull(evidence_type);

    g_assert_null(
        evidence_type_get_description(evidence_type)
    );

    evidence_type_free(evidence_type);
}

/**
 * @brief Vérifie qu’une description vide est normalisée vers NULL.
 */
static void test_evidence_type_empty_description_becomes_null(void)
{
    EvidenceType *evidence_type = NULL;
    GError *error = NULL;

    evidence_type = evidence_type_new(
        8,
        "text",
        "Texte",
        "     ",
        &error
    );

    g_assert_no_error(error);
    g_assert_nonnull(evidence_type);

    g_assert_null(
        evidence_type_get_description(evidence_type)
    );

    evidence_type_free(evidence_type);
}

/**
 * @brief Vérifie le refus d’un identifiant invalide.
 */
static void test_evidence_type_invalid_identifier(void)
{
    EvidenceType *evidence_type = NULL;
    GError *error = NULL;

    evidence_type = evidence_type_new(
        0,
        "document",
        "Document",
        NULL,
        &error
    );

    g_assert_null(evidence_type);

    g_assert_error(
        error,
        EVIDENCE_TYPE_ERROR,
        EVIDENCE_TYPE_ERROR_INVALID_IDENTIFIER
    );

    g_clear_error(&error);
}

/**
 * @brief Vérifie le refus d’un code absent.
 */
static void test_evidence_type_missing_code(void)
{
    EvidenceType *evidence_type = NULL;
    GError *error = NULL;

    evidence_type = evidence_type_new(
        4,
        NULL,
        "Document",
        NULL,
        &error
    );

    g_assert_null(evidence_type);

    g_assert_error(
        error,
        EVIDENCE_TYPE_ERROR,
        EVIDENCE_TYPE_ERROR_INVALID_CODE
    );

    g_clear_error(&error);
}

/**
 * @brief Vérifie le refus d’un code vide.
 */
static void test_evidence_type_empty_code(void)
{
    EvidenceType *evidence_type = NULL;
    GError *error = NULL;

    evidence_type = evidence_type_new(
        4,
        "     ",
        "Document",
        NULL,
        &error
    );

    g_assert_null(evidence_type);

    g_assert_error(
        error,
        EVIDENCE_TYPE_ERROR,
        EVIDENCE_TYPE_ERROR_INVALID_CODE
    );

    g_clear_error(&error);
}

/**
 * @brief Vérifie le refus d’un libellé absent.
 */
static void test_evidence_type_missing_label(void)
{
    EvidenceType *evidence_type = NULL;
    GError *error = NULL;

    evidence_type = evidence_type_new(
        4,
        "document",
        NULL,
        NULL,
        &error
    );

    g_assert_null(evidence_type);

    g_assert_error(
        error,
        EVIDENCE_TYPE_ERROR,
        EVIDENCE_TYPE_ERROR_INVALID_LABEL
    );

    g_clear_error(&error);
}

/**
 * @brief Vérifie le comportement des getters avec NULL.
 */
static void test_evidence_type_null_getters(void)
{
    g_assert_cmpint(
        evidence_type_get_identifier(NULL),
        ==,
        0
    );

    g_assert_null(
        evidence_type_get_code(NULL)
    );

    g_assert_null(
        evidence_type_get_label(NULL)
    );

    g_assert_null(
        evidence_type_get_description(NULL)
    );

    evidence_type_free(NULL);
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
        "/evidence-type/new-valid",
        test_evidence_type_new_valid
    );

    g_test_add_func(
        "/evidence-type/values-are-trimmed",
        test_evidence_type_values_are_trimmed
    );

    g_test_add_func(
        "/evidence-type/description-is-optional",
        test_evidence_type_description_is_optional
    );

    g_test_add_func(
        "/evidence-type/empty-description-becomes-null",
        test_evidence_type_empty_description_becomes_null
    );

    g_test_add_func(
        "/evidence-type/invalid-identifier",
        test_evidence_type_invalid_identifier
    );

    g_test_add_func(
        "/evidence-type/missing-code",
        test_evidence_type_missing_code
    );

    g_test_add_func(
        "/evidence-type/empty-code",
        test_evidence_type_empty_code
    );

    g_test_add_func(
        "/evidence-type/missing-label",
        test_evidence_type_missing_label
    );

    g_test_add_func(
        "/evidence-type/null-getters",
        test_evidence_type_null_getters
    );

    return g_test_run();
}
