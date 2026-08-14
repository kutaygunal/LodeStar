// core/riskai/KnowledgeService.h
// Gap-Fill RiskAI 1.5: multi-document knowledge input.
//
// Ingests requirements, flow charts, standards, historical data, drawing
// text and lessons-learned into a persistable store, and provides a
// lightweight retrieval step (keyword relevance) that feeds context to the
// LLM prompt during generation. Retrieval is deterministic, so generation
// works with no LLM.

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"

namespace lodestar::riskai {

// Source document kinds. Deterministic weighting lets retrieval rank by type.
enum class KnowledgeDocType {
    Requirement,
    FlowChart,
    Standard,
    HistoricalData,
    Drawing,
    LessonsLearned,
    Other
};

std::string docTypeName(KnowledgeDocType t);
KnowledgeDocType docTypeFromName(const std::string& name);

struct KnowledgeDoc {
    std::string id;
    KnowledgeDocType type = KnowledgeDocType::Requirement;
    std::string title;
    std::string content;
    std::string source;     // original filename / reference
    std::string createdBy;
    std::string createdAt;
};

// One retrieved context chunk with its relevance score.
struct KnowledgeChunk {
    std::string docId;
    std::string content;
    int score = 0;  // relevance (higher = more relevant); type weighting applied
};

class KnowledgeService {
public:
    explicit KnowledgeService(persistence::Database& db);

    common::Result<std::string> ingest(const KnowledgeDoc& doc);

    // List all docs, optionally filtered by type.
    common::Result<std::vector<KnowledgeDoc>> list(
        const std::optional<KnowledgeDocType>& type = std::nullopt);

    // Deterministic retrieval: score each doc's chunks against the query,
    // weight by document type, and return the top `limit` chunks.
    common::Result<std::vector<KnowledgeChunk>> retrieve(const std::string& query,
                                                         int limit = 5);

    // Build a compact context string (for the LLM prompt) from retrieved chunks.
    // Deterministic fallback: if nothing retrieves, returns an empty string.
    std::string buildContext(const std::vector<KnowledgeChunk>& chunks) const;

private:
    persistence::Database& db_;
};

}  // namespace lodestar::riskai
