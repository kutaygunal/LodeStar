// core/api/ApiServer.cpp
// Implementation of the thin REST API over the adapter registry + ScenarioForge.

#include "core/api/ApiServer.h"

#include <string>
#include <utility>

#include "core/adapters/Adapter.h"
#include "core/adapters/Json.h"
#include "core/scenario/frames/Frames.h"

namespace lodestar::api {

using lodestar::adapters::AdapterError;
using lodestar::adapters::AdapterRegistry;
using lodestar::adapters::IAdapter;
using lodestar::Json;
using lodestar::JsonError;
using lodestar::scenario::BroadcastEphemeris;
using lodestar::scenario::GpsTime;
using lodestar::scenario::Scenario;
using lodestar::scenario::ScenarioConfig;

namespace {

Json errorJson(int code, const std::string& message) {
    Json e = Json::object();
    e["error"] = Json::object();
    e["error"]["code"] = Json::number(code);
    e["error"]["message"] = Json::string(message);
    return e;
}

// Serialize a ScenarioEpoch to JSON for the /scenario/step endpoint.
Json epochToJson(const lodestar::scenario::ScenarioEpoch& epoch) {
    Json out = Json::object();

    Json views = Json::array();
    for (const auto& v : epoch.views) {
        Json vj = Json::object();
        vj["prn"] = Json::number(v.prn);
        vj["visible"] = Json::boolean(v.visible);
        vj["elevationRad"] = Json::number(v.elevationRad);
        vj["azimuthRad"] = Json::number(v.azimuthRad);
        vj["slantRange"] = Json::number(v.slantRange);
        views.push(vj);
    }
    out["views"] = views;
    out["numViews"] = Json::number(static_cast<double>(epoch.views.size()));

    if (epoch.pvt.isOk()) {
        const auto& p = epoch.pvt.value();
        Json pj = Json::object();
        pj["valid"] = Json::boolean(p.valid);
        pj["numSats"] = Json::number(p.numSats);
        pj["x"] = Json::number(p.posEcef.x);
        pj["y"] = Json::number(p.posEcef.y);
        pj["z"] = Json::number(p.posEcef.z);
        pj["pdop"] = Json::number(p.pdop);
        pj["hdop"] = Json::number(p.hdop);
        pj["vdop"] = Json::number(p.vdop);
        out["pvt"] = pj;
    } else {
        Json pj = Json::object();
        pj["valid"] = Json::boolean(false);
        pj["error"] = Json::string(epoch.pvt.error());
        out["pvt"] = pj;
    }

    Json pl = Json::object();
    pl["valid"] = Json::boolean(epoch.protection.valid);
    pl["hpl"] = Json::number(epoch.protection.hpl);
    pl["vpl"] = Json::number(epoch.protection.vpl);
    out["protection"] = pl;

    Json nmea = Json::array();
    for (const auto& s : epoch.nmea) nmea.push(Json::string(s));
    out["nmea"] = nmea;

    return out;
}

}  // namespace

ApiServer::ApiServer(AdapterRegistry& registry, int version)
    : registry_(registry), version_(version) {
    buildDefaultScenario();
}

void ApiServer::setup(HttpServer& server) {
    server.route("GET", "/health",
                 [this](const HttpRequest& r) { return health(r); });
    server.route("GET", "/adapters",
                 [this](const HttpRequest& r) { return listAdapters(r); });
    server.route("POST", "/adapters/<name>/invoke",
                 [this](const HttpRequest& r) { return invokeAdapter(r); });
    server.route("GET", "/scenario/satellites",
                 [this](const HttpRequest& r) { return scenarioSatellites(r); });
    server.route("POST", "/scenario/step",
                 [this](const HttpRequest& r) { return scenarioStep(r); });
}

void ApiServer::buildDefaultScenario() {
    ScenarioConfig cfg;
    cfg.elevationMaskRad = 0.05;
    Scenario s(cfg);
    // Six GPS satellites, spatially varied so >=4 are visible at the origin.
    for (int i = 0; i < 6; ++i) {
        BroadcastEphemeris eph;
        eph.sqrtA = 5153.6556;
        eph.e = 0.003;
        eph.i0 = 0.959931;                 // ~55 deg
        eph.omega0 = -1.5707963;
        eph.argp = static_cast<double>(i) * 1.0;
        eph.M0 = static_cast<double>(i) * 1.0;
        eph.toe = 0.0;
        eph.af0 = 0.0; eph.af1 = 0.0; eph.af2 = 0.0;
        int prn = i + 1;
        s.addGps(eph, prn);
        prns_.push_back(prn);
    }
    // Use a real surface location (not the ECEF origin, which is degenerate for
    // geodetic conversion in the NMEA path).
    auto rx = lodestar::scenario::Frames::geodeticToEcef(0.6, 0.0, 0.0);
    s.setReceiver(rx, lodestar::scenario::Vec3(0, 0, 0));
    scenario_ = std::make_shared<Scenario>(std::move(s));
}

HttpResponse ApiServer::health(const HttpRequest&) const {
    Json r = Json::object();
    r["status"] = Json::string("ok");
    r["version"] = Json::number(version_);
    HttpResponse resp;
    resp.body = r.dump();
    return resp;
}

HttpResponse ApiServer::listAdapters(const HttpRequest&) const {
    Json r = Json::object();
    Json list = Json::array();
    for (const std::string& name : registry_.names()) {
        auto a = registry_.getOrNull(name);
        if (!a) continue;
        lodestar::adapters::AdapterStatus st = a->status();
        Json item = Json::object();
        item["name"] = Json::string(name);
        item["state"] = Json::string(lodestar::adapters::AdapterStatus::stateName(st.state));
        item["connected"] = Json::boolean(st.connected());
        if (!st.lastError.empty()) item["error"] = Json::string(st.lastError);
        list.push(item);
    }
    r["adapters"] = list;
    HttpResponse resp;
    resp.body = r.dump();
    return resp;
}

HttpResponse ApiServer::invokeAdapter(const HttpRequest& req) const {
    auto it = req.params.find("name");
    if (it == req.params.end()) {
        HttpResponse r;
        r.status = 400;
        r.body = errorJson(400, "missing adapter name").dump();
        return r;
    }
    const std::string& name = it->second;

    std::shared_ptr<IAdapter> adapter;
    try {
        adapter = registry_.get(name);
    } catch (const AdapterError& e) {
        HttpResponse r;
        r.status = 404;
        r.body = errorJson(404, e.what()).dump();
        return r;
    }

    // Parse {"op":..., "params":...}.
    std::string op;
    Json params = Json::object();
    try {
        Json body = Json::parse(req.body);
        if (!body.has("op")) {
            HttpResponse r;
            r.status = 400;
            r.body = errorJson(400, "missing 'op' in request body").dump();
            return r;
        }
        op = body.at("op").asString();
        if (body.has("params")) params = body.at("params");
    } catch (const JsonError& e) {
        HttpResponse r;
        r.status = 400;
        r.body = errorJson(400, std::string("invalid JSON body: ") + e.what()).dump();
        return r;
    }

    try {
        Json result = adapter->invoke(op, params);
        HttpResponse r;
        r.body = result.dump();
        return r;
    } catch (const AdapterError& e) {
        int code = (e.code() == AdapterError::Code::Unsupported) ? 400 : 500;
        HttpResponse r;
        r.status = code;
        r.body = errorJson(code, e.what()).dump();
        return r;
    } catch (const std::exception& e) {
        HttpResponse r;
        r.status = 500;
        r.body = errorJson(500, e.what()).dump();
        return r;
    }
}

HttpResponse ApiServer::scenarioSatellites(const HttpRequest&) const {
    Json r = Json::object();
    r["count"] = Json::number(static_cast<double>(prns_.size()));
    Json arr = Json::array();
    for (int p : prns_) arr.push(Json::number(p));
    r["prns"] = arr;
    HttpResponse resp;
    resp.body = r.dump();
    return resp;
}

HttpResponse ApiServer::scenarioStep(const HttpRequest&) {
    ++stepCount_;
    GpsTime t{2100, sow_};
    sow_ += 1.0;
    auto epoch = scenario_->step(t);
    if (epoch.failed()) {
        HttpResponse r;
        r.status = 500;
        r.body = errorJson(500, "scenario step failed: " + epoch.error()).dump();
        return r;
    }
    Json out = epochToJson(epoch.value());
    out["epoch"] = Json::number(stepCount_);
    HttpResponse r;
    r.body = out.dump();
    return r;
}

// Keep module_version() linkable for the aggregate library (replaces the old
// stub.cpp registration).
int module_version() { return 1; }

}  // namespace lodestar::api
