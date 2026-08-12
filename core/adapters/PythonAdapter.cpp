// core/adapters/PythonAdapter.cpp
// HTTP/JSON client for the out-of-process Python intelligence layer.

#include "core/adapters/PythonAdapter.h"

#include "core/adapters/HttpClient.h"

namespace lodestar::adapters {

std::string PythonAdapter::name() const { return name_; }

bool PythonAdapter::connect(const AdapterConfig& cfg) {
    cfg_ = cfg;
    if (!cfg_.name.empty()) name_ = cfg_.name;
    if (cfg_.host.empty()) {
        status_.state = AdapterState::Error;
        status_.lastError = "PythonAdapter connect: no host configured";
        return false;
    }
    if (cfg_.port <= 0) cfg_.port = 8080;
    status_.state = AdapterState::Connected;
    status_.lastError.clear();
    status_.detail = "python intelligence endpoint " + cfg_.host + ":" +
                     std::to_string(cfg_.port);
    return true;
}

void PythonAdapter::disconnect() {
    status_.state = AdapterState::Disconnected;
    status_.detail.clear();
}

AdapterStatus PythonAdapter::status() const { return status_; }

Json PythonAdapter::doPost(const std::string& op) {
    if (status_.state != AdapterState::Connected) {
        throw AdapterError(AdapterError::Code::NotConnected,
                           "python adapter not connected");
    }
    std::string path = cfg_.path.empty() ? "/" + op : cfg_.path;
    Json req = Json::object();
    req["op"] = Json::string(op);

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
    try {
        Json parsed = Json::parse(resp.body);
        out["report"] = parsed;
    } catch (...) {
        out["report"] = Json::string(resp.body);
    }
    return out;
}

Json PythonAdapter::invoke(const std::string& op, const Json& params) {
    (void)params;
    if (op == "analyze") return doPost("analyze");
    if (op == "report") return doPost("report");
    throw AdapterError(AdapterError::Code::Unsupported,
                       "python adapter: unsupported op '" + op + "'");
}

}  // namespace lodestar::adapters
