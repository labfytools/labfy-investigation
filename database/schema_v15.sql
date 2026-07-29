/* Migration V15 — OCR contrôlé des documents d'identité. */
CREATE TABLE IF NOT EXISTS identity_ocr_runs (
    id TEXT PRIMARY KEY,
    evidence_id TEXT NOT NULL,
    expected_sha256 TEXT NOT NULL CHECK(length(expected_sha256)=64),
    page_number INTEGER NOT NULL CHECK(page_number > 0),
    document_type TEXT NOT NULL CHECK(document_type IN
      ('identity_card','passport','driving_licence','residence_permit','other')),
    document_side TEXT NOT NULL CHECK(document_side IN
      ('front','back','identity_page','other_page')),
    engine TEXT NOT NULL,
    engine_version TEXT,
    requested_languages TEXT NOT NULL,
    available_languages TEXT NOT NULL,
    parameters TEXT NOT NULL,
    preprocessing_profile TEXT NOT NULL CHECK(preprocessing_profile IN
      ('none','orientation','grayscale','upscale')),
    executed_at TEXT NOT NULL CHECK(length(executed_at)=20),
    status TEXT NOT NULL CHECK(status IN ('success','partial','error','cancelled')),
    error_message TEXT,
    text_relative_path TEXT NOT NULL,
    text_sha256 TEXT NOT NULL CHECK(length(text_sha256)=64),
    tsv_relative_path TEXT NOT NULL,
    tsv_sha256 TEXT NOT NULL CHECK(length(tsv_sha256)=64),
    work_image_relative_path TEXT,
    work_image_sha256 TEXT,
    FOREIGN KEY(evidence_id) REFERENCES preuves(id) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS idx_identity_ocr_runs_evidence ON identity_ocr_runs(evidence_id);

CREATE TABLE IF NOT EXISTS identity_document_observations (
    id TEXT PRIMARY KEY,
    person_id TEXT NOT NULL,
    evidence_id TEXT NOT NULL,
    ocr_run_id TEXT NOT NULL UNIQUE,
    document_type TEXT NOT NULL,
    issuing_country_declared TEXT,
    document_side TEXT NOT NULL,
    page_number INTEGER NOT NULL CHECK(page_number > 0),
    review_state TEXT NOT NULL CHECK(review_state IN
      ('proposed','accepted','modified','rejected','conflict')),
    observed_at TEXT NOT NULL CHECK(length(observed_at)=20),
    factual_notes TEXT,
    FOREIGN KEY(person_id) REFERENCES entites(id) ON DELETE CASCADE,
    FOREIGN KEY(evidence_id) REFERENCES preuves(id) ON DELETE CASCADE,
    FOREIGN KEY(ocr_run_id) REFERENCES identity_ocr_runs(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS identity_field_observations (
    id TEXT PRIMARY KEY,
    observation_id TEXT NOT NULL,
    field_code TEXT NOT NULL,
    raw_value TEXT NOT NULL,
    corrected_value TEXT,
    normalized_value TEXT,
    confidence REAL CHECK(confidence IS NULL OR confidence BETWEEN 0 AND 100),
    review_status TEXT NOT NULL CHECK(review_status IN
      ('accepted','modified')),
    origin TEXT NOT NULL CHECK(origin IN ('ocr','mrz','manual_override')),
    evidence_id TEXT NOT NULL,
    ocr_run_id TEXT NOT NULL,
    page_number INTEGER NOT NULL CHECK(page_number > 0),
    source_x INTEGER,
    source_y INTEGER,
    source_width INTEGER,
    source_height INTEGER,
    source_image_width INTEGER,
    source_image_height INTEGER,
    display_order INTEGER NOT NULL CHECK(display_order >= 0),
    reviewed_at TEXT NOT NULL CHECK(length(reviewed_at)=20),
    review_note TEXT,
    FOREIGN KEY(observation_id) REFERENCES identity_document_observations(id)
      ON DELETE CASCADE,
    FOREIGN KEY(evidence_id) REFERENCES preuves(id) ON DELETE CASCADE,
    FOREIGN KEY(ocr_run_id) REFERENCES identity_ocr_runs(id) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS idx_identity_fields_observation
  ON identity_field_observations(observation_id,display_order);
