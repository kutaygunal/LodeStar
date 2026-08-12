// core/adapters/SpirentAdapter.h
// Adapter to Spirent GSS9000 / SimGEN / PNT-Automation remote control (Phase 5,
// P5-1.5). Ops: start, stop, setScenario, queryState.

#pragma once

#include "core/adapters/Adapter.h"

namespace lodestar::adapters {

class SpirentAdapter final : public IAdapter {
public:
    SpirentAdapter() = default;
    explicit SpirentAdapter(std::string name) : name_(std::move(name)) {}

    std::string name() const override;
    bool connect(const AdapterConfig& cfg) override;
    void disconnect() override;
    AdapterStatus status() const override;
    Json invoke(const std::string& op, const Json& params) override;

private:
    Json requireConnected(const std::string& op);

    std::string name_ = "spirent";
    AdapterConfig cfg_;
    AdapterStatus status_;
};

}  // namespace lodestar::adapters
