-- 022_integratehub.sql
-- Sprint 1 Phase 4 (IntegrateHub): cross-disciplinary issue/coordination model.
-- Idempotent (IF NOT EXISTS). Never edit after it is applied.
--
-- `integratehub_issues` stores one cross-disciplinary issue owned by a single
-- discipline (Systems | Software | Hardware | Test | Safety). `status` is one
-- of open | in_progress | resolved.
--
-- `integratehub_coordination` stores coordination notes attached to an issue,
-- oldest first (ordered by created_at, then rowid for stability).

CREATE TABLE IF NOT EXISTS integratehub_issues (
    id          TEXT PRIMARY KEY,             -- UUID
    title       TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    owner       TEXT NOT NULL,                -- Systems | Software | Hardware | Test | Safety
    status      TEXT NOT NULL DEFAULT 'open', -- open | in_progress | resolved
    created_at  TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS idx_integratehub_issues_owner
    ON integratehub_issues(owner);
CREATE INDEX IF NOT EXISTS idx_integratehub_issues_status
    ON integratehub_issues(status);

CREATE TABLE IF NOT EXISTS integratehub_coordination (
    id          TEXT PRIMARY KEY,             -- UUID
    issue_id    TEXT NOT NULL,
    note        TEXT NOT NULL DEFAULT '',
    created_at  TEXT NOT NULL DEFAULT '',
    FOREIGN KEY (issue_id) REFERENCES integratehub_issues(id)
);
CREATE INDEX IF NOT EXISTS idx_integratehub_coordination_issue
    ON integratehub_coordination(issue_id);
