/******************************************************************************
 * @file investigation_record.c
 * @brief Implémentation du modèle InvestigationRecord.
 ******************************************************************************/

#include "models/investigation_record.h"

#include <glib.h>
#include <string.h>

/**
 * @brief Représentation privée d'une enquête persistée.
 */
struct InvestigationRecord
{
    char *id;
    char *name;
    char *root_path;
    char *created_at;
    char *updated_at;
};

/**
 * @brief Vérifie qu'une chaîne obligatoire est présente et non vide.
 */
static gboolean investigation_record_text_is_valid(
    const char *text
)
{
    return text != NULL && text[0] != '\0';
}

/**
 * @brief Copie une chaîne en utilisant une allocation non fatale.
 *
 * Contrairement à g_strdup(), cette fonction retourne NULL si
 * l'allocation échoue au lieu d'interrompre le programme.
 */
static char *investigation_record_copy_text(
    const char *text
)
{
    char *text_copy = NULL;
    gsize text_length = 0;

    if (text == NULL)
    {
        return NULL;
    }

    text_length = strlen(text);

    if (text_length == G_MAXSIZE)
    {
        return NULL;
    }

    text_copy = g_try_malloc(
        text_length + 1
    );

    if (text_copy == NULL)
    {
        return NULL;
    }

    memcpy(
        text_copy,
        text,
        text_length + 1
    );

    return text_copy;
}

InvestigationRecord *investigation_record_new(
    const char *id,
    const char *name,
    const char *root_path,
    const char *created_at,
    const char *updated_at
)
{
    InvestigationRecord *record = NULL;

    if (!investigation_record_text_is_valid(id) ||
        !investigation_record_text_is_valid(name) ||
        !investigation_record_text_is_valid(root_path) ||
        !investigation_record_text_is_valid(created_at) ||
        !investigation_record_text_is_valid(updated_at))
    {
        return NULL;
    }

    record = g_try_new0(
        InvestigationRecord,
        1
    );

    if (record == NULL)
    {
        return NULL;
    }

    record->id = investigation_record_copy_text(id);
    record->name = investigation_record_copy_text(name);
    record->root_path = investigation_record_copy_text(root_path);
    record->created_at = investigation_record_copy_text(created_at);
    record->updated_at = investigation_record_copy_text(updated_at);

    if (record->id == NULL ||
        record->name == NULL ||
        record->root_path == NULL ||
        record->created_at == NULL ||
        record->updated_at == NULL)
    {
        investigation_record_free(record);
        return NULL;
    }

    return record;
}

void investigation_record_free(
    InvestigationRecord *record
)
{
    if (record == NULL)
    {
        return;
    }

    g_free(record->updated_at);
    g_free(record->created_at);
    g_free(record->root_path);
    g_free(record->name);
    g_free(record->id);
    g_free(record);
}

const char *investigation_record_get_id(
    const InvestigationRecord *record
)
{
    if (record == NULL)
    {
        return NULL;
    }

    return record->id;
}

const char *investigation_record_get_name(
    const InvestigationRecord *record
)
{
    if (record == NULL)
    {
        return NULL;
    }

    return record->name;
}

const char *investigation_record_get_root_path(
    const InvestigationRecord *record
)
{
    if (record == NULL)
    {
        return NULL;
    }

    return record->root_path;
}

const char *investigation_record_get_created_at(
    const InvestigationRecord *record
)
{
    if (record == NULL)
    {
        return NULL;
    }

    return record->created_at;
}

const char *investigation_record_get_updated_at(
    const InvestigationRecord *record
)
{
    if (record == NULL)
    {
        return NULL;
    }

    return record->updated_at;
}
