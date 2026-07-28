/******************************************************************************
 * Schéma SQLite V13 — propriété des associations preuve-entité.
 ******************************************************************************/
CREATE TABLE preuve_entite_sources
(
    id          TEXT PRIMARY KEY,
    preuve_id   TEXT NOT NULL,
    entite_id   TEXT NOT NULL,
    source_kind TEXT NOT NULL CHECK (
        source_kind IN ('manual', 'legacy_manual', 'eml_observation')
    ),
    source_uuid TEXT,
    created_at  TEXT NOT NULL CHECK (length(created_at) = 20),
    FOREIGN KEY (preuve_id, entite_id)
        REFERENCES preuve_entites(preuve_id, entite_id) ON DELETE CASCADE,
    CHECK (
        (source_kind = 'eml_observation' AND source_uuid IS NOT NULL) OR
        (source_kind <> 'eml_observation' AND source_uuid IS NULL)
    )
);

CREATE UNIQUE INDEX idx_preuve_entite_sources_unique
    ON preuve_entite_sources(
        preuve_id, entite_id, source_kind, COALESCE(source_uuid, '')
    );
CREATE INDEX idx_preuve_entite_sources_entity
    ON preuve_entite_sources(entite_id);
CREATE INDEX idx_preuve_entite_sources_source
    ON preuve_entite_sources(source_kind, source_uuid);

/* Les rattachements antérieurs sont conservés de façon prudente. */
INSERT INTO preuve_entite_sources(
    id, preuve_id, entite_id, source_kind, source_uuid, created_at
)
SELECT
    lower(hex(randomblob(4))) || '-' || lower(hex(randomblob(2))) || '-4' ||
    substr(lower(hex(randomblob(2))),2) || '-a' ||
    substr(lower(hex(randomblob(2))),2) || '-' || lower(hex(randomblob(6))),
    preuve_id, entite_id, 'legacy_manual', NULL,
    strftime('%Y-%m-%dT%H:%M:%SZ', 'now')
FROM preuve_entites;

/* Une promotion V12 identifiable reçoit aussi sa justification précise. */
INSERT OR IGNORE INTO preuve_entite_sources(
    id, preuve_id, entite_id, source_kind, source_uuid, created_at
)
SELECT
    lower(hex(randomblob(4))) || '-' || lower(hex(randomblob(2))) || '-4' ||
    substr(lower(hex(randomblob(2))),2) || '-a' ||
    substr(lower(hex(randomblob(2))),2) || '-' || lower(hex(randomblob(6))),
    evidence_id, entity_id, 'eml_observation', id,
    COALESCE(promoted_at, integrated_at)
FROM evidence_entity_observations
WHERE entity_id IS NOT NULL;
