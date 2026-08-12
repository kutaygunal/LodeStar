// core/test/wpE_tests.cpp
// ---------------------------------------------------------------------------
// WP-E unit/integration tests (test-first).
//
// Written by the scrum-master BEFORE the WP-E engineer implements the feature.
// The engineer must implement the contract documented below so these tests
// compile and pass. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// Covers (PLAN.md, WP-E):
//   A8. REST API auth / API keys.
//   A9. Duplicate / similarity detection.
//
// Uses the same lightweight self-contained harness as WP-1..WP-8. Each
// DB-dependent test opens its own fresh throwaway DB.
//
// ---------------------------------------------------------------------------
// CONTRACT the WP-E engineer must provide.
// ---------------------------------------------------------------------------
// (A) Migration 010 (core/persistence/migrations/010_*.sql) creates an
//     `api_keys` table (id, key, name, enabled, created_at), append-only and
//     idempotent.
//
// (B) New ApiKeyService (core/api/ApiKeyService.h):
//
//   class ApiKeyService {
//   public:
//       explicit ApiKeyService(persistence::Database& db);
//       common::Result<std::string> createKey(const std::string& name); // returns the key
//       common::Result<void> revokeKey(const std::string& key);
//       bool isValid(const std::string& key) const;   // enabled + present
//   };
//
// (C) HTTP plumbing for auth:
//   - HttpRequest (core/api/HttpServer.h) gains
//       std::map<std::string,std::string> headers;   // lowercased keys
//   - HttpServer parses request headers into req.headers.
//   - HttpClient::request (core/adapters/HttpClient.h) gains a 9th parameter
//       const std::string& extraHeaders = "";   // CRLF-separated extra header lines
//
// (D) TraceLinkApiServer (core/api/TraceLinkApiServer.h) constructor gains an
//     optional trailing parameter `ApiKeyService* auth = nullptr`. When auth is
//     non-null, every /tracelink route requires a valid `X-API-Key` header;
//     missing or invalid -> 401 with the standard error model
//     {"error":{"code":401,"message":"..."}}.
//
// (E) TraceLinkService addition (core/tracelink/TraceLinkService.h):
//
//   // A group of similar entities (by name/text similarity).
//   struct DuplicateGroup {
//       std::vector<Entity> entities;   // >= 2 similar entities
//       double similarity = 0.0;        // max pairwise similarity (0..1)
//   };
//
//   // Groups entities of `type` whose pairwise similarity >= threshold.
//   // Exact duplicates (identical name+text) always group. Returns groups
//   // with at least 2 members.
//   common::Result<std::vector<DuplicateGroup>> findDuplicates(
//       EntityType type, double threshold = 0.8);
// ---------------------------------------------------------------------------

#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/adapters/HttpClient.h"
#include "core/adapters/Json.h"
#include "core/api/ApiKeyService.h"
#include "core/api/HttpServer.h"
#include "core/api/TraceLinkApiServer.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/tracelink/BaselineService.h"
#include "core/tracelink/GraphEngine.h"
#include "core/tracelink/IoService.h"
#include "core/tracelink/RulesEngine.h"
#include "core/tracelink/TraceLinkService.h"
#include "core/tracelink/Types.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace tl = lodestar::tracelink;
namespace ap = lodestar::api;
namespace p  = lodestar::persistence;

namespace {

std::string g_migrationsDir = LODESTAR_MIGRATIONS_DIR;

// Opens a fresh throwaway DB for one test, runs migrations, returns true on ok.
bool openFreshDb(p::Database& db, const char* file) {
    std::remove(file);
    if (db.open(file).failed()) return false;
    p::MigrationRunner runner(db);
    auto mig = runner.run(g_migrationsDir);
    return mig.isOk();
}

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

    int failures() const { return failures_; }
    const char* name() const { return name_; }

private:
    const char* name_;
    int failures_ = 0;
};

bool hasField(const std::string& body, const std::string& path) {
    try {
        lodestar::Json j = lodestar::Json::parse(body);
        const lodestar::Json* cur = &j;
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

class Client {
public:
    Client(const std::string& host, int port) : host_(host), port_(port) {}

    lodestar::adapters::HttpClient::Response req(
        const std::string& method, const std::string& path,
        const std::string& body = "",
        const std::string& contentType = "application/json",
        const std::string& extraHeaders = "") {
        std::string err;
        return lodestar::adapters::HttpClient::request(
            host_, port_, method, path, body, contentType, 4000, &err, extraHeaders);
    }

private:
    std::string host_;
    int port_;
};

// Server fixture with optional API-key auth.
struct Server {
    p::Database db;
    std::unique_ptr<tl::TraceLinkService> svc;
    std::unique_ptr<tl::GraphEngine> graph;
    std::unique_ptr<tl::RulesEngine> rules;
    std::unique_ptr<tl::BaselineService> baseline;
    std::unique_ptr<tl::IoService> io;
    std::unique_ptr<ap::ApiKeyService> auth;
    std::unique_ptr<ap::TraceLinkApiServer> api;
    ap::HttpServer http;
    int port = -1;
};

bool setupServer(Server& s, const char* dbfile, bool withAuth) {
    std::remove(dbfile);
    if (s.db.open(dbfile).failed()) return false;
    p::MigrationRunner runner(s.db);
    if (runner.run(g_migrationsDir).failed()) return false;

    s.svc = std::make_unique<tl::TraceLinkService>(s.db);
    s.graph = std::make_unique<tl::GraphEngine>(s.db);
    s.rules = std::make_unique<tl::RulesEngine>(s.db);
    s.baseline = std::make_unique<tl::BaselineService>(s.db);
    s.io = std::make_unique<tl::IoService>(s.db);
    if (withAuth) {
        s.auth = std::make_unique<ap::ApiKeyService>(s.db);
    }

    s.api = std::make_unique<ap::TraceLinkApiServer>(
        *s.svc, *s.graph, *s.rules, *s.baseline, *s.io, s.auth.get());
    s.api->setup(s.http);
    if (!s.http.start(0)) return false;
    s.port = s.http.port();
    return s.port > 0;
}

// ---------------------------------------------------------------------------
// A8. REST API auth / API keys
// ---------------------------------------------------------------------------
void testApiAuth(Harness& h) {
    h.section("A8. REST API auth / API keys");

    Server s;
    if (!setupServer(s, "lodestar_wpE_auth.db", true)) {
        h.check(false, "server start with auth");
        return;
    }
    Client c("127.0.0.1", s.port);

    // Create a valid API key.
    auto keyRes = s.auth->createKey("test-key");
    h.check(keyRes.isOk() && !keyRes.value().empty(), "createKey returns a key");
    const std::string key = keyRes.value();

    // Request without a key -> 401.
    auto noKey = c.req("GET", "/tracelink/entities?type=requirement");
    h.check(noKey.status == 401 && hasField(noKey.body, "error.code"),
            "request without API key -> 401 error model");

    // Request with an invalid key -> 401.
    auto badKey = c.req("GET", "/tracelink/entities?type=requirement",
                        "", "application/json", "X-API-Key: not-a-real-key");
    h.check(badKey.status == 401 && hasField(badKey.body, "error.code"),
            "request with invalid API key -> 401 error model");

    // Request with the valid key -> 200.
    auto ok = c.req("GET", "/tracelink/entities?type=requirement",
                    "", "application/json", "X-API-Key: " + key);
    h.check(ok.status == 200 && hasField(ok.body, "entities"),
            "request with valid API key -> 200");

    // Revoke the key -> subsequent requests are 401.
    h.check(s.auth->revokeKey(key).isOk(), "revokeKey ok");
    auto revoked = c.req("GET", "/tracelink/entities?type=requirement",
                         "", "application/json", "X-API-Key: " + key);
    h.check(revoked.status == 401 && hasField(revoked.body, "error.code"),
            "request with revoked API key -> 401");

    s.http.stop();
}

// ---------------------------------------------------------------------------
// A9. Duplicate / similarity detection
// ---------------------------------------------------------------------------
void testDuplicates(Harness& h) {
    h.section("A9. Duplicate / similarity detection");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wpE_dup.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);

    // Two exact duplicates (identical name + text).
    auto d1 = svc.addEntity([&] {
        tl::Entity e;
        e.externalId = "REQ-D1";
        e.type = tl::EntityType::Requirement;
        e.name = "Position Accuracy";
        e.text = "The system shall provide position accuracy.";
        e.status = "Draft";
        return e;
    }());
    auto d2 = svc.addEntity([&] {
        tl::Entity e;
        e.externalId = "REQ-D2";
        e.type = tl::EntityType::Requirement;
        e.name = "Position Accuracy";
        e.text = "The system shall provide position accuracy.";
        e.status = "Draft";
        return e;
    }());
    h.check(d1.isOk() && d2.isOk(), "seed exact duplicates ok");

    // A clearly distinct requirement.
    auto d3 = svc.addEntity([&] {
        tl::Entity e;
        e.externalId = "REQ-D3";
        e.type = tl::EntityType::Requirement;
        e.name = "Altitude Output";
        e.text = "The system shall output altitude.";
        e.status = "Draft";
        return e;
    }());
    h.check(d3.isOk(), "seed distinct requirement ok");

    // findDuplicates groups the exact duplicates.
    auto groups = svc.findDuplicates(tl::EntityType::Requirement);
    h.check(groups.isOk(), "findDuplicates ok");
    h.check(groups.value().size() == 1, "exactly one duplicate group");
    if (groups.isOk() && groups.value().size() == 1) {
        h.check(groups.value()[0].entities.size() == 2,
                "duplicate group has 2 members");
        h.check(groups.value()[0].similarity >= 0.99,
                "exact duplicates have near-1.0 similarity");
    }

    // A near-duplicate (same name, slightly different text) is grouped.
    auto d4 = svc.addEntity([&] {
        tl::Entity e;
        e.externalId = "REQ-D4";
        e.type = tl::EntityType::Requirement;
        e.name = "Position Accuracy";
        e.text = "The system shall provide position accuracy to 1m.";
        e.status = "Draft";
        return e;
    }());
    h.check(d4.isOk(), "seed near-duplicate ok");
    auto groups2 = svc.findDuplicates(tl::EntityType::Requirement);
    h.check(groups2.isOk() && groups2.value().size() == 1,
            "near-duplicate joins the same group");
    if (groups2.isOk() && groups2.value().size() == 1) {
        h.check(groups2.value()[0].entities.size() == 3,
                "group now has 3 members");
    }

    // A high threshold excludes the near-duplicate but keeps exact ones.
    auto strict = svc.findDuplicates(tl::EntityType::Requirement, 0.99);
    h.check(strict.isOk() && strict.value().size() == 1,
            "strict threshold still groups exact duplicates");
    if (strict.isOk() && strict.value().size() == 1) {
        h.check(strict.value()[0].entities.size() == 2,
                "strict threshold excludes the near-duplicate");
    }

    db.close();
    std::remove("lodestar_wpE_dup.db");
    std::remove("lodestar_wpE_dup.db-wal");
    std::remove("lodestar_wpE_dup.db-shm");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("WP-E API + duplicates");
    std::printf("WP-E API + DUPLICATES TESTS (migrations: %s)\n",
                g_migrationsDir.c_str());

    testApiAuth(h);
    testDuplicates(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
