// core/adapters/LlmAdapter.cpp
// HTTP/JSON client for a local Qwen/Gemma model server.

#include "core/adapters/LlmAdapter.h"

#include <string>

#include "core/adapters/HttpClient.h"

namespace lodestar::adapters {

std::string LlmAdapter::name() const { return name_; }

bool LlmAdapter::connect(const AdapterConfig& cfg) {
    cfg_ = cfg;
    if (!cfg_.name.empty()) name_ = cfg_.name;
    if (cfg_.host.empty()) {
        status_.state = AdapterState::Error;
        status_.lastError = "LlmAdapter connect: no host configured";
        return false;
    }
    if (cfg_.port <= 0) cfg_.port = 11434;  // Ollama default
    status_.state = AdapterState::Connected;
    status_.lastError.clear();
    status_.detail = "llm endpoint " + cfg_.host + ":" + std::to_string(cfg_.port);
    return true;
}

void LlmAdapter::disconnect() {
    status_.state = AdapterState::Disconnected;
    status_.detail.clear();
}

AdapterStatus LlmAdapter::status() const { return status_; }

Json LlmAdapter::doHealth() {
    if (status_.state != AdapterState::Connected) {
        throw AdapterError(AdapterError::Code::NotConnected,
                           "llm adapter not connected");
    }
    std::string path = cfg_.path.empty() ? "/" : cfg_.path;
    std::string err;
    auto resp = HttpClient::request(cfg_.host, cfg_.port, "GET", path, "", "",
                                    cfg_.timeoutMs, &err);
    if (resp.status == 0) {
        status_.state = AdapterState::Error;
        status_.lastError = err;
        throw AdapterError(AdapterError::Code::Network, err);
    }
    Json out = Json::object();
    out["ok"] = Json::boolean(resp.ok());
    out["status"] = Json::number(resp.status);
    out["body"] = Json::string(resp.body);
    return out;
}

Json LlmAdapter::doComplete(const Json& params) {
    if (status_.state != AdapterState::Connected) {
        throw AdapterError(AdapterError::Code::NotConnected,
                           "llm adapter not connected");
    }
    std::string model = "qwen2.5:7b";
    std::string prompt = "Hello";
    if (params.has("model")) model = params.at("model").asString();
    if (params.has("prompt")) prompt = params.at("prompt").asString();

    Json req = Json::object();
    req["model"] = Json::string(model);
    req["prompt"] = Json::string(prompt);
    req["stream"] = Json::boolean(false);

    std::string path = cfg_.path.empty() ? "/api/generate" : cfg_.path;
    std::string err;
    auto resp = HttpClient::request(cfg_.host, cfg_.port, "POST", path,
                                    req.dump(), "application/json",
                                    cfg_.timeoutMs, &err);
    if (resp.status == 0) {
        status_.state = AdapterState::Error;
        status_.lastError = err;
        throw AdapterError(AdapterError::Code::Network, err);
    }

    Json out = Json::object();
    out["ok"] = Json::boolean(resp.ok());
    out["status"] = Json::number(resp.status);
    // Preserve the raw model reply; also try to surface a parsed field.
    try {
        Json parsed = Json::parse(resp.body);
        out["reply"] = parsed;
    } catch (...) {
        out["reply"] = Json::string(resp.body);
    }
    return out;
}

Json LlmAdapter::invoke(const std::string& op, const Json& params) {
    if (op == "complete") return doComplete(params);
    if (op == "health") return doHealth();
    throw AdapterError(AdapterError::Code::Unsupported,
                       "llm adapter: unsupported op '" + op + "'");
}

}  // namespace lodestar::adapters
