// core/adapters/LlmClient.h
// Gap-Fill Cross-cutting #4: shared LlmClient abstraction.
//
// A single, shared client every AI feature (RiskAI, TraceLink scoring) uses to
// guarantee three properties:
//   1. local models only — the configured host must be a loopback/local address;
//   2. no data egress — a non-local host is rejected before any request;
//   3. deterministic fallback — a `fallback` callback returns a value when the
//      LLM is unavailable, so features always produce a result.
//
// LlmAdapter remains the underlying transport; LlmClient adds the policy layer
// so no feature can accidentally talk to a remote model server.

#pragma once

#include <functional>
#include <string>

#include "core/adapters/Adapter.h"

namespace lodestar::adapters {

// A shared LLM client enforcing local-only + deterministic fallback.
class LlmClient {
public:
    // fallback is invoked when the LLM is unavailable or the request fails.
    explicit LlmClient(IAdapter& llm, AdapterConfig cfg,
                       std::function<std::string()> fallback);

    // True if the configured host is a local address (loopback, localhost, or
    // an RFC1918/private address) — i.e. the request would not egress the host.
    static bool isLocalHost(const std::string& host);

    // Returns the effective host this client is allowed to reach ("" if not
    // local, meaning the client is disabled for egress).
    const std::string& localHost() const { return effectiveHost_; }

    // Whether the client is usable (a local host was configured).
    bool usable() const { return !effectiveHost_.empty(); }

    // Run a completion. On failure or unavailability returns the fallback value.
    std::string complete(const std::string& prompt);

    // The configured model name.
    std::string model() const { return cfg_.param("model", "qwen2.5:7b"); }

private:
    IAdapter& llm_;
    AdapterConfig cfg_;
    std::string effectiveHost_;
    std::function<std::string()> fallback_;
};

}  // namespace lodestar::adapters
