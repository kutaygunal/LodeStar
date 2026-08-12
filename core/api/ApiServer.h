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
#include "core/scenario/Scenario.h"

namespace lodestar::api {

class ApiServer {
public:
    ApiServer(lodestar::adapters::AdapterRegistry& registry, int version = 1);

    // Register the routes on the given server.
    void setup(HttpServer& server);

private:
    // Endpoint handlers.
    HttpResponse health(const HttpRequest& req) const;
    HttpResponse listAdapters(const HttpRequest& req) const;
    HttpResponse invokeAdapter(const HttpRequest& req) const;
    HttpResponse scenarioSatellites(const HttpRequest& req) const;
    HttpResponse scenarioStep(const HttpRequest& req);

    // Build a default 6-satellite GPS scenario and remember its PRNs.
    void buildDefaultScenario();

    lodestar::adapters::AdapterRegistry& registry_;
    int version_;
    std::shared_ptr<lodestar::scenario::Scenario> scenario_;
    std::vector<int> prns_;
    double sow_ = 0.0;
    int stepCount_ = 0;
};

}  // namespace lodestar::api
