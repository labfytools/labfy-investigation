#ifndef LABFY_INVESTIGATION_OBSERVATION_REVIEW_SERVICE_H
#define LABFY_INVESTIGATION_OBSERVATION_REVIEW_SERVICE_H

#include "database/database.h"

#include <glib.h>

G_BEGIN_DECLS

typedef struct ObservationReviewService ObservationReviewService;

typedef enum {
    OBSERVATION_REVIEW_ACTION_DECIDE,
    OBSERVATION_REVIEW_ACTION_CORRECT,
    OBSERVATION_REVIEW_ACTION_PROMOTE_CREATE,
    OBSERVATION_REVIEW_ACTION_PROMOTE_ATTACH,
    OBSERVATION_REVIEW_ACTION_WITHDRAW_PROMOTION
} ObservationReviewAction;

typedef enum {
    OBSERVATION_REVIEW_ERROR_INVALID_ARGUMENT,
    OBSERVATION_REVIEW_ERROR_NOT_FOUND,
    OBSERVATION_REVIEW_ERROR_CONFLICT,
    OBSERVATION_REVIEW_ERROR_UNSUPPORTED_TYPE,
    OBSERVATION_REVIEW_ERROR_DATABASE
} ObservationReviewError;

#define OBSERVATION_REVIEW_ERROR observation_review_error_quark()
GQuark observation_review_error_quark(void);

/**
 * CONTRACT: operation_identifier identifie une intention immuable. Un rejeu
 * strict retourne replayed=TRUE ; le même UUID avec une autre intention est
 * refusé. expected_revision commence à zéro et protège les onglets concurrents.
 *
 * Pour DECIDE, verification_status vaut proposed/confirmed/rejected/conflicted.
 * Pour CORRECT, corrected_value est la correction humaine distincte et non vide.
 * PROMOTE_CREATE crée exclusivement une entité ; PROMOTE_ATTACH exige son UUID.
 */
typedef struct {
    const char *operation_identifier;
    const char *evidence_identifier;
    const char *observation_identifier;
    guint64 expected_revision;
    ObservationReviewAction action;
    const char *verification_status;
    const char *corrected_value;
    const char *entity_identifier;
    const char *author;
    const char *reason;
    const char *occurred_at;
} ObservationReviewRequest;

typedef struct {
    gboolean replayed;
    guint64 revision;
    char *entity_identifier;
    gboolean entity_created;
} ObservationReviewResult;

ObservationReviewService *observation_review_service_new(
    Database *database, GError **error);
void observation_review_service_free(ObservationReviewService *service);
void observation_review_result_clear(ObservationReviewResult *result);

gboolean observation_review_service_apply(
    ObservationReviewService *service,
    const ObservationReviewRequest *request,
    ObservationReviewResult *out_result,
    GError **error);

/**
 * Vérifie un rejeu d'import sans exiger que les projections de revue soient
 * restées à leur valeur initiale.
 *
 * CONTRACT: tous les champs d'extraction immuables doivent être identiques.
 * Une correction, décision ou promotion n'est tolérée que si le journal v1
 * contient au moins une opération de revue pour cette observation.
 */
gboolean observation_review_service_validate_replay(
    ObservationReviewService *service,
    const char *evidence_identifier,
    const char *observation_identifier,
    const char *extraction_identifier,
    const char *entity_type,
    const char *value_raw,
    const char *value_normalized,
    const char *role,
    const char *provenance_kind,
    const char *source_header,
    guint occurrence,
    gboolean *out_accepted,
    GError **error);

G_END_DECLS

#endif
