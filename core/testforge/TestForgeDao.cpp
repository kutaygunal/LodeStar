// core/testforge/TestForgeDao.cpp
#include "core/testforge/TestForgeDao.h"

#include <sqlite3.h>

namespace lodestar::testforge {

namespace {

const char* stepStatus(StepStatus s) { return toString(s); }
const char* runStatus(RunStatus s) { return toString(s); }

void bindText(sqlite3_stmt* stmt, int index, const std::string& value) {
    sqlite3_bind_text(stmt, index, value.c_str(), static_cast<int>(value.size()),
                      SQLITE_TRANSIENT);
}

void bindDouble(sqlite3_stmt* stmt, int index, double value) {
    sqlite3_bind_double(stmt, index, value);
}

void bindInt(sqlite3_stmt* stmt, int index, int value) {
    sqlite3_bind_int(stmt, index, value);
}

std::string columnText(sqlite3_stmt* stmt, int col) {
    const unsigned char* text = sqlite3_column_text(stmt, col);
    return text ? reinterpret_cast<const char*>(text) : std::string();
}

double columnDouble(sqlite3_stmt* stmt, int col) {
    return sqlite3_column_double(stmt, col);
}

int columnInt(sqlite3_stmt* stmt, int col) { return sqlite3_column_int(stmt, col); }

StepStatus parseStepStatus(const std::string& s) {
    if (s == "Passed") return StepStatus::Passed;
    if (s == "Failed") return StepStatus::Failed;
    if (s == "Blocked") return StepStatus::Blocked;
    return StepStatus::Pending;
}

RunStatus parseRunStatus(const std::string& s) {
    if (s == "Running") return RunStatus::Running;
    if (s == "Passed") return RunStatus::Passed;
    if (s == "Failed") return RunStatus::Failed;
    if (s == "Blocked") return RunStatus::Blocked;
    return RunStatus::Pending;
}

}  // namespace

const char* toString(StepStatus s) {
    switch (s) {
        case StepStatus::Pending: return "Pending";
        case StepStatus::Passed: return "Passed";
        case StepStatus::Failed: return "Failed";
        case StepStatus::Blocked: return "Blocked";
    }
    return "Pending";
}

const char* toString(RunStatus s) {
    switch (s) {
        case RunStatus::Pending: return "Pending";
        case RunStatus::Running: return "Running";
        case RunStatus::Passed: return "Passed";
        case RunStatus::Failed: return "Failed";
        case RunStatus::Blocked: return "Blocked";
    }
    return "Pending";
}

common::Result<void> TestForgeDao::saveProcedure(const TestProcedure& p) {
    sqlite3_exec(db_.handle(), "BEGIN;", nullptr, nullptr, nullptr);

    auto rollback = [this](const std::string& msg) {
        sqlite3_exec(db_.handle(), "ROLLBACK;", nullptr, nullptr, nullptr);
        return common::Result<void>::err(msg);
    };

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO test_procedures (id, name, version, objective, scenario_id, status)"
        " VALUES (?, ?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return rollback("prepare procedure failed: " +
                        std::string(sqlite3_errmsg(db_.handle())));
    }
    bindText(stmt, 1, p.id);
    bindText(stmt, 2, p.name);
    bindText(stmt, 3, p.version);
    bindText(stmt, 4, p.objective);
    bindText(stmt, 5, p.scenarioId);
    bindText(stmt, 6, runStatus(p.status));
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return rollback("insert procedure failed: " +
                        std::string(sqlite3_errmsg(db_.handle())));
    }

    const char* stepSql =
        "INSERT INTO test_steps (id, procedure_id, seq, name, description, metric,"
        " expected_value, tolerance) VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    for (const TestStep& s : p.steps) {
        if (sqlite3_prepare_v2(db_.handle(), stepSql, -1, &stmt, nullptr) != SQLITE_OK) {
            return rollback("prepare step failed: " +
                            std::string(sqlite3_errmsg(db_.handle())));
        }
        bindText(stmt, 1, s.id);
        bindText(stmt, 2, p.id);
        bindInt(stmt, 3, s.seq);
        bindText(stmt, 4, s.name);
        bindText(stmt, 5, s.description);
        bindText(stmt, 6, s.metric);
        bindDouble(stmt, 7, s.expectedValue);
        bindDouble(stmt, 8, s.tolerance);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE) {
            return rollback("insert step failed: " +
                            std::string(sqlite3_errmsg(db_.handle())));
        }
    }

    sqlite3_exec(db_.handle(), "COMMIT;", nullptr, nullptr, nullptr);
    return common::Result<void>::ok();
}

common::Result<std::optional<TestProcedure>> TestForgeDao::loadProcedure(
    const std::string& id) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT id, name, version, objective, scenario_id, status FROM test_procedures"
        " WHERE id = ?;";
    if (sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<std::optional<TestProcedure>>::err(
            "prepare procedure select failed: " +
            std::string(sqlite3_errmsg(db_.handle())));
    }
    bindText(stmt, 1, id);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return common::Result<std::optional<TestProcedure>>::ok(std::nullopt);
    }
    TestProcedure p;
    p.id = columnText(stmt, 0);
    p.name = columnText(stmt, 1);
    p.version = columnText(stmt, 2);
    p.objective = columnText(stmt, 3);
    p.scenarioId = columnText(stmt, 4);
    p.status = parseRunStatus(columnText(stmt, 5));
    sqlite3_finalize(stmt);

    const char* stepSql =
        "SELECT id, seq, name, description, metric, expected_value, tolerance"
        " FROM test_steps WHERE procedure_id = ? ORDER BY seq;";
    if (sqlite3_prepare_v2(db_.handle(), stepSql, -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<std::optional<TestProcedure>>::err(
            "prepare step select failed: " +
            std::string(sqlite3_errmsg(db_.handle())));
    }
    bindText(stmt, 1, id);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        TestStep s;
        s.id = columnText(stmt, 0);
        s.seq = columnInt(stmt, 1);
        s.name = columnText(stmt, 2);
        s.description = columnText(stmt, 3);
        s.metric = columnText(stmt, 4);
        s.expectedValue = columnDouble(stmt, 5);
        s.tolerance = columnDouble(stmt, 6);
        p.steps.push_back(std::move(s));
    }
    sqlite3_finalize(stmt);
    return common::Result<std::optional<TestProcedure>>::ok(std::move(p));
}

common::Result<void> TestForgeDao::saveRun(const TestRun& run) {
    sqlite3_exec(db_.handle(), "BEGIN;", nullptr, nullptr, nullptr);

    auto rollback = [this](const std::string& msg) {
        sqlite3_exec(db_.handle(), "ROLLBACK;", nullptr, nullptr, nullptr);
        return common::Result<void>::err(msg);
    };

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO test_runs (id, procedure_id, procedure_name, scenario_id, status,"
        " started_at, finished_at) VALUES (?, ?, ?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return rollback("prepare run insert failed: " +
                        std::string(sqlite3_errmsg(db_.handle())));
    }
    bindText(stmt, 1, run.id);
    bindText(stmt, 2, run.procedureId);
    bindText(stmt, 3, run.procedureName);
    bindText(stmt, 4, run.scenarioId);
    bindText(stmt, 5, runStatus(run.status));
    bindText(stmt, 6, run.startedAt);
    bindText(stmt, 7, run.finishedAt);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return rollback("insert run failed: " +
                        std::string(sqlite3_errmsg(db_.handle())));
    }

    const char* resSql =
        "INSERT INTO step_results (id, run_id, step_id, seq, name, status, actual_value,"
        " expected_value, tolerance, measured, message)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    for (const StepResult& r : run.results) {
        if (sqlite3_prepare_v2(db_.handle(), resSql, -1, &stmt, nullptr) != SQLITE_OK) {
            return rollback("prepare step result insert failed: " +
                            std::string(sqlite3_errmsg(db_.handle())));
        }
        bindText(stmt, 1, run.id + "-" + r.stepId);
        bindText(stmt, 2, run.id);
        bindText(stmt, 3, r.stepId);
        bindInt(stmt, 4, r.seq);
        bindText(stmt, 5, r.name);
        bindText(stmt, 6, stepStatus(r.status));
        bindDouble(stmt, 7, r.actualValue);
        bindDouble(stmt, 8, r.expectedValue);
        bindDouble(stmt, 9, r.tolerance);
        bindInt(stmt, 10, r.measured ? 1 : 0);
        bindText(stmt, 11, r.message);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE) {
            return rollback("insert step result failed: " +
                            std::string(sqlite3_errmsg(db_.handle())));
        }
    }

    sqlite3_exec(db_.handle(), "COMMIT;", nullptr, nullptr, nullptr);
    return common::Result<void>::ok();
}

common::Result<std::optional<TestRun>> TestForgeDao::loadRun(const std::string& id) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT id, procedure_id, procedure_name, scenario_id, status, started_at,"
        " finished_at FROM test_runs WHERE id = ?;";
    if (sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<std::optional<TestRun>>::err(
            "prepare run select failed: " + std::string(sqlite3_errmsg(db_.handle())));
    }
    bindText(stmt, 1, id);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return common::Result<std::optional<TestRun>>::ok(std::nullopt);
    }
    TestRun run;
    run.id = columnText(stmt, 0);
    run.procedureId = columnText(stmt, 1);
    run.procedureName = columnText(stmt, 2);
    run.scenarioId = columnText(stmt, 3);
    run.status = parseRunStatus(columnText(stmt, 4));
    run.startedAt = columnText(stmt, 5);
    run.finishedAt = columnText(stmt, 6);
    sqlite3_finalize(stmt);

    const char* resSql =
        "SELECT step_id, seq, name, status, actual_value, expected_value, tolerance,"
        " measured, message FROM step_results WHERE run_id = ? ORDER BY seq;";
    if (sqlite3_prepare_v2(db_.handle(), resSql, -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<std::optional<TestRun>>::err(
            "prepare step result select failed: " +
            std::string(sqlite3_errmsg(db_.handle())));
    }
    bindText(stmt, 1, id);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        StepResult r;
        r.stepId = columnText(stmt, 0);
        r.seq = columnInt(stmt, 1);
        r.name = columnText(stmt, 2);
        r.status = parseStepStatus(columnText(stmt, 3));
        r.actualValue = columnDouble(stmt, 4);
        r.expectedValue = columnDouble(stmt, 5);
        r.tolerance = columnDouble(stmt, 6);
        r.measured = columnInt(stmt, 7) != 0;
        r.message = columnText(stmt, 8);
        run.results.push_back(std::move(r));
    }
    sqlite3_finalize(stmt);
    return common::Result<std::optional<TestRun>>::ok(std::move(run));
}

}  // namespace lodestar::testforge
