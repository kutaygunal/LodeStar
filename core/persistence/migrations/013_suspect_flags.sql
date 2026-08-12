-- 013_suspect_flags.sql
-- Phase 10 WP-1 (A1): suspect-link workflow.
--
-- A downstream artifact is flagged `suspect` when an upstream requirement
-- changes. The `suspect_flags` table records one active (uncleared) flag per
-- affected artifact, together with the changed upstream source that triggered
-- it. `cleared_at`/`cleared_by` are empty while the flag is active; clearing a
-- flag stamps them so the flag leaves the review queue.
--
-- Append-only and idempotent: never edit this file after it is applied.

CREATE TABLE IF NOT EXISTS suspect_flags (
    id                 TEXT PRIMARY KEY,             -- UUID
    entity_type        TEXT NOT NULL,                -- requirement|design|test_case|...
    entity_id          TEXT NOT NULL,                -- flagged artifact UUID
    reason             TEXT NOT NULL DEFAULT '',
    source_type        TEXT NOT NULL DEFAULT '',     -- the changed upstream entity type
    source_id          TEXT NOT NULL DEFAULT '',     -- the changed upstream entity UUID
    created_at         TEXT NOT NULL DEFAULT '',
    cleared_at         TEXT NOT NULL DEFAULT '',     -- '' while active
    cleared_by         TEXT NOT NULL DEFAULT ''
);

-- Fast lookup of active flags by artifact (the hot path for isSuspect and the
-- review queue).
CREATE INDEX IF NOT EXISTS idx_suspect_active ON suspect_flags(entity_type, entity_id);
