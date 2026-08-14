// core/riskai/RiskReportService.cpp
// Gap-Fill RiskAI 1.4: AIAG/VDA-compatible export.

#include "core/riskai/RiskReportService.h"

#include <algorithm>
#include <sstream>

#include "core/riskai/FmeaWorkflowService.h"

namespace lodestar::riskai {

namespace {

// CSV-escape a single field (quote if it contains a comma/quote/newline).
std::string csvField(const std::string& s) {
    if (s.find(',') == std::string::npos &&
        s.find('"') == std::string::npos &&
        s.find('\n') == std::string::npos) {
        return s;
    }
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\"\"";
        else out += c;
    }
    out += "\"";
    return out;
}

}  // namespace

RiskReportService::RiskReportService(persistence::Database& db) : db_(db) {}

common::Result<std::string> RiskReportService::csvText(
    const std::string& workflowId) {
    FmeaWorkflowService svc(db_);
    auto wf = svc.findWorkflow(workflowId);
    if (wf.failed()) return common::Result<std::string>::err(wf.error());
    if (!wf.value().has_value()) {
        return common::Result<std::string>::err(
            common::ErrorCode::NotFound, "workflow not found");
    }
    auto rows = svc.rowsFor(workflowId);
    if (rows.failed()) return common::Result<std::string>::err(rows.error());

    std::ostringstream out;
    // Header: AIAG-VDA seven-step columns.
    out << "system,function,failure_mode,effect,cause,severity,occurrence,"
           "detection,action_priority,rpn\n";
    for (const auto& r : rows.value()) {
        auto sc = FmeaWorkflowService::computeScore(r.severity, r.occurrence,
                                                    r.detection);
        out << csvField(wf.value()->system) << ","
            << csvField(r.failureMode) << ","
            << csvField(r.failureMode) << ","
            << csvField(r.effect) << ","
            << csvField(r.cause) << ","
            << (r.severity >= 1 ? std::to_string(r.severity) : "") << ","
            << (r.occurrence >= 1 ? std::to_string(r.occurrence) : "") << ","
            << (r.detection >= 1 ? std::to_string(r.detection) : "") << ","
            << csvField(r.actionPriority) << ","
            << sc.rpn << "\n";
    }
    return common::Result<std::string>::ok(out.str());
}

common::Result<std::vector<std::uint8_t>> RiskReportService::exportCsv(
    const std::string& workflowId) {
    auto text = csvText(workflowId);
    if (text.failed()) {
        return common::Result<std::vector<std::uint8_t>>::err(text.error());
    }
    std::vector<std::uint8_t> bytes(text.value().begin(), text.value().end());
    return common::Result<std::vector<std::uint8_t>>::ok(std::move(bytes));
}

common::Result<std::vector<std::uint8_t>> RiskReportService::exportHtml(
    const std::string& workflowId) {
    FmeaWorkflowService svc(db_);
    auto wf = svc.findWorkflow(workflowId);
    if (wf.failed()) return common::Result<std::vector<std::uint8_t>>::err(wf.error());
    if (!wf.value().has_value()) {
        return common::Result<std::vector<std::uint8_t>>::err(
            common::ErrorCode::NotFound, "workflow not found");
    }
    auto rows = svc.rowsFor(workflowId);
    if (rows.failed()) {
        return common::Result<std::vector<std::uint8_t>>::err(rows.error());
    }
    auto funcs = svc.functionsFor(workflowId);
    if (funcs.failed()) {
        return common::Result<std::vector<std::uint8_t>>::err(funcs.error());
    }

    std::ostringstream out;
    out << "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\">"
        << "<title>FMEA Review - " << wf.value()->name << "</title>"
        << "<style>body{font-family:sans-serif}table{border-collapse:collapse}"
        << "th,td{border:1px solid #999;padding:4px 8px;text-align:left}"
        << "</style></head><body>\n";
    out << "<h1>FMEA Review: " << wf.value()->name << "</h1>\n";
    out << "<p><b>System:</b> " << wf.value()->system << "<br>\n"
        << "<b>Stage:</b> " << stageName(wf.value()->stage) << "</p>\n";

    out << "<h2>Functions</h2>\n<ul>\n";
    for (const auto& f : funcs.value()) {
        out << "<li>" << f.text << "</li>\n";
    }
    out << "</ul>\n";

    out << "<h2>Failure Analysis</h2>\n"
        << "<table><tr><th>Failure Mode</th><th>Effect</th><th>Cause</th>"
        << "<th>S</th><th>O</th><th>D</th><th>RPN</th><th>AP</th></tr>\n";
    for (const auto& r : rows.value()) {
        auto sc = FmeaWorkflowService::computeScore(r.severity, r.occurrence,
                                                    r.detection);
        out << "<tr><td>" << r.failureMode << "</td><td>" << r.effect
            << "</td><td>" << r.cause << "</td>"
            << "<td>" << (r.severity >= 1 ? std::to_string(r.severity) : "-")
            << "</td><td>"
            << (r.occurrence >= 1 ? std::to_string(r.occurrence) : "-")
            << "</td><td>"
            << (r.detection >= 1 ? std::to_string(r.detection) : "-")
            << "</td><td>" << sc.rpn << "</td><td>"
            << (r.actionPriority.empty() ? "-" : r.actionPriority)
            << "</td></tr>\n";
    }
    out << "</table>\n</body></html>\n";

    std::string s = out.str();
    std::vector<std::uint8_t> bytes(s.begin(), s.end());
    return common::Result<std::vector<std::uint8_t>>::ok(std::move(bytes));
}

}  // namespace lodestar::riskai
