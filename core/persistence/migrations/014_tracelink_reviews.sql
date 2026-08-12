-- 014_tracelink_reviews.sql
-- WP-2 (Phase 10): general artifact review / comment / approval.
--
-- Any artifact (requirement, design, interface, test_case, hazard, decision,
-- assumption, ...) can carry general review comments and an approval verdict,
-- beyond the change-request workflow (migration 010).
--
-- Append-only: never edit this file after it is applied.

CREATE TABLE IF NOT EXISTS comments (
    id          TEXT PRIMARY KEY,             -- UUID
    entity_type TEXT NOT NULL DEFAULT '',
    entity_id   TEXT NOT NULL DEFAULT '',
    author      TEXT NOT NULL DEFAULT '',
    body        TEXT NOT NULL DEFAULT '',
    created_at  TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS idx_comments_entity ON comments(entity_type, entity_id);

CREATE TABLE IF NOT EXISTS reviews (
    id          TEXT PRIMARY KEY,             -- UUID
    entity_type TEXT NOT NULL DEFAULT '',
    entity_id   TEXT NOT NULL DEFAULT '',
    reviewer    TEXT NOT NULL DEFAULT '',
    verdict     TEXT NOT NULL DEFAULT '',    -- Approve|Reject|RequestChanges
    comment     TEXT NOT NULL DEFAULT '',
    created_at  TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS idx_reviews_entity ON reviews(entity_type, entity_id);
