/* V20 — évaluations humaines distinctes de l’usage abusif d’identité. */
CREATE TABLE document_identity_misuse_assessments(
 id TEXT PRIMARY KEY,
 evidence_id TEXT NOT NULL,
 ocr_run_id TEXT,
 status TEXT NOT NULL CHECK(status IN('indeterminate','presumed','confirmed')),
 justification TEXT,
 assessed_at TEXT NOT NULL CHECK(length(assessed_at)=20),
 previous_assessment_id TEXT,
 origin TEXT NOT NULL CHECK(origin='human'),
 CHECK(status='indeterminate' OR length(trim(justification))>0),
 FOREIGN KEY(evidence_id) REFERENCES preuves(id) ON DELETE RESTRICT,
 FOREIGN KEY(ocr_run_id) REFERENCES identity_ocr_runs(id) ON DELETE RESTRICT,
 FOREIGN KEY(previous_assessment_id)
  REFERENCES document_identity_misuse_assessments(id) ON DELETE RESTRICT);
CREATE INDEX idx_identity_misuse_evidence_history
 ON document_identity_misuse_assessments(evidence_id,assessed_at,id);
CREATE UNIQUE INDEX idx_identity_misuse_previous
 ON document_identity_misuse_assessments(previous_assessment_id)
 WHERE previous_assessment_id IS NOT NULL;
CREATE TRIGGER identity_misuse_consistency BEFORE INSERT
 ON document_identity_misuse_assessments BEGIN
 SELECT CASE WHEN NEW.ocr_run_id IS NOT NULL AND NOT EXISTS(
  SELECT 1 FROM identity_ocr_runs r
  WHERE r.id=NEW.ocr_run_id AND r.evidence_id=NEW.evidence_id)
 THEN RAISE(ABORT,'OCR run does not belong to evidence') END;
 SELECT CASE WHEN NEW.previous_assessment_id IS NOT NULL AND NOT EXISTS(
  SELECT 1 FROM document_identity_misuse_assessments p
  WHERE p.id=NEW.previous_assessment_id AND p.evidence_id=NEW.evidence_id)
 THEN RAISE(ABORT,'previous assessment does not belong to evidence') END;
 SELECT CASE WHEN EXISTS(SELECT 1 FROM document_identity_misuse_assessments p
  WHERE p.evidence_id=NEW.evidence_id) AND NEW.previous_assessment_id IS NULL
 THEN RAISE(ABORT,'previous assessment is required') END;
 SELECT CASE WHEN NEW.previous_assessment_id IS NOT NULL AND EXISTS(
  SELECT 1 FROM document_identity_misuse_assessments n
  WHERE n.previous_assessment_id=NEW.previous_assessment_id)
 THEN RAISE(ABORT,'previous assessment is not current') END;
END;
CREATE TRIGGER identity_misuse_append_only_update BEFORE UPDATE
 ON document_identity_misuse_assessments BEGIN
 SELECT RAISE(ABORT,'identity misuse history is append-only'); END;
CREATE TRIGGER identity_misuse_append_only_delete BEFORE DELETE
 ON document_identity_misuse_assessments BEGIN
 SELECT RAISE(ABORT,'identity misuse history is append-only'); END;
