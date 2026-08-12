-- 027_templates.sql
-- S2 Phase 15 (AssureCheck): guided compliance templates/checklists.
--
-- Creates the `guided_templates` and `guided_template_items` tables used by
-- TemplateService to store and track guided out-of-the-box compliance
-- templates/checklists for ARP4754A and DO-178C. Each template is tied to an
-- assurance standard and provides a guided sequence of checklist items with a
-- per-item status (pending | in_progress | complete) so a user can be walked
-- through the compliance process with progress tracking.
-- Append-only and idempotent: never edit this file after it is applied.

CREATE TABLE IF NOT EXISTS guided_templates (
    id          TEXT PRIMARY KEY,             -- UUID
    name        TEXT NOT NULL,                -- ARP4754A | DO-178C
    standard    TEXT NOT NULL,                -- tied assurance standard code
    description TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS guided_template_items (
    id          TEXT PRIMARY KEY,             -- UUID
    template_id TEXT NOT NULL,
    seq         INTEGER NOT NULL DEFAULT 0,   -- order within the template
    title       TEXT NOT NULL DEFAULT '',     -- the guided step title
    guidance    TEXT NOT NULL DEFAULT '',     -- guidance for the step
    status      TEXT NOT NULL DEFAULT 'pending', -- pending | in_progress | complete
    FOREIGN KEY (template_id) REFERENCES guided_templates(id)
);

-- Fast lookup of a template's guided checklist items in order.
CREATE INDEX IF NOT EXISTS idx_guided_items_template
    ON guided_template_items(template_id, seq);
