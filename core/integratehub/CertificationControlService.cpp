// core/integratehub/CertificationControlService.cpp
// Gap-Fill IntegrateHub 6.2: integration with certification control.

#include "core/integratehub/CertificationControlService.h"

#include <sqlite3.h>

namespace lodestar::integratehub {

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

}  // namespace

CertificationControlService::CertificationControlService(persistence::Database& db)
    : db_(db) {}

common::Result<std::vector<CertControlRow>>
CertificationControlService::configControlView() {
    // Every CR, optionally joined with its PR, plus its impact counts.
    auto rows = query(
        db_.handle(),
        "SELECT cr.id, cr.title, cr.status, cr.pr_id, pr.title, pr.status, "
        "       (SELECT COUNT(*) FROM integratehub_impact i WHERE i.cr_id=cr.id), "
        "       (SELECT COUNT(*) FROM integratehub_impact i WHERE i.cr_id=cr.id "
        "         AND i.risk_unverified=1) "
        "FROM integratehub_cr cr "
        "LEFT JOIN integratehub_pr pr ON pr.id = cr.pr_id "
        "ORDER BY cr.created_at, cr.id;",
        {}, 8);
    if (rows.failed()) {
        return common::Result<std::vector<CertControlRow>>::err(rows.error());
    }
    std::vector<CertControlRow> out;
    for (const auto& r : rows.value()) {
        CertControlRow row;
        row.crId = r[0];
        row.crTitle = r[1];
        row.crStatus = r[2];
        row.prId = r[3];
        row.prTitle = r[4];
        row.prStatus = r[5];
        try {
            row.impactCount = std::stoi(r[6]);
        } catch (...) { row.impactCount = 0; }
        try {
            row.unverifiedImpact = std::stoi(r[7]);
        } catch (...) { row.unverifiedImpact = 0; }
        out.push_back(std::move(row));
    }
    return common::Result<std::vector<CertControlRow>>::ok(std::move(out));
}

common::Result<std::string> CertificationControlService::emitPrCrLog() {
    auto view = configControlView();
    if (view.failed()) {
        return common::Result<std::string>::err(view.error());
    }
    std::string log =
        "CERTIFICATION CONFIGURATION CONTROL LOG\n"
        "========================================\n";
    log += "Change requests: " + std::to_string(view.value().size()) + "\n\n";
    for (const auto& r : view.value()) {
        log += "[CR] " + r.crId + " \"" + r.crTitle + "\" [" + r.crStatus + "]\n";
        if (!r.prId.empty()) {
            log += "   linked PR: " + r.prId + " \"" + r.prTitle + "\" [" +
                   r.prStatus + "]\n";
        }
        log += "   impact: " + std::to_string(r.impactCount) + " item(s), " +
               std::to_string(r.unverifiedImpact) + " unverified\n";
    }
    return common::Result<std::string>::ok(std::move(log));
}

}  // namespace lodestar::integratehub
