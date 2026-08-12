-- 016_rbac.sql
-- WP-4: User roles + permissions (RBAC) on entities/links.
--
-- Creates the `users`, `roles` and `user_permissions` tables used by
-- RbacService to enforce role-based access control. A user has a single
-- role (admin|editor|reviewer|viewer); fine-grained permissions are granted
-- per user, optionally scoped to one entity type ('' = all types). The
-- `admin` role implicitly holds every permission (enforced in RbacService).
-- Append-only and idempotent: never edit this file after it is applied.

CREATE TABLE IF NOT EXISTS users (
    id       TEXT PRIMARY KEY,             -- UUID
    username TEXT NOT NULL UNIQUE,
    role     TEXT NOT NULL DEFAULT 'viewer'  -- admin|editor|reviewer|viewer
);

-- Canonical role catalog (informational; enforcement lives in RbacService).
CREATE TABLE IF NOT EXISTS roles (
    role        TEXT PRIMARY KEY,          -- admin|editor|reviewer|viewer
    description TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS user_permissions (
    id          TEXT PRIMARY KEY,          -- UUID
    user_id     TEXT NOT NULL,
    permission  TEXT NOT NULL,             -- e.g. "edit", "approve", "delete"
    entity_type TEXT NOT NULL DEFAULT '', -- '' = all types
    FOREIGN KEY (user_id) REFERENCES users(id)
);

-- Fast lookup by (user, permission) — the hot path for every auth check.
CREATE INDEX IF NOT EXISTS idx_user_perm ON user_permissions(user_id, permission);
