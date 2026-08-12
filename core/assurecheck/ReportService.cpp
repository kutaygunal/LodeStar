// core/assurecheck/ReportService.cpp
// Phase 11 WP-4 (AssureCheck): compliance reporting implementation.

#include "core/assurecheck/ReportService.h"

#include <algorithm>
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

std::string statusToString(CheckStatus s) {
    switch (s) {
        case CheckStatus::Pass: return "PASS";
        case CheckStatus::Fail: return "FAIL";
        case CheckStatus::Na: return "NA";
        case CheckStatus::Warning: return "WARNING";
    }
    return "NA";
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

// A checklist item's lookup data keyed by item_code.
struct ItemMeta {
    std::string objective;
    std::string dalLevel;
    int seq = 0;
};

}  // namespace

ReportService::ReportService(persistence::Database& db) : db_(db) {}

common::Result<ComplianceReport> ReportService::buildReport(
    const std::string& standardCode, const std::string& dalLevel,
    const std::vector<CheckResult>& results) {
    sqlite3* db = db_.handle();
    if (db == nullptr) {
        return common::Result<ComplianceReport>::err("database not open");
    }

    // Resolve the standard id + name.
    auto stdRows = query(db,
                         "SELECT id, name FROM assurance_standards WHERE code=?;",
                         {standardCode}, 2);
    if (stdRows.failed() || stdRows.value().empty()) {
        return common::Result<ComplianceReport>::err(
            "unknown standard: " + standardCode);
    }
    const std::string standardId = stdRows.value().front()[0];
    const std::string standardName = stdRows.value().front()[1];

    // Load checklist item metadata (objective, dal range, seq) for the standard.
    auto items = query(db,
                       "SELECT item_code, objective, dal_level, seq "
                       "FROM assurance_checklist_items "
                       "WHERE standard_id=?;",
                       {standardId}, 4);
    if (items.failed()) {
        return common::Result<ComplianceReport>::err(items.error());
    }

    // Build a code -> (objective, dal, seq) lookup from the checklist.
    std::vector<std::pair<std::string, ItemMeta>> codeMeta;
    for (const auto& r : items.value()) {
        ItemMeta m;
        m.objective = r[1];
        m.dalLevel = r[2];
        m.seq = std::atoi(r[3].c_str());
        codeMeta.emplace_back(r[0], std::move(m));
    }

    ComplianceReport report;
    report.standardCode = standardCode;
    report.standardName = standardName;
    report.dalLevel = dalLevel;

    for (const auto& res : results) {
        ReportRow row;
        row.itemCode = res.itemCode;
        row.status = statusToString(res.status);
        row.dalLevel = res.dalLevel;
        row.evidence = evidenceToString(res.evidence);

        // Look up objective text + dal range from the checklist by item_code.
        for (const auto& cm : codeMeta) {
            if (cm.first == row.itemCode) {
                row.objective = cm.second.objective;
                row.dalLevel = cm.second.dalLevel;
                break;
            }
        }
        report.rows.push_back(std::move(row));
    }

    // Order rows by item seq (results from runChecks are already seq-ordered,
    // but we sort defensively).
    std::stable_sort(report.rows.begin(), report.rows.end(),
                     [&codeMeta](const ReportRow& a, const ReportRow& b) {
                         int sa = 0, sb = 0;
                         for (const auto& cm : codeMeta) {
                             if (cm.first == a.itemCode) sa = cm.second.seq;
                             if (cm.first == b.itemCode) sb = cm.second.seq;
                         }
                         return sa < sb;
                     });

    report.coverage = coverage(report);
    return common::Result<ComplianceReport>::ok(std::move(report));
}

CoverageSummary ReportService::coverage(const ComplianceReport& report) {
    CoverageSummary c;
    c.total = static_cast<int>(report.rows.size());
    for (const auto& row : report.rows) {
        if (row.status == "PASS") ++c.pass;
        else if (row.status == "FAIL") ++c.fail;
        else if (row.status == "NA") ++c.na;
        else if (row.status == "WARNING") ++c.warning;
    }
    c.applicable = c.total - c.na;
    c.percent = c.applicable > 0 ? (c.pass * 100 / c.applicable) : 0;
    return c;
}

common::Result<std::string> ReportService::toHtml(
    const ComplianceReport& report) {
    std::string html;
    html += "<!DOCTYPE html>\n<html>\n<head>\n";
    html += "<meta charset=\"utf-8\">\n";
    html += "<title>Compliance Report - " + report.standardCode + "</title>\n";
    html += "</head>\n<body>\n";
    html += "<h1>Compliance Report</h1>\n";
    html += "<p>Standard: " + report.standardCode + " - " +
            report.standardName + "</p>\n";
    html += "<p>Project DAL: " + report.dalLevel + "</p>\n";
    html += "<p>Coverage: " + std::to_string(report.coverage.percent) +
            "%</p>\n";
    html += "<table border=\"1\">\n";
    html += "<tr><th>Item</th><th>Objective</th><th>DAL</th><th>Status</th>"
            "<th>Evidence</th></tr>\n";
    for (const auto& row : report.rows) {
        html += "<tr><td>" + row.itemCode + "</td><td>" + row.objective +
                "</td><td>" + row.dalLevel + "</td><td>" + row.status +
                "</td><td>" + row.evidence + "</td></tr>\n";
    }
    html += "</table>\n";
    html += "</body>\n</html>\n";
    return common::Result<std::string>::ok(std::move(html));
}

common::Result<std::string> ReportService::toCsv(
    const ComplianceReport& report) {
    std::string csv = "item_code,objective,dal_level,status,evidence\n";
    for (const auto& row : report.rows) {
        csv += row.itemCode + "," + row.objective + "," + row.dalLevel + "," +
               row.status + "," + row.evidence + "\n";
    }
    return common::Result<std::string>::ok(std::move(csv));
}

common::Result<Json> ReportService::toJson(const ComplianceReport& report) {
    Json root = Json::object();
    root["standard"] = Json::string(report.standardCode);
    root["standard_name"] = Json::string(report.standardName);
    root["dal"] = Json::string(report.dalLevel);

    Json cov = Json::object();
    cov["total"] = Json::number(report.coverage.total);
    cov["applicable"] = Json::number(report.coverage.applicable);
    cov["pass"] = Json::number(report.coverage.pass);
    cov["fail"] = Json::number(report.coverage.fail);
    cov["na"] = Json::number(report.coverage.na);
    cov["warning"] = Json::number(report.coverage.warning);
    cov["percent"] = Json::number(report.coverage.percent);
    root["coverage"] = cov;

    Json rows = Json::array();
    for (const auto& row : report.rows) {
        Json r = Json::object();
        r["item_code"] = Json::string(row.itemCode);
        r["objective"] = Json::string(row.objective);
        r["dal_level"] = Json::string(row.dalLevel);
        r["status"] = Json::string(row.status);
        r["evidence"] = Json::string(row.evidence);
        rows.push(r);
    }
    root["rows"] = rows;

    return common::Result<Json>::ok(std::move(root));
}

}  // namespace lodestar::assurecheck
