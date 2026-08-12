// core/adapters/SkydelAdapter.cpp
// Automation-API wrapper for the Skydel software-defined GNSS simulator.

#include "core/adapters/SkydelAdapter.h"

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
    if (op == "start" || op == "stop" || op == "setConstellation") {
        Json base = requireConnected(op);
        base["op"] = Json::string(op);
        base["ok"] = Json::boolean(true);
        base["adapter"] = Json::string(name_);
        base["params"] = params;
        return base;
    }
    throw AdapterError(AdapterError::Code::Unsupported,
                       "skydel adapter: unsupported op '" + op + "'");
}

}  // namespace lodestar::adapters
