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
