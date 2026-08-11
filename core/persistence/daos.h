#pragma once
// core/persistence/daos.h
// Thin DAO layer wrapping SQLite access for each domain. These are the only
// classes that issue SQL against the Lodestar database.

#include <optional>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/Models.h"

namespace lodestar::persistence {

class RequirementDao {
public:
    explicit RequirementDao(Database& db) : db_(db) {}
    common::Result<void> create(const Requirement& r);
    common::Result<std::vector<Requirement>> findAll();
    common::Result<std::optional<Requirement>> findById(const std::string& id);

private:
    Database& db_;
};

class DesignItemDao {
public:
    explicit DesignItemDao(Database& db) : db_(db) {}
    common::Result<void> create(const DesignItem& d);
    common::Result<std::vector<DesignItem>> findAll();

private:
    Database& db_;
};

class InterfaceDao {
public:
    explicit InterfaceDao(Database& db) : db_(db) {}
    common::Result<void> create(const InterfaceDef& i);
    common::Result<std::vector<InterfaceDef>> findAll();

private:
    Database& db_;
};

class TestCaseDao {
public:
    explicit TestCaseDao(Database& db) : db_(db) {}
    common::Result<void> create(const TestCase& t);
    common::Result<std::vector<TestCase>> findAll();
    common::Result<std::optional<TestCase>> findById(const std::string& id);

private:
    Database& db_;
};

class TraceLinkDao {
public:
    explicit TraceLinkDao(Database& db) : db_(db) {}
    common::Result<void> create(const TraceLink& link);
    common::Result<std::vector<TraceLink>> findAll();
    common::Result<std::vector<TraceLink>> findBySource(const std::string& sourceType,
                                                        const std::string& sourceId);
    common::Result<std::vector<TraceLink>> findByTarget(const std::string& targetType,
                                                        const std::string& targetId);

private:
    Database& db_;
};

class ScenarioDao {
public:
    explicit ScenarioDao(Database& db) : db_(db) {}
    common::Result<void> create(const Scenario& s);
    common::Result<std::vector<Scenario>> findAll();

private:
    Database& db_;
};

}  // namespace lodestar::persistence
