// core/adapters/Adapter.h
// Adapter pattern core (Phase 5, P5-1.1).
//
// Every external dependency (RF generators, local LLMs, Python intelligence) is
// wrapped behind the single IAdapter interface so the core treats them uniformly
// and can be exercised with a MockAdapter when no hardware is present.
//
// JSON decision: the core uses the self-contained lodestar::Json value type
// (core/adapters/Json.h) rather than nlohmann/json. This keeps the adapter and
// API layer header-only/dependency-free and satisfies the plan's requirement to
// implement a small JSON utility when nlohmann cannot be vendored. Documented here
// so the trade-off is explicit.

#pragma once

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/adapters/Json.h"

namespace lodestar::adapters {

// Typed adapter error. All hard failures (unknown op, not connected, network,
// timeout, protocol, connect failure) are reported through this type so callers
// can distinguish typed failures from ordinary results.
class AdapterError : public std::runtime_error {
public:
    enum class Code {
        Unsupported,      // unknown op or unknown adapter name
        NotConnected,     // operation requires a live connection
        ConnectFailed,    // connect() could not establish the session
        Network,          // network-level failure (unreachable host, socket)
        Timeout,          // request timed out
        Protocol,         // malformed/vendor-protocol response
        InvalidParams,    // bad request parameters
        Internal          // unexpected internal failure
    };

    AdapterError(Code code, const std::string& message)
        : std::runtime_error(message), code_(code) {}

    Code code() const { return code_; }
    static const char* codeName(Code c);

private:
    Code code_;
};

// Connection/endpoint configuration shared by all adapters.
class AdapterConfig {
public:
    std::string name;      // logical name of the adapter instance
    std::string host;      // host / endpoint address
    int port = 0;          // port (0 => default for the adapter type)
    std::string auth;      // optional bearer token / API key / user:pass
    std::string path;      // base path on the endpoint (for HTTP adapters)
    int timeoutMs = 5000;  // network timeout
    std::map<std::string, std::string> params;  // vendor-specific knobs

    AdapterConfig() = default;

    // Convenience: build a config from host/port/auth.
    AdapterConfig(std::string host_, int port_, std::string auth_ = "",
                  std::string path_ = "")
        : host(std::move(host_)),
          port(port_),
          auth(std::move(auth_)),
          path(std::move(path_)) {}

    std::string param(const std::string& key,
                      const std::string& def = "") const {
        auto it = params.find(key);
        return it == params.end() ? def : it->second;
    }
};

enum class AdapterState { Disconnected, Connecting, Connected, Error };

// Immutable snapshot of an adapter's runtime state.
struct AdapterStatus {
    AdapterState state = AdapterState::Disconnected;
    std::string lastError;
    std::string detail;

    bool connected() const { return state == AdapterState::Connected; }
    bool failed() const { return state == AdapterState::Error; }

    static const char* stateName(AdapterState s);
};

// Common interface implemented by every adapter.
class IAdapter {
public:
    virtual ~IAdapter() = default;

    virtual std::string name() const = 0;

    // Establish the out-of-process session. Returns true on success; on failure
    // the adapter stores an Error status with a message.
    virtual bool connect(const AdapterConfig& cfg) = 0;

    // Tear down the session and return to Disconnected.
    virtual void disconnect() = 0;

    virtual AdapterStatus status() const = 0;

    // Run a named operation. On success returns a Json document. On hard failure
    // throws AdapterError so the caller can classify the error.
    virtual Json invoke(const std::string& op, const Json& params) = 0;
};

}  // namespace lodestar::adapters
