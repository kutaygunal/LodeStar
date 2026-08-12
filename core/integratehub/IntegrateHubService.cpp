// core/integratehub/IntegrateHubService.cpp
// Sprint 1 Phase 4 (IntegrateHub): cross-disciplinary issue/coordination model.

#include "core/integratehub/IntegrateHubService.h"

#include <cstdlib>
#include <string>
#include <vector>

#include <sqlite3.h>

#include "core/common/Time.h"
#include "core/common/Uuid.h"

namespace lodestar::integratehub {

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

common::Result<std::vector<std::vector<std::string>>> query(
    sqlite3* db, const std::string& sql, const std::vector<std::string>& params,
    int ncols) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<std::vector<std::vector<std::string>>>::err(
            "prepare failed: " + std::string(sqlite3_errmsg(db)));
    }
    for (size_t i = 0; i < params.size(); ++i) {
        bindText(stmt, static_cast<int>(i + 1), params[i]);
    }
    std::vector<std::vector<std::string>> out;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::vector<std::string> row;
        for (int c = 0; c < ncols; ++c) row.push_back(colText(stmt, c));
        out.push_back(std::move(row));
    }
    sqlite3_finalize(stmt);
    return common::Result<std::vector<std::vector<std::string>>>::ok(
        std::move(out));
}

std::string disciplineName(Discipline d) {
    switch (d) {
        case Discipline::Systems: return "Systems";
        case Discipline::Software: return "Software";
        case Discipline::Hardware: return "Hardware";
        case Discipline::Test: return "Test";
        case Discipline::Safety: return "Safety";
    }
    return "Systems";
}

Discipline disciplineFromName(const std::string& name) {
    if (name == "Software") return Discipline::Software;
    if (name == "Hardware") return Discipline::Hardware;
    if (name == "Test") return Discipline::Test;
    if (name == "Safety") return Discipline::Safety;
    return Discipline::Systems;
}

}  // namespace

IntegrateHubService::IntegrateHubService(persistence::Database& db) : db_(db) {}

common::Result<std::string> IntegrateHubService::createIssue(const Issue& issue) {
    if (issue.title.empty()) {
        return common::Result<std::string>::err(
            common::ErrorCode::InvalidArgument, "issue title must not be empty");
    }
    const std::string id = newUuid();
    const std::string status =
        issue.status.empty() ? "open" : issue.status;
    const std::string createdAt =
        issue.createdAt.empty() ? nowIso() : issue.createdAt;
    auto res = exec(db_.handle(),
                    "INSERT INTO integratehub_issues "
                    "(id, title, description, owner, status, created_at) "
                    "VALUES (?,?,?,?,?,?);",
                    {id, issue.title, issue.description,
                     disciplineName(issue.owner), status, createdAt});
    if (res.failed()) {
        return common::Result<std::string>::err(res.error());
    }
    return common::Result<std::string>::ok(id);
}

common::Result<std::vector<Issue>> IntegrateHubService::listIssues(
    Discipline d) {
    // Same tiebreaker reasoning as coordinationFor() below: created_at alone
    // doesn't distinguish rows created within the same nowIso() second.
    auto rows = query(db_.handle(),
                      "SELECT id, title, description, owner, status, created_at "
                      "FROM integratehub_issues WHERE owner=? "
                      "ORDER BY created_at, rowid;",
                      {disciplineName(d)}, 6);
    if (rows.failed()) {
        return common::Result<std::vector<Issue>>::err(rows.error());
    }
    std::vector<Issue> out;
    for (const auto& r : rows.value()) {
        Issue it;
        it.id = r[0];
        it.title = r[1];
        it.description = r[2];
        it.owner = disciplineFromName(r[3]);
        it.status = r[4];
        it.createdAt = r[5];
        out.push_back(std::move(it));
    }
    return common::Result<std::vector<Issue>>::ok(std::move(out));
}

common::Result<void> IntegrateHubService::setStatus(const std::string& issueId,
                                                    const std::string& status) {
    if (issueId.empty()) {
        return common::Result<void>::err(common::ErrorCode::InvalidArgument,
                                         "issueId must not be empty");
    }
    if (status.empty()) {
        return common::Result<void>::err(common::ErrorCode::InvalidArgument,
                                         "status must not be empty");
    }
    auto res = exec(db_.handle(),
                    "UPDATE integratehub_issues SET status=? WHERE id=?;",
                    {status, issueId});
    if (res.failed()) {
        return common::Result<void>::err(res.error());
    }
    return common::Result<void>::ok();
}

common::Result<std::string> IntegrateHubService::addCoordination(
    const std::string& issueId, const std::string& note) {
    if (issueId.empty()) {
        return common::Result<std::string>::err(
            common::ErrorCode::InvalidArgument, "issueId must not be empty");
    }
    const std::string id = newUuid();
    const std::string createdAt = nowIso();
    auto res = exec(db_.handle(),
                    "INSERT INTO integratehub_coordination "
                    "(id, issue_id, note, created_at) VALUES (?,?,?,?);",
                    {id, issueId, note, createdAt});
    if (res.failed()) {
        return common::Result<std::string>::err(res.error());
    }
    return common::Result<std::string>::ok(id);
}

common::Result<std::vector<Coordination>>
IntegrateHubService::coordinationFor(const std::string& issueId) {
    // Order by created_at first, then by rowid (SQLite's implicit,
    // monotonically-increasing insertion-order column) as the tiebreaker -
    // matches the ordering documented in migration 022. `id` (a UUID) is
    // *not* a valid tiebreaker: it doesn't correlate with insertion order,
    // so two notes added within the same nowIso() second (a very common
    // case for fast test/automation code) would sort randomly rather than
    // oldest-first.
    auto rows = query(db_.handle(),
                      "SELECT id, issue_id, note, created_at "
                      "FROM integratehub_coordination WHERE issue_id=? "
                      "ORDER BY created_at, rowid;",
                      {issueId}, 4);
    if (rows.failed()) {
        return common::Result<std::vector<Coordination>>::err(rows.error());
    }
    std::vector<Coordination> out;
    for (const auto& r : rows.value()) {
        Coordination c;
        c.id = r[0];
        c.issueId = r[1];
        c.note = r[2];
        c.createdAt = r[3];
        out.push_back(std::move(c));
    }
    return common::Result<std::vector<Coordination>>::ok(std::move(out));
}

}  // namespace lodestar::integratehub
