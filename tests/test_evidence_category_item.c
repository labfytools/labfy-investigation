/******************************************************************************
 * @file test_evidence_category_item.c
 * @brief Tests unitaires de EvidenceCategoryItem.
 ******************************************************************************/

#include "widgets/evidence_category_item.h"

#include <glib.h>

/**
 * @brief Crée une preuve métier valide.
 */
static EvidenceRecord *test_evidence_category_item_create_record(void)
{
    EvidenceRecord *evidence_record = NULL;

    GError *error = NULL;

    evidence_record =
        evidence_record_new(
            "550e8400-e29b-41d4-a716-446655440000",
            "capture-instagram.png",
            "550e8400-e29b-41d4-a716-446655440000.png",
            "01_Preuves_Originales/Documents/"
            "550e8400-e29b-41d4-a716-446655440000.png",
            "image",
            4096,
            "00000000000000000000000000000000"
            "00000000000000000000000000000000",
            "2026-07-20T07:16:01Z",
            "2026-07-19T18:30:00Z",
            "Conversation Instagram",
            "Capture d'écran reçue.",
            EVIDENCE_INTEGRITY_STATUS_UNKNOWN,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        evidence_record
    );

    return evidence_record;
}

/**
 * @brief Vérifie la création d'une catégorie vide.
 */
static void test_evidence_category_item_new_valid(void)
{
    EvidenceCategoryItem *evidence_category_item =
        NULL;

    evidence_category_item =
        evidence_category_item_new(
            " image ",
            " Images "
        );

    g_assert_nonnull(
        evidence_category_item
    );

    g_assert_cmpstr(
        evidence_category_item_get_type_identifier(
            evidence_category_item
        ),
        ==,
        "image"
    );

    g_assert_cmpstr(
        evidence_category_item_get_label(
            evidence_category_item
        ),
        ==,
        "Images"
    );

    g_assert_cmpuint(
        evidence_category_item_get_count(
            evidence_category_item
        ),
        ==,
        0
    );

    g_assert_nonnull(
        evidence_category_item_get_children(
            evidence_category_item
        )
    );

    g_assert_cmpuint(
        g_list_model_get_item_type(
            evidence_category_item_get_children(
                evidence_category_item
            )
        ),
        ==,
        EVIDENCE_TYPE_LIST_ITEM
    );

    g_object_unref(
        evidence_category_item
    );
}

/**
 * @brief Vérifie l'ajout et la propriété des preuves.
 */
static void test_evidence_category_item_append(void)
{
    EvidenceCategoryItem *evidence_category_item =
        NULL;

    EvidenceRecord *evidence_record =
        NULL;

    EvidenceListItem *evidence_list_item =
        NULL;

    EvidenceListItem *stored_item =
        NULL;

    evidence_category_item =
        evidence_category_item_new(
            "image",
            "Images"
        );

    evidence_record =
        test_evidence_category_item_create_record();

    evidence_list_item =
        evidence_list_item_new(
            evidence_record
        );

    g_assert_nonnull(
        evidence_category_item
    );

    g_assert_nonnull(
        evidence_list_item
    );

    g_assert_true(
        evidence_category_item_append(
            evidence_category_item,
            evidence_list_item
        )
    );

    /*
     * La catégorie possède maintenant sa propre référence.
     */
    g_object_unref(
        evidence_list_item
    );

    evidence_record_free(
        evidence_record
    );

    g_assert_cmpuint(
        evidence_category_item_get_count(
            evidence_category_item
        ),
        ==,
        1
    );

    stored_item =
        g_list_model_get_item(
            evidence_category_item_get_children(
                evidence_category_item
            ),
            0
        );

    g_assert_nonnull(
        stored_item
    );

    g_assert_cmpstr(
        evidence_list_item_get_identifier(
            stored_item
        ),
        ==,
        "550e8400-e29b-41d4-a716-446655440000"
    );

    g_assert_cmpstr(
        evidence_list_item_get_original_name(
            stored_item
        ),
        ==,
        "capture-instagram.png"
    );

    g_object_unref(
        stored_item
    );

    g_object_unref(
        evidence_category_item
    );
}

/**
 * @brief Vérifie les arguments invalides.
 */
static void test_evidence_category_item_invalid_arguments(void)
{
    EvidenceCategoryItem *evidence_category_item =
        NULL;

    g_assert_null(
        evidence_category_item_new(
            NULL,
            "Images"
        )
    );

    g_assert_null(
        evidence_category_item_new(
            "image",
            NULL
        )
    );

    g_assert_null(
        evidence_category_item_new(
            "   ",
            "Images"
        )
    );

    g_assert_null(
        evidence_category_item_new(
            "image",
            "   "
        )
    );

    evidence_category_item =
        evidence_category_item_new(
            "image",
            "Images"
        );

    g_assert_nonnull(
        evidence_category_item
    );

    g_assert_false(
        evidence_category_item_append(
            evidence_category_item,
            NULL
        )
    );

    g_assert_false(
        evidence_category_item_append(
            NULL,
            NULL
        )
    );

    g_assert_null(
        evidence_category_item_get_type_identifier(
            NULL
        )
    );

    g_assert_null(
        evidence_category_item_get_label(
            NULL
        )
    );

    g_assert_cmpuint(
        evidence_category_item_get_count(
            NULL
        ),
        ==,
        0
    );

    g_assert_null(
        evidence_category_item_get_children(
            NULL
        )
    );

    g_clear_object(
        &evidence_category_item
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
        "/evidence-category-item/new-valid",
        test_evidence_category_item_new_valid
    );

    g_test_add_func(
        "/evidence-category-item/append",
        test_evidence_category_item_append
    );

    g_test_add_func(
        "/evidence-category-item/invalid-arguments",
        test_evidence_category_item_invalid_arguments
    );

    return g_test_run();
}
