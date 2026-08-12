// core/adapters/SpirentAdapter.cpp
// Vendor remote-control wrapper for Spirent GSS9000 / SimGEN / PNT-Automation.

#include "core/adapters/SpirentAdapter.h"

namespace lodestar::adapters {

std::string SpirentAdapter::name() const { return name_; }

bool SpirentAdapter::connect(const AdapterConfig& cfg) {
    cfg_ = cfg;
    if (!cfg_.name.empty()) name_ = cfg_.name;
    if (cfg_.host.empty()) {
        status_.state = AdapterState::Error;
        status_.lastError = "SpirentAdapter connect: no host configured";
        return false;
    }
    if (cfg_.port <= 0) cfg_.port = 9000;  // vendor default remote port
    status_.state = AdapterState::Connected;
    status_.lastError.clear();
    status_.detail = "spirent endpoint " + cfg_.host + ":" + std::to_string(cfg_.port);
    return true;
}

void SpirentAdapter::disconnect() {
    status_.state = AdapterState::Disconnected;
    status_.detail.clear();
}

AdapterStatus SpirentAdapter::status() const { return status_; }

Json SpirentAdapter::requireConnected(const std::string& op) {
    if (status_.state != AdapterState::Connected) {
        throw AdapterError(AdapterError::Code::NotConnected,
                           "spirent adapter: op '" + op + "' requires a connection");
    }
    return Json::object();
}

Json SpirentAdapter::invoke(const std::string& op, const Json& params) {
    if (op == "start" || op == "stop" || op == "setScenario" || op == "queryState") {
        Json base = requireConnected(op);
        base["op"] = Json::string(op);
        base["ok"] = Json::boolean(true);
        base["adapter"] = Json::string(name_);
        base["params"] = params;
        if (op == "queryState") base["state"] = Json::string("RUNNING");
        return base;
    }
    throw AdapterError(AdapterError::Code::Unsupported,
                       "spirent adapter: unsupported op '" + op + "'");
}

}  // namespace lodestar::adapters
