-- 021_assurecheck_perf.sql
-- Phase 11 WP-5 (AssureCheck): performance indexes that make compliance
-- lookups fast at scale. Append-only and idempotent (IF NOT EXISTS).
-- Never edit after it is applied.
--
-- WP-1 migration 019 already creates idx_assurance_items_standard; WP-2
-- migration 020 already creates idx_assurance_checks_item and
-- idx_assurance_checks_status. WP-5 adds the two indexes below.

CREATE INDEX IF NOT EXISTS idx_assurance_checks_standard
    ON assurance_checks(standard_id);

CREATE INDEX IF NOT EXISTS idx_assurance_items_code
    ON assurance_checklist_items(item_code);
