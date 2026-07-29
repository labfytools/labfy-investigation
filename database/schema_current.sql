/******************************************************************************
 * Labfy Investigation
 *
 * Extensions idempotentes du schéma SQLite courant V9
 ******************************************************************************/

/*
 * Les coordonnées du graphe sont un état de présentation.
 *
 * Elles restent séparées des données métier de la table entites.
 *
 * Cette table peut être créée à chaque ouverture grâce à IF NOT EXISTS.
 */
CREATE TABLE IF NOT EXISTS graph_node_positions
(
    entity_id  TEXT PRIMARY KEY,

    x          REAL NOT NULL,
    y          REAL NOT NULL,

    updated_at TEXT NOT NULL,

    FOREIGN KEY (entity_id)
        REFERENCES entites(id)
        ON UPDATE CASCADE
        ON DELETE CASCADE,

    CHECK (
        length(trim(entity_id)) > 0
    ),

    CHECK (
        typeof(x) IN (
            'integer',
            'real'
        )
    ),

    CHECK (
        typeof(y) IN (
            'integer',
            'real'
        )
    ),

    CHECK (
        x = x
        AND abs(x) <= 1.7976931348623157e308
    ),

    CHECK (
        y = y
        AND abs(y) <= 1.7976931348623157e308
    ),

    CHECK (
        length(trim(updated_at)) > 0
    )
);

/*
 * Disposition générique du graphe.
 *
 * Contrairement à graph_node_positions, cette table accepte aussi les UUID
 * des relations. L'ancienne table reste présente pour assurer la compatibilité
 * avec les enquêtes créées avant l'introduction des nœuds de relation.
 */
CREATE TABLE IF NOT EXISTS extractions
(
    id TEXT PRIMARY KEY,
    evidence_id TEXT,
    source_kind TEXT NOT NULL,
    source_id TEXT NOT NULL,
    tool_id TEXT NOT NULL,
    created_at TEXT NOT NULL,
    FOREIGN KEY (evidence_id) REFERENCES preuves(id) ON DELETE CASCADE,
    CHECK (source_kind IN ('evidence', 'entity')),
    CHECK (length(trim(source_id)) > 0),
    CHECK (length(trim(tool_id)) > 0),
    CHECK (length(created_at) = 20)
);
CREATE INDEX IF NOT EXISTS idx_extractions_source
    ON extractions(source_kind, source_id);

CREATE TABLE IF NOT EXISTS graph_layout_positions
(
    node_id    TEXT PRIMARY KEY,
    x          REAL NOT NULL,
    y          REAL NOT NULL,
    updated_at TEXT NOT NULL,

    CHECK (length(trim(node_id)) > 0),
    CHECK (length(updated_at) = 20)
);

CREATE TABLE IF NOT EXISTS graph_viewport
(
    id         INTEGER PRIMARY KEY CHECK (id = 1),
    zoom       REAL NOT NULL CHECK (zoom = zoom AND zoom > 0),
    offset_x   REAL NOT NULL CHECK (offset_x = offset_x),
    offset_y   REAL NOT NULL CHECK (offset_y = offset_y),
    updated_at TEXT NOT NULL CHECK (length(updated_at) = 20)
);

CREATE TABLE IF NOT EXISTS relation_types
(
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    code TEXT UNIQUE,
    label TEXT NOT NULL,
    normalized_key TEXT NOT NULL UNIQUE,
    description TEXT,
    is_system INTEGER NOT NULL DEFAULT 0 CHECK (is_system IN (0, 1)),
    CHECK (code IS NULL OR length(trim(code)) > 0),
    CHECK (length(trim(label)) > 0),
    CHECK (length(normalized_key) > 0)
);

/* Migration idempotente des positions d'entités déjà enregistrées. */
INSERT OR IGNORE INTO graph_layout_positions(node_id, x, y, updated_at)
SELECT entity_id, x, y, updated_at
FROM graph_node_positions;

/* Évite qu'une réinitialisation future ne réimporte des coordonnées obsolètes. */
DELETE FROM graph_node_positions;

/* Une clé étrangère polymorphe n'existe pas dans SQLite : ces triggers
 * suppriment donc les positions devenues orphelines. */
CREATE TRIGGER IF NOT EXISTS graph_layout_positions_delete_entity
AFTER DELETE ON entites
FOR EACH ROW
BEGIN
    DELETE FROM graph_layout_positions WHERE node_id = OLD.id;
END;

CREATE TRIGGER IF NOT EXISTS graph_layout_positions_delete_relation
AFTER DELETE ON relations
FOR EACH ROW
BEGIN
    DELETE FROM graph_layout_positions WHERE node_id = OLD.id;
END;

CREATE TABLE IF NOT EXISTS bank_account_entities
(
    id                  TEXT PRIMARY KEY,
    iban                TEXT NOT NULL,
    bic                 TEXT,
    holder_name         TEXT,
    bank_name           TEXT,
    bank_address        TEXT,
    country_code        TEXT,
    bank_code           TEXT,
    branch_code         TEXT,
    account_number      TEXT,
    rib_key             TEXT,
    verification_status TEXT NOT NULL DEFAULT 'proposed' CHECK (verification_status IN ('proposed', 'confirmed', 'rejected', 'conflicted', 'invalid')),
    provenance_kind     TEXT NOT NULL DEFAULT 'ocr' CHECK (provenance_kind IN ('observed', 'ocr', 'header', 'metadata', 'derived', 'manual')),
    evidence_id         TEXT,
    extraction_id       TEXT,
    created_at          TEXT NOT NULL CHECK (length(created_at) = 20),
    updated_at          TEXT NOT NULL CHECK (length(updated_at) = 20),
    FOREIGN KEY (evidence_id) REFERENCES preuves(id) ON DELETE SET NULL,
    FOREIGN KEY (extraction_id) REFERENCES extractions(id) ON DELETE SET NULL,
    CHECK (length(trim(id)) > 0),
    CHECK (length(trim(iban)) > 0)
);

CREATE INDEX IF NOT EXISTS idx_bank_account_entities_iban ON bank_account_entities(iban);
CREATE INDEX IF NOT EXISTS idx_bank_account_entities_evidence ON bank_account_entities(evidence_id);

INSERT OR IGNORE INTO relation_types(code, label, normalized_key, description, is_system) VALUES
('sent_from',           'Envoyé depuis',             'envoyé depuis',             'Message e-mail envoyé depuis une adresse ou serveur.', 1),
('sent_to',             'Envoyé à',                  'envoyé à',                  'Message e-mail envoyé à une adresse.',                 1),
('reply_to',            'Répondre à',                'répondre à',                'Adresse de réponse configurée.',                        1),
('has_attachment',      'Possède la pièce jointe',  'possède la pièce jointe',  'Preuve ou fichier joint à un e-mail.',                 1),
('relayed_by',          'Relayé par',                'relayé par',                'Relais SMTP ayant acheminé le message.',                1),
('uses_domain',         'Utilise le domaine',        'utilise le domaine',        'Adresse e-mail rattachée à un domaine.',               1),
('held_at',             'Tenu auprès de',            'tenu auprès de',            'Compte bancaire ouvert dans une banque.',               1),
('named_as_holder_of',  'Nommé titulaire de',        'nommé titulaire de',        'Personne ou entité observée comme titulaire du RIB.',   1),
('supports',            'Soutient',                  'soutient',                  'Preuve soutenant une entité ou relation.',              1);

CREATE TABLE IF NOT EXISTS evidence_entity_observations
(
    id TEXT PRIMARY KEY,
    evidence_id TEXT NOT NULL,
    entity_id TEXT,
    entity_type TEXT NOT NULL,
    value_raw TEXT NOT NULL,
    value_normalized TEXT,
    value_corrected TEXT,
    role TEXT NOT NULL,
    provenance_kind TEXT NOT NULL,
    source_header TEXT NOT NULL,
    occurrence INTEGER NOT NULL DEFAULT 1 CHECK (occurrence > 0),
    verification_status TEXT NOT NULL DEFAULT 'proposed',
    extraction_id TEXT,
    warning TEXT,
    observed_at TEXT NOT NULL CHECK (length(observed_at) = 20),
    integrated_at TEXT NOT NULL CHECK (length(integrated_at) = 20),
    promoted_at TEXT,
    promotion_kind TEXT,
    UNIQUE (evidence_id, entity_type, value_normalized, role, source_header,
            occurrence, provenance_kind, extraction_id),
    FOREIGN KEY (evidence_id) REFERENCES preuves(id) ON DELETE CASCADE,
    FOREIGN KEY (entity_id) REFERENCES entites(id) ON DELETE SET NULL,
    FOREIGN KEY (extraction_id) REFERENCES extractions(id) ON DELETE SET NULL
);
CREATE INDEX IF NOT EXISTS idx_evidence_entity_observations_evidence
    ON evidence_entity_observations(evidence_id);
CREATE INDEX IF NOT EXISTS idx_evidence_entity_observations_entity
    ON evidence_entity_observations(entity_id);
CREATE UNIQUE INDEX IF NOT EXISTS idx_evidence_entity_observations_semantic
    ON evidence_entity_observations(
        evidence_id,entity_type,value_normalized,role,source_header,
        occurrence,provenance_kind,COALESCE(extraction_id,''));

CREATE TABLE IF NOT EXISTS preuve_entite_sources
(
    id TEXT PRIMARY KEY,
    preuve_id TEXT NOT NULL,
    entite_id TEXT NOT NULL,
    source_kind TEXT NOT NULL CHECK (
        source_kind IN ('manual', 'legacy_manual', 'eml_observation')
    ),
    source_uuid TEXT,
    created_at TEXT NOT NULL CHECK (length(created_at) = 20),
    FOREIGN KEY (preuve_id, entite_id)
        REFERENCES preuve_entites(preuve_id, entite_id) ON DELETE CASCADE,
    CHECK (
        (source_kind = 'eml_observation' AND source_uuid IS NOT NULL) OR
        (source_kind <> 'eml_observation' AND source_uuid IS NULL)
    )
);
CREATE UNIQUE INDEX IF NOT EXISTS idx_preuve_entite_sources_unique
    ON preuve_entite_sources(
        preuve_id, entite_id, source_kind, COALESCE(source_uuid, '')
    );
CREATE INDEX IF NOT EXISTS idx_preuve_entite_sources_entity
    ON preuve_entite_sources(entite_id);
CREATE INDEX IF NOT EXISTS idx_preuve_entite_sources_source
    ON preuve_entite_sources(source_kind, source_uuid);

CREATE TABLE IF NOT EXISTS person_role_assignments
(
    id TEXT PRIMARY KEY CHECK (length(trim(id)) > 0),
    entity_id TEXT NOT NULL,
    role_code TEXT NOT NULL CHECK (length(trim(role_code)) > 0),
    evidence_id TEXT,
    provenance_kind TEXT NOT NULL CHECK (
        provenance_kind IN ('manual', 'legacy_manual')
    ),
    confidence INTEGER CHECK (
        confidence IS NULL OR confidence BETWEEN 0 AND 100
    ),
    notes TEXT,
    created_at TEXT NOT NULL CHECK (length(created_at) = 20),
    updated_at TEXT NOT NULL CHECK (length(updated_at) = 20),
    FOREIGN KEY (entity_id) REFERENCES entites(id) ON DELETE CASCADE,
    FOREIGN KEY (evidence_id) REFERENCES preuves(id) ON DELETE SET NULL
);
CREATE UNIQUE INDEX IF NOT EXISTS idx_person_role_assignments_semantic
    ON person_role_assignments(
        entity_id, role_code, COALESCE(evidence_id, ''), provenance_kind
    );
CREATE INDEX IF NOT EXISTS idx_person_role_assignments_entity
    ON person_role_assignments(entity_id);
CREATE INDEX IF NOT EXISTS idx_person_role_assignments_evidence
    ON person_role_assignments(evidence_id);

CREATE TABLE IF NOT EXISTS identity_ocr_runs (
    id TEXT PRIMARY KEY, evidence_id TEXT NOT NULL,
    expected_sha256 TEXT NOT NULL CHECK(length(expected_sha256)=64),
    page_number INTEGER NOT NULL CHECK(page_number>0),
    document_type TEXT NOT NULL, document_side TEXT NOT NULL,
    engine TEXT NOT NULL, engine_version TEXT, requested_languages TEXT NOT NULL,
    available_languages TEXT NOT NULL, parameters TEXT NOT NULL,
    preprocessing_profile TEXT NOT NULL, executed_at TEXT NOT NULL,
    status TEXT NOT NULL, error_message TEXT,
    text_relative_path TEXT NOT NULL, text_sha256 TEXT NOT NULL,
    tsv_relative_path TEXT NOT NULL, tsv_sha256 TEXT NOT NULL,
    work_image_relative_path TEXT, work_image_sha256 TEXT,
    corrected_transcription TEXT,
    transcription_is_human INTEGER NOT NULL DEFAULT 0
      CHECK(transcription_is_human IN (0,1)),
    transcription_corrected_at TEXT,
    transcription_origin TEXT
      CHECK(transcription_origin IS NULL OR transcription_origin='human'),
    FOREIGN KEY(evidence_id) REFERENCES preuves(id) ON DELETE CASCADE);
CREATE INDEX IF NOT EXISTS idx_identity_ocr_runs_evidence
    ON identity_ocr_runs(evidence_id);
CREATE TABLE IF NOT EXISTS identity_document_observations (
    id TEXT PRIMARY KEY, person_id TEXT NOT NULL, evidence_id TEXT NOT NULL,
    ocr_run_id TEXT NOT NULL UNIQUE, document_type TEXT NOT NULL,
    issuing_country_declared TEXT, document_side TEXT NOT NULL,
    page_number INTEGER NOT NULL, review_state TEXT NOT NULL,
    observed_at TEXT NOT NULL, factual_notes TEXT,
    FOREIGN KEY(person_id) REFERENCES entites(id) ON DELETE CASCADE,
    FOREIGN KEY(evidence_id) REFERENCES preuves(id) ON DELETE CASCADE,
    FOREIGN KEY(ocr_run_id) REFERENCES identity_ocr_runs(id) ON DELETE CASCADE);
CREATE TABLE IF NOT EXISTS identity_field_observations (
    id TEXT PRIMARY KEY, observation_id TEXT NOT NULL, field_code TEXT NOT NULL,
    raw_value TEXT, corrected_value TEXT, normalized_value TEXT,
    confidence REAL, review_status TEXT NOT NULL, origin TEXT NOT NULL
      CHECK(origin IN ('ocr','mrz','manual_override','manual_entry')),
    evidence_id TEXT NOT NULL, ocr_run_id TEXT NOT NULL,
    page_number INTEGER NOT NULL, source_x INTEGER, source_y INTEGER,
    source_width INTEGER, source_height INTEGER, source_image_width INTEGER,
    source_image_height INTEGER, display_order INTEGER NOT NULL,
    reviewed_at TEXT NOT NULL, review_note TEXT,
    CHECK (
      (origin = 'manual_entry' AND raw_value IS NULL
       AND corrected_value IS NOT NULL AND confidence IS NULL)
      OR
      (origin <> 'manual_entry' AND raw_value IS NOT NULL)
    ),
    FOREIGN KEY(observation_id) REFERENCES identity_document_observations(id)
      ON DELETE CASCADE,
    FOREIGN KEY(evidence_id) REFERENCES preuves(id) ON DELETE CASCADE,
    FOREIGN KEY(ocr_run_id) REFERENCES identity_ocr_runs(id) ON DELETE CASCADE);
CREATE INDEX IF NOT EXISTS idx_identity_fields_observation
    ON identity_field_observations(observation_id,display_order);
