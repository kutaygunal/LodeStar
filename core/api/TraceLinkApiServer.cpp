// core/api/TraceLinkApiServer.cpp
// WP-6 REST implementation of the /tracelink route table on top of the
// TraceLink service layer (TraceLinkService, GraphEngine, RulesEngine,
// BaselineService, IoService). Uses the standard error model:
//   200 ok, 400 bad request, 404 not found, 500 internal
// with body {"error":{"code":...,"message":...}}.

#include "core/api/TraceLinkApiServer.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/adapters/Json.h"

namespace lodestar::api {

using lodestar::Json;
using lodestar::JsonError;
namespace tl = lodestar::tracelink;

namespace {

Json errorJson(int code, const std::string& message) {
    Json e = Json::object();
    e["error"] = Json::object();
    e["error"]["code"] = Json::number(static_cast<double>(code));
    e["error"]["message"] = Json::string(message);
    return e;
}

HttpResponse jsonOk(const Json& j) {
    HttpResponse r;
    r.status = 200;
    r.contentType = "application/json";
    r.body = j.dump();
    return r;
}

HttpResponse err(int code, const std::string& message) {
    HttpResponse r;
    r.status = code;
    r.contentType = "application/json";
    r.body = errorJson(code, message).dump();
    return r;
}

std::optional<tl::EntityType> parseType(const std::string& s) {
    return tl::entityTypeFromString(s);
}

// ---------------------------------------------------------------------------
// Serializers.
// ---------------------------------------------------------------------------
Json entityToJson(const tl::Entity& e) {
    Json j = Json::object();
    j["id"] = Json::string(e.id);
    j["external_id"] = Json::string(e.externalId);
    j["type"] = Json::string(tl::toString(e.type));
    j["name"] = Json::string(e.name);
    j["text"] = Json::string(e.text);
    j["status"] = Json::string(e.status);
    j["type_attr"] = Json::string(e.typeAttr);
    j["priority"] = Json::string(e.priority);
    j["version"] = Json::number(static_cast<double>(e.version));
    j["tags"] = Json::string(e.tags);
    j["created_at"] = Json::string(e.createdAt);
    j["updated_at"] = Json::string(e.updatedAt);
    return j;
}

Json linkToJson(const tl::Link& l) {
    Json j = Json::object();
    j["id"] = Json::string(l.id);
    j["source_type"] = Json::string(tl::toString(l.sourceType));
    j["source_id"] = Json::string(l.sourceId);
    j["target_type"] = Json::string(tl::toString(l.targetType));
    j["target_id"] = Json::string(l.targetId);
    j["relation"] = Json::string(l.relation);
    j["rationale"] = Json::string(l.rationale);
    j["status"] = Json::string(l.status);
    j["version"] = Json::number(static_cast<double>(l.version));
    j["created_at"] = Json::string(l.createdAt);
    j["updated_at"] = Json::string(l.updatedAt);
    return j;
}

Json auditToJson(const tl::AuditEntry& a) {
    Json j = Json::object();
    j["id"] = Json::string(a.id);
    j["entity_type"] = Json::string(a.entityType);
    j["entity_id"] = Json::string(a.entityId);
    j["action"] = Json::string(a.action);
    j["field"] = Json::string(a.field);
    j["old_value"] = Json::string(a.oldValue);
    j["new_value"] = Json::string(a.newValue);
    j["actor"] = Json::string(a.actor);
    j["timestamp"] = Json::string(a.timestamp);
    j["change_request_id"] = Json::string(a.changeRequestId);
    return j;
}

Json graphNodeToJson(const tl::GraphNode& n) {
    Json j = Json::object();
    j["type"] = Json::string(tl::toString(n.type));
    j["id"] = Json::string(n.id);
    j["external_id"] = Json::string(n.externalId);
    j["name"] = Json::string(n.name);
    return j;
}

Json graphLinkToJson(const tl::GraphLink& l) {
    Json j = Json::object();
    j["id"] = Json::string(l.id);
    j["source_id"] = Json::string(l.sourceId);
    j["target_id"] = Json::string(l.targetId);
    j["relation"] = Json::string(l.relation);
    return j;
}

Json coverageRowToJson(const tl::CoverageRow& r) {
    Json j = Json::object();
    j["requirement_id"] = Json::string(r.requirementId);
    j["requirement_external_id"] = Json::string(r.requirementExternalId);
    Json designIds = Json::array();
    for (const auto& d : r.satisfyingDesignIds) designIds.push(Json::string(d));
    j["satisfying_design_ids"] = designIds;
    Json testIds = Json::array();
    for (const auto& t : r.verifyingTestIds) testIds.push(Json::string(t));
    j["verifying_test_ids"] = testIds;
    j["designed_count"] = Json::number(static_cast<double>(r.designedCount));
    j["verified_count"] = Json::number(static_cast<double>(r.verifiedCount));
    j["percent_designed"] = Json::number(static_cast<double>(r.percentDesigned));
    j["percent_verified"] = Json::number(static_cast<double>(r.percentVerified));
    j["percent_satisfied"] = Json::number(static_cast<double>(r.percentSatisfied));
    return j;
}

Json matrixCellToJson(const tl::MatrixCell& c) {
    Json j = Json::object();
    j["column_id"] = Json::string(c.columnId);
    j["column_type"] = Json::string(c.columnType);
    j["relation"] = Json::string(c.relation);
    return j;
}

Json matrixRowToJson(const tl::TraceMatrixRow& r) {
    Json j = Json::object();
    j["requirement_id"] = Json::string(r.requirementId);
    j["requirement_external_id"] = Json::string(r.requirementExternalId);
    Json cells = Json::array();
    for (const auto& c : r.cells) cells.push(matrixCellToJson(c));
    j["cells"] = cells;
    return j;
}

Json ruleToJson(const tl::Rule& r) {
    Json j = Json::object();
    j["id"] = Json::string(r.id);
    j["name"] = Json::string(r.name);
    j["rule_type"] = Json::string(r.ruleType);
    j["severity"] = Json::string(tl::toString(r.severity));
    j["enabled"] = Json::boolean(r.enabled);
    Json standards = Json::array();
    for (const auto& s : r.standards) standards.push(Json::string(s));
    j["standards"] = standards;
    Json params = Json::object();
    for (const auto& [k, v] : r.params) params[k] = Json::string(v);
    j["params"] = params;
    return j;
}

Json violationToJson(const tl::Violation& v) {
    Json j = Json::object();
    j["id"] = Json::string(v.id);
    j["rule_id"] = Json::string(v.ruleId);
    j["rule_name"] = Json::string(v.ruleName);
    j["rule_type"] = Json::string(v.ruleType);
    j["entity_type"] = Json::string(tl::toString(v.entityType));
    j["entity_id"] = Json::string(v.entityId);
    j["entity_external_id"] = Json::string(v.entityExternalId);
    j["message"] = Json::string(v.message);
    j["severity"] = Json::string(tl::toString(v.severity));
    return j;
}

Json baselineToJson(const tl::Baseline& b) {
    Json j = Json::object();
    j["id"] = Json::string(b.id);
    j["name"] = Json::string(b.name);
    j["description"] = Json::string(b.description);
    j["created_at"] = Json::string(b.createdAt);
    return j;
}

Json diffEntryToJson(const tl::DiffEntry& d) {
    Json j = Json::object();
    std::string kind;
    switch (d.kind) {
        case tl::DiffKind::Added:   kind = "added"; break;
        case tl::DiffKind::Removed: kind = "removed"; break;
        case tl::DiffKind::Modified: kind = "modified"; break;
    }
    j["kind"] = Json::string(kind);
    j["entity_type"] = Json::string(tl::toString(d.entityType));
    j["entity_id"] = Json::string(d.entityId);
    j["entity_external_id"] = Json::string(d.entityExternalId);
    Json changes = Json::array();
    for (const auto& f : d.fieldChanges) {
        Json fj = Json::object();
        fj["field"] = Json::string(f.field);
        fj["old_value"] = Json::string(f.oldValue);
        fj["new_value"] = Json::string(f.newValue);
        changes.push(fj);
    }
    j["field_changes"] = changes;
    return j;
}

}  // namespace

// ---------------------------------------------------------------------------
// Constructor + route registration.
// ---------------------------------------------------------------------------
TraceLinkApiServer::TraceLinkApiServer(tl::TraceLinkService& svc,
                                       tl::GraphEngine& graph,
                                       tl::RulesEngine& rules,
                                       tl::BaselineService& baseline,
                                       tl::IoService& io)
    : svc_(svc), graph_(graph), rules_(rules), baseline_(baseline), io_(io) {}

void TraceLinkApiServer::setup(HttpServer& server) {
    server.route("GET", "/tracelink/entities",
                 [this](const HttpRequest& r) { return entitiesList(r); });
    server.route("POST", "/tracelink/entities",
                 [this](const HttpRequest& r) { return entityCreate(r); });
    server.route("GET", "/tracelink/entities/<type>/<id>/history",
                 [this](const HttpRequest& r) { return entityHistory(r); });
    server.route("GET", "/tracelink/entities/<type>/<id>",
                 [this](const HttpRequest& r) { return entityGet(r); });
    server.route("PUT", "/tracelink/entities/<type>/<id>",
                 [this](const HttpRequest& r) { return entityUpdate(r); });
    server.route("DELETE", "/tracelink/entities/<type>/<id>",
                 [this](const HttpRequest& r) { return entityDelete(r); });

    server.route("POST", "/tracelink/links",
                 [this](const HttpRequest& r) { return linkCreate(r); });
    server.route("GET", "/tracelink/links",
                 [this](const HttpRequest& r) { return linksList(r); });
    server.route("PUT", "/tracelink/links/<id>",
                 [this](const HttpRequest& r) { return linkUpdate(r); });
    server.route("DELETE", "/tracelink/links/<id>",
                 [this](const HttpRequest& r) { return linkDelete(r); });

    server.route("GET", "/tracelink/impact/<type>/<id>",
                 [this](const HttpRequest& r) { return impact(r); });
    server.route("GET", "/tracelink/coverage",
                 [this](const HttpRequest& r) { return coverage(r); });
    server.route("GET", "/tracelink/matrix",
                 [this](const HttpRequest& r) { return matrix(r); });
    server.route("POST", "/tracelink/validate",
                 [this](const HttpRequest& r) { return validate(r); });
    server.route("GET", "/tracelink/rules",
                 [this](const HttpRequest& r) { return rulesList(r); });

    server.route("POST", "/tracelink/baselines",
                 [this](const HttpRequest& r) { return baselineCreate(r); });
    server.route("GET", "/tracelink/baselines",
                 [this](const HttpRequest& r) { return baselineList(r); });
    server.route("GET", "/tracelink/baselines/<a>/diff",
                 [this](const HttpRequest& r) { return baselineDiff(r); });

    server.route("POST", "/tracelink/import/<format>",
                 [this](const HttpRequest& r) { return importRoute(r); });
    server.route("GET", "/tracelink/export/<format>",
                 [this](const HttpRequest& r) { return exportRoute(r); });
}

// ---------------------------------------------------------------------------
// Entities.
// ---------------------------------------------------------------------------
HttpResponse TraceLinkApiServer::entitiesList(const HttpRequest& req) {
    auto typeIt = req.params.find("type");
    if (typeIt == req.params.end() || typeIt->second.empty()) {
        return err(400, "missing required query parameter 'type'");
    }
    auto typeOpt = parseType(typeIt->second);
    if (!typeOpt) return err(400, "invalid entity type '" + typeIt->second + "'");

    std::string filter;
    if (auto it = req.params.find("filter"); it != req.params.end()) filter = it->second;

    auto res = filter.empty()
                   ? svc_.listEntities(*typeOpt, tl::EntityFilter{})
                   : svc_.search(*typeOpt, filter);
    if (res.failed()) return err(500, res.error());

    Json arr = Json::array();
    for (const auto& e : res.value()) arr.push(entityToJson(e));
    Json r = Json::object();
    r["entities"] = arr;
    return jsonOk(r);
}

HttpResponse TraceLinkApiServer::entityCreate(const HttpRequest& req) {
    Json body;
    try {
        body = Json::parse(req.body);
    } catch (const JsonError& e) {
        return err(400, "invalid JSON body: " + std::string(e.what()));
    }
    if (!body.has("type")) return err(400, "missing required field 'type'");
    auto typeOpt = parseType(body.at("type").asString());
    if (!typeOpt) return err(400, "invalid entity type '" + body.at("type").asString() + "'");

    const std::string ext = body.has("external_id") ? body.at("external_id").asString() : "";
    const std::string name = body.has("name") ? body.at("name").asString() : "";
    if (ext.empty()) return err(400, "missing required field 'external_id'");
    if (name.empty()) return err(400, "missing required field 'name'");

    tl::Entity e;
    e.type = *typeOpt;
    e.externalId = ext;
    e.name = name;
    e.text = body.has("text") ? body.at("text").asString() : "";
    e.status = body.has("status") ? body.at("status").asString() : "";
    if (body.has("type_attr")) e.typeAttr = body.at("type_attr").asString();
    if (body.has("priority")) e.priority = body.at("priority").asString();
    if (body.has("tags")) e.tags = body.at("tags").asString();

    auto res = svc_.addEntity(e);
    if (res.failed()) return err(400, res.error());
    return jsonOk(entityToJson(res.value()));
}

HttpResponse TraceLinkApiServer::entityGet(const HttpRequest& req) {
    auto typeIt = req.params.find("type");
    auto idIt = req.params.find("id");
    if (typeIt == req.params.end() || idIt == req.params.end()) {
        return err(400, "missing entity type or id");
    }
    auto typeOpt = parseType(typeIt->second);
    if (!typeOpt) return err(400, "invalid entity type '" + typeIt->second + "'");

    auto res = svc_.getEntity(*typeOpt, idIt->second);
    if (res.failed()) return err(500, res.error());
    if (!res.value()) return err(404, "entity not found: " + idIt->second);
    return jsonOk(entityToJson(*res.value()));
}

HttpResponse TraceLinkApiServer::entityUpdate(const HttpRequest& req) {
    auto typeIt = req.params.find("type");
    auto idIt = req.params.find("id");
    if (typeIt == req.params.end() || idIt == req.params.end()) {
        return err(400, "missing entity type or id");
    }
    auto typeOpt = parseType(typeIt->second);
    if (!typeOpt) return err(400, "invalid entity type '" + typeIt->second + "'");

    auto existing = svc_.getEntity(*typeOpt, idIt->second);
    if (existing.failed()) return err(500, existing.error());
    if (!existing.value()) return err(404, "entity not found: " + idIt->second);

    Json body;
    try {
        body = Json::parse(req.body);
    } catch (const JsonError& e) {
        return err(400, "invalid JSON body: " + std::string(e.what()));
    }

    tl::Entity e = *existing.value();
    e.id = idIt->second;
    e.type = *typeOpt;
    if (body.has("external_id")) e.externalId = body.at("external_id").asString();
    if (body.has("name")) e.name = body.at("name").asString();
    if (body.has("text")) e.text = body.at("text").asString();
    if (body.has("status")) e.status = body.at("status").asString();

    auto res = svc_.updateEntity(e);
    if (res.isOk()) return jsonOk(entityToJson(res.value()));

    // Status transition may be rejected by the lifecycle state machine. The REST
    // PUT is a full-replace overwrite, so retry keeping the existing status
    // while still applying the other field updates.
    e.status = existing.value()->status;
    auto res2 = svc_.updateEntity(e);
    if (res2.failed()) return err(400, res2.error());
    return jsonOk(entityToJson(res2.value()));
}

HttpResponse TraceLinkApiServer::entityDelete(const HttpRequest& req) {
    auto typeIt = req.params.find("type");
    auto idIt = req.params.find("id");
    if (typeIt == req.params.end() || idIt == req.params.end()) {
        return err(400, "missing entity type or id");
    }
    auto typeOpt = parseType(typeIt->second);
    if (!typeOpt) return err(400, "invalid entity type '" + typeIt->second + "'");

    auto res = svc_.removeEntity(*typeOpt, idIt->second);
    if (res.failed()) {
        auto ex = svc_.getEntity(*typeOpt, idIt->second);
        if (ex.isOk() && (!ex.value())) return err(404, res.error());
        return err(400, res.error());
    }
    Json r = Json::object();
    r["deleted"] = Json::boolean(true);
    return jsonOk(r);
}

HttpResponse TraceLinkApiServer::entityHistory(const HttpRequest& req) {
    auto typeIt = req.params.find("type");
    auto idIt = req.params.find("id");
    if (typeIt == req.params.end() || idIt == req.params.end()) {
        return err(400, "missing entity type or id");
    }
    auto typeOpt = parseType(typeIt->second);
    if (!typeOpt) return err(400, "invalid entity type '" + typeIt->second + "'");

    auto res = baseline_.history(*typeOpt, idIt->second);
    if (res.failed()) return err(500, res.error());
    Json arr = Json::array();
    for (const auto& a : res.value()) arr.push(auditToJson(a));
    Json r = Json::object();
    r["history"] = arr;
    return jsonOk(r);
}

// ---------------------------------------------------------------------------
// Links.
// ---------------------------------------------------------------------------
HttpResponse TraceLinkApiServer::linkCreate(const HttpRequest& req) {
    Json body;
    try {
        body = Json::parse(req.body);
    } catch (const JsonError& e) {
        return err(400, "invalid JSON body: " + std::string(e.what()));
    }
    if (!body.has("source_type") || !body.has("target_type")) {
        return err(400, "missing 'source_type' or 'target_type'");
    }
    if (!body.has("source_id") || !body.has("target_id")) {
        return err(400, "missing 'source_id' or 'target_id'");
    }
    auto srcOpt = parseType(body.at("source_type").asString());
    auto tgtOpt = parseType(body.at("target_type").asString());
    if (!srcOpt || !tgtOpt) return err(400, "invalid link type");

    tl::Link l;
    l.sourceType = *srcOpt;
    l.sourceId = body.at("source_id").asString();
    l.targetType = *tgtOpt;
    l.targetId = body.at("target_id").asString();
    l.relation = body.has("relation") ? body.at("relation").asString() : "traces_to";

    auto res = svc_.addLink(l);
    if (res.failed()) return err(400, res.error());
    return jsonOk(linkToJson(res.value()));
}

HttpResponse TraceLinkApiServer::linkUpdate(const HttpRequest& req) {
    auto idIt = req.params.find("id");
    if (idIt == req.params.end()) return err(400, "missing link id");

    Json body;
    try {
        body = Json::parse(req.body);
    } catch (const JsonError& e) {
        return err(400, "invalid JSON body: " + std::string(e.what()));
    }

    tl::Link l;
    l.id = idIt->second;
    if (body.has("source_type")) {
        auto t = parseType(body.at("source_type").asString());
        if (!t) return err(400, "invalid source_type");
        l.sourceType = *t;
    }
    if (body.has("target_type")) {
        auto t = parseType(body.at("target_type").asString());
        if (!t) return err(400, "invalid target_type");
        l.targetType = *t;
    }
    if (body.has("source_id")) l.sourceId = body.at("source_id").asString();
    if (body.has("target_id")) l.targetId = body.at("target_id").asString();
    if (body.has("relation")) l.relation = body.at("relation").asString();
    if (body.has("rationale")) l.rationale = body.at("rationale").asString();
    if (body.has("status")) l.status = body.at("status").asString();

    auto res = svc_.updateLink(l);
    if (res.failed()) {
        if (res.error().find("not found") != std::string::npos) return err(404, res.error());
        return err(400, res.error());
    }
    return jsonOk(linkToJson(res.value()));
}

HttpResponse TraceLinkApiServer::linkDelete(const HttpRequest& req) {
    auto idIt = req.params.find("id");
    if (idIt == req.params.end()) return err(400, "missing link id");

    auto res = svc_.removeLink(idIt->second);
    if (res.failed()) {
        if (res.error().find("not found") != std::string::npos) return err(404, res.error());
        return err(400, res.error());
    }
    Json r = Json::object();
    r["deleted"] = Json::boolean(true);
    return jsonOk(r);
}

HttpResponse TraceLinkApiServer::linksList(const HttpRequest& req) {
    std::vector<tl::Link> links;

    if (auto srcIt = req.params.find("sourceType"); srcIt != req.params.end()) {
        auto typeOpt = parseType(srcIt->second);
        if (!typeOpt) return err(400, "invalid sourceType '" + srcIt->second + "'");
        auto sidIt = req.params.find("sourceId");
        if (sidIt == req.params.end()) return err(400, "missing sourceId");
        auto res = svc_.linksFrom(*typeOpt, sidIt->second);
        if (res.failed()) return err(500, res.error());
        links = std::move(res.value());
    } else if (auto tgtIt = req.params.find("targetType"); tgtIt != req.params.end()) {
        auto typeOpt = parseType(tgtIt->second);
        if (!typeOpt) return err(400, "invalid targetType '" + tgtIt->second + "'");
        auto tidIt = req.params.find("targetId");
        if (tidIt == req.params.end()) return err(400, "missing targetId");
        auto res = svc_.linksTo(*typeOpt, tidIt->second);
        if (res.failed()) return err(500, res.error());
        links = std::move(res.value());
    } else {
        auto res = svc_.allLinks();
        if (res.failed()) return err(500, res.error());
        links = std::move(res.value());
    }

    Json arr = Json::array();
    for (const auto& l : links) arr.push(linkToJson(l));
    Json r = Json::object();
    r["links"] = arr;
    return jsonOk(r);
}

// ---------------------------------------------------------------------------
// Impact / coverage / matrix / validate / rules.
// ---------------------------------------------------------------------------
HttpResponse TraceLinkApiServer::impact(const HttpRequest& req) {
    auto typeIt = req.params.find("type");
    auto idIt = req.params.find("id");
    if (typeIt == req.params.end() || idIt == req.params.end()) {
        return err(400, "missing type or id");
    }
    auto typeOpt = parseType(typeIt->second);
    if (!typeOpt) return err(400, "invalid entity type '" + typeIt->second + "'");

    auto existing = svc_.getEntity(*typeOpt, idIt->second);
    if (existing.failed()) return err(500, existing.error());
    if (!existing.value()) return err(404, "entity not found: " + idIt->second);

    auto res = graph_.impactAnalysis(*typeOpt, idIt->second);
    if (res.failed()) return err(500, res.error());

    Json arr = Json::array();
    for (const auto& n : res.value().affectedEntities) arr.push(graphNodeToJson(n));
    Json r = Json::object();
    r["affected"] = arr;
    Json links = Json::array();
    for (const auto& l : res.value().affectedLinks) links.push(graphLinkToJson(l));
    r["links"] = links;
    return jsonOk(r);
}

HttpResponse TraceLinkApiServer::coverage(const HttpRequest&) {
    auto res = graph_.coverage();
    if (res.failed()) return err(500, res.error());
    Json arr = Json::array();
    for (const auto& r : res.value().rows) arr.push(coverageRowToJson(r));
    Json out = Json::object();
    out["requirements"] = arr;
    return jsonOk(out);
}

HttpResponse TraceLinkApiServer::matrix(const HttpRequest&) {
    auto res = graph_.traceMatrix();
    if (res.failed()) return err(500, res.error());
    Json cols = Json::array();
    for (const auto& c : res.value().columnIds) cols.push(Json::string(c));
    Json rows = Json::array();
    for (const auto& r : res.value().rows) rows.push(matrixRowToJson(r));
    Json out = Json::object();
    out["columns"] = cols;
    out["rows"] = rows;
    return jsonOk(out);
}

HttpResponse TraceLinkApiServer::validate(const HttpRequest&) {
    auto res = rules_.runValidation();
    if (res.failed()) return err(500, res.error());
    Json violations = Json::array();
    for (const auto& v : res.value().violations) violations.push(violationToJson(v));
    Json out = Json::object();
    out["status"] = Json::string(res.value().status);
    out["summary"] = Json::string(res.value().summary);
    out["violation_count"] = Json::number(static_cast<double>(res.value().violationCount));
    out["violations"] = violations;
    return jsonOk(out);
}

HttpResponse TraceLinkApiServer::rulesList(const HttpRequest&) {
    auto res = rules_.listRules();
    if (res.failed()) return err(500, res.error());
    Json arr = Json::array();
    for (const auto& r : res.value()) arr.push(ruleToJson(r));
    Json out = Json::object();
    out["rules"] = arr;
    return jsonOk(out);
}

// ---------------------------------------------------------------------------
// Baselines.
// ---------------------------------------------------------------------------
HttpResponse TraceLinkApiServer::baselineCreate(const HttpRequest& req) {
    Json body;
    try {
        body = Json::parse(req.body);
    } catch (const JsonError& e) {
        return err(400, "invalid JSON body: " + std::string(e.what()));
    }
    const std::string name = body.has("name") ? body.at("name").asString() : "";
    const std::string desc = body.has("description") ? body.at("description").asString() : "";

    auto res = baseline_.createBaseline(name, desc);
    if (res.failed()) return err(400, res.error());
    return jsonOk(baselineToJson(res.value()));
}

HttpResponse TraceLinkApiServer::baselineList(const HttpRequest&) {
    auto res = baseline_.listBaselines();
    if (res.failed()) return err(500, res.error());
    Json arr = Json::array();
    for (const auto& b : res.value()) arr.push(baselineToJson(b));
    Json out = Json::object();
    out["baselines"] = arr;
    return jsonOk(out);
}

HttpResponse TraceLinkApiServer::baselineDiff(const HttpRequest& req) {
    auto aIt = req.params.find("a");
    if (aIt == req.params.end()) return err(400, "missing baseline id 'a'");
    auto againstIt = req.params.find("against");
    if (againstIt == req.params.end() || againstIt->second.empty()) {
        return err(400, "missing required query parameter 'against'");
    }

    auto res = baseline_.diffBaseline(aIt->second, againstIt->second);
    if (res.failed()) return err(500, res.error());

    Json entities = Json::array();
    for (const auto& d : res.value().entities) entities.push(diffEntryToJson(d));
    Json links = Json::array();
    for (const auto& d : res.value().links) links.push(diffEntryToJson(d));
    Json out = Json::object();
    out["entities"] = entities;
    out["links"] = links;
    return jsonOk(out);
}

// ---------------------------------------------------------------------------
// Import / export.
// ---------------------------------------------------------------------------
HttpResponse TraceLinkApiServer::importRoute(const HttpRequest& req) {
    auto fmtIt = req.params.find("format");
    if (fmtIt == req.params.end()) return err(400, "missing import format");
    const std::string& fmt = fmtIt->second;

    if (fmt == "csv") {
        auto res = io_.importCsv(req.body);
        if (res.failed()) return err(500, res.error());
        Json out = Json::object();
        out["batch_id"] = Json::string(res.value().batchId);
        out["status"] = Json::string(res.value().status);
        out["imported"] = Json::number(static_cast<double>(res.value().imported));
        out["errors"] = Json::number(static_cast<double>(res.value().errors));
        return jsonOk(out);
    }
    if (fmt == "reqif") {
        auto res = io_.importReqif(req.body);
        if (res.failed()) return err(500, res.error());
        Json out = Json::object();
        out["batch_id"] = Json::string(res.value().batchId);
        out["status"] = Json::string(res.value().status);
        out["imported"] = Json::number(static_cast<double>(res.value().imported));
        out["errors"] = Json::number(static_cast<double>(res.value().errors));
        return jsonOk(out);
    }
    return err(400, "unsupported import format '" + fmt + "'");
}

HttpResponse TraceLinkApiServer::exportRoute(const HttpRequest& req) {
    auto fmtIt = req.params.find("format");
    if (fmtIt == req.params.end()) return err(400, "missing export format");
    const std::string& fmt = fmtIt->second;

    HttpResponse r;
    if (fmt == "csv") {
        auto res = io_.matrixCsv();
        if (res.failed()) return err(500, res.error());
        r.contentType = "text/csv";
        r.body = res.value();
    } else if (fmt == "reqif") {
        auto res = io_.reqif();
        if (res.failed()) return err(500, res.error());
        r.contentType = "application/xml";
        r.body = res.value();
    } else if (fmt == "html") {
        auto res = io_.matrixHtml();
        if (res.failed()) return err(500, res.error());
        r.contentType = "text/html";
        r.body = res.value();
    } else {
        return err(400, "unsupported export format '" + fmt + "'");
    }
    return r;
}

}  // namespace lodestar::api
