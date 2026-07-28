#ifndef LABFY_INVESTIGATION_EVIDENCE_OBSERVATION_H
#define LABFY_INVESTIGATION_EVIDENCE_OBSERVATION_H
#include <glib.h>
typedef struct {
    char *identifier;
    char *value;
    char *type_identifier;
    char *role;
    char *source_header;
    guint occurrence;
    char *provenance_kind;
    char *verification_status;
    char *integrated_at;
    char *entity_identifier;
    char *promotion_kind;
} EvidenceObservation;
void evidence_observation_free(EvidenceObservation *observation);
#endif
