/******************************************************************************
 * Labfy Investigation
 *
 * Extensions idempotentes du schéma SQLite courant V8
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
