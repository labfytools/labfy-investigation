/******************************************************************************
 * @file evidence_category_model.c
 * @brief Regroupement des preuves par catégorie pour l'affichage GTK.
 ******************************************************************************/

#include "widgets/evidence_category_model.h"

#include "widgets/evidence_category_item.h"
#include "widgets/evidence_list_item.h"

/**
 * @brief État interne du modèle.
 */
struct EvidenceCategoryModel
{
    GListStore *categories;
};

GQuark evidence_category_model_error_quark(void)
{
    return g_quark_from_static_string(
        "labfy-investigation-evidence-category-model-error"
    );
}

/**
 * @brief Compare deux catégories par libellé puis par code métier.
 */
static gint evidence_category_model_compare_categories(
    gconstpointer first_pointer,
    gconstpointer second_pointer,
    gpointer user_data
)
{
    const EvidenceCategoryItem *first_category =
        first_pointer;

    const EvidenceCategoryItem *second_category =
        second_pointer;

    const char *first_label = NULL;
    const char *second_label = NULL;

    gint comparison = 0;

    (void) user_data;

    first_label =
        evidence_category_item_get_label(
            first_category
        );

    second_label =
        evidence_category_item_get_label(
            second_category
        );

    comparison =
        g_utf8_collate(
            first_label,
            second_label
        );

    if (comparison != 0)
    {
        return comparison;
    }

    return g_strcmp0(
        evidence_category_item_get_type_identifier(
            first_category
        ),
        evidence_category_item_get_type_identifier(
            second_category
        )
    );
}

/**
 * @brief Vérifie qu'une chaîne obligatoire contient du texte.
 */
static gboolean evidence_category_model_value_has_text(
    const char *value
)
{
    return value != NULL &&
           value[0] != '\0';
}

/**
 * @brief Libère les références temporaires d'un tableau d'ajouts.
 */
static void evidence_category_model_release_additions(
    gpointer *additions,
    guint addition_count
)
{
    guint addition_index = 0;

    if (additions == NULL)
    {
        return;
    }

    for (addition_index = 0;
         addition_index < addition_count;
         addition_index++)
    {
        g_clear_object(
            &additions[addition_index]
        );
    }

    g_free(
        additions
    );
}

EvidenceCategoryModel *evidence_category_model_new(void)
{
    EvidenceCategoryModel *evidence_category_model =
        NULL;

    evidence_category_model =
        g_try_new0(
            EvidenceCategoryModel,
            1
        );

    if (evidence_category_model == NULL)
    {
        return NULL;
    }

    evidence_category_model->categories =
        g_list_store_new(
            EVIDENCE_TYPE_CATEGORY_ITEM
        );

    if (evidence_category_model->categories == NULL)
    {
        g_free(
            evidence_category_model
        );

        return NULL;
    }

    return evidence_category_model;
}

void evidence_category_model_free(
    EvidenceCategoryModel *evidence_category_model
)
{
    if (evidence_category_model == NULL)
    {
        return;
    }

    g_clear_object(
        &evidence_category_model->categories
    );

    g_free(
        evidence_category_model
    );
}

GListModel *evidence_category_model_get_list_model(
    EvidenceCategoryModel *evidence_category_model
)
{
    if (evidence_category_model == NULL ||
        evidence_category_model->categories == NULL)
    {
        return NULL;
    }

    return G_LIST_MODEL(
        evidence_category_model->categories
    );
}

guint evidence_category_model_get_count(
    const EvidenceCategoryModel *evidence_category_model
)
{
    if (evidence_category_model == NULL ||
        evidence_category_model->categories == NULL)
    {
        return 0;
    }

    return g_list_model_get_n_items(
        G_LIST_MODEL(
            evidence_category_model->categories
        )
    );
}

gboolean evidence_category_model_replace(
    EvidenceCategoryModel *evidence_category_model,
    GListModel *evidence_items,
    GHashTable *type_labels,
    GError **error
)
{
    GListStore *new_categories = NULL;

    GHashTable *categories_by_type =
        NULL;

    EvidenceListItem *evidence_list_item =
        NULL;

    EvidenceCategoryItem *evidence_category_item =
        NULL;

    gpointer *additions = NULL;

    const char *type_identifier = NULL;
    const char *category_label = NULL;

    guint evidence_count = 0;
    guint evidence_index = 0;

    guint new_category_count = 0;
    guint category_index = 0;
    guint previous_category_count = 0;

    gboolean replaced = FALSE;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        FALSE
    );

    if (evidence_category_model == NULL ||
        evidence_category_model->categories == NULL ||
        evidence_items == NULL)
    {
        g_set_error_literal(
            error,
            EVIDENCE_CATEGORY_MODEL_ERROR,
            EVIDENCE_CATEGORY_MODEL_ERROR_INVALID_ARGUMENT,
            "Le modèle de catégories ou la liste de preuves est invalide."
        );

        return FALSE;
    }

    if (!g_type_is_a(
            g_list_model_get_item_type(
                evidence_items
            ),
            EVIDENCE_TYPE_LIST_ITEM
        ))
    {
        g_set_error_literal(
            error,
            EVIDENCE_CATEGORY_MODEL_ERROR,
            EVIDENCE_CATEGORY_MODEL_ERROR_INVALID_ARGUMENT,
            "Le modèle fourni ne contient pas des EvidenceListItem."
        );

        return FALSE;
    }

    new_categories =
        g_list_store_new(
            EVIDENCE_TYPE_CATEGORY_ITEM
        );

    categories_by_type =
        g_hash_table_new_full(
            g_str_hash,
            g_str_equal,
            g_free,
            NULL
        );

    if (new_categories == NULL ||
        categories_by_type == NULL)
    {
        g_set_error_literal(
            error,
            EVIDENCE_CATEGORY_MODEL_ERROR,
            EVIDENCE_CATEGORY_MODEL_ERROR_MEMORY,
            "Impossible de préparer le regroupement des preuves."
        );

        goto cleanup;
    }

    evidence_count =
        g_list_model_get_n_items(
            evidence_items
        );

    for (evidence_index = 0;
         evidence_index < evidence_count;
         evidence_index++)
    {
        evidence_list_item =
            g_list_model_get_item(
                evidence_items,
                evidence_index
            );

        if (evidence_list_item == NULL ||
            !EVIDENCE_IS_LIST_ITEM(
                evidence_list_item
            ))
        {
            g_set_error(
                error,
                EVIDENCE_CATEGORY_MODEL_ERROR,
                EVIDENCE_CATEGORY_MODEL_ERROR_INVALID_ITEM,
                "La preuve située à l'index %u est invalide.",
                evidence_index
            );

            goto cleanup;
        }

        type_identifier =
            evidence_list_item_get_type_identifier(
                evidence_list_item
            );

        if (!evidence_category_model_value_has_text(
                type_identifier
            ))
        {
            g_set_error(
                error,
                EVIDENCE_CATEGORY_MODEL_ERROR,
                EVIDENCE_CATEGORY_MODEL_ERROR_INVALID_ITEM,
                "La preuve située à l'index %u ne possède aucun type.",
                evidence_index
            );

            goto cleanup;
        }

        evidence_category_item =
            g_hash_table_lookup(
                categories_by_type,
                type_identifier
            );

        if (evidence_category_item == NULL)
        {
            category_label = NULL;

            if (type_labels != NULL)
            {
                category_label =
                    g_hash_table_lookup(
                        type_labels,
                        type_identifier
                    );
            }

            /*
             * Un type inconnu reste affichable.
             * Son code métier devient son libellé de secours.
             */
            if (!evidence_category_model_value_has_text(
                    category_label
                ))
            {
                category_label =
                    type_identifier;
            }

            evidence_category_item =
                evidence_category_item_new(
                    type_identifier,
                    category_label
                );

            if (evidence_category_item == NULL)
            {
                g_set_error(
                    error,
                    EVIDENCE_CATEGORY_MODEL_ERROR,
                    EVIDENCE_CATEGORY_MODEL_ERROR_CATEGORY,
                    "Impossible de créer la catégorie '%s'.",
                    type_identifier
                );

                goto cleanup;
            }

            g_list_store_insert_sorted(
                new_categories,
                G_OBJECT(
                    evidence_category_item
                ),
                evidence_category_model_compare_categories,
                NULL
            );

            /*
             * La table emprunte le pointeur.
             * Le GListStore possède la référence.
             */
            g_hash_table_insert(
                categories_by_type,
                g_strdup(
                    type_identifier
                ),
                evidence_category_item
            );

            g_object_unref(
                evidence_category_item
            );
        }

        if (!evidence_category_item_append(
                evidence_category_item,
                evidence_list_item
            ))
        {
            g_set_error(
                error,
                EVIDENCE_CATEGORY_MODEL_ERROR,
                EVIDENCE_CATEGORY_MODEL_ERROR_CATEGORY,
                "Impossible d'ajouter la preuve %u à sa catégorie.",
                evidence_index
            );

            goto cleanup;
        }

        g_clear_object(
            &evidence_list_item
        );

        evidence_category_item =
            NULL;
    }

    new_category_count =
        g_list_model_get_n_items(
            G_LIST_MODEL(
                new_categories
            )
        );

    previous_category_count =
        g_list_model_get_n_items(
            G_LIST_MODEL(
                evidence_category_model->categories
            )
        );

    if (new_category_count == 0)
    {
        g_list_store_remove_all(
            evidence_category_model->categories
        );

        replaced = TRUE;

        goto cleanup;
    }

    additions =
        g_try_new0(
            gpointer,
            new_category_count
        );

    if (additions == NULL)
    {
        g_set_error_literal(
            error,
            EVIDENCE_CATEGORY_MODEL_ERROR,
            EVIDENCE_CATEGORY_MODEL_ERROR_MEMORY,
            "Impossible de préparer le remplacement des catégories."
        );

        goto cleanup;
    }

    for (category_index = 0;
         category_index < new_category_count;
         category_index++)
    {
        additions[category_index] =
            g_list_model_get_item(
                G_LIST_MODEL(
                    new_categories
                ),
                category_index
            );

        if (additions[category_index] == NULL)
        {
            g_set_error_literal(
                error,
                EVIDENCE_CATEGORY_MODEL_ERROR,
                EVIDENCE_CATEGORY_MODEL_ERROR_CATEGORY,
                "Impossible de lire une catégorie préparée."
            );

            goto cleanup;
        }
    }

    g_list_store_splice(
        evidence_category_model->categories,
        0,
        previous_category_count,
        additions,
        new_category_count
    );

    replaced = TRUE;

cleanup:

    g_clear_object(
        &evidence_list_item
    );

    evidence_category_model_release_additions(
        additions,
        new_category_count
    );

    if (categories_by_type != NULL)
    {
        g_hash_table_unref(
            categories_by_type
        );
    }

    g_clear_object(
        &new_categories
    );

    return replaced;
}
