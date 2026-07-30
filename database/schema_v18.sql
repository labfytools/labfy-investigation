/* Migration V18 — traçabilité humaine des documents d'identité. */

CREATE TABLE identification_status_vocabulary (
    code TEXT PRIMARY KEY,
    label TEXT NOT NULL,
    description TEXT NOT NULL,
    display_order INTEGER NOT NULL UNIQUE,
    active INTEGER NOT NULL DEFAULT 1 CHECK(active IN (0,1)),
    requires_justification INTEGER NOT NULL DEFAULT 0
      CHECK(requires_justification IN (0,1)),
    sensitive INTEGER NOT NULL DEFAULT 0 CHECK(sensitive IN (0,1))
);
INSERT INTO identification_status_vocabulary VALUES
('unknown','Inconnu','Aucune identification disponible.',10,1,0,0),
('unverified','Non vérifié','Identification déclarée, non vérifiée.',20,1,0,0),
('presumed','Présumé','Identification présumée à justifier.',30,1,1,1),
('partially_identified','Partiellement identifié','Identification incomplète.',40,1,0,0),
('confirmed','Confirmé','Identification confirmée à justifier.',50,1,1,1),
('disputed','Contesté','Identification contestée à justifier.',60,1,1,1);

CREATE TABLE person_role_vocabulary (
    code TEXT PRIMARY KEY,
    label TEXT NOT NULL,
    description TEXT NOT NULL,
    display_order INTEGER NOT NULL UNIQUE,
    active INTEGER NOT NULL DEFAULT 1 CHECK(active IN (0,1)),
    requires_justification INTEGER NOT NULL DEFAULT 0
      CHECK(requires_justification IN (0,1)),
    sensitive INTEGER NOT NULL DEFAULT 0 CHECK(sensitive IN (0,1))
);
INSERT INTO person_role_vocabulary VALUES
('alleged_author','Auteur présumé','Rôle allégué, sans attribution automatique.',10,1,1,1),
('presented_identity','Identité présentée','Identité déclarée ou présentée dans une preuve.',20,1,0,0),
('potentially_impersonated_identity','Identité potentiellement usurpée','Hypothèse sensible nécessitant une justification.',30,1,1,1),
('victim','Victime','Personne déclarée victime.',40,1,0,0),
('witness','Témoin','Personne déclarée témoin.',50,1,0,0),
('declared_bank_holder','Titulaire bancaire déclaré','Titulaire déclaré par une source.',60,1,0,0),
('intermediary','Intermédiaire','Personne observée comme intermédiaire.',70,1,0,0),
('mentioned_person','Personne citée','Personne seulement citée.',80,1,0,0),
('other','Autre','Rôle factuel non couvert.',90,1,1,0),
('uncategorized','Non catégorisée','Code historique.',100,1,0,0),
('alleged_scammer','Escroc présumé (historique)','Code historique sensible.',110,0,1,1),
('suspect','Suspect (historique)','Code historique sensible.',120,0,1,1),
('related_person','Personne liée (historique)','Code historique.',130,0,0,0),
('impersonated_identity','Identité usurpée (historique)','Code historique sensible.',140,0,1,1);

CREATE TABLE document_authenticity_assessments (
    id TEXT PRIMARY KEY,
    evidence_id TEXT NOT NULL,
    ocr_run_id TEXT,
    status TEXT NOT NULL CHECK(status IN (
      'indeterminate','presumed_authentic','suspicious',
      'presumed_forged','confirmed_forged')),
    justification TEXT,
    assessed_at TEXT NOT NULL CHECK(length(assessed_at)=20),
    previous_assessment_id TEXT,
    technical_note TEXT,
    origin TEXT NOT NULL CHECK(origin='human'),
    CHECK(status='indeterminate' OR
      (justification IS NOT NULL AND length(trim(justification))>0)),
    FOREIGN KEY(evidence_id) REFERENCES preuves(id) ON DELETE RESTRICT,
    FOREIGN KEY(ocr_run_id) REFERENCES identity_ocr_runs(id) ON DELETE SET NULL,
    FOREIGN KEY(previous_assessment_id)
      REFERENCES document_authenticity_assessments(id) ON DELETE RESTRICT
);
CREATE INDEX idx_authenticity_evidence_history
  ON document_authenticity_assessments(evidence_id,assessed_at,id);
CREATE UNIQUE INDEX idx_authenticity_previous
  ON document_authenticity_assessments(previous_assessment_id)
  WHERE previous_assessment_id IS NOT NULL;

CREATE TABLE person_evidence_factual_relations (
    id TEXT PRIMARY KEY,
    person_id TEXT NOT NULL,
    evidence_id TEXT NOT NULL,
    ocr_run_id TEXT,
    relation_type TEXT NOT NULL CHECK(relation_type IN (
      'identity_observed_in','document_presented_in_name_of',
      'declared_holder_in','data_extracted_from')),
    factual_note TEXT,
    observed_at TEXT NOT NULL CHECK(length(observed_at)=20),
    origin TEXT NOT NULL CHECK(origin='human'),
    active INTEGER NOT NULL DEFAULT 1 CHECK(active IN (0,1)),
    FOREIGN KEY(person_id) REFERENCES entites(id) ON DELETE RESTRICT,
    FOREIGN KEY(evidence_id) REFERENCES preuves(id) ON DELETE RESTRICT,
    FOREIGN KEY(ocr_run_id) REFERENCES identity_ocr_runs(id) ON DELETE SET NULL
);
CREATE INDEX idx_person_evidence_factual_person
  ON person_evidence_factual_relations(person_id,observed_at,id);
CREATE INDEX idx_person_evidence_factual_evidence
  ON person_evidence_factual_relations(evidence_id,observed_at,id);
CREATE TRIGGER authenticity_v18_consistency
BEFORE INSERT ON document_authenticity_assessments BEGIN
 SELECT CASE
 WHEN NEW.ocr_run_id IS NOT NULL AND NOT EXISTS(
  SELECT 1 FROM identity_ocr_runs
  WHERE id=NEW.ocr_run_id AND evidence_id=NEW.evidence_id)
 THEN RAISE(ABORT,'OCR run belongs to another evidence')
 WHEN NEW.previous_assessment_id IS NOT NULL AND NOT EXISTS(
  SELECT 1 FROM document_authenticity_assessments
  WHERE id=NEW.previous_assessment_id AND evidence_id=NEW.evidence_id)
 THEN RAISE(ABORT,'previous assessment belongs to another evidence') END;
END;
CREATE TRIGGER factual_relation_v18_consistency
BEFORE INSERT ON person_evidence_factual_relations BEGIN
 SELECT CASE
 WHEN NOT EXISTS(SELECT 1 FROM entites e JOIN types_entite t ON t.id=e.type_id
  WHERE e.id=NEW.person_id AND t.code='person')
 THEN RAISE(ABORT,'factual relation requires a person')
 WHEN NEW.ocr_run_id IS NOT NULL AND NOT EXISTS(
  SELECT 1 FROM identity_ocr_runs
  WHERE id=NEW.ocr_run_id AND evidence_id=NEW.evidence_id)
 THEN RAISE(ABORT,'OCR run belongs to another evidence') END;
END;

CREATE TABLE person_identification_assessments (
    id TEXT PRIMARY KEY,
    person_id TEXT NOT NULL,
    status_code TEXT NOT NULL,
    justification TEXT,
    assessed_at TEXT NOT NULL CHECK(length(assessed_at)=20),
    origin TEXT NOT NULL CHECK(origin='human'),
    previous_assessment_id TEXT,
    CHECK(status_code<>'disputed' OR
      (justification IS NOT NULL AND length(trim(justification))>0)),
    FOREIGN KEY(person_id) REFERENCES entites(id) ON DELETE CASCADE,
    FOREIGN KEY(status_code) REFERENCES identification_status_vocabulary(code)
      ON DELETE RESTRICT,
    FOREIGN KEY(previous_assessment_id)
      REFERENCES person_identification_assessments(id) ON DELETE RESTRICT
);

CREATE TABLE identity_field_observations_v18 (
    id TEXT PRIMARY KEY,
    observation_id TEXT NOT NULL,
    field_code TEXT NOT NULL,
    raw_value TEXT,
    corrected_value TEXT,
    normalized_value TEXT,
    confidence REAL CHECK(confidence IS NULL OR confidence BETWEEN 0 AND 100),
    review_status TEXT NOT NULL CHECK(review_status IN
      ('proposed','accepted','modified','rejected','conflict')),
    origin TEXT NOT NULL CHECK(origin IN
      ('ocr','mrz','manual_override','manual_entry')),
    evidence_id TEXT NOT NULL,
    ocr_run_id TEXT NOT NULL,
    page_number INTEGER NOT NULL CHECK(page_number>0),
    source_x INTEGER,source_y INTEGER,source_width INTEGER,source_height INTEGER,
    source_image_width INTEGER,source_image_height INTEGER,
    display_order INTEGER NOT NULL CHECK(display_order>=0),
    reviewed_at TEXT NOT NULL CHECK(length(reviewed_at)=20),
    review_note TEXT,
    confirmed_value TEXT,
    confirmation_state TEXT NOT NULL DEFAULT 'unconfirmed'
      CHECK(confirmation_state IN ('unconfirmed','human_confirmed')),
    value_quality TEXT NOT NULL DEFAULT 'complete'
      CHECK(value_quality IN ('complete','partial','uncertain','invalid')),
    CHECK((origin='manual_entry' AND raw_value IS NULL
           AND corrected_value IS NOT NULL AND confidence IS NULL)
       OR (origin<>'manual_entry' AND raw_value IS NOT NULL)),
    FOREIGN KEY(observation_id) REFERENCES identity_document_observations(id)
      ON DELETE CASCADE,
    FOREIGN KEY(evidence_id) REFERENCES preuves(id) ON DELETE CASCADE,
    FOREIGN KEY(ocr_run_id) REFERENCES identity_ocr_runs(id) ON DELETE CASCADE
);
INSERT INTO identity_field_observations_v18(
 id,observation_id,field_code,raw_value,corrected_value,normalized_value,
 confidence,review_status,origin,evidence_id,ocr_run_id,page_number,
 source_x,source_y,source_width,source_height,source_image_width,
 source_image_height,display_order,reviewed_at,review_note)
SELECT id,observation_id,field_code,raw_value,corrected_value,normalized_value,
 confidence,review_status,origin,evidence_id,ocr_run_id,page_number,
 source_x,source_y,source_width,source_height,source_image_width,
 source_image_height,display_order,reviewed_at,review_note
FROM identity_field_observations;
DROP TABLE identity_field_observations;
ALTER TABLE identity_field_observations_v18
  RENAME TO identity_field_observations;
CREATE INDEX idx_identity_fields_observation
  ON identity_field_observations(observation_id,display_order);

CREATE TRIGGER identity_fields_v18_insert_guard
BEFORE INSERT ON identity_field_observations
BEGIN
  SELECT CASE
    WHEN NEW.review_status IN ('rejected','conflict')
      AND NEW.confirmed_value IS NOT NULL
      THEN RAISE(ABORT,'rejected or conflict field cannot be confirmed')
    WHEN NEW.value_quality IN ('uncertain','invalid')
      AND NEW.confirmed_value IS NOT NULL
      THEN RAISE(ABORT,'uncertain or invalid field cannot be confirmed')
    WHEN NEW.confirmed_value IS NOT NULL
      AND NEW.confirmation_state<>'human_confirmed'
      THEN RAISE(ABORT,'confirmed value requires human confirmation')
    WHEN NEW.confirmation_state='human_confirmed'
      AND NEW.confirmed_value IS NULL
      THEN RAISE(ABORT,'human confirmation requires a value')
  END;
END;
CREATE TRIGGER identity_fields_v18_update_guard
BEFORE UPDATE ON identity_field_observations
BEGIN
  SELECT CASE
    WHEN NEW.review_status IN ('rejected','conflict')
      AND NEW.confirmed_value IS NOT NULL
      THEN RAISE(ABORT,'rejected or conflict field cannot be confirmed')
    WHEN NEW.value_quality IN ('uncertain','invalid')
      AND NEW.confirmed_value IS NOT NULL
      THEN RAISE(ABORT,'uncertain or invalid field cannot be confirmed')
    WHEN NEW.confirmed_value IS NOT NULL
      AND NEW.confirmation_state<>'human_confirmed'
      THEN RAISE(ABORT,'confirmed value requires human confirmation')
    WHEN NEW.confirmation_state='human_confirmed'
      AND NEW.confirmed_value IS NULL
      THEN RAISE(ABORT,'human confirmation requires a value')
  END;
END;
