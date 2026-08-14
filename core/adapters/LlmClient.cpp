// core/adapters/LlmClient.cpp
// Gap-Fill Cross-cutting #4: shared LlmClient abstraction.

#include "core/adapters/LlmClient.h"

#include "core/adapters/Json.h"

namespace lodestar::adapters {

namespace {

bool isLoopback(const std::string& host) {
    if (host == "localhost") return true;
    if (host == "127.0.0.1" || host == "::1") return true;
    if (host.rfind("127.", 0) == 0) return true;  // 127.0.0.0/8
    return false;
}

// RFC1918 private ranges + link-local, so a LAN model box is still "local".
bool isPrivateOrLinkLocal(const std::string& host) {
    if (host.rfind("10.", 0) == 0) return true;
    if (host.rfind("192.168.", 0) == 0) return true;
    if (host.rfind("172.", 0) == 0) {
        // 172.16.0.0/12
        std::string rest = host.substr(4);
        try {
            int b = std::stoi(rest.substr(0, rest.find('.')));
            return b >= 16 && b <= 31;
        } catch (...) { return false; }
    }
    if (host.rfind("169.254.", 0) == 0) return true;
    return false;
}

}  // namespace

LlmClient::LlmClient(IAdapter& llm, AdapterConfig cfg,
                     std::function<std::string()> fallback)
    : llm_(llm), cfg_(std::move(cfg)), fallback_(std::move(fallback)) {
    if (isLocalHost(cfg_.host)) effectiveHost_ = cfg_.host;
}

bool LlmClient::isLocalHost(const std::string& host) {
    if (host.empty()) return false;
    return isLoopback(host) || isPrivateOrLinkLocal(host);
}

std::string LlmClient::complete(const std::string& prompt) {
    if (effectiveHost_.empty()) {
        // Non-local host (would egress) -> never send; use the fallback.
        return fallback_ ? fallback_() : "";
    }
    try {
        lodestar::Json params = lodestar::Json::object();
        params["model"] = lodestar::Json::string(model());
        params["prompt"] = lodestar::Json::string(prompt);
        lodestar::Json reply = llm_.invoke("complete", params);
        if (reply.isObject() && reply.has("reply")) {
            const lodestar::Json& r = reply.at("reply");
            if (r.isString()) return r.asString();
        }
        return fallback_ ? fallback_() : "";
    } catch (const adapters::AdapterError&) {
        return fallback_ ? fallback_() : "";
    } catch (const std::exception&) {
        return fallback_ ? fallback_() : "";
    }
}

}  // namespace lodestar::adapters
