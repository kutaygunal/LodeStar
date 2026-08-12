#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

// core/tracelink/RulesEngine.cpp
// WP-3 compliance rules engine: data-driven rule definition, evaluation, and
// persisted validation runs + violations (migration 007).

#include "core/tracelink/RulesEngine.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <set>

#include <sqlite3.h>

#include "core/common/Uuid.h"
#include "core/persistence/daos.h"
#include "core/tracelink/StateMachine.h"

namespace lodestar::tracelink {

using common::newUuid;
using persistence::Assumption;
using persistence::Decision;
using persistence::DesignItem;
using persistence::Hazard;
using persistence::InterfaceDef;
using persistence::Requirement;
using persistence::TestCase;
using persistence::TraceLink;

// ---------------------------------------------------------------------------
// Severity
// ---------------------------------------------------------------------------
std::string toString(Severity s) {
    switch (s) {
        case Severity::Info:     return "Info";
        case Severity::Warning:  return "Warning";
        case Severity::Error:    return "Error";
        case Severity::Critical: return "Critical";
    }
    return "Error";
}

std::optional<Severity> severityFromString(const std::string& s) {
    if (s == "Info")     return Severity::Info;
    if (s == "Warning")  return Severity::Warning;
    if (s == "Error")    return Severity::Error;
    if (s == "Critical") return Severity::Critical;
    return std::nullopt;
}

namespace {

std::string now() {
    char buf[32];
    const auto t = std::time(nullptr);
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
    return buf;
}

void bindText(sqlite3_stmt* stmt, int index, const std::string& value) {
    sqlite3_bind_text(stmt, index, value.c_str(), static_cast<int>(value.size()),
                      SQLITE_TRANSIENT);
}

std::string columnText(sqlite3_stmt* stmt, int col) {
    const unsigned char* text = sqlite3_column_text(stmt, col);
    return text ? reinterpret_cast<const char*>(text) : std::string();
}

using Row = std::map<std::string, std::string>;
common::Result<std::vector<Row>> queryRows(sqlite3* db, const std::string& sql,
                                           const std::vector<std::string>& columns,
                                           const std::vector<std::string>& params = {}) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<std::vector<Row>>::err(
            "prepare failed: " + std::string(sqlite3_errmsg(db)));
    }
    for (size_t i = 0; i < params.size(); ++i) {
        bindText(stmt, static_cast<int>(i + 1), params[i]);
    }
    std::vector<Row> out;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Row row;
        for (size_t c = 0; c < columns.size(); ++c) {
            row[columns[c]] = columnText(stmt, static_cast<int>(c));
        }
        out.push_back(std::move(row));
    }
    sqlite3_finalize(stmt);
    return common::Result<std::vector<Row>>::ok(std::move(out));
}

common::Result<void> execBinds(sqlite3* db, const std::string& sql,
                               const std::vector<std::string>& params = {}) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<void>::err("prepare failed: " +
                                         std::string(sqlite3_errmsg(db)));
    }
    for (size_t i = 0; i < params.size(); ++i) {
        bindText(stmt, static_cast<int>(i + 1), params[i]);
    }
    int rc = sqlite3_step(stmt);
    std::string msg = sqlite3_errmsg(db);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return common::Result<void>::err("step failed: " + msg);
    }
    return common::Result<void>::ok();
}

// ---- standards (stored as a comma-joined string in the single `standard` col)
std::string serializeStandards(const std::vector<std::string>& standards) {
    std::string out;
    for (size_t i = 0; i < standards.size(); ++i) {
        if (i) out += ",";
        out += standards[i];
    }
    return out;
}

std::vector<std::string> parseStandards(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ',') {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else if (!std::isspace(static_cast<unsigned char>(c))) {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

// ---- params (stored as a JSON-ish object string, e.g. {"min_percent":"100"})
std::string serializeParams(const std::map<std::string, std::string>& params) {
    if (params.empty()) return "{}";
    std::string out = "{";
    bool first = true;
    for (const auto& [k, v] : params) {
        if (!first) out += ",";
        first = false;
        out += "\"" + k + "\":\"" + v + "\"";
    }
    out += "}";
    return out;
}

// Parses a JSON-ish object string into key/value pairs. Tolerates quoted and
// bare keys/values, e.g. {"min_percent":100} or {"min_percent":"100"}.
std::map<std::string, std::string> parseParams(const std::string& s) {
    std::map<std::string, std::string> out;
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '{' || s[i] == '}' || s[i] == ',' || s[i] == ' ') {
            ++i;
            continue;
        }
        if (s[i] == '"') ++i;  // optional opening quote for key
        std::string key;
        while (i < s.size() && s[i] != '"' && s[i] != ':') key.push_back(s[i++]);
        if (i < s.size() && s[i] == '"') ++i;   // closing key quote
        if (i < s.size() && s[i] == ':') ++i;   // colon
        // value
        if (i < s.size() && s[i] == '"') ++i;   // opening value quote
        std::string val;
        while (i < s.size() && s[i] != '"' && s[i] != ',' && s[i] != '}') {
            val.push_back(s[i++]);
        }
        if (i < s.size() && s[i] == '"') ++i;   // closing value quote
        if (!key.empty()) out[key] = val;
    }
    return out;
}

}  // namespace

// ---------------------------------------------------------------------------
// Built-in templates
// ---------------------------------------------------------------------------
const std::vector<RulesEngine::Template>& RulesEngine::builtInTemplates() {
    static const std::vector<Template> kTemplates = {
        {"REQ_MUST_BE_VERIFIED",
         "Every active requirement must have at least one 'verifies' link.",
         {"ARP4754A", "DO-178C"}, "{}"},
        {"REQ_MUST_BE_SATISFIED",
         "Every active requirement must have at least one 'satisfies' link.",
         {"ARP4754A"}, "{}"},
        {"NO_DANGLING_LINKS",
         "No link may reference a deleted, non-existent, or Obsolete entity.",
         {"ARP4754A", "ARP4761", "DO-178C", "DO-254"}, "{}"},
        {"NO_DUPLICATE_LINKS",
         "No identical (source, target, relation) link pairs may exist.",
         {"ARP4754A"}, "{}"},
        {"NO_SELF_LINKS",
         "No link may have the same source and target entity.",
         {"ARP4754A"}, "{}"},
        {"BIDIRECTIONAL",
         "Every link must have a reciprocal link in the opposite direction.",
         {"ARP4754A"}, "{}"},
        {"COVERAGE_MIN",
         "Requirement coverage must be at least min_percent percent.",
         {"DO-178C", "DO-254"}, "{\"min_percent\":100}"},
        {"NO_ORPHAN_DESIGN",
         "Every active design item must have an outgoing 'satisfies' link.",
         {"ARP4754A"}, "{}"},
        {"STATUS_VALID",
         "Every entity status must be a legal state for its type.",
         {"ARP4754A", "DO-178C", "DO-254"}, "{}"},
    };
    return kTemplates;
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
RulesEngine::RulesEngine(persistence::Database& db) : db_(db) {}

// ---------------------------------------------------------------------------
// Rule persistence
// ---------------------------------------------------------------------------
namespace {
const char* kRuleCols =
    "id, name, description, rule_type, params, severity, standard, enabled";
}

common::Result<Rule> RulesEngine::defineRule(const Rule& rule) {
    std::string id = rule.id;
    if (id.empty()) id = newUuid();

    // If a rule with this id already exists, update it; otherwise insert.
    auto exists = queryRows(db_.handle(),
                            "SELECT id FROM compliance_rules WHERE id=?;", {"id"},
                            {id});
    if (exists.failed()) {
        return common::Result<Rule>::err(exists.error());
    }
    std::string paramsStr = serializeParams(rule.params);
    std::string standardsStr = serializeStandards(rule.standards);
    std::string severityStr = toString(rule.severity);
    std::string enabledStr = rule.enabled ? "1" : "0";

    common::Result<void> rc = common::Result<void>::ok();
    if (exists.value().empty()) {
        rc = execBinds(db_.handle(),
                       "INSERT INTO compliance_rules "
                       "(id, name, description, rule_type, params, severity, "
                       " standard, enabled) VALUES (?,?,?,?,?,?,?,?);",
                       {id, rule.name, "", rule.ruleType, paramsStr, severityStr,
                        standardsStr, enabledStr});
    } else {
        rc = execBinds(db_.handle(),
                       "UPDATE compliance_rules SET name=?, description=?, rule_type=?, "
                       " params=?, severity=?, standard=?, enabled=? WHERE id=?;",
                       {rule.name, "", rule.ruleType, paramsStr, severityStr,
                        standardsStr, enabledStr, id});
    }
    if (rc.failed()) {
        return common::Result<Rule>::err(rc.error());
    }

    Rule out = rule;
    out.id = id;
    return common::Result<Rule>::ok(out);
}

common::Result<std::vector<Rule>> RulesEngine::loadRules(bool onlyEnabled) {
    std::string where = onlyEnabled ? " WHERE enabled=1" : "";
    auto rows = queryRows(db_.handle(),
                          std::string("SELECT ") + kRuleCols +
                              " FROM compliance_rules" + where + " ORDER BY name;",
                          {"id", "name", "description", "rule_type", "params",
                           "severity", "standard", "enabled"});
    if (rows.failed()) {
        return common::Result<std::vector<Rule>>::err(rows.error());
    }
    std::vector<Rule> out;
    for (const auto& row : rows.value()) {
        Rule r;
        r.id = row.at("id");
        r.name = row.at("name");
        r.ruleType = row.at("rule_type");
        r.params = parseParams(row.at("params"));
        r.standards = parseStandards(row.at("standard"));
        r.severity = severityFromString(row.at("severity")).value_or(Severity::Error);
        r.enabled = row.at("enabled") == "1";
        out.push_back(std::move(r));
    }
    return common::Result<std::vector<Rule>>::ok(std::move(out));
}

common::Result<std::vector<Rule>> RulesEngine::listRules() {
    return loadRules(false);
}

common::Result<void> RulesEngine::enableRule(const std::string& ruleId, bool enabled) {
    std::string val = enabled ? "1" : "0";
    auto rc = execBinds(db_.handle(),
                        "UPDATE compliance_rules SET enabled=? WHERE id=?;",
                        {val, ruleId});
    if (rc.failed()) return common::Result<void>::err(rc.error());
    return common::Result<void>::ok();
}

// ---------------------------------------------------------------------------
// Data loading helpers
// ---------------------------------------------------------------------------
std::vector<RulesEngine::EntityRef> RulesEngine::loadAllEntities() const {
    std::vector<EntityRef> out;
    auto add = [&out](EntityType t, const std::string& id, const std::string& status,
                      const std::string& ext) {
        out.push_back({t, id, status, ext});
    };
    persistence::RequirementDao req(db_);
    if (auto r = req.findAll(); r.isOk()) {
        for (const auto& e : r.value()) add(EntityType::Requirement, e.id, e.status, e.externalId);
    }
    persistence::DesignItemDao d(db_);
    if (auto r = d.findAll(); r.isOk()) {
        for (const auto& e : r.value()) add(EntityType::Design, e.id, e.status, e.externalId);
    }
    persistence::InterfaceDao i(db_);
    if (auto r = i.findAll(); r.isOk()) {
        for (const auto& e : r.value()) add(EntityType::Interface, e.id, e.status, e.externalId);
    }
    persistence::TestCaseDao tc(db_);
    if (auto r = tc.findAll(); r.isOk()) {
        for (const auto& e : r.value()) add(EntityType::TestCase, e.id, e.status, e.externalId);
    }
    persistence::HazardDao h(db_);
    if (auto r = h.findAll(); r.isOk()) {
        for (const auto& e : r.value()) add(EntityType::Hazard, e.id, e.status, e.externalId);
    }
    persistence::DecisionDao dec(db_);
    if (auto r = dec.findAll(); r.isOk()) {
        for (const auto& e : r.value()) add(EntityType::Decision, e.id, e.status, e.externalId);
    }
    persistence::AssumptionDao a(db_);
    if (auto r = a.findAll(); r.isOk()) {
        for (const auto& e : r.value()) add(EntityType::Assumption, e.id, e.status, e.externalId);
    }
    return out;
}

std::vector<TraceLink> RulesEngine::loadActiveLinks() const {
    persistence::TraceLinkDao dao(db_);
    std::vector<TraceLink> out;
    auto all = dao.findAll();
    if (!all.isOk()) return out;
    for (const auto& l : all.value()) {
        if (l.status == "Active") out.push_back(l);
    }
    return out;
}

bool RulesEngine::isActiveEntity(EntityType type, const std::string& id) const {
    if (id.empty()) return false;
    const char* table = nullptr;
    switch (type) {
        case EntityType::Requirement: table = "requirements"; break;
        case EntityType::Design:      table = "design_items"; break;
        case EntityType::Interface:   table = "interfaces"; break;
        case EntityType::TestCase:    table = "test_cases"; break;
        case EntityType::Hazard:      table = "hazards"; break;
        case EntityType::Decision:    table = "decisions"; break;
        case EntityType::Assumption:  table = "assumptions"; break;
    }
    if (!table) return false;
    // Absent or Obsolete -> not active.
    auto rows = queryRows(db_.handle(),
                          "SELECT status FROM " + std::string(table) + " WHERE id=?;",
                          {"status"}, {id});
    if (rows.failed() || rows.value().empty()) return false;
    return rows.value().front().at("status") != "Obsolete";
}

Violation RulesEngine::makeViolation(const Rule& r, EntityType type,
                                     const std::string& entityId,
                                     const std::string& externalId,
                                     const std::string& message) const {
    Violation v;
    v.id = newUuid();
    v.ruleId = r.id;
    v.ruleName = r.name;
    v.ruleType = r.ruleType;
    v.standards = r.standards;
    v.entityType = type;
    v.entityId = entityId;
    v.entityExternalId = externalId;
    v.message = message;
    v.severity = r.severity;
    return v;
}

// ---------------------------------------------------------------------------
// Rule evaluators (active / non-Obsolete entities only)
// ---------------------------------------------------------------------------
void RulesEngine::evalReqVerified(const Rule& r, std::vector<Violation>& out) {
    auto links = loadActiveLinks();
    std::set<std::string> verified;
    for (const auto& l : links) {
        if (l.relation == "verifies" && l.targetType == "requirement") {
            verified.insert(l.targetId);
        }
    }
    for (const auto& e : loadAllEntities()) {
        if (e.type == EntityType::Requirement && verified.count(e.id) == 0) {
            out.push_back(makeViolation(
                r, e.type, e.id, e.externalId,
                "Requirement '" + (e.externalId.empty() ? e.id : e.externalId) +
                    "' has no 'verifies' link."));
        }
    }
}

void RulesEngine::evalReqSatisfied(const Rule& r, std::vector<Violation>& out) {
    auto links = loadActiveLinks();
    std::set<std::string> satisfied;
    for (const auto& l : links) {
        if (l.relation == "satisfies" && l.targetType == "requirement") {
            satisfied.insert(l.targetId);
        }
    }
    for (const auto& e : loadAllEntities()) {
        if (e.type == EntityType::Requirement && satisfied.count(e.id) == 0) {
            out.push_back(makeViolation(
                r, e.type, e.id, e.externalId,
                "Requirement '" + (e.externalId.empty() ? e.id : e.externalId) +
                    "' has no 'satisfies' link."));
        }
    }
}

void RulesEngine::evalNoDangling(const Rule& r, std::vector<Violation>& out) {
    for (const auto& l : loadActiveLinks()) {
        auto st = entityTypeFromString(l.sourceType);
        if (st && !isActiveEntity(*st, l.sourceId)) {
            out.push_back(makeViolation(
                r, *st, l.sourceId, "",
                "Dangling source: link '" + l.id + "' references missing or Obsolete " +
                    l.sourceType + " '" + l.sourceId + "'."));
        }
        auto tt = entityTypeFromString(l.targetType);
        if (tt && !isActiveEntity(*tt, l.targetId)) {
            out.push_back(makeViolation(
                r, *tt, l.targetId, "",
                "Dangling target: link '" + l.id + "' references missing or Obsolete " +
                    l.targetType + " '" + l.targetId + "'."));
        }
    }
}

void RulesEngine::evalNoDuplicates(const Rule& r, std::vector<Violation>& out) {
    using Key = std::tuple<std::string, std::string, std::string, std::string, std::string>;
    std::map<Key, std::vector<TraceLink>> groups;
    for (const auto& l : loadActiveLinks()) {
        groups[{l.sourceType, l.sourceId, l.targetType, l.targetId, l.relation}].push_back(l);
    }
    for (const auto& [key, links] : groups) {
        for (size_t i = 1; i < links.size(); ++i) {
            const auto& dup = links[i];
            auto st = entityTypeFromString(dup.sourceType);
            out.push_back(makeViolation(
                r, st ? *st : EntityType::Requirement, dup.sourceId, "",
                "Duplicate link '" + dup.id + "' (same source, target, relation as '" +
                    links[0].id + "')."));
        }
    }
}

void RulesEngine::evalNoSelfLinks(const Rule& r, std::vector<Violation>& out) {
    for (const auto& l : loadActiveLinks()) {
        if (l.sourceType == l.targetType && l.sourceId == l.targetId) {
            auto st = entityTypeFromString(l.sourceType);
            out.push_back(makeViolation(
                r, st ? *st : EntityType::Requirement, l.sourceId, "",
                "Self link '" + l.id + "': source and target are the same entity."));
        }
    }
}

void RulesEngine::evalBidirectional(const Rule& r, std::vector<Violation>& out) {
    auto links = loadActiveLinks();
    for (const auto& l : links) {
        bool hasReciprocal = false;
        for (const auto& o : links) {
            if (o.id == l.id) continue;
            if (o.sourceType == l.targetType && o.sourceId == l.targetId &&
                o.targetType == l.sourceType && o.targetId == l.sourceId &&
                o.relation == l.relation) {
                hasReciprocal = true;
                break;
            }
        }
        if (!hasReciprocal) {
            auto st = entityTypeFromString(l.sourceType);
            out.push_back(makeViolation(
                r, st ? *st : EntityType::Requirement, l.sourceId, "",
                "Link '" + l.id + "' has no reciprocal link in the opposite direction."));
        }
    }
}

void RulesEngine::evalCoverageMin(const Rule& r, std::vector<Violation>& out) {
    auto it = r.params.find("min_percent");
    int min = 100;
    if (it != r.params.end()) {
        try {
            min = std::stoi(it->second);
        } catch (...) {
            min = 100;
        }
    }
    auto links = loadActiveLinks();
    std::set<std::string> designed, verified;
    for (const auto& l : links) {
        if (l.relation == "satisfies" && l.targetType == "requirement") designed.insert(l.targetId);
        if (l.relation == "verifies" && l.targetType == "requirement") verified.insert(l.targetId);
    }
    for (const auto& e : loadAllEntities()) {
        if (e.type != EntityType::Requirement) continue;
        int pct = ((designed.count(e.id) ? 1 : 0) + (verified.count(e.id) ? 1 : 0)) * 50;
        if (pct < min) {
            out.push_back(makeViolation(
                r, e.type, e.id, e.externalId,
                "Requirement '" + (e.externalId.empty() ? e.id : e.externalId) +
                    "' coverage " + std::to_string(pct) + "% is below the " +
                    std::to_string(min) + "% minimum."));
        }
    }
}

void RulesEngine::evalNoOrphanDesign(const Rule& r, std::vector<Violation>& out) {
    auto links = loadActiveLinks();
    std::set<std::string> satisfied;
    for (const auto& l : links) {
        if (l.sourceType == "design" && l.relation == "satisfies") {
            satisfied.insert(l.sourceId);
        }
    }
    for (const auto& e : loadAllEntities()) {
        if (e.type == EntityType::Design && satisfied.count(e.id) == 0) {
            out.push_back(makeViolation(
                r, e.type, e.id, e.externalId,
                "Design item '" + (e.externalId.empty() ? e.id : e.externalId) +
                    "' has no outgoing 'satisfies' link."));
        }
    }
}

void RulesEngine::evalStatusValid(const Rule& r, std::vector<Violation>& out) {
    for (const auto& e : loadAllEntities()) {
        if (!isValidStatus(e.type, e.status)) {
            out.push_back(makeViolation(
                r, e.type, e.id, e.externalId,
                "Entity has illegal status '" + e.status + "' for type '" +
                    toString(e.type) + "'."));
        }
    }
}

// ---------------------------------------------------------------------------
// runValidation
// ---------------------------------------------------------------------------
common::Result<ValidationRun> RulesEngine::runValidation() {
    auto rules = loadRules(true);
    if (rules.failed()) {
        return common::Result<ValidationRun>::err(rules.error());
    }

    std::string runId = newUuid();
    std::string started = now();

    std::vector<Violation> violations;
    for (const auto& rule : rules.value()) {
        if (rule.ruleType == "REQ_MUST_BE_VERIFIED") evalReqVerified(rule, violations);
        else if (rule.ruleType == "REQ_MUST_BE_SATISFIED") evalReqSatisfied(rule, violations);
        else if (rule.ruleType == "NO_DANGLING_LINKS") evalNoDangling(rule, violations);
        else if (rule.ruleType == "NO_DUPLICATE_LINKS") evalNoDuplicates(rule, violations);
        else if (rule.ruleType == "NO_SELF_LINKS") evalNoSelfLinks(rule, violations);
        else if (rule.ruleType == "BIDIRECTIONAL") evalBidirectional(rule, violations);
        else if (rule.ruleType == "COVERAGE_MIN") evalCoverageMin(rule, violations);
        else if (rule.ruleType == "NO_ORPHAN_DESIGN") evalNoOrphanDesign(rule, violations);
        else if (rule.ruleType == "STATUS_VALID") evalStatusValid(rule, violations);
        // Unknown/custom rule types are skipped.
    }

    for (auto& v : violations) v.runId = runId;

    std::string status = violations.empty() ? "ok" : "violations";
    std::string summary = std::to_string(rules.value().size()) +
                          " enabled rule(s) evaluated, " +
                          std::to_string(violations.size()) + " violation(s)";
    std::string finished = now();

    // Persist within one transaction.
    auto begin = db_.execute("BEGIN IMMEDIATE;");
    if (begin.failed()) {
        return common::Result<ValidationRun>::err("BEGIN failed: " + begin.error());
    }
    auto insertRun = execBinds(
        db_.handle(),
        "INSERT INTO validation_runs (id, name, started_at, finished_at, status, summary) "
        "VALUES (?,?,?,?,?,?);",
        {runId, "validation", started, finished, status, summary});
    if (insertRun.failed()) {
        db_.execute("ROLLBACK;");
        return common::Result<ValidationRun>::err("insert run failed: " + insertRun.error());
    }
    for (const auto& v : violations) {
        auto ins = execBinds(
            db_.handle(),
            "INSERT INTO compliance_violations "
            "(id, run_id, rule_id, entity_type, entity_id, message, severity, timestamp) "
            "VALUES (?,?,?,?,?,?,?,?);",
            {v.id, v.runId, v.ruleId, toString(v.entityType), v.entityId, v.message,
             toString(v.severity), now()});
        if (ins.failed()) {
            db_.execute("ROLLBACK;");
            return common::Result<ValidationRun>::err("insert violation failed: " + ins.error());
        }
    }
    auto commit = db_.execute("COMMIT;");
    if (commit.failed()) {
        db_.execute("ROLLBACK;");
        return common::Result<ValidationRun>::err("COMMIT failed: " + commit.error());
    }

    ValidationRun report;
    report.id = runId;
    report.name = "validation";
    report.status = status;
    report.summary = summary;
    report.violationCount = static_cast<int>(violations.size());
    report.violations = std::move(violations);
    return common::Result<ValidationRun>::ok(std::move(report));
}

}  // namespace lodestar::tracelink
