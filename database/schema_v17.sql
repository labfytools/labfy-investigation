/* Migration V17 — transcription humaine distincte du texte OCR brut. */
ALTER TABLE identity_ocr_runs
  ADD COLUMN corrected_transcription TEXT;
ALTER TABLE identity_ocr_runs
  ADD COLUMN transcription_is_human INTEGER NOT NULL DEFAULT 0
    CHECK(transcription_is_human IN (0,1));
ALTER TABLE identity_ocr_runs
  ADD COLUMN transcription_corrected_at TEXT;
ALTER TABLE identity_ocr_runs
  ADD COLUMN transcription_origin TEXT
    CHECK(transcription_origin IS NULL OR transcription_origin='human');
