#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

// core/tracelink/TraceLinkService.cpp
// The canonical TraceLink service: rich entity CRUD, typed link CRUD with
// integrity-on-write, and status state-machine enforcement.

#include "core/tracelink/TraceLinkService.h"

#include <ctime>
#include <tuple>
#include <vector>

#include <sqlite3.h>

#include "core/common/Uuid.h"
#include "core/tracelink/StateMachine.h"

namespace lodestar::tracelink {

using lodestar::common::newUuid;
using persistence::Assumption;
using persistence::Decision;
using persistence::DesignItem;
using persistence::Hazard;
using persistence::InterfaceDef;
using persistence::Requirement;
using persistence::TestCase;
using persistence::TraceLink;

TraceLinkService::TraceLinkService(persistence::Database& db)
    : db_(db),
      reqDao_(db),
      designDao_(db),
      ifaceDao_(db),
      testDao_(db),
      hazardDao_(db),
      decisionDao_(db),
      assumptionDao_(db),
      linkDao_(db) {}

namespace {

std::string now() {
    char buf[32];
    const auto t = std::time(nullptr);
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
    return buf;
}

std::string defaultExternalId(EntityType type) {
    const char* prefix = "ENT";
    switch (type) {
        case EntityType::Requirement: prefix = "REQ"; break;
        case EntityType::Design:      prefix = "DES"; break;
        case EntityType::Interface:   prefix = "IF"; break;
        case EntityType::TestCase:    prefix = "TC"; break;
        case EntityType::Hazard:      prefix = "HAZ"; break;
        case EntityType::Decision:    prefix = "DEC"; break;
        case EntityType::Assumption:  prefix = "ASS"; break;
    }
    return std::string(prefix) + "-" + newUuid().substr(0, 8);
}

persistence::EntityFilter toPersist(const EntityFilter& f) {
    persistence::EntityFilter pf;
    pf.status = f.status ? *f.status : "";
    pf.tags = f.tags ? *f.tags : "";
    pf.text = f.text ? *f.text : "";
    pf.limit = f.limit;
    pf.offset = f.offset;
    return pf;
}

// -------- entity conversions ------------------------------------------------
Requirement toReq(const Entity& e) {
    Requirement r;
    r.id = e.id; r.externalId = e.externalId; r.name = e.name; r.description = e.text;
    r.type = e.typeAttr.empty() ? "functional" : e.typeAttr; r.status = e.status;
    r.priority = e.priority; r.source = e.source; r.owner = e.owner;
    r.rationale = e.rationale; r.verificationMethod = e.verificationMethod;
    r.safetyLevel = e.safetyLevel; r.parentId = e.parentId; r.sortOrder = e.sortOrder;
    r.tags = e.tags; r.version = e.version; r.createdBy = e.createdBy;
    r.createdAt = e.createdAt; r.updatedBy = e.updatedBy; r.updatedAt = e.updatedAt;
    return r;
}
Entity fromReq(const Requirement& r) {
    Entity e;
    e.type = EntityType::Requirement; e.id = r.id; e.externalId = r.externalId;
    e.name = r.name; e.text = r.description; e.typeAttr = r.type; e.status = r.status;
    e.priority = r.priority; e.source = r.source; e.owner = r.owner;
    e.rationale = r.rationale; e.verificationMethod = r.verificationMethod;
    e.safetyLevel = r.safetyLevel; e.parentId = r.parentId; e.sortOrder = r.sortOrder;
    e.tags = r.tags; e.version = r.version; e.createdBy = r.createdBy;
    e.createdAt = r.createdAt; e.updatedBy = r.updatedBy; e.updatedAt = r.updatedAt;
    return e;
}
DesignItem toDesign(const Entity& e) {
    DesignItem d;
    d.id = e.id; d.externalId = e.externalId; d.name = e.name; d.description = e.text;
    d.type = e.typeAttr.empty() ? "component" : e.typeAttr; d.status = e.status;
    d.owner = e.owner; d.parentId = e.parentId; d.tags = e.tags; d.version = e.version;
    d.createdBy = e.createdBy; d.createdAt = e.createdAt; d.updatedBy = e.updatedBy;
    d.updatedAt = e.updatedAt;
    return d;
}
Entity fromDesign(const DesignItem& d) {
    Entity e;
    e.type = EntityType::Design; e.id = d.id; e.externalId = d.externalId;
    e.name = d.name; e.text = d.description; e.typeAttr = d.type; e.status = d.status;
    e.owner = d.owner; e.parentId = d.parentId; e.tags = d.tags; e.version = d.version;
    e.createdBy = d.createdBy; e.createdAt = d.createdAt; e.updatedBy = d.updatedBy;
    e.updatedAt = d.updatedAt;
    return e;
}
InterfaceDef toIface(const Entity& e) {
    InterfaceDef i;
    i.id = e.id; i.externalId = e.externalId; i.name = e.name; i.description = e.text;
    i.status = e.status; i.direction = e.direction; i.sourceEntity = e.sourceEntity;
    i.targetEntity = e.targetEntity; i.dataItems = e.dataItems; i.protocol = e.protocol;
    i.tags = e.tags; i.version = e.version; i.createdBy = e.createdBy;
    i.createdAt = e.createdAt; i.updatedBy = e.updatedBy; i.updatedAt = e.updatedAt;
    return i;
}
Entity fromIface(const InterfaceDef& i) {
    Entity e;
    e.type = EntityType::Interface; e.id = i.id; e.externalId = i.externalId;
    e.name = i.name; e.text = i.description; e.status = i.status;
    e.direction = i.direction; e.sourceEntity = i.sourceEntity;
    e.targetEntity = i.targetEntity; e.dataItems = i.dataItems; e.protocol = i.protocol;
    e.tags = i.tags; e.version = i.version; e.createdBy = i.createdBy;
    e.createdAt = i.createdAt; e.updatedBy = i.updatedBy; e.updatedAt = i.updatedAt;
    return e;
}
TestCase toTc(const Entity& e) {
    TestCase t;
    t.id = e.id; t.externalId = e.externalId; t.name = e.name; t.description = e.text;
    t.status = e.status; t.verificationMethod = e.verificationMethod;
    t.resultStatus = e.resultStatus; t.priority = e.priority; t.tags = e.tags;
    t.version = e.version; t.createdBy = e.createdBy; t.createdAt = e.createdAt;
    t.updatedBy = e.updatedBy; t.updatedAt = e.updatedAt;
    return t;
}
Entity fromTc(const TestCase& t) {
    Entity e;
    e.type = EntityType::TestCase; e.id = t.id; e.externalId = t.externalId;
    e.name = t.name; e.text = t.description; e.status = t.status;
    e.verificationMethod = t.verificationMethod; e.resultStatus = t.resultStatus;
    e.priority = t.priority; e.tags = t.tags; e.version = t.version;
    e.createdBy = t.createdBy; e.createdAt = t.createdAt; e.updatedBy = t.updatedBy;
    e.updatedAt = t.updatedAt;
    return e;
}
Hazard toHazard(const Entity& e) {
    Hazard h;
    h.id = e.id; h.externalId = e.externalId; h.name = e.name; h.description = e.text;
    h.status = e.status; h.severity = e.severity; h.likelihood = e.likelihood;
    h.owner = e.owner; h.tags = e.tags; h.version = e.version;
    h.createdBy = e.createdBy; h.createdAt = e.createdAt; h.updatedBy = e.updatedBy;
    h.updatedAt = e.updatedAt;
    return h;
}
Entity fromHazard(const Hazard& h) {
    Entity e;
    e.type = EntityType::Hazard; e.id = h.id; e.externalId = h.externalId;
    e.name = h.name; e.text = h.description; e.status = h.status;
    e.severity = h.severity; e.likelihood = h.likelihood; e.owner = h.owner;
    e.tags = h.tags; e.version = h.version; e.createdBy = h.createdBy;
    e.createdAt = h.createdAt; e.updatedBy = h.updatedBy; e.updatedAt = h.updatedAt;
    return e;
}
Decision toDecision(const Entity& e) {
    Decision d;
    d.id = e.id; d.externalId = e.externalId; d.name = e.name; d.description = e.text;
    d.status = e.status; d.rationale = e.rationale; d.owner = e.owner; d.date = e.date;
    d.tags = e.tags; d.version = e.version; d.createdBy = e.createdBy;
    d.createdAt = e.createdAt; d.updatedBy = e.updatedBy; d.updatedAt = e.updatedAt;
    return d;
}
Entity fromDecision(const Decision& d) {
    Entity e;
    e.type = EntityType::Decision; e.id = d.id; e.externalId = d.externalId;
    e.name = d.name; e.text = d.description; e.status = d.status;
    e.rationale = d.rationale; e.owner = d.owner; e.date = d.date; e.tags = d.tags;
    e.version = d.version; e.createdBy = d.createdBy; e.createdAt = d.createdAt;
    e.updatedBy = d.updatedBy; e.updatedAt = d.updatedAt;
    return e;
}
Assumption toAssumption(const Entity& e) {
    Assumption a;
    a.id = e.id; a.externalId = e.externalId; a.name = e.name; a.description = e.text;
    a.status = e.status; a.owner = e.owner; a.tags = e.tags; a.version = e.version;
    a.createdBy = e.createdBy; a.createdAt = e.createdAt; a.updatedBy = e.updatedBy;
    a.updatedAt = e.updatedAt;
    return a;
}
Entity fromAssumption(const Assumption& a) {
    Entity e;
    e.type = EntityType::Assumption; e.id = a.id; e.externalId = a.externalId;
    e.name = a.name; e.text = a.description; e.status = a.status; e.owner = a.owner;
    e.tags = a.tags; e.version = a.version; e.createdBy = a.createdBy;
    e.createdAt = a.createdAt; e.updatedBy = a.updatedBy; e.updatedAt = a.updatedAt;
    return e;
}

Link fromLink(const TraceLink& pl) {
    Link l;
    l.id = pl.id;
    l.sourceType = entityTypeFromString(pl.sourceType).value_or(EntityType::Requirement);
    l.sourceId = pl.sourceId;
    l.targetType = entityTypeFromString(pl.targetType).value_or(EntityType::Requirement);
    l.targetId = pl.targetId;
    l.relation = pl.relation;
    l.rationale = pl.rationale;
    l.status = pl.status;
    l.createdBy = pl.createdBy;
    l.createdAt = pl.createdAt;
    l.updatedAt = pl.updatedAt;
    l.version = pl.version;
    l.supersededBy = pl.supersededBy;
    l.validFrom = pl.validFrom;
    l.validTo = pl.validTo;
    return l;
}

TraceLink toLink(const Link& l) {
    TraceLink pl;
    pl.id = l.id;
    pl.sourceType = toString(l.sourceType);
    pl.sourceId = l.sourceId;
    pl.targetType = toString(l.targetType);
    pl.targetId = l.targetId;
    pl.relation = l.relation;
    pl.rationale = l.rationale;
    pl.status = l.status;
    pl.createdBy = l.createdBy;
    pl.createdAt = l.createdAt;
    pl.updatedAt = l.updatedAt;
    pl.version = l.version;
    pl.supersededBy = l.supersededBy;
    pl.validFrom = l.validFrom;
    pl.validTo = l.validTo;
    return pl;
}

// Parameterized statement execution used to insert audit rows safely.
common::Result<void> execBind(sqlite3* db, const std::string& sql,
                              const std::vector<std::string>& params) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return common::Result<void>::err("prepare failed: " +
                                         std::string(sqlite3_errmsg(db)));
    }
    for (size_t i = 0; i < params.size(); ++i) {
        sqlite3_bind_text(stmt, static_cast<int>(i + 1), params[i].c_str(),
                          static_cast<int>(params[i].size()), SQLITE_TRANSIENT);
    }
    int rc = sqlite3_step(stmt);
    std::string msg = sqlite3_errmsg(db);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return common::Result<void>::err("step failed: " + msg);
    }
    return common::Result<void>::ok();
}

// Records a changed field as an (old, new) pair keyed by field name.
void pushChange(std::vector<std::tuple<std::string, std::string, std::string>>& out,
                const std::string& field, const std::string& a, const std::string& b) {
    if (a != b) out.emplace_back(field, a, b);
}

// Field-level diff between an entity's before/after states. Metadata fields
// (id, version, timestamps, actor stamps) are excluded so only the real change
// is recorded.
std::vector<std::tuple<std::string, std::string, std::string>>
entityFieldChanges(const Entity& a, const Entity& b) {
    std::vector<std::tuple<std::string, std::string, std::string>> out;
    pushChange(out, "externalId", a.externalId, b.externalId);
    pushChange(out, "name", a.name, b.name);
    pushChange(out, "text", a.text, b.text);
    pushChange(out, "status", a.status, b.status);
    pushChange(out, "typeAttr", a.typeAttr, b.typeAttr);
    pushChange(out, "priority", a.priority, b.priority);
    pushChange(out, "source", a.source, b.source);
    pushChange(out, "owner", a.owner, b.owner);
    pushChange(out, "rationale", a.rationale, b.rationale);
    pushChange(out, "verificationMethod", a.verificationMethod, b.verificationMethod);
    pushChange(out, "safetyLevel", a.safetyLevel, b.safetyLevel);
    pushChange(out, "direction", a.direction, b.direction);
    pushChange(out, "sourceEntity", a.sourceEntity, b.sourceEntity);
    pushChange(out, "targetEntity", a.targetEntity, b.targetEntity);
    pushChange(out, "dataItems", a.dataItems, b.dataItems);
    pushChange(out, "protocol", a.protocol, b.protocol);
    pushChange(out, "resultStatus", a.resultStatus, b.resultStatus);
    pushChange(out, "severity", a.severity, b.severity);
    pushChange(out, "likelihood", a.likelihood, b.likelihood);
    pushChange(out, "date", a.date, b.date);
    pushChange(out, "parentId", a.parentId, b.parentId);
    pushChange(out, "tags", a.tags, b.tags);
    return out;
}

}  // namespace

// ---------------------------------------------------------------------------
// Audit
// ---------------------------------------------------------------------------
void TraceLinkService::setAuditContext(const std::string& actor,
                                       const std::string& changeRequestId) {
    actor_ = actor;
    changeRequestId_ = changeRequestId;
}

void TraceLinkService::beginTx() { db_.execute("BEGIN IMMEDIATE;"); }
common::Result<void> TraceLinkService::commitTx() { return db_.execute("COMMIT;"); }
void TraceLinkService::rollbackTx() { db_.execute("ROLLBACK;"); }

common::Result<void> TraceLinkService::writeAudit(
    const std::string& entityType, const std::string& entityId,
    const std::string& action, const std::string& field, const std::string& oldValue,
    const std::string& newValue) {
    return execBind(db_.handle(),
                    "INSERT INTO audit_log (id, entity_type, entity_id, action, field, "
                    "old_value, new_value, actor, timestamp, change_request_id) "
                    "VALUES (?,?,?,?,?,?,?,?,?,?);",
                    {newUuid(), entityType, entityId, action, field, oldValue, newValue,
                     actor_, now(), changeRequestId_});
}

// ---------------------------------------------------------------------------
// Entity CRUD
// ---------------------------------------------------------------------------
common::Result<Entity> TraceLinkService::addEntity(const Entity& input) {
    Entity e = input;
    if (e.id.empty()) e.id = newUuid();
    if (e.externalId.empty()) e.externalId = defaultExternalId(e.type);
    if (e.status.empty()) e.status = statuses(e.type).front();
    if (!isValidStatus(e.type, e.status)) {
        return common::Result<Entity>::err("invalid status '" + e.status +
                                           "' for type '" + toString(e.type) + "'");
    }
    // External id must be unique per type.
    auto dup = dispatchGet(e.type, "");
    (void)dup;
    bool extTaken = false;
    switch (e.type) {
        case EntityType::Requirement: {
            auto r = reqDao_.findByExternalId(e.externalId);
            extTaken = r.isOk() && r.value().has_value();
            break;
        }
        case EntityType::Design: {
            auto d = designDao_.findByExternalId(e.externalId);
            extTaken = d.isOk() && d.value().has_value();
            break;
        }
        case EntityType::Interface: {
            auto i = ifaceDao_.findByExternalId(e.externalId);
            extTaken = i.isOk() && i.value().has_value();
            break;
        }
        case EntityType::TestCase: {
            auto t = testDao_.findByExternalId(e.externalId);
            extTaken = t.isOk() && t.value().has_value();
            break;
        }
        case EntityType::Hazard: {
            auto h = hazardDao_.findByExternalId(e.externalId);
            extTaken = h.isOk() && h.value().has_value();
            break;
        }
        case EntityType::Decision: {
            auto d = decisionDao_.findByExternalId(e.externalId);
            extTaken = d.isOk() && d.value().has_value();
            break;
        }
        case EntityType::Assumption: {
            auto a = assumptionDao_.findByExternalId(e.externalId);
            extTaken = a.isOk() && a.value().has_value();
            break;
        }
    }
    if (extTaken) {
        return common::Result<Entity>::err("duplicate external id '" + e.externalId +
                                           "' for type '" + toString(e.type) + "'");
    }
    e.version = 1;
    if (e.createdAt.empty()) e.createdAt = now();
    if (e.updatedAt.empty()) e.updatedAt = e.createdAt;
    beginTx();
    auto res = dispatchCreate(e.type, e);
    if (res.failed()) {
        rollbackTx();
        return res;
    }
    auto audit = writeAudit(toString(e.type), res.value().id, "create", "", "", "");
    if (audit.failed()) {
        rollbackTx();
        return common::Result<Entity>::err(audit.error());
    }
    auto commit = commitTx();
    if (commit.failed()) {
        rollbackTx();
        return common::Result<Entity>::err(commit.error());
    }
    return res;
}

common::Result<Entity> TraceLinkService::updateEntity(const Entity& data) {
    auto existingRes = dispatchGet(data.type, data.id);
    if (existingRes.failed()) return common::Result<Entity>::err(existingRes.error());
    if (!existingRes.value()) {
        return common::Result<Entity>::err("entity not found: " + data.id);
    }
    const Entity before = *existingRes.value();
    Entity existing = *existingRes.value();

    if (!data.status.empty() && data.status != existing.status) {
        std::string err = transitionError(data.type, existing.status, data.status);
        if (!err.empty()) return common::Result<Entity>::err(err);
        existing.status = data.status;
    }
    existing.version = existing.version + 1;
    existing.name = data.name.empty() ? existing.name : data.name;
    existing.text = data.text;
    existing.externalId = data.externalId.empty() ? existing.externalId : data.externalId;
    existing.typeAttr = data.typeAttr;
    existing.priority = data.priority;
    existing.source = data.source;
    existing.owner = data.owner;
    existing.rationale = data.rationale;
    existing.verificationMethod = data.verificationMethod;
    existing.safetyLevel = data.safetyLevel;
    existing.direction = data.direction;
    existing.sourceEntity = data.sourceEntity;
    existing.targetEntity = data.targetEntity;
    existing.dataItems = data.dataItems;
    existing.protocol = data.protocol;
    existing.resultStatus = data.resultStatus;
    existing.severity = data.severity;
    existing.likelihood = data.likelihood;
    existing.date = data.date;
    existing.parentId = data.parentId;
    existing.sortOrder = data.sortOrder;
    existing.tags = data.tags;
    existing.updatedBy = data.updatedBy;
    existing.updatedAt = now();
    beginTx();
    auto res = dispatchUpdate(data.type, existing);
    if (res.failed()) {
        rollbackTx();
        return common::Result<Entity>::err(res.error());
    }
    // Audit one row per changed field (exact before -> after values).
    auto changes = entityFieldChanges(before, existing);
    for (const auto& [field, oldV, newV] : changes) {
        auto audit = writeAudit(toString(data.type), existing.id, "update", field, oldV, newV);
        if (audit.failed()) {
            rollbackTx();
            return common::Result<Entity>::err(audit.error());
        }
    }
    auto commit = commitTx();
    if (commit.failed()) {
        rollbackTx();
        return common::Result<Entity>::err(commit.error());
    }
    return res;
}

common::Result<void> TraceLinkService::removeEntity(EntityType type, const std::string& id) {
    auto existingRes = dispatchGet(type, id);
    if (existingRes.failed()) return common::Result<void>::err(existingRes.error());
    if (!existingRes.value()) {
        return common::Result<void>::err(toString(type) + " not found: " + id);
    }
    Entity e = *existingRes.value();
    e.status = "Obsolete";
    e.version = e.version + 1;
    e.updatedAt = now();
    beginTx();
    auto res = dispatchUpdate(type, e);
    if (res.failed()) {
        rollbackTx();
        return common::Result<void>::err(res.error());
    }
    auto audit = writeAudit(toString(type), id, "soft_delete", "", "", "");
    if (audit.failed()) {
        rollbackTx();
        return common::Result<void>::err(audit.error());
    }
    auto commit = commitTx();
    if (commit.failed()) {
        rollbackTx();
        return common::Result<void>::err(commit.error());
    }
    return common::Result<void>::ok();
}

common::Result<std::optional<Entity>> TraceLinkService::getEntity(EntityType type,
                                                                  const std::string& id) {
    return dispatchGet(type, id);
}

common::Result<std::vector<Entity>> TraceLinkService::listEntities(
    EntityType type, const EntityFilter& filter) {
    persistence::EntityFilter pf = toPersist(filter);
    switch (type) {
        case EntityType::Requirement: {
            auto res = reqDao_.findByFilters(pf);
            if (res.failed()) return common::Result<std::vector<Entity>>::err(res.error());
            std::vector<Entity> out;
            for (const auto& r : res.value()) out.push_back(fromReq(r));
            return common::Result<std::vector<Entity>>::ok(std::move(out));
        }
        case EntityType::Design: {
            auto res = designDao_.findByFilters(pf);
            if (res.failed()) return common::Result<std::vector<Entity>>::err(res.error());
            std::vector<Entity> out;
            for (const auto& d : res.value()) out.push_back(fromDesign(d));
            return common::Result<std::vector<Entity>>::ok(std::move(out));
        }
        case EntityType::Interface: {
            auto res = ifaceDao_.findByFilters(pf);
            if (res.failed()) return common::Result<std::vector<Entity>>::err(res.error());
            std::vector<Entity> out;
            for (const auto& i : res.value()) out.push_back(fromIface(i));
            return common::Result<std::vector<Entity>>::ok(std::move(out));
        }
        case EntityType::TestCase: {
            auto res = testDao_.findByFilters(pf);
            if (res.failed()) return common::Result<std::vector<Entity>>::err(res.error());
            std::vector<Entity> out;
            for (const auto& t : res.value()) out.push_back(fromTc(t));
            return common::Result<std::vector<Entity>>::ok(std::move(out));
        }
        case EntityType::Hazard: {
            auto res = hazardDao_.findByFilters(pf);
            if (res.failed()) return common::Result<std::vector<Entity>>::err(res.error());
            std::vector<Entity> out;
            for (const auto& h : res.value()) out.push_back(fromHazard(h));
            return common::Result<std::vector<Entity>>::ok(std::move(out));
        }
        case EntityType::Decision: {
            auto res = decisionDao_.findByFilters(pf);
            if (res.failed()) return common::Result<std::vector<Entity>>::err(res.error());
            std::vector<Entity> out;
            for (const auto& d : res.value()) out.push_back(fromDecision(d));
            return common::Result<std::vector<Entity>>::ok(std::move(out));
        }
        case EntityType::Assumption: {
            auto res = assumptionDao_.findByFilters(pf);
            if (res.failed()) return common::Result<std::vector<Entity>>::err(res.error());
            std::vector<Entity> out;
            for (const auto& a : res.value()) out.push_back(fromAssumption(a));
            return common::Result<std::vector<Entity>>::ok(std::move(out));
        }
    }
    return common::Result<std::vector<Entity>>::err("unknown entity type");
}

common::Result<std::vector<Entity>> TraceLinkService::search(EntityType type,
                                                             const std::string& text) {
    EntityFilter f;
    f.text = text;
    return listEntities(type, f);
}

// ---------------------------------------------------------------------------
// Links
// ---------------------------------------------------------------------------
common::Result<Link> TraceLinkService::addLink(const Link& link) {
    auto relOpt = relationFromString(link.relation);
    if (!relOpt) return common::Result<Link>::err("invalid relation: " + link.relation);

    if (!nodeExists(link.sourceType, link.sourceId)) {
        return common::Result<Link>::err("dangling link: source '" +
                                         toString(link.sourceType) + ":" + link.sourceId +
                                         "' does not exist");
    }
    if (!nodeExists(link.targetType, link.targetId)) {
        return common::Result<Link>::err("dangling link: target '" +
                                         toString(link.targetType) + ":" + link.targetId +
                                         "' does not exist");
    }
    if (link.sourceType == link.targetType && link.sourceId == link.targetId) {
        return common::Result<Link>::err("self-loop links are not allowed");
    }
    if (!isRelationAllowed(link.sourceType, link.targetType, *relOpt)) {
        return common::Result<Link>::err("relation '" + link.relation +
                                         "' not allowed between " +
                                         toString(link.sourceType) + " and " +
                                         toString(link.targetType));
    }
    auto dup = linkDao_.existsActive(toString(link.sourceType), link.sourceId,
                                     toString(link.targetType), link.targetId,
                                     link.relation);
    if (dup.failed()) return common::Result<Link>::err(dup.error());
    if (dup.value()) {
        return common::Result<Link>::err("duplicate link: " + toString(link.sourceType) +
                                         ":" + link.sourceId + " -" + link.relation +
                                         "-> " + toString(link.targetType) + ":" +
                                         link.targetId);
    }

    TraceLink pl = toLink(link);
    if (pl.id.empty()) pl.id = newUuid();
    if (pl.status.empty()) pl.status = "Active";
    if (pl.createdAt.empty()) pl.createdAt = now();
    if (pl.updatedAt.empty()) pl.updatedAt = pl.createdAt;
    pl.version = 1;
    beginTx();
    auto res = linkDao_.create(pl);
    if (res.failed()) {
        rollbackTx();
        return common::Result<Link>::err(res.error());
    }
    auto audit = writeAudit("link", pl.id, "add_link", "", "", "");
    if (audit.failed()) {
        rollbackTx();
        return common::Result<Link>::err(audit.error());
    }
    auto commit = commitTx();
    if (commit.failed()) {
        rollbackTx();
        return common::Result<Link>::err(commit.error());
    }
    return common::Result<Link>::ok(fromLink(pl));
}

common::Result<Link> TraceLinkService::updateLink(const Link& link) {
    auto found = linkDao_.findById(link.id);
    if (found.failed()) return common::Result<Link>::err(found.error());
    if (!found.value()) return common::Result<Link>::err("link not found: " + link.id);
    TraceLink pl = toLink(link);
    pl.version = found.value()->version + 1;
    pl.createdAt = found.value()->createdAt;
    if (pl.status.empty()) pl.status = found.value()->status;
    if (pl.status != "Active" && pl.status != "Proposed" && pl.status != "Superseded") {
        return common::Result<Link>::err("invalid link status: " + pl.status);
    }
    pl.updatedAt = now();
    beginTx();
    auto res = linkDao_.update(pl);
    if (res.failed()) {
        rollbackTx();
        return common::Result<Link>::err(res.error());
    }
    auto audit = writeAudit("link", pl.id, "update_link", "", "", "");
    if (audit.failed()) {
        rollbackTx();
        return common::Result<Link>::err(audit.error());
    }
    auto commit = commitTx();
    if (commit.failed()) {
        rollbackTx();
        return common::Result<Link>::err(commit.error());
    }
    return common::Result<Link>::ok(fromLink(pl));
}

common::Result<void> TraceLinkService::removeLink(const std::string& id) {
    auto found = linkDao_.findById(id);
    if (found.failed()) return common::Result<void>::err(found.error());
    if (!found.value()) return common::Result<void>::err("link not found: " + id);
    TraceLink pl = *found.value();
    pl.status = "Superseded";
    pl.version = pl.version + 1;
    pl.updatedAt = now();
    beginTx();
    auto res = linkDao_.update(pl);
    if (res.failed()) {
        rollbackTx();
        return common::Result<void>::err(res.error());
    }
    auto audit = writeAudit("link", id, "remove_link", "", "", "");
    if (audit.failed()) {
        rollbackTx();
        return common::Result<void>::err(audit.error());
    }
    auto commit = commitTx();
    if (commit.failed()) {
        rollbackTx();
        return common::Result<void>::err(commit.error());
    }
    return common::Result<void>::ok();
}

common::Result<std::vector<Link>> TraceLinkService::linksFrom(EntityType type,
                                                              const std::string& id) {
    auto res = linkDao_.findBySource(toString(type), id);
    if (res.failed()) return common::Result<std::vector<Link>>::err(res.error());
    std::vector<Link> out;
    for (const auto& l : res.value()) out.push_back(fromLink(l));
    return common::Result<std::vector<Link>>::ok(std::move(out));
}

common::Result<std::vector<Link>> TraceLinkService::linksTo(EntityType type,
                                                            const std::string& id) {
    auto res = linkDao_.findByTarget(toString(type), id);
    if (res.failed()) return common::Result<std::vector<Link>>::err(res.error());
    std::vector<Link> out;
    for (const auto& l : res.value()) out.push_back(fromLink(l));
    return common::Result<std::vector<Link>>::ok(std::move(out));
}

common::Result<std::vector<Link>> TraceLinkService::allLinks() {
    auto res = linkDao_.findAll();
    if (res.failed()) return common::Result<std::vector<Link>>::err(res.error());
    std::vector<Link> out;
    for (const auto& l : res.value()) out.push_back(fromLink(l));
    return common::Result<std::vector<Link>>::ok(std::move(out));
}

// ---------------------------------------------------------------------------
// Status state machine
// ---------------------------------------------------------------------------
bool TraceLinkService::isLegalTransition(EntityType type, const std::string& from,
                                         const std::string& to) {
    return canTransition(type, from, to);
}

common::Result<void> TraceLinkService::transition(EntityType type, const std::string& id,
                                                  const std::string& to) {
    auto existingRes = dispatchGet(type, id);
    if (existingRes.failed()) return common::Result<void>::err(existingRes.error());
    if (!existingRes.value()) {
        return common::Result<void>::err(toString(type) + " not found: " + id);
    }
    Entity e = *existingRes.value();
    const std::string oldStatus = e.status;
    if (!canTransition(type, e.status, to)) {
        return common::Result<void>::err(transitionError(type, e.status, to));
    }
    e.status = to;
    e.version = e.version + 1;
    e.updatedAt = now();
    beginTx();
    auto res = dispatchUpdate(type, e);
    if (res.failed()) {
        rollbackTx();
        return common::Result<void>::err(res.error());
    }
    auto audit = writeAudit(toString(type), id, "update", "status", oldStatus, to);
    if (audit.failed()) {
        rollbackTx();
        return common::Result<void>::err(audit.error());
    }
    auto commit = commitTx();
    if (commit.failed()) {
        rollbackTx();
        return common::Result<void>::err(commit.error());
    }
    return common::Result<void>::ok();
}

// ---------------------------------------------------------------------------
// Internal dispatch / helpers
// ---------------------------------------------------------------------------
bool TraceLinkService::nodeExists(EntityType type, const std::string& id) {
    auto res = dispatchGet(type, id);
    return res.isOk() && res.value().has_value();
}

common::Result<std::optional<Entity>> TraceLinkService::dispatchGet(
    EntityType type, const std::string& id) {
    switch (type) {
        case EntityType::Requirement: {
            auto res = reqDao_.findById(id);
            if (res.failed()) return common::Result<std::optional<Entity>>::err(res.error());
            if (!res.value()) return common::Result<std::optional<Entity>>::ok(std::nullopt);
            return common::Result<std::optional<Entity>>::ok(fromReq(*res.value()));
        }
        case EntityType::Design: {
            auto res = designDao_.findById(id);
            if (res.failed()) return common::Result<std::optional<Entity>>::err(res.error());
            if (!res.value()) return common::Result<std::optional<Entity>>::ok(std::nullopt);
            return common::Result<std::optional<Entity>>::ok(fromDesign(*res.value()));
        }
        case EntityType::Interface: {
            auto res = ifaceDao_.findById(id);
            if (res.failed()) return common::Result<std::optional<Entity>>::err(res.error());
            if (!res.value()) return common::Result<std::optional<Entity>>::ok(std::nullopt);
            return common::Result<std::optional<Entity>>::ok(fromIface(*res.value()));
        }
        case EntityType::TestCase: {
            auto res = testDao_.findById(id);
            if (res.failed()) return common::Result<std::optional<Entity>>::err(res.error());
            if (!res.value()) return common::Result<std::optional<Entity>>::ok(std::nullopt);
            return common::Result<std::optional<Entity>>::ok(fromTc(*res.value()));
        }
        case EntityType::Hazard: {
            auto res = hazardDao_.findById(id);
            if (res.failed()) return common::Result<std::optional<Entity>>::err(res.error());
            if (!res.value()) return common::Result<std::optional<Entity>>::ok(std::nullopt);
            return common::Result<std::optional<Entity>>::ok(fromHazard(*res.value()));
        }
        case EntityType::Decision: {
            auto res = decisionDao_.findById(id);
            if (res.failed()) return common::Result<std::optional<Entity>>::err(res.error());
            if (!res.value()) return common::Result<std::optional<Entity>>::ok(std::nullopt);
            return common::Result<std::optional<Entity>>::ok(fromDecision(*res.value()));
        }
        case EntityType::Assumption: {
            auto res = assumptionDao_.findById(id);
            if (res.failed()) return common::Result<std::optional<Entity>>::err(res.error());
            if (!res.value()) return common::Result<std::optional<Entity>>::ok(std::nullopt);
            return common::Result<std::optional<Entity>>::ok(fromAssumption(*res.value()));
        }
    }
    return common::Result<std::optional<Entity>>::err("unknown entity type");
}

common::Result<Entity> TraceLinkService::dispatchCreate(EntityType type, const Entity& e) {
    switch (type) {
        case EntityType::Requirement: {
            auto r = toReq(e);
            auto res = reqDao_.create(r);
            if (res.failed()) return common::Result<Entity>::err(res.error());
            return common::Result<Entity>::ok(fromReq(r));
        }
        case EntityType::Design: {
            auto d = toDesign(e);
            auto res = designDao_.create(d);
            if (res.failed()) return common::Result<Entity>::err(res.error());
            return common::Result<Entity>::ok(fromDesign(d));
        }
        case EntityType::Interface: {
            auto i = toIface(e);
            auto res = ifaceDao_.create(i);
            if (res.failed()) return common::Result<Entity>::err(res.error());
            return common::Result<Entity>::ok(fromIface(i));
        }
        case EntityType::TestCase: {
            auto t = toTc(e);
            auto res = testDao_.create(t);
            if (res.failed()) return common::Result<Entity>::err(res.error());
            return common::Result<Entity>::ok(fromTc(t));
        }
        case EntityType::Hazard: {
            auto h = toHazard(e);
            auto res = hazardDao_.create(h);
            if (res.failed()) return common::Result<Entity>::err(res.error());
            return common::Result<Entity>::ok(fromHazard(h));
        }
        case EntityType::Decision: {
            auto d = toDecision(e);
            auto res = decisionDao_.create(d);
            if (res.failed()) return common::Result<Entity>::err(res.error());
            return common::Result<Entity>::ok(fromDecision(d));
        }
        case EntityType::Assumption: {
            auto a = toAssumption(e);
            auto res = assumptionDao_.create(a);
            if (res.failed()) return common::Result<Entity>::err(res.error());
            return common::Result<Entity>::ok(fromAssumption(a));
        }
    }
    return common::Result<Entity>::err("unknown entity type");
}

common::Result<Entity> TraceLinkService::dispatchUpdate(EntityType type, const Entity& e) {
    switch (type) {
        case EntityType::Requirement: {
            auto r = toReq(e);
            auto res = reqDao_.update(r);
            if (res.failed()) return common::Result<Entity>::err(res.error());
            return common::Result<Entity>::ok(fromReq(r));
        }
        case EntityType::Design: {
            auto d = toDesign(e);
            auto res = designDao_.update(d);
            if (res.failed()) return common::Result<Entity>::err(res.error());
            return common::Result<Entity>::ok(fromDesign(d));
        }
        case EntityType::Interface: {
            auto i = toIface(e);
            auto res = ifaceDao_.update(i);
            if (res.failed()) return common::Result<Entity>::err(res.error());
            return common::Result<Entity>::ok(fromIface(i));
        }
        case EntityType::TestCase: {
            auto t = toTc(e);
            auto res = testDao_.update(t);
            if (res.failed()) return common::Result<Entity>::err(res.error());
            return common::Result<Entity>::ok(fromTc(t));
        }
        case EntityType::Hazard: {
            auto h = toHazard(e);
            auto res = hazardDao_.update(h);
            if (res.failed()) return common::Result<Entity>::err(res.error());
            return common::Result<Entity>::ok(fromHazard(h));
        }
        case EntityType::Decision: {
            auto d = toDecision(e);
            auto res = decisionDao_.update(d);
            if (res.failed()) return common::Result<Entity>::err(res.error());
            return common::Result<Entity>::ok(fromDecision(d));
        }
        case EntityType::Assumption: {
            auto a = toAssumption(e);
            auto res = assumptionDao_.update(a);
            if (res.failed()) return common::Result<Entity>::err(res.error());
            return common::Result<Entity>::ok(fromAssumption(a));
        }
    }
    return common::Result<Entity>::err("unknown entity type");
}

}  // namespace lodestar::tracelink
