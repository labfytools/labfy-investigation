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
