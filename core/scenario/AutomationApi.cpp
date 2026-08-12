// core/scenario/AutomationApi.cpp
// ScenarioForge automation API implementation (S2 Phase 11).

#include "core/scenario/AutomationApi.h"

namespace lodestar::scenario {

common::Result<std::string> AutomationApi::startScenario(
    const std::string& scenarioId) {
    if (scenarioId.empty()) {
        return common::Result<std::string>::err(
            common::ErrorCode::InvalidArgument,
            "startScenario: empty scenario id");
    }
    const std::string handle = "scn-" + std::to_string(nextId_++);
    sessions_[handle] = Session{scenarioId, true, {}};
    return common::Result<std::string>::ok(handle);
}

common::Result<void> AutomationApi::stopScenario(const std::string& handle) {
    auto it = sessions_.find(handle);
    if (it == sessions_.end()) {
        return common::Result<void>::err(
            common::ErrorCode::NotFound, "stopScenario: unknown handle");
    }
    it->second.running = false;
    return common::Result<void>::ok();
}

common::Result<void> AutomationApi::configure(const std::string& handle,
                                              const std::string& key,
                                              const std::string& value) {
    auto it = sessions_.find(handle);
    if (it == sessions_.end()) {
        return common::Result<void>::err(
            common::ErrorCode::NotFound, "configure: unknown handle");
    }
    if (key.empty()) {
        return common::Result<void>::err(
            common::ErrorCode::InvalidArgument, "configure: empty key");
    }
    it->second.config[key] = value;
    return common::Result<void>::ok();
}

common::Result<std::string> AutomationApi::query(const std::string& scpi) {
    if (scpi == "SYST:STAT?") {
        if (sessions_.empty()) {
            return common::Result<std::string>::ok("IDLE");
        }
        // Report the status of the most recently started session.
        auto it = sessions_.rbegin();
        return common::Result<std::string>::ok(
            it->second.running ? "RUN" : "STOP");
    }
    return common::Result<std::string>::err(
        common::ErrorCode::InvalidArgument,
        "query: unsupported command: " + scpi);
}

bool AutomationApi::isRunning(const std::string& handle) const {
    auto it = sessions_.find(handle);
    return it != sessions_.end() && it->second.running;
}

}  // namespace lodestar::scenario
