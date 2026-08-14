-- 032_collaboration.sql
-- Gap-Fill TraceLink 3.2: real-time multi-user collaboration.
-- Idempotent (IF NOT EXISTS). Never edit after it is applied.
--
-- `collab_operation_log` - an append-only log of every collaboration operation
-- (create/update/delete on an entity), with a per-entity version. The version
-- is the operation counter for that entity (used as a version vector element).
--
-- `collab_vector` - one element of an entity's version vector: the version
-- number contributed by a given actor/session. Merge uses these to detect
-- concurrent edits.

CREATE TABLE IF NOT EXISTS collab_operation_log (
    id          TEXT PRIMARY KEY,             -- UUID
    entity_type TEXT NOT NULL,
    entity_id   TEXT NOT NULL,
    op          TEXT NOT NULL,                -- create | update | delete
    actor       TEXT NOT NULL DEFAULT '',
    version     INTEGER NOT NULL DEFAULT 0,   -- per-entity operation counter
    payload     TEXT NOT NULL DEFAULT '',     -- JSON snapshot/change
    created_at  TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS idx_collab_op_entity
    ON collab_operation_log(entity_id, version);

CREATE TABLE IF NOT EXISTS collab_vector (
    id          TEXT PRIMARY KEY,             -- UUID
    entity_id   TEXT NOT NULL,
    actor       TEXT NOT NULL,
    version     INTEGER NOT NULL DEFAULT 0,   -- highest version actor has seen
    UNIQUE(entity_id, actor)
);
CREATE INDEX IF NOT EXISTS idx_collab_vector_entity ON collab_vector(entity_id);
