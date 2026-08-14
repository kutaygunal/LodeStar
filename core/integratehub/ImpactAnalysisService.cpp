// core/integratehub/ImpactAnalysisService.cpp
// Gap-Fill IntegrateHub 6.1: Problem Report -> Change Request -> impact analysis.

#include "core/integratehub/ImpactAnalysisService.h"

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

}  // namespace

ImpactAnalysisService::ImpactAnalysisService(persistence::Database& db)
    : db_(db) {}

// --- PR workflow ----------------------------------------------------------

common::Result<std::string> ImpactAnalysisService::createPr(
    const ProblemReport& pr) {
    if (pr.title.empty()) {
        return common::Result<std::string>::err(
            common::ErrorCode::InvalidArgument, "PR title must not be empty");
    }
    const std::string id = newUuid();
    const std::string now = nowIso();
    auto res = exec(db_.handle(),
                    "INSERT INTO integratehub_pr "
                    "(id, title, description, severity, status, reported_by, "
                    " approval_authority, created_at, updated_at) "
                    "VALUES (?,?,?,?,?,?,?,?,?);",
                    {id, pr.title, pr.description, pr.severity, pr.status,
                     pr.reportedBy, pr.approvalAuthority, now, now});
    if (res.failed()) {
        return common::Result<std::string>::err(res.error());
    }
    return common::Result<std::string>::ok(id);
}

common::Result<std::vector<ProblemReport>> ImpactAnalysisService::listPrs() {
    std::vector<ProblemReport> out;
    auto rows = query(db_.handle(),
                      "SELECT id, title, description, severity, status, "
                      " reported_by, approval_authority, created_at, updated_at "
                      "FROM integratehub_pr ORDER BY created_at, rowid;",
                      {}, 9);
    if (rows.failed()) {
        return common::Result<std::vector<ProblemReport>>::err(rows.error());
    }
    for (const auto& r : rows.value()) {
        ProblemReport p;
        p.id = r[0]; p.title = r[1]; p.description = r[2]; p.severity = r[3];
        p.status = r[4]; p.reportedBy = r[5]; p.approvalAuthority = r[6];
        p.createdAt = r[7]; p.updatedAt = r[8];
        out.push_back(std::move(p));
    }
    return common::Result<std::vector<ProblemReport>>::ok(std::move(out));
}

common::Result<ProblemReport> ImpactAnalysisService::transitionPr(
    const std::string& id, const std::string& to) {
    auto rows = query(db_.handle(),
                      "SELECT id, title, description, severity, status, "
                      " reported_by, approval_authority, created_at, updated_at "
                      "FROM integratehub_pr WHERE id=?;",
                      {id}, 9);
    if (rows.failed()) {
        return common::Result<ProblemReport>::err(rows.error());
    }
    if (rows.value().empty()) {
        return common::Result<ProblemReport>::err(
            common::ErrorCode::NotFound, "PR not found");
    }
    const auto& r = rows.value()[0];
    ProblemReport p;
    p.id = r[0]; p.title = r[1]; p.description = r[2]; p.severity = r[3];
    p.status = r[4]; p.reportedBy = r[5]; p.approvalAuthority = r[6];
    p.createdAt = r[7]; p.updatedAt = r[8];

    // Legal transitions: open -> under_investigation -> resolved -> closed.
    static const std::vector<std::pair<std::string, std::string>> allowed = {
        {"open", "under_investigation"},
        {"under_investigation", "resolved"},
        {"resolved", "closed"}};
    bool legal = false;
    for (const auto& a : allowed) {
        if (a.first == p.status && a.second == to) { legal = true; break; }
    }
    if (!legal) {
        return common::Result<ProblemReport>::err(
            common::ErrorCode::IllegalTransition,
            "illegal PR transition " + p.status + " -> " + to);
    }
    // Closing requires an approval authority.
    if (to == "closed" && p.approvalAuthority.empty()) {
        return common::Result<ProblemReport>::err(
            common::ErrorCode::ValidationFailed,
            "cannot close PR: approval authority is not assigned");
    }
    const std::string now = nowIso();
    auto up = exec(db_.handle(),
                   "UPDATE integratehub_pr SET status=?, updated_at=? WHERE id=?;",
                   {to, now, id});
    if (up.failed()) {
        return common::Result<ProblemReport>::err(up.error());
    }
    p.status = to;
    p.updatedAt = now;
    return common::Result<ProblemReport>::ok(p);
}

// --- CR -------------------------------------------------------------------

common::Result<std::string> ImpactAnalysisService::createCr(
    const ChangeRequest& cr) {
    if (cr.title.empty() && cr.entityId.empty()) {
        return common::Result<std::string>::err(
            common::ErrorCode::InvalidArgument,
            "CR must have a title or a target entity");
    }
    const std::string id = newUuid();
    // pr_id may be empty (a CR need not be linked to a PR). Bind NULL only for
    // pr_id so the foreign key is satisfied; other NOT NULL text columns keep
    // their (possibly empty) string value.
    sqlite3_stmt* stmt = nullptr;
    const std::string sql =
        "INSERT INTO integratehub_cr "
        "(id, pr_id, title, description, status, entity_type, "
        " entity_id, proposed_change, created_by, created_at) "
        "VALUES (?,?,?,?,?,?,?,?,?,?);";
    if (sqlite3_prepare_v2(db_.handle(), sql.c_str(), -1, &stmt, nullptr) !=
        SQLITE_OK) {
        return common::Result<std::string>::err(
            "prepare failed: " + std::string(sqlite3_errmsg(db_.handle())));
    }
    const std::vector<std::string> params{
        id, cr.prId, cr.title, cr.description, cr.status, cr.entityType,
        cr.entityId, cr.proposedChange, cr.createdBy, nowIso()};
    for (size_t i = 0; i < params.size(); ++i) {
        // Param index 1 (0-based) is pr_id -> NULL when empty.
        if (i == 1 && params[i].empty()) {
            sqlite3_bind_null(stmt, 2);
        } else {
            bindText(stmt, static_cast<int>(i + 1), params[i]);
        }
    }
    int rc = sqlite3_step(stmt);
    std::string msg = sqlite3_errmsg(db_.handle());
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return common::Result<std::string>::err("step failed: " + msg);
    }
    return common::Result<std::string>::ok(id);
}

common::Result<std::vector<ChangeRequest>> ImpactAnalysisService::listCrs() {
    std::vector<ChangeRequest> out;
    auto rows = query(db_.handle(),
                      "SELECT id, pr_id, title, description, status, "
                      " entity_type, entity_id, proposed_change, created_by, "
                      " created_at FROM integratehub_cr ORDER BY created_at, rowid;",
                      {}, 10);
    if (rows.failed()) {
        return common::Result<std::vector<ChangeRequest>>::err(rows.error());
    }
    for (const auto& r : rows.value()) {
        ChangeRequest c;
        c.id = r[0]; c.prId = r[1]; c.title = r[2]; c.description = r[3];
        c.status = r[4]; c.entityType = r[5]; c.entityId = r[6];
        c.proposedChange = r[7]; c.createdBy = r[8]; c.createdAt = r[9];
        out.push_back(std::move(c));
    }
    return common::Result<std::vector<ChangeRequest>>::ok(std::move(out));
}

// --- Impact analysis ------------------------------------------------------

common::Result<std::vector<ImpactItem>> ImpactAnalysisService::analyzeImpact(
    const std::string& crId) {
    auto crs = query(db_.handle(),
                     "SELECT id, entity_type, entity_id, title FROM "
                     "integratehub_cr WHERE id=?;", {crId}, 4);
    if (crs.failed()) {
        return common::Result<std::vector<ImpactItem>>::err(crs.error());
    }
    if (crs.value().empty()) {
        return common::Result<std::vector<ImpactItem>>::err(
            common::ErrorCode::NotFound, "CR not found");
    }
    const auto& c = crs.value()[0];
    const std::string entityType = c[1];
    const std::string entityId = c[2];

    std::vector<ImpactItem> impact;
    auto add = [&](const std::string& type, const std::string& tid,
                   bool unverified, const std::string& detail) {
        // De-dup.
        for (const auto& it : impact) {
            if (it.targetType == type && it.targetId == tid) return;
        }
        ImpactItem item;
        item.targetType = type;
        item.targetId = tid;
        item.riskUnverified = unverified;
        item.detail = detail;
        impact.push_back(std::move(item));
    };

    // The direct target entity.
    if (!entityType.empty() && !entityId.empty()) {
        add(entityType, entityId, false, "direct change target");
    }

    // TraceLink graph: requirements/design/tests linked to the target. The
    // graph is queried via the trace_links table (source/target entity ids).
    auto links = query(
        db_.handle(),
        "SELECT source_type, source_id, target_type, target_id "
        "FROM trace_links WHERE (source_id=? OR target_id=?) "
        "AND status='Active';",
        {entityId, entityId}, 4);
    if (links.isOk()) {
        for (const auto& l : links.value()) {
            add(l[0], l[1], false, "linked via trace graph");
            add(l[2], l[3], false, "linked via trace graph");
        }
    }

    // Baselines referencing the entity. (Baseline membership is in the
    // baseline_items table; if present, surface those. Otherwise honest.)
    auto baselines = query(
        db_.handle(),
        "SELECT baseline_id FROM baseline_items WHERE item_id=?;", {entityId}, 1);
    if (baselines.isOk()) {
        for (const auto& b : baselines.value()) {
            add("baseline", b[0], false, "in baseline");
        }
    }

    // Unverified risk: if the CR has no verified (passed) test run for the
    // target, flag the impact as unverified (risk of unverified impact).
    // A test case is verified when test_run_coverage has a passed run for it.
    auto verifiedRuns = query(
        db_.handle(),
        "SELECT COUNT(*) FROM test_run_coverage WHERE test_case_id=? AND passed=1;",
        {entityId}, 1);
    bool hasVerified = false;
    if (verifiedRuns.isOk() && !verifiedRuns.value().empty()) {
        try { hasVerified = std::stoi(verifiedRuns.value()[0][0]) > 0; }
        catch (...) { hasVerified = false; }
    }
    for (auto& it : impact) {
        if (it.targetType == "requirement" || it.targetType == "design" ||
            it.targetType == "test_case") {
            if (!hasVerified) it.riskUnverified = true;
        }
    }

    // Persist impact rows.
    for (const auto& it : impact) {
        exec(db_.handle(),
             "INSERT INTO integratehub_impact (id, cr_id, target_type, "
             " target_id, risk_unverified, detail) VALUES (?,?,?,?,?,?);",
             {newUuid(), crId, it.targetType, it.targetId,
              it.riskUnverified ? "1" : "0", it.detail});
    }
    return common::Result<std::vector<ImpactItem>>::ok(std::move(impact));
}

common::Result<void> ImpactAnalysisService::checkApprovalGate(
    const std::string& crId) {
    auto impact = analyzeImpact(crId);
    if (impact.failed()) {
        return common::Result<void>::err(impact.error());
    }
    // Approval gating: a CR with unverified high-risk impact may not approve.
    for (const auto& it : impact.value()) {
        if (it.riskUnverified) {
            return common::Result<void>::err(
                common::ErrorCode::ValidationFailed,
                "CR has unverified impact on " + it.targetType + " " +
                    it.targetId + "; resolve before approving");
        }
    }
    return common::Result<void>::ok();
}

}  // namespace lodestar::integratehub
