// core/riskai/AuthoringScoringService.cpp
// Gap-Fill RiskAI 1.7: inline requirement-quality scoring in TraceLink.

#include "core/riskai/AuthoringScoringService.h"

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

int colInt(sqlite3_stmt* stmt, int col) { return sqlite3_column_int(stmt, col); }

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

}  // namespace

AuthoringScoringService::AuthoringScoringService(persistence::Database& db)
    : db_(db) {}

common::QualityResult AuthoringScoringService::score(const tracelink::Entity& req) {
    std::string body = req.text.empty() ? req.name : req.text;
    return common::scoreQualityWithFlags(body);
}

common::Result<int> AuthoringScoringService::scoreOnSave(
    const tracelink::Entity& req) {
    if (req.id.empty()) {
        return common::Result<int>::err(common::ErrorCode::InvalidArgument,
                                        "entity id must not be empty");
    }
    auto q = score(req);
    const std::string id = newUuid();
    auto res = exec(db_.handle(),
                    "INSERT INTO authoring_quality_score "
                    "(id, entity_id, clarity, testability, atomicity, "
                    " completeness, ambiguity, overall, created_at) "
                    "VALUES (?,?,?,?,?,?,?,?,?);",
                    {id, req.id,
                     std::to_string(q.score.clarity),
                     std::to_string(q.score.testability),
                     std::to_string(q.score.atomicity),
                     std::to_string(q.score.completeness),
                     std::to_string(q.score.ambiguity),
                     std::to_string(q.score.overall), nowIso()});
    if (res.failed()) {
        return common::Result<int>::err(res.error());
    }
    return common::Result<int>::ok(q.score.overall);
}

common::Result<std::optional<common::QualityScore>>
AuthoringScoringService::lastScore(const std::string& entityId) {
    sqlite3_stmt* stmt = nullptr;
    const std::string sql =
        "SELECT clarity, testability, atomicity, completeness, ambiguity, overall "
        "FROM authoring_quality_score WHERE entity_id=? "
        "ORDER BY created_at DESC, rowid DESC LIMIT 1;";
    if (sqlite3_prepare_v2(db_.handle(), sql.c_str(), -1, &stmt, nullptr) !=
        SQLITE_OK) {
        return common::Result<std::optional<common::QualityScore>>::err(
            "prepare failed: " + std::string(sqlite3_errmsg(db_.handle())));
    }
    bindText(stmt, 1, entityId);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return common::Result<std::optional<common::QualityScore>>::ok(
            std::nullopt);
    }
    common::QualityScore s;
    s.clarity = colInt(stmt, 0);
    s.testability = colInt(stmt, 1);
    s.atomicity = colInt(stmt, 2);
    s.completeness = colInt(stmt, 3);
    s.ambiguity = colInt(stmt, 4);
    s.overall = colInt(stmt, 5);
    sqlite3_finalize(stmt);
    return common::Result<std::optional<common::QualityScore>>::ok(s);
}

}  // namespace lodestar::riskai
