#pragma once
// core/tracelink/Types.h
// Typed domain vocabulary for TraceLink: entity kinds, relation enums, the
// canonical relation map + reverse mapping (consumed by WP-2 traversal), plus
// the generic Entity / Link views used by TraceLinkService.

#include <optional>
#include <string>
#include <vector>

namespace lodestar::tracelink {

// ---------------------------------------------------------------------------
// Entity kinds. The four core kinds match the WP-1 contract; hazard, decision
// and assumption are supported as the domain model grows.
// ---------------------------------------------------------------------------
enum class EntityType {
    Requirement,
    Design,
    Interface,
    TestCase,
    Hazard,
    Decision,
    Assumption
};

std::string toString(EntityType t);
std::optional<EntityType> entityTypeFromString(const std::string& s);

// ---------------------------------------------------------------------------
// Typed relations (the canonical relation map).
// ---------------------------------------------------------------------------
enum class Relation {
    Satisfies,   // source meets the target          design -> requirement
    Verifies,    // source proves target             test_case -> requirement
    Derives,     // source comes from target         requirement -> higher requirement
    Allocates,   // source assigned to target        requirement -> design
    Refines,     // source is a detail of target     requirement -> requirement
    Decomposes,  // source splits into target        design -> design
    DependsOn,   // source needs target              design -> interface
    TracesTo,    // generic trace (legacy)           any -> any
    Validates,   // source checks target             test_case -> design
    Conflicts    // source conflicts with target     any -> any
};

std::string toString(Relation r);
std::optional<Relation> relationFromString(const std::string& s);

// All defined relations in canonical order.
const std::vector<Relation>& allRelations();

// Reverse mapping: e.g. Verifies -> "is_verified_by". Required by WP-2 so that
// traversal can walk a link in the opposite direction without a second edge.
std::string reverseRelationName(Relation r);

// Whether `relation` is a permitted edge from an entity of type `src` to one
// of type `tgt`. Integrity-on-write rejects disallowed relation-type pairs.
bool isRelationAllowed(EntityType src, EntityType tgt, Relation relation);

// ---------------------------------------------------------------------------
// Generic entity view (a superset of all typed attributes). The service layer
// works on this and converts to/from the typed persistence models.
// ---------------------------------------------------------------------------
struct Entity {
    std::string id;             // internal UUID
    std::string externalId;     // human id, e.g. "REQ-100"; unique per type
    EntityType  type = EntityType::Requirement;
    std::string name;
    std::string text;           // full body
    std::string status = "Draft";
    std::string typeAttr;       // type-specific "type" (functional/component/...)
    std::string priority;
    std::string source;
    std::string owner;
    std::string rationale;
    std::string verificationMethod;
    std::string safetyLevel;
    std::string direction;
    std::string sourceEntity;
    std::string targetEntity;
    std::string dataItems;
    std::string protocol;
    std::string resultStatus;
    std::string severity;
    std::string likelihood;
    std::string date;
    std::string parentId;
    int sortOrder = 0;
    std::string tags;
    int version = 1;
    std::string createdBy;
    std::string createdAt;
    std::string updatedBy;
    std::string updatedAt;
};

// A typed directed edge.
struct Link {
    std::string id;
    EntityType  sourceType;
    std::string sourceId;
    EntityType  targetType;
    std::string targetId;
    std::string relation = "traces_to";   // one of the relation names
    std::string rationale;
    std::string status = "Active";        // Active | Proposed | Superseded
    std::string createdBy;
    std::string createdAt;
    std::string updatedAt;
    int version = 1;
    std::string supersededBy;
    std::string validFrom;
    std::string validTo;
};

// List/filter criteria for entity queries.
struct EntityFilter {
    std::optional<std::string> status;
    std::optional<std::string> tags;    // substring match on the tags column
    std::optional<std::string> text;    // substring match on name + description
    int limit = 0;
    int offset = 0;
};

}  // namespace lodestar::tracelink
