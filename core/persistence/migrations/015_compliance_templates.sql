-- 015_compliance_templates.sql
-- WP-3 (Phase 10): guided OOTB compliance templates/checklists.
--
-- Creates the `compliance_templates` and `compliance_checklist_items` tables
-- used by ComplianceService to store and track guided out-of-the-box
-- templates/checklists for ARP4754A / ARP4761 / DO-178C / DO-254.
-- Append-only and idempotent: never edit this file after it is applied.

CREATE TABLE IF NOT EXISTS compliance_templates (
    id          TEXT PRIMARY KEY,             -- UUID
    name        TEXT NOT NULL,                -- ARP4754A | ARP4761 | DO-178C | DO-254
    description TEXT NOT NULL DEFAULT '',
    created_at  TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS compliance_checklist_items (
    id          TEXT PRIMARY KEY,             -- UUID
    template_id TEXT NOT NULL,
    seq         INTEGER NOT NULL DEFAULT 0,
    title       TEXT NOT NULL DEFAULT '',
    guidance    TEXT NOT NULL DEFAULT '',
    checked     INTEGER NOT NULL DEFAULT 0,   -- 0/1 progress state
    FOREIGN KEY (template_id) REFERENCES compliance_templates(id)
);

-- Fast lookup of a template's checklist items in order.
CREATE INDEX IF NOT EXISTS idx_checklist_template
    ON compliance_checklist_items(template_id, seq);
