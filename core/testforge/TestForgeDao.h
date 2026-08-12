#pragma once
// core/testforge/TestForgeDao.h
// Persistence for the TestForge domain: save/load test procedures (with steps)
// and test runs (with step results). Issues SQL only against the Lodestar
// SQLite database, matching the persistence DAO convention.

#include <optional>
#include <string>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/testforge/Models.h"

namespace lodestar::testforge {

class TestForgeDao {
public:
    explicit TestForgeDao(persistence::Database& db) : db_(db) {}

    // Saves a procedure and all its steps inside a single transaction.
    common::Result<void> saveProcedure(const TestProcedure& p);

    // Loads a procedure with its steps; returns nullopt if not found.
    common::Result<std::optional<TestProcedure>> loadProcedure(const std::string& id);

    // Saves a run and all its step results inside a single transaction.
    common::Result<void> saveRun(const TestRun& run);

    // Loads a run with its step results; returns nullopt if not found.
    common::Result<std::optional<TestRun>> loadRun(const std::string& id);

private:
    persistence::Database& db_;
};

}  // namespace lodestar::testforge
