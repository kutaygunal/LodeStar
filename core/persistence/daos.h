#pragma once
// core/persistence/daos.h
// Thin DAO layer wrapping SQLite access for each domain. These are the only
// classes that issue SQL against the Lodestar database.
//
// WP-1: CRUD expanded to create / findById / update / soft-delete /
// findByFilters / search for every TraceLink entity type, plus link metadata.

#include <optional>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/Models.h"

namespace lodestar::persistence {

// Filter criteria shared by entity queries.
struct EntityFilter {
    std::string status;
    std::string tags;    // substring match on the tags column
    std::string text;    // substring match on name + description
    int limit = 0;
    int offset = 0;
};

class RequirementDao {
public:
    explicit RequirementDao(Database& db) : db_(db) {}
    common::Result<void> create(const Requirement& r);
    common::Result<std::optional<Requirement>> findById(const std::string& id);
    common::Result<std::optional<Requirement>> findByExternalId(const std::string& extId);
    common::Result<std::vector<Requirement>> findAll();
    common::Result<void> update(const Requirement& r);
    common::Result<void> softDelete(const std::string& id);
    common::Result<std::vector<Requirement>> findByFilters(const EntityFilter& f);
    common::Result<std::vector<Requirement>> search(const std::string& text);

private:
    Database& db_;
};

class DesignItemDao {
public:
    explicit DesignItemDao(Database& db) : db_(db) {}
    common::Result<void> create(const DesignItem& d);
    common::Result<std::optional<DesignItem>> findById(const std::string& id);
    common::Result<std::optional<DesignItem>> findByExternalId(const std::string& extId);
    common::Result<std::vector<DesignItem>> findAll();
    common::Result<void> update(const DesignItem& d);
    common::Result<void> softDelete(const std::string& id);
    common::Result<std::vector<DesignItem>> findByFilters(const EntityFilter& f);
    common::Result<std::vector<DesignItem>> search(const std::string& text);

private:
    Database& db_;
};

class InterfaceDao {
public:
    explicit InterfaceDao(Database& db) : db_(db) {}
    common::Result<void> create(const InterfaceDef& i);
    common::Result<std::optional<InterfaceDef>> findById(const std::string& id);
    common::Result<std::optional<InterfaceDef>> findByExternalId(const std::string& extId);
    common::Result<std::vector<InterfaceDef>> findAll();
    common::Result<void> update(const InterfaceDef& i);
    common::Result<void> softDelete(const std::string& id);
    common::Result<std::vector<InterfaceDef>> findByFilters(const EntityFilter& f);
    common::Result<std::vector<InterfaceDef>> search(const std::string& text);

private:
    Database& db_;
};

class TestCaseDao {
public:
    explicit TestCaseDao(Database& db) : db_(db) {}
    common::Result<void> create(const TestCase& t);
    common::Result<std::optional<TestCase>> findById(const std::string& id);
    common::Result<std::optional<TestCase>> findByExternalId(const std::string& extId);
    common::Result<std::vector<TestCase>> findAll();
    common::Result<void> update(const TestCase& t);
    common::Result<void> softDelete(const std::string& id);
    common::Result<std::vector<TestCase>> findByFilters(const EntityFilter& f);
    common::Result<std::vector<TestCase>> search(const std::string& text);

private:
    Database& db_;
};

class HazardDao {
public:
    explicit HazardDao(Database& db) : db_(db) {}
    common::Result<void> create(const Hazard& h);
    common::Result<std::optional<Hazard>> findById(const std::string& id);
    common::Result<std::optional<Hazard>> findByExternalId(const std::string& extId);
    common::Result<std::vector<Hazard>> findAll();
    common::Result<void> update(const Hazard& h);
    common::Result<void> softDelete(const std::string& id);
    common::Result<std::vector<Hazard>> findByFilters(const EntityFilter& f);
    common::Result<std::vector<Hazard>> search(const std::string& text);

private:
    Database& db_;
};

class DecisionDao {
public:
    explicit DecisionDao(Database& db) : db_(db) {}
    common::Result<void> create(const Decision& d);
    common::Result<std::optional<Decision>> findById(const std::string& id);
    common::Result<std::optional<Decision>> findByExternalId(const std::string& extId);
    common::Result<std::vector<Decision>> findAll();
    common::Result<void> update(const Decision& d);
    common::Result<void> softDelete(const std::string& id);
    common::Result<std::vector<Decision>> findByFilters(const EntityFilter& f);
    common::Result<std::vector<Decision>> search(const std::string& text);

private:
    Database& db_;
};

class AssumptionDao {
public:
    explicit AssumptionDao(Database& db) : db_(db) {}
    common::Result<void> create(const Assumption& a);
    common::Result<std::optional<Assumption>> findById(const std::string& id);
    common::Result<std::optional<Assumption>> findByExternalId(const std::string& extId);
    common::Result<std::vector<Assumption>> findAll();
    common::Result<void> update(const Assumption& a);
    common::Result<void> softDelete(const std::string& id);
    common::Result<std::vector<Assumption>> findByFilters(const EntityFilter& f);
    common::Result<std::vector<Assumption>> search(const std::string& text);

private:
    Database& db_;
};

class TraceLinkDao {
public:
    explicit TraceLinkDao(Database& db) : db_(db) {}
    common::Result<void> create(const TraceLink& link);
    common::Result<std::optional<TraceLink>> findById(const std::string& id);
    common::Result<std::vector<TraceLink>> findAll();
    common::Result<std::vector<TraceLink>> findBySource(const std::string& sourceType,
                                                        const std::string& sourceId);
    common::Result<std::vector<TraceLink>> findByTarget(const std::string& targetType,
                                                        const std::string& targetId);
    // Whether an Active link with the exact (src, tgt, relation) triple exists.
    common::Result<bool> existsActive(const std::string& sourceType,
                                      const std::string& sourceId,
                                      const std::string& targetType,
                                      const std::string& targetId,
                                      const std::string& relation);
    common::Result<void> update(const TraceLink& link);
    common::Result<void> softDelete(const std::string& id);

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
