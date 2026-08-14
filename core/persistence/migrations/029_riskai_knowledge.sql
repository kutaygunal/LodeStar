-- 029_riskai_knowledge.sql
-- Gap-Fill RiskAI 1.5: multi-document knowledge input.
-- Idempotent (IF NOT EXISTS). Never edit after it is applied.
--
-- `riskai_knowledge_doc` - one ingested source document (requirement, flow
-- chart, standard, historical data, drawing text, lessons-learned). The
-- `doc_type` distinguishes the source kind so retrieval can weight by type.
--
-- `riskai_knowledge_chunk` - a retrieved context chunk derived from a doc,
-- used to feed the LLM prompt during generation. Kept in its own table so a
-- lightweight retrieval step (keyword search) can serve context with no LLM.

CREATE TABLE IF NOT EXISTS riskai_knowledge_doc (
    id          TEXT PRIMARY KEY,             -- UUID
    doc_type    TEXT NOT NULL DEFAULT 'requirement', -- requirement | flow_chart |
                                                -- standard | historical | drawing |
                                                -- lessons_learned | other
    title       TEXT NOT NULL DEFAULT '',
    content     TEXT NOT NULL DEFAULT '',
    source      TEXT NOT NULL DEFAULT '',    -- original filename / reference
    created_by  TEXT NOT NULL DEFAULT '',
    created_at  TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS idx_riskai_knowledge_doc_type
    ON riskai_knowledge_doc(doc_type);

CREATE TABLE IF NOT EXISTS riskai_knowledge_chunk (
    id          TEXT PRIMARY KEY,             -- UUID
    doc_id      TEXT NOT NULL,
    content     TEXT NOT NULL DEFAULT '',
    FOREIGN KEY (doc_id) REFERENCES riskai_knowledge_doc(id)
);
CREATE INDEX IF NOT EXISTS idx_riskai_knowledge_chunk_doc
    ON riskai_knowledge_chunk(doc_id);
