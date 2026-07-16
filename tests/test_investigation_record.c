/******************************************************************************
 * @file test_investigation_record.c
 * @brief Tests du modèle InvestigationRecord.
 ******************************************************************************/

#include "models/investigation_record.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Vérifie la création et les accesseurs du modèle.
 */
static void test_investigation_record_valid(void)
{
    InvestigationRecord *record = NULL;

    char id[] = "6e62b9af-2046-4efd-b3b9-29869f816951";
    char name[] = "Enquete_Test";
    char root_path[] = "/tmp/enquete-test";
    char created_at[] = "2026-07-16T10:00:00Z";
    char updated_at[] = "2026-07-16T10:00:00Z";

    record = investigation_record_new(
        id,
        name,
        root_path,
        created_at,
        updated_at
    );

    assert(record != NULL);

    assert(
        strcmp(
            investigation_record_get_id(record),
            id
        ) == 0
    );

    assert(
        strcmp(
            investigation_record_get_name(record),
            name
        ) == 0
    );

    assert(
        strcmp(
            investigation_record_get_root_path(record),
            root_path
        ) == 0
    );

    assert(
        strcmp(
            investigation_record_get_created_at(record),
            created_at
        ) == 0
    );

    assert(
        strcmp(
            investigation_record_get_updated_at(record),
            updated_at
        ) == 0
    );

    /*
     * Le modèle doit posséder ses propres copies.
     */
    id[0] = 'X';
    name[0] = 'X';
    root_path[0] = 'X';
    created_at[0] = 'X';
    updated_at[0] = 'X';

    assert(
        strcmp(
            investigation_record_get_id(record),
            "6e62b9af-2046-4efd-b3b9-29869f816951"
        ) == 0
    );

    assert(
        strcmp(
            investigation_record_get_name(record),
            "Enquete_Test"
        ) == 0
    );

    assert(
        strcmp(
            investigation_record_get_root_path(record),
            "/tmp/enquete-test"
        ) == 0
    );

    assert(
        strcmp(
            investigation_record_get_created_at(record),
            "2026-07-16T10:00:00Z"
        ) == 0
    );

    assert(
        strcmp(
            investigation_record_get_updated_at(record),
            "2026-07-16T10:00:00Z"
        ) == 0
    );

    investigation_record_free(record);
}

/**
 * @brief Vérifie le refus des pointeurs NULL.
 */
static void test_investigation_record_null_parameters(void)
{
    assert(
        investigation_record_new(
            NULL,
            "Enquete",
            "/tmp/enquete",
            "2026-07-16T10:00:00Z",
            "2026-07-16T10:00:00Z"
        ) == NULL
    );

    assert(
        investigation_record_new(
            "id",
            NULL,
            "/tmp/enquete",
            "2026-07-16T10:00:00Z",
            "2026-07-16T10:00:00Z"
        ) == NULL
    );

    assert(
        investigation_record_new(
            "id",
            "Enquete",
            NULL,
            "2026-07-16T10:00:00Z",
            "2026-07-16T10:00:00Z"
        ) == NULL
    );

    assert(
        investigation_record_new(
            "id",
            "Enquete",
            "/tmp/enquete",
            NULL,
            "2026-07-16T10:00:00Z"
        ) == NULL
    );

    assert(
        investigation_record_new(
            "id",
            "Enquete",
            "/tmp/enquete",
            "2026-07-16T10:00:00Z",
            NULL
        ) == NULL
    );
}

/**
 * @brief Vérifie le refus des chaînes vides.
 */
static void test_investigation_record_empty_parameters(void)
{
    assert(
        investigation_record_new(
            "",
            "Enquete",
            "/tmp/enquete",
            "2026-07-16T10:00:00Z",
            "2026-07-16T10:00:00Z"
        ) == NULL
    );

    assert(
        investigation_record_new(
            "id",
            "",
            "/tmp/enquete",
            "2026-07-16T10:00:00Z",
            "2026-07-16T10:00:00Z"
        ) == NULL
    );

    assert(
        investigation_record_new(
            "id",
            "Enquete",
            "",
            "2026-07-16T10:00:00Z",
            "2026-07-16T10:00:00Z"
        ) == NULL
    );

    assert(
        investigation_record_new(
            "id",
            "Enquete",
            "/tmp/enquete",
            "",
            "2026-07-16T10:00:00Z"
        ) == NULL
    );

    assert(
        investigation_record_new(
            "id",
            "Enquete",
            "/tmp/enquete",
            "2026-07-16T10:00:00Z",
            ""
        ) == NULL
    );
}

/**
 * @brief Vérifie les opérations acceptant un modèle NULL.
 */
static void test_investigation_record_null_instance(void)
{
    assert(
        investigation_record_get_id(NULL) == NULL
    );

    assert(
        investigation_record_get_name(NULL) == NULL
    );

    assert(
        investigation_record_get_root_path(NULL) == NULL
    );

    assert(
        investigation_record_get_created_at(NULL) == NULL
    );

    assert(
        investigation_record_get_updated_at(NULL) == NULL
    );

    investigation_record_free(NULL);
}

int main(void)
{
    test_investigation_record_valid();
    test_investigation_record_null_parameters();
    test_investigation_record_empty_parameters();
    test_investigation_record_null_instance();

    printf(
        "InvestigationRecord : tous les tests sont valides.\n"
    );

    return 0;
}
