// core/persistence/ServerPersistence.cpp
// Gap-Fill TraceLink 3.1: client-server persistence path.

#include "core/persistence/ServerPersistence.h"

#include "core/adapters/Json.h"

namespace lodestar::persistence {

ServerRequirementStore::ServerRequirementStore(const std::string& host,
                                               int port,
                                               const std::string& apiKey,
                                               int timeoutMs)
    : host_(host), port_(port), apiKey_(apiKey), timeoutMs_(timeoutMs) {}

lodestar::adapters::HttpClient::Response ServerRequirementStore::request(
    const std::string& method, const std::string& path,
    const std::string& body) const {
    std::string err;
    std::string extraHeaders;
    if (!apiKey_.empty()) {
        extraHeaders = "X-API-Key: " + apiKey_;
    }
    return lodestar::adapters::HttpClient::request(
        host_, port_, method, path, body, "application/json", timeoutMs_, &err,
        extraHeaders);
}

namespace {
std::string field(const lodestar::Json& obj, const char* key) {
    if (obj.has(key) && obj.at(key).isString()) return obj.at(key).asString();
    return "";
}
}  // namespace

common::Result<ServerRequirement> ServerRequirementStore::create(
    const ServerRequirement& req) {
    lodestar::Json body = lodestar::Json::object();
    body["name"] = lodestar::Json::string(req.name);
    body["text"] = lodestar::Json::string(req.text);
    body["status"] = lodestar::Json::string(req.status);
    body["type"] = lodestar::Json::string("requirement");
    if (!req.externalId.empty())
        body["external_id"] = lodestar::Json::string(req.externalId);

    auto res = request("POST", "/tracelink/entities", body.dump());
    if (!res.ok()) {
        return common::Result<ServerRequirement>::err(
            common::ErrorCode::Internal,
            "create failed: HTTP " + std::to_string(res.status) + " " + res.reason);
    }
    lodestar::Json created;
    try {
        created = lodestar::Json::parse(res.body);
    } catch (...) {
        return common::Result<ServerRequirement>::err(
            common::ErrorCode::Internal, "create: unparseable response");
    }
    ServerRequirement out;
    out.id = field(created, "id");
    out.externalId = field(created, "external_id");
    out.name = field(created, "name");
    out.text = field(created, "text");
    out.status = field(created, "status");
    if (out.id.empty()) {
        return common::Result<ServerRequirement>::err(
            common::ErrorCode::Internal, "create: response missing id");
    }
    return common::Result<ServerRequirement>::ok(std::move(out));
}

common::Result<std::optional<ServerRequirement>>
ServerRequirementStore::findById(const std::string& id) {
    auto res = request("GET", "/tracelink/entities/requirement/" + id, "");
    if (res.status == 404) {
        return common::Result<std::optional<ServerRequirement>>::ok(
            std::nullopt);
    }
    if (!res.ok()) {
        return common::Result<std::optional<ServerRequirement>>::err(
            common::ErrorCode::Internal,
            "findById failed: HTTP " + std::to_string(res.status));
    }
    lodestar::Json got;
    try {
        got = lodestar::Json::parse(res.body);
    } catch (...) {
        return common::Result<std::optional<ServerRequirement>>::err(
            common::ErrorCode::Internal, "findById: unparseable response");
    }
    ServerRequirement out;
    out.id = field(got, "id");
    out.externalId = field(got, "external_id");
    out.name = field(got, "name");
    out.text = field(got, "text");
    out.status = field(got, "status");
    return common::Result<std::optional<ServerRequirement>>::ok(
        std::move(out));
}

common::Result<std::vector<ServerRequirement>>
ServerRequirementStore::listAll() {
    auto res = request("GET", "/tracelink/entities?type=requirement", "");
    if (!res.ok()) {
        return common::Result<std::vector<ServerRequirement>>::err(
            common::ErrorCode::Internal,
            "list failed: HTTP " + std::to_string(res.status));
    }
    lodestar::Json got;
    try {
        got = lodestar::Json::parse(res.body);
    } catch (...) {
        return common::Result<std::vector<ServerRequirement>>::err(
            common::ErrorCode::Internal, "list: unparseable response");
    }
    std::vector<ServerRequirement> out;
    if (got.isObject() && got.has("entities") && got.at("entities").isArray()) {
        for (const auto& item : got.at("entities").asArray()) {
            if (!item.isObject()) continue;
            ServerRequirement r;
            r.id = field(item, "id");
            r.externalId = field(item, "external_id");
            r.name = field(item, "name");
            r.text = field(item, "text");
            r.status = field(item, "status");
            if (r.id.empty() && r.name.empty()) continue;
            out.push_back(std::move(r));
        }
    }
    return common::Result<std::vector<ServerRequirement>>::ok(std::move(out));
}

common::Result<ServerRequirement> ServerRequirementStore::update(
    const ServerRequirement& req) {
    if (req.id.empty()) {
        return common::Result<ServerRequirement>::err(
            common::ErrorCode::InvalidArgument, "update requires an id");
    }
    lodestar::Json body = lodestar::Json::object();
    body["name"] = lodestar::Json::string(req.name);
    body["text"] = lodestar::Json::string(req.text);
    body["status"] = lodestar::Json::string(req.status);
    auto res = request("PUT", "/tracelink/entities/requirement/" + req.id,
                       body.dump());
    if (!res.ok()) {
        return common::Result<ServerRequirement>::err(
            common::ErrorCode::Internal,
            "update failed: HTTP " + std::to_string(res.status));
    }
    auto refreshed = findById(req.id);
    if (refreshed.failed()) {
        return common::Result<ServerRequirement>::err(refreshed.error());
    }
    return common::Result<ServerRequirement>::ok(
        refreshed.value().value_or(req));
}

common::Result<void> ServerRequirementStore::remove(const std::string& id) {
    auto res = request("DELETE", "/tracelink/entities/requirement/" + id, "");
    if (!res.ok()) {
        return common::Result<void>::err(
            common::ErrorCode::Internal,
            "remove failed: HTTP " + std::to_string(res.status));
    }
    return common::Result<void>::ok();
}

}  // namespace lodestar::persistence
