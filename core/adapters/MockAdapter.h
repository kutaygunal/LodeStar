// core/adapters/MockAdapter.h
// Adapter that requires no hardware or network. Returns canned success/values so
// the core and the smoke path run without any external dependency (Phase 5,
// P5-1.2).

#pragma once

#include "core/adapters/Adapter.h"

namespace lodestar::adapters {

class MockAdapter final : public IAdapter {
public:
    MockAdapter() = default;
    explicit MockAdapter(std::string name) : name_(std::move(name)) {}

    std::string name() const override;
    bool connect(const AdapterConfig& cfg) override;
    void disconnect() override;
    AdapterStatus status() const override;
    Json invoke(const std::string& op, const Json& params) override;

private:
    std::string name_ = "mock";
    AdapterStatus status_;
};

}  // namespace lodestar::adapters
