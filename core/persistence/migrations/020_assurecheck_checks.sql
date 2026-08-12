-- 020_assurecheck_checks.sql
-- Phase 11 WP-2 (AssureCheck): stores compliance-check results produced by the
-- ComplianceEngine. Append-only and idempotent (IF NOT EXISTS). Never edit
-- after it is applied.
--
-- Each row is one evaluated checklist item for a standard. `status` is one of
-- PASS | FAIL | NA | WARNING. `dal_level` records the item's DAL range (e.g.
-- A | A-B | A-C | A-D). `evidence` holds "type:id;type:id" evidence links that
-- satisfied the objective on PASS.

CREATE TABLE IF NOT EXISTS assurance_checks (
    id          TEXT PRIMARY KEY,             -- UUID
    standard_id TEXT NOT NULL,
    item_id     TEXT NOT NULL,
    item_code   TEXT NOT NULL DEFAULT '',     -- A1-1 | D254-1 | ...
    status      TEXT NOT NULL DEFAULT 'NA',   -- PASS | FAIL | NA | WARNING
    dal_level   TEXT NOT NULL DEFAULT '',     -- item's DAL range (A | A-B | A-C | A-D)
    evidence    TEXT NOT NULL DEFAULT '',     -- "type:id;type:id" evidence links
    detail      TEXT NOT NULL DEFAULT '',
    checked_at  TEXT NOT NULL DEFAULT '',
    FOREIGN KEY (standard_id) REFERENCES assurance_standards(id),
    FOREIGN KEY (item_id) REFERENCES assurance_checklist_items(id)
);
CREATE INDEX IF NOT EXISTS idx_assurance_checks_item
    ON assurance_checks(item_id);
CREATE INDEX IF NOT EXISTS idx_assurance_checks_status
    ON assurance_checks(status);
