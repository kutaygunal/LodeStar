-- 011_tracelink_fts.sql
-- WP-A: FTS5 full-text search index across all entity kinds.
--
-- Creates a single FTS5 virtual table `entity_fts` that indexes every entity
-- kind (requirement, design, interface, test_case, hazard, decision,
-- assumption) by (type, external_id, name, body). The index is populated and
-- refreshed by TraceLinkService::rebuildSearchIndex(), which is safe to call
-- at any time. Ranked search uses the FTS5 bm25() score (lower = better).
--
-- Append-only: never edit this file after it is applied.

CREATE VIRTUAL TABLE IF NOT EXISTS entity_fts USING fts5(
    type,
    external_id,
    name,
    body
);
