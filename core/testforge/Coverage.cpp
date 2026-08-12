// core/testforge/Coverage.cpp
#include "core/testforge/Coverage.h"

#include <sqlite3.h>

namespace lodestar::testforge {

namespace {

void bindText(sqlite3_stmt* stmt, int index, const std::string& value) {
    sqlite3_bind_text(stmt, index, value.c_str(), static_cast<int>(value.size()),
                      SQLITE_TRANSIENT);
}

void bindInt(sqlite3_stmt* stmt, int index, int value) {
    sqlite3_bind_int(stmt, index, value);
}

std::string columnText(sqlite3_stmt* stmt, int col) {
    const unsigned char* text = sqlite3_column_text(stmt, col);
    return text ? reinterpret_cast<const char*>(text) : std::string();
}

int columnInt(sqlite3_stmt* stmt, int col) { return sqlite3_column_int(stmt, col); }

}  // namespace

double computeStatementCoverage(int executed, int total) {
    if (total <= 0) return 0.0;
    double ratio = static_cast<double>(executed) / static_cast<double>(total);
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;
    return ratio * 100.0;
}

double computeDecisionCoverage(int decisionsTaken, int decisionsTotal) {
    if (decisionsTotal <= 0) return 0.0;
    double ratio = static_cast<double>(decisionsTaken) /
                   static_cast<double>(decisionsTotal);
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;
    return ratio * 100.0;
}

double computeMcdcCoverage(int conditionsSatisfied, int conditionsTotal) {
    if (conditionsTotal <= 0) return 0.0;
    double ratio = static_cast<double>(conditionsSatisfied) /
                   static_cast<double>(conditionsTotal);
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;
    return ratio * 100.0;
}

common::Result<void> CoverageDao::save(const CoverageResult& r) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO coverage_results (id, run_id, scope, statements_executed,"
        " statements_total, decisions_taken, decisions_total, conditions_satisfied,"
        " conditions_total, recorded_at)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
        " ON CONFLICT(id) DO UPDATE SET"
        "  run_id=excluded.run_id, scope=excluded.scope,"
        "  statements_executed=excluded.statements_executed,"
        "  statements_total=excluded.statements_total,"
        "  decisions_taken=excluded.decisions_taken,"
        "  decisions_total=excluded.decisions_total,"
        "  conditions_satisfied=excluded.conditions_satisfied,"
        "  conditions_total=excluded.conditions_total,"
        "  recorded_at=excluded.recorded_at;";
    if (sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<void>::err(
            "prepare coverage insert failed: " +
            std::string(sqlite3_errmsg(db_.handle())));
    }
    bindText(stmt, 1, r.id);
    bindText(stmt, 2, r.runId);
    bindText(stmt, 3, r.scope);
    bindInt(stmt, 4, r.statementsExecuted);
    bindInt(stmt, 5, r.statementsTotal);
    bindInt(stmt, 6, r.decisionsTaken);
    bindInt(stmt, 7, r.decisionsTotal);
    bindInt(stmt, 8, r.conditionsSatisfied);
    bindInt(stmt, 9, r.conditionsTotal);
    bindText(stmt, 10, r.recordedAt);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return common::Result<void>::err(
            "insert coverage failed: " + std::string(sqlite3_errmsg(db_.handle())));
    }
    return common::Result<void>::ok();
}

common::Result<std::optional<CoverageResult>> CoverageDao::load(
    const std::string& id) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT id, run_id, scope, statements_executed, statements_total,"
        " decisions_taken, decisions_total, conditions_satisfied, conditions_total,"
        " recorded_at FROM coverage_results WHERE id = ?;";
    if (sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<std::optional<CoverageResult>>::err(
            "prepare coverage select failed: " +
            std::string(sqlite3_errmsg(db_.handle())));
    }
    bindText(stmt, 1, id);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return common::Result<std::optional<CoverageResult>>::ok(std::nullopt);
    }
    CoverageResult r;
    r.id = columnText(stmt, 0);
    r.runId = columnText(stmt, 1);
    r.scope = columnText(stmt, 2);
    r.statementsExecuted = columnInt(stmt, 3);
    r.statementsTotal = columnInt(stmt, 4);
    r.decisionsTaken = columnInt(stmt, 5);
    r.decisionsTotal = columnInt(stmt, 6);
    r.conditionsSatisfied = columnInt(stmt, 7);
    r.conditionsTotal = columnInt(stmt, 8);
    r.recordedAt = columnText(stmt, 9);
    sqlite3_finalize(stmt);
    return common::Result<std::optional<CoverageResult>>::ok(std::move(r));
}

common::Result<std::vector<CoverageResult>> CoverageDao::list() {
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT id, run_id, scope, statements_executed, statements_total,"
        " decisions_taken, decisions_total, conditions_satisfied, conditions_total,"
        " recorded_at FROM coverage_results ORDER BY id;";
    if (sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<std::vector<CoverageResult>>::err(
            "prepare coverage list failed: " +
            std::string(sqlite3_errmsg(db_.handle())));
    }
    std::vector<CoverageResult> out;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        CoverageResult r;
        r.id = columnText(stmt, 0);
        r.runId = columnText(stmt, 1);
        r.scope = columnText(stmt, 2);
        r.statementsExecuted = columnInt(stmt, 3);
        r.statementsTotal = columnInt(stmt, 4);
        r.decisionsTaken = columnInt(stmt, 5);
        r.decisionsTotal = columnInt(stmt, 6);
        r.conditionsSatisfied = columnInt(stmt, 7);
        r.conditionsTotal = columnInt(stmt, 8);
        r.recordedAt = columnText(stmt, 9);
        out.push_back(std::move(r));
    }
    sqlite3_finalize(stmt);
    return common::Result<std::vector<CoverageResult>>::ok(std::move(out));
}

}  // namespace lodestar::testforge
