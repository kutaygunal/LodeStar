-- 018_matrix_views.sql
-- WP-8: persists named interactive traceability-matrix views (search text,
-- status filter, and toggled-off relations) so a user can save and re-apply
-- their matrix configuration. Append-only and idempotent (IF NOT EXISTS).
-- Never edit after it is applied.

CREATE TABLE IF NOT EXISTS matrix_views (
    id              TEXT PRIMARY KEY,          -- UUID
    name            TEXT NOT NULL,             -- display name (unique)
    search          TEXT NOT NULL DEFAULT '',  -- substring on name/externalId
    status_filter   TEXT NOT NULL DEFAULT '',  -- "" = all, else a status
    hidden_relations TEXT NOT NULL DEFAULT ''   -- '|'-delimited relations to hide
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_matrix_views_name ON matrix_views(name);
