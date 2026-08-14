// core/riskai/KnowledgeService.cpp
// Gap-Fill RiskAI 1.5: multi-document knowledge input.

#include "core/riskai/KnowledgeService.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#include <sqlite3.h>

#include "core/common/Time.h"
#include "core/common/Uuid.h"

namespace lodestar::riskai {

using lodestar::common::newUuid;
using lodestar::common::nowIso;

namespace {

void bindText(sqlite3_stmt* stmt, int index, const std::string& value) {
    sqlite3_bind_text(stmt, index, value.c_str(), static_cast<int>(value.size()),
                      SQLITE_TRANSIENT);
}

std::string colText(sqlite3_stmt* stmt, int col) {
    const unsigned char* t = sqlite3_column_text(stmt, col);
    return t ? reinterpret_cast<const char*>(t) : std::string();
}

common::Result<void> exec(sqlite3* db, const std::string& sql,
                          const std::vector<std::string>& params = {}) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<void>::err("prepare failed: " +
                                         std::string(sqlite3_errmsg(db)));
    }
    for (size_t i = 0; i < params.size(); ++i) {
        bindText(stmt, static_cast<int>(i + 1), params[i]);
    }
    int rc = sqlite3_step(stmt);
    std::string msg = sqlite3_errmsg(db);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return common::Result<void>::err("step failed: " + msg);
    }
    return common::Result<void>::ok();
}

std::string lower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return out;
}

// Split into lowercase words, discarding very short tokens/stop words.
std::vector<std::string> tokens(const std::string& s) {
    static const std::vector<std::string> stop = {
        "the", "a", "an", "and", "or", "of", "to", "in", "for", "on", "with",
        "is", "are", "be", "shall", "must", "should", "will", "this", "that"};
    std::vector<std::string> out;
    std::string cur;
    auto flush = [&]() {
        if (cur.size() < 3) { cur.clear(); return; }
        std::string w = lower(cur);
        if (std::find(stop.begin(), stop.end(), w) == stop.end()) out.push_back(w);
        cur.clear();
    };
    for (char c : s) {
        if (std::isalnum(static_cast<unsigned char>(c))) cur += c;
        else flush();
    }
    flush();
    return out;
}

}  // namespace

std::string docTypeName(KnowledgeDocType t) {
    switch (t) {
        case KnowledgeDocType::Requirement: return "requirement";
        case KnowledgeDocType::FlowChart: return "flow_chart";
        case KnowledgeDocType::Standard: return "standard";
        case KnowledgeDocType::HistoricalData: return "historical";
        case KnowledgeDocType::Drawing: return "drawing";
        case KnowledgeDocType::LessonsLearned: return "lessons_learned";
        case KnowledgeDocType::Other: return "other";
    }
    return "other";
}

KnowledgeDocType docTypeFromName(const std::string& name) {
    if (name == "requirement") return KnowledgeDocType::Requirement;
    if (name == "flow_chart") return KnowledgeDocType::FlowChart;
    if (name == "standard") return KnowledgeDocType::Standard;
    if (name == "historical") return KnowledgeDocType::HistoricalData;
    if (name == "drawing") return KnowledgeDocType::Drawing;
    if (name == "lessons_learned") return KnowledgeDocType::LessonsLearned;
    return KnowledgeDocType::Other;
}

KnowledgeService::KnowledgeService(persistence::Database& db) : db_(db) {}

common::Result<std::string> KnowledgeService::ingest(const KnowledgeDoc& doc) {
    if (doc.content.empty() && doc.title.empty()) {
        return common::Result<std::string>::err(
            common::ErrorCode::InvalidArgument,
            "doc must have a title or content");
    }
    const std::string id = newUuid();
    auto res = exec(db_.handle(),
                    "INSERT INTO riskai_knowledge_doc "
                    "(id, doc_type, title, content, source, created_by, created_at) "
                    "VALUES (?,?,?,?,?,?,?);",
                    {id, docTypeName(doc.type), doc.title, doc.content,
                     doc.source, doc.createdBy, nowIso()});
    if (res.failed()) {
        return common::Result<std::string>::err(res.error());
    }
    // Store the content as a single retrievable chunk for simplicity and so
    // retrieval works even when the LLM is absent (deterministic fallback).
    const std::string chunkId = newUuid();
    exec(db_.handle(),
         "INSERT INTO riskai_knowledge_chunk (id, doc_id, content) "
         "VALUES (?,?,?);",
         {chunkId, id, doc.content});
    return common::Result<std::string>::ok(id);
}

common::Result<std::vector<KnowledgeDoc>> KnowledgeService::list(
    const std::optional<KnowledgeDocType>& type) {
    std::vector<KnowledgeDoc> out;
    sqlite3_stmt* stmt = nullptr;
    std::string sql =
        "SELECT id, doc_type, title, content, source, created_by, created_at "
        "FROM riskai_knowledge_doc";
    std::vector<std::string> params;
    if (type.has_value()) {
        sql += " WHERE doc_type=?";
        params.push_back(docTypeName(*type));
    }
    sql += " ORDER BY created_at, rowid;";
    if (sqlite3_prepare_v2(db_.handle(), sql.c_str(), -1, &stmt, nullptr) !=
        SQLITE_OK) {
        return common::Result<std::vector<KnowledgeDoc>>::err(
            "prepare failed: " + std::string(sqlite3_errmsg(db_.handle())));
    }
    for (size_t i = 0; i < params.size(); ++i) bindText(stmt, (int)(i + 1), params[i]);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        KnowledgeDoc d;
        d.id = colText(stmt, 0);
        d.type = docTypeFromName(colText(stmt, 1));
        d.title = colText(stmt, 2);
        d.content = colText(stmt, 3);
        d.source = colText(stmt, 4);
        d.createdBy = colText(stmt, 5);
        d.createdAt = colText(stmt, 6);
        out.push_back(std::move(d));
    }
    sqlite3_finalize(stmt);
    return common::Result<std::vector<KnowledgeDoc>>::ok(std::move(out));
}

// Type weight: standards + requirements + lessons-learned rank higher than
// generic/other sources for risk context.
static int typeWeight(KnowledgeDocType t) {
    switch (t) {
        case KnowledgeDocType::Standard: return 3;
        case KnowledgeDocType::Requirement: return 2;
        case KnowledgeDocType::LessonsLearned: return 2;
        case KnowledgeDocType::HistoricalData: return 1;
        case KnowledgeDocType::FlowChart: return 1;
        case KnowledgeDocType::Drawing: return 1;
        case KnowledgeDocType::Other: return 1;
    }
    return 1;
}

common::Result<std::vector<KnowledgeChunk>> KnowledgeService::retrieve(
    const std::string& query, int limit) {
    std::vector<KnowledgeChunk> out;
    if (query.empty() || limit <= 0) {
        return common::Result<std::vector<KnowledgeChunk>>::ok(std::move(out));
    }
    std::vector<std::string> qtokens = tokens(query);

    // Read all docs (with type) and their chunks.
    sqlite3_stmt* stmt = nullptr;
    const std::string sql =
        "SELECT kd.id, kd.doc_type, kd.title, kd.content, kc.content "
        "FROM riskai_knowledge_doc kd "
        "JOIN riskai_knowledge_chunk kc ON kc.doc_id = kd.id;";
    if (sqlite3_prepare_v2(db_.handle(), sql.c_str(), -1, &stmt, nullptr) !=
        SQLITE_OK) {
        return common::Result<std::vector<KnowledgeChunk>>::err(
            "prepare failed: " + std::string(sqlite3_errmsg(db_.handle())));
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const std::string docId = colText(stmt, 0);
        KnowledgeDocType type = docTypeFromName(colText(stmt, 1));
        std::string haystack = colText(stmt, 2) + " " + colText(stmt, 3) + " " +
                               colText(stmt, 4);
        std::string hl = lower(haystack);
        int score = 0;
        for (const auto& q : qtokens) {
            std::size_t pos = 0;
            while ((pos = hl.find(q, pos)) != std::string::npos) {
                ++score;
                pos += q.size();
            }
        }
        if (score == 0) continue;
        score *= typeWeight(type);
        KnowledgeChunk c;
        c.docId = docId;
        c.content = colText(stmt, 4);
        c.score = score;
        out.push_back(std::move(c));
    }
    sqlite3_finalize(stmt);

    // Stable sort by score desc (ties broken by docId for determinism).
    std::stable_sort(out.begin(), out.end(),
                     [](const KnowledgeChunk& a, const KnowledgeChunk& b) {
                         if (a.score != b.score) return a.score > b.score;
                         return a.docId < b.docId;
                     });
    if ((int)out.size() > limit) out.resize((std::size_t)limit);
    return common::Result<std::vector<KnowledgeChunk>>::ok(std::move(out));
}

std::string KnowledgeService::buildContext(
    const std::vector<KnowledgeChunk>& chunks) const {
    std::ostringstream out;
    int n = 0;
    for (const auto& c : chunks) {
        if (n) out << "\n";
        out << "[" << (++n) << "] " << c.content;
    }
    return out.str();
}

}  // namespace lodestar::riskai
