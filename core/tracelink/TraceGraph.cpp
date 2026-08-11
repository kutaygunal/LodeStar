// core/tracelink/TraceGraph.cpp
#include "core/tracelink/TraceGraph.h"

#include "core/common/Uuid.h"

namespace lodestar::tracelink {

TraceGraph::TraceGraph(persistence::Database& db)
    : db_(db),
      reqDao_(db),
      designDao_(db),
      ifaceDao_(db),
      testDao_(db),
      linkDao_(db) {}

common::Result<void> TraceGraph::addRequirement(persistence::Requirement& r) {
    if (r.id.empty()) {
        r.id = common::newUuid();
    }
    return reqDao_.create(r);
}

common::Result<void> TraceGraph::addDesignItem(persistence::DesignItem& d) {
    if (d.id.empty()) {
        d.id = common::newUuid();
    }
    return designDao_.create(d);
}

common::Result<void> TraceGraph::addInterface(persistence::InterfaceDef& i) {
    if (i.id.empty()) {
        i.id = common::newUuid();
    }
    return ifaceDao_.create(i);
}

common::Result<void> TraceGraph::addTestCase(persistence::TestCase& t) {
    if (t.id.empty()) {
        t.id = common::newUuid();
    }
    return testDao_.create(t);
}

common::Result<void> TraceGraph::addLink(persistence::TraceLink& link) {
    if (link.id.empty()) {
        link.id = common::newUuid();
    }
    return linkDao_.create(link);
}

common::Result<std::vector<persistence::TraceLink>> TraceGraph::linksFrom(
    const std::string& type, const std::string& id) {
    return linkDao_.findBySource(type, id);
}

common::Result<std::vector<persistence::TraceLink>> TraceGraph::linksTo(
    const std::string& type, const std::string& id) {
    return linkDao_.findByTarget(type, id);
}

common::Result<std::vector<persistence::Requirement>> TraceGraph::requirements() {
    return reqDao_.findAll();
}

common::Result<std::vector<persistence::TestCase>> TraceGraph::testCases() {
    return testDao_.findAll();
}

}  // namespace lodestar::tracelink
