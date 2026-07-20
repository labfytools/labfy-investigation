/******************************************************************************
 * @file test_evidence_category_model.c
 * @brief Tests unitaires de EvidenceCategoryModel.
 ******************************************************************************/

#include "widgets/evidence_category_model.h"

#include "widgets/evidence_category_item.h"
#include "widgets/evidence_list_item.h"

#include "models/evidence_record.h"

#include <glib.h>

/**
 * @brief Crée un élément d'affichage valide.
 */
static EvidenceListItem *test_evidence_category_model_create_item(
    const char *identifier,
    const char *original_name,
    const char *type_identifier
)
{
    EvidenceRecord *evidence_record = NULL;
    EvidenceListItem *evidence_list_item = NULL;

    char *internal_name = NULL;
    char *relative_path = NULL;

    GError *error = NULL;

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

    g_assert_nonnull(
        internal_name
    );

    g_assert_nonnull(
        relative_path
    );

    evidence_record =
        evidence_record_new(
            identifier,
            original_name,
            internal_name,
            relative_path,
            type_identifier,
            4096,
            "00000000000000000000000000000000"
            "00000000000000000000000000000000",
            "2026-07-20T07:16:01Z",
            "2026-07-19T18:30:00Z",
            NULL,
            NULL,
            EVIDENCE_INTEGRITY_STATUS_UNKNOWN,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        evidence_record
    );

    evidence_list_item =
        evidence_list_item_new(
            evidence_record
        );

    g_assert_nonnull(
        evidence_list_item
    );

    evidence_record_free(
        evidence_record
    );

    g_free(
        relative_path
    );

    g_free(
        internal_name
    );

    return evidence_list_item;
}

/**
 * @brief Ajoute une preuve à un GListStore.
 */
static void test_evidence_category_model_append_item(
    GListStore *evidence_items,
    EvidenceListItem *evidence_list_item
)
{
    g_list_store_append(
        evidence_items,
        G_OBJECT(
            evidence_list_item
        )
    );

    g_object_unref(
        evidence_list_item
    );
}

/**
 * @brief Crée la table des libellés de types.
 */
static GHashTable *test_evidence_category_model_create_labels(void)
{
    GHashTable *type_labels =
        NULL;

    type_labels =
        g_hash_table_new_full(
            g_str_hash,
            g_str_equal,
            g_free,
            g_free
        );

    g_hash_table_insert(
        type_labels,
        g_strdup("document"),
        g_strdup("Documents")
    );

    g_hash_table_insert(
        type_labels,
        g_strdup("image"),
        g_strdup("Images")
    );

    return type_labels;
}

/**
 * @brief Vérifie la création d'un modèle vide.
 */
static void test_evidence_category_model_new_empty(void)
{
    EvidenceCategoryModel *evidence_category_model =
        NULL;

    evidence_category_model =
        evidence_category_model_new();

    g_assert_nonnull(
        evidence_category_model
    );

    g_assert_nonnull(
        evidence_category_model_get_list_model(
            evidence_category_model
        )
    );

    g_assert_cmpuint(
        evidence_category_model_get_count(
            evidence_category_model
        ),
        ==,
        0
    );

    g_assert_cmpuint(
        g_list_model_get_item_type(
            evidence_category_model_get_list_model(
                evidence_category_model
            )
        ),
        ==,
        EVIDENCE_TYPE_CATEGORY_ITEM
    );

    evidence_category_model_free(
        evidence_category_model
    );

    evidence_category_model_free(
        NULL
    );
}

/**
 * @brief Vérifie le regroupement et le tri.
 */
static void test_evidence_category_model_group_and_sort(void)
{
    EvidenceCategoryModel *evidence_category_model =
        NULL;

    GListStore *evidence_items =
        NULL;

    GHashTable *type_labels =
        NULL;

    EvidenceCategoryItem *documents_category =
        NULL;

    EvidenceCategoryItem *images_category =
        NULL;

    EvidenceListItem *first_image =
        NULL;

    evidence_category_model =
        evidence_category_model_new();

    evidence_items =
        g_list_store_new(
            EVIDENCE_TYPE_LIST_ITEM
        );

    type_labels =
        test_evidence_category_model_create_labels();

    test_evidence_category_model_append_item(
        evidence_items,
        test_evidence_category_model_create_item(
            "550e8400-e29b-41d4-a716-446655440000",
            "zeta.png",
            "image"
        )
    );

    test_evidence_category_model_append_item(
        evidence_items,
        test_evidence_category_model_create_item(
            "550e8400-e29b-41d4-a716-446655440001",
            "rapport.pdf",
            "document"
        )
    );

    test_evidence_category_model_append_item(
        evidence_items,
        test_evidence_category_model_create_item(
            "550e8400-e29b-41d4-a716-446655440002",
            "alpha.png",
            "image"
        )
    );

    g_assert_true(
        evidence_category_model_replace(
            evidence_category_model,
            G_LIST_MODEL(
                evidence_items
            ),
            type_labels,
            NULL
        )
    );

    g_assert_cmpuint(
        evidence_category_model_get_count(
            evidence_category_model
        ),
        ==,
        2
    );

    documents_category =
        g_list_model_get_item(
            evidence_category_model_get_list_model(
                evidence_category_model
            ),
            0
        );

    images_category =
        g_list_model_get_item(
            evidence_category_model_get_list_model(
                evidence_category_model
            ),
            1
        );

    g_assert_cmpstr(
        evidence_category_item_get_label(
            documents_category
        ),
        ==,
        "Documents"
    );

    g_assert_cmpuint(
        evidence_category_item_get_count(
            documents_category
        ),
        ==,
        1
    );

    g_assert_cmpstr(
        evidence_category_item_get_label(
            images_category
        ),
        ==,
        "Images"
    );

    g_assert_cmpuint(
        evidence_category_item_get_count(
            images_category
        ),
        ==,
        2
    );

    first_image =
        g_list_model_get_item(
            evidence_category_item_get_children(
                images_category
            ),
            0
        );

    g_assert_cmpstr(
        evidence_list_item_get_original_name(
            first_image
        ),
        ==,
        "alpha.png"
    );

    g_object_unref(
        first_image
    );

    g_object_unref(
        images_category
    );

    g_object_unref(
        documents_category
    );

    g_hash_table_unref(
        type_labels
    );

    g_object_unref(
        evidence_items
    );

    evidence_category_model_free(
        evidence_category_model
    );
}

/**
 * @brief Vérifie le libellé de secours pour un type inconnu.
 */
static void test_evidence_category_model_unknown_type(void)
{
    EvidenceCategoryModel *evidence_category_model =
        NULL;

    GListStore *evidence_items =
        NULL;

    EvidenceCategoryItem *category =
        NULL;

    evidence_category_model =
        evidence_category_model_new();

    evidence_items =
        g_list_store_new(
            EVIDENCE_TYPE_LIST_ITEM
        );

    test_evidence_category_model_append_item(
        evidence_items,
        test_evidence_category_model_create_item(
            "550e8400-e29b-41d4-a716-446655440003",
            "archive.bin",
            "unknown"
        )
    );

    g_assert_true(
        evidence_category_model_replace(
            evidence_category_model,
            G_LIST_MODEL(
                evidence_items
            ),
            NULL,
            NULL
        )
    );

    category =
        g_list_model_get_item(
            evidence_category_model_get_list_model(
                evidence_category_model
            ),
            0
        );

    g_assert_cmpstr(
        evidence_category_item_get_label(
            category
        ),
        ==,
        "unknown"
    );

    g_object_unref(
        category
    );

    g_object_unref(
        evidence_items
    );

    evidence_category_model_free(
        evidence_category_model
    );
}

/**
 * @brief Vérifie les remplacements successifs et les erreurs.
 */
static void test_evidence_category_model_replace_and_clear(void)
{
    EvidenceCategoryModel *evidence_category_model =
        NULL;

    GListStore *valid_items =
        NULL;

    GListStore *empty_items =
        NULL;

    GListStore *invalid_items =
        NULL;

    GObject *invalid_object =
        NULL;

    GError *error = NULL;

    evidence_category_model =
        evidence_category_model_new();

    valid_items =
        g_list_store_new(
            EVIDENCE_TYPE_LIST_ITEM
        );

    empty_items =
        g_list_store_new(
            EVIDENCE_TYPE_LIST_ITEM
        );

    invalid_items =
        g_list_store_new(
            G_TYPE_OBJECT
        );

    test_evidence_category_model_append_item(
        valid_items,
        test_evidence_category_model_create_item(
            "550e8400-e29b-41d4-a716-446655440004",
            "preuve.txt",
            "document"
        )
    );

    g_assert_true(
        evidence_category_model_replace(
            evidence_category_model,
            G_LIST_MODEL(
                valid_items
            ),
            NULL,
            NULL
        )
    );

    invalid_object =
        g_object_new(
            G_TYPE_OBJECT,
            NULL
        );

    g_list_store_append(
        invalid_items,
        invalid_object
    );

    g_object_unref(
        invalid_object
    );

    g_assert_false(
        evidence_category_model_replace(
            evidence_category_model,
            G_LIST_MODEL(
                invalid_items
            ),
            NULL,
            &error
        )
    );

    g_assert_error(
        error,
        EVIDENCE_CATEGORY_MODEL_ERROR,
        EVIDENCE_CATEGORY_MODEL_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    /*
     * L'ancien contenu est conservé après l'échec.
     */
    g_assert_cmpuint(
        evidence_category_model_get_count(
            evidence_category_model
        ),
        ==,
        1
    );

    g_assert_true(
        evidence_category_model_replace(
            evidence_category_model,
            G_LIST_MODEL(
                empty_items
            ),
            NULL,
            NULL
        )
    );

    g_assert_cmpuint(
        evidence_category_model_get_count(
            evidence_category_model
        ),
        ==,
        0
    );

    g_assert_false(
        evidence_category_model_replace(
            NULL,
            G_LIST_MODEL(
                valid_items
            ),
            NULL,
            &error
        )
    );

    g_assert_error(
        error,
        EVIDENCE_CATEGORY_MODEL_ERROR,
        EVIDENCE_CATEGORY_MODEL_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    g_object_unref(
        invalid_items
    );

    g_object_unref(
        empty_items
    );

    g_object_unref(
        valid_items
    );

    evidence_category_model_free(
        evidence_category_model
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
        "/evidence-category-model/new-empty",
        test_evidence_category_model_new_empty
    );

    g_test_add_func(
        "/evidence-category-model/group-and-sort",
        test_evidence_category_model_group_and_sort
    );

    g_test_add_func(
        "/evidence-category-model/unknown-type",
        test_evidence_category_model_unknown_type
    );

    g_test_add_func(
        "/evidence-category-model/replace-and-clear",
        test_evidence_category_model_replace_and_clear
    );

    return g_test_run();
}
