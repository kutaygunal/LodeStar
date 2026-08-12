#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

// core/tracelink/CoverageService.cpp
// WP-5: wires TestForge test runs into live traceability coverage. See the
// header for the contract. The mapping table test_run_coverage (migration 017)
// records which run executed a test case; the latest recorded run per test
// case governs whether the requirement it verifies is `verified`.

#include "core/tracelink/CoverageService.h"

#include <ctime>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <sqlite3.h>

#include "core/common/Uuid.h"
#include "core/tracelink/TraceLinkService.h"
#include "core/tracelink/Types.h"

namespace lodestar::tracelink {

namespace {

std::string now() {
    char buf[32];
    const auto t = std::time(nullptr);
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
    return std::string(buf);
}

void bindText(sqlite3_stmt* stmt, int index, const std::string& value) {
    sqlite3_bind_text(stmt, index, value.c_str(), static_cast<int>(value.size()),
                      SQLITE_TRANSIENT);
}

std::string columnText(sqlite3_stmt* stmt, int col) {
    const unsigned char* text = sqlite3_column_text(stmt, col);
    return text ? reinterpret_cast<const char*>(text) : std::string();
}

int columnInt(sqlite3_stmt* stmt, int col) {
    return sqlite3_column_int(stmt, col);
}

}  // namespace

CoverageService::CoverageService(persistence::Database& db) : db_(db) {}

common::Result<void> CoverageService::recordRun(const std::string& runId,
                                                const std::string& testCaseId,
                                                bool passed) {
    if (runId.empty() || testCaseId.empty()) {
        return common::Result<void>::err(common::ErrorCode::InvalidArgument,
                                         "runId and testCaseId must be non-empty");
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO test_run_coverage (id, run_id, test_case_id, passed, executed_at)"
        " VALUES (?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<void>::err(
            "prepare test_run_coverage insert failed: " +
            std::string(sqlite3_errmsg(db_.handle())));
    }
    bindText(stmt, 1, common::newUuid());
    bindText(stmt, 2, runId);
    bindText(stmt, 3, testCaseId);
    sqlite3_bind_int(stmt, 4, passed ? 1 : 0);
    bindText(stmt, 5, now());
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return common::Result<void>::err(
            "insert test_run_coverage failed: " +
            std::string(sqlite3_errmsg(db_.handle())));
    }
    return common::Result<void>::ok();
}

common::Result<std::vector<ExecutedCoverageRow>> CoverageService::executedCoverage() {
    TraceLinkService svc(db_);

    auto reqsRes = svc.listEntities(EntityType::Requirement, EntityFilter{});
    if (reqsRes.failed()) {
        return common::Result<std::vector<ExecutedCoverageRow>>::err(reqsRes.error());
    }
    auto linksRes = svc.allLinks();
    if (linksRes.failed()) {
        return common::Result<std::vector<ExecutedCoverageRow>>::err(linksRes.error());
    }

    // Test cases that have at least one recorded run (executed set).
    std::set<std::string> executedTestCases;
    // test_case_id -> latest recorded run's passed flag (rowid governs order).
    std::map<std::string, bool> latestPassed;

    sqlite3_stmt* stmt = nullptr;
    const char* distinctSql = "SELECT DISTINCT test_case_id FROM test_run_coverage;";
    if (sqlite3_prepare_v2(db_.handle(), distinctSql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            executedTestCases.insert(columnText(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }

    const char* latestSql =
        "SELECT test_case_id, passed FROM test_run_coverage"
        " WHERE rowid = (SELECT MAX(rowid) FROM test_run_coverage t2"
        "                WHERE t2.test_case_id = test_run_coverage.test_case_id);";
    if (sqlite3_prepare_v2(db_.handle(), latestSql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            latestPassed[columnText(stmt, 0)] = (columnInt(stmt, 1) != 0);
        }
        sqlite3_finalize(stmt);
    }

    std::vector<ExecutedCoverageRow> rows;
    for (const auto& req : reqsRes.value()) {
        ExecutedCoverageRow row;
        row.requirementId = req.id;
        row.requirementExternalId = req.externalId;

        for (const auto& l : linksRes.value()) {
            if (l.status != "Active") continue;
            if (l.targetType != EntityType::Requirement || l.targetId != req.id) continue;
            if (l.relation == "satisfies") {
                row.designed = true;
            } else if (l.relation == "verifies") {
                if (executedTestCases.count(l.sourceId)) {
                    row.executed = true;
                }
                auto it = latestPassed.find(l.sourceId);
                if (it != latestPassed.end() && it->second) {
                    row.verified = true;
                }
            }
        }
        rows.push_back(std::move(row));
    }
    return common::Result<std::vector<ExecutedCoverageRow>>::ok(std::move(rows));
}

}  // namespace lodestar::tracelink
