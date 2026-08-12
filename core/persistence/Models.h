#pragma once
// core/persistence/Models.h
// Plain data structures for the core entities persisted by the DAO layer.
// These are the rich, typed domain models produced by WP-1 (migrations 003/004).

#include <string>

namespace lodestar::persistence {

// ---------------------------------------------------------------------------
// Shared attribute helpers (declared inline so structs stay plain).
// ---------------------------------------------------------------------------

struct Requirement {
    std::string id;
    std::string externalId;
    std::string name;
    std::string description;   // full body text
    std::string type = "functional";
    std::string status = "Draft";
    std::string priority = "Medium";
    std::string source;
    std::string owner;
    std::string rationale;
    std::string verificationMethod;
    std::string safetyLevel;
    std::string parentId;
    int sortOrder = 0;
    std::string tags;
    int version = 1;
    std::string createdBy;
    std::string createdAt;
    std::string updatedBy;
    std::string updatedAt;
};

struct DesignItem {
    std::string id;
    std::string externalId;
    std::string name;
    std::string description;
    std::string type = "component";   // subsystem / component / module
    std::string status = "Draft";
    std::string owner;
    std::string parentId;
    std::string tags;
    int version = 1;
    std::string createdBy;
    std::string createdAt;
    std::string updatedBy;
    std::string updatedAt;
};

struct InterfaceDef {
    std::string id;
    std::string externalId;
    std::string name;
    std::string description;
    std::string status = "Draft";
    std::string direction = "bidirectional";
    std::string sourceEntity;
    std::string targetEntity;
    std::string dataItems;
    std::string protocol;
    std::string tags;
    int version = 1;
    std::string createdBy;
    std::string createdAt;
    std::string updatedBy;
    std::string updatedAt;
};

struct TestCase {
    std::string id;
    std::string externalId;
    std::string name;
    std::string description;
    std::string status = "Draft";
    std::string verificationMethod;
    std::string resultStatus = "NotExecuted";
    std::string priority = "Medium";
    std::string tags;
    int version = 1;
    std::string createdBy;
    std::string createdAt;
    std::string updatedBy;
    std::string updatedAt;
};

struct Hazard {
    std::string id;
    std::string externalId;
    std::string name;
    std::string description;
    std::string status = "Identified";
    std::string severity;
    std::string likelihood;
    std::string owner;
    std::string tags;
    int version = 1;
    std::string createdBy;
    std::string createdAt;
    std::string updatedBy;
    std::string updatedAt;
};

struct Decision {
    std::string id;
    std::string externalId;
    std::string name;
    std::string description;
    std::string status = "Open";
    std::string rationale;
    std::string owner;
    std::string date;
    std::string tags;
    int version = 1;
    std::string createdBy;
    std::string createdAt;
    std::string updatedBy;
    std::string updatedAt;
};

struct Assumption {
    std::string id;
    std::string externalId;
    std::string name;
    std::string description;
    std::string status = "Active";
    std::string owner;
    std::string tags;
    int version = 1;
    std::string createdBy;
    std::string createdAt;
    std::string updatedBy;
    std::string updatedAt;
};

// A directed edge in the trace graph. source/target types are one of the
// entity type strings ("requirement", "design", "interface", "test_case",
// "hazard", "decision", "assumption").
struct TraceLink {
    std::string id;
    std::string sourceType;
    std::string sourceId;
    std::string targetType;
    std::string targetId;
    std::string relation = "traces_to";
    std::string rationale;
    std::string status = "Active";       // Active / Proposed / Superseded
    std::string createdBy;
    std::string createdAt;
    std::string updatedAt;
    int version = 1;
    std::string supersededBy;
    std::string validFrom;
    std::string validTo;
};

struct Scenario {
    std::string id;
    std::string name;
    std::string description;
    std::string status = "Draft";
};

}  // namespace lodestar::persistence
