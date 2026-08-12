-- 006_tracelink_baseline.sql
-- Baseline snapshots (WP-4, plan schema 3.4).
-- A baseline is a point-in-time snapshot of every active entity and link,
-- stored as JSON so an auditor can reconstruct exactly what the project
-- looked like at release time.
-- Append-only: never edit this file after it is applied.

CREATE TABLE IF NOT EXISTS baselines (
    id          TEXT PRIMARY KEY,          -- UUID
    name        TEXT NOT NULL DEFAULT '',
    description TEXT NOT NULL DEFAULT '',
    created_by  TEXT NOT NULL DEFAULT '',
    created_at  TEXT NOT NULL DEFAULT ''
);

-- One row per entity snapshot captured in a baseline.
CREATE TABLE IF NOT EXISTS baseline_entities (
    baseline_id TEXT NOT NULL DEFAULT '',
    entity_type TEXT NOT NULL DEFAULT '',
    entity_id   TEXT NOT NULL DEFAULT '',
    version     INTEGER NOT NULL DEFAULT 1,
    snapshot    TEXT NOT NULL DEFAULT '{}',  -- JSON of the full entity at release time
    PRIMARY KEY (baseline_id, entity_type, entity_id)
);

-- One row per link snapshot captured in a baseline.
CREATE TABLE IF NOT EXISTS baseline_links (
    baseline_id TEXT NOT NULL DEFAULT '',
    link_id     TEXT NOT NULL DEFAULT '',
    snapshot    TEXT NOT NULL DEFAULT '{}',  -- JSON of the link at release time
    PRIMARY KEY (baseline_id, link_id)
);

-- Lookup / filter indexes.
CREATE INDEX IF NOT EXISTS idx_baseline_entities_ent ON baseline_entities(entity_type, entity_id);
CREATE INDEX IF NOT EXISTS idx_baseline_links_link   ON baseline_links(link_id);
