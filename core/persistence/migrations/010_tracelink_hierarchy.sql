-- 010_tracelink_hierarchy.sql
-- WP-C (A2): requirement hierarchy tree support.
--
-- The requirements and design_items tables already carry parent_id and
-- sort_order (added in 003). This migration extends the same parent/child
-- navigation + ordering columns to the remaining entity tables so the
-- hierarchy API (setParent / children / subtree / ancestors / reorder /
-- rootNodes / buildTree) works uniformly across every TraceLink entity type.
--
-- Append-only: never edit this file after it is applied.

-- Interfaces.
ALTER TABLE interfaces ADD COLUMN parent_id  TEXT NOT NULL DEFAULT '';
ALTER TABLE interfaces ADD COLUMN sort_order INTEGER NOT NULL DEFAULT 0;

-- Test cases.
ALTER TABLE test_cases ADD COLUMN parent_id  TEXT NOT NULL DEFAULT '';
ALTER TABLE test_cases ADD COLUMN sort_order INTEGER NOT NULL DEFAULT 0;

-- Hazards.
ALTER TABLE hazards ADD COLUMN parent_id  TEXT NOT NULL DEFAULT '';
ALTER TABLE hazards ADD COLUMN sort_order INTEGER NOT NULL DEFAULT 0;

-- Decisions.
ALTER TABLE decisions ADD COLUMN parent_id  TEXT NOT NULL DEFAULT '';
ALTER TABLE decisions ADD COLUMN sort_order INTEGER NOT NULL DEFAULT 0;

-- Assumptions.
ALTER TABLE assumptions ADD COLUMN parent_id  TEXT NOT NULL DEFAULT '';
ALTER TABLE assumptions ADD COLUMN sort_order INTEGER NOT NULL DEFAULT 0;

-- Hierarchy traversal indexes (parent lookup + ordered children).
CREATE INDEX IF NOT EXISTS idx_iface_parent   ON interfaces(parent_id);
CREATE INDEX IF NOT EXISTS idx_tc_parent      ON test_cases(parent_id);
CREATE INDEX IF NOT EXISTS idx_hazard_parent  ON hazards(parent_id);
CREATE INDEX IF NOT EXISTS idx_decision_parent ON decisions(parent_id);
CREATE INDEX IF NOT EXISTS idx_assumption_parent ON assumptions(parent_id);
