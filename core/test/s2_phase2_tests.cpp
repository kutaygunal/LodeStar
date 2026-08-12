// core/test/s2_phase2_tests.cpp
// ---------------------------------------------------------------------------
// S2 Phase 2 tests (test contract): web / browser read-review layer over the
// REST API.
//
// Written by the scrum-master BEFORE the Phase 2 engineer implements the
// feature. The engineer must implement the contract below so these tests
// compile and pass. Do NOT weaken the assertions; implement the feature to
// satisfy them.
//
// Covers (PLAN.md, S2 Phase 2):
//   (A) A web layer over the REST API (reusing core/api/HttpServer):
//         GET /web/            -> an HTML page
//         GET /web/requirements-> requirements as HTML/JSON
//         GET /web/trace       -> the trace matrix as HTML/JSON
//         GET /web/assure      -> AssureCheck compliance summary as HTML/JSON
//   (B) Auth-aware: honors Phase 1 auth (login/roles); a viewer can read, an
//       editor can review.
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / S1 phases.
// Each test opens its own fresh throwaway DB and drives a real in-process
// HTTP server over localhost.
// ---------------------------------------------------------------------------
// CONTRACT the Phase 2 engineer must provide.
// ---------------------------------------------------------------------------
// (A) A web route registrar (core/api/WebServer.h, namespace lodestar::api):
//
//   class WebServer {
//   public:
//       explicit WebServer(persistence::Database& db);
//       void setup(HttpServer& server);   // registers every /web route
//   };
//
// (B) Routes:
//   GET /web/             -> 200 HTML page (contains <html or <!DOCTYPE)
//   GET /web/requirements -> 200 body contains the seeded requirement's
//                            external id (e.g. REQ-001)
//   GET /web/trace        -> 200 body contains the trace link data
//   GET /web/assure       -> 200 body contains a compliance summary
//                            (pass count or percentage)
// ---------------------------------------------------------------------------

#include <cstdio>
#include <memory>
#include <string>

#include "core/adapters/HttpClient.h"
#include "core/api/HttpServer.h"
#include "core/api/WebServer.h"
#include "core/assurecheck/AssureCheckService.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/tracelink/TraceLinkService.h"
#include "core/tracelink/Types.h"
#include "core/tracelink/UserService.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace tl = lodestar::tracelink;
namespace ap = lodestar::api;
namespace p  = lodestar::persistence;
namespace ac = lodestar::assurecheck;

namespace {

std::string g_migrationsDir = LODESTAR_MIGRATIONS_DIR;

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

// ---------------------------------------------------------------------------
// Live HTTP test client bound to a running server.
// ---------------------------------------------------------------------------
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
            host_, port_, method, path, body, contentType, 4000, &err,
            extraHeaders);
    }

private:
    std::string host_;
    int port_;
};

// ---------------------------------------------------------------------------
// Server fixture: fresh DB + seeded data + web routes on an ephemeral port.
// ---------------------------------------------------------------------------
struct Server {
    p::Database db;
    std::unique_ptr<tl::TraceLinkService> tls;
    std::unique_ptr<ap::WebServer> web;
    ap::HttpServer http;
    int port = -1;
};

bool setupServer(Server& s, const char* dbfile) {
    std::remove(dbfile);
    if (s.db.open(dbfile).failed()) return false;
    p::MigrationRunner runner(s.db);
    if (runner.run(g_migrationsDir).failed()) return false;

    // Seed the assurance standards (idempotent) so /web/assure has data.
    ac::AssureCheckService assure(s.db);
    if (assure.seedStandards().failed()) return false;

    // Seed a requirement, a design item, and a trace link between them.
    s.tls = std::make_unique<tl::TraceLinkService>(s.db);

    tl::Entity req;
    req.externalId = "REQ-001";
    req.type = tl::EntityType::Requirement;
    req.name = "Navigation";
    req.text = "Provide navigation.";
    req.status = "Draft";
    auto addedReq = s.tls->addEntity(req);
    if (addedReq.failed()) return false;

    tl::Entity design;
    design.externalId = "DES-001";
    design.type = tl::EntityType::Design;
    design.name = "Nav Module";
    design.text = "Implements navigation.";
    design.status = "Draft";
    auto addedDesign = s.tls->addEntity(design);
    if (addedDesign.failed()) return false;

    tl::Link link;
    link.sourceType = tl::EntityType::Requirement;
    link.sourceId = addedReq.value().id;
    link.targetType = tl::EntityType::Design;
    link.targetId = addedDesign.value().id;
    link.relation = "allocates";
    auto addedLink = s.tls->addLink(link);
    if (addedLink.failed()) return false;

    s.web = std::make_unique<ap::WebServer>(s.db);
    s.web->setup(s.http);
    if (!s.http.start(0)) return false;
    s.port = s.http.port();
    return s.port > 0;
}

// ---------------------------------------------------------------------------
// T1. web root serves an HTML page
// ---------------------------------------------------------------------------
void testWebRoot(Harness& h) {
    h.section("T1. GET /web/ serves an HTML page");

    Server s;
    if (!setupServer(s, "lodestar_s2p2_root.db")) {
        h.check(false, "server start");
        return;
    }
    Client c("127.0.0.1", s.port);

    auto r = c.req("GET", "/web/");
    h.check(r.status == 200, "GET /web/ -> 200");
    h.check(r.body.find("<html") != std::string::npos ||
                r.body.find("<!DOCTYPE") != std::string::npos,
            "body contains <html or <!DOCTYPE");

    s.http.stop();
    s.db.close();
    std::remove("lodestar_s2p2_root.db");
    std::remove("lodestar_s2p2_root.db-wal");
    std::remove("lodestar_s2p2_root.db-shm");
}

// ---------------------------------------------------------------------------
// T2. requirements endpoint returns data
// ---------------------------------------------------------------------------
void testRequirements(Harness& h) {
    h.section("T2. GET /web/requirements returns the seeded requirement");

    Server s;
    if (!setupServer(s, "lodestar_s2p2_req.db")) {
        h.check(false, "server start");
        return;
    }
    Client c("127.0.0.1", s.port);

    auto r = c.req("GET", "/web/requirements");
    h.check(r.status == 200, "GET /web/requirements -> 200");
    h.check(r.body.find("REQ-001") != std::string::npos,
            "body contains the seeded requirement external id REQ-001");

    s.http.stop();
    s.db.close();
    std::remove("lodestar_s2p2_req.db");
    std::remove("lodestar_s2p2_req.db-wal");
    std::remove("lodestar_s2p2_req.db-shm");
}

// ---------------------------------------------------------------------------
// T3. trace endpoint returns the matrix
// ---------------------------------------------------------------------------
void testTrace(Harness& h) {
    h.section("T3. GET /web/trace returns the trace matrix");

    Server s;
    if (!setupServer(s, "lodestar_s2p2_trace.db")) {
        h.check(false, "server start");
        return;
    }
    Client c("127.0.0.1", s.port);

    auto r = c.req("GET", "/web/trace");
    h.check(r.status == 200, "GET /web/trace -> 200");
    h.check(r.body.find("allocates") != std::string::npos,
            "body contains the trace link data (relation 'allocates')");

    s.http.stop();
    s.db.close();
    std::remove("lodestar_s2p2_trace.db");
    std::remove("lodestar_s2p2_trace.db-wal");
    std::remove("lodestar_s2p2_trace.db-shm");
}

// ---------------------------------------------------------------------------
// T4. assure endpoint returns compliance summary
// ---------------------------------------------------------------------------
void testAssure(Harness& h) {
    h.section("T4. GET /web/assure returns a compliance summary");

    Server s;
    if (!setupServer(s, "lodestar_s2p2_assure.db")) {
        h.check(false, "server start");
        return;
    }
    Client c("127.0.0.1", s.port);

    auto r = c.req("GET", "/web/assure");
    h.check(r.status == 200, "GET /web/assure -> 200");
    h.check(r.body.find("Pass count") != std::string::npos ||
                r.body.find("%") != std::string::npos,
            "body contains a compliance summary (pass count or percentage)");

    s.http.stop();
    s.db.close();
    std::remove("lodestar_s2p2_assure.db");
    std::remove("lodestar_s2p2_assure.db-wal");
    std::remove("lodestar_s2p2_assure.db-shm");
}

// ---------------------------------------------------------------------------
// T5. auth-aware: a viewer can read; an invalid token is rejected
// ---------------------------------------------------------------------------
void testAuthAware(Harness& h) {
    h.section("T5. web layer honors Phase 1 auth (viewer can read)");

    Server s;
    if (!setupServer(s, "lodestar_s2p2_auth.db")) {
        h.check(false, "server start");
        return;
    }
    Client c("127.0.0.1", s.port);

    // Register a viewer and log in.
    tl::UserService users(s.db);
    auto reg = users.registerUser("viewer1", "pw", "viewer");
    h.check(reg.isOk(), "register viewer1");
    auto login = users.login("viewer1", "pw");
    h.check(login.isOk(), "login viewer1");
    std::string token = login.value();

    // A viewer can read the requirements endpoint.
    auto viewer = c.req("GET", "/web/requirements", "", "application/json",
                        "Authorization: Bearer " + token);
    h.check(viewer.status == 200 && viewer.body.find("REQ-001") != std::string::npos,
            "viewer can read /web/requirements (200 + REQ-001)");

    // An invalid token is rejected with 401.
    auto bad = c.req("GET", "/web/requirements", "", "application/json",
                     "Authorization: Bearer not-a-real-token");
    h.check(bad.status == 401, "invalid token -> 401");

    s.http.stop();
    s.db.close();
    std::remove("lodestar_s2p2_auth.db");
    std::remove("lodestar_s2p2_auth.db-wal");
    std::remove("lodestar_s2p2_auth.db-shm");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("S2 Phase 2 Web / browser read-review layer");
    std::printf("S2 PHASE 2 TESTS (migrations: %s)\n", g_migrationsDir.c_str());

    testWebRoot(h);
    testRequirements(h);
    testTrace(h);
    testAssure(h);
    testAuthAware(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
