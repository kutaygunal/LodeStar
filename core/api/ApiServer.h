// core/api/ApiServer.h
// Wires the adapter registry + a built Scenario to the HTTP server endpoints
// (Phase 5, P5-2.2).
//
// Endpoints:
//   GET  /health                       -> {"status":"ok","version":N}
//   GET  /adapters                     -> list registered adapters + status
//   POST /adapters/<name>/invoke       -> {"op":...} -> adapter result JSON
//   GET  /scenario/satellites          -> count/PRNs of the built scenario
//   POST /scenario/step                -> step one epoch (PVT + PL + NMEA)
//
// Error model: 200 ok, 400 bad request, 404 unknown adapter/route, 500 internal,
// with body {"error":{"code":...,"message":...}}.

#pragma once

#include <memory>
#include <vector>

#include "core/adapters/AdapterRegistry.h"
#include "core/api/HttpServer.h"
#include "core/persistence/Database.h"
#include "core/scenario/Scenario.h"
#include "core/tracelink/UserService.h"

namespace lodestar::api {

class ApiServer {
public:
    // When `db` is non-null, the /auth and /users REST surface is registered.
    ApiServer(lodestar::adapters::AdapterRegistry& registry, int version = 1,
              lodestar::persistence::Database* db = nullptr);

    // Register the routes on the given server.
    void setup(HttpServer& server);

private:
    // Endpoint handlers.
    HttpResponse health(const HttpRequest& req) const;
    HttpResponse listAdapters(const HttpRequest& req) const;
    HttpResponse invokeAdapter(const HttpRequest& req) const;
    HttpResponse scenarioSatellites(const HttpRequest& req) const;
    HttpResponse scenarioStep(const HttpRequest& req);

    // S2 Phase 1 auth + user endpoints.
    HttpResponse authRegister(const HttpRequest& req);
    HttpResponse authLogin(const HttpRequest& req);
    HttpResponse authLogout(const HttpRequest& req);
    HttpResponse authMe(const HttpRequest& req);
    HttpResponse listUsers(const HttpRequest& req);
    HttpResponse changeUserRole(const HttpRequest& req);
    HttpResponse updateEntity(const HttpRequest& req);

    // Build a default 6-satellite GPS scenario and remember its PRNs.
    void buildDefaultScenario();

    lodestar::adapters::AdapterRegistry& registry_;
    int version_;
    std::shared_ptr<lodestar::scenario::Scenario> scenario_;
    std::vector<int> prns_;
    double sow_ = 0.0;
    int stepCount_ = 0;

    // S2 Phase 1: optional user/auth surface (null when no DB is provided).
    lodestar::persistence::Database* db_ = nullptr;
    std::unique_ptr<lodestar::tracelink::UserService> users_;
};

}  // namespace lodestar::api
