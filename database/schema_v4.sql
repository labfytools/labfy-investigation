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
