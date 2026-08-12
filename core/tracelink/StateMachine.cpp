// core/tracelink/StateMachine.cpp
// Lifecycle state machines per entity type.

#include "core/tracelink/StateMachine.h"

#include <algorithm>
#include <map>
#include <set>

namespace lodestar::tracelink {

namespace {
const std::vector<std::string> kRequirementStatuses = {
    "Draft", "Proposed", "Approved", "Validated", "Implemented", "Verified", "Obsolete"};
const std::vector<std::string> kDesignStatuses = {
    "Draft", "Reviewed", "Released", "Obsolete"};
const std::vector<std::string> kTestCaseStatuses = {
    "Draft", "Ready", "Executed", "Passed", "Failed", "Obsolete"};
const std::vector<std::string> kInterfaceStatuses = {
    "Draft", "Agreed", "Released", "Changed", "Obsolete"};
const std::vector<std::string> kHazardStatuses = {
    "Identified", "Analyzed", "Mitigated", "Closed", "Obsolete"};
const std::vector<std::string> kDecisionStatuses = {
    "Open", "Decided", "Closed", "Obsolete"};
const std::vector<std::string> kAssumptionStatuses = {
    "Active", "Validated", "Invalid", "Obsolete"};

const std::vector<std::string>& statusSet(EntityType t) {
    switch (t) {
        case EntityType::Requirement: return kRequirementStatuses;
        case EntityType::Design:      return kDesignStatuses;
        case EntityType::TestCase:    return kTestCaseStatuses;
        case EntityType::Interface:   return kInterfaceStatuses;
        case EntityType::Hazard:      return kHazardStatuses;
        case EntityType::Decision:    return kDecisionStatuses;
        case EntityType::Assumption:  return kAssumptionStatuses;
    }
    return kRequirementStatuses;
}

const std::map<EntityType, std::map<std::string, std::set<std::string>>>& transitionTable() {
    static const std::map<EntityType, std::map<std::string, std::set<std::string>>> kTable = {
        {EntityType::Requirement,
         {{"Draft", {"Proposed", "Approved", "Obsolete"}},
          {"Proposed", {"Approved", "Obsolete"}},
          {"Approved", {"Validated", "Obsolete"}},
          {"Validated", {"Implemented", "Obsolete"}},
          {"Implemented", {"Verified", "Obsolete"}},
          {"Verified", {"Obsolete"}},
          {"Obsolete", {}}}},
        {EntityType::Design,
         {{"Draft", {"Reviewed", "Obsolete"}},
          {"Reviewed", {"Released", "Obsolete"}},
          {"Released", {"Obsolete"}},
          {"Obsolete", {}}}},
        {EntityType::TestCase,
         {{"Draft", {"Ready", "Obsolete"}},
          {"Ready", {"Executed", "Obsolete"}},
          {"Executed", {"Passed", "Failed", "Obsolete"}},
          {"Passed", {"Ready", "Executed", "Obsolete"}},   // re-run allowed
          {"Failed", {"Ready", "Executed", "Obsolete"}},   // re-run allowed
          {"Obsolete", {}}}},
        {EntityType::Interface,
         {{"Draft", {"Agreed", "Obsolete"}},
          {"Agreed", {"Released", "Obsolete"}},
          {"Released", {"Changed", "Obsolete"}},
          {"Changed", {"Released", "Obsolete"}},           // re-release allowed
          {"Obsolete", {}}}},
        {EntityType::Hazard,
         {{"Identified", {"Analyzed", "Obsolete"}},
          {"Analyzed", {"Mitigated", "Obsolete"}},
          {"Mitigated", {"Closed", "Obsolete"}},
          {"Closed", {"Obsolete"}},
          {"Obsolete", {}}}},
        {EntityType::Decision,
         {{"Open", {"Decided", "Obsolete"}},
          {"Decided", {"Closed", "Obsolete"}},
          {"Closed", {"Obsolete"}},
          {"Obsolete", {}}}},
        {EntityType::Assumption,
         {{"Active", {"Validated", "Invalid", "Obsolete"}},
          {"Validated", {"Invalid", "Obsolete"}},
          {"Invalid", {"Obsolete"}},
          {"Obsolete", {}}}},
    };
    return kTable;
}
}  // namespace

std::vector<std::string> statuses(EntityType t) {
    return statusSet(t);
}

bool isValidStatus(EntityType t, const std::string& status) {
    const auto& set = statusSet(t);
    return std::find(set.begin(), set.end(), status) != set.end();
}

bool canTransition(EntityType t, const std::string& from, const std::string& to) {
    if (from == to) return true;  // no-op
    auto tableIt = transitionTable().find(t);
    if (tableIt == transitionTable().end()) return false;
    auto fromIt = tableIt->second.find(from);
    if (fromIt == tableIt->second.end()) return false;
    return fromIt->second.count(to) > 0;
}

std::string transitionError(EntityType t, const std::string& from,
                            const std::string& to) {
    if (from == to) return "";
    if (canTransition(t, from, to)) return "";
    return "illegal status transition '" + from + "' -> '" + to + "' for type '" +
           toString(t) + "'";
}

}  // namespace lodestar::tracelink
