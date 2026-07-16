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

