-- 033_electronic_signatures.sql
-- Gap-Fill TraceLink 3.4: electronic signatures on the review/approval workflow.
-- Idempotent (IF NOT EXISTS). Never edit after it is applied.
--
-- `esignature` - an immutable approval signature: signer + role + timestamp
-- + a SHA-256 hash of the approved content. Immutability is enforced by the
-- service (no update/delete); the hash lets the service detect whether the
-- approved content has changed since signing (signature validity check).

CREATE TABLE IF NOT EXISTS esignature (
    id          TEXT PRIMARY KEY,             -- UUID
    entity_type TEXT NOT NULL,
    entity_id   TEXT NOT NULL,
    signer      TEXT NOT NULL,                -- human signer
    role        TEXT NOT NULL DEFAULT '',     -- e.g. certifying engineer
    signed_at   TEXT NOT NULL DEFAULT '',
    content_hash TEXT NOT NULL DEFAULT '',    -- SHA-256 of approved content
    review_id   TEXT NOT NULL DEFAULT ''      -- the approving review this signs
);
CREATE INDEX IF NOT EXISTS idx_esignature_entity
    ON esignature(entity_type, entity_id);
