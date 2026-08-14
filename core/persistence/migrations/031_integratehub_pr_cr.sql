-- 031_integratehub_pr_cr.sql
-- Gap-Fill IntegrateHub 6.1: Problem Report -> Change Request -> impact analysis.
-- Idempotent (IF NOT EXISTS). Never edit after it is applied.
--
-- `integratehub_pr` - a formal problem report with fields, states and approval
-- authority. status: open | under_investigation | resolved | closed.
--
-- `integratehub_cr` - a change request linked to a PR (nullable) and to a
-- target entity (TraceLink entity type + id). status: Open | InReview |
-- Approved | Rejected | Implemented.
--
-- `integratehub_impact` - one impacted item computed during impact analysis:
-- affected requirements, design items, tests and baselines, plus a risk flag
-- for unverified impact.

CREATE TABLE IF NOT EXISTS integratehub_pr (
    id          TEXT PRIMARY KEY,             -- UUID
    title       TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    severity    TEXT NOT NULL DEFAULT 'medium', -- low | medium | high | critical
    status      TEXT NOT NULL DEFAULT 'open', -- open | under_investigation | resolved | closed
    reported_by TEXT NOT NULL DEFAULT '',
    approval_authority TEXT NOT NULL DEFAULT '',
    created_at  TEXT NOT NULL DEFAULT '',
    updated_at  TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS idx_integratehub_pr_status ON integratehub_pr(status);

CREATE TABLE IF NOT EXISTS integratehub_cr (
    id          TEXT PRIMARY KEY,             -- UUID
    pr_id       TEXT,                         -- NULLABLE: linked problem report
    title       TEXT NOT NULL DEFAULT '',
    description TEXT NOT NULL DEFAULT '',
    status      TEXT NOT NULL DEFAULT 'Open', -- Open | InReview | Approved | Rejected | Implemented
    entity_type TEXT NOT NULL DEFAULT '',     -- requirement | design | test_case | ...
    entity_id   TEXT NOT NULL DEFAULT '',
    proposed_change TEXT NOT NULL DEFAULT '', -- description of the change
    created_by  TEXT NOT NULL DEFAULT '',
    created_at  TEXT NOT NULL DEFAULT '',
    FOREIGN KEY (pr_id) REFERENCES integratehub_pr(id)
);
CREATE INDEX IF NOT EXISTS idx_integratehub_cr_status ON integratehub_cr(status);
CREATE INDEX IF NOT EXISTS idx_integratehub_cr_pr ON integratehub_cr(pr_id);

CREATE TABLE IF NOT EXISTS integratehub_impact (
    id          TEXT PRIMARY KEY,             -- UUID
    cr_id       TEXT NOT NULL,
    target_type TEXT NOT NULL DEFAULT '',     -- requirement | design | test_case | baseline
    target_id   TEXT NOT NULL DEFAULT '',
    risk_unverified INTEGER NOT NULL DEFAULT 0,  -- 1 if impact unverified
    detail      TEXT NOT NULL DEFAULT '',
    FOREIGN KEY (cr_id) REFERENCES integratehub_cr(id)
);
CREATE INDEX IF NOT EXISTS idx_integratehub_impact_cr ON integratehub_impact(cr_id);
