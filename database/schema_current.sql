/******************************************************************************
 * Labfy Investigation
 *
 * Extensions idempotentes du schéma SQLite courant V2
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
