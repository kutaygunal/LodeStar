// core/adapters/LlmAdapter.h
// Adapter to a local model server (Qwen/Gemma) over HTTP/JSON (Phase 5, P5-1.3).
// Ops: complete (POST a completion request), health (GET the model server).

#pragma once

#include "core/adapters/Adapter.h"

namespace lodestar::adapters {

class LlmAdapter final : public IAdapter {
public:
    LlmAdapter() = default;
    explicit LlmAdapter(std::string name) : name_(std::move(name)) {}

    std::string name() const override;
    bool connect(const AdapterConfig& cfg) override;
    void disconnect() override;
    AdapterStatus status() const override;
    Json invoke(const std::string& op, const Json& params) override;

private:
    Json doComplete(const Json& params);
    Json doHealth();

    std::string name_ = "llm";
    AdapterConfig cfg_;
    AdapterStatus status_;
};

}  // namespace lodestar::adapters
