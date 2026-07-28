/******************************************************************************
 * Schéma SQLite V12 — observations indépendantes et promotion facultative.
 ******************************************************************************/
ALTER TABLE evidence_entity_observations RENAME TO evidence_entity_observations_v11;

CREATE TABLE evidence_entity_observations
(
    id                  TEXT PRIMARY KEY,
    evidence_id         TEXT NOT NULL,
    entity_id           TEXT,
    entity_type         TEXT NOT NULL,
    value_raw           TEXT NOT NULL,
    value_normalized    TEXT,
    value_corrected     TEXT,
    role                TEXT NOT NULL,
    provenance_kind     TEXT NOT NULL,
    source_header       TEXT NOT NULL,
    occurrence          INTEGER NOT NULL DEFAULT 1 CHECK (occurrence > 0),
    extraction_id       TEXT,
    verification_status TEXT NOT NULL DEFAULT 'proposed',
    warning             TEXT,
    observed_at         TEXT NOT NULL CHECK (length(observed_at) = 20),
    integrated_at       TEXT NOT NULL CHECK (length(integrated_at) = 20),
    promoted_at         TEXT CHECK (promoted_at IS NULL OR length(promoted_at) = 20),
    promotion_kind      TEXT CHECK (promotion_kind IS NULL OR promotion_kind IN ('created', 'reused', 'legacy')),
    FOREIGN KEY (evidence_id) REFERENCES preuves(id) ON DELETE CASCADE,
    FOREIGN KEY (entity_id) REFERENCES entites(id) ON DELETE SET NULL,
    FOREIGN KEY (extraction_id) REFERENCES extractions(id) ON DELETE SET NULL,
    UNIQUE (evidence_id, entity_type, value_normalized, role, source_header,
            occurrence, provenance_kind, extraction_id)
);

INSERT INTO evidence_entity_observations(
    id,evidence_id,entity_id,entity_type,value_raw,value_normalized,role,
    provenance_kind,source_header,occurrence,verification_status,
    observed_at,integrated_at,promoted_at,promotion_kind)
SELECT
    lower(hex(randomblob(4))) || '-' || lower(hex(randomblob(2))) || '-4' ||
    substr(lower(hex(randomblob(2))),2) || '-a' ||
    substr(lower(hex(randomblob(2))),2) || '-' || lower(hex(randomblob(6))),
    evidence_id,entity_id,entity_type,value_raw,value_normalized,role,
    provenance_kind,source_header,occurrence,verification_status,
    created_at,created_at,created_at,'legacy'
FROM evidence_entity_observations_v11;

DROP TABLE evidence_entity_observations_v11;

CREATE INDEX idx_evidence_entity_observations_evidence
    ON evidence_entity_observations(evidence_id);
CREATE INDEX idx_evidence_entity_observations_entity
    ON evidence_entity_observations(entity_id);
CREATE UNIQUE INDEX idx_evidence_entity_observations_semantic
    ON evidence_entity_observations(
        evidence_id,entity_type,value_normalized,role,source_header,
        occurrence,provenance_kind,COALESCE(extraction_id,''));
