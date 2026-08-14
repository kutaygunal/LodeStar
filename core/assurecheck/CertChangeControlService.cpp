// core/assurecheck/CertChangeControlService.cpp
// Gap-Fill AssureCheck 2.4: certification change/impact control.

#include "core/assurecheck/CertChangeControlService.h"

#include <sqlite3.h>

namespace lodestar::assurecheck {

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

// Classify an impacted entity type as HLR / LLR / test / design for DO-178C.
std::string classify(const std::string& type) {
    if (type == "requirement") return "HLR/LLR";
    if (type == "test_case") return "test";
    if (type == "design") return "design";
    return type;
}

}  // namespace

CertChangeControlService::CertChangeControlService(persistence::Database& db)
    : db_(db) {}

common::Result<std::vector<CertChangeRow>>
CertChangeControlService::configurationControlView() {
    // Join every CR with its impact rows (and optional PR).
    auto rows = query(
        db_.handle(),
        "SELECT cr.id, cr.title, cr.status, cr.pr_id, "
        "       imp.target_type, imp.target_id, imp.risk_unverified "
        "FROM integratehub_cr cr "
        "LEFT JOIN integratehub_impact imp ON imp.cr_id = cr.id "
        "ORDER BY cr.created_at, cr.id, imp.target_type, imp.target_id;",
        {}, 7);
    if (rows.failed()) {
        return common::Result<std::vector<CertChangeRow>>::err(rows.error());
    }
    std::vector<CertChangeRow> out;
    for (const auto& r : rows.value()) {
        CertChangeRow row;
        row.crId = r[0];
        row.crTitle = r[1];
        row.crStatus = r[2];
        row.prId = r[3];
        row.impactedType = classify(r[4]);
        row.impactedId = r[5];
        try { row.unverifiedImpact = std::stoi(r[6]); } catch (...) { row.unverifiedImpact = 0; }
        // Approval state per baseline: Approved only when the CR is approved and
        // the impact is verified.
        if (r[2] == "Approved" && row.unverifiedImpact == 0) row.approvalState = "Approved";
        else if (r[2] == "Rejected") row.approvalState = "Rejected";
        else row.approvalState = "Pending";
        row.baseline = "current";
        out.push_back(std::move(row));
    }
    return common::Result<std::vector<CertChangeRow>>::ok(std::move(out));
}

common::Result<std::string>
CertChangeControlService::emitChangeImpactReport() {
    auto view = configurationControlView();
    if (view.failed()) {
        return common::Result<std::string>::err(view.error());
    }
    std::string report =
        "DO-178C CHANGE IMPACT REPORT\n"
        "============================\n";
    report += "Change items: " + std::to_string(view.value().size()) + "\n\n";
    for (const auto& r : view.value()) {
        report += "[CR] " + r.crId + " \"" + r.crTitle + "\" [" + r.crStatus +
                  "] approval=" + r.approvalState + "\n";
        if (!r.prId.empty()) report += "   linked PR: " + r.prId + "\n";
        report += "   impacted " + r.impactedType + ": " + r.impactedId +
                  " (unverified=" +
                  std::to_string(r.unverifiedImpact) + ")\n";
    }
    return common::Result<std::string>::ok(std::move(report));
}

}  // namespace lodestar::assurecheck
