// core/adapters/RsAdapter.cpp
// SCPI remote-control wrapper for Rohde & Schwarz SMW200A.

#include "core/adapters/RsAdapter.h"

namespace lodestar::adapters {

std::string RsAdapter::name() const { return name_; }

bool RsAdapter::connect(const AdapterConfig& cfg) {
    cfg_ = cfg;
    if (!cfg_.name.empty()) name_ = cfg_.name;
    if (cfg_.host.empty()) {
        status_.state = AdapterState::Error;
        status_.lastError = "RsAdapter connect: no host configured";
        return false;
    }
    if (cfg_.port <= 0) cfg_.port = 5025;  // SCPI TCP default
    status_.state = AdapterState::Connected;
    status_.lastError.clear();
    status_.detail = "rs endpoint " + cfg_.host + ":" + std::to_string(cfg_.port);
    return true;
}

void RsAdapter::disconnect() {
    status_.state = AdapterState::Disconnected;
    status_.detail.clear();
}

AdapterStatus RsAdapter::status() const { return status_; }

Json RsAdapter::requireConnected(const std::string& op) {
    if (status_.state != AdapterState::Connected) {
        throw AdapterError(AdapterError::Code::NotConnected,
                           "rs adapter: op '" + op + "' requires a connection");
    }
    return Json::object();
}

Json RsAdapter::invoke(const std::string& op, const Json& params) {
    if (op == "connect" || op == "sendScpi" || op == "setFreq" || op == "setLevel") {
        Json base = requireConnected(op);
        base["op"] = Json::string(op);
        base["ok"] = Json::boolean(true);
        base["adapter"] = Json::string(name_);
        base["params"] = params;
        if (op == "sendScpi") {
            std::string scpi = params.has("cmd") ? params.at("cmd").asString() : "";
            base["scpi"] = Json::string(scpi);
        }
        return base;
    }
    throw AdapterError(AdapterError::Code::Unsupported,
                       "rs adapter: unsupported op '" + op + "'");
}

}  // namespace lodestar::adapters
