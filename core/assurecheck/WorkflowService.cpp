// core/assurecheck/WorkflowService.cpp
// S2 Phase 3 (AssureCheck): review/approval/sign-off workflow + audit trail +
// objective->evidence package implementation.

#include "core/assurecheck/WorkflowService.h"

#include <string>
#include <vector>

#include <sqlite3.h>

#include "core/common/Time.h"
#include "core/common/Uuid.h"

namespace lodestar::assurecheck {

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

// Parses "type:id;type:id" evidence text into EvidenceLink list.
std::vector<EvidenceLink> evidenceFromString(const std::string& s) {
    std::vector<EvidenceLink> out;
    std::string cur;
    for (char c : s) {
        if (c == ';') {
            const size_t colon = cur.find(':');
            if (colon != std::string::npos) {
                EvidenceLink e;
                e.entityType = cur.substr(0, colon);
                e.entityId = cur.substr(colon + 1);
                out.push_back(std::move(e));
            }
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) {
        const size_t colon = cur.find(':');
        if (colon != std::string::npos) {
            EvidenceLink e;
            e.entityType = cur.substr(0, colon);
            e.entityId = cur.substr(colon + 1);
            out.push_back(std::move(e));
        }
    }
    return out;
}

}  // namespace

WorkflowService::WorkflowService(persistence::Database& db) : db_(db) {}

common::Result<void> WorkflowService::submitForReview(
    const std::string& resultId, const std::string& actor) {
    return transition(resultId, actor, "submit", "draft", "in_review");
}

common::Result<void> WorkflowService::approve(const std::string& resultId,
                                              const std::string& actor) {
    return transition(resultId, actor, "approve", "in_review", "approved");
}

common::Result<void> WorkflowService::reject(const std::string& resultId,
                                             const std::string& actor) {
    return transition(resultId, actor, "reject", "in_review", "rejected");
}

common::Result<void> WorkflowService::transition(
    const std::string& resultId, const std::string& actor,
    const std::string& action, const std::string& fromState,
    const std::string& toState) {
    sqlite3* db = db_.handle();
    if (db == nullptr) {
        return common::Result<void>::err("database not open");
    }

    // Current state of the result.
    auto cur = query(db,
                     "SELECT workflow_state FROM assurance_checks WHERE id=?;",
                     {resultId}, 1);
    if (cur.failed()) {
        return common::Result<void>::err(cur.error());
    }
    if (cur.value().empty()) {
        return common::Result<void>::err("result not found: " + resultId);
    }
    const std::string current = cur.value().front()[0];
    if (current != fromState) {
        return common::Result<void>::err(
            "illegal transition from '" + current + "' to '" + toState + "'");
    }

    const std::string ts = nowIso();

    // Update the result's workflow columns.
    std::string update;
    if (action == "submit") {
        update = "UPDATE assurance_checks SET workflow_state=?, reviewed_by=?, "
                 "reviewed_at=? WHERE id=?;";
    } else if (action == "approve") {
        update = "UPDATE assurance_checks SET workflow_state=?, approved_by=?, "
                 "approved_at=? WHERE id=?;";
    } else {  // reject
        update = "UPDATE assurance_checks SET workflow_state=?, rejected_by=?, "
                 "rejected_at=? WHERE id=?;";
    }
    auto up = exec(db, update, {toState, actor, ts, resultId});
    if (up.failed()) return up;

    // Append an audit entry.
    auto ins = exec(db,
                    "INSERT INTO assurance_workflow_audit "
                    "(id, result_id, actor, action, target, timestamp, "
                    "from_state, to_state) VALUES (?,?,?,?,?,?,?,?);",
                    {newUuid(), resultId, actor, action, resultId, ts,
                     fromState, toState});
    if (ins.failed()) return ins;

    return common::Result<void>::ok();
}

common::Result<std::string> WorkflowService::stateFor(
    const std::string& resultId) {
    sqlite3* db = db_.handle();
    if (db == nullptr) {
        return common::Result<std::string>::err("database not open");
    }
    auto rows = query(db,
                      "SELECT workflow_state FROM assurance_checks WHERE id=?;",
                      {resultId}, 1);
    if (rows.failed()) {
        return common::Result<std::string>::err(rows.error());
    }
    if (rows.value().empty()) {
        return common::Result<std::string>::ok(std::string());
    }
    return common::Result<std::string>::ok(rows.value().front()[0]);
}

common::Result<std::vector<AuditEntry>> WorkflowService::auditLog(
    const std::string& resultId) {
    sqlite3* db = db_.handle();
    if (db == nullptr) {
        return common::Result<std::vector<AuditEntry>>::err(
            "database not open");
    }
    auto rows = query(db,
                      "SELECT id, result_id, actor, action, target, timestamp, "
                      "from_state, to_state FROM assurance_workflow_audit "
                      "WHERE result_id=? ORDER BY timestamp;",
                      {resultId}, 8);
    if (rows.failed()) {
        return common::Result<std::vector<AuditEntry>>::err(rows.error());
    }
    std::vector<AuditEntry> out;
    for (const auto& r : rows.value()) {
        AuditEntry e;
        e.id = r[0];
        e.resultId = r[1];
        e.actor = r[2];
        e.action = r[3];
        e.target = r[4];
        e.timestamp = r[5];
        e.fromState = r[6];
        e.toState = r[7];
        out.push_back(std::move(e));
    }
    return common::Result<std::vector<AuditEntry>>::ok(std::move(out));
}

common::Result<EvidencePackage> WorkflowService::buildEvidencePackage(
    const std::string& objectiveId) {
    sqlite3* db = db_.handle();
    if (db == nullptr) {
        return common::Result<EvidencePackage>::err("database not open");
    }
    auto rows = query(db,
                      "SELECT evidence FROM assurance_checks WHERE item_id=?;",
                      {objectiveId}, 1);
    if (rows.failed()) {
        return common::Result<EvidencePackage>::err(rows.error());
    }
    if (rows.value().empty()) {
        return common::Result<EvidencePackage>::err(
            "no check result for objective: " + objectiveId);
    }
    EvidencePackage pkg;
    pkg.objectiveId = objectiveId;
    pkg.links = evidenceFromString(rows.value().front()[0]);
    return common::Result<EvidencePackage>::ok(std::move(pkg));
}

}  // namespace lodestar::assurecheck
