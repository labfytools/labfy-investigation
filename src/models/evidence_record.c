/******************************************************************************
 * @file evidence_record.c
 * @brief Implémentation du modèle EvidenceRecord.
 ******************************************************************************/

#include "models/evidence_record.h"

#include <glib.h>
#include <string.h>

/**
 * @brief Représentation privée d’une preuve numérique persistée.
 */
struct EvidenceRecord
{
    char *identifier;
    char *original_name;
    char *internal_name;
    char *relative_path;
    char *type_identifier;
    char *type_label;
    char *mime_type;

    guint64 size_bytes;

    char *sha256;
    char *imported_at;
    char *collected_at;
    char *source;
    char *description;

    EvidenceIntegrityStatus integrity_status;
};

EvidenceRecord *evidence_record_copy(const EvidenceRecord *evidence_record)
{
    EvidenceRecord *copy;
    if (evidence_record == NULL) return NULL;
    copy = g_new0(EvidenceRecord, 1);
    copy->identifier = g_strdup(evidence_record->identifier);
    copy->original_name = g_strdup(evidence_record->original_name);
    copy->internal_name = g_strdup(evidence_record->internal_name);
    copy->relative_path = g_strdup(evidence_record->relative_path);
    copy->type_identifier = g_strdup(evidence_record->type_identifier);
    copy->type_label = g_strdup(evidence_record->type_label);
    copy->mime_type = g_strdup(evidence_record->mime_type);
    copy->size_bytes = evidence_record->size_bytes;
    copy->sha256 = g_strdup(evidence_record->sha256);
    copy->imported_at = g_strdup(evidence_record->imported_at);
    copy->collected_at = g_strdup(evidence_record->collected_at);
    copy->source = g_strdup(evidence_record->source);
    copy->description = g_strdup(evidence_record->description);
    copy->integrity_status = evidence_record->integrity_status;
    return copy;
}

/**
 * @brief Produit une erreur du domaine EvidenceRecord.
 */
static void evidence_record_set_error(
    GError **error,
    EvidenceRecordError error_code,
    const char *message
)
{
    if (error == NULL)
    {
        return;
    }

    g_set_error_literal(
        error,
        EVIDENCE_RECORD_ERROR,
        error_code,
        message
    );
}

/**
 * @brief Copie une chaîne et retire les espaces extérieurs.
 *
 * @return Nouvelle chaîne, ou NULL lorsque text vaut NULL.
 */
static char *evidence_record_duplicate_trimmed(
    const char *text
)
{
    char *text_copy = NULL;

    if (text == NULL)
    {
        return NULL;
    }

    text_copy = g_strdup(
        text
    );

    g_strstrip(
        text_copy
    );

    return text_copy;
}

/**
 * @brief Indique si une chaîne obligatoire contient du texte.
 */
static gboolean evidence_record_required_text_is_valid(
    const char *text
)
{
    return text != NULL &&
           text[0] != '\0';
}

/**
 * @brief Vérifie qu’un nom ne contient aucun composant de chemin.
 */
static gboolean evidence_record_filename_is_valid(
    const char *filename
)
{
    if (!evidence_record_required_text_is_valid(
            filename
        ))
    {
        return FALSE;
    }

    if (g_strcmp0(
            filename,
            "."
        ) == 0 ||
        g_strcmp0(
            filename,
            ".."
        ) == 0)
    {
        return FALSE;
    }

    /*
     * Les deux séparateurs sont refusés, même si l’application cible
     * principalement les systèmes Unix.
     */
    if (strchr(
            filename,
            '/'
        ) != NULL ||
        strchr(
            filename,
            '\\'
        ) != NULL)
    {
        return FALSE;
    }

    return TRUE;
}

/**
 * @brief Vérifie qu’un chemin reste relatif à l’enquête.
 */
static gboolean evidence_record_relative_path_is_valid(
    const char *relative_path
)
{
    char **components = NULL;
    gsize component_index = 0;

    gboolean is_valid = TRUE;

    if (!evidence_record_required_text_is_valid(
            relative_path
        ))
    {
        return FALSE;
    }

    if (g_path_is_absolute(
            relative_path
        ))
    {
        return FALSE;
    }

    if (g_strcmp0(
            relative_path,
            "."
        ) == 0 ||
        g_strcmp0(
            relative_path,
            ".."
        ) == 0)
    {
        return FALSE;
    }

    if (relative_path[
            strlen(relative_path) - 1
        ] == G_DIR_SEPARATOR)
    {
        return FALSE;
    }

    components = g_strsplit(
        relative_path,
        G_DIR_SEPARATOR_S,
        -1
    );

    if (components == NULL)
    {
        return FALSE;
    }

    for (component_index = 0;
         components[component_index] != NULL;
         component_index++)
    {
        const char *component =
            components[component_index];

        if (component[0] == '\0' ||
            g_strcmp0(
                component,
                "."
            ) == 0 ||
            g_strcmp0(
                component,
                ".."
            ) == 0)
        {
            is_valid = FALSE;
            break;
        }
    }

    g_strfreev(
        components
    );

    return is_valid;
}

/**
 * @brief Vérifie qu’une empreinte contient 64 caractères hexadécimaux.
 */
static gboolean evidence_record_sha256_is_valid(
    const char *sha256
)
{
    gsize character_index = 0;

    if (sha256 == NULL ||
        strlen(sha256) != 64)
    {
        return FALSE;
    }

    for (character_index = 0;
         character_index < 64;
         character_index++)
    {
        if (!g_ascii_isxdigit(
                sha256[character_index]
            ))
        {
            return FALSE;
        }
    }

    return TRUE;
}

/**
 * @brief Convertit une empreinte hexadécimale en minuscules.
 */
static void evidence_record_sha256_to_lowercase(
    char *sha256
)
{
    gsize character_index = 0;

    if (sha256 == NULL)
    {
        return;
    }

    for (character_index = 0;
         sha256[character_index] != '\0';
         character_index++)
    {
        sha256[character_index] =
            g_ascii_tolower(
                sha256[character_index]
            );
    }
}

/**
 * @brief Vérifie une date ISO 8601 à la minute ou à la seconde avec fuseau.
 */
static gboolean evidence_record_utc_date_is_valid(
    const char *date_text
)
{
    GDateTime *date_time = NULL;
    char *normalized = NULL;
    gsize length = date_text != NULL ? strlen(date_text) : 0U;
    gboolean utc = length > 0U && date_text[length - 1U] == 'Z';
    gboolean offset = length >= 6U &&
        (date_text[length - 6U] == '+' || date_text[length - 6U] == '-') &&
        date_text[length - 3U] == ':';
    const char *zone = utc ? date_text + length - 1U :
        (offset ? date_text + length - 6U : NULL);

    if (zone == NULL || length < 17U || date_text[4] != '-' ||
        date_text[7] != '-' || date_text[10] != 'T' ||
        date_text[13] != ':') return FALSE;

    gsize clock_length = (gsize)(zone - date_text - 11);
    if (clock_length == 5U) {
        /* INVARIANT: normaliser pour GLib ne modifie pas la précision
         * persistée ; la chaîne d'origine reste conservée byte pour byte. */
        normalized = g_strdup_printf("%.*s:00%s", (int)(zone - date_text),
                                     date_text, zone);
    } else if (clock_length == 8U && date_text[16] == ':') {
        normalized = g_strdup(date_text);
    } else {
        return FALSE;
    }
    date_time = g_date_time_new_from_iso8601(normalized, NULL);
    g_free(normalized);

    if (date_time == NULL)
    {
        return FALSE;
    }

    g_date_time_unref(
        date_time
    );

    return TRUE;
}

/**
 * @brief Vérifie que la valeur appartient à EvidenceIntegrityStatus.
 */
static gboolean evidence_record_integrity_status_is_valid(
    EvidenceIntegrityStatus integrity_status
)
{
    switch (integrity_status)
    {
        case EVIDENCE_INTEGRITY_STATUS_UNKNOWN:
        case EVIDENCE_INTEGRITY_STATUS_VALID:
        case EVIDENCE_INTEGRITY_STATUS_MISSING:
        case EVIDENCE_INTEGRITY_STATUS_MODIFIED:
        case EVIDENCE_INTEGRITY_STATUS_ERROR:
            return TRUE;

        default:
            return FALSE;
    }
}

GQuark evidence_record_error_quark(void)
{
    return g_quark_from_static_string(
        "labfy-investigation-evidence-record-error"
    );
}

EvidenceRecord *evidence_record_new(
    const char *identifier,
    const char *original_name,
    const char *internal_name,
    const char *relative_path,
    const char *type_identifier,
    guint64 size_bytes,
    const char *sha256,
    const char *imported_at,
    const char *collected_at,
    const char *source,
    const char *description,
    EvidenceIntegrityStatus integrity_status,
    GError **error
)
{
    EvidenceRecord *evidence_record = NULL;

    char *identifier_copy = NULL;
    char *original_name_copy = NULL;
    char *internal_name_copy = NULL;
    char *relative_path_copy = NULL;
    char *type_identifier_copy = NULL;
    char *sha256_copy = NULL;
    char *imported_at_copy = NULL;
    char *collected_at_copy = NULL;
    char *source_copy = NULL;
    char *description_copy = NULL;

    g_return_val_if_fail(
        error == NULL || *error == NULL,
        NULL
    );

    identifier_copy =
        evidence_record_duplicate_trimmed(
            identifier
        );

    original_name_copy =
        evidence_record_duplicate_trimmed(
            original_name
        );

    internal_name_copy =
        evidence_record_duplicate_trimmed(
            internal_name
        );

    relative_path_copy =
        evidence_record_duplicate_trimmed(
            relative_path
        );

    type_identifier_copy =
        evidence_record_duplicate_trimmed(
            type_identifier
        );

    sha256_copy =
        evidence_record_duplicate_trimmed(
            sha256
        );

    imported_at_copy =
        evidence_record_duplicate_trimmed(
            imported_at
        );

    collected_at_copy =
        evidence_record_duplicate_trimmed(
            collected_at
        );

    source_copy =
        evidence_record_duplicate_trimmed(
            source
        );

    description_copy =
        evidence_record_duplicate_trimmed(
            description
        );

    if (!evidence_record_required_text_is_valid(
            identifier_copy
        ))
    {
        evidence_record_set_error(
            error,
            EVIDENCE_RECORD_ERROR_INVALID_IDENTIFIER,
            "L’identifiant de la preuve est obligatoire."
        );

        goto cleanup;
    }

    if (!g_uuid_string_is_valid(
            identifier_copy
        ))
    {
        evidence_record_set_error(
            error,
            EVIDENCE_RECORD_ERROR_INVALID_IDENTIFIER,
            "L’identifiant de la preuve n’est pas un UUID valide."
        );

        goto cleanup;
    }

    if (!evidence_record_filename_is_valid(
            original_name_copy
        ))
    {
        evidence_record_set_error(
            error,
            EVIDENCE_RECORD_ERROR_INVALID_NAME,
            "Le nom original de la preuve est invalide."
        );

        goto cleanup;
    }

    if (!evidence_record_filename_is_valid(
            internal_name_copy
        ))
    {
        evidence_record_set_error(
            error,
            EVIDENCE_RECORD_ERROR_INVALID_NAME,
            "Le nom interne de la preuve est invalide."
        );

        goto cleanup;
    }

    if (!evidence_record_relative_path_is_valid(
            relative_path_copy
        ))
    {
        evidence_record_set_error(
            error,
            EVIDENCE_RECORD_ERROR_INVALID_PATH,
            "Le chemin relatif de la preuve est invalide."
        );

        goto cleanup;
    }

    if (!evidence_record_required_text_is_valid(
            type_identifier_copy
        ))
    {
        evidence_record_set_error(
            error,
            EVIDENCE_RECORD_ERROR_INVALID_TYPE,
            "Le type de la preuve est obligatoire."
        );

        goto cleanup;
    }

    if (!evidence_record_sha256_is_valid(
            sha256_copy
        ))
    {
        evidence_record_set_error(
            error,
            EVIDENCE_RECORD_ERROR_INVALID_SHA256,
            "L’empreinte SHA-256 de la preuve est invalide."
        );

        goto cleanup;
    }

    if (!evidence_record_utc_date_is_valid(
            imported_at_copy
        ))
    {
        evidence_record_set_error(
            error,
            EVIDENCE_RECORD_ERROR_INVALID_DATE,
            "La date d’importation est invalide."
        );

        goto cleanup;
    }

    if (collected_at_copy != NULL &&
        !evidence_record_utc_date_is_valid(
            collected_at_copy
        ))
    {
        evidence_record_set_error(
            error,
            EVIDENCE_RECORD_ERROR_INVALID_DATE,
            "La date de collecte est invalide."
        );

        goto cleanup;
    }

    if (!evidence_record_integrity_status_is_valid(
            integrity_status
        ))
    {
        evidence_record_set_error(
            error,
            EVIDENCE_RECORD_ERROR_INVALID_STATUS,
            "Le statut d’intégrité de la preuve est invalide."
        );

        goto cleanup;
    }

    /*
     * Les champs facultatifs composés uniquement d’espaces deviennent
     * NULL afin de ne pas conserver de chaînes vides.
     */
    if (source_copy != NULL &&
        source_copy[0] == '\0')
    {
        g_clear_pointer(
            &source_copy,
            g_free
        );
    }

    if (description_copy != NULL &&
        description_copy[0] == '\0')
    {
        g_clear_pointer(
            &description_copy,
            g_free
        );
    }

    evidence_record_sha256_to_lowercase(
        sha256_copy
    );

    evidence_record = g_new0(
        EvidenceRecord,
        1
    );

    evidence_record->identifier =
        identifier_copy;

    evidence_record->original_name =
        original_name_copy;

    evidence_record->internal_name =
        internal_name_copy;

    evidence_record->relative_path =
        relative_path_copy;

    evidence_record->type_identifier =
        type_identifier_copy;

    evidence_record->size_bytes =
        size_bytes;

    evidence_record->sha256 =
        sha256_copy;

    evidence_record->imported_at =
        imported_at_copy;

    evidence_record->collected_at =
        collected_at_copy;

    evidence_record->source =
        source_copy;

    evidence_record->description =
        description_copy;

    evidence_record->integrity_status =
        integrity_status;

    return evidence_record;

cleanup:
    g_free(
        description_copy
    );

    g_free(
        source_copy
    );

    g_free(
        collected_at_copy
    );

    g_free(
        imported_at_copy
    );

    g_free(
        sha256_copy
    );

    g_free(
        type_identifier_copy
    );

    g_free(
        relative_path_copy
    );

    g_free(
        internal_name_copy
    );

    g_free(
        original_name_copy
    );

    g_free(
        identifier_copy
    );

    return NULL;
}

void evidence_record_free(
    EvidenceRecord *evidence_record
)
{
    if (evidence_record == NULL)
    {
        return;
    }

    g_free(
        evidence_record->description
    );

    g_free(
        evidence_record->source
    );

    g_free(
        evidence_record->collected_at
    );

    g_free(
        evidence_record->imported_at
    );

    g_free(
        evidence_record->sha256
    );

    g_free(
        evidence_record->type_identifier
    );
    g_free(evidence_record->type_label);
    g_free(evidence_record->mime_type);

    g_free(
        evidence_record->relative_path
    );

    g_free(
        evidence_record->internal_name
    );

    g_free(
        evidence_record->original_name
    );

    g_free(
        evidence_record->identifier
    );

    g_free(
        evidence_record
    );
}

const char *evidence_record_get_identifier(
    const EvidenceRecord *evidence_record
)
{
    if (evidence_record == NULL)
    {
        return NULL;
    }

    return evidence_record->identifier;
}

const char *evidence_record_get_original_name(
    const EvidenceRecord *evidence_record
)
{
    if (evidence_record == NULL)
    {
        return NULL;
    }

    return evidence_record->original_name;
}

const char *evidence_record_get_internal_name(
    const EvidenceRecord *evidence_record
)
{
    if (evidence_record == NULL)
    {
        return NULL;
    }

    return evidence_record->internal_name;
}

const char *evidence_record_get_relative_path(
    const EvidenceRecord *evidence_record
)
{
    if (evidence_record == NULL)
    {
        return NULL;
    }

    return evidence_record->relative_path;
}

const char *evidence_record_get_type_identifier(
    const EvidenceRecord *evidence_record
)
{
    if (evidence_record == NULL)
    {
        return NULL;
    }

    return evidence_record->type_identifier;
}

void evidence_record_set_display_metadata(EvidenceRecord *record,
    const char *type_label, const char *mime_type)
{
    if (record == NULL) return;
    g_free(record->type_label);
    g_free(record->mime_type);
    record->type_label = g_strdup(type_label);
    record->mime_type = g_strdup(mime_type);
}

const char *evidence_record_get_type_label(const EvidenceRecord *record)
{
    if (record == NULL) return NULL;
    return record->type_label != NULL
        ? record->type_label : record->type_identifier;
}

const char *evidence_record_get_mime_type(const EvidenceRecord *record)
{
    return record != NULL ? record->mime_type : NULL;
}

guint64 evidence_record_get_size_bytes(
    const EvidenceRecord *evidence_record
)
{
    if (evidence_record == NULL)
    {
        return 0;
    }

    return evidence_record->size_bytes;
}

const char *evidence_record_get_sha256(
    const EvidenceRecord *evidence_record
)
{
    if (evidence_record == NULL)
    {
        return NULL;
    }

    return evidence_record->sha256;
}

const char *evidence_record_get_imported_at(
    const EvidenceRecord *evidence_record
)
{
    if (evidence_record == NULL)
    {
        return NULL;
    }

    return evidence_record->imported_at;
}

const char *evidence_record_get_collected_at(
    const EvidenceRecord *evidence_record
)
{
    if (evidence_record == NULL)
    {
        return NULL;
    }

    return evidence_record->collected_at;
}

const char *evidence_record_get_source(
    const EvidenceRecord *evidence_record
)
{
    if (evidence_record == NULL)
    {
        return NULL;
    }

    return evidence_record->source;
}

const char *evidence_record_get_description(
    const EvidenceRecord *evidence_record
)
{
    if (evidence_record == NULL)
    {
        return NULL;
    }

    return evidence_record->description;
}

EvidenceIntegrityStatus evidence_record_get_integrity_status(
    const EvidenceRecord *evidence_record
)
{
    if (evidence_record == NULL)
    {
        return EVIDENCE_INTEGRITY_STATUS_UNKNOWN;
    }

    return evidence_record->integrity_status;
}
