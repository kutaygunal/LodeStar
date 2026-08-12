// core/assurecheck/ComplianceEngine.cpp
// Phase 11 WP-2 (AssureCheck): compliance engine implementation.

#include "core/assurecheck/ComplianceEngine.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
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

int colInt(sqlite3_stmt* stmt, int col) {
    return sqlite3_column_int(stmt, col);
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
// substring matching, in the order specified by the contract.
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

std::string statusToString(CheckStatus s) {
    switch (s) {
        case CheckStatus::Pass: return "PASS";
        case CheckStatus::Fail: return "FAIL";
        case CheckStatus::Na: return "NA";
        case CheckStatus::Warning: return "WARNING";
    }
    return "NA";
}

CheckStatus statusFromString(const std::string& s) {
    if (s == "PASS") return CheckStatus::Pass;
    if (s == "FAIL") return CheckStatus::Fail;
    if (s == "WARNING") return CheckStatus::Warning;
    return CheckStatus::Na;
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

// Serializes evidence links as "type:id;type:id".
std::string evidenceToString(const std::vector<EvidenceLink>& ev) {
    std::string out;
    for (const auto& e : ev) {
        if (!out.empty()) out += ";";
        out += e.entityType + ":" + e.entityId;
    }
    return out;
}

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

ComplianceEngine::ComplianceEngine(persistence::Database& db) : db_(db) {}

common::Result<std::vector<CheckResult>> ComplianceEngine::runChecks(
    const std::string& standardCode, const std::string& dalLevel) {
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
        CheckResult res;
        res.id = newUuid();
        res.standardCode = standardCode;
        res.itemId = r[0];
        res.itemCode = r[1];
        res.dalLevel = r[2];
        const std::string evidenceText = r[3];

        // 1. DAL applicability.
        if (!dalApplies(res.dalLevel, dalLevel)) {
            res.status = CheckStatus::Na;
            res.detail = "Not applicable for DAL " + dalLevel;
            results.push_back(std::move(res));
            continue;
        }

        // 2. Evidence source.
        const std::string source = mapEvidenceSource(evidenceText);

        // 3. Status.
        EvalOutcome outcome = evaluateSource(db, source);
        res.status = outcome.status;

        // 4. Evidence links on PASS.
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

common::Result<void> ComplianceEngine::storeResults(
    const std::vector<CheckResult>& results) {
    sqlite3* db = db_.handle();
    if (db == nullptr) {
        return common::Result<void>::err("database not open");
    }
    if (results.empty()) {
        return common::Result<void>::ok();
    }

    // Resolve the standard id from the first result's standardCode.
    const std::string& standardCode = results.front().standardCode;
    auto stdRows = query(db,
                         "SELECT id FROM assurance_standards WHERE code=?;",
                         {standardCode}, 1);
    if (stdRows.failed() || stdRows.value().empty()) {
        return common::Result<void>::err("unknown standard: " + standardCode);
    }
    const std::string standardId = stdRows.value().front()[0];

    // Idempotent: replace any previously stored results for this standard.
    auto del = exec(db, "DELETE FROM assurance_checks WHERE standard_id=?;",
                    {standardId});
    if (del.failed()) return del;

    const std::string now = "now";
    for (const auto& res : results) {
        auto ins = exec(db,
                        "INSERT INTO assurance_checks "
                        "(id, standard_id, item_id, item_code, status, "
                        "dal_level, evidence, detail, checked_at) "
                        "VALUES (?,?,?,?,?,?,?,?,?);",
                        {res.id, standardId, res.itemId, res.itemCode,
                         statusToString(res.status), res.dalLevel,
                         evidenceToString(res.evidence), res.detail, now});
        if (ins.failed()) return ins;
    }
    return common::Result<void>::ok();
}

common::Result<std::vector<CheckResult>> ComplianceEngine::resultsFor(
    const std::string& standardCode) {
    sqlite3* db = db_.handle();
    if (db == nullptr) {
        return common::Result<std::vector<CheckResult>>::err(
            "database not open");
    }
    auto rows = query(db,
                      "SELECT c.id, c.item_id, c.item_code, c.status, "
                      "c.dal_level, c.evidence, c.detail "
                      "FROM assurance_checks c "
                      "JOIN assurance_standards s ON s.id = c.standard_id "
                      "JOIN assurance_checklist_items i ON i.id = c.item_id "
                      "WHERE s.code=? ORDER BY i.seq;",
                      {standardCode}, 7);
    if (rows.failed()) {
        return common::Result<std::vector<CheckResult>>::err(rows.error());
    }
    std::vector<CheckResult> out;
    for (const auto& r : rows.value()) {
        CheckResult res;
        res.id = r[0];
        res.standardCode = standardCode;
        res.itemId = r[1];
        res.itemCode = r[2];
        res.status = statusFromString(r[3]);
        res.dalLevel = r[4];
        res.evidence = evidenceFromString(r[5]);
        res.detail = r[6];
        out.push_back(std::move(res));
    }
    return common::Result<std::vector<CheckResult>>::ok(std::move(out));
}

common::Result<CheckSummary> ComplianceEngine::summaryFor(
    const std::string& standardCode) {
    sqlite3* db = db_.handle();
    if (db == nullptr) {
        return common::Result<CheckSummary>::err("database not open");
    }
    auto rows = query(db,
                      "SELECT c.status, COUNT(*) "
                      "FROM assurance_checks c "
                      "JOIN assurance_standards s ON s.id = c.standard_id "
                      "WHERE s.code=? GROUP BY c.status;",
                      {standardCode}, 2);
    if (rows.failed()) {
        return common::Result<CheckSummary>::err(rows.error());
    }
    CheckSummary sum;
    for (const auto& r : rows.value()) {
        const int n = std::atoi(r[1].c_str());
        sum.total += n;
        if (r[0] == "PASS") sum.pass += n;
        else if (r[0] == "FAIL") sum.fail += n;
        else if (r[0] == "NA") sum.na += n;
        else if (r[0] == "WARNING") sum.warning += n;
    }
    sum.percent = (sum.pass > 0 && sum.total > 0) ? (sum.pass * 100 / sum.total) : 0;
    return common::Result<CheckSummary>::ok(sum);
}

}  // namespace lodestar::assurecheck
