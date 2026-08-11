-- 001_initial.sql
-- Initial Lodestar schema for the Phase 3 vertical slice.
-- Tables cover requirements, design, interfaces, test cases, trace links,
-- and scenarios. Append-only: never edit this file after it is applied.

CREATE TABLE IF NOT EXISTS requirements (
    id          TEXT PRIMARY KEY,
    name        TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    status      TEXT NOT NULL DEFAULT 'Draft'
);

CREATE TABLE IF NOT EXISTS design_items (
    id          TEXT PRIMARY KEY,
    name        TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS interfaces (
    id          TEXT PRIMARY KEY,
    name        TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS test_cases (
    id          TEXT PRIMARY KEY,
    name        TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    status      TEXT NOT NULL DEFAULT 'Draft'
);

CREATE TABLE IF NOT EXISTS trace_links (
    id          TEXT PRIMARY KEY,
    source_type TEXT NOT NULL,
    source_id   TEXT NOT NULL,
    target_type TEXT NOT NULL,
    target_id   TEXT NOT NULL,
    relation    TEXT NOT NULL DEFAULT 'traces_to'
);

CREATE TABLE IF NOT EXISTS scenarios (
    id          TEXT PRIMARY KEY,
    name        TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    status      TEXT NOT NULL DEFAULT 'Draft'
);
