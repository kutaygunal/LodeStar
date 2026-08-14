// core/persistence/ServerPersistence.h
// Gap-Fill TraceLink 3.1: client-server persistence path.
//
// A server-backed adapter for the TraceLink requirement store that talks to the
// REST API over HTTP (instead of SQLite directly). SQLite stays the single-user
// default; this adapter is the alternative storage backend for the web/multi-user
// deployment mode.
//
// The interface mirrors the subset of TraceLink requirement operations needed to
// run the same test suite against both backends (parameterized): create, get,
// list, update, and delete-by-external-id.

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/adapters/HttpClient.h"
#include "core/common/Result.h"

namespace lodestar::persistence {

// One requirement as seen through the server API.
struct ServerRequirement {
    std::string id;           // internal UUID (server-assigned)
    std::string externalId;   // human id, e.g. REQ-100
    std::string name;
    std::string text;
    std::string status;
};

// A client-side persistence adapter backed by the Lodestar REST API. Implements
// the requirement CRUD path over HTTP so a web deployment can store TraceLink
// data in a remote server.
class ServerRequirementStore {
public:
    // host/port of the Lodestar API server; apiKey optional (X-API-Key header).
    ServerRequirementStore(const std::string& host, int port,
                           const std::string& apiKey = "",
                           int timeoutMs = 4000);

    common::Result<ServerRequirement> create(const ServerRequirement& req);
    common::Result<std::optional<ServerRequirement>> findById(const std::string& id);
    common::Result<std::vector<ServerRequirement>> listAll();
    common::Result<ServerRequirement> update(const ServerRequirement& req);
    common::Result<void> remove(const std::string& id);

private:
    lodestar::adapters::HttpClient::Response request(
        const std::string& method, const std::string& path,
        const std::string& body) const;

    std::string host_;
    int port_ = 0;
    std::string apiKey_;
    int timeoutMs_ = 4000;
};

}  // namespace lodestar::persistence
