-- 008_tracelink_import_export.sql
-- Import / export bookkeeping (WP-5, plan schema 3.6).
-- Non-destructive import logging: every CSV/ReqIF import records one row in
-- import_batches and a per-line record in import_log so partial failures are
-- reported without ever corrupting existing data.
-- Append-only: never edit this file after it is applied.

CREATE TABLE IF NOT EXISTS import_batches (
    id             TEXT PRIMARY KEY,          -- UUID
    format         TEXT NOT NULL DEFAULT '',  -- csv | reqif
    filename       TEXT NOT NULL DEFAULT '',
    imported_by    TEXT NOT NULL DEFAULT '',
    imported_at    TEXT NOT NULL DEFAULT '',
    status         TEXT NOT NULL DEFAULT '',  -- Success | Partial | Failed
    result_summary TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS import_log (
    id          TEXT PRIMARY KEY,          -- UUID
    batch_id    TEXT NOT NULL DEFAULT '',
    line        INTEGER NOT NULL DEFAULT 0,
    severity    TEXT NOT NULL DEFAULT '',  -- Info | Warning | Error
    message     TEXT NOT NULL DEFAULT ''
);

-- Lookup / filter indexes.
CREATE INDEX IF NOT EXISTS idx_import_log_batch ON import_log(batch_id);
CREATE INDEX IF NOT EXISTS idx_import_batches_at ON import_batches(imported_at);
