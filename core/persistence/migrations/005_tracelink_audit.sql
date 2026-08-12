-- 005_tracelink_audit.sql
-- Append-only audit trail (WP-4, plan schema 3.3).
-- Every mutation to an entity or a trace link writes one or more rows here,
-- in the SAME transaction as the change, so no audit entry is ever lost.
-- Append-only: never edit this file after it is applied.

CREATE TABLE IF NOT EXISTS audit_log (
    id                TEXT PRIMARY KEY,             -- UUID
    entity_type       TEXT NOT NULL DEFAULT '',     -- requirement|design|interface|test_case|hazard|decision|assumption|link
    entity_id         TEXT NOT NULL DEFAULT '',     -- entity UUID, or link UUID for link ops
    action            TEXT NOT NULL DEFAULT '',     -- create|update|soft_delete|add_link|update_link|remove_link
    field             TEXT NOT NULL DEFAULT '',     -- the changed field (updates only)
    old_value         TEXT NOT NULL DEFAULT '',
    new_value         TEXT NOT NULL DEFAULT '',
    actor             TEXT NOT NULL DEFAULT '',
    timestamp         TEXT NOT NULL DEFAULT '',
    change_request_id TEXT NOT NULL DEFAULT ''      -- optional CR tag for change impact
);

-- Lookup / filter indexes.
CREATE INDEX IF NOT EXISTS idx_audit_entity    ON audit_log(entity_type, entity_id);
CREATE INDEX IF NOT EXISTS idx_audit_timestamp ON audit_log(timestamp);
CREATE INDEX IF NOT EXISTS idx_audit_cr        ON audit_log(change_request_id);
