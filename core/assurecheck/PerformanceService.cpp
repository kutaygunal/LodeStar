// core/assurecheck/PerformanceService.cpp
// Phase 11 WP-5 (AssureCheck): performance + hardening implementation.

#include "core/assurecheck/PerformanceService.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include <sqlite3.h>

#include "core/common/Uuid.h"

namespace lodestar::assurecheck {

using lodestar::common::newUuid;

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

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

// Maps an item's evidence text to a project-data source using case-insensitive
// substring matching, in the order specified by the WP-2/3 contract.
std::string mapEvidenceSource(const std::string& evidence) {
    const std::string e = toLower(evidence);
    if (contains(e, "test")) return "test_cases";
    if (contains(e, "traceab")) return "trace_links";
    if (contains(e, "design") || contains(e, "architecture") ||
        contains(e, "source code") || contains(e, "code") ||
        contains(e, "build") || contains(e, "implementation") ||
        contains(e, "partitioning")) {
        return "design_items";
    }
    return "requirements";
}

// Returns true if `level` (a single letter) falls within [first, last] of the
// item's DAL range string (e.g. "A", "A-B", "A-C", "A-D").
bool dalApplies(const std::string& range, const std::string& level) {
    if (range.empty() || level.empty()) return false;
    const char lo = range.front();
    const char hi = range.back();
    const char l = level[0];
    return l >= lo && l <= hi;
}

// Evaluates the mapped source and returns (status, evidence ids).
struct EvalOutcome {
    CheckStatus status;
    std::vector<std::string> ids;
};

EvalOutcome evaluateSource(sqlite3* db, const std::string& source) {
    EvalOutcome out;
    if (source == "test_cases") {
        auto passed = query(db,
                            "SELECT id FROM test_cases "
                            "WHERE result_status='Passed' ORDER BY id;",
                            {}, 1);
        if (passed.isOk() && !passed.value().empty()) {
            out.status = CheckStatus::Pass;
            for (const auto& r : passed.value()) out.ids.push_back(r[0]);
            return out;
        }
        auto any = query(db, "SELECT id FROM test_cases ORDER BY id;", {}, 1);
        if (any.isOk() && !any.value().empty()) {
            out.status = CheckStatus::Warning;
            return out;
        }
        out.status = CheckStatus::Fail;
        return out;
    }
    if (source == "trace_links") {
        auto rows = query(db, "SELECT id FROM trace_links ORDER BY id;", {}, 1);
        if (rows.isOk() && !rows.value().empty()) {
            out.status = CheckStatus::Pass;
            for (const auto& r : rows.value()) out.ids.push_back(r[0]);
        } else {
            out.status = CheckStatus::Fail;
        }
        return out;
    }
    if (source == "design_items") {
        auto rows = query(db, "SELECT id FROM design_items ORDER BY id;", {}, 1);
        if (rows.isOk() && !rows.value().empty()) {
            out.status = CheckStatus::Pass;
            for (const auto& r : rows.value()) out.ids.push_back(r[0]);
        } else {
            out.status = CheckStatus::Fail;
        }
        return out;
    }
    // requirements
    auto rows = query(db, "SELECT id FROM requirements ORDER BY id;", {}, 1);
    if (rows.isOk() && !rows.value().empty()) {
        out.status = CheckStatus::Pass;
        for (const auto& r : rows.value()) out.ids.push_back(r[0]);
    } else {
        out.status = CheckStatus::Fail;
    }
    return out;
}

std::string sourceToEntityType(const std::string& source) {
    if (source == "test_cases") return "test_case";
    if (source == "trace_links") return "trace_link";
    if (source == "design_items") return "design";
    return "requirement";
}

}  // namespace

PerformanceService::PerformanceService(persistence::Database& db) : db_(db) {}

common::Result<std::vector<CheckResult>> PerformanceService::evaluateBatched(
    const std::string& standardCode, const std::string& dalLevel) {
    if (!db_.isOpen()) {
        return common::Result<std::vector<CheckResult>>::err(
            "database not open");
    }

    // Run the whole standard atomically in a single transaction.
    auto begin = db_.beginImmediate();
    if (begin.failed()) {
        return common::Result<std::vector<CheckResult>>::err(begin.error());
    }

    ComplianceEngine engine(db_);
    auto results = engine.runChecks(standardCode, dalLevel);
    if (results.failed()) {
        db_.rollback();
        return common::Result<std::vector<CheckResult>>::err(results.error());
    }

    // Persist via storeResults semantics (idempotent per standard).
    auto store = engine.storeResults(results.value());
    if (store.failed()) {
        db_.rollback();
        return common::Result<std::vector<CheckResult>>::err(store.error());
    }

    auto commit = db_.commit();
    if (commit.failed()) {
        db_.rollback();
        return common::Result<std::vector<CheckResult>>::err(commit.error());
    }
    return common::Result<std::vector<CheckResult>>::ok(std::move(results.value()));
}

common::Result<std::vector<CheckResult>> PerformanceService::recheckIncremental(
    const std::string& standardCode, const std::string& dalLevel,
    const std::vector<std::string>& changedSources) {
    sqlite3* db = db_.handle();
    if (db == nullptr) {
        return common::Result<std::vector<CheckResult>>::err(
            "database not open");
    }

    // Resolve the standard id.
    auto stdRows = query(db,
                         "SELECT id FROM assurance_standards WHERE code=?;",
                         {standardCode}, 1);
    if (stdRows.failed() || stdRows.value().empty()) {
        return common::Result<std::vector<CheckResult>>::err(
            "unknown standard: " + standardCode);
    }
    const std::string standardId = stdRows.value().front()[0];

    // Map changedSources to project-data sources.
    std::vector<std::string> changed;
    for (const auto& s : changedSources) {
        const std::string ls = toLower(s);
        if (ls == "test_run" || ls == "test_case") {
            changed.push_back("test_cases");
        } else if (ls == "trace_link") {
            changed.push_back("trace_links");
        } else if (ls == "design") {
            changed.push_back("design_items");
        } else if (ls == "requirement") {
            changed.push_back("requirements");
        }
    }

    // Checklist items for the standard, ordered by seq.
    auto items = query(db,
                       "SELECT id, item_code, dal_level, evidence "
                       "FROM assurance_checklist_items "
                       "WHERE standard_id=? ORDER BY seq;",
                       {standardId}, 4);
    if (items.failed()) {
        return common::Result<std::vector<CheckResult>>::err(items.error());
    }

    std::vector<CheckResult> results;
    for (const auto& r : items.value()) {
        const std::string evidenceText = r[3];
        const std::string source = mapEvidenceSource(evidenceText);

        // Only re-evaluate items whose source is in the changed set.
        bool affected = false;
        for (const auto& c : changed) {
            if (c == source) {
                affected = true;
                break;
            }
        }
        if (!affected) continue;

        CheckResult res;
        res.id = newUuid();
        res.standardCode = standardCode;
        res.itemId = r[0];
        res.itemCode = r[1];
        res.dalLevel = r[2];

        // 1. DAL applicability.
        if (!dalApplies(res.dalLevel, dalLevel)) {
            res.status = CheckStatus::Na;
            res.detail = "Not applicable for DAL " + dalLevel;
            results.push_back(std::move(res));
            continue;
        }

        // 2. Status against current project data.
        EvalOutcome outcome = evaluateSource(db, source);
        res.status = outcome.status;

        // 3. Evidence links on PASS.
        if (outcome.status == CheckStatus::Pass) {
            const std::string type = sourceToEntityType(source);
            for (const auto& id : outcome.ids) {
                EvidenceLink link;
                link.entityType = type;
                link.entityId = id;
                res.evidence.push_back(std::move(link));
            }
        }
        results.push_back(std::move(res));
    }
    return common::Result<std::vector<CheckResult>>::ok(std::move(results));
}

}  // namespace lodestar::assurecheck
