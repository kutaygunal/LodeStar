-- 023_user_sessions.sql
-- S2 Phase 1: user accounts with login + sessions + optimistic-lock version.
--
-- Extends the WP-4 `users` table (migration 016) with a salted password hash
-- and an optimistic-lock `version` column, and adds a `sessions` table for
-- login/logout. The '' default for password_hash keeps the WP-4
-- RbacService::createUser insert (id, username, role) working unchanged.
-- Append-only and idempotent: never edit this file after it is applied.

-- Salted password hash (never plaintext).
ALTER TABLE users ADD COLUMN password_hash TEXT NOT NULL DEFAULT '';

-- Optimistic-lock version for concurrent-edit conflict detection on user rows.
ALTER TABLE users ADD COLUMN version INTEGER NOT NULL DEFAULT 1;

-- Active login sessions. A token is valid only while present and not expired.
CREATE TABLE IF NOT EXISTS sessions (
    token      TEXT PRIMARY KEY,
    user_id    TEXT NOT NULL,
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    expires_at TEXT NOT NULL,
    FOREIGN KEY (user_id) REFERENCES users(id)
);

CREATE INDEX IF NOT EXISTS idx_sessions_user ON sessions(user_id);
