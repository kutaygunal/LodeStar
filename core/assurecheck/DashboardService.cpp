// core/assurecheck/DashboardService.cpp
// Phase 11 WP-6 (AssureCheck): compliance dashboard data implementation.

#include "core/assurecheck/DashboardService.h"

#include <cstdlib>
#include <string>
#include <vector>

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

DashboardService::DashboardService(persistence::Database& db) : db_(db) {}

common::Result<std::vector<DashboardStandard>> DashboardService::dashboard() {
    sqlite3* db = db_.handle();
    if (db == nullptr) {
        return common::Result<std::vector<DashboardStandard>>::err(
            "database not open");
    }

    // Aggregate stored results per standard, ordered by code. Only standards
    // that have at least one stored result are returned.
    auto rows = query(db,
                      "SELECT s.code, s.name, c.status, COUNT(*) "
                      "FROM assurance_checks c "
                      "JOIN assurance_standards s ON s.id = c.standard_id "
                      "GROUP BY s.code, c.status "
                      "ORDER BY s.code;",
                      {}, 4);
    if (rows.failed()) {
        return common::Result<std::vector<DashboardStandard>>::err(
            rows.error());
    }

    std::vector<DashboardStandard> out;
    std::string curCode;
    for (const auto& r : rows.value()) {
        const std::string& code = r[0];
        const std::string& name = r[1];
        const std::string& status = r[2];
        const int n = std::atoi(r[3].c_str());

        if (code != curCode) {
            DashboardStandard st;
            st.code = code;
            st.name = name;
            out.push_back(std::move(st));
            curCode = code;
        }

        DashboardStandard& st = out.back();
        st.coverage.total += n;
        if (status == "PASS") st.coverage.pass += n;
        else if (status == "FAIL") st.coverage.fail += n;
        else if (status == "NA") st.coverage.na += n;
        else if (status == "WARNING") st.coverage.warning += n;
    }

    for (auto& st : out) {
        st.coverage.applicable = st.coverage.total - st.coverage.na;
        st.coverage.percent =
            st.coverage.applicable > 0
                ? (st.coverage.pass * 100 / st.coverage.applicable)
                : 0;
    }

    return common::Result<std::vector<DashboardStandard>>::ok(std::move(out));
}

}  // namespace lodestar::assurecheck
