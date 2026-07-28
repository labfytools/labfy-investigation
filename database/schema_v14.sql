/******************************************************************************
 * Schéma SQLite V14 — affectations contextuelles multiples des personnes.
 *
 * L'unicité porte sur (personne, rôle, preuve, provenance) : un même rôle
 * peut donc être justifié par plusieurs preuves distinctes.
 ******************************************************************************/
CREATE TABLE person_role_assignments
(
    id              TEXT PRIMARY KEY CHECK (length(trim(id)) > 0),
    entity_id       TEXT NOT NULL,
    role_code       TEXT NOT NULL CHECK (length(trim(role_code)) > 0),
    evidence_id     TEXT,
    provenance_kind TEXT NOT NULL CHECK (
        provenance_kind IN ('manual', 'legacy_manual')
    ),
    confidence      INTEGER CHECK (
        confidence IS NULL OR confidence BETWEEN 0 AND 100
    ),
    notes           TEXT,
    created_at      TEXT NOT NULL CHECK (length(created_at) = 20),
    updated_at      TEXT NOT NULL CHECK (length(updated_at) = 20),
    FOREIGN KEY (entity_id) REFERENCES entites(id) ON DELETE CASCADE,
    FOREIGN KEY (evidence_id) REFERENCES preuves(id) ON DELETE SET NULL
);

CREATE UNIQUE INDEX idx_person_role_assignments_semantic
    ON person_role_assignments(
        entity_id, role_code, COALESCE(evidence_id, ''), provenance_kind
    );
CREATE INDEX idx_person_role_assignments_entity
    ON person_role_assignments(entity_id);
CREATE INDEX idx_person_role_assignments_evidence
    ON person_role_assignments(evidence_id);

/* Les codes historiques sont conservés littéralement : aucune équivalence
 * approximative n'est appliquée pendant la migration. */
INSERT INTO person_role_assignments(
    id, entity_id, role_code, evidence_id, provenance_kind,
    confidence, notes, created_at, updated_at
)
SELECT
    lower(hex(randomblob(4))) || '-' || lower(hex(randomblob(2))) || '-4' ||
    substr(lower(hex(randomblob(2))),2) || '-a' ||
    substr(lower(hex(randomblob(2))),2) || '-' || lower(hex(randomblob(6))),
    entity_id, role, NULL, 'legacy_manual', NULL, NULL, updated_at, updated_at
FROM person_roles;
