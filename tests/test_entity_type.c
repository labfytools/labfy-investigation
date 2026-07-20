/******************************************************************************
 * @file test_entity_type.c
 * @brief Tests unitaires du modèle EntityType.
 ******************************************************************************/

#include "models/entity_type.h"

#include <glib.h>

/**
 * @brief Vérifie la création d'un type valide.
 */
static void test_entity_type_new_valid(void)
{
    EntityType *entity_type =
        NULL;

    GError *error =
        NULL;

    entity_type =
        entity_type_new(
            3,
            "email",
            "Adresse e-mail",
            "Adresse électronique associée à l'enquête.",
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        entity_type
    );

    g_assert_cmpint(
        entity_type_get_identifier(
            entity_type
        ),
        ==,
        3
    );

    g_assert_cmpstr(
        entity_type_get_code(
            entity_type
        ),
        ==,
        "email"
    );

    g_assert_cmpstr(
        entity_type_get_label(
            entity_type
        ),
        ==,
        "Adresse e-mail"
    );

    g_assert_cmpstr(
        entity_type_get_description(
            entity_type
        ),
        ==,
        "Adresse électronique associée à l'enquête."
    );

    entity_type_free(
        entity_type
    );
}

/**
 * @brief Vérifie la suppression des espaces périphériques.
 */
static void test_entity_type_values_are_trimmed(void)
{
    EntityType *entity_type =
        NULL;

    GError *error =
        NULL;

    entity_type =
        entity_type_new(
            4,
            "  iban  ",
            "  IBAN  ",
            "  Identifiant international d'un compte bancaire.  ",
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        entity_type
    );

    g_assert_cmpstr(
        entity_type_get_code(
            entity_type
        ),
        ==,
        "iban"
    );

    g_assert_cmpstr(
        entity_type_get_label(
            entity_type
        ),
        ==,
        "IBAN"
    );

    g_assert_cmpstr(
        entity_type_get_description(
            entity_type
        ),
        ==,
        "Identifiant international d'un compte bancaire."
    );

    entity_type_free(
        entity_type
    );
}

/**
 * @brief Vérifie qu'une description absente reste facultative.
 */
static void test_entity_type_description_is_optional(void)
{
    EntityType *entity_type =
        NULL;

    GError *error =
        NULL;

    entity_type =
        entity_type_new(
            14,
            "other",
            "Autre",
            NULL,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        entity_type
    );

    g_assert_null(
        entity_type_get_description(
            entity_type
        )
    );

    entity_type_free(
        entity_type
    );
}

/**
 * @brief Vérifie qu'une description vide est normalisée vers NULL.
 */
static void test_entity_type_empty_description_becomes_null(void)
{
    EntityType *entity_type =
        NULL;

    GError *error =
        NULL;

    entity_type =
        entity_type_new(
            7,
            "person",
            "Personne",
            "     ",
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        entity_type
    );

    g_assert_null(
        entity_type_get_description(
            entity_type
        )
    );

    entity_type_free(
        entity_type
    );
}

/**
 * @brief Vérifie le refus d'un identifiant nul.
 */
static void test_entity_type_zero_identifier(void)
{
    EntityType *entity_type =
        NULL;

    GError *error =
        NULL;

    entity_type =
        entity_type_new(
            0,
            "email",
            "Adresse e-mail",
            NULL,
            &error
        );

    g_assert_null(
        entity_type
    );

    g_assert_error(
        error,
        ENTITY_TYPE_ERROR,
        ENTITY_TYPE_ERROR_INVALID_IDENTIFIER
    );

    g_clear_error(
        &error
    );
}

/**
 * @brief Vérifie le refus d'un identifiant négatif.
 */
static void test_entity_type_negative_identifier(void)
{
    EntityType *entity_type =
        NULL;

    GError *error =
        NULL;

    entity_type =
        entity_type_new(
            -1,
            "email",
            "Adresse e-mail",
            NULL,
            &error
        );

    g_assert_null(
        entity_type
    );

    g_assert_error(
        error,
        ENTITY_TYPE_ERROR,
        ENTITY_TYPE_ERROR_INVALID_IDENTIFIER
    );

    g_clear_error(
        &error
    );
}

/**
 * @brief Vérifie le refus d'un code absent.
 */
static void test_entity_type_missing_code(void)
{
    EntityType *entity_type =
        NULL;

    GError *error =
        NULL;

    entity_type =
        entity_type_new(
            3,
            NULL,
            "Adresse e-mail",
            NULL,
            &error
        );

    g_assert_null(
        entity_type
    );

    g_assert_error(
        error,
        ENTITY_TYPE_ERROR,
        ENTITY_TYPE_ERROR_INVALID_CODE
    );

    g_clear_error(
        &error
    );
}

/**
 * @brief Vérifie le refus d'un code vide.
 */
static void test_entity_type_empty_code(void)
{
    EntityType *entity_type =
        NULL;

    GError *error =
        NULL;

    entity_type =
        entity_type_new(
            3,
            "     ",
            "Adresse e-mail",
            NULL,
            &error
        );

    g_assert_null(
        entity_type
    );

    g_assert_error(
        error,
        ENTITY_TYPE_ERROR,
        ENTITY_TYPE_ERROR_INVALID_CODE
    );

    g_clear_error(
        &error
    );
}

/**
 * @brief Vérifie le refus d'un libellé absent.
 */
static void test_entity_type_missing_label(void)
{
    EntityType *entity_type =
        NULL;

    GError *error =
        NULL;

    entity_type =
        entity_type_new(
            3,
            "email",
            NULL,
            NULL,
            &error
        );

    g_assert_null(
        entity_type
    );

    g_assert_error(
        error,
        ENTITY_TYPE_ERROR,
        ENTITY_TYPE_ERROR_INVALID_LABEL
    );

    g_clear_error(
        &error
    );
}

/**
 * @brief Vérifie le refus d'un libellé vide.
 */
static void test_entity_type_empty_label(void)
{
    EntityType *entity_type =
        NULL;

    GError *error =
        NULL;

    entity_type =
        entity_type_new(
            3,
            "email",
            "     ",
            NULL,
            &error
        );

    g_assert_null(
        entity_type
    );

    g_assert_error(
        error,
        ENTITY_TYPE_ERROR,
        ENTITY_TYPE_ERROR_INVALID_LABEL
    );

    g_clear_error(
        &error
    );
}

/**
 * @brief Vérifie le comportement des getters avec NULL.
 */
static void test_entity_type_null_getters(void)
{
    g_assert_cmpint(
        entity_type_get_identifier(
            NULL
        ),
        ==,
        0
    );

    g_assert_null(
        entity_type_get_code(
            NULL
        )
    );

    g_assert_null(
        entity_type_get_label(
            NULL
        )
    );

    g_assert_null(
        entity_type_get_description(
            NULL
        )
    );

    entity_type_free(
        NULL
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
        "/entity-type/new-valid",
        test_entity_type_new_valid
    );

    g_test_add_func(
        "/entity-type/values-are-trimmed",
        test_entity_type_values_are_trimmed
    );

    g_test_add_func(
        "/entity-type/description-is-optional",
        test_entity_type_description_is_optional
    );

    g_test_add_func(
        "/entity-type/empty-description-becomes-null",
        test_entity_type_empty_description_becomes_null
    );

    g_test_add_func(
        "/entity-type/zero-identifier",
        test_entity_type_zero_identifier
    );

    g_test_add_func(
        "/entity-type/negative-identifier",
        test_entity_type_negative_identifier
    );

    g_test_add_func(
        "/entity-type/missing-code",
        test_entity_type_missing_code
    );

    g_test_add_func(
        "/entity-type/empty-code",
        test_entity_type_empty_code
    );

    g_test_add_func(
        "/entity-type/missing-label",
        test_entity_type_missing_label
    );

    g_test_add_func(
        "/entity-type/empty-label",
        test_entity_type_empty_label
    );

    g_test_add_func(
        "/entity-type/null-getters",
        test_entity_type_null_getters
    );

    return g_test_run();
}
