// core/adapters/SkydelAdapter.h
// Adapter to the Skydel software-defined GNSS simulator automation API (Phase 5,
// P5-1.7). Ops: start, stop, setConstellation.

#pragma once

#include "core/adapters/Adapter.h"

namespace lodestar::adapters {

class SkydelAdapter final : public IAdapter {
public:
    SkydelAdapter() = default;
    explicit SkydelAdapter(std::string name) : name_(std::move(name)) {}

    std::string name() const override;
    bool connect(const AdapterConfig& cfg) override;
    void disconnect() override;
    AdapterStatus status() const override;
    Json invoke(const std::string& op, const Json& params) override;

private:
    Json requireConnected(const std::string& op);

    std::string name_ = "skydel";
    AdapterConfig cfg_;
    AdapterStatus status_;
};

}  // namespace lodestar::adapters
