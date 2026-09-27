/******************************************************************************
 * @file exiftool_persistence_service.h
 * @brief Publication V20 d'une analyse ExifTool issue du registre J4.
 ******************************************************************************/
#ifndef LABFY_INVESTIGATION_EXIFTOOL_PERSISTENCE_SERVICE_H
#define LABFY_INVESTIGATION_EXIFTOOL_PERSISTENCE_SERVICE_H

#include "core/local_capability_registry.h"
#include "database/database.h"

G_BEGIN_DECLS

#define EXIFTOOL_ARTIFACT_CONTRACT "labfy.exiftool.metadata.derivative.v1"
#define EXIFTOOL_PERSISTENCE_MAX_SOURCE (16U * 1024U * 1024U)

typedef struct ExiftoolPersistenceService ExiftoolPersistenceService;
typedef struct {
    const char *request_identifier;
    const char *source_evidence_identifier;
    const char *derivative_evidence_identifier;
    const char *requested_at;
} ExiftoolPersistenceRequest;
typedef struct {
    char *request_identifier;
    char *derivative_evidence_identifier;
    char *tool_version;
    guint metadata_count;
    gboolean reused;
    char *status;
} ExiftoolPublicationResult;

ExiftoolPersistenceService *exiftool_persistence_service_new(
    Database *database, const char *investigation_root,
    const LocalCapabilityRegistry *registry, GError **error);
void exiftool_persistence_service_free(ExiftoolPersistenceService *service);

/** Exécute via le registre puis publie ; aucun résultat partiel n'est publié. */
ExiftoolPublicationResult *exiftool_persistence_service_execute(
    ExiftoolPersistenceService *service,
    const ExiftoolPersistenceRequest *request,
    GCancellable *cancellable,
    GError **error);
void exiftool_publication_result_free(ExiftoolPublicationResult *result);

G_END_DECLS
#endif
