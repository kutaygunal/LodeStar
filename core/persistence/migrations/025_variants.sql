-- 025_variants.sql
-- S2 Phase 16 (Variants / branching): product-line engineering.
-- Append-only. Never edit after it is applied.
--
-- A product variant (e.g. "Base", "Pro", "Avionics") is a named set of
-- included/excluded requirements. A branch is a working copy of a variant's
-- requirement set that can be modified independently and merged back, with
-- conflict detection when the same requirement was changed differently on the
-- branch and the target since the branch was created.

-- Product variants.
CREATE TABLE IF NOT EXISTS variants (
    id          TEXT PRIMARY KEY,             -- UUID
    name        TEXT NOT NULL,                -- e.g. "Base", "Pro", "Avionics"
    created_at  TEXT NOT NULL DEFAULT ''
);

-- Which requirements belong to a variant (included=1) or are excluded (0).
-- `version` is bumped on every change so a branch can detect concurrent edits.
CREATE TABLE IF NOT EXISTS variant_requirements (
    variant_id      TEXT NOT NULL,
    requirement_id  TEXT NOT NULL,
    included        INTEGER NOT NULL DEFAULT 1,
    version         INTEGER NOT NULL DEFAULT 0,
    updated_at      TEXT NOT NULL DEFAULT '',
    PRIMARY KEY (variant_id, requirement_id)
);
CREATE INDEX IF NOT EXISTS idx_variant_req_variant
    ON variant_requirements(variant_id);

-- Branches of a variant.
CREATE TABLE IF NOT EXISTS variant_branches (
    id              TEXT PRIMARY KEY,         -- UUID
    base_variant_id TEXT NOT NULL,            -- the variant this branch was cut from
    name            TEXT NOT NULL,            -- e.g. "feature-x"
    created_at      TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS idx_variant_branch_base
    ON variant_branches(base_variant_id);

-- Working copy of a variant's requirement set on a branch. `base_version`
-- records the version of each requirement at the moment the branch was created
-- (the merge base), so mergeBranch can detect when both the branch and the
-- target changed the same requirement since the branch was cut.
CREATE TABLE IF NOT EXISTS branch_requirements (
    branch_id       TEXT NOT NULL,
    requirement_id  TEXT NOT NULL,
    included        INTEGER NOT NULL DEFAULT 1,
    version         INTEGER NOT NULL DEFAULT 0,
    base_version    INTEGER NOT NULL DEFAULT 0,
    updated_at      TEXT NOT NULL DEFAULT '',
    PRIMARY KEY (branch_id, requirement_id)
);
CREATE INDEX IF NOT EXISTS idx_branch_req_branch
    ON branch_requirements(branch_id);
