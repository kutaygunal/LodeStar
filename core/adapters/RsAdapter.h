// core/adapters/RsAdapter.h
// Adapter to Rohde & Schwarz SMW200A via SCPI remote control (Phase 5, P5-1.6).
// Ops: connect, sendScpi, setFreq, setLevel.

#pragma once

#include "core/adapters/Adapter.h"

namespace lodestar::adapters {

class RsAdapter final : public IAdapter {
public:
    RsAdapter() = default;
    explicit RsAdapter(std::string name) : name_(std::move(name)) {}

    std::string name() const override;
    bool connect(const AdapterConfig& cfg) override;
    void disconnect() override;
    AdapterStatus status() const override;
    Json invoke(const std::string& op, const Json& params) override;

private:
    Json requireConnected(const std::string& op);

    std::string name_ = "rs";
    AdapterConfig cfg_;
    AdapterStatus status_;
};

}  // namespace lodestar::adapters
