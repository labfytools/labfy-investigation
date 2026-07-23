/******************************************************************************
 * @file extraction_drop_service.c
 * @brief Validation et intégration d'une extraction déposée sur le graphe.
 ******************************************************************************/

#include "core/extraction_drop_service.h"

#include "core/file_hash.h"
#include "dao/entity_dao.h"
#include "dao/evidence_dao.h"
#include "dao/evidence_entity_dao.h"
#include "database/transaction.h"
#include "models/entity_record.h"
#include "models/evidence_record.h"

#include <string.h>

struct ExtractionDropInfo
{
    char *file_path;
    char *relative_path;
    char *file_name;
    char *source;
    char *content;
};

GQuark extraction_drop_service_error_quark(void)
{
    return g_quark_from_static_string("extraction-drop-service-error");
}

static void extraction_drop_service_set_error(
    GError **error,
    ExtractionDropServiceError code,
    const char *message)
{
    if (error != NULL && *error == NULL)
    {
        g_set_error_literal(error, EXTRACTION_DROP_SERVICE_ERROR, code, message);
    }
}

static char *extraction_drop_service_timestamp(void)
{
    GDateTime *now = g_date_time_new_now_utc();
    char *timestamp = now != NULL
        ? g_date_time_format(now, "%Y-%m-%dT%H:%M:%SZ") : NULL;
    g_clear_pointer(&now, g_date_time_unref);
    return timestamp;
}

static char *extraction_drop_service_source_from_content(const char *content)
{
    const char *line_end = NULL;
    if (content == NULL || content[0] == '\0')
    {
        return g_strdup("Extraction locale");
    }
    line_end = strchr(content, '\n');
    if (line_end != NULL && line_end > content)
    {
        return g_strndup(content, (gsize) (line_end - content));
    }
    return g_strdup(content);
}

ExtractionDropInfo *extraction_drop_service_prepare(
    const char *investigation_root,
    const char *file_path,
    GError **error)
{
    ExtractionDropInfo *info = NULL;
    char *canonical_root = NULL;
    char *extractions_root = NULL;
    char *canonical_extractions_root = NULL;
    char *canonical_file = NULL;
    char *content = NULL;
    gsize content_length = 0U;

    g_return_val_if_fail(error == NULL || *error == NULL, NULL);
    if (investigation_root == NULL || investigation_root[0] == '\0' ||
        file_path == NULL || file_path[0] == '\0')
    {
        extraction_drop_service_set_error(error,
            EXTRACTION_DROP_SERVICE_ERROR_INVALID_ARGUMENT,
            "Le chemin de l'extraction est invalide.");
        return NULL;
    }

    canonical_root = g_canonicalize_filename(investigation_root, NULL);
    extractions_root = g_build_filename(canonical_root,
        "02_Preuves_Traitees", "Extractions", NULL);
    canonical_extractions_root = g_canonicalize_filename(extractions_root, NULL);
    canonical_file = g_canonicalize_filename(file_path, NULL);
    if (canonical_root == NULL || canonical_extractions_root == NULL ||
        canonical_file == NULL)
    {
        extraction_drop_service_set_error(error,
            EXTRACTION_DROP_SERVICE_ERROR_INVALID_ARGUMENT,
            "Impossible de normaliser le chemin de l'extraction.");
        goto cleanup;
    }
    if (!g_str_has_prefix(canonical_file, canonical_extractions_root) ||
        canonical_file[strlen(canonical_extractions_root)] != G_DIR_SEPARATOR)
    {
        extraction_drop_service_set_error(error,
            EXTRACTION_DROP_SERVICE_ERROR_OUTSIDE_INVESTIGATION,
            "Le fichier ne se trouve pas dans les extractions de l'enquête.");
        goto cleanup;
    }
    if (!g_file_test(canonical_file, G_FILE_TEST_IS_REGULAR) ||
        g_file_test(canonical_file, G_FILE_TEST_IS_SYMLINK) ||
        !g_str_has_suffix(canonical_file, ".txt"))
    {
        extraction_drop_service_set_error(error,
            EXTRACTION_DROP_SERVICE_ERROR_INVALID_FILE,
            "Seuls les fichiers texte réguliers d'extraction sont acceptés.");
        goto cleanup;
    }
    if (!g_file_get_contents(canonical_file, &content, &content_length, error))
    {
        if (error != NULL && *error != NULL)
        {
            (*error)->domain = EXTRACTION_DROP_SERVICE_ERROR;
            (*error)->code = EXTRACTION_DROP_SERVICE_ERROR_READ;
        }
        goto cleanup;
    }
    if (!g_utf8_validate(content, (gssize) content_length, NULL))
    {
        extraction_drop_service_set_error(error,
            EXTRACTION_DROP_SERVICE_ERROR_INVALID_FILE,
            "L'extraction texte n'est pas encodée en UTF-8.");
        goto cleanup;
    }

    info = g_new0(ExtractionDropInfo, 1);
    info->file_path = g_strdup(canonical_file);
    info->relative_path = g_strdup(canonical_file + strlen(canonical_root) + 1U);
    info->file_name = g_path_get_basename(canonical_file);
    info->content = g_steal_pointer(&content);
    info->source = extraction_drop_service_source_from_content(info->content);
    if (info->file_path == NULL || info->relative_path == NULL ||
        info->file_name == NULL || info->content == NULL || info->source == NULL)
    {
        extraction_drop_info_free(info);
        info = NULL;
        extraction_drop_service_set_error(error,
            EXTRACTION_DROP_SERVICE_ERROR_INVALID_ARGUMENT,
            "Impossible d'allouer les informations de l'extraction.");
    }

cleanup:
    g_free(content);
    g_free(canonical_file);
    g_free(canonical_extractions_root);
    g_free(extractions_root);
    g_free(canonical_root);
    return info;
}

void extraction_drop_info_free(ExtractionDropInfo *info)
{
    if (info == NULL) return;
    g_free(info->file_path);
    g_free(info->relative_path);
    g_free(info->file_name);
    g_free(info->source);
    g_free(info->content);
    g_free(info);
}

#define EXTRACTION_DROP_GETTER(name, field) \
const char *name(const ExtractionDropInfo *info) \
{ return info != NULL ? info->field : NULL; }

EXTRACTION_DROP_GETTER(extraction_drop_info_get_file_path, file_path)
EXTRACTION_DROP_GETTER(extraction_drop_info_get_relative_path, relative_path)
EXTRACTION_DROP_GETTER(extraction_drop_info_get_file_name, file_name)
EXTRACTION_DROP_GETTER(extraction_drop_info_get_source, source)
EXTRACTION_DROP_GETTER(extraction_drop_info_get_content, content)

static char *extraction_drop_service_ensure_evidence(
    Database *database,
    const ExtractionDropInfo *info,
    GError **error)
{
    EvidenceDao *dao = NULL;
    EvidenceRecord *record = NULL;
    GPtrArray *records = NULL;
    char *identifier = NULL;
    char *sha256 = NULL;
    char *timestamp = NULL;
    guint64 size_bytes = 0U;

    dao = evidence_dao_new(database, error);
    if (dao == NULL) goto cleanup;
    records = evidence_dao_list_all(dao, error);
    if (records == NULL) goto cleanup;
    for (guint index = 0U; index < records->len; index++)
    {
        const EvidenceRecord *candidate = g_ptr_array_index(records, index);
        if (g_strcmp0(evidence_record_get_relative_path(candidate),
                info->relative_path) == 0)
        {
            identifier = g_strdup(evidence_record_get_identifier(candidate));
            goto cleanup;
        }
    }
    if (!file_hash_compute_sha256(info->file_path, NULL, &sha256,
            &size_bytes, error)) goto cleanup;
    identifier = g_uuid_string_random();
    timestamp = extraction_drop_service_timestamp();
    record = evidence_record_new(identifier, info->file_name, info->file_name,
        info->relative_path, "text", size_bytes, sha256, timestamp, NULL,
        info->source, "Extraction intégrée depuis le graphe.",
        EVIDENCE_INTEGRITY_STATUS_VALID, error);
    if (record == NULL || !evidence_dao_insert(dao, record, error))
    {
        g_clear_pointer(&identifier, g_free);
    }

cleanup:
    evidence_record_free(record);
    g_clear_pointer(&records, g_ptr_array_unref);
    evidence_dao_free(dao);
    g_free(timestamp);
    g_free(sha256);
    return identifier;
}

static gboolean extraction_drop_service_attach_in_transaction(
    Database *database,
    const ExtractionDropInfo *info,
    const char *entity_identifier,
    GError **error)
{
    EvidenceEntityDao *link_dao = NULL;
    char *evidence_identifier = NULL;
    gboolean exists = FALSE;
    gboolean success = FALSE;

    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    if (database == NULL || info == NULL || entity_identifier == NULL ||
        !g_uuid_string_is_valid(entity_identifier))
    {
        extraction_drop_service_set_error(error,
            EXTRACTION_DROP_SERVICE_ERROR_INVALID_ARGUMENT,
            "L'entité de destination est invalide.");
        return FALSE;
    }
    evidence_identifier = extraction_drop_service_ensure_evidence(database,
        info, error);
    if (evidence_identifier == NULL) goto cleanup;
    link_dao = evidence_entity_dao_new(database, error);
    if (link_dao == NULL ||
        !evidence_entity_dao_exists(link_dao, evidence_identifier,
            entity_identifier, &exists, error) ||
        (!exists && !evidence_entity_dao_link(link_dao, evidence_identifier,
            entity_identifier, error))) goto cleanup;
    success = TRUE;
cleanup:
    evidence_entity_dao_free(link_dao);
    g_free(evidence_identifier);
    return success;
}

gboolean extraction_drop_service_attach(
    Database *database,
    const ExtractionDropInfo *info,
    const char *entity_identifier,
    GError **error)
{
    gboolean success = FALSE;
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    if (database == NULL || info == NULL || entity_identifier == NULL ||
        !g_uuid_string_is_valid(entity_identifier))
    {
        extraction_drop_service_set_error(error,
            EXTRACTION_DROP_SERVICE_ERROR_INVALID_ARGUMENT,
            "L'entité de destination est invalide.");
        return FALSE;
    }
    if (!database_transaction_begin(database)) goto database_failure;
    if (!extraction_drop_service_attach_in_transaction(database, info,
            entity_identifier, error))
    {
        database_transaction_rollback(database);
        return FALSE;
    }
    if (!database_transaction_commit(database)) goto database_failure;
    success = TRUE;
    return success;

database_failure:
    extraction_drop_service_set_error(error,
        EXTRACTION_DROP_SERVICE_ERROR_DATABASE,
        "Impossible d'enregistrer l'association de l'extraction.");
    return FALSE;
}

gboolean extraction_drop_service_create_entity(
    Database *database,
    const ExtractionDropInfo *info,
    char **out_entity_identifier,
    GError **error)
{
    EntityDao *entity_dao = NULL;
    EntityRecord *entity = NULL;
    char *identifier = NULL;
    char *timestamp = NULL;
    gboolean success = FALSE;

    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    if (out_entity_identifier != NULL) *out_entity_identifier = NULL;
    if (database == NULL || info == NULL)
    {
        extraction_drop_service_set_error(error,
            EXTRACTION_DROP_SERVICE_ERROR_INVALID_ARGUMENT,
            "Les paramètres de création d'entité sont invalides.");
        return FALSE;
    }
    identifier = g_uuid_string_random();
    timestamp = extraction_drop_service_timestamp();
    entity = entity_record_new(identifier, "other", info->file_name,
        info->file_name, info->content, 50, timestamp, timestamp,
        ENTITY_STATUS_ACTIVE, error);
    if (!database_transaction_begin(database))
    {
        extraction_drop_service_set_error(error,
            EXTRACTION_DROP_SERVICE_ERROR_DATABASE,
            "Impossible de démarrer la création de l'entité.");
        goto cleanup;
    }
    entity_dao = entity_dao_new(database, error);
    if (entity == NULL || entity_dao == NULL ||
        !entity_dao_insert(entity_dao, entity, error) ||
        !extraction_drop_service_attach_in_transaction(database, info,
            identifier, error))
    {
        database_transaction_rollback(database);
        goto cleanup;
    }
    if (!database_transaction_commit(database))
    {
        extraction_drop_service_set_error(error,
            EXTRACTION_DROP_SERVICE_ERROR_DATABASE,
            "Impossible de valider la création de l'entité.");
        goto cleanup;
    }
    if (out_entity_identifier != NULL)
        *out_entity_identifier = g_strdup(identifier);
    success = TRUE;

cleanup:
    entity_dao_free(entity_dao);
    entity_record_free(entity);
    g_free(timestamp);
    g_free(identifier);
    return success;
}
