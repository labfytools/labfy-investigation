#ifndef LABFY_INVESTIGATION_EVIDENCE_OBSERVATION_H
#define LABFY_INVESTIGATION_EVIDENCE_OBSERVATION_H
#include <glib.h>
/**
 * @brief Vue typée possédée d'une observation persistée.
 *
 * CONTRACT: value reste la valeur d'affichage historique
 * corrected/normalized/raw. Les trois valeurs sources demeurent séparées afin
 * qu'une projection ne reconstruise jamais la provenance depuis ce libellé.
 * Toutes les chaînes appartiennent à la structure et peuvent être NULL lorsque
 * la colonne SQLite correspondante est absente.
 */
typedef struct {
    char *identifier;
    char *evidence_identifier;
    char *value;
    char *value_raw;
    char *value_normalized;
    char *value_corrected;
    char *type_identifier;
    char *role;
    char *source_header;
    guint occurrence;
    char *provenance_kind;
    char *verification_status;
    char *observed_at;
    char *integrated_at;
    char *extraction_identifier;
    char *warning;
    char *entity_identifier;
    char *promoted_at;
    char *promotion_kind;
} EvidenceObservation;
void evidence_observation_free(EvidenceObservation *observation);
#endif
