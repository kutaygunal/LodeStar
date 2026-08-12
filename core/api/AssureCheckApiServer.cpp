// core/api/AssureCheckApiServer.cpp
// Phase 11 WP-6 (AssureCheck): REST implementation of the /assurecheck route
// table on top of the AssureCheck service layer (AssureCheckService,
// ComplianceEngine, ReportService, DashboardService). Uses the standard error
// model:
//   200 ok, 400 bad request, 404 not found, 500 internal
// with body {"error":{"code":...,"message":...}}.

#include "core/api/AssureCheckApiServer.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "core/adapters/Json.h"

namespace lodestar::api {

using lodestar::Json;
using lodestar::JsonError;
namespace ac = lodestar::assurecheck;

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

std::string statusToString(ac::CheckStatus s) {
    switch (s) {
        case ac::CheckStatus::Pass: return "PASS";
        case ac::CheckStatus::Fail: return "FAIL";
        case ac::CheckStatus::Na: return "NA";
        case ac::CheckStatus::Warning: return "WARNING";
    }
    return "NA";
}

Json checkResultToJson(const ac::CheckResult& r) {
    Json j = Json::object();
    j["id"] = Json::string(r.id);
    j["standard"] = Json::string(r.standardCode);
    j["item_code"] = Json::string(r.itemCode);
    j["item_id"] = Json::string(r.itemId);
    j["status"] = Json::string(statusToString(r.status));
    j["dal_level"] = Json::string(r.dalLevel);
    Json ev = Json::array();
    for (const auto& e : r.evidence) {
        Json ej = Json::object();
        ej["entity_type"] = Json::string(e.entityType);
        ej["entity_id"] = Json::string(e.entityId);
        ev.push(ej);
    }
    j["evidence"] = ev;
    j["detail"] = Json::string(r.detail);
    return j;
}

Json summaryToJson(const ac::CheckSummary& s) {
    Json j = Json::object();
    j["total"] = Json::number(static_cast<double>(s.total));
    j["pass"] = Json::number(static_cast<double>(s.pass));
    j["fail"] = Json::number(static_cast<double>(s.fail));
    j["na"] = Json::number(static_cast<double>(s.na));
    j["warning"] = Json::number(static_cast<double>(s.warning));
    j["percent"] = Json::number(static_cast<double>(s.percent));
    return j;
}

Json coverageToJson(const ac::CoverageSummary& c) {
    Json j = Json::object();
    j["total"] = Json::number(static_cast<double>(c.total));
    j["applicable"] = Json::number(static_cast<double>(c.applicable));
    j["pass"] = Json::number(static_cast<double>(c.pass));
    j["fail"] = Json::number(static_cast<double>(c.fail));
    j["na"] = Json::number(static_cast<double>(c.na));
    j["warning"] = Json::number(static_cast<double>(c.warning));
    j["percent"] = Json::number(static_cast<double>(c.percent));
    return j;
}

}  // namespace

// ---------------------------------------------------------------------------
// Constructor + route registration.
// ---------------------------------------------------------------------------
AssureCheckApiServer::AssureCheckApiServer(ac::AssureCheckService& standards,
                                           ac::ComplianceEngine& engine,
                                           ac::ReportService& reports,
                                           ac::DashboardService& dashboard)
    : standards_(standards), engine_(engine), reports_(reports),
      dashboard_(dashboard) {}

void AssureCheckApiServer::setup(HttpServer& server) {
    server.route("GET", "/assurecheck/standards",
                 [this](const HttpRequest& r) { return standardsList(r); });
    server.route("GET", "/assurecheck/standards/<code>",
                 [this](const HttpRequest& r) { return standardGet(r); });
    server.route("POST", "/assurecheck/checks",
                 [this](const HttpRequest& r) { return checksRun(r); });
    server.route("GET", "/assurecheck/summary",
                 [this](const HttpRequest& r) { return summaryGet(r); });
    server.route("GET", "/assurecheck/dashboard",
                 [this](const HttpRequest& r) { return dashboardGet(r); });
}

// ---------------------------------------------------------------------------
// Standards.
// ---------------------------------------------------------------------------
HttpResponse AssureCheckApiServer::standardsList(const HttpRequest&) {
    auto res = standards_.listStandards();
    if (res.failed()) return err(500, res.error());

    Json arr = Json::array();
    for (const auto& s : res.value()) {
        Json j = Json::object();
        j["code"] = Json::string(s.code);
        j["name"] = Json::string(s.name);
        j["description"] = Json::string(s.description);
        arr.push(j);
    }
    Json out = Json::object();
    out["standards"] = arr;
    return jsonOk(out);
}

HttpResponse AssureCheckApiServer::standardGet(const HttpRequest& req) {
    auto codeIt = req.params.find("code");
    if (codeIt == req.params.end() || codeIt->second.empty()) {
        return err(400, "missing standard code");
    }
    auto res = standards_.getStandard(codeIt->second);
    if (res.failed()) return err(500, res.error());
    if (!res.value()) return err(404, "standard not found: " + codeIt->second);

    const auto& s = *res.value();
    Json j = Json::object();
    j["code"] = Json::string(s.code);
    j["name"] = Json::string(s.name);
    j["description"] = Json::string(s.description);
    return jsonOk(j);
}

// ---------------------------------------------------------------------------
// Checks.
// ---------------------------------------------------------------------------
HttpResponse AssureCheckApiServer::checksRun(const HttpRequest& req) {
    std::fprintf(stderr, "[DBG] checksRun method=%s path=%s body=%s\n", req.method.c_str(), req.path.c_str(), req.body.c_str());
    Json body;
    try {
        body = Json::parse(req.body);
    } catch (const JsonError& e) {
        return err(400, "invalid JSON body: " + std::string(e.what()));
    }
    if (!body.has("standard") || body.at("standard").asString().empty()) {
        return err(400, "missing required field 'standard'");
    }
    if (!body.has("dal") || body.at("dal").asString().empty()) {
        return err(400, "missing required field 'dal'");
    }
    const std::string standard = body.at("standard").asString();
    const std::string dal = body.at("dal").asString();

    auto run = engine_.runChecks(standard, dal);
    if (run.failed()) return err(400, run.error());

    auto stored = engine_.storeResults(run.value());
    if (stored.failed()) return err(500, stored.error());

    Json arr = Json::array();
    for (const auto& r : run.value()) arr.push(checkResultToJson(r));
    Json out = Json::object();
    out["results"] = arr;
    std::fprintf(stderr, "[DBG] checksRun returning 200 size=%zu\n", out.dump().size());
    return jsonOk(out);
}

// ---------------------------------------------------------------------------
// Summary.
// ---------------------------------------------------------------------------
HttpResponse AssureCheckApiServer::summaryGet(const HttpRequest& req) {
    std::fprintf(stderr, "[DBG] summaryGet path=%s params=%zu\n", req.path.c_str(), req.params.size());
    auto stdIt = req.params.find("standard");
    if (stdIt == req.params.end() || stdIt->second.empty()) {
        return err(400, "missing required query parameter 'standard'");
    }
    auto res = engine_.summaryFor(stdIt->second);
    if (res.failed()) return err(500, res.error());

    Json out = Json::object();
    out["summary"] = summaryToJson(res.value());
    std::fprintf(stderr, "[DBG] summaryGet returning 200\n");
    return jsonOk(out);
}

// ---------------------------------------------------------------------------
// Dashboard.
// ---------------------------------------------------------------------------
HttpResponse AssureCheckApiServer::dashboardGet(const HttpRequest& req) {
    std::fprintf(stderr, "[DBG] dashboardGet path=%s\n", req.path.c_str());
    auto res = dashboard_.dashboard();
    if (res.failed()) return err(500, res.error());

    Json arr = Json::array();
    for (const auto& st : res.value()) {
        Json j = Json::object();
        j["code"] = Json::string(st.code);
        j["name"] = Json::string(st.name);
        j["coverage"] = coverageToJson(st.coverage);
        arr.push(j);
    }
    Json out = Json::object();
    out["standards"] = arr;
    std::fprintf(stderr, "[DBG] dashboardGet returning 200 size=%zu\n", out.dump().size());
    return jsonOk(out);
}

}  // namespace lodestar::api
