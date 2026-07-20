/******************************************************************************
 * @file evidence_category_item.c
 * @brief Implémentation d'une catégorie de preuves affichées.
 ******************************************************************************/

#include "widgets/evidence_category_item.h"

/**
 * @brief État interne de la catégorie.
 */
struct _EvidenceCategoryItem
{
    GObject parent_instance;

    char *type_identifier;
    char *label;

    GListStore *children;
};

G_DEFINE_TYPE(
    EvidenceCategoryItem,
    evidence_category_item,
    G_TYPE_OBJECT
)

/**
 * @brief Copie et nettoie une chaîne obligatoire.
 */
static char *evidence_category_item_duplicate_required_text(
    const char *value
)
{
    char *normalized_value = NULL;

    if (value == NULL)
    {
        return NULL;
    }

    normalized_value =
        g_strdup(
            value
        );

    if (normalized_value == NULL)
    {
        return NULL;
    }

    g_strstrip(
        normalized_value
    );

    if (normalized_value[0] == '\0')
    {
        g_free(
            normalized_value
        );

        return NULL;
    }

    return normalized_value;
}

/**
 * @brief Libère les ressources possédées par la catégorie.
 */
static void evidence_category_item_finalize(
    GObject *object
)
{
    EvidenceCategoryItem *evidence_category_item =
        EVIDENCE_CATEGORY_ITEM(
            object
        );

    g_clear_pointer(
        &evidence_category_item->type_identifier,
        g_free
    );

    g_clear_pointer(
        &evidence_category_item->label,
        g_free
    );

    g_clear_object(
        &evidence_category_item->children
    );

    G_OBJECT_CLASS(
        evidence_category_item_parent_class
    )->finalize(
        object
    );
}

/**
 * @brief Initialise la classe GObject.
 */
static void evidence_category_item_class_init(
    EvidenceCategoryItemClass *evidence_category_item_class
)
{
    GObjectClass *object_class =
        G_OBJECT_CLASS(
            evidence_category_item_class
        );

    object_class->finalize =
        evidence_category_item_finalize;
}

/**
 * @brief Initialise une catégorie vide.
 */
static void evidence_category_item_init(
    EvidenceCategoryItem *evidence_category_item
)
{
    evidence_category_item->children =
        g_list_store_new(
            EVIDENCE_TYPE_LIST_ITEM
        );
}

EvidenceCategoryItem *evidence_category_item_new(
    const char *type_identifier,
    const char *label
)
{
    EvidenceCategoryItem *evidence_category_item =
        NULL;

    char *normalized_type_identifier =
        NULL;

    char *normalized_label =
        NULL;

    normalized_type_identifier =
        evidence_category_item_duplicate_required_text(
            type_identifier
        );

    normalized_label =
        evidence_category_item_duplicate_required_text(
            label
        );

    if (normalized_type_identifier == NULL ||
        normalized_label == NULL)
    {
        g_free(
            normalized_label
        );

        g_free(
            normalized_type_identifier
        );

        return NULL;
    }

    evidence_category_item =
        g_object_new(
            EVIDENCE_TYPE_CATEGORY_ITEM,
            NULL
        );

    if (evidence_category_item == NULL ||
        evidence_category_item->children == NULL)
    {
        g_clear_object(
            &evidence_category_item
        );

        g_free(
            normalized_label
        );

        g_free(
            normalized_type_identifier
        );

        return NULL;
    }

    evidence_category_item->type_identifier =
        normalized_type_identifier;

    evidence_category_item->label =
        normalized_label;

    return evidence_category_item;
}

/**
 * @brief Compare deux preuves par nom original puis par identifiant.
 */
static gint evidence_category_item_compare_list_items(
    gconstpointer first_pointer,
    gconstpointer second_pointer,
    gpointer user_data
)
{
    const EvidenceListItem *first_item =
        first_pointer;

    const EvidenceListItem *second_item =
        second_pointer;

    const char *first_name = NULL;
    const char *second_name = NULL;

    gint comparison = 0;

    (void) user_data;

    first_name =
        evidence_list_item_get_original_name(
            first_item
        );

    second_name =
        evidence_list_item_get_original_name(
            second_item
        );

    comparison =
        g_utf8_collate(
            first_name,
            second_name
        );

    if (comparison != 0)
    {
        return comparison;
    }

    return g_strcmp0(
        evidence_list_item_get_identifier(
            first_item
        ),
        evidence_list_item_get_identifier(
            second_item
        )
    );
}

gboolean evidence_category_item_append(
    EvidenceCategoryItem *evidence_category_item,
    EvidenceListItem *evidence_list_item
)
{
    if (evidence_category_item == NULL ||
        evidence_category_item->children == NULL ||
        evidence_list_item == NULL)
    {
        return FALSE;
    }

    g_list_store_insert_sorted(
        evidence_category_item->children,
        G_OBJECT(
            evidence_list_item
        ),
        evidence_category_item_compare_list_items,
        NULL
    );

    return TRUE;
}

const char *evidence_category_item_get_type_identifier(
    const EvidenceCategoryItem *evidence_category_item
)
{
    if (evidence_category_item == NULL)
    {
        return NULL;
    }

    return evidence_category_item->type_identifier;
}

const char *evidence_category_item_get_label(
    const EvidenceCategoryItem *evidence_category_item
)
{
    if (evidence_category_item == NULL)
    {
        return NULL;
    }

    return evidence_category_item->label;
}

guint evidence_category_item_get_count(
    const EvidenceCategoryItem *evidence_category_item
)
{
    if (evidence_category_item == NULL ||
        evidence_category_item->children == NULL)
    {
        return 0;
    }

    return g_list_model_get_n_items(
        G_LIST_MODEL(
            evidence_category_item->children
        )
    );
}

GListModel *evidence_category_item_get_children(
    EvidenceCategoryItem *evidence_category_item
)
{
    if (evidence_category_item == NULL ||
        evidence_category_item->children == NULL)
    {
        return NULL;
    }

    return G_LIST_MODEL(
        evidence_category_item->children
    );
}
