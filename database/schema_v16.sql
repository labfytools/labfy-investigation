/* Migration V16 — saisie manuelle contrôlée des champs d'identité omis. */
CREATE TABLE identity_field_observations_v16 (
    id TEXT PRIMARY KEY,
    observation_id TEXT NOT NULL,
    field_code TEXT NOT NULL,
    raw_value TEXT,
    corrected_value TEXT,
    normalized_value TEXT,
    confidence REAL CHECK(confidence IS NULL OR confidence BETWEEN 0 AND 100),
    review_status TEXT NOT NULL CHECK(review_status IN ('accepted','modified')),
    origin TEXT NOT NULL CHECK(origin IN
      ('ocr','mrz','manual_override','manual_entry')),
    evidence_id TEXT NOT NULL,
    ocr_run_id TEXT NOT NULL,
    page_number INTEGER NOT NULL CHECK(page_number > 0),
    source_x INTEGER, source_y INTEGER, source_width INTEGER,
    source_height INTEGER, source_image_width INTEGER,
    source_image_height INTEGER,
    display_order INTEGER NOT NULL CHECK(display_order >= 0),
    reviewed_at TEXT NOT NULL CHECK(length(reviewed_at)=20),
    review_note TEXT,
    CHECK (
      (origin = 'manual_entry' AND raw_value IS NULL
       AND corrected_value IS NOT NULL AND confidence IS NULL)
      OR
      (origin <> 'manual_entry' AND raw_value IS NOT NULL)
    ),
    FOREIGN KEY(observation_id) REFERENCES identity_document_observations(id)
      ON DELETE CASCADE,
    FOREIGN KEY(evidence_id) REFERENCES preuves(id) ON DELETE CASCADE,
    FOREIGN KEY(ocr_run_id) REFERENCES identity_ocr_runs(id) ON DELETE CASCADE
);
INSERT INTO identity_field_observations_v16
SELECT * FROM identity_field_observations;
DROP TABLE identity_field_observations;
ALTER TABLE identity_field_observations_v16
  RENAME TO identity_field_observations;
CREATE INDEX idx_identity_fields_observation
  ON identity_field_observations(observation_id,display_order);
