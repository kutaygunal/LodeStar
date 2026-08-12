-- 007_tracelink_rules.sql
-- Compliance rules engine storage (WP-3, plan schema 3.5).
-- Append-only: never edit this file after it is applied.
--
-- Tables:
--   compliance_rules     - user-defined validation policy (rules are data).
--   compliance_violations- one row per detected rule breach in a validation run.
--   validation_runs      - one row per runValidation() execution.
--
-- Rule types (template names) evaluated by the RulesEngine:
--   REQ_MUST_BE_VERIFIED  REQ_MUST_BE_SATISFIED  NO_DANGLING_LINKS
--   NO_DUPLICATE_LINKS    NO_SELF_LINKS          BIDIRECTIONAL
--   COVERAGE_MIN (param)  NO_ORPHAN_DESIGN       STATUS_VALID
--
-- Each rule is tagged with an assurance standard (ARP4754A, ARP4761, DO-178C,
-- DO-254). Rules are enabled/disabled via enableRule()/disableRule(); only
-- enabled rules are evaluated by runValidation().

CREATE TABLE IF NOT EXISTS compliance_rules (
    id          TEXT PRIMARY KEY,
    name        TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    rule_type   TEXT NOT NULL DEFAULT '',
    params      TEXT NOT NULL DEFAULT '{}',   -- JSON, e.g. {"min_coverage": 80}
    severity    TEXT NOT NULL DEFAULT 'Major',
    standard    TEXT NOT NULL DEFAULT '',     -- ARP4754A | ARP4761 | DO-178C | DO-254
    enabled     INTEGER NOT NULL DEFAULT 1    -- 1 = evaluated, 0 = skipped
);

CREATE TABLE IF NOT EXISTS compliance_violations (
    id          TEXT PRIMARY KEY,
    run_id      TEXT NOT NULL DEFAULT '',
    rule_id     TEXT NOT NULL DEFAULT '',
    entity_type TEXT NOT NULL DEFAULT '',
    entity_id   TEXT NOT NULL DEFAULT '',
    message     TEXT NOT NULL DEFAULT '',
    severity    TEXT NOT NULL DEFAULT '',
    timestamp   TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS validation_runs (
    id          TEXT PRIMARY KEY,
    name        TEXT NOT NULL DEFAULT '',
    started_at  TEXT NOT NULL DEFAULT '',
    finished_at TEXT NOT NULL DEFAULT '',
    status      TEXT NOT NULL DEFAULT '',      -- Complete | Failed
    summary     TEXT NOT NULL DEFAULT ''
);

-- --------------------------------------------------------------------------
-- Lookup / filter indexes.
-- --------------------------------------------------------------------------
CREATE INDEX IF NOT EXISTS idx_rules_name       ON compliance_rules(name);
CREATE INDEX IF NOT EXISTS idx_rules_enabled    ON compliance_rules(enabled);
CREATE INDEX IF NOT EXISTS idx_violations_run   ON compliance_violations(run_id);
CREATE INDEX IF NOT EXISTS idx_violations_rule  ON compliance_violations(rule_id);
CREATE INDEX IF NOT EXISTS idx_violations_ent   ON compliance_violations(entity_type, entity_id);
