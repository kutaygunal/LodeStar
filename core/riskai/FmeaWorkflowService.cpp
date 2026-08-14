// core/riskai/FmeaWorkflowService.cpp
// Gap-Fill RiskAI 1.1 + 1.2: FMEA workflow engine (AIAG/VDA shape) and
// deterministic RPN / Action Priority scoring.

#include "core/riskai/FmeaWorkflowService.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#include <sqlite3.h>

#include "core/common/Time.h"
#include "core/common/Uuid.h"

namespace lodestar::riskai {

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

}  // namespace

// ---------------------------------------------------------------------------
// Stage helpers
// ---------------------------------------------------------------------------

std::optional<FmeaStage> nextStage(FmeaStage s) {
    if (s == FmeaStage::Documentation) return std::nullopt;
    return static_cast<FmeaStage>(static_cast<int>(s) + 1);
}

std::string stageName(FmeaStage s) {
    switch (s) {
        case FmeaStage::Planning: return "Planning";
        case FmeaStage::Structure: return "Structure";
        case FmeaStage::Function: return "Function";
        case FmeaStage::Failure: return "Failure";
        case FmeaStage::Risk: return "Risk";
        case FmeaStage::Optimization: return "Optimization";
        case FmeaStage::Documentation: return "Documentation";
    }
    return "Planning";
}

std::optional<FmeaStage> stageFromName(const std::string& name) {
    if (name == "Planning") return FmeaStage::Planning;
    if (name == "Structure") return FmeaStage::Structure;
    if (name == "Function") return FmeaStage::Function;
    if (name == "Failure") return FmeaStage::Failure;
    if (name == "Risk") return FmeaStage::Risk;
    if (name == "Optimization") return FmeaStage::Optimization;
    if (name == "Documentation") return FmeaStage::Documentation;
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// RiskAI 1.2: deterministic RPN + Action Priority scoring
// ---------------------------------------------------------------------------

RiskScore FmeaWorkflowService::computeScore(int severity, int occurrence,
                                            int detection) {
    // Clamp to the valid range so the pure function is total.
    auto clamp = [](int v) { return v < 1 ? 1 : (v > 10 ? 10 : v); };
    int s = clamp(severity);
    int o = clamp(occurrence);
    int d = clamp(detection);

    RiskScore out;
    out.rpn = s * o * d;

    // Documented AP model (see data/action_priority_matrix.csv). Must exactly
    // match the generated grid so a data-driven and function-driven lookup
    // agree.
    if (s >= 9) out.actionPriority = "High";
    else if (s >= 7 && o >= 6 && d >= 6) out.actionPriority = "High";
    else if (s >= 5 && o >= 7 && d >= 8) out.actionPriority = "High";
    else if (s <= 4 && o <= 5 && d <= 5) out.actionPriority = "Low";
    else if (s <= 6 && o <= 3 && d <= 4) out.actionPriority = "Low";
    else out.actionPriority = "Medium";

    return out;
}

bool FmeaWorkflowService::rowIsRated(const FmeaRow& row) {
    if (row.severity < 1 || row.severity > 10) return false;
    if (row.occurrence < 1 || row.occurrence > 10) return false;
    if (row.detection < 1 || row.detection > 10) return false;
    return row.actionPriority == "High" || row.actionPriority == "Medium" ||
           row.actionPriority == "Low";
}

// ---------------------------------------------------------------------------
// Workflow CRUD
// ---------------------------------------------------------------------------

FmeaWorkflowService::FmeaWorkflowService(persistence::Database& db) : db_(db) {}

common::Result<std::string>
FmeaWorkflowService::createWorkflow(const FmeaWorkflow& wf) {
    if (wf.name.empty()) {
        return common::Result<std::string>::err(
            common::ErrorCode::InvalidArgument, "workflow name must not be empty");
    }
    const std::string id = newUuid();
    const std::string created = wf.createdAt.empty() ? nowIso() : wf.createdAt;
    auto res = exec(db_.handle(),
                    "INSERT INTO riskai_fmea "
                    "(id, name, system, next_higher, next_lower, stage, "
                    " created_by, created_at, updated_at) "
                    "VALUES (?,?,?,?,?,?,?,?,?);",
                    {id, wf.name, wf.system, wf.nextHigher, wf.nextLower,
                     stageName(wf.stage), wf.createdBy, created, created});
    if (res.failed()) {
        return common::Result<std::string>::err(res.error());
    }
    return common::Result<std::string>::ok(id);
}

common::Result<std::optional<FmeaWorkflow>>
FmeaWorkflowService::findWorkflow(const std::string& id) {
    sqlite3_stmt* stmt = nullptr;
    const std::string sql =
        "SELECT id, name, system, next_higher, next_lower, stage, "
        "       created_by, created_at, updated_at FROM riskai_fmea "
        "WHERE id=?;";
    if (sqlite3_prepare_v2(db_.handle(), sql.c_str(), -1, &stmt, nullptr) !=
        SQLITE_OK) {
        return common::Result<std::optional<FmeaWorkflow>>::err(
            "prepare failed: " + std::string(sqlite3_errmsg(db_.handle())));
    }
    bindText(stmt, 1, id);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return common::Result<std::optional<FmeaWorkflow>>::ok(std::nullopt);
    }
    FmeaWorkflow wf;
    wf.id = colText(stmt, 0);
    wf.name = colText(stmt, 1);
    wf.system = colText(stmt, 2);
    wf.nextHigher = colText(stmt, 3);
    wf.nextLower = colText(stmt, 4);
    auto stage = stageFromName(colText(stmt, 5));
    wf.stage = stage ? *stage : FmeaStage::Planning;
    wf.createdBy = colText(stmt, 6);
    wf.createdAt = colText(stmt, 7);
    wf.updatedAt = colText(stmt, 8);
    sqlite3_finalize(stmt);
    return common::Result<std::optional<FmeaWorkflow>>::ok(std::move(wf));
}

common::Result<std::vector<FmeaWorkflow>>
FmeaWorkflowService::listWorkflows() {
    std::vector<FmeaWorkflow> out;
    sqlite3_stmt* stmt = nullptr;
    const std::string sql =
        "SELECT id, name, system, next_higher, next_lower, stage, "
        "       created_by, created_at, updated_at FROM riskai_fmea "
        "ORDER BY created_at, rowid;";
    if (sqlite3_prepare_v2(db_.handle(), sql.c_str(), -1, &stmt, nullptr) !=
        SQLITE_OK) {
        return common::Result<std::vector<FmeaWorkflow>>::err(
            "prepare failed: " + std::string(sqlite3_errmsg(db_.handle())));
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        FmeaWorkflow wf;
        wf.id = colText(stmt, 0);
        wf.name = colText(stmt, 1);
        wf.system = colText(stmt, 2);
        wf.nextHigher = colText(stmt, 3);
        wf.nextLower = colText(stmt, 4);
        auto stage = stageFromName(colText(stmt, 5));
        wf.stage = stage ? *stage : FmeaStage::Planning;
        wf.createdBy = colText(stmt, 6);
        wf.createdAt = colText(stmt, 7);
        wf.updatedAt = colText(stmt, 8);
        out.push_back(std::move(wf));
    }
    sqlite3_finalize(stmt);
    return common::Result<std::vector<FmeaWorkflow>>::ok(std::move(out));
}

common::Result<void> FmeaWorkflowService::updateWorkflow(const FmeaWorkflow& wf) {
    if (wf.id.empty()) {
        return common::Result<void>::err(common::ErrorCode::InvalidArgument,
                                         "workflow id must not be empty");
    }
    if (wf.name.empty()) {
        return common::Result<void>::err(common::ErrorCode::InvalidArgument,
                                         "workflow name must not be empty");
    }
    auto res = exec(db_.handle(),
                    "UPDATE riskai_fmea SET name=?, system=?, next_higher=?, "
                    " next_lower=?, updated_at=? WHERE id=?;",
                    {wf.name, wf.system, wf.nextHigher, wf.nextLower, nowIso(),
                     wf.id});
    if (res.failed()) return common::Result<void>::err(res.error());
    return common::Result<void>::ok();
}

common::Result<void> FmeaWorkflowService::deleteWorkflow(const std::string& id) {
    auto res = exec(db_.handle(), "DELETE FROM riskai_fmea WHERE id=?;", {id});
    if (res.failed()) return common::Result<void>::err(res.error());
    exec(db_.handle(), "DELETE FROM riskai_fmea_function WHERE fmea_id=?;", {id});
    exec(db_.handle(), "DELETE FROM riskai_fmea_row WHERE fmea_id=?;", {id});
    return common::Result<void>::ok();
}

std::string FmeaWorkflowService::missingForStage(const FmeaWorkflow& wf) const {
    auto countRows = [this](const std::string& col, const std::string& table,
                            const std::string& fmeaId) -> long long {
        std::string sql =
            "SELECT COUNT(*) FROM " + table + " WHERE fmea_id=?";
        (void)col;
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_.handle(), sql.c_str(), -1, &stmt, nullptr) !=
            SQLITE_OK) {
            return 0;
        }
        bindText(stmt, 1, fmeaId);
        long long n = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) n = sqlite3_column_int64(stmt, 0);
        sqlite3_finalize(stmt);
        return n;
    };

    switch (wf.stage) {
        case FmeaStage::Planning:
            // A named focus system is required to leave Planning.
            return wf.system.empty() ? "system (focus element)" : "";
        case FmeaStage::Structure:
            // Next-higher + next-lower structure elements.
            if (wf.nextHigher.empty()) return "next-higher structure element";
            return wf.nextLower.empty() ? "next-lower structure element" : "";
        case FmeaStage::Function:
            // Advancing out of Function requires at least one function element.
            return countRows("function_text", "riskai_fmea_function", wf.id) == 0
                       ? "at least one function element"
                       : "";
        case FmeaStage::Failure:
            // Advancing out of Failure requires at least one failure row.
            return countRows("failure_mode", "riskai_fmea_row", wf.id) == 0
                       ? "at least one failure row"
                       : "";
        case FmeaStage::Risk:
            // Advancing out of Risk requires every row to be rated (S/O/D set
            // and AP computed).
            {
                auto rows = rowsFor(wf.id);
                if (rows.failed()) return "readable failure rows";
                for (const auto& r : rows.value()) {
                    if (!rowIsRated(r)) return "all failure rows rated";
                }
                return "";
            }
        case FmeaStage::Optimization:
            return "";
        case FmeaStage::Documentation:
            return "";
    }
    return "";
}

common::Result<void> FmeaWorkflowService::setStage(const std::string& id,
                                                   FmeaStage stage) {
    auto found = findWorkflow(id);
    if (found.failed()) {
        return common::Result<void>::err(found.error());
    }
    if (!found.value().has_value()) {
        return common::Result<void>::err(common::ErrorCode::NotFound,
                                         "workflow not found");
    }
    // setStage is a low-level setter (rewind / jump). Gating (validating the
    // current stage's required fields before moving on) is owned by
    // advanceStage(), which is the only path the workflow advances through.
    const std::string updated = nowIso();
    auto res = exec(db_.handle(),
                    "UPDATE riskai_fmea SET stage=?, updated_at=? WHERE id=?;",
                    {stageName(stage), updated, id});
    if (res.failed()) return common::Result<void>::err(res.error());
    return common::Result<void>::ok();
}

common::Result<FmeaStage> FmeaWorkflowService::advanceStage(
    const std::string& id) {
    auto found = findWorkflow(id);
    if (found.failed()) {
        return common::Result<FmeaStage>::err(found.error());
    }
    if (!found.value().has_value()) {
        return common::Result<FmeaStage>::err(common::ErrorCode::NotFound,
                                              "workflow not found");
    }
    FmeaWorkflow wf = *found.value();
    if (wf.stage == FmeaStage::Documentation) {
        return common::Result<FmeaStage>::err(
            common::ErrorCode::IllegalTransition,
            "workflow already at terminal Documentation stage");
    }
    const std::string missing = missingForStage(wf);
    if (!missing.empty()) {
        return common::Result<FmeaStage>::err(
            common::ErrorCode::ValidationFailed,
            "cannot advance from " + stageName(wf.stage) + ": missing " +
                missing);
    }
    FmeaStage next = *nextStage(wf.stage);
    auto res = setStage(id, next);
    if (res.failed()) {
        return common::Result<FmeaStage>::err(res.error());
    }
    return common::Result<FmeaStage>::ok(next);
}

// ---------------------------------------------------------------------------
// Functions
// ---------------------------------------------------------------------------

common::Result<std::string> FmeaWorkflowService::addFunction(
    const std::string& fmeaId, const std::string& text,
    const std::string& requirement) {
    if (text.empty()) {
        return common::Result<std::string>::err(
            common::ErrorCode::InvalidArgument, "function text must not be empty");
    }
    auto wf = findWorkflow(fmeaId);
    if (wf.failed() || !wf.value().has_value()) {
        return common::Result<std::string>::err(common::ErrorCode::NotFound,
                                                "workflow not found");
    }
    const std::string id = newUuid();
    auto res = exec(db_.handle(),
                    "INSERT INTO riskai_fmea_function "
                    "(id, fmea_id, function_text, requirement, sort_order) "
                    "VALUES (?,?,?,?,0);",
                    {id, fmeaId, text, requirement});
    if (res.failed()) {
        return common::Result<std::string>::err(res.error());
    }
    return common::Result<std::string>::ok(id);
}

common::Result<std::vector<FmeaFunction>>
FmeaWorkflowService::functionsFor(const std::string& fmeaId) {
    std::vector<FmeaFunction> out;
    sqlite3_stmt* stmt = nullptr;
    const std::string sql =
        "SELECT id, fmea_id, function_text, requirement, sort_order "
        "FROM riskai_fmea_function WHERE fmea_id=? ORDER BY sort_order, rowid;";
    if (sqlite3_prepare_v2(db_.handle(), sql.c_str(), -1, &stmt, nullptr) !=
        SQLITE_OK) {
        return common::Result<std::vector<FmeaFunction>>::err(
            "prepare failed: " + std::string(sqlite3_errmsg(db_.handle())));
    }
    bindText(stmt, 1, fmeaId);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        FmeaFunction f;
        f.id = colText(stmt, 0);
        f.fmeaId = colText(stmt, 1);
        f.text = colText(stmt, 2);
        f.requirement = colText(stmt, 3);
        f.sortOrder = colInt(stmt, 4);
        out.push_back(std::move(f));
    }
    sqlite3_finalize(stmt);
    return common::Result<std::vector<FmeaFunction>>::ok(std::move(out));
}

// ---------------------------------------------------------------------------
// Failure rows
// ---------------------------------------------------------------------------

common::Result<std::string> FmeaWorkflowService::addRow(const FmeaRow& row) {
    if (row.fmeaId.empty()) {
        return common::Result<std::string>::err(
            common::ErrorCode::InvalidArgument, "row.fmeaId must not be empty");
    }
    if (row.failureMode.empty()) {
        return common::Result<std::string>::err(
            common::ErrorCode::InvalidArgument, "row.failureMode must not be empty");
    }
    auto wf = findWorkflow(row.fmeaId);
    if (wf.failed() || !wf.value().has_value()) {
        return common::Result<std::string>::err(common::ErrorCode::NotFound,
                                                "workflow not found");
    }
    const std::string id = newUuid();
    auto res = exec(db_.handle(),
                    "INSERT INTO riskai_fmea_row "
                    "(id, fmea_id, function_id, effect, failure_mode, cause, "
                    " severity, occurrence, detection, action_priority, sort_order) "
                    "VALUES (?,?,?,?,?,?,?,?,?,?,0);",
                    {id, row.fmeaId, row.functionId, row.effect, row.failureMode,
                     row.cause, std::to_string(row.severity),
                     std::to_string(row.occurrence), std::to_string(row.detection),
                     row.actionPriority});
    if (res.failed()) {
        return common::Result<std::string>::err(res.error());
    }
    return common::Result<std::string>::ok(id);
}

common::Result<std::vector<FmeaRow>> FmeaWorkflowService::rowsFor(
    const std::string& fmeaId) const {
    std::vector<FmeaRow> out;
    sqlite3_stmt* stmt = nullptr;
    const std::string sql =
        "SELECT id, fmea_id, function_id, effect, failure_mode, cause, "
        "       severity, occurrence, detection, action_priority, sort_order "
        "FROM riskai_fmea_row WHERE fmea_id=? ORDER BY sort_order, rowid;";
    if (sqlite3_prepare_v2(db_.handle(), sql.c_str(), -1, &stmt, nullptr) !=
        SQLITE_OK) {
        return common::Result<std::vector<FmeaRow>>::err(
            "prepare failed: " + std::string(sqlite3_errmsg(db_.handle())));
    }
    bindText(stmt, 1, fmeaId);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        FmeaRow r;
        r.id = colText(stmt, 0);
        r.fmeaId = colText(stmt, 1);
        r.functionId = colText(stmt, 2);
        r.effect = colText(stmt, 3);
        r.failureMode = colText(stmt, 4);
        r.cause = colText(stmt, 5);
        r.severity = colInt(stmt, 6);
        r.occurrence = colInt(stmt, 7);
        r.detection = colInt(stmt, 8);
        r.actionPriority = colText(stmt, 9);
        r.sortOrder = colInt(stmt, 10);
        out.push_back(std::move(r));
    }
    sqlite3_finalize(stmt);
    return common::Result<std::vector<FmeaRow>>::ok(std::move(out));
}

common::Result<void> FmeaWorkflowService::updateRow(const FmeaRow& row) {
    if (row.id.empty()) {
        return common::Result<void>::err(common::ErrorCode::InvalidArgument,
                                         "row.id must not be empty");
    }
    auto res = exec(db_.handle(),
                    "UPDATE riskai_fmea_row SET function_id=?, effect=?, "
                    " failure_mode=?, cause=?, severity=?, occurrence=?, "
                    " detection=?, action_priority=? WHERE id=?;",
                    {row.functionId, row.effect, row.failureMode, row.cause,
                     std::to_string(row.severity), std::to_string(row.occurrence),
                     std::to_string(row.detection), row.actionPriority, row.id});
    if (res.failed()) return common::Result<void>::err(res.error());
    return common::Result<void>::ok();
}

}  // namespace lodestar::riskai
