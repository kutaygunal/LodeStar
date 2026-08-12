-- 019_assurecheck_standards.sql
-- Phase 11 WP-1 (AssureCheck): standards registry + checklist-item tables so
-- the five assurance standards (DO-178C, DO-254, ARP4754A, ARP4761, DO-278A)
-- and all 136 checklist items from docs/assurecheck-standards-checklist.md can
-- be stored and seeded. Append-only and idempotent (IF NOT EXISTS).
-- Never edit after it is applied.

CREATE TABLE IF NOT EXISTS assurance_standards (
    id          TEXT PRIMARY KEY,             -- UUID
    code        TEXT NOT NULL UNIQUE,         -- DO-178C | DO-254 | ARP4754A | ARP4761 | DO-278A
    name        TEXT NOT NULL,                -- full standard name
    description TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS assurance_checklist_items (
    id          TEXT PRIMARY KEY,             -- UUID
    standard_id TEXT NOT NULL,
    item_code   TEXT NOT NULL,                -- A1-1 | D254-1 | A4754-1 | A4761-1 | D278-1
    seq         INTEGER NOT NULL DEFAULT 0,   -- order within the standard
    objective   TEXT NOT NULL,                -- the objective text
    dal_level   TEXT NOT NULL DEFAULT 'A-D',  -- A | A-B | A-C | A-D (range of applicable DALs)
    evidence    TEXT NOT NULL DEFAULT '',     -- evidence required
    FOREIGN KEY (standard_id) REFERENCES assurance_standards(id)
);
CREATE INDEX IF NOT EXISTS idx_assurance_items_standard
    ON assurance_checklist_items(standard_id, seq);
