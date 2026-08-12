// core/test/s1_phase3_tests.cpp
// ---------------------------------------------------------------------------
// S1 Phase 3: RiskAI first slice (hazard -> LLM -> FMEA) tests.
//
// Test contract: docs/s1-phase3-test.md (written by the scrum-master BEFORE the
// Phase 3 engineer implements the feature). The engineer must implement the
// contract so RiskAI turns a hazard description into a structured FMEA table
// via a real LLM call. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// Scope: Sprint 1 Phase 3 (PLAN.md). Deliverable = working LLM-assisted FMEA.
// core/riskai/stub.cpp is replaced by a real RiskAiService.
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G.
// No DB required. T3 requires a live Ollama server and is marked [SKIP] when
// absent so the suite stays deterministic in CI. T1/T2/T4/T5/T6 are fully
// deterministic and must always pass.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/adapters/Adapter.h"
#include "core/adapters/HttpClient.h"
#include "core/adapters/LlmAdapter.h"
#include "core/adapters/MockAdapter.h"
#include "core/riskai/RiskAiService.h"

namespace ad = lodestar::adapters;
namespace ra = lodestar::riskai;

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

// Probe whether a live server is reachable at host:port.
bool serverReachable(const std::string& host, int port) {
    std::string err;
    auto resp = ad::HttpClient::request(host, port, "GET", "/", "", "", 1500, &err);
    return resp.status != 0;
}

// Validate every row: severity/likelihood in 1..10 and risk == severity*likelihood.
bool rowsValid(const std::vector<ra::FmeaRow>& rows) {
    for (const auto& r : rows) {
        if (r.severity < 1 || r.severity > 10) return false;
        if (r.likelihood < 1 || r.likelihood > 10) return false;
        if (r.risk != r.severity * r.likelihood) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// T1. analyze returns a non-empty FMEA table (deterministic fallback).
// ---------------------------------------------------------------------------
void testT1(Harness& h) {
    h.section("T1 analyze returns non-empty FMEA table (fallback)");
    ad::MockAdapter mock("mock");
    ad::AdapterConfig cfg("127.0.0.1", 0);
    mock.connect(cfg);
    ra::RiskAiService svc(mock, cfg);

    auto res = svc.analyze("GPS signal loss during approach");
    h.check(res.isOk(), "analyze isOk()");
    if (!res.isOk()) return;
    const auto& rows = res.value();
    h.check(rows.size() >= 2, "at least 2 rows");
    h.check(rowsValid(rows), "all rows valid (severity/likelihood 1..10, risk=sev*lik)");
}

// ---------------------------------------------------------------------------
// T2. risk is computed as severity * likelihood.
// ---------------------------------------------------------------------------
void testT2(Harness& h) {
    h.section("T2 risk == severity * likelihood");
    ad::MockAdapter mock("mock");
    ad::AdapterConfig cfg("127.0.0.1", 0);
    mock.connect(cfg);
    ra::RiskAiService svc(mock, cfg);

    auto res = svc.analyze("GPS signal loss during approach");
    h.check(res.isOk(), "analyze isOk()");
    if (!res.isOk()) return;
    bool allOk = true;
    for (const auto& r : res.value()) {
        if (r.risk != r.severity * r.likelihood) { allOk = false; break; }
    }
    h.check(allOk, "every row satisfies risk == severity * likelihood");
}

// ---------------------------------------------------------------------------
// T3. analyze against a live LLM returns parsed rows.
// ---------------------------------------------------------------------------
void testT3(Harness& h) {
    h.section("T3 analyze against live LLM returns parsed rows");
    ad::LlmAdapter llm("llm");
    ad::AdapterConfig cfg("127.0.0.1", 11434);
    if (!llm.connect(cfg)) {
        h.check(false, "llm connect");
        return;
    }
    if (!serverReachable("127.0.0.1", 11434)) {
        h.skip("no live Ollama server; skipping live LLM assertion");
        return;
    }
    ra::RiskAiService svc(llm, cfg);
    auto res = svc.analyze("GPS signal loss");
    h.check(res.isOk(), "analyze isOk()");
    if (!res.isOk()) return;
    h.check(!res.value().empty(), "returned rows non-empty");
}

// ---------------------------------------------------------------------------
// T4. analyze never throws on LLM failure.
// ---------------------------------------------------------------------------
void testT4(Harness& h) {
    h.section("T4 analyze never throws on LLM failure");
    ad::LlmAdapter llm("llm");
    ad::AdapterConfig cfg("127.0.0.1", 1);
    cfg.timeoutMs = 1000;
    if (!llm.connect(cfg)) {
        h.check(false, "llm connect");
        return;
    }
    ra::RiskAiService svc(llm, cfg);
    auto res = svc.analyze("hazard");
    h.check(res.isOk(), "analyze isOk() (fallback table, never error)");
    if (res.isOk()) {
        h.check(!res.value().empty(), "fallback rows non-empty");
    }
}

// ---------------------------------------------------------------------------
// T5. canned fallback is deterministic.
// ---------------------------------------------------------------------------
void testT5(Harness& h) {
    h.section("T5 canned fallback is deterministic");
    ad::LlmAdapter llm("llm");
    ad::AdapterConfig cfg("127.0.0.1", 1);
    cfg.timeoutMs = 1000;
    if (!llm.connect(cfg)) {
        h.check(false, "llm connect");
        return;
    }
    ra::RiskAiService svc(llm, cfg);
    auto r1 = svc.analyze("hazard A");
    auto r2 = svc.analyze("hazard A");
    h.check(r1.isOk() && r2.isOk(), "both calls isOk()");
    if (!r1.isOk() || !r2.isOk()) return;
    const auto& a = r1.value();
    const auto& b = r2.value();
    bool same = (a.size() == b.size());
    if (same) {
        for (size_t i = 0; i < a.size(); ++i) {
            if (a[i].failureMode != b[i].failureMode ||
                a[i].effect != b[i].effect ||
                a[i].severity != b[i].severity ||
                a[i].likelihood != b[i].likelihood ||
                a[i].risk != b[i].risk) {
                same = false;
                break;
            }
        }
    }
    h.check(same, "both calls return identical rows");
}

// ---------------------------------------------------------------------------
// T6. empty hazard is rejected.
// ---------------------------------------------------------------------------
void testT6(Harness& h) {
    h.section("T6 empty hazard is rejected");
    ad::MockAdapter mock("mock");
    ad::AdapterConfig cfg("127.0.0.1", 0);
    mock.connect(cfg);
    ra::RiskAiService svc(mock, cfg);

    auto r1 = svc.analyze("");
    auto r2 = svc.analyze("   ");
    h.check(r1.failed(), "analyze(\"\") failed()");
    h.check(!r1.error().empty(), "empty-hazard error message present");
    h.check(r2.failed(), "analyze(\"   \") failed()");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    (void)argc;
    (void)argv;

    Harness h("S1 Phase 3 RiskAI");
    std::printf("S1 PHASE 3 RISKAI TESTS\n");

    testT1(h);
    testT2(h);
    testT3(h);
    testT4(h);
    testT5(h);
    testT6(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
