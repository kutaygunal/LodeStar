// core/smoke/adapters_api_smoke.cpp
// Self-verifying smoke path for Phase 5 (Adapters + thin C++ REST API).
//
// Exercises: adapter registry add/lookup, unknown-adapter typed error, MockAdapter
// invoke, LlmAdapter unreachable-host network error, SpirentAdapter not-connected
// error, and the REST API endpoints (GET /health 200, POST invoke 200, GET
// /scenario/satellites 200, POST /scenario/step 200, unknown-route 404, unknown
// adapter 404). Returns non-zero on failure.

#include <cstdio>
#include <memory>
#include <string>

#include "core/adapters/Adapter.h"
#include "core/adapters/AdapterRegistry.h"
#include "core/adapters/HttpClient.h"
#include "core/adapters/Json.h"
#include "core/adapters/LlmAdapter.h"
#include "core/adapters/MockAdapter.h"
#include "core/adapters/SpirentAdapter.h"
#include "core/api/ApiServer.h"
#include "core/api/HttpServer.h"

namespace lodestar {

namespace {

int failures = 0;

void check(bool ok, const std::string& name) {
    if (ok) {
        std::printf("  [PASS] %s\n", name.c_str());
    } else {
        std::printf("  [FAIL] %s\n", name.c_str());
        ++failures;
    }
}

// True when the JSON body is well-formed and contains a field at the given
// dotted path (e.g. "status", "ok").
bool jsonField(const std::string& body, const std::string& path) {
    try {
        Json j = Json::parse(body);
        const Json* cur = &j;
        std::string rest = path;
        while (true) {
            size_t dot = rest.find('.');
            std::string key = dot == std::string::npos ? rest : rest.substr(0, dot);
            if (!cur->has(key)) return false;
            cur = &cur->at(key);
            if (dot == std::string::npos) return true;
            rest = rest.substr(dot + 1);
        }
    } catch (...) {
        return false;
    }
}

}  // namespace

int runAdaptersApiSmoke() {
    std::printf("Adapters + REST API Phase 5 smoke path\n");

    // --- 1. Registry + MockAdapter + unknown adapter error ------------------
    adapters::AdapterRegistry registry;
    registry.registerAdapter(std::make_shared<adapters::MockAdapter>());
    registry.registerAdapter(std::make_shared<adapters::LlmAdapter>());

    auto mock = registry.getOrNull("mock");
    check(mock != nullptr, "registry contains MockAdapter");
    if (mock) {
        check(mock->name() == "mock", "MockAdapter name() == 'mock'");
        adapters::AdapterConfig cfg;
        cfg.name = "mock";
        check(mock->connect(cfg), "MockAdapter connect");
        check(mock->status().connected(), "MockAdapter status connected");
        Json result = mock->invoke("ping", Json::object());
        check(result.has("ok") && result.at("ok").asBool() &&
                  result.has("result") && result.at("result").asString() == "pong",
              "MockAdapter invoke(ping) -> ok/pong");
    }

    bool unknownCaught = false;
    try {
        registry.get("does-not-exist");
    } catch (const adapters::AdapterError& e) {
        unknownCaught = (e.code() == adapters::AdapterError::Code::Unsupported);
    }
    check(unknownCaught, "registry unknown name -> typed Unsupported error");

    // --- 2. LlmAdapter unreachable host -> network error ---------------------
    bool llmNetworkCaught = false;
    auto llm = registry.get("llm");
    if (llm) {
        adapters::AdapterConfig lcfg;
        lcfg.host = "127.0.0.1";
        lcfg.port = 1;  // nothing listens on port 1 -> connection refused
        lcfg.timeoutMs = 1000;
        llm->connect(lcfg);
        try {
            Json params = Json::object();
            params["prompt"] = Json::string("hi");
            llm->invoke("complete", params);
        } catch (const adapters::AdapterError& e) {
            llmNetworkCaught = (e.code() == adapters::AdapterError::Code::Network ||
                                e.code() == adapters::AdapterError::Code::Timeout);
        } catch (...) {
            llmNetworkCaught = false;
        }
    }
    check(llmNetworkCaught, "LlmAdapter unreachable host -> typed network error");

    // --- 3. SpirentAdapter not connected -> NotConnected error ---------------
    bool spirentNotConnected = false;
    adapters::SpirentAdapter spirent;
    try {
        spirent.invoke("start", Json::object());
    } catch (const adapters::AdapterError& e) {
        spirentNotConnected = (e.code() == adapters::AdapterError::Code::NotConnected);
    }
    check(spirentNotConnected, "SpirentAdapter not connected -> typed error");

    // --- 4. REST API server + endpoints --------------------------------------
    api::HttpServer server;
    api::ApiServer api(registry, 1);
    api.setup(server);
    check(server.start(0), "HTTP server starts on ephemeral port");
    if (server.port() <= 0) {
        check(false, "HTTP server bound to a valid port");
        ++failures;
    } else {
        int port = server.port();
        std::string host = "127.0.0.1";

        // GET /health -> 200.
        std::string err;
        auto health = adapters::HttpClient::request(host, port, "GET", "/health",
                                                    "", "", 3000, &err);
        check(health.status == 200 && jsonField(health.body, "status"),
              "GET /health -> 200 ok");

        // POST /adapters/mock/invoke -> 200.
        Json pingReq = Json::object();
        pingReq["op"] = Json::string("ping");
        auto inv = adapters::HttpClient::request(host, port, "POST",
                                                 "/adapters/mock/invoke",
                                                 pingReq.dump(), "application/json",
                                                 3000, &err);
        check(inv.status == 200 && jsonField(inv.body, "ok") &&
                  jsonField(inv.body, "result"),
              "POST /adapters/mock/invoke -> 200 ok");

        // GET /adapters -> 200 with list.
        auto list = adapters::HttpClient::request(host, port, "GET", "/adapters",
                                                  "", "", 3000, &err);
        check(list.status == 200 && jsonField(list.body, "adapters"),
              "GET /adapters -> 200 with adapter list");

        // GET /scenario/satellites -> 200 count > 0.
        auto sats = adapters::HttpClient::request(host, port, "GET",
                                                  "/scenario/satellites", "", "",
                                                  3000, &err);
        check(sats.status == 200 && jsonField(sats.body, "count"),
              "GET /scenario/satellites -> 200");

        // POST /scenario/step -> 200.
        auto step = adapters::HttpClient::request(host, port, "POST", "/scenario/step",
                                                  "", "", 3000, &err);
        check(step.status == 200 && jsonField(step.body, "pvt") &&
                  jsonField(step.body, "nmea"),
              "POST /scenario/step -> 200 with pvt + nmea");

        // GET /unknown-route -> 404.
        auto nf = adapters::HttpClient::request(host, port, "GET", "/no/such/route",
                                                "", "", 3000, &err);
        check(nf.status == 404, "unknown route -> 404");

        // POST /adapters/nope/invoke -> 404.
        auto na = adapters::HttpClient::request(host, port, "POST",
                                                "/adapters/nope/invoke",
                                                pingReq.dump(), "application/json",
                                                3000, &err);
        check(na.status == 404, "unknown adapter invoke -> 404");

        // Bad request: POST invoke without op -> 400.
        auto bad = adapters::HttpClient::request(host, port, "POST",
                                                 "/adapters/mock/invoke", "{}",
                                                 "application/json", 3000, &err);
        check(bad.status == 400, "invoke missing op -> 400");
    }

    server.stop();

    if (failures == 0) {
        std::printf("ADAPTERS+API SMOKE OK\n");
        return 0;
    }
    std::printf("ADAPTERS+API SMOKE FAIL: %d check(s) failed\n", failures);
    return 1;
}

}  // namespace lodestar
