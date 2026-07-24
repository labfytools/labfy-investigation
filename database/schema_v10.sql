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
