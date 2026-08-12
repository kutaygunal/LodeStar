-- 017_test_run_coverage.sql
-- WP-5: records which TestForge test run executed a given traceability test
-- case, so live coverage reflects EXECUTED results (not just linked ones).
-- Append-only and idempotent (IF NOT EXISTS). Never edit after it is applied.

CREATE TABLE IF NOT EXISTS test_run_coverage (
    id           TEXT PRIMARY KEY,          -- UUID
    run_id       TEXT NOT NULL,             -- TestForge TestRun id
    test_case_id TEXT NOT NULL,             -- traceability test_case entity UUID
    passed       INTEGER NOT NULL DEFAULT 0, -- 1 if the run passed
    executed_at  TEXT NOT NULL DEFAULT ''
);

CREATE INDEX IF NOT EXISTS idx_run_coverage_tc ON test_run_coverage(test_case_id);
