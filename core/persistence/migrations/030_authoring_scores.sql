-- 030_authoring_scores.sql
-- Gap-Fill RiskAI 1.7: TraceLink inline requirement-quality scoring write hook.
-- Idempotent (IF NOT EXISTS). Never edit after it is applied.
--
-- `authoring_quality_score` stores the persisted per-dimension quality scores
-- written when a requirement is saved (scoreOnSave hook), so the authoring
-- surface has a durable, reviewable record.

CREATE TABLE IF NOT EXISTS authoring_quality_score (
    id          TEXT PRIMARY KEY,             -- UUID
    entity_id   TEXT NOT NULL,                -- TraceLink requirement entity id
    clarity     INTEGER NOT NULL DEFAULT 0,   -- 0..100
    testability INTEGER NOT NULL DEFAULT 0,   -- 0..100
    atomicity   INTEGER NOT NULL DEFAULT 0,   -- 0..100
    completeness INTEGER NOT NULL DEFAULT 0,  -- 0..100
    ambiguity   INTEGER NOT NULL DEFAULT 0,   -- 0..100
    overall     INTEGER NOT NULL DEFAULT 0,   -- 0..100
    created_at  TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS idx_authoring_quality_score_entity
    ON authoring_quality_score(entity_id);
