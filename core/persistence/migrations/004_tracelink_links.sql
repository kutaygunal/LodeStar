-- 004_tracelink_links.sql
-- Rich trace-link model: metadata, supersession, validity window, and a
-- link_validation view used by the integrity/rules engine.
-- Append-only: never edit this file after it is applied.

-- --------------------------------------------------------------------------
-- trace_links: add metadata columns.
-- --------------------------------------------------------------------------
ALTER TABLE trace_links ADD COLUMN rationale     TEXT NOT NULL DEFAULT '';
ALTER TABLE trace_links ADD COLUMN status        TEXT NOT NULL DEFAULT 'Active';
ALTER TABLE trace_links ADD COLUMN created_by    TEXT NOT NULL DEFAULT '';
ALTER TABLE trace_links ADD COLUMN created_at    TEXT NOT NULL DEFAULT '';
ALTER TABLE trace_links ADD COLUMN updated_at    TEXT NOT NULL DEFAULT '';
ALTER TABLE trace_links ADD COLUMN version       INTEGER NOT NULL DEFAULT 1;
ALTER TABLE trace_links ADD COLUMN superseded_by TEXT NOT NULL DEFAULT '';
ALTER TABLE trace_links ADD COLUMN valid_from    TEXT NOT NULL DEFAULT '';
ALTER TABLE trace_links ADD COLUMN valid_to      TEXT NOT NULL DEFAULT '';

-- --------------------------------------------------------------------------
-- Query indexes.
-- --------------------------------------------------------------------------
CREATE INDEX IF NOT EXISTS idx_link_source   ON trace_links(source_type, source_id);
CREATE INDEX IF NOT EXISTS idx_link_target   ON trace_links(target_type, target_id);
CREATE INDEX IF NOT EXISTS idx_link_relation ON trace_links(relation);

-- --------------------------------------------------------------------------
-- link_validation view: flags dangling links (a side references a node that
-- does not exist in its entity table) and duplicate links (the same
-- source/target/relation triple already exists as an Active link).
-- Each trace link is classified as 'valid', 'dangling_source',
-- 'dangling_target', or 'duplicate'.
-- --------------------------------------------------------------------------
CREATE VIEW IF NOT EXISTS link_validation AS
SELECT
    l.id            AS link_id,
    l.source_type,
    l.source_id,
    l.target_type,
    l.target_id,
    l.relation,
    CASE
        WHEN NOT EXISTS (
            SELECT 1 FROM requirements  r WHERE r.id = l.source_id AND l.source_type = 'requirement'
            UNION ALL
            SELECT 1 FROM design_items  d WHERE d.id = l.source_id AND l.source_type = 'design'
            UNION ALL
            SELECT 1 FROM interfaces   i WHERE i.id = l.source_id AND l.source_type = 'interface'
            UNION ALL
            SELECT 1 FROM test_cases  tc WHERE tc.id = l.source_id AND l.source_type = 'test_case'
            UNION ALL
            SELECT 1 FROM hazards     h WHERE h.id = l.source_id AND l.source_type = 'hazard'
            UNION ALL
            SELECT 1 FROM decisions   dc WHERE dc.id = l.source_id AND l.source_type = 'decision'
            UNION ALL
            SELECT 1 FROM assumptions a WHERE a.id = l.source_id AND l.source_type = 'assumption'
        ) THEN 'dangling_source'
        WHEN NOT EXISTS (
            SELECT 1 FROM requirements  r WHERE r.id = l.target_id AND l.target_type = 'requirement'
            UNION ALL
            SELECT 1 FROM design_items  d WHERE d.id = l.target_id AND l.target_type = 'design'
            UNION ALL
            SELECT 1 FROM interfaces   i WHERE i.id = l.target_id AND l.target_type = 'interface'
            UNION ALL
            SELECT 1 FROM test_cases  tc WHERE tc.id = l.target_id AND l.target_type = 'test_case'
            UNION ALL
            SELECT 1 FROM hazards     h WHERE h.id = l.target_id AND l.target_type = 'hazard'
            UNION ALL
            SELECT 1 FROM decisions   dc WHERE dc.id = l.target_id AND l.target_type = 'decision'
            UNION ALL
            SELECT 1 FROM assumptions a WHERE a.id = l.target_id AND l.target_type = 'assumption'
        ) THEN 'dangling_target'
        WHEN EXISTS (
            SELECT 1 FROM trace_links dup
            WHERE dup.id      != l.id
              AND dup.source_type = l.source_type
              AND dup.source_id   = l.source_id
              AND dup.target_type = l.target_type
              AND dup.target_id   = l.target_id
              AND dup.relation    = l.relation
              AND dup.status      = 'Active'
        ) THEN 'duplicate'
        ELSE 'valid'
    END AS validation
FROM trace_links l;
