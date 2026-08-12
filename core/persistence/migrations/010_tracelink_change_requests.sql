-- 010_tracelink_change_requests.sql
-- WP-B (A4): change-request + review workflow.
--
-- A change request (CR) proposes a set of field changes to a target entity.
-- It flows through a review lifecycle (Open -> InReview -> Approved/Rejected
-- -> Implemented) and, once approved, its proposed change is applied to the
-- entity with every audit row stamped with the CR id (linking CRs to audit).
--
-- Append-only: never edit this file after it is applied.

CREATE TABLE IF NOT EXISTS change_requests (
    id               TEXT PRIMARY KEY,             -- UUID
    title            TEXT NOT NULL DEFAULT '',
    description      TEXT NOT NULL DEFAULT '',
    status           TEXT NOT NULL DEFAULT 'Open', -- Open|InReview|Approved|Rejected|Implemented
    entity_type      TEXT NOT NULL DEFAULT '',     -- requirement|design|interface|test_case|hazard|decision|assumption
    entity_id        TEXT NOT NULL DEFAULT '',     -- target entity UUID
    proposed_change  TEXT NOT NULL DEFAULT '{}',   -- JSON of proposed field changes, e.g. {"name":"X"}
    created_by       TEXT NOT NULL DEFAULT '',
    created_at       TEXT NOT NULL DEFAULT '',
    reviewed_by      TEXT NOT NULL DEFAULT '',
    reviewed_at      TEXT NOT NULL DEFAULT '',
    review_comment   TEXT NOT NULL DEFAULT ''
);

-- Review-queue lookup (Open/InReview) and per-entity CR lookup.
CREATE INDEX IF NOT EXISTS idx_cr_status ON change_requests(status);
CREATE INDEX IF NOT EXISTS idx_cr_entity ON change_requests(entity_type, entity_id);
