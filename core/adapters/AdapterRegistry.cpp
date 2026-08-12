// core/adapters/AdapterRegistry.cpp
// Implementation of the adapter registry + shared adapter utilities.

#include "core/adapters/AdapterRegistry.h"

#include <algorithm>

namespace lodestar::adapters {

const char* AdapterError::codeName(Code c) {
    switch (c) {
        case Code::Unsupported: return "unsupported";
        case Code::NotConnected: return "not_connected";
        case Code::ConnectFailed: return "connect_failed";
        case Code::Network: return "network";
        case Code::Timeout: return "timeout";
        case Code::Protocol: return "protocol";
        case Code::InvalidParams: return "invalid_params";
        case Code::Internal: return "internal";
    }
    return "unknown";
}

const char* AdapterStatus::stateName(AdapterState s) {
    switch (s) {
        case AdapterState::Disconnected: return "disconnected";
        case AdapterState::Connecting: return "connecting";
        case AdapterState::Connected: return "connected";
        case AdapterState::Error: return "error";
    }
    return "unknown";
}

void AdapterRegistry::registerAdapter(std::shared_ptr<IAdapter> adapter) {
    if (!adapter) return;
    const std::string n = adapter->name();
    for (auto& existing : adapters_) {
        if (existing->name() == n) {
            existing = std::move(adapter);  // replace
            return;
        }
    }
    adapters_.push_back(std::move(adapter));
}

std::shared_ptr<IAdapter> AdapterRegistry::getOrNull(const std::string& name) const {
    for (const auto& a : adapters_) {
        if (a->name() == name) return a;
    }
    return nullptr;
}

std::shared_ptr<IAdapter> AdapterRegistry::get(const std::string& name) const {
    auto a = getOrNull(name);
    if (!a) {
        throw AdapterError(AdapterError::Code::Unsupported,
                           "no adapter registered with name '" + name + "'");
    }
    return a;
}

bool AdapterRegistry::contains(const std::string& name) const {
    return getOrNull(name) != nullptr;
}

std::vector<std::string> AdapterRegistry::names() const {
    std::vector<std::string> out;
    out.reserve(adapters_.size());
    for (const auto& a : adapters_) out.push_back(a->name());
    std::sort(out.begin(), out.end());
    return out;
}

// Keep module_version() linkable for the aggregate library (replaces the old
// stub.cpp registration).
int module_version() { return 1; }

}  // namespace lodestar::adapters
