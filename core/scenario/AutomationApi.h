// core/scenario/AutomationApi.h
// ScenarioForge automation API (S2 Phase 11).
//
// A remote-control interface (SCPI-style command set) to start/stop/configure
// scenario generation. This is the control surface a Python binding, REST
// endpoint, or SCPI client would drive.

#pragma once

#include <map>
#include <string>

#include "core/common/Result.h"

namespace lodestar::scenario {

// SCPI-style remote-control automation API for ScenarioForge.
class AutomationApi {
public:
    AutomationApi() = default;

    // Start a scenario by id; returns a handle token on success.
    common::Result<std::string> startScenario(const std::string& scenarioId);

    // Stop a running scenario by handle.
    common::Result<void> stopScenario(const std::string& handle);

    // Configure a running scenario (SCPI-style key/value).
    common::Result<void> configure(const std::string& handle,
                                   const std::string& key,
                                   const std::string& value);

    // SCPI-style query, e.g. "SYST:STAT?" -> "RUN" / "STOP" / "IDLE".
    common::Result<std::string> query(const std::string& scpi);

    // True if the given handle refers to a running scenario.
    bool isRunning(const std::string& handle) const;

private:
    struct Session {
        std::string scenarioId;
        bool running = false;
        std::map<std::string, std::string> config;
    };

    std::map<std::string, Session> sessions_;
    int nextId_ = 1;
};

}  // namespace lodestar::scenario
