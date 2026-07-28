/******************************************************************************
 * Schéma SQLite V11 — observations sémantiques preuve-entité.
 ******************************************************************************/
CREATE TABLE IF NOT EXISTS evidence_entity_observations
(
    evidence_id         TEXT NOT NULL,
    entity_id           TEXT NOT NULL,
    entity_type         TEXT NOT NULL,
    value_raw           TEXT NOT NULL,
    value_normalized    TEXT NOT NULL,
    role                TEXT NOT NULL,
    provenance_kind     TEXT NOT NULL,
    source_header       TEXT NOT NULL,
    occurrence          INTEGER NOT NULL DEFAULT 1 CHECK (occurrence > 0),
    verification_status TEXT NOT NULL DEFAULT 'proposed',
    created_at          TEXT NOT NULL CHECK (length(created_at) = 20),
    PRIMARY KEY (evidence_id, entity_id, role, source_header, occurrence),
    FOREIGN KEY (evidence_id) REFERENCES preuves(id) ON DELETE CASCADE,
    FOREIGN KEY (entity_id) REFERENCES entites(id) ON DELETE CASCADE,
    CHECK (length(trim(role)) > 0),
    CHECK (length(trim(source_header)) > 0)
);
CREATE INDEX IF NOT EXISTS idx_evidence_entity_observations_evidence
    ON evidence_entity_observations(evidence_id);
