#include "models/evidence_observation.h"
void evidence_observation_free(EvidenceObservation *observation)
{
    if (observation == NULL) return;
    g_free(observation->identifier); g_free(observation->value);
    g_free(observation->type_identifier); g_free(observation->role);
    g_free(observation->source_header); g_free(observation->provenance_kind);
    g_free(observation->verification_status); g_free(observation->integrated_at);
    g_free(observation->entity_identifier); g_free(observation->promotion_kind);
    g_free(observation);
}
