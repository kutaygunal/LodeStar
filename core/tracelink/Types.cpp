// core/tracelink/Types.cpp
// Implementations of the TraceLink domain vocabulary: string/enum conversions,
// the canonical relation map + reverse mapping, and relation-type permission.

#include "core/tracelink/Types.h"

#include <map>

namespace lodestar::tracelink {

// ---------------------------------------------------------------------------
// EntityType
// ---------------------------------------------------------------------------
std::string toString(EntityType t) {
    switch (t) {
        case EntityType::Requirement: return "requirement";
        case EntityType::Design:      return "design";
        case EntityType::Interface:   return "interface";
        case EntityType::TestCase:    return "test_case";
        case EntityType::Hazard:      return "hazard";
        case EntityType::Decision:    return "decision";
        case EntityType::Assumption:  return "assumption";
    }
    return "unknown";
}

std::optional<EntityType> entityTypeFromString(const std::string& s) {
    static const std::map<std::string, EntityType> kMap = {
        {"requirement", EntityType::Requirement},
        {"design",      EntityType::Design},
        {"interface",   EntityType::Interface},
        {"test_case",   EntityType::TestCase},
        {"hazard",      EntityType::Hazard},
        {"decision",    EntityType::Decision},
        {"assumption",  EntityType::Assumption},
    };
    auto it = kMap.find(s);
    if (it == kMap.end()) return std::nullopt;
    return it->second;
}

// ---------------------------------------------------------------------------
// Relation
// ---------------------------------------------------------------------------
namespace {
const std::vector<Relation> kAllRelations = {
    Relation::Satisfies, Relation::Verifies, Relation::Derives,
    Relation::Allocates, Relation::Refines,  Relation::Decomposes,
    Relation::DependsOn, Relation::TracesTo, Relation::Validates,
    Relation::Conflicts};

struct RelationInfo {
    const char* name;
    const char* reverse;
};

const std::map<Relation, RelationInfo>& relationTable() {
    static const std::map<Relation, RelationInfo> kTable = {
        {Relation::Satisfies,  {"satisfies",  "is_satisfied_by"}},
        {Relation::Verifies,   {"verifies",   "is_verified_by"}},
        {Relation::Derives,    {"derives",    "is_derived_from"}},
        {Relation::Allocates,  {"allocates",  "is_allocated_to"}},
        {Relation::Refines,    {"refines",    "is_refined_by"}},
        {Relation::Decomposes, {"decomposes", "is_decomposed_into"}},
        {Relation::DependsOn,  {"depends_on", "is_depended_on_by"}},
        {Relation::TracesTo,   {"traces_to",  "is_traced_from"}},
        {Relation::Validates,  {"validates",  "is_validated_by"}},
        {Relation::Conflicts,  {"conflicts",  "conflicts_with"}},
    };
    return kTable;
}
}  // namespace

std::string toString(Relation r) {
    auto it = relationTable().find(r);
    return it == relationTable().end() ? "unknown" : it->second.name;
}

std::optional<Relation> relationFromString(const std::string& s) {
    for (const auto& [rel, info] : relationTable()) {
        if (s == info.name) return rel;
    }
    return std::nullopt;
}

const std::vector<Relation>& allRelations() {
    return kAllRelations;
}

std::string reverseRelationName(Relation r) {
    auto it = relationTable().find(r);
    return it == relationTable().end() ? "unknown" : it->second.reverse;
}

// ---------------------------------------------------------------------------
// Relation-type permission.
// ---------------------------------------------------------------------------
bool isRelationAllowed(EntityType src, EntityType tgt, Relation relation) {
    // Generic relations are permitted between any pair.
    if (relation == Relation::TracesTo || relation == Relation::Conflicts) {
        return true;
    }
    switch (relation) {
        case Relation::Satisfies:
            return src == EntityType::Design && tgt == EntityType::Requirement;
        case Relation::Verifies:
            return src == EntityType::TestCase && tgt == EntityType::Requirement;
        case Relation::Derives:
            return src == EntityType::Requirement && tgt == EntityType::Requirement;
        case Relation::Allocates:
            return src == EntityType::Requirement && tgt == EntityType::Design;
        case Relation::Refines:
            return src == EntityType::Requirement && tgt == EntityType::Requirement;
        case Relation::Decomposes:
            return src == EntityType::Design && tgt == EntityType::Design;
        case Relation::DependsOn:
            return src == EntityType::Design && tgt == EntityType::Interface;
        case Relation::Validates:
            return src == EntityType::TestCase && tgt == EntityType::Design;
        default:
            return false;
    }
}

}  // namespace lodestar::tracelink
