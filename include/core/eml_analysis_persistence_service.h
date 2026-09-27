/******************************************************************************
 * @file eml_analysis_persistence_service.h
 * @brief Analyse EML native puis publication durable de sa provenance.
 ******************************************************************************/

#ifndef LABFY_INVESTIGATION_EML_ANALYSIS_PERSISTENCE_SERVICE_H
#define LABFY_INVESTIGATION_EML_ANALYSIS_PERSISTENCE_SERVICE_H

#include "database/database.h"

#include <gio/gio.h>
#include <glib.h>

G_BEGIN_DECLS

#define EML_ANALYSIS_TOOL_ID "labfy.eml_analyzer"
#define EML_ANALYSIS_TOOL_VERSION "1"
#define EML_ANALYSIS_MAX_SOURCE_BYTES (4U * 1024U * 1024U)
#define EML_ANALYSIS_MAX_OBSERVATIONS 256U
#define EML_ANALYSIS_MAX_ARTIFACT_BYTES (1024U * 1024U)

typedef struct EmlAnalysisPersistenceService EmlAnalysisPersistenceService;
typedef struct EmlAnalysisPrepared EmlAnalysisPrepared;
typedef struct EmlAnalysisPublicationResult EmlAnalysisPublicationResult;

typedef enum
{
    EML_ANALYSIS_PERSISTENCE_ERROR_INVALID_ARGUMENT,
    EML_ANALYSIS_PERSISTENCE_ERROR_CANCELLED,
    EML_ANALYSIS_PERSISTENCE_ERROR_SOURCE,
    EML_ANALYSIS_PERSISTENCE_ERROR_LIMIT,
    EML_ANALYSIS_PERSISTENCE_ERROR_ANALYSIS,
    EML_ANALYSIS_PERSISTENCE_ERROR_CONFLICT,
    EML_ANALYSIS_PERSISTENCE_ERROR_RECOVERY_REQUIRED,
    EML_ANALYSIS_PERSISTENCE_ERROR_ARTIFACT,
    EML_ANALYSIS_PERSISTENCE_ERROR_DATABASE,
    EML_ANALYSIS_PERSISTENCE_ERROR_ROLLBACK
} EmlAnalysisPersistenceError;

#define EML_ANALYSIS_PERSISTENCE_ERROR \
    eml_analysis_persistence_error_quark()

GQuark eml_analysis_persistence_error_quark(void);

/**
 * Identités et date empruntées d'une demande. `request_identifier` devient
 * l'identité persistante de l'extraction ; `derivative_evidence_identifier`
 * reste une identité de preuve distincte. Les deux sont des UUID valides.
 */
typedef struct
{
    const char *request_identifier;
    const char *source_evidence_identifier;
    const char *derivative_evidence_identifier;
    const char *requested_at;
} EmlAnalysisPersistenceRequest;

/**
 * Crée un service qui emprunte la connexion d'écriture et copie la racine.
 * La racine doit être un répertoire réel existant. Le service ne l'ouvre pas
 * comme une enquête implicite : seul un EvidenceRecord persistant peut choisir
 * la preuve analysée.
 */
EmlAnalysisPersistenceService *eml_analysis_persistence_service_new(
    Database *database,
    const char *investigation_root,
    GError **error);

void eml_analysis_persistence_service_free(
    EmlAnalysisPersistenceService *service);

/**
 * Valide la preuve contrôlée, son chemin, sa taille et son hash, puis exécute
 * l'analyseur natif. Aucun fichier ni ligne SQLite n'est créé. Si la demande
 * existe déjà et est cohérente, le résultat idempotent est préparé sans
 * republier. L'annulation est contrôlée avant/après le parseur synchrone.
 */
EmlAnalysisPrepared *eml_analysis_persistence_service_prepare(
    EmlAnalysisPersistenceService *service,
    const EmlAnalysisPersistenceRequest *request,
    GCancellable *cancellable,
    GError **error);

void eml_analysis_prepared_free(EmlAnalysisPrepared *prepared);

/**
 * Publie le dérivé privé puis, dans une transaction unique, la preuve dérivée,
 * l'extraction et toutes les observations `proposed`. Un échec supprime
 * uniquement le fichier créé par cette tentative et annule les lignes.
 */
EmlAnalysisPublicationResult *eml_analysis_persistence_service_publish(
    EmlAnalysisPersistenceService *service,
    EmlAnalysisPrepared *prepared,
    GCancellable *cancellable,
    GError **error);

void eml_analysis_publication_result_free(
    EmlAnalysisPublicationResult *result);

const char *eml_analysis_publication_result_get_request_identifier(
    const EmlAnalysisPublicationResult *result);
const char *eml_analysis_publication_result_get_derivative_evidence_identifier(
    const EmlAnalysisPublicationResult *result);
const GPtrArray *eml_analysis_publication_result_get_observation_identifiers(
    const EmlAnalysisPublicationResult *result);
guint eml_analysis_publication_result_get_observation_count(
    const EmlAnalysisPublicationResult *result);
gboolean eml_analysis_publication_result_was_reused(
    const EmlAnalysisPublicationResult *result);
const char *eml_analysis_publication_result_get_status(
    const EmlAnalysisPublicationResult *result);
const GPtrArray *eml_analysis_publication_result_get_warnings(
    const EmlAnalysisPublicationResult *result);

G_END_DECLS

#endif
