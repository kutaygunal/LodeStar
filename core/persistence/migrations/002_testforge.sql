-- 002_testforge.sql
-- TestForge (Phase 6): test procedures, steps, runs, and step results.
-- Append-only: never edit this file after it is applied.

CREATE TABLE IF NOT EXISTS test_procedures (
    id          TEXT PRIMARY KEY,
    name        TEXT NOT NULL,
    version     TEXT NOT NULL DEFAULT '1.0',
    objective   TEXT NOT NULL DEFAULT '',
    scenario_id TEXT NOT NULL DEFAULT '',
    status      TEXT NOT NULL DEFAULT 'Pending'
);

CREATE TABLE IF NOT EXISTS test_steps (
    id             TEXT PRIMARY KEY,
    procedure_id   TEXT NOT NULL,
    seq            INTEGER NOT NULL DEFAULT 0,
    name           TEXT NOT NULL,
    description    TEXT NOT NULL DEFAULT '',
    metric         TEXT NOT NULL DEFAULT '',
    expected_value REAL NOT NULL DEFAULT 0.0,
    tolerance      REAL NOT NULL DEFAULT 0.0
);

CREATE TABLE IF NOT EXISTS test_runs (
    id             TEXT PRIMARY KEY,
    procedure_id   TEXT NOT NULL,
    procedure_name TEXT NOT NULL DEFAULT '',
    scenario_id    TEXT NOT NULL DEFAULT '',
    status         TEXT NOT NULL DEFAULT 'Pending',
    started_at     TEXT NOT NULL DEFAULT '',
    finished_at    TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS step_results (
    id             TEXT PRIMARY KEY,
    run_id         TEXT NOT NULL,
    step_id        TEXT NOT NULL,
    seq            INTEGER NOT NULL DEFAULT 0,
    name           TEXT NOT NULL DEFAULT '',
    status         TEXT NOT NULL DEFAULT 'Pending',
    actual_value   REAL NOT NULL DEFAULT 0.0,
    expected_value REAL NOT NULL DEFAULT 0.0,
    tolerance      REAL NOT NULL DEFAULT 0.0,
    measured       INTEGER NOT NULL DEFAULT 0,
    message        TEXT NOT NULL DEFAULT ''
);
