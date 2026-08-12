#pragma once
// core/tracelink/RulesEngine.h
// WP-3 compliance rules engine: a data-driven validation engine built on the
// TraceLinkService graph model. Rules are stored as data (compliance_rules),
// evaluated against the current graph, and results are persisted to
// validation_runs + compliance_violations (migration 007).
//
// Contract written by the scrum-master in core/test/wp3_rules_engine_tests.cpp.
// Built-in rule templates (docs/tracelink-plan.md, section 4.4):
//   REQ_MUST_BE_VERIFIED   every active requirement needs >=1 verifies link
//   REQ_MUST_BE_SATISFIED  every active requirement needs >=1 satisfies link
//   NO_DANGLING_LINKS      no link to a deleted/nonexistent/Obsolete entity
//   NO_DUPLICATE_LINKS     no identical (src, tgt, relation) pairs
//   NO_SELF_LINKS          no link where src == tgt
//   BIDIRECTIONAL          a link requires its reciprocal link (optional)
//   COVERAGE_MIN (param)   requirement coverage >= N% (min_percent)
//   NO_ORPHAN_DESIGN       every design item has an outgoing satisfies link
//   STATUS_VALID           every entity status is a legal state
//
// Each rule carries one or more assurance standard tags (ARP4754A, ARP4761,
// DO-178C, DO-254). Only enabled rules are evaluated by runValidation().

#include <map>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/Models.h"
#include "core/tracelink/Types.h"

namespace lodestar::tracelink {

// ---------------------------------------------------------------------------
// Severity of a rule / violation.
// ---------------------------------------------------------------------------
enum class Severity { Info, Warning, Error, Critical };

std::string toString(Severity s);
std::optional<Severity> severityFromString(const std::string& s);

// ---------------------------------------------------------------------------
// Data model (mirrors migration 007 tables).
// ---------------------------------------------------------------------------

// A stored compliance rule (a row in compliance_rules).
struct Rule {
    std::string id;                            // UUID; assigned by defineRule if empty
    std::string name;                          // display name
    std::string ruleType;                      // built-in template key
    std::map<std::string, std::string> params; // e.g. COVERAGE_MIN {"min_percent":"100"}
    Severity severity = Severity::Error;
    std::vector<std::string> standards;        // ARP4754A / ARP4761 / DO-178C / DO-254
    bool enabled = true;
};

// One recorded rule violation (a row in compliance_violations).
struct Violation {
    std::string id;
    std::string runId;
    std::string ruleId;
    std::string ruleName;
    std::string ruleType;
    std::vector<std::string> standards;        // copied from the rule (tagging)
    EntityType entityType = EntityType::Requirement;
    std::string entityId;                      // offending entity
    std::string entityExternalId;
    std::string message;
    Severity severity = Severity::Error;
};

// Result of one runValidation() invocation.
struct ValidationRun {
    std::string id;
    std::string name;
    std::string status;                        // "ok" | "violations"
    std::string summary;
    int violationCount = 0;
    std::vector<Violation> violations;         // every violation written this run
};

// ---------------------------------------------------------------------------
// RulesEngine
// ---------------------------------------------------------------------------
class RulesEngine {
public:
    explicit RulesEngine(persistence::Database& db);

    // Persists a rule. Assigns a UUID to rule.id if empty. Returns the rule.
    common::Result<Rule> defineRule(const Rule& rule);

    common::Result<std::vector<Rule>> listRules();

    // Toggle a rule on/off. Disabled rules are skipped by runValidation.
    common::Result<void> enableRule(const std::string& ruleId, bool enabled);

    // Evaluates every ENABLED rule against the current graph, writes a
    // validation_run plus compliance_violations, and returns the report.
    // A run is always recorded; status is "ok" with zero violations or
    // "violations" otherwise.
    common::Result<ValidationRun> runValidation();

    // Metadata for the built-in rule templates (documentation / seeding helper,
    // NOT auto-registered by the engine).
    struct Template {
        std::string name;
        std::string description;
        std::vector<std::string> standards;
        std::string defaultParams;   // JSON-ish, e.g. {"min_percent":100}
    };
    static const std::vector<Template>& builtInTemplates();

private:
    common::Result<std::vector<Rule>> loadRules(bool onlyEnabled);

    // Rule evaluators. Each appends to `out`.
    void evalReqVerified(const Rule& r, std::vector<Violation>& out);
    void evalReqSatisfied(const Rule& r, std::vector<Violation>& out);
    void evalNoDangling(const Rule& r, std::vector<Violation>& out);
    void evalNoDuplicates(const Rule& r, std::vector<Violation>& out);
    void evalNoSelfLinks(const Rule& r, std::vector<Violation>& out);
    void evalBidirectional(const Rule& r, std::vector<Violation>& out);
    void evalCoverageMin(const Rule& r, std::vector<Violation>& out);
    void evalNoOrphanDesign(const Rule& r, std::vector<Violation>& out);
    void evalStatusValid(const Rule& r, std::vector<Violation>& out);

    Violation makeViolation(const Rule& r, EntityType type,
                            const std::string& entityId,
                            const std::string& externalId,
                            const std::string& message) const;

    // Loads all active (non-Obsolete) entities as (type,id,status,externalId).
    struct EntityRef {
        EntityType type;
        std::string id;
        std::string status;
        std::string externalId;
    };
    std::vector<EntityRef> loadAllEntities() const;
    std::vector<persistence::TraceLink> loadActiveLinks() const;
    // Whether an entity exists AND is not Obsolete.
    bool isActiveEntity(EntityType type, const std::string& id) const;

    persistence::Database& db_;
};

}  // namespace lodestar::tracelink
