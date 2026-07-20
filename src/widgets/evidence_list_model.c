/******************************************************************************
 * @file evidence_list_model.c
 * @brief Implémentation du modèle de liste des preuves.
 ******************************************************************************/

#include "widgets/evidence_list_model.h"

#include "widgets/evidence_list_item.h"

/**
 * @brief État interne du modèle de liste.
 */
struct EvidenceListModel
{
    GListStore *items;
};

GQuark evidence_list_model_error_quark(void)
{
    return g_quark_from_static_string(
        "labfy-investigation-evidence-list-model-error"
    );
}

EvidenceListModel *evidence_list_model_new(void)
{
    EvidenceListModel *evidence_list_model =
        NULL;

    evidence_list_model =
        g_try_new0(
            EvidenceListModel,
            1
        );

    if (evidence_list_model == NULL)
    {
        return NULL;
    }

    evidence_list_model->items =
        g_list_store_new(
            EVIDENCE_TYPE_LIST_ITEM
        );

    if (evidence_list_model->items == NULL)
    {
        g_free(
            evidence_list_model
        );

        return NULL;
    }

    return evidence_list_model;
}

void evidence_list_model_free(
    EvidenceListModel *evidence_list_model
)
{
    if (evidence_list_model == NULL)
    {
        return;
    }

    g_clear_object(
        &evidence_list_model->items
    );

    g_free(
        evidence_list_model
    );
}

GListModel *evidence_list_model_get_list_model(
    EvidenceListModel *evidence_list_model
)
{
    if (evidence_list_model == NULL ||
        evidence_list_model->items == NULL)
    {
        return NULL;
    }

    return G_LIST_MODEL(
        evidence_list_model->items
    );
}

guint evidence_list_model_get_count(
    const EvidenceListModel *evidence_list_model
)
{
    if (evidence_list_model == NULL ||
        evidence_list_model->items == NULL)
    {
        return 0;
    }

    return g_list_model_get_n_items(
        G_LIST_MODEL(
            evidence_list_model->items
        )
    );
}

gboolean evidence_list_model_replace(
    EvidenceListModel *evidence_list_model,
    const GPtrArray *evidence_records,
    GError **error
)
{
    GPtrArray *new_items = NULL;

    guint record_index = 0;
    guint previous_count = 0;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (evidence_list_model == NULL ||
        evidence_list_model->items == NULL ||
        evidence_records == NULL)
    {
        g_set_error_literal(
            error,
            EVIDENCE_LIST_MODEL_ERROR,
            EVIDENCE_LIST_MODEL_ERROR_INVALID_ARGUMENT,
            "Le modèle ou le tableau de preuves est invalide."
        );

        return FALSE;
    }

    /*
     * Le tableau libère notre référence locale sur chaque objet.
     * GListStore prendra ensuite sa propre référence lors du splice.
     */
    new_items =
        g_ptr_array_new_with_free_func(
            g_object_unref
        );

    if (new_items == NULL)
    {
        g_set_error_literal(
            error,
            EVIDENCE_LIST_MODEL_ERROR,
            EVIDENCE_LIST_MODEL_ERROR_MEMORY,
            "Impossible d'allouer la nouvelle liste de preuves."
        );

        return FALSE;
    }

    for (record_index = 0;
         record_index < evidence_records->len;
         record_index++)
    {
        const EvidenceRecord *evidence_record =
            NULL;

        EvidenceListItem *evidence_list_item =
            NULL;

        evidence_record =
            g_ptr_array_index(
                evidence_records,
                record_index
            );

        evidence_list_item =
            evidence_list_item_new(
                evidence_record
            );

        if (evidence_list_item == NULL)
        {
            g_set_error(
                error,
                EVIDENCE_LIST_MODEL_ERROR,
                EVIDENCE_LIST_MODEL_ERROR_INVALID_RECORD,
                "La preuve située à l'index %u est invalide.",
                record_index
            );

            g_ptr_array_unref(
                new_items
            );

            return FALSE;
        }

        g_ptr_array_add(
            new_items,
            evidence_list_item
        );
    }

    previous_count =
        g_list_model_get_n_items(
            G_LIST_MODEL(
                evidence_list_model->items
            )
        );

    if (new_items->len == 0)
    {
        g_list_store_remove_all(
            evidence_list_model->items
        );
    }
    else
    {
        /*
         * Tous les éléments sont prêts.
         * Le remplacement peut maintenant être effectué en une opération.
         */
        g_list_store_splice(
            evidence_list_model->items,
            0,
            previous_count,
            new_items->pdata,
            new_items->len
        );
    }

    /*
     * GListStore possède désormais une référence sur chaque élément.
     */
    g_ptr_array_unref(
        new_items
    );

    return TRUE;
}
