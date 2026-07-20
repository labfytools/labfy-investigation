/******************************************************************************
 * @file evidence_list_item.c
 * @brief Implémentation de EvidenceListItem.
 ******************************************************************************/

#include "widgets/evidence_list_item.h"

/**
 * @brief Données privées copiées pour l'affichage GTK.
 */
struct _EvidenceListItem
{
    GObject parent_instance;

    char *identifier;
    char *original_name;
    char *type_identifier;
    guint64 size_bytes;
    char *collected_at;
    char *source;

    EvidenceIntegrityStatus integrity_status;
};

G_DEFINE_TYPE(
    EvidenceListItem,
    evidence_list_item,
    G_TYPE_OBJECT
)

/**
 * @brief Libère les données possédées par l'objet.
 */
static void evidence_list_item_finalize(
    GObject *object
)
{
    EvidenceListItem *evidence_list_item =
        EVIDENCE_LIST_ITEM(
            object
        );

    g_clear_pointer(
        &evidence_list_item->original_name,
        g_free
    );

    g_clear_pointer(
        &evidence_list_item->type_identifier,
        g_free
    );

    g_clear_pointer(
        &evidence_list_item->collected_at,
        g_free
    );

    g_clear_pointer(
        &evidence_list_item->source,
        g_free
    );

    G_OBJECT_CLASS(
        evidence_list_item_parent_class
    )->finalize(
        object
    );
}

/**
 * @brief Initialise la classe GObject.
 */
static void evidence_list_item_class_init(
    EvidenceListItemClass *evidence_list_item_class
)
{
    GObjectClass *object_class =
        G_OBJECT_CLASS(
            evidence_list_item_class
        );

    object_class->finalize =
        evidence_list_item_finalize;
}

/**
 * @brief Initialise une nouvelle instance vide.
 */
static void evidence_list_item_init(
    EvidenceListItem *evidence_list_item
)
{
    evidence_list_item->integrity_status =
        EVIDENCE_INTEGRITY_STATUS_UNKNOWN;
}

EvidenceListItem *evidence_list_item_new(
    const EvidenceRecord *evidence_record
)
{
    EvidenceListItem *evidence_list_item = NULL;

    const char *identifier = NULL;
    const char *original_name = NULL;
    const char *type_identifier = NULL;

    if (evidence_record == NULL)
    {
        return NULL;
    }

    identifier =
        evidence_record_get_identifier(
            evidence_record
        );

    original_name =
        evidence_record_get_original_name(
            evidence_record
        );

    type_identifier =
        evidence_record_get_type_identifier(
            evidence_record
        );

    /*
     * Ces deux valeurs sont obligatoires dans EvidenceRecord.
     * Cette vérification protège néanmoins l'adaptateur d'affichage.
     */
    if (identifier == NULL ||
        original_name == NULL ||
        original_name[0] == '\0' ||
        type_identifier == NULL ||
        type_identifier[0] == '\0')
    {
        return NULL;
    }

    evidence_list_item =
        g_object_new(
            EVIDENCE_TYPE_LIST_ITEM,
            NULL
        );

    evidence_list_item->identifier =
        g_strdup(
            identifier
        );

    if (evidence_list_item == NULL)
    {
        return NULL;
    }

    evidence_list_item->original_name =
        g_strdup(
            original_name
        );

    evidence_list_item->type_identifier =
        g_strdup(
            type_identifier
        );

    evidence_list_item->size_bytes =
        evidence_record_get_size_bytes(
            evidence_record
        );

    evidence_list_item->collected_at =
        g_strdup(
            evidence_record_get_collected_at(
                evidence_record
            )
        );

    evidence_list_item->source =
        g_strdup(
            evidence_record_get_source(
                evidence_record
            )
        );

    evidence_list_item->integrity_status =
        evidence_record_get_integrity_status(
            evidence_record
        );

    if (evidence_list_item->identifier == NULL ||
        evidence_list_item->original_name == NULL ||
        evidence_list_item->type_identifier == NULL)
    {
        g_object_unref(
            evidence_list_item
        );

        return NULL;
    }

    return evidence_list_item;
}

const char *evidence_list_item_get_identifier(
    const EvidenceListItem *evidence_list_item
)
{
    if (evidence_list_item == NULL)
    {
        return NULL;
    }

    return evidence_list_item->identifier;
}

const char *evidence_list_item_get_original_name(
    const EvidenceListItem *evidence_list_item
)
{
    if (evidence_list_item == NULL)
    {
        return NULL;
    }

    return evidence_list_item->original_name;
}

const char *evidence_list_item_get_type_identifier(
    const EvidenceListItem *evidence_list_item
)
{
    if (evidence_list_item == NULL)
    {
        return NULL;
    }

    return evidence_list_item->type_identifier;
}

guint64 evidence_list_item_get_size_bytes(
    const EvidenceListItem *evidence_list_item
)
{
    if (evidence_list_item == NULL)
    {
        return 0;
    }

    return evidence_list_item->size_bytes;
}

const char *evidence_list_item_get_collected_at(
    const EvidenceListItem *evidence_list_item
)
{
    if (evidence_list_item == NULL)
    {
        return NULL;
    }

    return evidence_list_item->collected_at;
}

const char *evidence_list_item_get_source(
    const EvidenceListItem *evidence_list_item
)
{
    if (evidence_list_item == NULL)
    {
        return NULL;
    }

    return evidence_list_item->source;
}

EvidenceIntegrityStatus evidence_list_item_get_integrity_status(
    const EvidenceListItem *evidence_list_item
)
{
    if (evidence_list_item == NULL)
    {
        return EVIDENCE_INTEGRITY_STATUS_UNKNOWN;
    }

    return evidence_list_item->integrity_status;
}

const char *evidence_list_item_get_integrity_status_label(
    const EvidenceListItem *evidence_list_item
)
{
    EvidenceIntegrityStatus integrity_status =
        evidence_list_item_get_integrity_status(
            evidence_list_item
        );

    switch (integrity_status)
    {
        case EVIDENCE_INTEGRITY_STATUS_VALID:
            return "Valide";

        case EVIDENCE_INTEGRITY_STATUS_MISSING:
            return "Fichier absent";

        case EVIDENCE_INTEGRITY_STATUS_MODIFIED:
            return "Modifiée";

        case EVIDENCE_INTEGRITY_STATUS_ERROR:
            return "Erreur";

        case EVIDENCE_INTEGRITY_STATUS_UNKNOWN:
        default:
            return "Non vérifiée";
    }
}
