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
