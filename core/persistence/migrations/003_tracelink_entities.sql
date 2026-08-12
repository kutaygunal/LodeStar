-- 003_tracelink_entities.sql
-- Rich typed domain model for TraceLink entities.
-- Turns the flat Phase-3 tables into commercial-grade typed entities and
-- adds the new hazard / decision / assumption tables.
-- Append-only: never edit this file after it is applied.

-- --------------------------------------------------------------------------
-- requirements: add rich attributes.
-- --------------------------------------------------------------------------
ALTER TABLE requirements ADD COLUMN external_id          TEXT NOT NULL DEFAULT '';
ALTER TABLE requirements ADD COLUMN type                 TEXT NOT NULL DEFAULT 'functional';
ALTER TABLE requirements ADD COLUMN priority             TEXT NOT NULL DEFAULT 'Medium';
ALTER TABLE requirements ADD COLUMN source               TEXT NOT NULL DEFAULT '';
ALTER TABLE requirements ADD COLUMN owner                TEXT NOT NULL DEFAULT '';
ALTER TABLE requirements ADD COLUMN rationale            TEXT NOT NULL DEFAULT '';
ALTER TABLE requirements ADD COLUMN verification_method  TEXT NOT NULL DEFAULT '';
ALTER TABLE requirements ADD COLUMN safety_level         TEXT NOT NULL DEFAULT '';
ALTER TABLE requirements ADD COLUMN parent_id            TEXT NOT NULL DEFAULT '';
ALTER TABLE requirements ADD COLUMN sort_order           INTEGER NOT NULL DEFAULT 0;
ALTER TABLE requirements ADD COLUMN tags                 TEXT NOT NULL DEFAULT '';
ALTER TABLE requirements ADD COLUMN version              INTEGER NOT NULL DEFAULT 1;
ALTER TABLE requirements ADD COLUMN created_by           TEXT NOT NULL DEFAULT '';
ALTER TABLE requirements ADD COLUMN created_at           TEXT NOT NULL DEFAULT '';
ALTER TABLE requirements ADD COLUMN updated_by           TEXT NOT NULL DEFAULT '';
ALTER TABLE requirements ADD COLUMN updated_at           TEXT NOT NULL DEFAULT '';

-- --------------------------------------------------------------------------
-- design_items: add rich attributes (status added to support its lifecycle).
-- --------------------------------------------------------------------------
ALTER TABLE design_items ADD COLUMN external_id TEXT NOT NULL DEFAULT '';
ALTER TABLE design_items ADD COLUMN type        TEXT NOT NULL DEFAULT 'component';
ALTER TABLE design_items ADD COLUMN status      TEXT NOT NULL DEFAULT 'Draft';
ALTER TABLE design_items ADD COLUMN owner       TEXT NOT NULL DEFAULT '';
ALTER TABLE design_items ADD COLUMN parent_id   TEXT NOT NULL DEFAULT '';
ALTER TABLE design_items ADD COLUMN tags        TEXT NOT NULL DEFAULT '';
ALTER TABLE design_items ADD COLUMN version     INTEGER NOT NULL DEFAULT 1;
ALTER TABLE design_items ADD COLUMN created_by  TEXT NOT NULL DEFAULT '';
ALTER TABLE design_items ADD COLUMN created_at  TEXT NOT NULL DEFAULT '';
ALTER TABLE design_items ADD COLUMN updated_by  TEXT NOT NULL DEFAULT '';
ALTER TABLE design_items ADD COLUMN updated_at  TEXT NOT NULL DEFAULT '';

-- --------------------------------------------------------------------------
-- interfaces: add rich attributes (status added to support its lifecycle).
-- --------------------------------------------------------------------------
ALTER TABLE interfaces ADD COLUMN external_id    TEXT NOT NULL DEFAULT '';
ALTER TABLE interfaces ADD COLUMN status         TEXT NOT NULL DEFAULT 'Draft';
ALTER TABLE interfaces ADD COLUMN direction      TEXT NOT NULL DEFAULT 'bidirectional';
ALTER TABLE interfaces ADD COLUMN source_entity  TEXT NOT NULL DEFAULT '';
ALTER TABLE interfaces ADD COLUMN target_entity  TEXT NOT NULL DEFAULT '';
ALTER TABLE interfaces ADD COLUMN data_items     TEXT NOT NULL DEFAULT '';
ALTER TABLE interfaces ADD COLUMN protocol       TEXT NOT NULL DEFAULT '';
ALTER TABLE interfaces ADD COLUMN tags           TEXT NOT NULL DEFAULT '';
ALTER TABLE interfaces ADD COLUMN version        INTEGER NOT NULL DEFAULT 1;
ALTER TABLE interfaces ADD COLUMN created_by     TEXT NOT NULL DEFAULT '';
ALTER TABLE interfaces ADD COLUMN created_at     TEXT NOT NULL DEFAULT '';
ALTER TABLE interfaces ADD COLUMN updated_by     TEXT NOT NULL DEFAULT '';
ALTER TABLE interfaces ADD COLUMN updated_at     TEXT NOT NULL DEFAULT '';

-- --------------------------------------------------------------------------
-- test_cases: add rich attributes.
-- --------------------------------------------------------------------------
ALTER TABLE test_cases ADD COLUMN external_id         TEXT NOT NULL DEFAULT '';
ALTER TABLE test_cases ADD COLUMN verification_method TEXT NOT NULL DEFAULT '';
ALTER TABLE test_cases ADD COLUMN result_status       TEXT NOT NULL DEFAULT 'NotExecuted';
ALTER TABLE test_cases ADD COLUMN priority            TEXT NOT NULL DEFAULT 'Medium';
ALTER TABLE test_cases ADD COLUMN tags                TEXT NOT NULL DEFAULT '';
ALTER TABLE test_cases ADD COLUMN version             INTEGER NOT NULL DEFAULT 1;
ALTER TABLE test_cases ADD COLUMN created_by          TEXT NOT NULL DEFAULT '';
ALTER TABLE test_cases ADD COLUMN created_at          TEXT NOT NULL DEFAULT '';
ALTER TABLE test_cases ADD COLUMN updated_by          TEXT NOT NULL DEFAULT '';
ALTER TABLE test_cases ADD COLUMN updated_at          TEXT NOT NULL DEFAULT '';

-- --------------------------------------------------------------------------
-- New entity tables.
-- --------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS hazards (
    id          TEXT PRIMARY KEY,
    external_id TEXT NOT NULL DEFAULT '',
    name        TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    status      TEXT NOT NULL DEFAULT 'Identified',
    severity    TEXT NOT NULL DEFAULT '',
    likelihood  TEXT NOT NULL DEFAULT '',
    owner       TEXT NOT NULL DEFAULT '',
    tags        TEXT NOT NULL DEFAULT '',
    version     INTEGER NOT NULL DEFAULT 1,
    created_by  TEXT NOT NULL DEFAULT '',
    created_at  TEXT NOT NULL DEFAULT '',
    updated_by  TEXT NOT NULL DEFAULT '',
    updated_at  TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS decisions (
    id          TEXT PRIMARY KEY,
    external_id TEXT NOT NULL DEFAULT '',
    name        TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    status      TEXT NOT NULL DEFAULT 'Open',
    rationale   TEXT NOT NULL DEFAULT '',
    owner       TEXT NOT NULL DEFAULT '',
    date        TEXT NOT NULL DEFAULT '',
    tags        TEXT NOT NULL DEFAULT '',
    version     INTEGER NOT NULL DEFAULT 1,
    created_by  TEXT NOT NULL DEFAULT '',
    created_at  TEXT NOT NULL DEFAULT '',
    updated_by  TEXT NOT NULL DEFAULT '',
    updated_at  TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS assumptions (
    id          TEXT PRIMARY KEY,
    external_id TEXT NOT NULL DEFAULT '',
    name        TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    status      TEXT NOT NULL DEFAULT 'Active',
    owner       TEXT NOT NULL DEFAULT '',
    tags        TEXT NOT NULL DEFAULT '',
    version     INTEGER NOT NULL DEFAULT 1,
    created_by  TEXT NOT NULL DEFAULT '',
    created_at  TEXT NOT NULL DEFAULT '',
    updated_by  TEXT NOT NULL DEFAULT '',
    updated_at  TEXT NOT NULL DEFAULT ''
);

-- --------------------------------------------------------------------------
-- Lookup / filter indexes.
-- --------------------------------------------------------------------------
CREATE INDEX IF NOT EXISTS idx_req_external ON requirements(external_id);
CREATE INDEX IF NOT EXISTS idx_req_parent   ON requirements(parent_id);
CREATE INDEX IF NOT EXISTS idx_req_status   ON requirements(status);
CREATE INDEX IF NOT EXISTS idx_req_tags     ON requirements(tags);

CREATE INDEX IF NOT EXISTS idx_design_external ON design_items(external_id);
CREATE INDEX IF NOT EXISTS idx_design_parent   ON design_items(parent_id);
CREATE INDEX IF NOT EXISTS idx_design_status   ON design_items(status);
CREATE INDEX IF NOT EXISTS idx_design_tags     ON design_items(tags);

CREATE INDEX IF NOT EXISTS idx_iface_external ON interfaces(external_id);
CREATE INDEX IF NOT EXISTS idx_iface_status   ON interfaces(status);
CREATE INDEX IF NOT EXISTS idx_iface_tags     ON interfaces(tags);

CREATE INDEX IF NOT EXISTS idx_tc_external ON test_cases(external_id);
CREATE INDEX IF NOT EXISTS idx_tc_status   ON test_cases(status);
CREATE INDEX IF NOT EXISTS idx_tc_tags     ON test_cases(tags);

CREATE INDEX IF NOT EXISTS idx_hazard_external   ON hazards(external_id);
CREATE INDEX IF NOT EXISTS idx_hazard_status     ON hazards(status);
CREATE INDEX IF NOT EXISTS idx_decision_external ON decisions(external_id);
CREATE INDEX IF NOT EXISTS idx_decision_status   ON decisions(status);
CREATE INDEX IF NOT EXISTS idx_assumption_external ON assumptions(external_id);
CREATE INDEX IF NOT EXISTS idx_assumption_status   ON assumptions(status);
