-- 026_coverage.sql
-- S2 Phase 7: structural code coverage results (statement / decision / MC/DC).
-- Stores the raw counts for a scope (module or test run) so percentages can be
-- recomputed and reported. Append-only and idempotent (IF NOT EXISTS). Never
-- edit after it is applied.

CREATE TABLE IF NOT EXISTS coverage_results (
    id                  TEXT PRIMARY KEY,   -- UUID
    run_id              TEXT NOT NULL DEFAULT '',  -- TestForge TestRun id
    scope               TEXT NOT NULL DEFAULT '',  -- e.g. "module:core/testforge/Coverage.cpp"
    statements_executed INTEGER NOT NULL DEFAULT 0,
    statements_total    INTEGER NOT NULL DEFAULT 0,
    decisions_taken     INTEGER NOT NULL DEFAULT 0,
    decisions_total     INTEGER NOT NULL DEFAULT 0,
    conditions_satisfied INTEGER NOT NULL DEFAULT 0,
    conditions_total    INTEGER NOT NULL DEFAULT 0,
    recorded_at         TEXT NOT NULL DEFAULT ''
);

CREATE INDEX IF NOT EXISTS idx_coverage_run ON coverage_results(run_id);
