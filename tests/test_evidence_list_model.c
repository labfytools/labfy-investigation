/******************************************************************************
 * @file test_evidence_list_model.c
 * @brief Tests unitaires de EvidenceListModel.
 ******************************************************************************/

#include "widgets/evidence_list_model.h"

#include "widgets/evidence_list_item.h"

#include <glib.h>

/**
 * @brief Crée une preuve valide pour les tests.
 */
static EvidenceRecord *test_evidence_list_model_create_record(
    const char *uuid,
    const char *original_name,
    const char *source
)
{
    EvidenceRecord *evidence_record = NULL;

    char *internal_name = NULL;
    char *relative_path = NULL;

    GError *error = NULL;

    internal_name =
        g_strdup_printf(
            "%s.bin",
            uuid
        );

    relative_path =
        g_strdup_printf(
            "01_Preuves_Originales/Documents/%s.bin",
            uuid
        );

    g_assert_nonnull(
        internal_name
    );

    g_assert_nonnull(
        relative_path
    );

    evidence_record =
        evidence_record_new(
            uuid,
            original_name,
            internal_name,
            relative_path,
            "document",
            4096,
            "00000000000000000000000000000000"
            "00000000000000000000000000000000",
            "2026-07-20T07:16:01Z",
            "2026-07-19T18:30:00Z",
            source,
            "Description de test.",
            EVIDENCE_INTEGRITY_STATUS_UNKNOWN,
            &error
        );

    g_assert_no_error(
        error
    );

    g_assert_nonnull(
        evidence_record
    );

    g_free(
        relative_path
    );

    g_free(
        internal_name
    );

    return evidence_record;
}

/**
 * @brief Crée un tableau qui libère automatiquement ses preuves.
 */
static GPtrArray *test_evidence_list_model_create_records(void)
{
    return g_ptr_array_new_with_free_func(
        (GDestroyNotify) evidence_record_free
    );
}

/**
 * @brief Vérifie la création d'un modèle vide.
 */
static void test_evidence_list_model_new_empty(void)
{
    EvidenceListModel *evidence_list_model =
        NULL;

    GListModel *list_model =
        NULL;

    evidence_list_model =
        evidence_list_model_new();

    g_assert_nonnull(
        evidence_list_model
    );

    list_model =
        evidence_list_model_get_list_model(
            evidence_list_model
        );

    g_assert_nonnull(
        list_model
    );

    g_assert_cmpuint(
        evidence_list_model_get_count(
            evidence_list_model
        ),
        ==,
        0
    );

    g_assert_cmpuint(
        g_list_model_get_item_type(
            list_model
        ),
        ==,
        EVIDENCE_TYPE_LIST_ITEM
    );

    evidence_list_model_free(
        evidence_list_model
    );

    evidence_list_model_free(
        NULL
    );
}

/**
 * @brief Vérifie l'alimentation nominale du modèle.
 */
static void test_evidence_list_model_replace_records(void)
{
    EvidenceListModel *evidence_list_model =
        NULL;

    GPtrArray *evidence_records =
        NULL;

    EvidenceListItem *first_item =
        NULL;

    evidence_list_model =
        evidence_list_model_new();

    evidence_records =
        test_evidence_list_model_create_records();

    g_ptr_array_add(
        evidence_records,
        test_evidence_list_model_create_record(
            "550e8400-e29b-41d4-a716-446655440000",
            "premiere-preuve.pdf",
            "Email reçu"
        )
    );

    g_ptr_array_add(
        evidence_records,
        test_evidence_list_model_create_record(
            "550e8400-e29b-41d4-a716-446655440001",
            "deuxieme-preuve.png",
            "Conversation Instagram"
        )
    );

    g_assert_true(
        evidence_list_model_replace(
            evidence_list_model,
            evidence_records,
            NULL
        )
    );

    g_assert_cmpuint(
        evidence_list_model_get_count(
            evidence_list_model
        ),
        ==,
        2
    );

    first_item =
        g_list_model_get_item(
            evidence_list_model_get_list_model(
                evidence_list_model
            ),
            0
        );

    g_assert_nonnull(
        first_item
    );

    g_assert_cmpstr(
        evidence_list_item_get_original_name(
            first_item
        ),
        ==,
        "premiere-preuve.pdf"
    );

    g_object_unref(
        first_item
    );

    /*
     * La liste doit rester valide après libération des EvidenceRecord.
     */
    g_ptr_array_unref(
        evidence_records
    );

    first_item =
        g_list_model_get_item(
            evidence_list_model_get_list_model(
                evidence_list_model
            ),
            0
        );

    g_assert_cmpstr(
        evidence_list_item_get_source(
            first_item
        ),
        ==,
        "Email reçu"
    );

    g_object_unref(
        first_item
    );

    evidence_list_model_free(
        evidence_list_model
    );
}

/**
 * @brief Vérifie qu'un rafraîchissement remplace l'ancien contenu.
 */
static void test_evidence_list_model_successive_replacements(void)
{
    EvidenceListModel *evidence_list_model =
        NULL;

    GPtrArray *first_records =
        NULL;

    GPtrArray *second_records =
        NULL;

    EvidenceListItem *remaining_item =
        NULL;

    evidence_list_model =
        evidence_list_model_new();

    first_records =
        test_evidence_list_model_create_records();

    second_records =
        test_evidence_list_model_create_records();

    g_ptr_array_add(
        first_records,
        test_evidence_list_model_create_record(
            "550e8400-e29b-41d4-a716-446655440002",
            "ancienne-preuve.txt",
            NULL
        )
    );

    g_ptr_array_add(
        second_records,
        test_evidence_list_model_create_record(
            "550e8400-e29b-41d4-a716-446655440003",
            "nouvelle-preuve.txt",
            NULL
        )
    );

    g_assert_true(
        evidence_list_model_replace(
            evidence_list_model,
            first_records,
            NULL
        )
    );

    g_assert_true(
        evidence_list_model_replace(
            evidence_list_model,
            second_records,
            NULL
        )
    );

    g_assert_cmpuint(
        evidence_list_model_get_count(
            evidence_list_model
        ),
        ==,
        1
    );

    remaining_item =
        g_list_model_get_item(
            evidence_list_model_get_list_model(
                evidence_list_model
            ),
            0
        );

    g_assert_cmpstr(
        evidence_list_item_get_original_name(
            remaining_item
        ),
        ==,
        "nouvelle-preuve.txt"
    );

    g_object_unref(
        remaining_item
    );

    g_ptr_array_unref(
        second_records
    );

    g_ptr_array_unref(
        first_records
    );

    evidence_list_model_free(
        evidence_list_model
    );
}

/**
 * @brief Vérifie qu'un tableau vide efface le modèle.
 */
static void test_evidence_list_model_replace_empty(void)
{
    EvidenceListModel *evidence_list_model =
        NULL;

    GPtrArray *evidence_records =
        NULL;

    GPtrArray *empty_records =
        NULL;

    evidence_list_model =
        evidence_list_model_new();

    evidence_records =
        test_evidence_list_model_create_records();

    empty_records =
        test_evidence_list_model_create_records();

    g_ptr_array_add(
        evidence_records,
        test_evidence_list_model_create_record(
            "550e8400-e29b-41d4-a716-446655440004",
            "preuve-temporaire.txt",
            NULL
        )
    );

    g_assert_true(
        evidence_list_model_replace(
            evidence_list_model,
            evidence_records,
            NULL
        )
    );

    g_assert_true(
        evidence_list_model_replace(
            evidence_list_model,
            empty_records,
            NULL
        )
    );

    g_assert_cmpuint(
        evidence_list_model_get_count(
            evidence_list_model
        ),
        ==,
        0
    );

    g_ptr_array_unref(
        empty_records
    );

    g_ptr_array_unref(
        evidence_records
    );

    evidence_list_model_free(
        evidence_list_model
    );
}

/**
 * @brief Vérifie les arguments invalides et le remplacement atomique.
 */
static void test_evidence_list_model_invalid_records(void)
{
    EvidenceListModel *evidence_list_model =
        NULL;

    GPtrArray *valid_records =
        NULL;

    GPtrArray *invalid_records =
        NULL;

    GError *error = NULL;

    evidence_list_model =
        evidence_list_model_new();

    valid_records =
        test_evidence_list_model_create_records();

    invalid_records =
        test_evidence_list_model_create_records();

    g_ptr_array_add(
        valid_records,
        test_evidence_list_model_create_record(
            "550e8400-e29b-41d4-a716-446655440005",
            "preuve-conservee.txt",
            NULL
        )
    );

    g_assert_true(
        evidence_list_model_replace(
            evidence_list_model,
            valid_records,
            NULL
        )
    );

    /*
     * NULL représente ici une ligne métier invalide.
     */
    g_ptr_array_add(
        invalid_records,
        NULL
    );

    g_assert_false(
        evidence_list_model_replace(
            evidence_list_model,
            invalid_records,
            &error
        )
    );

    g_assert_error(
        error,
        EVIDENCE_LIST_MODEL_ERROR,
        EVIDENCE_LIST_MODEL_ERROR_INVALID_RECORD
    );

    g_clear_error(
        &error
    );

    /*
     * L'ancien élément doit toujours être présent.
     */
    g_assert_cmpuint(
        evidence_list_model_get_count(
            evidence_list_model
        ),
        ==,
        1
    );

    g_assert_false(
        evidence_list_model_replace(
            NULL,
            valid_records,
            &error
        )
    );

    g_assert_error(
        error,
        EVIDENCE_LIST_MODEL_ERROR,
        EVIDENCE_LIST_MODEL_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    g_assert_false(
        evidence_list_model_replace(
            evidence_list_model,
            NULL,
            &error
        )
    );

    g_assert_error(
        error,
        EVIDENCE_LIST_MODEL_ERROR,
        EVIDENCE_LIST_MODEL_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    g_ptr_array_unref(
        invalid_records
    );

    g_ptr_array_unref(
        valid_records
    );

    evidence_list_model_free(
        evidence_list_model
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
        "/evidence-list-model/new-empty",
        test_evidence_list_model_new_empty
    );

    g_test_add_func(
        "/evidence-list-model/replace-records",
        test_evidence_list_model_replace_records
    );

    g_test_add_func(
        "/evidence-list-model/successive-replacements",
        test_evidence_list_model_successive_replacements
    );

    g_test_add_func(
        "/evidence-list-model/replace-empty",
        test_evidence_list_model_replace_empty
    );

    g_test_add_func(
        "/evidence-list-model/invalid-records",
        test_evidence_list_model_invalid_records
    );

    return g_test_run();
}
