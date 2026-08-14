-- 034_variant_attribute_override.sql
-- Gap-Fill TraceLink 3.3: variant inheritance / override of requirement
-- attributes. Idempotent (IF NOT EXISTS). Never edit after it is applied.
--
-- A requirement attribute may be overridden per variant (product-line
-- engineering). Without an override, a variant inherits the base requirement
-- attribute value; with one, the variant's value wins. The override is
-- versioned so branching / divergence can be detected.

CREATE TABLE IF NOT EXISTS variant_attribute_override (
    variant_id      TEXT NOT NULL,
    requirement_id  TEXT NOT NULL,
    attribute       TEXT NOT NULL,             -- e.g. priority | status | safety_level
    value           TEXT NOT NULL DEFAULT '',  -- overridden value
    version         INTEGER NOT NULL DEFAULT 0,
    updated_at      TEXT NOT NULL DEFAULT '',
    PRIMARY KEY (variant_id, requirement_id, attribute)
);
CREATE INDEX IF NOT EXISTS idx_variant_attr_req
    ON variant_attribute_override(requirement_id);
