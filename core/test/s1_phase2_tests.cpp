// core/test/s1_phase2_tests.cpp
// ---------------------------------------------------------------------------
// S1 Phase 2: functional adapters (real Skydel + LLM invoke) tests.
//
// Test contract: docs/s1-phase2-test.md (written by the scrum-master BEFORE the
// Phase 2 engineer implements the feature). The engineer must implement the
// contract so the adapters perform REAL out-of-process calls. Do NOT weaken the
// assertions to make them pass; implement the feature to satisfy them.
//
// Scope: Sprint 1 Phase 2 (PLAN.md). Deliverable = one end-to-end RF injection
// (or simulated) + a real LLM call. The adapters previously only connect() and
// returned canned JSON from invoke(); this phase makes invoke() perform real
// HTTP calls.
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G.
// No DB required. T1/T2 require a live Ollama server and are marked [SKIP] when
// absent so the suite stays deterministic in CI. T3-T6 are fully deterministic.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <string>

#include "core/adapters/Adapter.h"
#include "core/adapters/HttpClient.h"
#include "core/adapters/LlmAdapter.h"
#include "core/adapters/SkydelAdapter.h"

namespace ad = lodestar::adapters;

namespace {

// ---------------------------------------------------------------------------
// Lightweight test harness.
// ---------------------------------------------------------------------------
class Harness {
public:
    explicit Harness(const char* name) : name_(name) {}

    void section(const char* s) { std::printf("\n-- %s --\n", s); }

    void check(bool cond, const char* what) {
        if (cond) {
            std::printf("  [PASS] %s\n", what);
        } else {
            std::printf("  [FAIL] %s\n", what);
            ++failures_;
        }
    }

    void skip(const char* what) { std::printf("  [SKIP] %s\n", what); }

    int failures() const { return failures_; }
    const char* name() const { return name_; }

private:
    const char* name_;
    int failures_ = 0;
};

// Probe whether a live server is reachable at host:port (status != 0 means the
// TCP connect + HTTP round-trip succeeded at the transport level).
bool serverReachable(const std::string& host, int port) {
    std::string err;
    auto resp = ad::HttpClient::request(host, port, "GET", "/", "", "", 1500, &err);
    return resp.status != 0;
}

// ---------------------------------------------------------------------------
// T1. LlmAdapter health against a live server returns ok.
// ---------------------------------------------------------------------------
void testLlmHealth(Harness& h) {
    h.section("T1 LlmAdapter health (live server)");
    ad::LlmAdapter llm("llm");
    ad::AdapterConfig cfg("127.0.0.1", 11434);
    if (!llm.connect(cfg)) {
        h.check(false, "llm connect");
        return;
    }
    if (!serverReachable("127.0.0.1", 11434)) {
        h.skip("no live Ollama server; skipping live health assertion");
        return;
    }
    try {
        lodestar::Json r = llm.invoke("health", lodestar::Json::object());
        h.check(r.has("ok") && r.at("ok").asBool(), "health ok == true");
        h.check(r.has("status") && r.at("status").asNumber() == 200,
                "health status == 200");
    } catch (const ad::AdapterError& e) {
        h.check(false, "health threw AdapterError unexpectedly");
    }
}

// ---------------------------------------------------------------------------
// T2. LlmAdapter complete returns a non-empty reply.
// ---------------------------------------------------------------------------
void testLlmComplete(Harness& h) {
    h.section("T2 LlmAdapter complete (live server)");
    ad::LlmAdapter llm("llm");
    ad::AdapterConfig cfg("127.0.0.1", 11434);
    if (!llm.connect(cfg)) {
        h.check(false, "llm connect");
        return;
    }
    if (!serverReachable("127.0.0.1", 11434)) {
        h.skip("no live Ollama server; skipping live complete assertion");
        return;
    }
    try {
        lodestar::Json params = lodestar::Json::object();
        params["prompt"] = lodestar::Json::string("Say hi");
        lodestar::Json r = llm.invoke("complete", params);
        h.check(r.has("ok") && r.at("ok").asBool(), "complete ok == true");
        bool hasReply = r.has("reply");
        h.check(hasReply, "reply present");
        if (hasReply) {
            const lodestar::Json& reply = r.at("reply");
            bool nonEmpty = false;
            if (reply.isString()) {
                nonEmpty = !reply.asString().empty();
            } else if (reply.isObject() && reply.has("response")) {
                nonEmpty = !reply.at("response").asString().empty();
            }
            h.check(nonEmpty, "reply non-empty");
        }
    } catch (const ad::AdapterError& e) {
        h.check(false, "complete threw AdapterError unexpectedly");
    }
}

// ---------------------------------------------------------------------------
// T3. LlmAdapter unreachable host -> typed network error.
// ---------------------------------------------------------------------------
void testLlmUnreachable(Harness& h) {
    h.section("T3 LlmAdapter unreachable host -> typed network error");
    ad::LlmAdapter llm("llm");
    ad::AdapterConfig cfg("127.0.0.1", 1);
    cfg.timeoutMs = 1000;
    if (!llm.connect(cfg)) {
        h.check(false, "llm connect");
        return;
    }
    bool threw = false;
    bool typed = false;
    try {
        lodestar::Json params = lodestar::Json::object();
        params["prompt"] = lodestar::Json::string("hi");
        llm.invoke("complete", params);
    } catch (const ad::AdapterError& e) {
        threw = true;
        typed = (e.code() == ad::AdapterError::Code::Network ||
                 e.code() == ad::AdapterError::Code::Timeout);
    } catch (...) {
        threw = true;
    }
    h.check(threw, "complete threw AdapterError");
    h.check(typed, "code == Network or Timeout");
    h.check(llm.status().failed(), "status().failed()");
}

// ---------------------------------------------------------------------------
// T4. SkydelAdapter simulated end-to-end RF injection.
// ---------------------------------------------------------------------------
void testSkydelSimulated(Harness& h) {
    h.section("T4 SkydelAdapter simulated end-to-end RF injection");
    ad::SkydelAdapter skydel("skydel");
    ad::AdapterConfig cfg("127.0.0.1", 8081);
    cfg.params["simulate"] = "1";
    if (!skydel.connect(cfg)) {
        h.check(false, "skydel connect");
        return;
    }
    try {
        lodestar::Json r1 = skydel.invoke("start", lodestar::Json::object());
        h.check(r1.has("ok") && r1.at("ok").asBool(), "start ok == true");

        lodestar::Json sc = lodestar::Json::object();
        sc["constellation"] = lodestar::Json::string("GPS");
        lodestar::Json r2 = skydel.invoke("setConstellation", sc);
        h.check(r2.has("ok") && r2.at("ok").asBool(), "setConstellation ok == true");
        h.check(r2.has("params") && r2.at("params").has("constellation") &&
                    r2.at("params").at("constellation").asString() == "GPS",
                "setConstellation echoes params");

        lodestar::Json r3 = skydel.invoke("stop", lodestar::Json::object());
        h.check(r3.has("ok") && r3.at("ok").asBool(), "stop ok == true");
    } catch (const ad::AdapterError& e) {
        h.check(false, "simulated sequence threw AdapterError unexpectedly");
    }
}

// ---------------------------------------------------------------------------
// T5. SkydelAdapter unreachable host -> typed network error.
// ---------------------------------------------------------------------------
void testSkydelUnreachable(Harness& h) {
    h.section("T5 SkydelAdapter unreachable host -> typed network error");
    ad::SkydelAdapter skydel("skydel");
    ad::AdapterConfig cfg("127.0.0.1", 1);
    cfg.timeoutMs = 1000;
    if (!skydel.connect(cfg)) {
        h.check(false, "skydel connect");
        return;
    }
    bool threw = false;
    bool typed = false;
    try {
        skydel.invoke("start", lodestar::Json::object());
    } catch (const ad::AdapterError& e) {
        threw = true;
        typed = (e.code() == ad::AdapterError::Code::Network ||
                 e.code() == ad::AdapterError::Code::Timeout);
    } catch (...) {
        threw = true;
    }
    h.check(threw, "start threw AdapterError");
    h.check(typed, "code == Network or Timeout");
    h.check(skydel.status().failed(), "status().failed()");
}

// ---------------------------------------------------------------------------
// T6. SkydelAdapter not connected -> typed NotConnected error.
// ---------------------------------------------------------------------------
void testSkydelNotConnected(Harness& h) {
    h.section("T6 SkydelAdapter not connected -> typed NotConnected error");
    ad::SkydelAdapter skydel("skydel");
    bool threw = false;
    bool notConnected = false;
    try {
        skydel.invoke("start", lodestar::Json::object());
    } catch (const ad::AdapterError& e) {
        threw = true;
        notConnected = (e.code() == ad::AdapterError::Code::NotConnected);
    } catch (...) {
        threw = true;
    }
    h.check(threw, "start threw AdapterError");
    h.check(notConnected, "code == NotConnected");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    (void)argc;
    (void)argv;

    Harness h("S1 Phase 2 functional adapters");
    std::printf("S1 PHASE 2 FUNCTIONAL ADAPTERS TESTS\n");

    testLlmHealth(h);
    testLlmComplete(h);
    testLlmUnreachable(h);
    testSkydelSimulated(h);
    testSkydelUnreachable(h);
    testSkydelNotConnected(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
