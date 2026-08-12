-- 009_tracelink_perf.sql
-- WP-8 commercial hardening: performance indexes for the hot queries
-- (closure / impact / coverage / filtering) and the required index names
-- from docs/tracelink-plan.md section 8 (WP-8).
--
-- All statements use IF NOT EXISTS so they are idempotent and coexist with
-- the earlier index set (003/004/005). These cover:
--   * requirements lookup by external_id and filter by status
--   * trace_links traversal by source, by target, and by relation
--   * audit_log filtering by entity and by timestamp
-- Append-only: never edit this file after it is applied.

-- Requirements.
CREATE INDEX IF NOT EXISTS idx_requirements_external_id ON requirements(external_id);
CREATE INDEX IF NOT EXISTS idx_requirements_status     ON requirements(status);
CREATE INDEX IF NOT EXISTS idx_requirements_type       ON requirements(type);
CREATE INDEX IF NOT EXISTS idx_requirements_type_status ON requirements(type, status);
CREATE INDEX IF NOT EXISTS idx_requirements_type_parent ON requirements(type, parent_id);

-- Trace links (closure / impact / coverage hot paths).
CREATE INDEX IF NOT EXISTS idx_trace_links_source    ON trace_links(source_type, source_id);
CREATE INDEX IF NOT EXISTS idx_trace_links_target    ON trace_links(target_type, target_id);
CREATE INDEX IF NOT EXISTS idx_trace_links_relation  ON trace_links(relation);

-- Audit log (append-only trail lookups).
CREATE INDEX IF NOT EXISTS idx_audit_entity          ON audit_log(entity_type, entity_id);
CREATE INDEX IF NOT EXISTS idx_audit_timestamp       ON audit_log(timestamp);
