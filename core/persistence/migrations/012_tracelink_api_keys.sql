-- 012_tracelink_api_keys.sql
-- WP-E (A8): REST API authentication / API keys.
--
-- Creates the `api_keys` table used by ApiKeyService to issue, validate and
-- revoke API keys for the /tracelink REST surface. Keys are stored as opaque
-- random strings; `enabled` is the revocation flag (1 = valid, 0 = revoked).
-- Append-only and idempotent: never edit this file after it is applied.

CREATE TABLE IF NOT EXISTS api_keys (
    id         TEXT PRIMARY KEY,          -- internal UUID
    key        TEXT NOT NULL UNIQUE,      -- the opaque API key value
    name       TEXT NOT NULL DEFAULT '',  -- human label for the key
    enabled    INTEGER NOT NULL DEFAULT 1, -- 1 = valid, 0 = revoked
    created_at TEXT NOT NULL DEFAULT ''    -- ISO-8601 timestamp
);

-- Fast lookup by key value (the hot path for every authenticated request).
CREATE INDEX IF NOT EXISTS idx_api_keys_key ON api_keys(key);
