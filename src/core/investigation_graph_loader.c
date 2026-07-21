/******************************************************************************
 * @file investigation_graph_loader.c
 * @brief Chargement du graphe métier d'une enquête depuis SQLite.
 ******************************************************************************/

#include "core/investigation_graph_loader.h"

#include "dao/entity_dao.h"
#include "dao/relation_dao.h"
#include "models/entity_record.h"
#include "models/relation_record.h"

#include <glib.h>

/**
 * @brief Représentation privée du chargeur.
 *
 * La connexion Database est empruntée.
 * Les DAO sont possédés.
 */
struct InvestigationGraphLoader
{
    Database *database;

    EntityDao *entity_dao;
    RelationDao *relation_dao;
};

/**
 * @brief Enregistre une erreur littérale du chargeur.
 */
static void investigation_graph_loader_set_error_literal(
    GError **error,
    InvestigationGraphLoaderError error_code,
    const char *message
)
{
    if (error == NULL ||
        *error != NULL)
    {
        return;
    }

    g_set_error_literal(
        error,
        INVESTIGATION_GRAPH_LOADER_ERROR,
        error_code,
        message
    );
}

/**
 * @brief Conserve le contexte d'une erreur provenant d'un autre composant.
 */
static void investigation_graph_loader_set_nested_error(
    GError **error,
    InvestigationGraphLoaderError error_code,
    const char *context,
    const GError *nested_error
)
{
    if (error == NULL ||
        *error != NULL)
    {
        return;
    }

    g_set_error(
        error,
        INVESTIGATION_GRAPH_LOADER_ERROR,
        error_code,
        "%s : %s",
        context,
        nested_error != NULL
            ? nested_error->message
            : "erreur inconnue"
    );
}

/**
 * @brief Transfère toutes les entités d'un tableau vers le graphe.
 *
 * Le tableau possède initialement tous ses EntityRecord.
 *
 * Chaque élément est retiré du tableau avant son ajout. En cas de succès, le
 * graphe en devient propriétaire. En cas d'échec, le modèle retiré est libéré
 * explicitement et les éléments restants restent possédés par le tableau.
 */
static gboolean investigation_graph_loader_transfer_entities(
    InvestigationGraphModel *graph_model,
    GPtrArray *entity_records,
    GError **error
)
{
    EntityRecord *entity_record =
        NULL;

    GError *graph_error =
        NULL;

    if (graph_model == NULL ||
        entity_records == NULL)
    {
        investigation_graph_loader_set_error_literal(
            error,
            INVESTIGATION_GRAPH_LOADER_ERROR_INVALID_ARGUMENT,
            "Le graphe ou la liste des entités est absent."
        );

        return FALSE;
    }

    while (entity_records->len > 0)
    {
        entity_record =
            g_ptr_array_steal_index(
                entity_records,
                0
            );

        if (entity_record == NULL)
        {
            investigation_graph_loader_set_error_literal(
                error,
                INVESTIGATION_GRAPH_LOADER_ERROR_ENTITY_TRANSFER,
                "La liste des entités contient un modèle absent."
            );

            return FALSE;
        }

        if (!investigation_graph_model_add_entity(
                graph_model,
                entity_record,
                &graph_error
            ))
        {
            investigation_graph_loader_set_nested_error(
                error,
                INVESTIGATION_GRAPH_LOADER_ERROR_ENTITY_TRANSFER,
                "Impossible de transférer une entité dans le graphe",
                graph_error
            );

            g_clear_error(
                &graph_error
            );

            entity_record_free(
                entity_record
            );

            return FALSE;
        }

        entity_record =
            NULL;
    }

    return TRUE;
}

/**
 * @brief Transfère toutes les relations d'un tableau vers le graphe.
 *
 * Le tableau possède initialement tous ses RelationRecord.
 *
 * Chaque élément est retiré du tableau avant son ajout. En cas de succès, le
 * graphe en devient propriétaire. En cas d'échec, le modèle retiré est libéré
 * explicitement et les éléments restants restent possédés par le tableau.
 */
static gboolean investigation_graph_loader_transfer_relations(
    InvestigationGraphModel *graph_model,
    GPtrArray *relation_records,
    GError **error
)
{
    RelationRecord *relation_record =
        NULL;

    GError *graph_error =
        NULL;

    if (graph_model == NULL ||
        relation_records == NULL)
    {
        investigation_graph_loader_set_error_literal(
            error,
            INVESTIGATION_GRAPH_LOADER_ERROR_INVALID_ARGUMENT,
            "Le graphe ou la liste des relations est absent."
        );

        return FALSE;
    }

    while (relation_records->len > 0)
    {
        relation_record =
            g_ptr_array_steal_index(
                relation_records,
                0
            );

        if (relation_record == NULL)
        {
            investigation_graph_loader_set_error_literal(
                error,
                INVESTIGATION_GRAPH_LOADER_ERROR_RELATION_TRANSFER,
                "La liste des relations contient un modèle absent."
            );

            return FALSE;
        }

        if (!investigation_graph_model_add_relation(
                graph_model,
                relation_record,
                &graph_error
            ))
        {
            investigation_graph_loader_set_nested_error(
                error,
                INVESTIGATION_GRAPH_LOADER_ERROR_RELATION_TRANSFER,
                "Impossible de transférer une relation dans le graphe",
                graph_error
            );

            g_clear_error(
                &graph_error
            );

            relation_record_free(
                relation_record
            );

            return FALSE;
        }

        relation_record =
            NULL;
    }

    return TRUE;
}

GQuark investigation_graph_loader_error_quark(void)
{
    return g_quark_from_static_string(
        "investigation-graph-loader-error-quark"
    );
}

InvestigationGraphLoader *investigation_graph_loader_new(
    Database *database,
    GError **error
)
{
    InvestigationGraphLoader *graph_loader =
        NULL;

    EntityDao *entity_dao =
        NULL;

    RelationDao *relation_dao =
        NULL;

    GError *dao_error =
        NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (database == NULL)
    {
        investigation_graph_loader_set_error_literal(
            error,
            INVESTIGATION_GRAPH_LOADER_ERROR_INVALID_ARGUMENT,
            "La connexion Database est obligatoire."
        );

        return NULL;
    }

    entity_dao =
        entity_dao_new(
            database,
            &dao_error
        );

    if (entity_dao == NULL)
    {
        investigation_graph_loader_set_nested_error(
            error,
            INVESTIGATION_GRAPH_LOADER_ERROR_ENTITY_LOAD,
            "Impossible de créer le DAO des entités",
            dao_error
        );

        g_clear_error(
            &dao_error
        );

        return NULL;
    }

    relation_dao =
        relation_dao_new(
            database,
            &dao_error
        );

    if (relation_dao == NULL)
    {
        investigation_graph_loader_set_nested_error(
            error,
            INVESTIGATION_GRAPH_LOADER_ERROR_RELATION_LOAD,
            "Impossible de créer le DAO des relations",
            dao_error
        );

        g_clear_error(
            &dao_error
        );

        entity_dao_free(
            entity_dao
        );

        return NULL;
    }

    graph_loader =
        g_try_new0(
            InvestigationGraphLoader,
            1
        );

    if (graph_loader == NULL)
    {
        relation_dao_free(
            relation_dao
        );

        entity_dao_free(
            entity_dao
        );

        investigation_graph_loader_set_error_literal(
            error,
            INVESTIGATION_GRAPH_LOADER_ERROR_MEMORY,
            "Impossible d'allouer le chargeur du graphe d'enquête."
        );

        return NULL;
    }

    graph_loader->database =
        database;

    graph_loader->entity_dao =
        entity_dao;

    graph_loader->relation_dao =
        relation_dao;

    return graph_loader;
}

void investigation_graph_loader_free(
    InvestigationGraphLoader *graph_loader
)
{
    if (graph_loader == NULL)
    {
        return;
    }

    relation_dao_free(
        graph_loader->relation_dao
    );

    entity_dao_free(
        graph_loader->entity_dao
    );

    graph_loader->relation_dao =
        NULL;

    graph_loader->entity_dao =
        NULL;

    graph_loader->database =
        NULL;

    g_free(
        graph_loader
    );
}

InvestigationGraphModel *investigation_graph_loader_load(
    InvestigationGraphLoader *graph_loader,
    GError **error
)
{
    InvestigationGraphModel *graph_model =
        NULL;

    GPtrArray *entity_records =
        NULL;

    GPtrArray *relation_records =
        NULL;

    GError *component_error =
        NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (graph_loader == NULL ||
        graph_loader->database == NULL ||
        graph_loader->entity_dao == NULL ||
        graph_loader->relation_dao == NULL)
    {
        investigation_graph_loader_set_error_literal(
            error,
            INVESTIGATION_GRAPH_LOADER_ERROR_INVALID_ARGUMENT,
            "Le chargeur du graphe d'enquête est invalide."
        );

        return NULL;
    }

    graph_model =
        investigation_graph_model_new(
            &component_error
        );

    if (graph_model == NULL)
    {
        investigation_graph_loader_set_nested_error(
            error,
            INVESTIGATION_GRAPH_LOADER_ERROR_MEMORY,
            "Impossible de créer le graphe d'enquête",
            component_error
        );

        g_clear_error(
            &component_error
        );

        return NULL;
    }

    entity_records =
        entity_dao_list_all(
            graph_loader->entity_dao,
            &component_error
        );

    if (entity_records == NULL)
    {
        investigation_graph_loader_set_nested_error(
            error,
            INVESTIGATION_GRAPH_LOADER_ERROR_ENTITY_LOAD,
            "Impossible de charger les entités depuis SQLite",
            component_error
        );

        goto failure;
    }

    g_clear_error(
        &component_error
    );

    if (!investigation_graph_loader_transfer_entities(
            graph_model,
            entity_records,
            error
        ))
    {
        goto failure;
    }

    g_ptr_array_unref(
        entity_records
    );

    entity_records =
        NULL;

    relation_records =
        relation_dao_list_all(
            graph_loader->relation_dao,
            &component_error
        );

    if (relation_records == NULL)
    {
        investigation_graph_loader_set_nested_error(
            error,
            INVESTIGATION_GRAPH_LOADER_ERROR_RELATION_LOAD,
            "Impossible de charger les relations depuis SQLite",
            component_error
        );

        goto failure;
    }

    g_clear_error(
        &component_error
    );

    if (!investigation_graph_loader_transfer_relations(
            graph_model,
            relation_records,
            error
        ))
    {
        goto failure;
    }

    g_ptr_array_unref(
        relation_records
    );

    relation_records =
        NULL;

    return graph_model;

failure:

    g_clear_error(
        &component_error
    );

    if (relation_records != NULL)
    {
        g_ptr_array_unref(
            relation_records
        );
    }

    if (entity_records != NULL)
    {
        g_ptr_array_unref(
            entity_records
        );
    }

    investigation_graph_model_free(
        graph_model
    );

    return NULL;
}
