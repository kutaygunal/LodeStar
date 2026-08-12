// core/adapters/SkydelAdapter.cpp
// Automation-API wrapper for the Skydel software-defined GNSS simulator.
//
// S1 Phase 2: invoke() now performs a REAL HTTP POST to the configured Skydel
// automation endpoint (default 127.0.0.1:8081) via HttpClient and returns the
// vendor response as Json. A `simulate` config param ("1") returns a realistic
// vendor-shaped response without hardware so the end-to-end RF-injection path
// can be exercised in CI. On transport failure invoke() throws
// AdapterError(Network|Timeout) and sets status_.state = Error.

#include "core/adapters/SkydelAdapter.h"

#include "core/adapters/HttpClient.h"

namespace lodestar::adapters {

std::string SkydelAdapter::name() const { return name_; }

bool SkydelAdapter::connect(const AdapterConfig& cfg) {
    cfg_ = cfg;
    if (!cfg_.name.empty()) name_ = cfg_.name;
    if (cfg_.host.empty()) {
        status_.state = AdapterState::Error;
        status_.lastError = "SkydelAdapter connect: no host configured";
        return false;
    }
    if (cfg_.port <= 0) cfg_.port = 8081;  // Skydel automation HTTP default
    status_.state = AdapterState::Connected;
    status_.lastError.clear();
    status_.detail = "skydel endpoint " + cfg_.host + ":" + std::to_string(cfg_.port);
    return true;
}

void SkydelAdapter::disconnect() {
    status_.state = AdapterState::Disconnected;
    status_.detail.clear();
}

AdapterStatus SkydelAdapter::status() const { return status_; }

Json SkydelAdapter::requireConnected(const std::string& op) {
    if (status_.state != AdapterState::Connected) {
        throw AdapterError(AdapterError::Code::NotConnected,
                           "skydel adapter: op '" + op + "' requires a connection");
    }
    return Json::object();
}

Json SkydelAdapter::invoke(const std::string& op, const Json& params) {
    if (op != "start" && op != "stop" && op != "setConstellation" &&
        op != "measure") {
        throw AdapterError(AdapterError::Code::Unsupported,
                           "skydel adapter: unsupported op '" + op + "'");
    }
    requireConnected(op);

    // Simulated mode: return a realistic vendor-shaped response without hardware.
    if (cfg_.param("simulate") == "1") {
        Json out = Json::object();
        out["ok"] = Json::boolean(true);
        out["status"] = Json::number(200);
        out["adapter"] = Json::string(name_);
        out["op"] = Json::string(op);
        out["params"] = params;
        Json vendor = Json::object();
        vendor["command"] = Json::string(op);
        vendor["result"] = Json::string("ok");
        vendor["message"] = Json::string("simulated");
        if (op == "setConstellation" && params.has("constellation")) {
            vendor["constellation"] = params.at("constellation");
        }
        if (op == "measure") {
            // Deterministic simulated measurement (metres). A caller may
            // override the value via params["value"]; otherwise a fixed
            // plausible value is reported so the RF-injection path can be
            // exercised end-to-end in CI without hardware.
            double value = 1.5;
            if (params.has("value") && params.at("value").isNumber()) {
                value = params.at("value").asNumber();
            }
            vendor["value"] = Json::number(value);
            vendor["unit"] = Json::string("m");
        }
        out["vendor"] = vendor;
        return out;
    }

    // Real mode: POST to the Skydel automation endpoint.
    std::string path = cfg_.path.empty() ? "/api/commands" : cfg_.path;
    Json req = Json::object();
    req["command"] = Json::string(op);
    req["params"] = params;

    std::string err;
    auto resp = HttpClient::request(cfg_.host, cfg_.port, "POST", path,
                                    req.dump(), "application/json",
                                    cfg_.timeoutMs, &err);
    if (resp.status == 0) {
        status_.state = AdapterState::Error;
        status_.lastError = err;
        throw AdapterError(AdapterError::Code::Network, err);
    }

    Json out = Json::object();
    out["ok"] = Json::boolean(resp.ok());
    out["status"] = Json::number(resp.status);
    out["adapter"] = Json::string(name_);
    out["op"] = Json::string(op);
    out["params"] = params;
    try {
        out["vendor"] = Json::parse(resp.body);
    } catch (...) {
        out["vendor"] = Json::string(resp.body);
    }
    return out;
}

}  // namespace lodestar::adapters
