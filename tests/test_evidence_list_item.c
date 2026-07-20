/******************************************************************************
 * @file test_evidence_list_item.c
 * @brief Tests unitaires de EvidenceListItem.
 ******************************************************************************/

#include "widgets/evidence_list_item.h"

#include <glib.h>

/**
 * @brief Crée une preuve métier valide pour les tests.
 */
static EvidenceRecord *test_evidence_list_item_create_record(
    const char *collected_at,
    const char *source,
    EvidenceIntegrityStatus integrity_status
)
{
    EvidenceRecord *evidence_record = NULL;
    GError *error = NULL;

    evidence_record =
        evidence_record_new(
            "550e8400-e29b-41d4-a716-446655440000",
            "capture_originale.png",
            "550e8400-e29b-41d4-a716-446655440000.png",
            "01_Preuves_Originales/Documents/"
            "550e8400-e29b-41d4-a716-446655440000.png",
            "image",
            4096,
            "00000000000000000000000000000000"
            "00000000000000000000000000000000",
            "2026-07-20T07:16:01Z",
            collected_at,
            source,
            "Capture reçue pendant la conversation.",
            integrity_status,
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
 * @brief Vérifie la copie des valeurs nominales.
 */
static void test_evidence_list_item_new_valid(void)
{
    EvidenceRecord *evidence_record = NULL;
    EvidenceListItem *evidence_list_item = NULL;

    evidence_record =
        test_evidence_list_item_create_record(
            "2026-07-19T18:30:00Z",
            "Conversation Instagram",
            EVIDENCE_INTEGRITY_STATUS_VALID
        );

    evidence_list_item =
        evidence_list_item_new(
            evidence_record
        );

    g_assert_nonnull(
        evidence_list_item
    );

    g_assert_cmpstr(
        evidence_list_item_get_original_name(
            evidence_list_item
        ),
        ==,
        "capture_originale.png"
    );

    g_assert_cmpstr(
        evidence_list_item_get_type_identifier(
            evidence_list_item
        ),
        ==,
        "image"
    );

    g_assert_cmpuint(
        evidence_list_item_get_size_bytes(
            evidence_list_item
        ),
        ==,
        4096
    );

    g_assert_cmpstr(
        evidence_list_item_get_collected_at(
            evidence_list_item
        ),
        ==,
        "2026-07-19T18:30:00Z"
    );

    g_assert_cmpstr(
        evidence_list_item_get_source(
            evidence_list_item
        ),
        ==,
        "Conversation Instagram"
    );

    g_assert_cmpint(
        evidence_list_item_get_integrity_status(
            evidence_list_item
        ),
        ==,
        EVIDENCE_INTEGRITY_STATUS_VALID
    );

    g_assert_cmpstr(
        evidence_list_item_get_integrity_status_label(
            evidence_list_item
        ),
        ==,
        "Valide"
    );

    g_object_unref(
        evidence_list_item
    );

    evidence_record_free(
        evidence_record
    );
}

/**
 * @brief Vérifie que les chaînes ne dépendent plus du modèle métier.
 */
static void test_evidence_list_item_copies_strings(void)
{
    EvidenceRecord *evidence_record = NULL;
    EvidenceListItem *evidence_list_item = NULL;

    evidence_record =
        test_evidence_list_item_create_record(
            "2026-07-19T18:30:00Z",
            "Email reçu",
            EVIDENCE_INTEGRITY_STATUS_UNKNOWN
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

    g_assert_cmpstr(
        evidence_list_item_get_original_name(
            evidence_list_item
        ),
        ==,
        "capture_originale.png"
    );

    g_assert_cmpstr(
        evidence_list_item_get_source(
            evidence_list_item
        ),
        ==,
        "Email reçu"
    );

    g_object_unref(
        evidence_list_item
    );
}

/**
 * @brief Vérifie les valeurs facultatives absentes.
 */
static void test_evidence_list_item_optional_values(void)
{
    EvidenceRecord *evidence_record = NULL;
    EvidenceListItem *evidence_list_item = NULL;

    evidence_record =
        test_evidence_list_item_create_record(
            NULL,
            NULL,
            EVIDENCE_INTEGRITY_STATUS_UNKNOWN
        );

    evidence_list_item =
        evidence_list_item_new(
            evidence_record
        );

    g_assert_nonnull(
        evidence_list_item
    );

    g_assert_null(
        evidence_list_item_get_collected_at(
            evidence_list_item
        )
    );

    g_assert_null(
        evidence_list_item_get_source(
            evidence_list_item
        )
    );

    g_assert_cmpstr(
        evidence_list_item_get_integrity_status_label(
            evidence_list_item
        ),
        ==,
        "Non vérifiée"
    );

    g_object_unref(
        evidence_list_item
    );

    evidence_record_free(
        evidence_record
    );
}

/**
 * @brief Vérifie les arguments NULL.
 */
static void test_evidence_list_item_null_values(void)
{
    EvidenceListItem *evidence_list_item = NULL;

    g_assert_null(
        evidence_list_item_new(
            NULL
        )
    );

    g_assert_null(
        evidence_list_item_get_original_name(
            NULL
        )
    );

    g_assert_null(
        evidence_list_item_get_type_identifier(
            NULL
        )
    );

    g_assert_cmpuint(
        evidence_list_item_get_size_bytes(
            NULL
        ),
        ==,
        0
    );

    g_assert_null(
        evidence_list_item_get_collected_at(
            NULL
        )
    );

    g_assert_null(
        evidence_list_item_get_source(
            NULL
        )
    );

    g_assert_cmpint(
        evidence_list_item_get_integrity_status(
            NULL
        ),
        ==,
        EVIDENCE_INTEGRITY_STATUS_UNKNOWN
    );

    /*
     * g_clear_object() accepte un pointeur contenant NULL.
     */
    g_clear_object(
        &evidence_list_item
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
        "/evidence-list-item/new-valid",
        test_evidence_list_item_new_valid
    );

    g_test_add_func(
        "/evidence-list-item/copies-strings",
        test_evidence_list_item_copies_strings
    );

    g_test_add_func(
        "/evidence-list-item/optional-values",
        test_evidence_list_item_optional_values
    );

    g_test_add_func(
        "/evidence-list-item/null-values",
        test_evidence_list_item_null_values
    );

    return g_test_run();
}
