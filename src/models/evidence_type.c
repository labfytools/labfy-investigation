/******************************************************************************
 * @file evidence_type.c
 * @brief Implémentation du modèle représentant un type de preuve.
 ******************************************************************************/

#include "models/evidence_type.h"

#include <glib.h>

/**
 * @struct EvidenceType
 * @brief Représentation interne d’un type de preuve.
 */
struct EvidenceType
{
    gint64 identifier;
    char *code;
    char *label;
    char *description;
};

/**
 * @brief Copie une chaîne après suppression des espaces périphériques.
 *
 * @param value Chaîne à copier.
 *
 * @return Nouvelle chaîne normalisée, ou NULL lorsque value vaut NULL.
 */
static char *evidence_type_duplicate_trimmed(
    const char *value
)
{
    char *copy = NULL;

    if (value == NULL)
    {
        return NULL;
    }

    copy = g_strdup(
        value
    );

    g_strstrip(
        copy
    );

    return copy;
}

/**
 * @brief Vérifie qu’une chaîne obligatoire contient du texte.
 *
 * @param value Chaîne déjà normalisée.
 *
 * @return TRUE lorsque la chaîne contient au moins un caractère.
 */
static gboolean evidence_type_required_value_is_valid(
    const char *value
)
{
    return value != NULL &&
           value[0] != '\0';
}

GQuark evidence_type_error_quark(void)
{
    return g_quark_from_static_string(
        "evidence-type-error-quark"
    );
}

EvidenceType *evidence_type_new(
    gint64 identifier,
    const char *code,
    const char *label,
    const char *description,
    GError **error
)
{
    EvidenceType *evidence_type = NULL;

    char *normalized_code = NULL;
    char *normalized_label = NULL;
    char *normalized_description = NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    if (identifier <= 0)
    {
        g_set_error_literal(
            error,
            EVIDENCE_TYPE_ERROR,
            EVIDENCE_TYPE_ERROR_INVALID_IDENTIFIER,
            "L’identifiant du type de preuve doit être strictement positif."
        );

        return NULL;
    }

    normalized_code =
        evidence_type_duplicate_trimmed(
            code
        );

    if (!evidence_type_required_value_is_valid(
            normalized_code
        ))
    {
        g_set_error_literal(
            error,
            EVIDENCE_TYPE_ERROR,
            EVIDENCE_TYPE_ERROR_INVALID_CODE,
            "Le code du type de preuve est obligatoire."
        );

        g_free(
            normalized_code
        );

        return NULL;
    }

    normalized_label =
        evidence_type_duplicate_trimmed(
            label
        );

    if (!evidence_type_required_value_is_valid(
            normalized_label
        ))
    {
        g_set_error_literal(
            error,
            EVIDENCE_TYPE_ERROR,
            EVIDENCE_TYPE_ERROR_INVALID_LABEL,
            "Le libellé du type de preuve est obligatoire."
        );

        g_free(
            normalized_code
        );

        g_free(
            normalized_label
        );

        return NULL;
    }

    normalized_description =
        evidence_type_duplicate_trimmed(
            description
        );

    /*
     * Une description vide ou composée uniquement d’espaces
     * est représentée par NULL.
     */
    if (normalized_description != NULL &&
        normalized_description[0] == '\0')
    {
        g_free(
            normalized_description
        );

        normalized_description = NULL;
    }

    evidence_type =
        g_new0(
            EvidenceType,
            1
        );

    evidence_type->identifier =
        identifier;

    evidence_type->code =
        normalized_code;

    evidence_type->label =
        normalized_label;

    evidence_type->description =
        normalized_description;

    return evidence_type;
}

void evidence_type_free(
    EvidenceType *evidence_type
)
{
    if (evidence_type == NULL)
    {
        return;
    }

    g_free(
        evidence_type->code
    );

    g_free(
        evidence_type->label
    );

    g_free(
        evidence_type->description
    );

    g_free(
        evidence_type
    );
}

gint64 evidence_type_get_identifier(
    const EvidenceType *evidence_type
)
{
    if (evidence_type == NULL)
    {
        return 0;
    }

    return evidence_type->identifier;
}

const char *evidence_type_get_code(
    const EvidenceType *evidence_type
)
{
    if (evidence_type == NULL)
    {
        return NULL;
    }

    return evidence_type->code;
}

const char *evidence_type_get_label(
    const EvidenceType *evidence_type
)
{
    if (evidence_type == NULL)
    {
        return NULL;
    }

    return evidence_type->label;
}

const char *evidence_type_get_description(
    const EvidenceType *evidence_type
)
{
    if (evidence_type == NULL)
    {
        return NULL;
    }

    return evidence_type->description;
}
