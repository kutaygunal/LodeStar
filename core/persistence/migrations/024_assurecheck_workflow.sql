-- 024_assurecheck_workflow.sql
-- S2 Phase 3 (AssureCheck): review/approval/sign-off workflow + audit trail.
-- Append-only. Never edit after it is applied.
--
-- Adds workflow columns to assurance_checks (created by migration 020) so a
-- check result can be submitted for review, approved, or rejected by a named
-- actor with a real timestamp (never the literal placeholder "now"). Also
-- creates the audit table that records every transition (who, what, when,
-- from->to).

ALTER TABLE assurance_checks ADD COLUMN workflow_state TEXT NOT NULL DEFAULT 'draft';
ALTER TABLE assurance_checks ADD COLUMN reviewed_by   TEXT NOT NULL DEFAULT '';
ALTER TABLE assurance_checks ADD COLUMN reviewed_at   TEXT NOT NULL DEFAULT '';
ALTER TABLE assurance_checks ADD COLUMN approved_by   TEXT NOT NULL DEFAULT '';
ALTER TABLE assurance_checks ADD COLUMN approved_at   TEXT NOT NULL DEFAULT '';
ALTER TABLE assurance_checks ADD COLUMN rejected_by   TEXT NOT NULL DEFAULT '';
ALTER TABLE assurance_checks ADD COLUMN rejected_at   TEXT NOT NULL DEFAULT '';

CREATE TABLE IF NOT EXISTS assurance_workflow_audit (
    id          TEXT PRIMARY KEY,             -- UUID
    result_id   TEXT NOT NULL,                -- target check result
    actor       TEXT NOT NULL,                -- who performed the transition
    action      TEXT NOT NULL,                -- submit | approve | reject
    target      TEXT NOT NULL,                -- result id
    timestamp   TEXT NOT NULL,                -- real date/time
    from_state  TEXT NOT NULL,                -- draft | in_review | approved | rejected
    to_state    TEXT NOT NULL,
    FOREIGN KEY (result_id) REFERENCES assurance_checks(id)
);
CREATE INDEX IF NOT EXISTS idx_assurance_audit_result
    ON assurance_workflow_audit(result_id);
