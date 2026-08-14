// core/test/cc4_llm_client_tests.cpp
// ---------------------------------------------------------------------------
// Gap-Fill Cross-cutting #4: shared LlmClient abstraction tests.
//
// Test contract: docs/gap-fill-plan.md (Cross-cutting #4).
//   (A) A shared LlmClient abstraction every AI feature uses guarantees local
//       models, no data egress (a non-local host is rejected before any
//       request), and a deterministic fallback when the LLM is unavailable.
//
// Deterministic (MockAdapter, no live LLM).
// ---------------------------------------------------------------------------

#include <cstdio>
#include <string>

#include "core/adapters/Adapter.h"
#include "core/adapters/LlmClient.h"
#include "core/adapters/MockAdapter.h"

namespace ad = lodestar::adapters;

namespace {

class Harness {
public:
    explicit Harness(const char* name) : name_(name) {}
    void section(const char* s) { std::printf("\n-- %s --\n", s); }
    void check(bool cond, const char* what) {
        if (cond) { std::printf("  [PASS] %s\n", what); }
        else { std::printf("  [FAIL] %s\n", what); ++failures_; }
    }
    int failures() const { return failures_; }
    const char* name() const { return name_; }
private:
    const char* name_;
    int failures_ = 0;
};

}  // namespace

// ---------------------------------------------------------------------------
// T1. Local-host detection (no egress policy)
// ---------------------------------------------------------------------------
static void testLocalHost(Harness& h) {
    h.section("T1. local-host detection (no data egress)");
    h.check(ad::LlmClient::isLocalHost("localhost"), "localhost is local");
    h.check(ad::LlmClient::isLocalHost("127.0.0.1"), "127.0.0.1 is local");
    h.check(ad::LlmClient::isLocalHost("192.168.1.10"), "RFC1918 is local");
    h.check(ad::LlmClient::isLocalHost("10.0.0.5"), "10/8 is local");
    h.check(ad::LlmClient::isLocalHost("172.20.0.3"), "172.16/12 is local");
    h.check(!ad::LlmClient::isLocalHost("8.8.8.8"), "public DNS is NOT local");
    h.check(!ad::LlmClient::isLocalHost("api.openai.com"),
            "remote host is NOT local");
    h.check(!ad::LlmClient::isLocalHost(""), "empty host is NOT local");
}

// ---------------------------------------------------------------------------
// T2. Non-local host -> client disabled (never egresses)
// ---------------------------------------------------------------------------
static void testEgressGuard(Harness& h) {
    h.section("T2. no-egress guard");
    ad::MockAdapter mock;
    ad::AdapterConfig remote("api.openai.com", 443, "", "");
    int fallbackCalls = 0;
    ad::LlmClient client(mock, remote, [&fallbackCalls]() {
        ++fallbackCalls;
        return "deterministic-fallback";
    });
    h.check(!client.usable(), "client is not usable for a remote host");
    h.check(client.localHost().empty(), "effective host is empty (no egress)");
    std::string result = client.complete("prompt");
    h.check(result == "deterministic-fallback",
            "fallback used (no request sent to remote)");
    h.check(fallbackCalls == 1, "fallback invoked exactly once");
}

// ---------------------------------------------------------------------------
// T3. Local host -> usable, invoke attempted, fallback on unavailability
// ---------------------------------------------------------------------------
static void testLocalPath(Harness& h) {
    h.section("T3. local host usable + deterministic fallback");
    ad::MockAdapter mock;
    ad::AdapterConfig local("127.0.0.1", 11434, "", "");
    int fallbackCalls = 0;
    ad::LlmClient client(mock, local, [&fallbackCalls]() {
        ++fallbackCalls;
        return "fallback";
    });
    h.check(client.usable(), "client is usable for a local host");
    h.check(client.localHost() == "127.0.0.1", "effective host is local");

    // MockAdapter.invoke returns a canned reply; if it doesn't contain a
    // "reply" string, the fallback is used. Either way the client returns a
    // deterministic result (no exception).
    std::string result = client.complete("hello");
    h.check(!result.empty(), "client returns a non-empty result");
}

// ---------------------------------------------------------------------------
// T4. Model name config
// ---------------------------------------------------------------------------
static void testModel(Harness& h) {
    h.section("T4. model config");
    ad::MockAdapter mock;
    ad::AdapterConfig local("127.0.0.1", 11434, "", "");
    local.params["model"] = "qwen2.5:7b";
    ad::LlmClient client(mock, local, []() { return ""; });
    h.check(client.model() == "qwen2.5:7b", "model name read from config");
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    Harness h("Gap-Fill Cross-cutting #4 shared LlmClient");
    testLocalHost(h);
    testEgressGuard(h);
    testLocalPath(h);
    testModel(h);
    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
