/* Schéma autonome V21 : historique V1-V20 suivi du pivot financier V21. */
/******************************************************************************
 * Labfy Investigation
 *
 * Schéma SQLite officiel V1
 ******************************************************************************/

CREATE TABLE metadata
(
    key   TEXT PRIMARY KEY,
    value TEXT NOT NULL
);

CREATE TABLE investigation
(
    id          TEXT PRIMARY KEY,
    name        TEXT NOT NULL,
    root_path   TEXT NOT NULL,
    created_at  TEXT NOT NULL,
    updated_at  TEXT NOT NULL
);

CREATE TABLE categories
(
    id          TEXT PRIMARY KEY,

    nom         TEXT NOT NULL COLLATE NOCASE,
    description TEXT,

    icone       TEXT,
    couleur     TEXT,

    system      INTEGER NOT NULL DEFAULT 0,

    created_at  TEXT NOT NULL,
    updated_at  TEXT NOT NULL,

    status      TEXT NOT NULL DEFAULT 'active',

    UNIQUE (nom),

    CHECK (
        length(trim(nom)) > 0
    ),

    CHECK (
        description IS NULL
        OR length(trim(description)) > 0
    ),

    CHECK (
        icone IS NULL
        OR length(trim(icone)) > 0
    ),

    CHECK (
        couleur IS NULL
        OR (
            length(couleur) = 7
            AND substr(couleur, 1, 1) = '#'
        )
    ),

    CHECK (
        system IN (0, 1)
    ),

    CHECK (
        status IN (
            'active',
            'archived',
            'deleted'
        )
    )
);

CREATE INDEX idx_categories_status
ON categories(status);

CREATE TABLE tags
(
    id          TEXT PRIMARY KEY,

    nom         TEXT NOT NULL COLLATE NOCASE,
    description TEXT,
    couleur     TEXT,

    system      INTEGER NOT NULL DEFAULT 0,

    created_at  TEXT NOT NULL,
    updated_at  TEXT NOT NULL,

    status      TEXT NOT NULL DEFAULT 'active',

    UNIQUE (nom),

    CHECK (
        length(trim(nom)) > 0
    ),

    CHECK (
        description IS NULL
        OR length(trim(description)) > 0
    ),

    CHECK (
        couleur IS NULL
        OR (
            length(couleur) = 7
            AND substr(couleur, 1, 1) = '#'
        )
    ),

    CHECK (
        system IN (0, 1)
    ),

    CHECK (
        status IN (
            'active',
            'archived',
            'deleted'
        )
    )
);

CREATE INDEX idx_tags_status
ON tags(status);

CREATE TABLE types_preuve
(
    id          INTEGER PRIMARY KEY,
    code        TEXT NOT NULL UNIQUE,
    label       TEXT NOT NULL,
    description TEXT
);

INSERT INTO types_preuve
(id, code, label)
VALUES
(1, 'screenshot', 'Capture d''écran'),
(2, 'photo', 'Photographie'),
(3, 'video', 'Vidéo'),
(4, 'document', 'Document'),
(5, 'email', 'Courrier électronique'),
(6, 'archive', 'Archive'),
(7, 'audio', 'Audio'),
(8, 'text', 'Texte'),
(9, 'other', 'Autre');

CREATE TABLE types_entite
(
    id          INTEGER PRIMARY KEY,
    code        TEXT NOT NULL UNIQUE,
    label       TEXT NOT NULL,
    description TEXT
);

CREATE TABLE tag_preuves
(
    tag_id    TEXT NOT NULL,
    preuve_id TEXT NOT NULL,

    PRIMARY KEY (tag_id, preuve_id),

    FOREIGN KEY (tag_id)
        REFERENCES tags(id)
        ON UPDATE CASCADE
        ON DELETE CASCADE,

    FOREIGN KEY (preuve_id)
        REFERENCES preuves(id)
        ON UPDATE CASCADE
        ON DELETE RESTRICT
);

CREATE INDEX idx_tag_preuves_preuve_id
ON tag_preuves(preuve_id);

CREATE TABLE tag_recherches
(
    tag_id       TEXT NOT NULL,
    recherche_id TEXT NOT NULL,

    PRIMARY KEY (tag_id, recherche_id),

    FOREIGN KEY (tag_id)
        REFERENCES tags(id)
        ON UPDATE CASCADE
        ON DELETE CASCADE,

    FOREIGN KEY (recherche_id)
        REFERENCES recherches(id)
        ON UPDATE CASCADE
        ON DELETE RESTRICT
);

CREATE INDEX idx_tag_recherches_recherche_id
ON tag_recherches(recherche_id);

CREATE TABLE tag_entites
(
    tag_id    TEXT NOT NULL,
    entite_id TEXT NOT NULL,

    PRIMARY KEY (tag_id, entite_id),

    FOREIGN KEY (tag_id)
        REFERENCES tags(id)
        ON UPDATE CASCADE
        ON DELETE CASCADE,

    FOREIGN KEY (entite_id)
        REFERENCES entites(id)
        ON UPDATE CASCADE
        ON DELETE RESTRICT
);

CREATE INDEX idx_tag_entites_entite_id
ON tag_entites(entite_id);

CREATE TABLE tag_relations
(
    tag_id      TEXT NOT NULL,
    relation_id TEXT NOT NULL,

    PRIMARY KEY (tag_id, relation_id),

    FOREIGN KEY (tag_id)
        REFERENCES tags(id)
        ON UPDATE CASCADE
        ON DELETE CASCADE,

    FOREIGN KEY (relation_id)
        REFERENCES relations(id)
        ON UPDATE CASCADE
        ON DELETE RESTRICT
);

CREATE INDEX idx_tag_relations_relation_id
ON tag_relations(relation_id);

CREATE TABLE tag_hypotheses
(
    tag_id       TEXT NOT NULL,
    hypothese_id TEXT NOT NULL,

    PRIMARY KEY (tag_id, hypothese_id),

    FOREIGN KEY (tag_id)
        REFERENCES tags(id)
        ON UPDATE CASCADE
        ON DELETE CASCADE,

    FOREIGN KEY (hypothese_id)
        REFERENCES hypotheses(id)
        ON UPDATE CASCADE
        ON DELETE RESTRICT
);

CREATE INDEX idx_tag_hypotheses_hypothese_id
ON tag_hypotheses(hypothese_id);

CREATE TABLE tag_chronologie
(
    tag_id         TEXT NOT NULL,
    chronologie_id TEXT NOT NULL,

    PRIMARY KEY (tag_id, chronologie_id),

    FOREIGN KEY (tag_id)
        REFERENCES tags(id)
        ON UPDATE CASCADE
        ON DELETE CASCADE,

    FOREIGN KEY (chronologie_id)
        REFERENCES chronologie(id)
        ON UPDATE CASCADE
        ON DELETE RESTRICT
);

CREATE INDEX idx_tag_chronologie_chronologie_id
ON tag_chronologie(chronologie_id);

INSERT INTO types_entite
(id, code, label)
VALUES
(1, 'email_address', 'Adresse email'),
(2, 'bank_account', 'Compte bancaire'),
(3, 'facebook_account', 'Compte Facebook'),
(4, 'instagram_account', 'Compte Instagram'),
(5, 'identity_document', 'Document d''identité'),
(6, 'iban', 'IBAN'),
(7, 'person', 'Personne'),
(8, 'pseudonym', 'Pseudonyme'),
(9, 'phone_number', 'Numéro de téléphone'),
(10, 'website', 'Site web'),
(11, 'domain_name', 'Nom de domaine'),
(12, 'ip_address', 'Adresse IP'),
(13, 'organization', 'Organisation'),
(14, 'other', 'Autre');

CREATE TABLE types_source
(
    id          INTEGER PRIMARY KEY,
    code        TEXT NOT NULL UNIQUE,
    label       TEXT NOT NULL,
    description TEXT
);

INSERT INTO types_source
(id, code, label)
VALUES
(1, 'website', 'Site web'),
(2, 'social_network', 'Réseau social'),
(3, 'email', 'Courrier électronique'),
(4, 'document', 'Document'),
(5, 'testimony', 'Témoignage'),
(6, 'phone_export', 'Export de téléphone'),
(7, 'disk_export', 'Export de disque'),
(8, 'osint_tool', 'Outil OSINT'),
(9, 'manual_entry', 'Saisie manuelle'),
(10, 'other', 'Autre');

CREATE TABLE sources
(
    id          TEXT PRIMARY KEY,

    type_id     INTEGER NOT NULL,

    nom         TEXT NOT NULL,
    reference   TEXT,
    description TEXT,

    created_at  TEXT NOT NULL,
    updated_at  TEXT NOT NULL,

    FOREIGN KEY (type_id)
        REFERENCES types_source(id)
        ON UPDATE CASCADE
        ON DELETE RESTRICT,

    CHECK (length(trim(nom)) > 0),

    CHECK (
        reference IS NULL
        OR length(trim(reference)) > 0
    )
);

CREATE INDEX idx_sources_type_id
ON sources(type_id);

CREATE INDEX idx_sources_reference
ON sources(reference);

CREATE TABLE preuves
(
    id              TEXT PRIMARY KEY,

    name            TEXT NOT NULL,
    relative_path   TEXT NOT NULL UNIQUE,

    type_id         INTEGER NOT NULL,

    size_bytes      INTEGER,
    sha256          TEXT,
    mime_type       TEXT,

    description     TEXT,
    commentaire     TEXT,
    categorie_id    TEXT,

    file_created_at TEXT,
    imported_at     TEXT NOT NULL,
    updated_at      TEXT NOT NULL,

    status          TEXT NOT NULL DEFAULT 'active',
    locked          INTEGER NOT NULL DEFAULT 0,

    FOREIGN KEY (type_id)
        REFERENCES types_preuve(id)
        ON UPDATE CASCADE
        ON DELETE RESTRICT,

    FOREIGN KEY (categorie_id)
        REFERENCES categories(id)
        ON UPDATE CASCADE
        ON DELETE SET NULL

    CHECK (length(trim(name)) > 0),

    CHECK (length(trim(relative_path)) > 0),

    CHECK (
        size_bytes IS NULL
        OR size_bytes >= 0
    ),

    CHECK (
        sha256 IS NULL
        OR (
            length(sha256) = 64
            AND sha256 = lower(sha256)
        )
    ),

    CHECK (
        status IN (
            'active',
            'archived',
            'deleted'
        )
    ),

    CHECK (
        locked IN (0, 1)
    )
);

CREATE INDEX idx_preuves_type_id
ON preuves(type_id);

CREATE INDEX idx_preuves_status
ON preuves(status);

CREATE INDEX idx_preuves_sha256
ON preuves(sha256);

CREATE INDEX idx_preuves_categorie_id
ON preuves(categorie_id);

CREATE TABLE recherches
(
    id              TEXT PRIMARY KEY,

    source_id       TEXT,
    type_outil_id   INTEGER NOT NULL,

    outil_nom       TEXT NOT NULL,
    requete         TEXT,
    resultat        TEXT,
    observations    TEXT,
    categorie_id    TEXT,

    started_at      TEXT NOT NULL,
    completed_at    TEXT,

    created_at      TEXT NOT NULL,
    updated_at      TEXT NOT NULL,

    status          TEXT NOT NULL DEFAULT 'planned',

    FOREIGN KEY (source_id)
        REFERENCES sources(id)
        ON UPDATE CASCADE
        ON DELETE SET NULL,

    FOREIGN KEY (type_outil_id)
        REFERENCES types_outil(id)
        ON UPDATE CASCADE
        ON DELETE RESTRICT,

    FOREIGN KEY (categorie_id)
        REFERENCES categories(id)
        ON UPDATE CASCADE
        ON DELETE SET NULL,

    CHECK (length(trim(outil_nom)) > 0),

    CHECK (
        requete IS NULL
        OR length(trim(requete)) > 0
    ),

    CHECK (
        completed_at IS NULL
        OR completed_at >= started_at
    ),

    CHECK (
        status IN (
            'planned',
            'running',
            'completed',
            'failed',
            'cancelled',
            'archived'
        )
    )
);

CREATE INDEX idx_recherches_source_id
ON recherches(source_id);

CREATE INDEX idx_recherches_type_outil_id
ON recherches(type_outil_id);

CREATE INDEX idx_recherches_status
ON recherches(status);

CREATE INDEX idx_recherches_started_at
ON recherches(started_at);

CREATE INDEX idx_recherches_categorie_id
ON recherches(categorie_id);

CREATE TABLE recherche_preuves
(
    recherche_id TEXT NOT NULL,
    preuve_id    TEXT NOT NULL,

    role         TEXT NOT NULL,

    PRIMARY KEY (
        recherche_id,
        preuve_id,
        role
    ),

    FOREIGN KEY (recherche_id)
        REFERENCES recherches(id)
        ON UPDATE CASCADE
        ON DELETE CASCADE,

    FOREIGN KEY (preuve_id)
        REFERENCES preuves(id)
        ON UPDATE CASCADE
        ON DELETE RESTRICT,

    CHECK (
        role IN (
            'input',
            'output'
        )
    )
);

CREATE INDEX idx_recherche_preuves_preuve_id
ON recherche_preuves(preuve_id);

CREATE TABLE entites
(
    id          TEXT PRIMARY KEY,

    type_id     INTEGER NOT NULL,

    valeur      TEXT NOT NULL,
    label       TEXT,
    description TEXT,

    confiance   INTEGER NOT NULL DEFAULT 50,

    created_at  TEXT NOT NULL,
    updated_at  TEXT NOT NULL,

    status      TEXT NOT NULL DEFAULT 'active',

    UNIQUE (type_id, valeur),

    FOREIGN KEY (type_id)
        REFERENCES types_entite(id)
        ON UPDATE CASCADE
        ON DELETE RESTRICT,

    CHECK (length(trim(valeur)) > 0),

    CHECK (
        label IS NULL
        OR length(trim(label)) > 0
    ),

    CHECK (
        confiance BETWEEN 0 AND 100
    ),

    CHECK (
        status IN (
            'active',
            'archived',
            'deleted'
        )
    )
);

CREATE INDEX idx_entites_type_id
ON entites(type_id);

CREATE INDEX idx_entites_valeur
ON entites(valeur);

CREATE INDEX idx_entites_status
ON entites(status);

CREATE INDEX idx_entites_confiance
ON entites(confiance);

CREATE TABLE types_outil
(
    id          INTEGER PRIMARY KEY,
    code        TEXT NOT NULL UNIQUE,
    label       TEXT NOT NULL,
    description TEXT
);

CREATE TABLE recherche_entites
(
    recherche_id TEXT NOT NULL,
    entite_id    TEXT NOT NULL,

    role         TEXT NOT NULL,

    PRIMARY KEY (
        recherche_id,
        entite_id,
        role
    ),

    FOREIGN KEY (recherche_id)
        REFERENCES recherches(id)
        ON UPDATE CASCADE
        ON DELETE CASCADE,

    FOREIGN KEY (entite_id)
        REFERENCES entites(id)
        ON UPDATE CASCADE
        ON DELETE RESTRICT,

    CHECK (
        role IN (
            'discovered',
            'enriched',
            'validated',
            'contradicted'
        )
    )
);

CREATE INDEX idx_recherche_entites_entite_id
ON recherche_entites(entite_id);

CREATE TABLE preuve_entites
(
    preuve_id TEXT NOT NULL,
    entite_id TEXT NOT NULL,

    PRIMARY KEY (
        preuve_id,
        entite_id
    ),

    FOREIGN KEY (preuve_id)
        REFERENCES preuves(id)
        ON UPDATE CASCADE
        ON DELETE RESTRICT,

    FOREIGN KEY (entite_id)
        REFERENCES entites(id)
        ON UPDATE CASCADE
        ON DELETE RESTRICT
);

CREATE INDEX idx_preuve_entites_entite_id
ON preuve_entites(entite_id);

CREATE TABLE relations
(
    id                  TEXT PRIMARY KEY,

    entite_source_id    TEXT NOT NULL,
    entite_cible_id     TEXT NOT NULL,

    type_relation       TEXT NOT NULL,
    label               TEXT,
    justification       TEXT,

    confiance           INTEGER NOT NULL DEFAULT 50,

    created_at          TEXT NOT NULL,
    updated_at          TEXT NOT NULL,

    status              TEXT NOT NULL DEFAULT 'active',

    UNIQUE (
        entite_source_id,
        entite_cible_id,
        type_relation
    ),

    FOREIGN KEY (entite_source_id)
        REFERENCES entites(id)
        ON UPDATE CASCADE
        ON DELETE RESTRICT,

    FOREIGN KEY (entite_cible_id)
        REFERENCES entites(id)
        ON UPDATE CASCADE
        ON DELETE RESTRICT,

    CHECK (
        entite_source_id <> entite_cible_id
    ),

    CHECK (
        length(trim(type_relation)) > 0
    ),

    CHECK (
        label IS NULL
        OR length(trim(label)) > 0
    ),

    CHECK (
        confiance BETWEEN 0 AND 100
    ),

    CHECK (
        status IN (
            'active',
            'archived',
            'deleted',
            'disputed'
        )
    )
);

CREATE INDEX idx_relations_entite_source_id
ON relations(entite_source_id);

CREATE INDEX idx_relations_entite_cible_id
ON relations(entite_cible_id);

CREATE INDEX idx_relations_type_relation
ON relations(type_relation);

CREATE INDEX idx_relations_status
ON relations(status);

CREATE INDEX idx_relations_confiance
ON relations(confiance);

CREATE TABLE relation_preuves
(
    relation_id TEXT NOT NULL,
    preuve_id   TEXT NOT NULL,

    PRIMARY KEY (
        relation_id,
        preuve_id
    ),

    FOREIGN KEY (relation_id)
        REFERENCES relations(id)
        ON UPDATE CASCADE
        ON DELETE CASCADE,

    FOREIGN KEY (preuve_id)
        REFERENCES preuves(id)
        ON UPDATE CASCADE
        ON DELETE RESTRICT
);

CREATE INDEX idx_relation_preuves_preuve_id
ON relation_preuves(preuve_id);

CREATE TABLE recherche_relations
(
    recherche_id TEXT NOT NULL,
    relation_id  TEXT NOT NULL,

    role         TEXT NOT NULL,

    PRIMARY KEY (
        recherche_id,
        relation_id,
        role
    ),

    FOREIGN KEY (recherche_id)
        REFERENCES recherches(id)
        ON UPDATE CASCADE
        ON DELETE CASCADE,

    FOREIGN KEY (relation_id)
        REFERENCES relations(id)
        ON UPDATE CASCADE
        ON DELETE RESTRICT,

    CHECK (
        role IN (
            'discovered',
            'confirmed',
            'contradicted'
        )
    )
);

CREATE INDEX idx_recherche_relations_relation_id
ON recherche_relations(relation_id);

CREATE TABLE chronologie
(
    id          TEXT PRIMARY KEY,

    event_time  TEXT NOT NULL,

    titre       TEXT NOT NULL,
    description TEXT,

    origine     TEXT NOT NULL DEFAULT 'manual',

    created_at  TEXT NOT NULL,
    updated_at  TEXT NOT NULL,

    status      TEXT NOT NULL DEFAULT 'active',

    CHECK (
        length(trim(titre)) > 0
    ),

    CHECK (
        description IS NULL
        OR length(trim(description)) > 0
    ),

    CHECK (
        origine IN (
            'manual',
            'automatic',
            'imported'
        )
    ),

    CHECK (
        status IN (
            'active',
            'archived',
            'deleted'
        )
    )
);

CREATE INDEX idx_chronologie_event_time
ON chronologie(event_time);

CREATE INDEX idx_chronologie_origine
ON chronologie(origine);

CREATE INDEX idx_chronologie_status
ON chronologie(status);

CREATE TABLE recherche_chronologie
(
    recherche_id  TEXT NOT NULL,
    chronologie_id TEXT NOT NULL,

    PRIMARY KEY (
        recherche_id,
        chronologie_id
    ),

    FOREIGN KEY (recherche_id)
        REFERENCES recherches(id)
        ON UPDATE CASCADE
        ON DELETE CASCADE,

    FOREIGN KEY (chronologie_id)
        REFERENCES chronologie(id)
        ON UPDATE CASCADE
        ON DELETE CASCADE
);

CREATE TABLE preuve_chronologie
(
    preuve_id      TEXT NOT NULL,
    chronologie_id TEXT NOT NULL,

    PRIMARY KEY (
        preuve_id,
        chronologie_id
    ),

    FOREIGN KEY (preuve_id)
        REFERENCES preuves(id)
        ON UPDATE CASCADE
        ON DELETE RESTRICT,

    FOREIGN KEY (chronologie_id)
        REFERENCES chronologie(id)
        ON UPDATE CASCADE
        ON DELETE CASCADE
);

CREATE TABLE entite_chronologie
(
    entite_id      TEXT NOT NULL,
    chronologie_id TEXT NOT NULL,

    PRIMARY KEY (
        entite_id,
        chronologie_id
    ),

    FOREIGN KEY (entite_id)
        REFERENCES entites(id)
        ON UPDATE CASCADE
        ON DELETE RESTRICT,

    FOREIGN KEY (chronologie_id)
        REFERENCES chronologie(id)
        ON UPDATE CASCADE
        ON DELETE CASCADE
);

CREATE TABLE relation_chronologie
(
    relation_id    TEXT NOT NULL,
    chronologie_id TEXT NOT NULL,

    PRIMARY KEY (
        relation_id,
        chronologie_id
    ),

    FOREIGN KEY (relation_id)
        REFERENCES relations(id)
        ON UPDATE CASCADE
        ON DELETE RESTRICT,

    FOREIGN KEY (chronologie_id)
        REFERENCES chronologie(id)
        ON UPDATE CASCADE
        ON DELETE CASCADE
);

CREATE INDEX idx_recherche_chronologie_chronologie_id
ON recherche_chronologie(chronologie_id);

CREATE INDEX idx_preuve_chronologie_chronologie_id
ON preuve_chronologie(chronologie_id);

CREATE INDEX idx_entite_chronologie_chronologie_id
ON entite_chronologie(chronologie_id);

CREATE INDEX idx_relation_chronologie_chronologie_id
ON relation_chronologie(chronologie_id);

CREATE TABLE journal
(
    id          TEXT PRIMARY KEY,

    event_time  TEXT NOT NULL,
    action      TEXT NOT NULL,

    objet_type  TEXT,
    objet_id    TEXT,

    resultat    TEXT NOT NULL,

    details     TEXT,
    acteur      TEXT,

    created_at  TEXT NOT NULL,

    CHECK (
        length(trim(action)) > 0
    ),

    CHECK (
        objet_type IS NULL
        OR length(trim(objet_type)) > 0
    ),

    CHECK (
        objet_id IS NULL
        OR length(trim(objet_id)) > 0
    ),

    CHECK (
        (
            objet_type IS NULL
            AND objet_id IS NULL
        )
        OR
        (
            objet_type IS NOT NULL
            AND objet_id IS NOT NULL
        )
    ),

    CHECK (
        resultat IN (
            'success',
            'failure',
            'partial',
            'cancelled'
        )
    ),

    CHECK (
        details IS NULL
        OR length(trim(details)) > 0
    ),

    CHECK (
        acteur IS NULL
        OR length(trim(acteur)) > 0
    )
);

CREATE INDEX idx_journal_event_time
ON journal(event_time);

CREATE INDEX idx_journal_action
ON journal(action);

CREATE INDEX idx_journal_objet
ON journal(objet_type, objet_id);

CREATE INDEX idx_journal_resultat
ON journal(resultat);

CREATE TABLE hypotheses
(
    id          TEXT PRIMARY KEY,

    titre           TEXT NOT NULL,
    description     TEXT NOT NULL,
    categorie_id    TEXT,

    confiance       INTEGER NOT NULL DEFAULT 50,
    evaluation      TEXT,

    created_at      TEXT NOT NULL,
    updated_at      TEXT NOT NULL,

    status          TEXT NOT NULL DEFAULT 'proposed',

    FOREIGN KEY (categorie_id)
        REFERENCES categories(id)
        ON UPDATE CASCADE
        ON DELETE SET NULL

    CHECK (
        length(trim(titre)) > 0
    ),

    CHECK (
        length(trim(description)) > 0
    ),

    CHECK (
        confiance BETWEEN 0 AND 100
    ),

    CHECK (
        evaluation IS NULL
        OR length(trim(evaluation)) > 0
    ),

    CHECK (
        status IN (
            'proposed',
            'under_review',
            'supported',
            'contradicted',
            'confirmed',
            'rejected',
            'archived'
        )
    )
);

CREATE INDEX idx_hypotheses_status
ON hypotheses(status);

CREATE INDEX idx_hypotheses_confiance
ON hypotheses(confiance);

CREATE INDEX idx_hypotheses_updated_at
ON hypotheses(updated_at);

CREATE INDEX idx_hypotheses_categorie_id
ON hypotheses(categorie_id);

CREATE TABLE hypothese_preuves
(
    hypothese_id TEXT NOT NULL,
    preuve_id    TEXT NOT NULL,

    role         TEXT NOT NULL,

    PRIMARY KEY (
        hypothese_id,
        preuve_id,
        role
    ),

    FOREIGN KEY (hypothese_id)
        REFERENCES hypotheses(id)
        ON UPDATE CASCADE
        ON DELETE CASCADE,

    FOREIGN KEY (preuve_id)
        REFERENCES preuves(id)
        ON UPDATE CASCADE
        ON DELETE RESTRICT,

    CHECK (
        role IN (
            'supports',
            'contradicts',
            'confirms'
        )
    )
);

CREATE INDEX idx_hypothese_preuves_preuve_id
ON hypothese_preuves(preuve_id);

CREATE TABLE hypothese_entites
(
    hypothese_id TEXT NOT NULL,
    entite_id    TEXT NOT NULL,

    PRIMARY KEY (
        hypothese_id,
        entite_id
    ),

    FOREIGN KEY (hypothese_id)
        REFERENCES hypotheses(id)
        ON UPDATE CASCADE
        ON DELETE CASCADE,

    FOREIGN KEY (entite_id)
        REFERENCES entites(id)
        ON UPDATE CASCADE
        ON DELETE RESTRICT
);

CREATE INDEX idx_hypothese_entites_entite_id
ON hypothese_entites(entite_id);

CREATE TABLE hypothese_relations
(
    hypothese_id TEXT NOT NULL,
    relation_id  TEXT NOT NULL,

    role         TEXT NOT NULL,

    PRIMARY KEY (
        hypothese_id,
        relation_id,
        role
    ),

    FOREIGN KEY (hypothese_id)
        REFERENCES hypotheses(id)
        ON UPDATE CASCADE
        ON DELETE CASCADE,

    FOREIGN KEY (relation_id)
        REFERENCES relations(id)
        ON UPDATE CASCADE
        ON DELETE RESTRICT,

    CHECK (
        role IN (
            'supports',
            'contradicts'
        )
    )
);

CREATE INDEX idx_hypothese_relations_relation_id
ON hypothese_relations(relation_id);

CREATE TABLE recherche_hypotheses
(
    recherche_id TEXT NOT NULL,
    hypothese_id TEXT NOT NULL,

    role         TEXT NOT NULL,

    PRIMARY KEY (
        recherche_id,
        hypothese_id,
        role
    ),

    FOREIGN KEY (recherche_id)
        REFERENCES recherches(id)
        ON UPDATE CASCADE
        ON DELETE CASCADE,

    FOREIGN KEY (hypothese_id)
        REFERENCES hypotheses(id)
        ON UPDATE CASCADE
        ON DELETE RESTRICT,

    CHECK (
        role IN (
            'created',
            'enriched',
            'confirmed',
            'contradicted'
        )
    )
);

CREATE INDEX idx_recherche_hypotheses_hypothese_id
ON recherche_hypotheses(hypothese_id);

INSERT INTO types_outil
(id, code, label)
VALUES
(1, 'web_browser', 'Navigateur web'),
(2, 'search_engine', 'Moteur de recherche'),
(3, 'dns_tool', 'Outil DNS'),
(4, 'whois_tool', 'Outil WHOIS'),
(5, 'social_network', 'Réseau social'),
(6, 'email_tool', 'Outil email'),
(7, 'metadata_tool', 'Outil de métadonnées'),
(8, 'hash_tool', 'Outil de calcul de hash'),
(9, 'ocr_tool', 'Outil OCR'),
(10, 'custom_script', 'Script personnalisé'),
(11, 'manual_process', 'Procédure manuelle'),
(12, 'other', 'Autre');

/******************************************************************************
 * Labfy Investigation
 *
 * Migration du schéma SQLite V1 vers V2
 ******************************************************************************/

/*
 * Nom du fichier tel qu’il existait avant son import.
 *
 * La colonne name existante conserve le nom interne utilisé dans
 * l’arborescence de l’enquête.
 */
ALTER TABLE preuves
ADD COLUMN original_name TEXT;

/*
 * Date déclarée de collecte de la preuve.
 *
 * Elle est distincte de :
 *
 * - file_created_at : date technique connue du fichier ;
 * - imported_at     : date d’import dans l’enquête.
 */
ALTER TABLE preuves
ADD COLUMN collected_at TEXT;

/*
 * Description textuelle initiale de la provenance.
 *
 * Une future évolution pourra relier une preuve à la table sources
 * avec un identifiant métier.
 */
ALTER TABLE preuves
ADD COLUMN source TEXT;

/*
 * Correspondance avec EvidenceIntegrityStatus :
 *
 * 0 = UNKNOWN
 * 1 = VALID
 * 2 = MISSING
 * 3 = MODIFIED
 * 4 = ERROR
 */
ALTER TABLE preuves
ADD COLUMN integrity_status INTEGER NOT NULL DEFAULT 0
CHECK (
    integrity_status BETWEEN 0 AND 4
);

/*
 * Les éventuelles preuves V1 utilisent leur ancien nom visible comme
 * nom original afin de rester lisibles après migration.
 */
UPDATE preuves
SET original_name = name
WHERE original_name IS NULL
   OR length(trim(original_name)) = 0;

CREATE INDEX idx_preuves_imported_at
ON preuves(imported_at);

/*
 * SQLite ne permet pas d’ajouter directement une contrainte NOT NULL
 * à une colonne ajoutée lorsque des lignes peuvent déjà exister.
 *
 * Ces triggers renforcent donc les insertions et modifications V2.
 */
CREATE TRIGGER preuves_v2_validate_insert
BEFORE INSERT ON preuves
FOR EACH ROW
WHEN
       NEW.original_name IS NULL
    OR length(trim(NEW.original_name)) = 0
    OR NEW.size_bytes IS NULL
    OR NEW.size_bytes < 0
    OR NEW.sha256 IS NULL
    OR length(NEW.sha256) != 64
    OR NEW.sha256 != lower(NEW.sha256)
    OR NEW.integrity_status NOT BETWEEN 0 AND 4
BEGIN
    SELECT RAISE(
        ABORT,
        'La preuve ne respecte pas les contraintes du schéma V2.'
    );
END;

CREATE TRIGGER preuves_v2_validate_update
BEFORE UPDATE ON preuves
FOR EACH ROW
WHEN
       NEW.original_name IS NULL
    OR length(trim(NEW.original_name)) = 0
    OR NEW.size_bytes IS NULL
    OR NEW.size_bytes < 0
    OR NEW.sha256 IS NULL
    OR length(NEW.sha256) != 64
    OR NEW.sha256 != lower(NEW.sha256)
    OR NEW.integrity_status NOT BETWEEN 0 AND 4
BEGIN
    SELECT RAISE(
        ABORT,
        'La preuve ne respecte pas les contraintes du schéma V2.'
    );
END;
/******************************************************************************
 * Labfy Investigation
 *
 * Migration du schéma SQLite V2 vers V3 : provenance OSINT structurée
 ******************************************************************************/

CREATE TABLE osint_executions
(
    id                  TEXT PRIMARY KEY,
    tool_identifier     TEXT NOT NULL,
    tool_version        TEXT,
    action_identifier   TEXT NOT NULL,
    selection_id        TEXT NOT NULL,
    selection_kind      TEXT NOT NULL,
    target_value        TEXT NOT NULL,
    arguments           TEXT NOT NULL,
    started_at          TEXT NOT NULL,
    finished_at         TEXT NOT NULL,
    exit_code           INTEGER,
    final_state         TEXT NOT NULL,
    stdout_raw          BLOB NOT NULL,
    stderr_raw          BLOB NOT NULL,
    output_sha256       TEXT NOT NULL,

    CHECK (length(id) = 36),
    CHECK (length(trim(tool_identifier)) > 0),
    CHECK (length(trim(action_identifier)) > 0),
    CHECK (length(selection_id) = 36),
    CHECK (selection_kind IN ('entity', 'relation')),
    CHECK (length(trim(target_value)) > 0),
    CHECK (length(started_at) = 20),
    CHECK (length(finished_at) = 20),
    CHECK (final_state IN ('completed', 'failed', 'cancelled')),
    CHECK (length(output_sha256) = 64),
    CHECK (output_sha256 = lower(output_sha256))
);

CREATE INDEX idx_osint_executions_finished_at
ON osint_executions(finished_at);

CREATE INDEX idx_osint_executions_selection
ON osint_executions(selection_kind, selection_id);

CREATE TABLE osint_execution_entities
(
    execution_id TEXT NOT NULL,
    entity_id    TEXT NOT NULL,
    disposition  TEXT NOT NULL,

    PRIMARY KEY (execution_id, entity_id),

    FOREIGN KEY (execution_id)
        REFERENCES osint_executions(id)
        ON UPDATE CASCADE
        ON DELETE CASCADE,

    FOREIGN KEY (entity_id)
        REFERENCES entites(id)
        ON UPDATE CASCADE
        ON DELETE RESTRICT,

    CHECK (disposition IN ('created', 'reused'))
);

CREATE INDEX idx_osint_execution_entities_entity
ON osint_execution_entities(entity_id);

CREATE TABLE osint_execution_relations
(
    execution_id TEXT NOT NULL,
    relation_id  TEXT NOT NULL,
    disposition  TEXT NOT NULL,

    PRIMARY KEY (execution_id, relation_id),

    FOREIGN KEY (execution_id)
        REFERENCES osint_executions(id)
        ON UPDATE CASCADE
        ON DELETE CASCADE,

    FOREIGN KEY (relation_id)
        REFERENCES relations(id)
        ON UPDATE CASCADE
        ON DELETE RESTRICT,

    CHECK (disposition IN ('created', 'reused'))
);

CREATE INDEX idx_osint_execution_relations_relation
ON osint_execution_relations(relation_id);
/******************************************************************************
 * Labfy Investigation
 *
 * Migration du schéma SQLite V3 vers V4 : comptes de réseaux sociaux
 ******************************************************************************/

INSERT INTO types_entite (code, label, description) VALUES
    ('tiktok_account', 'Compte TikTok', NULL),
    ('x_account', 'Compte X', NULL),
    ('telegram_account', 'Compte Telegram', NULL),
    ('social_account', 'Autre compte social', NULL);

CREATE TABLE comptes_sociaux
(
    entite_id              TEXT PRIMARY KEY,
    plateforme             TEXT NOT NULL,
    url_profil             TEXT NOT NULL,
    pseudonyme             TEXT NOT NULL,
    identifiant_plateforme TEXT,
    premiere_observation   TEXT NOT NULL,
    etat_compte            TEXT NOT NULL DEFAULT 'unknown',
    notes                  TEXT,

    FOREIGN KEY (entite_id)
        REFERENCES entites(id)
        ON UPDATE CASCADE
        ON DELETE CASCADE,

    UNIQUE (plateforme, url_profil),

    CHECK (plateforme IN ('tiktok', 'instagram', 'facebook', 'x', 'telegram', 'other')),
    CHECK (length(trim(url_profil)) > 0),
    CHECK (length(trim(pseudonyme)) > 0),
    CHECK (length(premiere_observation) = 20),
    CHECK (etat_compte IN ('active', 'private', 'suspended', 'deleted', 'unknown'))
);

CREATE INDEX idx_comptes_sociaux_pseudonyme
ON comptes_sociaux(plateforme, pseudonyme);
/******************************************************************************
 * Labfy Investigation
 *
 * Migration du schéma SQLite V4 vers V5 : rôles d'enquête des personnes
 ******************************************************************************/

CREATE TABLE person_roles
(
    entity_id  TEXT PRIMARY KEY,
    role       TEXT NOT NULL DEFAULT 'uncategorized',
    updated_at TEXT NOT NULL,

    FOREIGN KEY (entity_id)
        REFERENCES entites(id)
        ON UPDATE CASCADE
        ON DELETE CASCADE,

    CHECK (role IN
    (
        'uncategorized',
        'alleged_scammer',
        'victim',
        'witness',
        'suspect',
        'related_person'
    )),
    CHECK (length(updated_at) = 20)
);

CREATE INDEX idx_person_roles_role ON person_roles(role);
/******************************************************************************
 * Labfy Investigation
 *
 * Migration du schéma SQLite V5 vers V6 : catégorie identité usurpée
 ******************************************************************************/

ALTER TABLE person_roles RENAME TO person_roles_v5;

CREATE TABLE person_roles
(
    entity_id  TEXT PRIMARY KEY,
    role       TEXT NOT NULL DEFAULT 'uncategorized',
    updated_at TEXT NOT NULL,

    FOREIGN KEY (entity_id)
        REFERENCES entites(id)
        ON UPDATE CASCADE
        ON DELETE CASCADE,

    CHECK (role IN
    (
        'uncategorized',
        'alleged_scammer',
        'victim',
        'witness',
        'suspect',
        'related_person',
        'impersonated_identity'
    )),
    CHECK (length(updated_at) = 20)
);

INSERT INTO person_roles(entity_id, role, updated_at)
SELECT entity_id, role, updated_at FROM person_roles_v5;

DROP TABLE person_roles_v5;

CREATE INDEX idx_person_roles_role ON person_roles(role);
/* Migration SQLite V6 vers V7 : traçabilité des extractions. */
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
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    code           TEXT UNIQUE,
    label          TEXT NOT NULL,
    normalized_key TEXT NOT NULL UNIQUE,
    description    TEXT,
    is_system      INTEGER NOT NULL DEFAULT 0 CHECK (is_system IN (0, 1)),
    CHECK (code IS NULL OR length(trim(code)) > 0),
    CHECK (length(trim(label)) > 0),
    CHECK (length(normalized_key) > 0)
);

INSERT OR IGNORE INTO relation_types(code,label,normalized_key,description,is_system) VALUES
('resolves_to','Résout vers','résout vers','Résolution DNS vers une adresse IP.',1),
('aliases_to','Alias de','alias de','Alias DNS canonique.',1),
('uses_name_server','Utilise le serveur de noms','utilise le serveur de noms','Serveur DNS faisant autorité.',1),
('links_to','Lié à','lié à','Lien générique entre deux entités.',1),
('sends','Envoie','envoie','Transmission d’un élément.',1),
('uses','Utilise','utilise','Utilisation d’une ressource ou identité.',1),
('controls','Contrôle','contrôle','Contrôle d’une entité.',1),
('owns','Possède','possède','Possession d’une entité.',1),
('knows','Connaît','connaît','Connaissance entre personnes.',1),
('redirects_to','Redirige vers','redirige vers','Redirection vers une cible.',1);
/******************************************************************************
 * Labfy Investigation
 *
 * Schéma SQLite officiel V10
 * Extension pour le pivot e-mail, les entités bancaires et la traçabilité.
 ******************************************************************************/

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
/******************************************************************************
 * Schéma SQLite V11 — observations sémantiques preuve-entité.
 ******************************************************************************/
CREATE TABLE IF NOT EXISTS evidence_entity_observations
(
    evidence_id         TEXT NOT NULL,
    entity_id           TEXT NOT NULL,
    entity_type         TEXT NOT NULL,
    value_raw           TEXT NOT NULL,
    value_normalized    TEXT NOT NULL,
    role                TEXT NOT NULL,
    provenance_kind     TEXT NOT NULL,
    source_header       TEXT NOT NULL,
    occurrence          INTEGER NOT NULL DEFAULT 1 CHECK (occurrence > 0),
    verification_status TEXT NOT NULL DEFAULT 'proposed',
    created_at          TEXT NOT NULL CHECK (length(created_at) = 20),
    PRIMARY KEY (evidence_id, entity_id, role, source_header, occurrence),
    FOREIGN KEY (evidence_id) REFERENCES preuves(id) ON DELETE CASCADE,
    FOREIGN KEY (entity_id) REFERENCES entites(id) ON DELETE CASCADE,
    CHECK (length(trim(role)) > 0),
    CHECK (length(trim(source_header)) > 0)
);
CREATE INDEX IF NOT EXISTS idx_evidence_entity_observations_evidence
    ON evidence_entity_observations(evidence_id);
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
/******************************************************************************
 * Labfy Investigation
 *
 * Extensions idempotentes du schéma SQLite courant V20
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
    confirmed_value TEXT,
    confirmation_state TEXT NOT NULL DEFAULT 'unconfirmed'
      CHECK(confirmation_state IN ('unconfirmed','human_confirmed')),
    value_quality TEXT NOT NULL DEFAULT 'complete'
      CHECK(value_quality IN ('complete','partial','uncertain','invalid')),
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

CREATE TABLE IF NOT EXISTS identification_status_vocabulary(
 code TEXT PRIMARY KEY,label TEXT NOT NULL,description TEXT NOT NULL,
 display_order INTEGER NOT NULL UNIQUE,
 active INTEGER NOT NULL DEFAULT 1 CHECK(active IN(0,1)),
 requires_justification INTEGER NOT NULL DEFAULT 0 CHECK(requires_justification IN(0,1)),
 sensitive INTEGER NOT NULL DEFAULT 0 CHECK(sensitive IN(0,1)));
INSERT OR IGNORE INTO identification_status_vocabulary VALUES
('unknown','Inconnu','Aucune identification disponible.',10,1,0,0),
('unverified','Non vérifié','Identification déclarée, non vérifiée.',20,1,0,0),
('presumed','Présumé','Identification présumée à justifier.',30,1,1,1),
('partially_identified','Partiellement identifié','Identification incomplète.',40,1,0,0),
('confirmed','Confirmé','Identification confirmée à justifier.',50,1,1,1),
('disputed','Contesté','Identification contestée à justifier.',60,1,1,1);
CREATE TABLE IF NOT EXISTS person_role_vocabulary(
 code TEXT PRIMARY KEY,label TEXT NOT NULL,description TEXT NOT NULL,
 display_order INTEGER NOT NULL UNIQUE,active INTEGER NOT NULL CHECK(active IN(0,1)),
 requires_justification INTEGER NOT NULL CHECK(requires_justification IN(0,1)),
 sensitive INTEGER NOT NULL CHECK(sensitive IN(0,1)));
INSERT OR IGNORE INTO person_role_vocabulary VALUES
('alleged_author','Auteur présumé','Rôle allégué.',10,1,1,1),
('presented_identity','Identité présentée','Identité déclarée.',20,1,0,0),
('potentially_impersonated_identity','Identité potentiellement usurpée','Hypothèse sensible.',30,1,1,1),
('victim','Victime','Personne déclarée victime.',40,1,0,0),
('witness','Témoin','Personne déclarée témoin.',50,1,0,0),
('declared_bank_holder','Titulaire bancaire déclaré','Titulaire déclaré.',60,1,0,0),
('intermediary','Intermédiaire','Intermédiaire observé.',70,1,0,0),
('mentioned_person','Personne citée','Personne citée.',80,1,0,0),
('other','Autre','Rôle factuel non couvert.',90,1,1,0),
('uncategorized','Non catégorisée','Code historique.',100,1,0,0),
('alleged_scammer','Escroc présumé (historique)','Code historique.',110,0,1,1),
('suspect','Suspect (historique)','Code historique.',120,0,1,1),
('related_person','Personne liée (historique)','Code historique.',130,0,0,0),
('impersonated_identity','Identité usurpée (historique)','Code historique.',140,0,1,1);
CREATE TABLE IF NOT EXISTS document_authenticity_assessments(
 id TEXT PRIMARY KEY,evidence_id TEXT NOT NULL,ocr_run_id TEXT,status TEXT NOT NULL
 CHECK(status IN('indeterminate','presumed_authentic','suspicious',
 'presumed_forged','confirmed_forged')),justification TEXT,
 assessed_at TEXT NOT NULL,previous_assessment_id TEXT,technical_note TEXT,
 origin TEXT NOT NULL CHECK(origin='human'),
 CHECK(status='indeterminate' OR
 (justification IS NOT NULL AND length(trim(justification))>0)),
 FOREIGN KEY(evidence_id) REFERENCES preuves(id) ON DELETE RESTRICT,
 FOREIGN KEY(ocr_run_id) REFERENCES identity_ocr_runs(id) ON DELETE SET NULL,
 FOREIGN KEY(previous_assessment_id) REFERENCES document_authenticity_assessments(id));
CREATE TABLE IF NOT EXISTS person_evidence_factual_relations(
 id TEXT PRIMARY KEY,person_id TEXT NOT NULL,evidence_id TEXT NOT NULL,
 ocr_run_id TEXT,relation_type TEXT NOT NULL CHECK(relation_type IN(
 'identity_observed_in','document_presented_in_name_of','declared_holder_in',
 'data_extracted_from')),factual_note TEXT,observed_at TEXT NOT NULL,
 origin TEXT NOT NULL CHECK(origin='human'),active INTEGER NOT NULL DEFAULT 1
 CHECK(active IN(0,1)),FOREIGN KEY(person_id) REFERENCES entites(id),
 FOREIGN KEY(evidence_id) REFERENCES preuves(id),
 FOREIGN KEY(ocr_run_id) REFERENCES identity_ocr_runs(id) ON DELETE SET NULL);
CREATE TABLE IF NOT EXISTS person_identification_assessments(
 id TEXT PRIMARY KEY,person_id TEXT NOT NULL,status_code TEXT NOT NULL,
 justification TEXT,assessed_at TEXT NOT NULL,origin TEXT NOT NULL CHECK(origin='human'),
 previous_assessment_id TEXT,CHECK(status_code<>'disputed' OR
 (justification IS NOT NULL AND length(trim(justification))>0)),
 FOREIGN KEY(person_id) REFERENCES entites(id),
 FOREIGN KEY(status_code) REFERENCES identification_status_vocabulary(code),
 FOREIGN KEY(previous_assessment_id) REFERENCES person_identification_assessments(id));

CREATE INDEX IF NOT EXISTS idx_authenticity_evidence_history
  ON document_authenticity_assessments(evidence_id,assessed_at,id);
CREATE INDEX IF NOT EXISTS idx_person_evidence_factual_person
  ON person_evidence_factual_relations(person_id,observed_at,id);
CREATE INDEX IF NOT EXISTS idx_person_evidence_factual_evidence
  ON person_evidence_factual_relations(evidence_id,observed_at,id);
CREATE TRIGGER IF NOT EXISTS authenticity_v18_consistency
BEFORE INSERT ON document_authenticity_assessments BEGIN
 SELECT CASE WHEN NEW.ocr_run_id IS NOT NULL AND NOT EXISTS(
 SELECT 1 FROM identity_ocr_runs WHERE id=NEW.ocr_run_id
 AND evidence_id=NEW.evidence_id)
 THEN RAISE(ABORT,'OCR run belongs to another evidence')
 WHEN NEW.previous_assessment_id IS NOT NULL AND NOT EXISTS(
 SELECT 1 FROM document_authenticity_assessments
 WHERE id=NEW.previous_assessment_id AND evidence_id=NEW.evidence_id)
 THEN RAISE(ABORT,'previous assessment belongs to another evidence') END;
END;
CREATE TRIGGER IF NOT EXISTS factual_relation_v18_consistency
BEFORE INSERT ON person_evidence_factual_relations BEGIN
 SELECT CASE WHEN NOT EXISTS(SELECT 1 FROM entites e JOIN types_entite t
 ON t.id=e.type_id WHERE e.id=NEW.person_id AND t.code='person')
 THEN RAISE(ABORT,'factual relation requires a person')
 WHEN NEW.ocr_run_id IS NOT NULL AND NOT EXISTS(SELECT 1 FROM identity_ocr_runs
 WHERE id=NEW.ocr_run_id AND evidence_id=NEW.evidence_id)
 THEN RAISE(ABORT,'OCR run belongs to another evidence') END;
END;
CREATE TRIGGER IF NOT EXISTS identity_fields_v18_insert_guard
BEFORE INSERT ON identity_field_observations BEGIN
 SELECT CASE
 WHEN NEW.review_status IN('rejected','conflict') AND NEW.confirmed_value IS NOT NULL
  THEN RAISE(ABORT,'non-projectable review status')
 WHEN NEW.value_quality IN('uncertain','invalid') AND NEW.confirmed_value IS NOT NULL
  THEN RAISE(ABORT,'non-projectable quality')
 WHEN NEW.confirmed_value IS NOT NULL AND NEW.confirmation_state<>'human_confirmed'
  THEN RAISE(ABORT,'human confirmation required')
 WHEN NEW.confirmation_state='human_confirmed' AND NEW.confirmed_value IS NULL
  THEN RAISE(ABORT,'confirmed value required') END;
END;
CREATE TRIGGER IF NOT EXISTS identity_fields_v18_update_guard
BEFORE UPDATE ON identity_field_observations BEGIN
 SELECT CASE
 WHEN NEW.review_status IN('rejected','conflict') AND NEW.confirmed_value IS NOT NULL
  THEN RAISE(ABORT,'non-projectable review status')
 WHEN NEW.value_quality IN('uncertain','invalid') AND NEW.confirmed_value IS NOT NULL
  THEN RAISE(ABORT,'non-projectable quality')
 WHEN NEW.confirmed_value IS NOT NULL AND NEW.confirmation_state<>'human_confirmed'
  THEN RAISE(ABORT,'human confirmation required')
 WHEN NEW.confirmation_state='human_confirmed' AND NEW.confirmed_value IS NULL
  THEN RAISE(ABORT,'confirmed value required') END;
END;

CREATE TABLE IF NOT EXISTS person_profile_fields(
 person_id TEXT NOT NULL,field_code TEXT NOT NULL,value TEXT NOT NULL,
 updated_at TEXT NOT NULL CHECK(length(updated_at)=20),
 PRIMARY KEY(person_id,field_code),
 CHECK(field_code IN('declared_name','surname','given_names','birth_date',
 'birth_place','nationality','sex_as_printed','address_as_printed')),
 CHECK(length(trim(value))>0),
 FOREIGN KEY(person_id) REFERENCES entites(id) ON DELETE CASCADE);
CREATE TABLE IF NOT EXISTS person_ocr_field_projections(
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
 FOREIGN KEY(ocr_field_id) REFERENCES identity_field_observations(id)
   ON DELETE RESTRICT);
CREATE INDEX IF NOT EXISTS idx_person_projection_person
 ON person_ocr_field_projections(person_id,projected_at,id);
CREATE TRIGGER IF NOT EXISTS projection_v19_consistency
BEFORE INSERT ON person_ocr_field_projections BEGIN
 SELECT CASE WHEN NOT EXISTS(SELECT 1 FROM identity_field_observations f
  WHERE f.id=NEW.ocr_field_id AND f.ocr_run_id=NEW.ocr_run_id
  AND f.evidence_id=NEW.evidence_id AND f.field_code=NEW.ocr_field_code
  AND f.confirmation_state='human_confirmed'
  AND length(trim(f.confirmed_value))>0
  AND f.review_status IN('accepted','modified')
  AND f.value_quality IN('complete','partial')
  AND f.confirmed_value=NEW.new_value)
 THEN RAISE(ABORT,'OCR field is no longer projectable') END;
END;

/* V20 — évaluations humaines distinctes de l’usage abusif d’identité. */
CREATE TABLE IF NOT EXISTS document_identity_misuse_assessments(
 id TEXT PRIMARY KEY,evidence_id TEXT NOT NULL,ocr_run_id TEXT,
 status TEXT NOT NULL CHECK(status IN('indeterminate','presumed','confirmed')),
 justification TEXT,assessed_at TEXT NOT NULL CHECK(length(assessed_at)=20),
 previous_assessment_id TEXT,origin TEXT NOT NULL CHECK(origin='human'),
 CHECK(status='indeterminate' OR length(trim(justification))>0),
 FOREIGN KEY(evidence_id) REFERENCES preuves(id) ON DELETE RESTRICT,
 FOREIGN KEY(ocr_run_id) REFERENCES identity_ocr_runs(id) ON DELETE RESTRICT,
 FOREIGN KEY(previous_assessment_id) REFERENCES document_identity_misuse_assessments(id) ON DELETE RESTRICT);
CREATE INDEX IF NOT EXISTS idx_identity_misuse_evidence_history
 ON document_identity_misuse_assessments(evidence_id,assessed_at,id);
CREATE UNIQUE INDEX IF NOT EXISTS idx_identity_misuse_previous
 ON document_identity_misuse_assessments(previous_assessment_id)
 WHERE previous_assessment_id IS NOT NULL;
CREATE TRIGGER IF NOT EXISTS identity_misuse_consistency BEFORE INSERT ON document_identity_misuse_assessments BEGIN
 SELECT CASE WHEN NEW.ocr_run_id IS NOT NULL AND NOT EXISTS(SELECT 1 FROM identity_ocr_runs r WHERE r.id=NEW.ocr_run_id AND r.evidence_id=NEW.evidence_id) THEN RAISE(ABORT,'OCR run does not belong to evidence') END;
 SELECT CASE WHEN NEW.previous_assessment_id IS NOT NULL AND NOT EXISTS(SELECT 1 FROM document_identity_misuse_assessments p WHERE p.id=NEW.previous_assessment_id AND p.evidence_id=NEW.evidence_id) THEN RAISE(ABORT,'previous assessment does not belong to evidence') END;
 SELECT CASE WHEN EXISTS(SELECT 1 FROM document_identity_misuse_assessments p WHERE p.evidence_id=NEW.evidence_id) AND NEW.previous_assessment_id IS NULL THEN RAISE(ABORT,'previous assessment is required') END;
 SELECT CASE WHEN NEW.previous_assessment_id IS NOT NULL AND EXISTS(SELECT 1 FROM document_identity_misuse_assessments n WHERE n.previous_assessment_id=NEW.previous_assessment_id) THEN RAISE(ABORT,'previous assessment is not current') END;
END;
CREATE TRIGGER IF NOT EXISTS identity_misuse_append_only_update BEFORE UPDATE ON document_identity_misuse_assessments BEGIN SELECT RAISE(ABORT,'identity misuse history is append-only'); END;
CREATE TRIGGER IF NOT EXISTS identity_misuse_append_only_delete BEFORE DELETE ON document_identity_misuse_assessments BEGIN SELECT RAISE(ABORT,'identity misuse history is append-only'); END;

/* V21 — pivot financier sourcé et append-only. */
INSERT INTO metadata(key,value) VALUES('schema_version','21');

CREATE TABLE bank_analyses(
 id TEXT PRIMARY KEY, investigation_id TEXT NOT NULL, evidence_id TEXT NOT NULL,
 evidence_sha256 TEXT NOT NULL, extraction_method TEXT NOT NULL,
 source_kind TEXT NOT NULL, created_at TEXT NOT NULL, origin TEXT NOT NULL,
 CHECK(length(id) BETWEEN 1 AND 128), CHECK(length(evidence_sha256)=64 AND evidence_sha256=lower(evidence_sha256)),
 CHECK(length(extraction_method) BETWEEN 1 AND 128),
 CHECK(source_kind IN('manual_text','eml_text','pdf_text','image_ocr','pdf_ocr')),
 CHECK(origin IN('automatic','human','legacy')), CHECK(length(created_at)=20 AND substr(created_at,20,1)='Z'),
 FOREIGN KEY(investigation_id) REFERENCES investigation(id) ON DELETE RESTRICT,
 FOREIGN KEY(evidence_id) REFERENCES preuves(id) ON DELETE RESTRICT);

CREATE TABLE bank_observations(
 id TEXT PRIMARY KEY, analysis_id TEXT NOT NULL, investigation_id TEXT NOT NULL,
 evidence_id TEXT NOT NULL, evidence_sha256 TEXT NOT NULL, occurrence_type TEXT NOT NULL,
 raw_value TEXT NOT NULL, start_offset INTEGER NOT NULL, end_offset INTEGER NOT NULL,
 extraction_rule TEXT NOT NULL, technical_validation TEXT NOT NULL,
 page_or_image TEXT, ocr_run_id TEXT, region_x TEXT, region_y TEXT,
 region_width TEXT, region_height TEXT, source_context TEXT, origin TEXT NOT NULL,
 created_at TEXT NOT NULL, UNIQUE(id,investigation_id),
 CHECK(length(id) BETWEEN 1 AND 128), CHECK(length(evidence_sha256)=64 AND evidence_sha256=lower(evidence_sha256)),
 CHECK(length(raw_value) BETWEEN 1 AND 512), CHECK(start_offset>=0 AND end_offset>=start_offset),
 CHECK(end_offset-start_offset=length(CAST(raw_value AS BLOB))),
 CHECK(length(extraction_rule) BETWEEN 1 AND 128),
 CHECK(technical_validation IN('valid','invalid','ambiguous','unverifiable')),
 CHECK(origin IN('automatic','human','legacy')), CHECK(source_context IS NULL OR length(CAST(source_context AS BLOB))<=1024),
 CHECK(length(created_at)=20 AND substr(created_at,20,1)='Z'),
 CHECK((region_x IS NULL AND region_y IS NULL AND region_width IS NULL AND region_height IS NULL) OR
       (region_x IS NOT NULL AND region_y IS NOT NULL AND region_width IS NOT NULL AND region_height IS NOT NULL)),
 FOREIGN KEY(analysis_id) REFERENCES bank_analyses(id) ON DELETE RESTRICT,
 FOREIGN KEY(investigation_id) REFERENCES investigation(id) ON DELETE RESTRICT,
 FOREIGN KEY(evidence_id) REFERENCES preuves(id) ON DELETE RESTRICT,
 FOREIGN KEY(ocr_run_id) REFERENCES identity_ocr_runs(id) ON DELETE RESTRICT);
CREATE INDEX idx_bank_observations_investigation ON bank_observations(investigation_id,created_at,id);
CREATE INDEX idx_bank_observations_evidence ON bank_observations(evidence_id,created_at,id);
CREATE INDEX idx_bank_observations_analysis ON bank_observations(analysis_id,start_offset,id);

CREATE TABLE bank_observation_warnings(
 observation_id TEXT NOT NULL, position INTEGER NOT NULL, warning TEXT NOT NULL,
 PRIMARY KEY(observation_id,position), CHECK(position>=0), CHECK(length(warning) BETWEEN 1 AND 256),
 FOREIGN KEY(observation_id) REFERENCES bank_observations(id) ON DELETE RESTRICT);

CREATE TABLE bank_proposals(
 id TEXT PRIMARY KEY, investigation_id TEXT NOT NULL, evidence_id TEXT NOT NULL,
 start_offset INTEGER NOT NULL, end_offset INTEGER NOT NULL, ambiguous INTEGER NOT NULL,
 origin TEXT NOT NULL, created_at TEXT NOT NULL, UNIQUE(id,investigation_id),
 CHECK(length(id) BETWEEN 1 AND 128), CHECK(start_offset>=0 AND end_offset>=start_offset),
 CHECK(ambiguous IN(0,1)), CHECK(origin IN('automatic','human')),
 CHECK(length(created_at)=20 AND substr(created_at,20,1)='Z'),
 FOREIGN KEY(investigation_id) REFERENCES investigation(id) ON DELETE RESTRICT,
 FOREIGN KEY(evidence_id) REFERENCES preuves(id) ON DELETE RESTRICT);
CREATE INDEX idx_bank_proposals_evidence ON bank_proposals(evidence_id,created_at,id);
CREATE TABLE bank_proposal_observations(
 proposal_id TEXT NOT NULL, observation_id TEXT NOT NULL, position INTEGER NOT NULL,
 role TEXT, PRIMARY KEY(proposal_id,observation_id), UNIQUE(proposal_id,position),
 CHECK(position>=0), CHECK(role IS NULL OR length(role) BETWEEN 1 AND 64),
 FOREIGN KEY(proposal_id) REFERENCES bank_proposals(id) ON DELETE RESTRICT,
 FOREIGN KEY(observation_id) REFERENCES bank_observations(id) ON DELETE RESTRICT);
CREATE INDEX idx_bank_proposal_observation ON bank_proposal_observations(observation_id);
CREATE TABLE bank_proposal_reasons(
 proposal_id TEXT NOT NULL, position INTEGER NOT NULL, reason TEXT NOT NULL,
 PRIMARY KEY(proposal_id,position), CHECK(position>=0), CHECK(length(reason) BETWEEN 1 AND 256),
 FOREIGN KEY(proposal_id) REFERENCES bank_proposals(id) ON DELETE RESTRICT);

CREATE TABLE bank_observation_revisions(
 id TEXT PRIMARY KEY, observation_id TEXT NOT NULL, corrected_value TEXT,
 normalized_value TEXT, technical_validation TEXT NOT NULL, factual_notes TEXT,
 author TEXT, origin TEXT NOT NULL CHECK(origin='human'), created_at TEXT NOT NULL,
 CHECK(length(id) BETWEEN 1 AND 128), CHECK(corrected_value IS NULL OR length(corrected_value)<=512),
 CHECK(normalized_value IS NULL OR length(normalized_value)<=512),
 CHECK(technical_validation IN('valid','invalid','ambiguous','unverifiable')),
 CHECK(factual_notes IS NULL OR length(factual_notes)<=4096),
 CHECK(length(created_at)=20 AND substr(created_at,20,1)='Z'),
 FOREIGN KEY(observation_id) REFERENCES bank_observations(id) ON DELETE RESTRICT);
CREATE INDEX idx_bank_revisions_observation ON bank_observation_revisions(observation_id,created_at,id);

CREATE TABLE bank_decisions(
 id TEXT PRIMARY KEY, observation_id TEXT, proposal_id TEXT, revision_id TEXT,
 decision TEXT NOT NULL, origin TEXT NOT NULL CHECK(origin='human'), created_at TEXT NOT NULL,
 create_account INTEGER NOT NULL DEFAULT 0, reuse_account INTEGER NOT NULL DEFAULT 0,
 create_holder INTEGER NOT NULL DEFAULT 0, create_transaction INTEGER NOT NULL DEFAULT 0,
 CHECK(length(id) BETWEEN 1 AND 128), CHECK((observation_id IS NULL)!=(proposal_id IS NULL)),
 CHECK(decision IN('proposed','confirmed','rejected')),
 CHECK(create_account IN(0,1) AND reuse_account IN(0,1) AND create_holder IN(0,1) AND create_transaction IN(0,1)),
 CHECK(length(created_at)=20 AND substr(created_at,20,1)='Z'),
 FOREIGN KEY(observation_id) REFERENCES bank_observations(id) ON DELETE RESTRICT,
 FOREIGN KEY(proposal_id) REFERENCES bank_proposals(id) ON DELETE RESTRICT,
 FOREIGN KEY(revision_id) REFERENCES bank_observation_revisions(id) ON DELETE RESTRICT);
CREATE INDEX idx_bank_decisions_observation ON bank_decisions(observation_id,created_at,id);
CREATE INDEX idx_bank_decisions_proposal ON bank_decisions(proposal_id,created_at,id);

CREATE TABLE bank_accounts(
 entity_id TEXT PRIMARY KEY, investigation_id TEXT NOT NULL, iban_normalized TEXT NOT NULL,
 country_code TEXT NOT NULL, bank_code TEXT, branch_code TEXT, account_number TEXT, rib_key TEXT,
 bic TEXT, declared_institution TEXT, account_state TEXT NOT NULL,
 first_observed_at TEXT NOT NULL, last_observed_at TEXT NOT NULL, factual_notes TEXT,
 UNIQUE(investigation_id,iban_normalized), UNIQUE(entity_id,investigation_id),
 CHECK(length(iban_normalized) BETWEEN 15 AND 34 AND iban_normalized=upper(iban_normalized)),
 CHECK(length(country_code)=2 AND country_code=upper(country_code)),
 CHECK(substr(iban_normalized,1,2)=country_code),
 CHECK(bic IS NULL OR length(bic) IN(8,11)), CHECK(account_state IN('unknown','active','closed')),
 CHECK(length(first_observed_at)=20 AND substr(first_observed_at,20,1)='Z'),
 CHECK(length(last_observed_at)=20 AND substr(last_observed_at,20,1)='Z'),
 CHECK(factual_notes IS NULL OR length(factual_notes)<=4096),
 FOREIGN KEY(entity_id) REFERENCES entites(id) ON DELETE RESTRICT,
 FOREIGN KEY(investigation_id) REFERENCES investigation(id) ON DELETE RESTRICT);
CREATE INDEX idx_bank_accounts_iban ON bank_accounts(iban_normalized);
CREATE INDEX idx_bank_accounts_investigation ON bank_accounts(investigation_id,entity_id);

CREATE TABLE declared_bank_holders(
 id TEXT PRIMARY KEY, investigation_id TEXT NOT NULL, evidence_id TEXT NOT NULL,
 observation_id TEXT, revision_id TEXT, account_entity_id TEXT, person_entity_id TEXT,
 decision_id TEXT, declared_name_raw TEXT NOT NULL, declared_name_corrected TEXT,
 origin TEXT NOT NULL, created_at TEXT NOT NULL, factual_notes TEXT,
 CHECK(length(id) BETWEEN 1 AND 128), CHECK(length(declared_name_raw) BETWEEN 1 AND 512),
 CHECK(declared_name_corrected IS NULL OR length(declared_name_corrected)<=512),
 CHECK(origin IN('human','legacy')), CHECK(length(created_at)=20 AND substr(created_at,20,1)='Z'),
 FOREIGN KEY(investigation_id) REFERENCES investigation(id) ON DELETE RESTRICT,
 FOREIGN KEY(evidence_id) REFERENCES preuves(id) ON DELETE RESTRICT,
 FOREIGN KEY(observation_id) REFERENCES bank_observations(id) ON DELETE RESTRICT,
 FOREIGN KEY(revision_id) REFERENCES bank_observation_revisions(id) ON DELETE RESTRICT,
 FOREIGN KEY(account_entity_id) REFERENCES bank_accounts(entity_id) ON DELETE RESTRICT,
 FOREIGN KEY(person_entity_id) REFERENCES entites(id) ON DELETE RESTRICT,
 FOREIGN KEY(decision_id) REFERENCES bank_decisions(id) ON DELETE RESTRICT);
CREATE INDEX idx_declared_holders_evidence ON declared_bank_holders(evidence_id,created_at,id);
CREATE INDEX idx_declared_holders_account ON declared_bank_holders(account_entity_id,created_at,id);
CREATE INDEX idx_declared_holders_person ON declared_bank_holders(person_entity_id,created_at,id);

CREATE TABLE financial_transactions(
 id TEXT PRIMARY KEY, investigation_id TEXT NOT NULL, transaction_type TEXT NOT NULL,
 observed_date_raw TEXT, observed_time_raw TEXT, observed_at_normalized TEXT,
 amount_decimal TEXT NOT NULL, currency TEXT, observed_status TEXT,
 bank_reference TEXT, reference_type TEXT, uetr TEXT,
 debtor_account_id TEXT, creditor_account_id TEXT,
 declared_payer TEXT, declared_beneficiary TEXT, source_institution TEXT,
 evidence_id TEXT NOT NULL, source_observation_id TEXT, source_proposal_id TEXT,
 decision_id TEXT NOT NULL, origin TEXT NOT NULL CHECK(origin='human'), factual_notes TEXT,
 created_at TEXT NOT NULL, UNIQUE(id,investigation_id),
 CHECK(length(id) BETWEEN 1 AND 128), CHECK(length(transaction_type) BETWEEN 1 AND 64),
 CHECK(length(amount_decimal) BETWEEN 1 AND 128),
 CHECK(amount_decimal NOT GLOB '*[^0-9.]*' AND amount_decimal NOT LIKE '.%' AND amount_decimal NOT LIKE '%.'),
 CHECK(length(amount_decimal)-length(replace(amount_decimal,'.',''))<=1),
 CHECK(currency IS NULL OR (length(currency)=3 AND currency=upper(currency))),
 CHECK(uetr IS NULL OR (length(uetr)=36 AND substr(uetr,9,1)='-' AND substr(uetr,14,1)='-' AND substr(uetr,19,1)='-' AND substr(uetr,24,1)='-' AND replace(lower(uetr),'-','') NOT GLOB '*[^0-9a-f]*')),
 CHECK(length(created_at)=20 AND substr(created_at,20,1)='Z'),
 FOREIGN KEY(investigation_id) REFERENCES investigation(id) ON DELETE RESTRICT,
 FOREIGN KEY(evidence_id) REFERENCES preuves(id) ON DELETE RESTRICT,
 FOREIGN KEY(source_observation_id) REFERENCES bank_observations(id) ON DELETE RESTRICT,
 FOREIGN KEY(source_proposal_id) REFERENCES bank_proposals(id) ON DELETE RESTRICT,
 FOREIGN KEY(decision_id) REFERENCES bank_decisions(id) ON DELETE RESTRICT,
 FOREIGN KEY(debtor_account_id) REFERENCES bank_accounts(entity_id) ON DELETE RESTRICT,
 FOREIGN KEY(creditor_account_id) REFERENCES bank_accounts(entity_id) ON DELETE RESTRICT);
CREATE INDEX idx_financial_transactions_investigation ON financial_transactions(investigation_id,created_at,id);
CREATE INDEX idx_financial_transactions_evidence ON financial_transactions(evidence_id,created_at,id);
CREATE INDEX idx_financial_transactions_debtor ON financial_transactions(debtor_account_id,created_at,id);
CREATE INDEX idx_financial_transactions_creditor ON financial_transactions(creditor_account_id,created_at,id);

CREATE TABLE bank_account_observation_sources(
 account_entity_id TEXT NOT NULL, observation_id TEXT NOT NULL, decision_id TEXT NOT NULL,
 use_kind TEXT NOT NULL CHECK(use_kind IN('created','reused')), created_at TEXT NOT NULL,
 PRIMARY KEY(account_entity_id,observation_id,decision_id),
 CHECK(length(created_at)=20 AND substr(created_at,20,1)='Z'),
 FOREIGN KEY(account_entity_id) REFERENCES bank_accounts(entity_id) ON DELETE RESTRICT,
 FOREIGN KEY(observation_id) REFERENCES bank_observations(id) ON DELETE RESTRICT,
 FOREIGN KEY(decision_id) REFERENCES bank_decisions(id) ON DELETE RESTRICT);
CREATE INDEX idx_account_sources_observation ON bank_account_observation_sources(observation_id);
CREATE TABLE financial_transaction_observation_sources(
 transaction_id TEXT NOT NULL, observation_id TEXT NOT NULL, position INTEGER NOT NULL,
 PRIMARY KEY(transaction_id,observation_id), UNIQUE(transaction_id,position), CHECK(position>=0),
 FOREIGN KEY(transaction_id) REFERENCES financial_transactions(id) ON DELETE RESTRICT,
 FOREIGN KEY(observation_id) REFERENCES bank_observations(id) ON DELETE RESTRICT);
CREATE INDEX idx_transaction_sources_observation ON financial_transaction_observation_sources(observation_id);
CREATE TABLE financial_transaction_proposal_sources(
 transaction_id TEXT NOT NULL, proposal_id TEXT NOT NULL, position INTEGER NOT NULL,
 PRIMARY KEY(transaction_id,proposal_id), UNIQUE(transaction_id,position), CHECK(position>=0),
 FOREIGN KEY(transaction_id) REFERENCES financial_transactions(id) ON DELETE RESTRICT,
 FOREIGN KEY(proposal_id) REFERENCES bank_proposals(id) ON DELETE RESTRICT);

/* Cohérence transversale, aucune association inter-preuve/enquête implicite. */
CREATE TRIGGER bank_observation_consistency BEFORE INSERT ON bank_observations BEGIN
 SELECT CASE WHEN NOT EXISTS(SELECT 1 FROM bank_analyses a WHERE a.id=NEW.analysis_id AND a.investigation_id=NEW.investigation_id AND a.evidence_id=NEW.evidence_id AND a.evidence_sha256=NEW.evidence_sha256) THEN RAISE(ABORT,'bank observation provenance mismatch') END;
 SELECT CASE WHEN NEW.ocr_run_id IS NOT NULL AND NOT EXISTS(SELECT 1 FROM identity_ocr_runs r WHERE r.id=NEW.ocr_run_id AND r.evidence_id=NEW.evidence_id) THEN RAISE(ABORT,'OCR run does not belong to evidence') END;
END;
CREATE TRIGGER bank_analysis_current_investigation BEFORE INSERT ON bank_analyses BEGIN
 SELECT CASE WHEN NOT EXISTS(SELECT 1 FROM metadata WHERE key='investigation_uuid' AND value=NEW.investigation_id) THEN RAISE(ABORT,'analysis investigation mismatch') END;
END;
CREATE TRIGGER bank_proposal_observation_consistency BEFORE INSERT ON bank_proposal_observations BEGIN
 SELECT CASE WHEN NOT EXISTS(SELECT 1 FROM bank_proposals p JOIN bank_observations o ON o.id=NEW.observation_id WHERE p.id=NEW.proposal_id AND p.investigation_id=o.investigation_id AND p.evidence_id=o.evidence_id) THEN RAISE(ABORT,'proposal observation mismatch') END;
END;
CREATE TRIGGER bank_proposal_current_investigation BEFORE INSERT ON bank_proposals BEGIN
 SELECT CASE WHEN NOT EXISTS(SELECT 1 FROM metadata WHERE key='investigation_uuid' AND value=NEW.investigation_id) THEN RAISE(ABORT,'proposal investigation mismatch') END;
END;
CREATE TRIGGER bank_revision_human BEFORE INSERT ON bank_observation_revisions WHEN NEW.origin<>'human' BEGIN SELECT RAISE(ABORT,'bank revision must be human'); END;
CREATE TRIGGER bank_decision_consistency BEFORE INSERT ON bank_decisions BEGIN
 SELECT CASE WHEN NEW.revision_id IS NOT NULL AND NEW.observation_id IS NULL THEN RAISE(ABORT,'revision requires observation decision') END;
 SELECT CASE WHEN NEW.revision_id IS NOT NULL AND NOT EXISTS(SELECT 1 FROM bank_observation_revisions r WHERE r.id=NEW.revision_id AND r.observation_id=NEW.observation_id) THEN RAISE(ABORT,'revision does not belong to observation') END;
 SELECT CASE WHEN NEW.decision='confirmed' AND NEW.origin<>'human' THEN RAISE(ABORT,'confirmed decision must be human') END;
END;
CREATE TRIGGER bank_account_entity_type BEFORE INSERT ON bank_accounts BEGIN
 SELECT CASE WHEN NOT EXISTS(SELECT 1 FROM entites e JOIN types_entite t ON t.id=e.type_id WHERE e.id=NEW.entity_id AND t.code='bank_account') THEN RAISE(ABORT,'canonical bank account entity required') END;
 SELECT CASE WHEN NOT EXISTS(SELECT 1 FROM metadata WHERE key='investigation_uuid' AND value=NEW.investigation_id) THEN RAISE(ABORT,'investigation mismatch') END;
END;
CREATE TRIGGER declared_holder_consistency BEFORE INSERT ON declared_bank_holders BEGIN
 SELECT CASE WHEN NOT EXISTS(SELECT 1 FROM metadata WHERE key='investigation_uuid' AND value=NEW.investigation_id) THEN RAISE(ABORT,'holder investigation mismatch') END;
 SELECT CASE WHEN NEW.observation_id IS NOT NULL AND NOT EXISTS(SELECT 1 FROM bank_observations o WHERE o.id=NEW.observation_id AND o.investigation_id=NEW.investigation_id AND o.evidence_id=NEW.evidence_id) THEN RAISE(ABORT,'holder observation mismatch') END;
 SELECT CASE WHEN NEW.revision_id IS NOT NULL AND NOT EXISTS(SELECT 1 FROM bank_observation_revisions r WHERE r.id=NEW.revision_id AND r.observation_id=NEW.observation_id) THEN RAISE(ABORT,'holder revision mismatch') END;
 SELECT CASE WHEN NEW.decision_id IS NOT NULL AND NOT EXISTS(SELECT 1 FROM bank_decisions d WHERE d.id=NEW.decision_id AND d.origin='human' AND (d.observation_id=NEW.observation_id OR d.proposal_id IS NOT NULL)) THEN RAISE(ABORT,'holder decision mismatch') END;
 SELECT CASE WHEN NEW.person_entity_id IS NOT NULL AND NOT EXISTS(SELECT 1 FROM entites e JOIN types_entite t ON t.id=e.type_id WHERE e.id=NEW.person_entity_id AND t.code='person') THEN RAISE(ABORT,'explicit person entity required') END;
END;
CREATE TRIGGER financial_transaction_consistency BEFORE INSERT ON financial_transactions BEGIN
 SELECT CASE WHEN NOT EXISTS(SELECT 1 FROM metadata WHERE key='investigation_uuid' AND value=NEW.investigation_id) THEN RAISE(ABORT,'transaction investigation mismatch') END;
 SELECT CASE WHEN NOT EXISTS(SELECT 1 FROM bank_decisions d WHERE d.id=NEW.decision_id AND d.decision='confirmed' AND d.origin='human') THEN RAISE(ABORT,'confirmed human decision required') END;
 SELECT CASE WHEN NOT EXISTS(SELECT 1 FROM bank_decisions d WHERE d.id=NEW.decision_id AND ((NEW.source_observation_id IS NOT NULL AND d.observation_id=NEW.source_observation_id) OR (NEW.source_proposal_id IS NOT NULL AND d.proposal_id=NEW.source_proposal_id))) THEN RAISE(ABORT,'transaction decision mismatch') END;
 SELECT CASE WHEN NEW.source_observation_id IS NOT NULL AND NOT EXISTS(SELECT 1 FROM bank_observations o WHERE o.id=NEW.source_observation_id AND o.investigation_id=NEW.investigation_id AND o.evidence_id=NEW.evidence_id) THEN RAISE(ABORT,'transaction observation mismatch') END;
 SELECT CASE WHEN NEW.source_proposal_id IS NOT NULL AND NOT EXISTS(SELECT 1 FROM bank_proposals p WHERE p.id=NEW.source_proposal_id AND p.investigation_id=NEW.investigation_id AND p.evidence_id=NEW.evidence_id) THEN RAISE(ABORT,'transaction proposal mismatch') END;
END;
CREATE TRIGGER bank_account_source_consistency BEFORE INSERT ON bank_account_observation_sources BEGIN
 SELECT CASE WHEN NOT EXISTS(SELECT 1 FROM bank_accounts a JOIN bank_observations o ON o.id=NEW.observation_id JOIN bank_decisions d ON d.id=NEW.decision_id WHERE a.entity_id=NEW.account_entity_id AND a.investigation_id=o.investigation_id AND d.decision='confirmed' AND d.origin='human' AND d.observation_id=o.id) THEN RAISE(ABORT,'account provenance mismatch') END;
END;

/* Historiques V21 strictement append-only. */
CREATE TRIGGER bank_observations_no_update BEFORE UPDATE ON bank_observations BEGIN SELECT RAISE(ABORT,'bank observations are append-only'); END;
CREATE TRIGGER bank_observations_no_delete BEFORE DELETE ON bank_observations BEGIN SELECT RAISE(ABORT,'bank observations are append-only'); END;
CREATE TRIGGER bank_proposals_no_update BEFORE UPDATE ON bank_proposals BEGIN SELECT RAISE(ABORT,'bank proposals are append-only'); END;
CREATE TRIGGER bank_proposals_no_delete BEFORE DELETE ON bank_proposals BEGIN SELECT RAISE(ABORT,'bank proposals are append-only'); END;
CREATE TRIGGER bank_proposal_observations_no_update BEFORE UPDATE ON bank_proposal_observations BEGIN SELECT RAISE(ABORT,'bank proposal membership is append-only'); END;
CREATE TRIGGER bank_proposal_observations_no_delete BEFORE DELETE ON bank_proposal_observations BEGIN SELECT RAISE(ABORT,'bank proposal membership is append-only'); END;
CREATE TRIGGER bank_revisions_no_update BEFORE UPDATE ON bank_observation_revisions BEGIN SELECT RAISE(ABORT,'bank revisions are append-only'); END;
CREATE TRIGGER bank_revisions_no_delete BEFORE DELETE ON bank_observation_revisions BEGIN SELECT RAISE(ABORT,'bank revisions are append-only'); END;
CREATE TRIGGER bank_decisions_no_update BEFORE UPDATE ON bank_decisions BEGIN SELECT RAISE(ABORT,'bank decisions are append-only'); END;
CREATE TRIGGER bank_decisions_no_delete BEFORE DELETE ON bank_decisions BEGIN SELECT RAISE(ABORT,'bank decisions are append-only'); END;
CREATE TRIGGER bank_account_sources_no_update BEFORE UPDATE ON bank_account_observation_sources BEGIN SELECT RAISE(ABORT,'bank account provenance is append-only'); END;
CREATE TRIGGER bank_account_sources_no_delete BEFORE DELETE ON bank_account_observation_sources BEGIN SELECT RAISE(ABORT,'bank account provenance is append-only'); END;
CREATE TRIGGER transaction_observation_sources_no_update BEFORE UPDATE ON financial_transaction_observation_sources BEGIN SELECT RAISE(ABORT,'transaction provenance is append-only'); END;
CREATE TRIGGER transaction_observation_sources_no_delete BEFORE DELETE ON financial_transaction_observation_sources BEGIN SELECT RAISE(ABORT,'transaction provenance is append-only'); END;
