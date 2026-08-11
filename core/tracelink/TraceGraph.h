#pragma once
// core/tracelink/TraceGraph.h
// Graph model linking requirements, design, interfaces, and test cases.
// Add/query operations are backed by the persistence DAO layer.

#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/Models.h"
#include "core/persistence/daos.h"

namespace lodestar::tracelink {

class TraceGraph {
public:
    explicit TraceGraph(persistence::Database& db);

    // Entity creation (assigns a UUID to the entity if its id is empty).
    common::Result<void> addRequirement(persistence::Requirement& r);
    common::Result<void> addDesignItem(persistence::DesignItem& d);
    common::Result<void> addInterface(persistence::InterfaceDef& i);
    common::Result<void> addTestCase(persistence::TestCase& t);

    // Adds a directed edge in the trace graph.
    common::Result<void> addLink(persistence::TraceLink& link);

    // Queries.
    common::Result<std::vector<persistence::TraceLink>> linksFrom(
        const std::string& type, const std::string& id);
    common::Result<std::vector<persistence::TraceLink>> linksTo(
        const std::string& type, const std::string& id);
    common::Result<std::vector<persistence::Requirement>> requirements();
    common::Result<std::vector<persistence::TestCase>> testCases();

private:
    persistence::Database& db_;
    persistence::RequirementDao reqDao_;
    persistence::DesignItemDao designDao_;
    persistence::InterfaceDao ifaceDao_;
    persistence::TestCaseDao testDao_;
    persistence::TraceLinkDao linkDao_;
};

}  // namespace lodestar::tracelink
