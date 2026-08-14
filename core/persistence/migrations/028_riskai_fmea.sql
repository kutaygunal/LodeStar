-- 028_riskai_fmea.sql
-- Gap-Fill RiskAI 1.1: FMEA workflow engine (AIAG/VDA shape).
-- Idempotent (IF NOT EXISTS). Never edit after it is applied.
--
-- `riskai_fmea`  - one FMEA analysis (a workflow instance). `stage` holds the
--                  current AIAG-VDA step: Planning | Structure | Function |
--                  Failure | Risk | Optimization | Documentation. Required
--                  fields are enforced by the service layer (stage gating),
--                  not by the schema.
--
-- `riskai_fmea_row` - one failure chain record within an FMEA. The chain is
--                  Effect (FE) -> Failure Mode (FM) -> Cause (FC), with
--                  ratings Severity (S), Occurrence (O), Detection (D) and a
--                  computed Action Priority (AP: High | Medium | Low).

CREATE TABLE IF NOT EXISTS riskai_fmea (
    id          TEXT PRIMARY KEY,             -- UUID
    name        TEXT NOT NULL,
    system      TEXT NOT NULL DEFAULT '',     -- focus element / system under analysis
    next_higher TEXT NOT NULL DEFAULT '',     -- next-higher structure element
    next_lower  TEXT NOT NULL DEFAULT '',     -- next-lower structure element
    stage       TEXT NOT NULL DEFAULT 'Planning', -- AIAG-VDA step 1..7
    created_by  TEXT NOT NULL DEFAULT '',
    created_at  TEXT NOT NULL DEFAULT '',
    updated_at  TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS idx_riskai_fmea_stage ON riskai_fmea(stage);

CREATE TABLE IF NOT EXISTS riskai_fmea_function (
    id           TEXT PRIMARY KEY,            -- UUID
    fmea_id      TEXT NOT NULL,
    function_text TEXT NOT NULL DEFAULT '',
    requirement  TEXT NOT NULL DEFAULT '',
    sort_order   INTEGER NOT NULL DEFAULT 0,
    FOREIGN KEY (fmea_id) REFERENCES riskai_fmea(id)
);
CREATE INDEX IF NOT EXISTS idx_riskai_fmea_function_fmea
    ON riskai_fmea_function(fmea_id);

CREATE TABLE IF NOT EXISTS riskai_fmea_row (
    id            TEXT PRIMARY KEY,           -- UUID
    fmea_id       TEXT NOT NULL,
    function_id   TEXT NOT NULL DEFAULT '',
    effect        TEXT NOT NULL DEFAULT '',   -- FE: failure effect
    failure_mode  TEXT NOT NULL DEFAULT '',   -- FM: failure mode
    cause         TEXT NOT NULL DEFAULT '',   -- FC: failure cause
    severity      INTEGER NOT NULL DEFAULT 0, -- S 1..10 (0 = unset)
    occurrence    INTEGER NOT NULL DEFAULT 0, -- O 1..10 (0 = unset)
    detection     INTEGER NOT NULL DEFAULT 0, -- D 1..10 (0 = unset)
    action_priority TEXT NOT NULL DEFAULT '', -- High | Medium | Low | '' (unset)
    sort_order    INTEGER NOT NULL DEFAULT 0,
    FOREIGN KEY (fmea_id) REFERENCES riskai_fmea(id)
);
CREATE INDEX IF NOT EXISTS idx_riskai_fmea_row_fmea ON riskai_fmea_row(fmea_id);
