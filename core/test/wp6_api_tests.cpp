// core/test/wp6_api_tests.cpp
// ---------------------------------------------------------------------------
// WP-6 unit/integration tests (test-first).
//
// Written by the scrum-master BEFORE the WP-6 engineer implements the feature.
// The engineer must implement the contract documented below so these tests
// compile and pass. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// Covers (docs/tracelink-plan.md, WP-6 / section 5):
//   1. Every /tracelink route returns the correct status
//   2. The error model (200/400/404/500 with {"error":{"code","message"}})
//   3. WP-6 acceptance chain:
//        POST entity -> POST link -> GET coverage -> POST validate ->
//        POST baseline -> GET diff
//
// Drives a real in-process HTTP server (like the adapters/api smoke path) with
// HttpClient over localhost. Each test opens its own fresh throwaway DB.
//
// ---------------------------------------------------------------------------
// CONTRACT the WP-6 engineer must provide.
// ---------------------------------------------------------------------------
// (A) A route registrar (core/api/TraceLinkApiServer.h, namespace lodestar::api):
//
//   class TraceLinkApiServer {
//   public:
//       TraceLinkApiServer(tracelink::TraceLinkService& svc,
//                          tracelink::GraphEngine& graph,
//                          tracelink::RulesEngine& rules,
//                          tracelink::BaselineService& baseline,
//                          tracelink::IoService& io);
//       void setup(HttpServer& server);   // registers every /tracelink route
//   };
//
// (B) Routes (method, path, success status, request body, response shape):
//
//   GET    /tracelink/entities?type=<t>&filter=<s>
//         -> 200 {"entities":[...]}                    400 missing type
//   POST   /tracelink/entities
//         body {"external_id","type","name","text","status"}
//         -> 200 entity {"id","external_id","type","name","text","status"}
//         -> 400 invalid/missing required field
//   GET    /tracelink/entities/{type}/{id}  -> 200 entity | 404 | 400 bad type
//   PUT    /tracelink/entities/{type}/{id}
//         body entity -> 200 entity | 404 | 400 bad type
//   DELETE /tracelink/entities/{type}/{id}  -> 200 | 404
//   GET    /tracelink/entities/{type}/{id}/history -> 200 {"history":[...]}
//   POST   /tracelink/links
//         body {"source_type","source_id","target_type","target_id","relation"}
//         -> 200 link {"id",...} | 400 dangling/self/duplicate/relation
//   PUT    /tracelink/links/{id} body link -> 200 | 404
//   DELETE /tracelink/links/{id} -> 200 | 404
//   GET    /tracelink/links?sourceType=&sourceId= -> 200 {"links":[...]}
//   GET    /tracelink/impact/{type}/{id} -> 200 {"affected":[...]}
//   GET    /tracelink/coverage -> 200 {"requirements":[...]}
//   GET    /tracelink/matrix  -> 200 {"columns":[...],"rows":[...]}
//   POST   /tracelink/validate -> 200 {"status","violations":[...]}
//   GET    /tracelink/rules -> 200 {"rules":[...]}
//   POST   /tracelink/baselines body {"name","description"} -> 200 {"id",...}
//   GET    /tracelink/baselines -> 200 {"baselines":[...]}
//   GET    /tracelink/baselines/{a}/diff?against={b} -> 200 {"entities":[...]}
//         -> 400 missing against
//   POST   /tracelink/import/{format} body=content -> 200 {"batch_id","status",
//         "imported","errors"} | 400 bad format
//   GET    /tracelink/export/{format} -> 200 content | 400 bad format
//
// (C) Error model for every 400/404/500:
//     body == {"error":{"code":<int>,"message":"..."}}
// ---------------------------------------------------------------------------

#include <cstdio>
#include <memory>
#include <string>

#include "core/adapters/HttpClient.h"
#include "core/adapters/Json.h"
#include "core/api/HttpServer.h"
#include "core/api/TraceLinkApiServer.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/tracelink/BaselineService.h"
#include "core/tracelink/GraphEngine.h"
#include "core/tracelink/IoService.h"
#include "core/tracelink/RulesEngine.h"
#include "core/tracelink/TraceLinkService.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace tl = lodestar::tracelink;
namespace ap = lodestar::api;
namespace p  = lodestar::persistence;

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
// JSON helpers.
// ---------------------------------------------------------------------------
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

std::string fieldStr(const std::string& body, const std::string& path) {
    try {
        lodestar::Json j = lodestar::Json::parse(body);
        const lodestar::Json* cur = &j;
        std::string rest = path;
        while (true) {
            size_t dot = rest.find('.');
            std::string key = dot == std::string::npos ? rest : rest.substr(0, dot);
            cur = &cur->at(key);
            if (dot == std::string::npos) return cur->asString();
            rest = rest.substr(dot + 1);
        }
    } catch (...) {
        return std::string();
    }
}

// ---------------------------------------------------------------------------
// Live HTTP test client bound to a running server.
// ---------------------------------------------------------------------------
class Client {
public:
    Client(const std::string& host, int port) : host_(host), port_(port) {}

    lodestar::adapters::HttpClient::Response req(
        const std::string& method, const std::string& path,
        const std::string& body = "",
        const std::string& contentType = "application/json") {
        std::string err;
        return lodestar::adapters::HttpClient::request(host_, port_, method, path,
                                                       body, contentType, 4000, &err);
    }

private:
    std::string host_;
    int port_;
};

// ---------------------------------------------------------------------------
// Server fixture: fresh DB + services + routes on an ephemeral port.
// ---------------------------------------------------------------------------
struct Server {
    p::Database db;
    std::unique_ptr<tl::TraceLinkService> svc;
    std::unique_ptr<tl::GraphEngine> graph;
    std::unique_ptr<tl::RulesEngine> rules;
    std::unique_ptr<tl::BaselineService> baseline;
    std::unique_ptr<tl::IoService> io;
    std::unique_ptr<ap::TraceLinkApiServer> api;
    ap::HttpServer http;
    int port = -1;
};

bool setupServer(Server& s, const char* dbfile) {
    std::remove(dbfile);
    if (s.db.open(dbfile).failed()) return false;
    p::MigrationRunner runner(s.db);
    if (runner.run(g_migrationsDir).failed()) return false;

    s.svc = std::make_unique<tl::TraceLinkService>(s.db);
    s.graph = std::make_unique<tl::GraphEngine>(s.db);
    s.rules = std::make_unique<tl::RulesEngine>(s.db);
    s.baseline = std::make_unique<tl::BaselineService>(s.db);
    s.io = std::make_unique<tl::IoService>(s.db);

    s.api = std::make_unique<ap::TraceLinkApiServer>(*s.svc, *s.graph, *s.rules,
                                                     *s.baseline, *s.io);
    s.api->setup(s.http);
    if (!s.http.start(0)) return false;
    s.port = s.http.port();
    return s.port > 0;
}

// ---------------------------------------------------------------------------
// 1 + 2. Entities CRUD + error model
// ---------------------------------------------------------------------------
void testEntities(Harness& h) {
    h.section("1. /tracelink/entities CRUD + error model");

    Server s;
    if (!setupServer(s, "lodestar_wp6_entities.db")) {
        h.check(false, "server start");
        return;
    }
    Client c("127.0.0.1", s.port);

    // list requires type
    auto noType = c.req("GET", "/tracelink/entities");
    h.check(noType.status == 400 && hasField(noType.body, "error.code"),
            "GET /entities without type -> 400 error model");

    auto empty = c.req("GET", "/tracelink/entities?type=requirement");
    h.check(empty.status == 200 && hasField(empty.body, "entities"),
            "GET /entities?type=requirement -> 200 list");

    // create entity
    auto create = c.req("POST", "/tracelink/entities",
                        "{\"external_id\":\"API-REQ-1\",\"type\":\"requirement\","
                        "\"name\":\"A\",\"text\":\"t\",\"status\":\"Draft\"}");
    h.check(create.status == 200 && hasField(create.body, "id"),
            "POST /entities -> 200 with id");
    std::string reqId = fieldStr(create.body, "id");

    // invalid create -> 400
    auto bad = c.req("POST", "/tracelink/entities", "{\"type\":\"requirement\"}");
    h.check(bad.status == 400 && hasField(bad.body, "error.code"),
            "POST /entities missing required field -> 400 error model");

    // get by id
    auto get = c.req("GET", "/tracelink/entities/requirement/" + reqId);
    h.check(get.status == 200 && hasField(get.body, "id") &&
                fieldStr(get.body, "external_id") == "API-REQ-1",
            "GET /entities/requirement/{id} -> 200");

    // get missing -> 404
    auto nf = c.req("GET", "/tracelink/entities/requirement/nope");
    h.check(nf.status == 404 && hasField(nf.body, "error.code"),
            "GET /entities/requirement/missing -> 404 error model");

    // bad type -> 400
    auto badType = c.req("GET", "/tracelink/entities/bogus/" + reqId);
    h.check(badType.status == 400 && hasField(badType.body, "error.code"),
            "GET /entities/bogus/{id} -> 400 error model");

    // update
    auto upd = c.req("PUT", "/tracelink/entities/requirement/" + reqId,
                     "{\"id\":\"" + reqId + "\",\"external_id\":\"API-REQ-1\","
                     "\"type\":\"requirement\",\"name\":\"A2\",\"text\":\"t2\","
                     "\"status\":\"Approved\"}");
    h.check(upd.status == 200 && fieldStr(upd.body, "name") == "A2",
            "PUT /entities/{type}/{id} -> 200 updated name");

    // history
    auto hist = c.req("GET", "/tracelink/entities/requirement/" + reqId + "/history");
    h.check(hist.status == 200 && hasField(hist.body, "history"),
            "GET /entities/{type}/{id}/history -> 200");

    // delete
    auto del = c.req("DELETE", "/tracelink/entities/requirement/" + reqId);
    h.check(del.status == 200, "DELETE /entities/{type}/{id} -> 200");

    s.http.stop();
}

// ---------------------------------------------------------------------------
// 1 + 2. Links / impact / coverage / matrix + error model
// ---------------------------------------------------------------------------
void testLinksAndQueries(Harness& h) {
    h.section("1. /tracelink/links, impact, coverage, matrix + error model");

    Server s;
    if (!setupServer(s, "lodestar_wp6_links.db")) {
        h.check(false, "server start");
        return;
    }
    Client c("127.0.0.1", s.port);

    auto r = c.req("POST", "/tracelink/entities",
                   "{\"external_id\":\"LQ-REQ\",\"type\":\"requirement\","
                   "\"name\":\"R\",\"text\":\"t\",\"status\":\"Approved\"}");
    auto t = c.req("POST", "/tracelink/entities",
                   "{\"external_id\":\"LQ-TC\",\"type\":\"test_case\","
                   "\"name\":\"T\",\"text\":\"t\",\"status\":\"Draft\"}");
    std::string reqId = fieldStr(r.body, "id");
    std::string tcId = fieldStr(t.body, "id");

    // add link
    auto add = c.req("POST", "/tracelink/links",
                     "{\"source_type\":\"test_case\",\"source_id\":\"" + tcId +
                     "\",\"target_type\":\"requirement\",\"target_id\":\"" + reqId +
                     "\",\"relation\":\"verifies\"}");
    h.check(add.status == 200 && hasField(add.body, "id"),
            "POST /links -> 200 with id");

    // duplicate -> 400
    auto dup = c.req("POST", "/tracelink/links",
                     "{\"source_type\":\"test_case\",\"source_id\":\"" + tcId +
                     "\",\"target_type\":\"requirement\",\"target_id\":\"" + reqId +
                     "\",\"relation\":\"verifies\"}");
    h.check(dup.status == 400 && hasField(dup.body, "error.code"),
            "duplicate link -> 400 error model");

    // dangling -> 400
    auto dangling = c.req("POST", "/tracelink/links",
                          "{\"source_type\":\"test_case\",\"source_id\":\"" + tcId +
                          "\",\"target_type\":\"requirement\",\"target_id\":\"ghost\","
                          "\"relation\":\"verifies\"}");
    h.check(dangling.status == 400 && hasField(dangling.body, "error.code"),
            "dangling link -> 400 error model");

    // query links from a node
    auto q = c.req("GET", "/tracelink/links?sourceType=test_case&sourceId=" + tcId);
    h.check(q.status == 200 && hasField(q.body, "links"),
            "GET /links?sourceType=..&sourceId=.. -> 200");

    // impact / coverage / matrix
    auto impact = c.req("GET", "/tracelink/impact/requirement/" + reqId);
    h.check(impact.status == 200 && hasField(impact.body, "affected"),
            "GET /impact/{type}/{id} -> 200");
    auto coverage = c.req("GET", "/tracelink/coverage");
    h.check(coverage.status == 200 && hasField(coverage.body, "requirements"),
            "GET /coverage -> 200");
    auto matrix = c.req("GET", "/tracelink/matrix");
    h.check(matrix.status == 200 && hasField(matrix.body, "columns") &&
                hasField(matrix.body, "rows"),
            "GET /matrix -> 200");

    // unknown /tracelink route -> 404 error model
    auto nf = c.req("GET", "/tracelink/does/not/exist");
    h.check(nf.status == 404 && hasField(nf.body, "error.code"),
            "unknown /tracelink route -> 404 error model");

    s.http.stop();
}

// ---------------------------------------------------------------------------
// 1 + 2. Rules + baselines + import/export
// ---------------------------------------------------------------------------
void testRulesBaselinesIo(Harness& h) {
    h.section("1. /tracelink/validate, rules, baselines, import/export");

    Server s;
    if (!setupServer(s, "lodestar_wp6_io.db")) {
        h.check(false, "server start");
        return;
    }
    Client c("127.0.0.1", s.port);

    // validate
    auto validate = c.req("POST", "/tracelink/validate", "{}");
    h.check(validate.status == 200 && hasField(validate.body, "status") &&
                hasField(validate.body, "violations"),
            "POST /validate -> 200 with violations");
    auto rules = c.req("GET", "/tracelink/rules");
    h.check(rules.status == 200 && hasField(rules.body, "rules"),
            "GET /rules -> 200");

    // baselines
    auto b1 = c.req("POST", "/tracelink/baselines",
                    "{\"name\":\"B1\",\"description\":\"first\"}");
    h.check(b1.status == 200 && hasField(b1.body, "id"), "POST /baselines -> 200");
    std::string b1id = fieldStr(b1.body, "id");
    auto bl = c.req("GET", "/tracelink/baselines");
    h.check(bl.status == 200 && hasField(bl.body, "baselines"),
            "GET /baselines -> 200");

    // export routes
    auto expCsv = c.req("GET", "/tracelink/export/csv");
    h.check(expCsv.status == 200 && !expCsv.body.empty(), "GET /export/csv -> 200");
    auto expRif = c.req("GET", "/tracelink/export/reqif");
    h.check(expRif.status == 200 && !expRif.body.empty(), "GET /export/reqif -> 200");
    auto expHtml = c.req("GET", "/tracelink/export/html");
    h.check(expHtml.status == 200 && !expHtml.body.empty(), "GET /export/html -> 200");
    auto expBad = c.req("GET", "/tracelink/export/bogus");
    h.check(expBad.status == 400 && hasField(expBad.body, "error.code"),
            "GET /export/bogus -> 400 error model");

    // import route
    auto imp = c.req("POST", "/tracelink/import/csv",
                     "entity,requirement,IO-REQ,Imported,Body,Approved",
                     "text/csv");
    h.check(imp.status == 200 && hasField(imp.body, "batch_id") &&
                hasField(imp.body, "status"),
            "POST /import/csv -> 200 with batch_id");
    auto impBad = c.req("POST", "/tracelink/import/bogus", "x", "text/plain");
    h.check(impBad.status == 400 && hasField(impBad.body, "error.code"),
            "POST /import/bogus -> 400 error model");

    s.http.stop();
}

// ---------------------------------------------------------------------------
// 7. WP-6 acceptance chain
// ---------------------------------------------------------------------------
void testAcceptance(Harness& h) {
    h.section("7. WP-6 acceptance: entity -> link -> coverage -> validate -> baseline -> diff");

    Server s;
    if (!setupServer(s, "lodestar_wp6_accept.db")) {
        h.check(false, "server start");
        return;
    }
    Client c("127.0.0.1", s.port);

    // POST entity (requirement)
    auto req = c.req("POST", "/tracelink/entities",
                     "{\"external_id\":\"AC-REQ\",\"type\":\"requirement\","
                     "\"name\":\"R\",\"text\":\"t\",\"status\":\"Approved\"}");
    h.check(req.status == 200 && hasField(req.body, "id"), "POST entity -> 200");
    std::string reqId = fieldStr(req.body, "id");

    // POST entity (test_case)
    auto tc = c.req("POST", "/tracelink/entities",
                    "{\"external_id\":\"AC-TC\",\"type\":\"test_case\","
                    "\"name\":\"T\",\"text\":\"t\",\"status\":\"Draft\"}");
    h.check(tc.status == 200, "POST test entity -> 200");
    std::string tcId = fieldStr(tc.body, "id");

    // POST link (test verifies req)
    auto link = c.req("POST", "/tracelink/links",
                      "{\"source_type\":\"test_case\",\"source_id\":\"" + tcId +
                      "\",\"target_type\":\"requirement\",\"target_id\":\"" + reqId +
                      "\",\"relation\":\"verifies\"}");
    h.check(link.status == 200, "POST link -> 200");

    // GET coverage
    auto coverage = c.req("GET", "/tracelink/coverage");
    h.check(coverage.status == 200 && hasField(coverage.body, "requirements"),
            "GET coverage -> 200");

    // POST validate
    auto validate = c.req("POST", "/tracelink/validate", "{}");
    h.check(validate.status == 200 && hasField(validate.body, "violations"),
            "POST validate -> 200");

    // POST baseline (A)
    auto bA = c.req("POST", "/tracelink/baselines",
                    "{\"name\":\"A\",\"description\":\"before\"}");
    h.check(bA.status == 200, "POST baseline A -> 200");
    std::string aId = fieldStr(bA.body, "id");

    // modify the requirement so the diff is meaningful
    auto upd = c.req("PUT", "/tracelink/entities/requirement/" + reqId,
                     "{\"id\":\"" + reqId + "\",\"external_id\":\"AC-REQ\","
                     "\"type\":\"requirement\",\"name\":\"R2\",\"text\":\"t2\","
                     "\"status\":\"Verified\"}");
    h.check(upd.status == 200, "PUT entity (modify) -> 200");

    // POST baseline (B)
    auto bB = c.req("POST", "/tracelink/baselines",
                    "{\"name\":\"B\",\"description\":\"after\"}");
    h.check(bB.status == 200, "POST baseline B -> 200");
    std::string bId = fieldStr(bB.body, "id");

    // GET diff A against B
    auto diff = c.req("GET", "/tracelink/baselines/" + aId + "/diff?against=" + bId);
    h.check(diff.status == 200 && hasField(diff.body, "entities"),
            "GET diff -> 200 with entities");

    s.http.stop();
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("WP-6 REST API");
    std::printf("WP-6 REST API TESTS (migrations: %s)\n", g_migrationsDir.c_str());

    testEntities(h);
    testLinksAndQueries(h);
    testRulesBaselinesIo(h);
    testAcceptance(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
