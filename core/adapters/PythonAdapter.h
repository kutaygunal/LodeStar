// core/adapters/PythonAdapter.h
// Adapter to the Python intelligence layer over HTTP/JSON (Phase 5, P5-1.4).
// Ops: analyze (POST an analysis request -> JSON report), report (POST a report
// request).

#pragma once

#include "core/adapters/Adapter.h"

namespace lodestar::adapters {

class PythonAdapter final : public IAdapter {
public:
    PythonAdapter() = default;
    explicit PythonAdapter(std::string name) : name_(std::move(name)) {}

    std::string name() const override;
    bool connect(const AdapterConfig& cfg) override;
    void disconnect() override;
    AdapterStatus status() const override;
    Json invoke(const std::string& op, const Json& params) override;

private:
    Json doPost(const std::string& op);

    std::string name_ = "python";
    AdapterConfig cfg_;
    AdapterStatus status_;
};

}  // namespace lodestar::adapters
