/******************************************************************************
 * @file test_investigation_graph_model.c
 * @brief Tests unitaires du graphe métier d'une enquête.
 ******************************************************************************/

#include "models/investigation_graph_model.h"

#include "models/entity_record.h"
#include "models/relation_record.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>

#define TEST_ENTITY_A_IDENTIFIER \
    "10000000-0000-4000-8000-000000000001"

#define TEST_ENTITY_B_IDENTIFIER \
    "20000000-0000-4000-8000-000000000001"

#define TEST_ENTITY_C_IDENTIFIER \
    "30000000-0000-4000-8000-000000000001"

#define TEST_RELATION_AB_IDENTIFIER \
    "40000000-0000-4000-8000-000000000001"

#define TEST_RELATION_AC_IDENTIFIER \
    "40000000-0000-4000-8000-000000000002"

#define TEST_RELATION_CA_IDENTIFIER \
    "40000000-0000-4000-8000-000000000003"

/**
 * @brief Vérifie le domaine et le code d'une erreur du graphe.
 */
static void test_graph_assert_error(
    const GError *error,
    InvestigationGraphModelError expected_code
)
{
    assert(error != NULL);

    assert(
        error->domain ==
        INVESTIGATION_GRAPH_MODEL_ERROR
    );

    assert(
        error->code ==
        (gint) expected_code
    );

    assert(error->message != NULL);
    assert(error->message[0] != '\0');
}

/**
 * @brief Crée une entité valide.
 */
static EntityRecord *test_graph_create_entity(
    const char *identifier,
    const char *value
)
{
    EntityRecord *entity_record =
        NULL;

    GError *error =
        NULL;

    entity_record =
        entity_record_new(
            identifier,
            "person",
            value,
            NULL,
            NULL,
            80,
            "2026-07-21T20:00:00Z",
            "2026-07-21T20:00:00Z",
            ENTITY_STATUS_ACTIVE,
            &error
        );

    assert(entity_record != NULL);
    assert(error == NULL);

    return entity_record;
}

/**
 * @brief Crée une relation valide.
 */
static RelationRecord *test_graph_create_relation(
    const char *identifier,
    const char *source_identifier,
    const char *target_identifier,
    const char *relation_type
)
{
    RelationRecord *relation_record =
        NULL;

    GError *error =
        NULL;

    relation_record =
        relation_record_new(
            identifier,
            source_identifier,
            target_identifier,
            relation_type,
            NULL,
            NULL,
            75,
            "2026-07-21T20:10:00Z",
            "2026-07-21T20:10:00Z",
            RELATION_STATUS_ACTIVE,
            &error
        );

    assert(relation_record != NULL);
    assert(error == NULL);

    return relation_record;
}

/**
 * @brief Ajoute une entité et vérifie le succès.
 */
static void test_graph_add_entity_or_abort(
    InvestigationGraphModel *graph_model,
    EntityRecord *entity_record
)
{
    GError *error =
        NULL;

    assert(
        investigation_graph_model_add_entity(
            graph_model,
            entity_record,
            &error
        )
    );

    assert(error == NULL);
}

/**
 * @brief Ajoute une relation et vérifie le succès.
 */
static void test_graph_add_relation_or_abort(
    InvestigationGraphModel *graph_model,
    RelationRecord *relation_record
)
{
    GError *error =
        NULL;

    assert(
        investigation_graph_model_add_relation(
            graph_model,
            relation_record,
            &error
        )
    );

    assert(error == NULL);
}

/**
 * @brief Ajoute les trois entités courantes.
 */
static void test_graph_add_standard_entities(
    InvestigationGraphModel *graph_model
)
{
    test_graph_add_entity_or_abort(
        graph_model,
        test_graph_create_entity(
            TEST_ENTITY_A_IDENTIFIER,
            "Personne A"
        )
    );

    test_graph_add_entity_or_abort(
        graph_model,
        test_graph_create_entity(
            TEST_ENTITY_B_IDENTIFIER,
            "Personne B"
        )
    );

    test_graph_add_entity_or_abort(
        graph_model,
        test_graph_create_entity(
            TEST_ENTITY_C_IDENTIFIER,
            "Personne C"
        )
    );
}

/**
 * @brief Vérifie la création, le comptage et la destruction d'un graphe vide.
 */
static void test_graph_empty_lifecycle(void)
{
    InvestigationGraphModel *graph_model =
        NULL;

    GPtrArray *entities =
        NULL;

    GPtrArray *relations =
        NULL;

    GError *error =
        NULL;

    graph_model =
        investigation_graph_model_new(
            &error
        );

    assert(graph_model != NULL);
    assert(error == NULL);

    assert(
        investigation_graph_model_get_entity_count(
            graph_model
        ) == 0
    );

    assert(
        investigation_graph_model_get_relation_count(
            graph_model
        ) == 0
    );

    entities =
        investigation_graph_model_list_entities(
            graph_model,
            &error
        );

    assert(entities != NULL);
    assert(error == NULL);
    assert(entities->len == 0);

    relations =
        investigation_graph_model_list_relations(
            graph_model,
            &error
        );

    assert(relations != NULL);
    assert(error == NULL);
    assert(relations->len == 0);

    g_ptr_array_unref(
        relations
    );

    g_ptr_array_unref(
        entities
    );

    investigation_graph_model_free(
        graph_model
    );

    investigation_graph_model_free(
        NULL
    );
}

/**
 * @brief Vérifie l'ajout et la recherche d'une entité.
 */
static void test_graph_add_and_find_entity(void)
{
    InvestigationGraphModel *graph_model =
        NULL;

    EntityRecord *entity_record =
        NULL;

    const EntityRecord *loaded_entity =
        NULL;

    GError *error =
        NULL;

    graph_model =
        investigation_graph_model_new(
            &error
        );

    assert(graph_model != NULL);
    assert(error == NULL);

    entity_record =
        test_graph_create_entity(
            TEST_ENTITY_A_IDENTIFIER,
            "Personne A"
        );

    test_graph_add_entity_or_abort(
        graph_model,
        entity_record
    );

    /*
     * Le pointeur retourné doit être le même objet possédé par le graphe.
     */
    loaded_entity =
        investigation_graph_model_find_entity(
            graph_model,
            TEST_ENTITY_A_IDENTIFIER
        );

    assert(loaded_entity == entity_record);

    assert(
        strcmp(
            entity_record_get_identifier(
                loaded_entity
            ),
            TEST_ENTITY_A_IDENTIFIER
        ) == 0
    );

    assert(
        investigation_graph_model_get_entity_count(
            graph_model
        ) == 1
    );

    assert(
        investigation_graph_model_find_entity(
            graph_model,
            TEST_ENTITY_B_IDENTIFIER
        ) == NULL
    );

    assert(
        investigation_graph_model_find_entity(
            graph_model,
            "identifiant-invalide"
        ) == NULL
    );

    investigation_graph_model_free(
        graph_model
    );
}

/**
 * @brief Vérifie le refus d'une entité NULL.
 */
static void test_graph_reject_null_entity(void)
{
    InvestigationGraphModel *graph_model =
        NULL;

    GError *error =
        NULL;

    graph_model =
        investigation_graph_model_new(
            &error
        );

    assert(graph_model != NULL);
    assert(error == NULL);

    assert(
        !investigation_graph_model_add_entity(
            graph_model,
            NULL,
            &error
        )
    );

    test_graph_assert_error(
        error,
        INVESTIGATION_GRAPH_MODEL_ERROR_INVALID_ARGUMENT
    );

    assert(
        investigation_graph_model_get_entity_count(
            graph_model
        ) == 0
    );

    g_clear_error(
        &error
    );

    investigation_graph_model_free(
        graph_model
    );
}

/**
 * @brief Vérifie le refus d'un UUID d'entité dupliqué.
 */
static void test_graph_reject_duplicate_entity(void)
{
    InvestigationGraphModel *graph_model =
        NULL;

    EntityRecord *first_entity =
        NULL;

    EntityRecord *duplicate_entity =
        NULL;

    GError *error =
        NULL;

    graph_model =
        investigation_graph_model_new(
            &error
        );

    assert(graph_model != NULL);
    assert(error == NULL);

    first_entity =
        test_graph_create_entity(
            TEST_ENTITY_A_IDENTIFIER,
            "Personne A"
        );

    duplicate_entity =
        test_graph_create_entity(
            TEST_ENTITY_A_IDENTIFIER,
            "Autre valeur"
        );

    test_graph_add_entity_or_abort(
        graph_model,
        first_entity
    );

    assert(
        !investigation_graph_model_add_entity(
            graph_model,
            duplicate_entity,
            &error
        )
    );

    test_graph_assert_error(
        error,
        INVESTIGATION_GRAPH_MODEL_ERROR_DUPLICATE_ENTITY
    );

    assert(
        investigation_graph_model_get_entity_count(
            graph_model
        ) == 1
    );

    /*
     * Après l'échec, le modèle appartient encore à l'appelant.
     */
    entity_record_free(
        duplicate_entity
    );

    g_clear_error(
        &error
    );

    investigation_graph_model_free(
        graph_model
    );
}

/**
 * @brief Vérifie l'ordre stable de la liste globale des entités.
 */
static void test_graph_list_entities_sorted(void)
{
    InvestigationGraphModel *graph_model =
        NULL;

    GPtrArray *entities =
        NULL;

    const EntityRecord *first_entity =
        NULL;

    const EntityRecord *second_entity =
        NULL;

    const EntityRecord *third_entity =
        NULL;

    GError *error =
        NULL;

    graph_model =
        investigation_graph_model_new(
            &error
        );

    assert(graph_model != NULL);
    assert(error == NULL);

    test_graph_add_entity_or_abort(
        graph_model,
        test_graph_create_entity(
            TEST_ENTITY_C_IDENTIFIER,
            "Personne C"
        )
    );

    test_graph_add_entity_or_abort(
        graph_model,
        test_graph_create_entity(
            TEST_ENTITY_A_IDENTIFIER,
            "Personne A"
        )
    );

    test_graph_add_entity_or_abort(
        graph_model,
        test_graph_create_entity(
            TEST_ENTITY_B_IDENTIFIER,
            "Personne B"
        )
    );

    entities =
        investigation_graph_model_list_entities(
            graph_model,
            &error
        );

    assert(entities != NULL);
    assert(error == NULL);
    assert(entities->len == 3);

    first_entity =
        g_ptr_array_index(
            entities,
            0
        );

    second_entity =
        g_ptr_array_index(
            entities,
            1
        );

    third_entity =
        g_ptr_array_index(
            entities,
            2
        );

    assert(
        strcmp(
            entity_record_get_identifier(
                first_entity
            ),
            TEST_ENTITY_A_IDENTIFIER
        ) == 0
    );

    assert(
        strcmp(
            entity_record_get_identifier(
                second_entity
            ),
            TEST_ENTITY_B_IDENTIFIER
        ) == 0
    );

    assert(
        strcmp(
            entity_record_get_identifier(
                third_entity
            ),
            TEST_ENTITY_C_IDENTIFIER
        ) == 0
    );

    /*
     * Le conteneur ne doit pas posséder les modèles.
     */
    g_ptr_array_unref(
        entities
    );

    assert(
        investigation_graph_model_find_entity(
            graph_model,
            TEST_ENTITY_A_IDENTIFIER
        ) != NULL
    );

    investigation_graph_model_free(
        graph_model
    );
}

/**
 * @brief Vérifie l'ajout, la recherche et l'orientation d'une relation.
 */
static void test_graph_add_and_find_relation(void)
{
    InvestigationGraphModel *graph_model =
        NULL;

    RelationRecord *relation_record =
        NULL;

    const RelationRecord *loaded_relation =
        NULL;

    GPtrArray *outgoing_relations =
        NULL;

    GPtrArray *incoming_relations =
        NULL;

    GError *error =
        NULL;

    graph_model =
        investigation_graph_model_new(
            &error
        );

    assert(graph_model != NULL);
    assert(error == NULL);

    test_graph_add_standard_entities(
        graph_model
    );

    relation_record =
        test_graph_create_relation(
            TEST_RELATION_AB_IDENTIFIER,
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_B_IDENTIFIER,
            "uses"
        );

    test_graph_add_relation_or_abort(
        graph_model,
        relation_record
    );

    loaded_relation =
        investigation_graph_model_find_relation(
            graph_model,
            TEST_RELATION_AB_IDENTIFIER
        );

    assert(loaded_relation == relation_record);

    assert(
        investigation_graph_model_get_relation_count(
            graph_model
        ) == 1
    );

    outgoing_relations =
        investigation_graph_model_list_outgoing_relations(
            graph_model,
            TEST_ENTITY_A_IDENTIFIER,
            &error
        );

    assert(outgoing_relations != NULL);
    assert(error == NULL);
    assert(outgoing_relations->len == 1);

    assert(
        g_ptr_array_index(
            outgoing_relations,
            0
        ) == relation_record
    );

    incoming_relations =
        investigation_graph_model_list_incoming_relations(
            graph_model,
            TEST_ENTITY_B_IDENTIFIER,
            &error
        );

    assert(incoming_relations != NULL);
    assert(error == NULL);
    assert(incoming_relations->len == 1);

    assert(
        g_ptr_array_index(
            incoming_relations,
            0
        ) == relation_record
    );

    g_ptr_array_unref(
        incoming_relations
    );

    g_ptr_array_unref(
        outgoing_relations
    );

    investigation_graph_model_free(
        graph_model
    );
}

/**
 * @brief Vérifie le refus d'une relation NULL.
 */
static void test_graph_reject_null_relation(void)
{
    InvestigationGraphModel *graph_model =
        NULL;

    GError *error =
        NULL;

    graph_model =
        investigation_graph_model_new(
            &error
        );

    assert(graph_model != NULL);
    assert(error == NULL);

    assert(
        !investigation_graph_model_add_relation(
            graph_model,
            NULL,
            &error
        )
    );

    test_graph_assert_error(
        error,
        INVESTIGATION_GRAPH_MODEL_ERROR_INVALID_ARGUMENT
    );

    assert(
        investigation_graph_model_get_relation_count(
            graph_model
        ) == 0
    );

    g_clear_error(
        &error
    );

    investigation_graph_model_free(
        graph_model
    );
}

/**
 * @brief Vérifie le refus d'un UUID de relation dupliqué.
 */
static void test_graph_reject_duplicate_relation(void)
{
    InvestigationGraphModel *graph_model =
        NULL;

    RelationRecord *first_relation =
        NULL;

    RelationRecord *duplicate_relation =
        NULL;

    GError *error =
        NULL;

    graph_model =
        investigation_graph_model_new(
            &error
        );

    assert(graph_model != NULL);
    assert(error == NULL);

    test_graph_add_standard_entities(
        graph_model
    );

    first_relation =
        test_graph_create_relation(
            TEST_RELATION_AB_IDENTIFIER,
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_B_IDENTIFIER,
            "uses"
        );

    duplicate_relation =
        test_graph_create_relation(
            TEST_RELATION_AB_IDENTIFIER,
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_C_IDENTIFIER,
            "controls"
        );

    test_graph_add_relation_or_abort(
        graph_model,
        first_relation
    );

    assert(
        !investigation_graph_model_add_relation(
            graph_model,
            duplicate_relation,
            &error
        )
    );

    test_graph_assert_error(
        error,
        INVESTIGATION_GRAPH_MODEL_ERROR_DUPLICATE_RELATION
    );

    assert(
        investigation_graph_model_get_relation_count(
            graph_model
        ) == 1
    );

    /*
     * Après l'échec, la relation appartient encore à l'appelant.
     */
    relation_record_free(
        duplicate_relation
    );

    g_clear_error(
        &error
    );

    investigation_graph_model_free(
        graph_model
    );
}

/**
 * @brief Vérifie le refus d'une relation dont la source est absente.
 */
static void test_graph_reject_missing_source(void)
{
    InvestigationGraphModel *graph_model =
        NULL;

    RelationRecord *relation_record =
        NULL;

    GPtrArray *incoming_relations =
        NULL;

    GError *error =
        NULL;

    graph_model =
        investigation_graph_model_new(
            &error
        );

    assert(graph_model != NULL);
    assert(error == NULL);

    test_graph_add_entity_or_abort(
        graph_model,
        test_graph_create_entity(
            TEST_ENTITY_B_IDENTIFIER,
            "Personne B"
        )
    );

    relation_record =
        test_graph_create_relation(
            TEST_RELATION_AB_IDENTIFIER,
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_B_IDENTIFIER,
            "uses"
        );

    assert(
        !investigation_graph_model_add_relation(
            graph_model,
            relation_record,
            &error
        )
    );

    test_graph_assert_error(
        error,
        INVESTIGATION_GRAPH_MODEL_ERROR_SOURCE_NOT_FOUND
    );

    assert(
        investigation_graph_model_get_relation_count(
            graph_model
        ) == 0
    );

    g_clear_error(
        &error
    );

    incoming_relations =
        investigation_graph_model_list_incoming_relations(
            graph_model,
            TEST_ENTITY_B_IDENTIFIER,
            &error
        );

    assert(incoming_relations != NULL);
    assert(error == NULL);
    assert(incoming_relations->len == 0);

    g_ptr_array_unref(
        incoming_relations
    );

    relation_record_free(
        relation_record
    );

    investigation_graph_model_free(
        graph_model
    );
}

/**
 * @brief Vérifie le refus d'une relation dont la cible est absente.
 */
static void test_graph_reject_missing_target(void)
{
    InvestigationGraphModel *graph_model =
        NULL;

    RelationRecord *relation_record =
        NULL;

    GPtrArray *outgoing_relations =
        NULL;

    GError *error =
        NULL;

    graph_model =
        investigation_graph_model_new(
            &error
        );

    assert(graph_model != NULL);
    assert(error == NULL);

    test_graph_add_entity_or_abort(
        graph_model,
        test_graph_create_entity(
            TEST_ENTITY_A_IDENTIFIER,
            "Personne A"
        )
    );

    relation_record =
        test_graph_create_relation(
            TEST_RELATION_AB_IDENTIFIER,
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_B_IDENTIFIER,
            "uses"
        );

    assert(
        !investigation_graph_model_add_relation(
            graph_model,
            relation_record,
            &error
        )
    );

    test_graph_assert_error(
        error,
        INVESTIGATION_GRAPH_MODEL_ERROR_TARGET_NOT_FOUND
    );

    assert(
        investigation_graph_model_get_relation_count(
            graph_model
        ) == 0
    );

    g_clear_error(
        &error
    );

    outgoing_relations =
        investigation_graph_model_list_outgoing_relations(
            graph_model,
            TEST_ENTITY_A_IDENTIFIER,
            &error
        );

    assert(outgoing_relations != NULL);
    assert(error == NULL);
    assert(outgoing_relations->len == 0);

    g_ptr_array_unref(
        outgoing_relations
    );

    relation_record_free(
        relation_record
    );

    investigation_graph_model_free(
        graph_model
    );
}

/**
 * @brief Vérifie l'ordre global des relations.
 */
static void test_graph_list_relations_sorted(void)
{
    InvestigationGraphModel *graph_model =
        NULL;

    GPtrArray *relations =
        NULL;

    const RelationRecord *first_relation =
        NULL;

    const RelationRecord *second_relation =
        NULL;

    const RelationRecord *third_relation =
        NULL;

    GError *error =
        NULL;

    graph_model =
        investigation_graph_model_new(
            &error
        );

    assert(graph_model != NULL);
    assert(error == NULL);

    test_graph_add_standard_entities(
        graph_model
    );

    test_graph_add_relation_or_abort(
        graph_model,
        test_graph_create_relation(
            TEST_RELATION_CA_IDENTIFIER,
            TEST_ENTITY_C_IDENTIFIER,
            TEST_ENTITY_A_IDENTIFIER,
            "knows"
        )
    );

    test_graph_add_relation_or_abort(
        graph_model,
        test_graph_create_relation(
            TEST_RELATION_AB_IDENTIFIER,
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_B_IDENTIFIER,
            "uses"
        )
    );

    test_graph_add_relation_or_abort(
        graph_model,
        test_graph_create_relation(
            TEST_RELATION_AC_IDENTIFIER,
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_C_IDENTIFIER,
            "controls"
        )
    );

    relations =
        investigation_graph_model_list_relations(
            graph_model,
            &error
        );

    assert(relations != NULL);
    assert(error == NULL);
    assert(relations->len == 3);

    first_relation =
        g_ptr_array_index(
            relations,
            0
        );

    second_relation =
        g_ptr_array_index(
            relations,
            1
        );

    third_relation =
        g_ptr_array_index(
            relations,
            2
        );

    assert(
        strcmp(
            relation_record_get_identifier(
                first_relation
            ),
            TEST_RELATION_AB_IDENTIFIER
        ) == 0
    );

    assert(
        strcmp(
            relation_record_get_identifier(
                second_relation
            ),
            TEST_RELATION_AC_IDENTIFIER
        ) == 0
    );

    assert(
        strcmp(
            relation_record_get_identifier(
                third_relation
            ),
            TEST_RELATION_CA_IDENTIFIER
        ) == 0
    );

    g_ptr_array_unref(
        relations
    );

    assert(
        investigation_graph_model_find_relation(
            graph_model,
            TEST_RELATION_AB_IDENTIFIER
        ) != NULL
    );

    investigation_graph_model_free(
        graph_model
    );
}

/**
 * @brief Vérifie les listes entrantes, sortantes et incidentes.
 */
static void test_graph_navigation_indexes(void)
{
    InvestigationGraphModel *graph_model =
        NULL;

    RelationRecord *relation_ab =
        NULL;

    RelationRecord *relation_ac =
        NULL;

    RelationRecord *relation_ca =
        NULL;

    GPtrArray *outgoing_a =
        NULL;

    GPtrArray *incoming_a =
        NULL;

    GPtrArray *incident_a =
        NULL;

    GPtrArray *outgoing_b =
        NULL;

    GError *error =
        NULL;

    graph_model =
        investigation_graph_model_new(
            &error
        );

    assert(graph_model != NULL);
    assert(error == NULL);

    test_graph_add_standard_entities(
        graph_model
    );

    relation_ab =
        test_graph_create_relation(
            TEST_RELATION_AB_IDENTIFIER,
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_B_IDENTIFIER,
            "uses"
        );

    relation_ac =
        test_graph_create_relation(
            TEST_RELATION_AC_IDENTIFIER,
            TEST_ENTITY_A_IDENTIFIER,
            TEST_ENTITY_C_IDENTIFIER,
            "controls"
        );

    relation_ca =
        test_graph_create_relation(
            TEST_RELATION_CA_IDENTIFIER,
            TEST_ENTITY_C_IDENTIFIER,
            TEST_ENTITY_A_IDENTIFIER,
            "knows"
        );

    test_graph_add_relation_or_abort(
        graph_model,
        relation_ca
    );

    test_graph_add_relation_or_abort(
        graph_model,
        relation_ac
    );

    test_graph_add_relation_or_abort(
        graph_model,
        relation_ab
    );

    outgoing_a =
        investigation_graph_model_list_outgoing_relations(
            graph_model,
            TEST_ENTITY_A_IDENTIFIER,
            &error
        );

    assert(outgoing_a != NULL);
    assert(error == NULL);
    assert(outgoing_a->len == 2);

    assert(
        g_ptr_array_index(
            outgoing_a,
            0
        ) == relation_ab
    );

    assert(
        g_ptr_array_index(
            outgoing_a,
            1
        ) == relation_ac
    );

    incoming_a =
        investigation_graph_model_list_incoming_relations(
            graph_model,
            TEST_ENTITY_A_IDENTIFIER,
            &error
        );

    assert(incoming_a != NULL);
    assert(error == NULL);
    assert(incoming_a->len == 1);

    assert(
        g_ptr_array_index(
            incoming_a,
            0
        ) == relation_ca
    );

    incident_a =
        investigation_graph_model_list_incident_relations(
            graph_model,
            TEST_ENTITY_A_IDENTIFIER,
            &error
        );

    assert(incident_a != NULL);
    assert(error == NULL);
    assert(incident_a->len == 3);

    assert(
        g_ptr_array_index(
            incident_a,
            0
        ) == relation_ab
    );

    assert(
        g_ptr_array_index(
            incident_a,
            1
        ) == relation_ac
    );

    assert(
        g_ptr_array_index(
            incident_a,
            2
        ) == relation_ca
    );

    /*
     * B ne possède aucune relation sortante.
     */
    outgoing_b =
        investigation_graph_model_list_outgoing_relations(
            graph_model,
            TEST_ENTITY_B_IDENTIFIER,
            &error
        );

    assert(outgoing_b != NULL);
    assert(error == NULL);
    assert(outgoing_b->len == 0);

    g_ptr_array_unref(
        outgoing_b
    );

    g_ptr_array_unref(
        incident_a
    );

    g_ptr_array_unref(
        incoming_a
    );

    g_ptr_array_unref(
        outgoing_a
    );

    investigation_graph_model_free(
        graph_model
    );
}

/**
 * @brief Vérifie les erreurs de navigation par entité.
 */
static void test_graph_navigation_errors(void)
{
    InvestigationGraphModel *graph_model =
        NULL;

    GPtrArray *relations =
        NULL;

    GError *error =
        NULL;

    graph_model =
        investigation_graph_model_new(
            &error
        );

    assert(graph_model != NULL);
    assert(error == NULL);

    relations =
        investigation_graph_model_list_outgoing_relations(
            graph_model,
            "identifiant-invalide",
            &error
        );

    assert(relations == NULL);

    test_graph_assert_error(
        error,
        INVESTIGATION_GRAPH_MODEL_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    relations =
        investigation_graph_model_list_incoming_relations(
            graph_model,
            TEST_ENTITY_A_IDENTIFIER,
            &error
        );

    assert(relations == NULL);

    test_graph_assert_error(
        error,
        INVESTIGATION_GRAPH_MODEL_ERROR_ENTITY_NOT_FOUND
    );

    g_clear_error(
        &error
    );

    relations =
        investigation_graph_model_list_incident_relations(
            NULL,
            TEST_ENTITY_A_IDENTIFIER,
            &error
        );

    assert(relations == NULL);

    test_graph_assert_error(
        error,
        INVESTIGATION_GRAPH_MODEL_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    investigation_graph_model_free(
        graph_model
    );
}

/**
 * @brief Vérifie les valeurs défensives pour un graphe NULL.
 */
static void test_graph_null_accessors(void)
{
    GPtrArray *items =
        NULL;

    GError *error =
        NULL;

    assert(
        investigation_graph_model_get_entity_count(
            NULL
        ) == 0
    );

    assert(
        investigation_graph_model_get_relation_count(
            NULL
        ) == 0
    );

    assert(
        investigation_graph_model_find_entity(
            NULL,
            TEST_ENTITY_A_IDENTIFIER
        ) == NULL
    );

    assert(
        investigation_graph_model_find_relation(
            NULL,
            TEST_RELATION_AB_IDENTIFIER
        ) == NULL
    );

    items =
        investigation_graph_model_list_entities(
            NULL,
            &error
        );

    assert(items == NULL);

    test_graph_assert_error(
        error,
        INVESTIGATION_GRAPH_MODEL_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );

    items =
        investigation_graph_model_list_relations(
            NULL,
            &error
        );

    assert(items == NULL);

    test_graph_assert_error(
        error,
        INVESTIGATION_GRAPH_MODEL_ERROR_INVALID_ARGUMENT
    );

    g_clear_error(
        &error
    );
}

/**
 * @brief Vérifie que GError est facultatif.
 */
static void test_graph_optional_error(void)
{
    InvestigationGraphModel *graph_model =
        NULL;

    EntityRecord *duplicate_entity =
        NULL;

    graph_model =
        investigation_graph_model_new(
            NULL
        );

    assert(graph_model != NULL);

    test_graph_add_entity_or_abort(
        graph_model,
        test_graph_create_entity(
            TEST_ENTITY_A_IDENTIFIER,
            "Personne A"
        )
    );

    duplicate_entity =
        test_graph_create_entity(
            TEST_ENTITY_A_IDENTIFIER,
            "Duplicata"
        );

    assert(
        !investigation_graph_model_add_entity(
            graph_model,
            duplicate_entity,
            NULL
        )
    );

    entity_record_free(
        duplicate_entity
    );

    investigation_graph_model_free(
        graph_model
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

    test_graph_empty_lifecycle();
    test_graph_add_and_find_entity();
    test_graph_reject_null_entity();
    test_graph_reject_duplicate_entity();
    test_graph_list_entities_sorted();
    test_graph_add_and_find_relation();
    test_graph_reject_null_relation();
    test_graph_reject_duplicate_relation();
    test_graph_reject_missing_source();
    test_graph_reject_missing_target();
    test_graph_list_relations_sorted();
    test_graph_navigation_indexes();
    test_graph_navigation_errors();
    test_graph_null_accessors();
    test_graph_optional_error();

    printf(
        "InvestigationGraphModel : tous les tests sont valides.\n"
    );

    return 0;
}
