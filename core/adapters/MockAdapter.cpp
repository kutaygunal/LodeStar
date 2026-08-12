// core/adapters/MockAdapter.cpp
// Canned adapter for hardware-free operation and the smoke path.

#include "core/adapters/MockAdapter.h"

namespace lodestar::adapters {

std::string MockAdapter::name() const { return name_; }

bool MockAdapter::connect(const AdapterConfig& cfg) {
    if (!cfg.name.empty()) name_ = cfg.name;
    status_.state = AdapterState::Connected;
    status_.lastError.clear();
    status_.detail = "mock connected (no hardware)";
    return true;
}

void MockAdapter::disconnect() {
    status_.state = AdapterState::Disconnected;
    status_.detail.clear();
}

AdapterStatus MockAdapter::status() const { return status_; }

Json MockAdapter::invoke(const std::string& op, const Json& params) {
    if (op == "ping") {
        Json r = Json::object();
        r["ok"] = Json::boolean(true);
        r["result"] = Json::string("pong");
        r["name"] = Json::string(name_);
        return r;
    }
    if (op == "info") {
        Json r = Json::object();
        r["ok"] = Json::boolean(true);
        r["type"] = Json::string("mock");
        r["echo"] = params;
        return r;
    }
    throw AdapterError(AdapterError::Code::Unsupported,
                       "mock adapter: unsupported op '" + op + "'");
}

}  // namespace lodestar::adapters
