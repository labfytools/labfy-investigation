/* V19 — attributs structurés et provenance des projections OCR humaines. */
CREATE TABLE person_profile_fields(
 person_id TEXT NOT NULL,field_code TEXT NOT NULL,value TEXT NOT NULL,
 updated_at TEXT NOT NULL CHECK(length(updated_at)=20),
 PRIMARY KEY(person_id,field_code),
 CHECK(field_code IN('declared_name','surname','given_names','birth_date',
 'birth_place','nationality','sex_as_printed','address_as_printed')),
 CHECK(length(trim(value))>0),
 FOREIGN KEY(person_id) REFERENCES entites(id) ON DELETE CASCADE);
CREATE TABLE person_ocr_field_projections(
 id TEXT PRIMARY KEY,person_id TEXT NOT NULL,person_field_code TEXT NOT NULL,
 previous_value TEXT,new_value TEXT NOT NULL,evidence_id TEXT NOT NULL,
 ocr_run_id TEXT NOT NULL,ocr_field_id TEXT NOT NULL,ocr_field_code TEXT NOT NULL,
 value_quality TEXT NOT NULL CHECK(value_quality IN('complete','partial')),
 review_status TEXT NOT NULL CHECK(review_status IN('accepted','modified')),
 strategy TEXT NOT NULL CHECK(strategy IN('fill_empty','replace_existing')),
 projected_at TEXT NOT NULL CHECK(length(projected_at)=20),
 origin TEXT NOT NULL CHECK(origin='human'),
 FOREIGN KEY(person_id) REFERENCES entites(id) ON DELETE RESTRICT,
 FOREIGN KEY(evidence_id) REFERENCES preuves(id) ON DELETE RESTRICT,
 FOREIGN KEY(ocr_run_id) REFERENCES identity_ocr_runs(id) ON DELETE RESTRICT,
 FOREIGN KEY(ocr_field_id) REFERENCES identity_field_observations(id) ON DELETE RESTRICT);
CREATE INDEX idx_person_projection_person ON person_ocr_field_projections(person_id,projected_at,id);
CREATE TRIGGER projection_v19_consistency BEFORE INSERT ON person_ocr_field_projections BEGIN
 SELECT CASE WHEN NOT EXISTS(SELECT 1 FROM identity_field_observations f
  WHERE f.id=NEW.ocr_field_id AND f.ocr_run_id=NEW.ocr_run_id
  AND f.evidence_id=NEW.evidence_id AND f.field_code=NEW.ocr_field_code
  AND f.confirmation_state='human_confirmed' AND length(trim(f.confirmed_value))>0
  AND f.review_status IN('accepted','modified')
  AND f.value_quality IN('complete','partial') AND f.confirmed_value=NEW.new_value)
 THEN RAISE(ABORT,'OCR field is no longer projectable') END;
END;
