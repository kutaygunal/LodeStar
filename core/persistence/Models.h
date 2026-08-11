#pragma once
// core/persistence/Models.h
// Plain data structures for the core entities persisted by the DAO layer.

#include <string>

namespace lodestar::persistence {

struct Requirement {
    std::string id;
    std::string name;
    std::string description;
    std::string status = "Draft";
};

struct DesignItem {
    std::string id;
    std::string name;
    std::string description;
};

struct InterfaceDef {
    std::string id;
    std::string name;
    std::string description;
};

struct TestCase {
    std::string id;
    std::string name;
    std::string description;
    std::string status = "Draft";
};

// A directed edge in the trace graph. source/target types are one of
// "requirement", "design", "interface", "test_case".
struct TraceLink {
    std::string id;
    std::string sourceType;
    std::string sourceId;
    std::string targetType;
    std::string targetId;
    std::string relation = "traces_to";
};

struct Scenario {
    std::string id;
    std::string name;
    std::string description;
    std::string status = "Draft";
};

}  // namespace lodestar::persistence
