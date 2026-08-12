// core/test/wp6_assurecheck_tests.cpp
// ---------------------------------------------------------------------------
// Phase 11 WP-6 (AssureCheck) REST API + compliance dashboard tests
// (test-first).
//
// Written by the scrum-master BEFORE the WP-6 engineer implements the feature.
// The engineer must implement the contract documented below so these tests
// compile and pass. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// Covers (docs/wp6-assurecheck-task.md): the /assurecheck REST endpoints
// (standards, checks, summary, dashboard) and the DashboardService
// (per-standard objective coverage + status).
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G.
// The API tests drive a real in-process HTTP server over localhost with
// HttpClient, exactly like the Phase-10 wp6_api_tests.cpp.
// ---------------------------------------------------------------------------
// CONTRACT the WP-6 engineer must provide.
// ---------------------------------------------------------------------------
// (A) core/assurecheck/DashboardService.h (+ .cpp):
//       struct DashboardStandard { std::string code; std::string name;
//                                  CoverageSummary coverage; };
//       class DashboardService {
//           explicit DashboardService(persistence::Database& db);
//           common::Result<std::vector<DashboardStandard>> dashboard();
//       };
// (B) core/api/AssureCheckApiServer.h (+ .cpp):
//       class AssureCheckApiServer {
//           AssureCheckApiServer(assurecheck::AssureCheckService& standards,
//                                assurecheck::ComplianceEngine& engine,
//                                assurecheck::ReportService& reports,
//                                assurecheck::DashboardService& dashboard);
//           void setup(HttpServer& server);
//       };
// (C) Routes:
//       GET  /assurecheck/standards            -> 200 {"standards":[...]}
//       GET  /assurecheck/standards/{code}     -> 200 | 404
//       POST /assurecheck/checks               -> 200 {"results":[...]} | 400
//       GET  /assurecheck/summary?standard=    -> 200 {"summary":{...}} | 400
//       GET  /assurecheck/dashboard            -> 200 {"standards":[...]}
//     Error model for 400/404/500:
//       {"error":{"code":<int>,"message":"..."}}
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "core/adapters/HttpClient.h"
#include "core/adapters/Json.h"
#include "core/api/AssureCheckApiServer.h"
#include "core/api/HttpServer.h"
#include "core/assurecheck/AssureCheckService.h"
#include "core/assurecheck/ComplianceEngine.h"
#include "core/assurecheck/DashboardService.h"
#include "core/assurecheck/ReportService.h"
#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace ac = lodestar::assurecheck;
namespace ap = lodestar::api;
namespace p  = lodestar::persistence;

namespace {

std::string g_migrationsDir = LODESTAR_MIGRATIONS_DIR;

// Opens a fresh throwaway DB for one test, runs migrations, returns true on ok.
bool openFreshDb(p::Database& db, const char* file) {
    std::remove(file);
    std::remove((std::string(file) + "-wal").c_str());
    std::remove((std::string(file) + "-shm").c_str());
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
// Server fixture: fresh DB + AssureCheck services + routes on an ephemeral port.
// ---------------------------------------------------------------------------
struct Server {
    p::Database db;
    std::unique_ptr<ac::AssureCheckService> standards;
    std::unique_ptr<ac::ComplianceEngine> engine;
    std::unique_ptr<ac::ReportService> reports;
    std::unique_ptr<ac::DashboardService> dashboard;
    std::unique_ptr<ap::AssureCheckApiServer> api;
    ap::HttpServer http;
    int port = -1;
};

bool setupServer(Server& s, const char* dbfile) {
    std::remove(dbfile);
    if (s.db.open(dbfile).failed()) return false;
    p::MigrationRunner runner(s.db);
    if (runner.run(g_migrationsDir).failed()) return false;

    s.standards = std::make_unique<ac::AssureCheckService>(s.db);
    s.engine = std::make_unique<ac::ComplianceEngine>(s.db);
    s.reports = std::make_unique<ac::ReportService>(s.db);
    s.dashboard = std::make_unique<ac::DashboardService>(s.db);

    s.api = std::make_unique<ap::AssureCheckApiServer>(*s.standards, *s.engine,
                                                       *s.reports, *s.dashboard);
    s.api->setup(s.http);
    if (!s.http.start(0)) return false;
    s.port = s.http.port();
    return s.port > 0;
}

// Seeds the five standards; returns true on success.
bool seedStandards(p::Database& db) {
    ac::AssureCheckService svc(db);
    return svc.seedStandards().isOk();
}

// Inserts the T3 project data: one requirements row, one design_items row,
// one test_cases row (result_status='Passed'), and one trace_links row.
void insertT3Data(p::Database& db) {
    db.execute("INSERT INTO requirements (id, name) VALUES ('req1', 'Req 1');");
    db.execute("INSERT INTO design_items (id, name) VALUES ('des1', 'Des 1');");
    db.execute("INSERT INTO test_cases (id, name, result_status) "
               "VALUES ('tc1', 'TC 1', 'Passed');");
    db.execute("INSERT INTO trace_links (id, source_type, source_id, "
               "target_type, target_id) "
               "VALUES ('tl1', 'requirement', 'req1', 'design', 'des1');");
}

// ---------------------------------------------------------------------------
// T1. GET /assurecheck/standards returns the five standards
// ---------------------------------------------------------------------------
void testStandards(Harness& h) {
    h.section("T1. GET /assurecheck/standards returns the five standards");

    Server s;
    if (!setupServer(s, "lodestar_wp6_ac_standards.db")) {
        h.check(false, "server start");
        return;
    }
    if (!seedStandards(s.db)) {
        h.check(false, "seedStandards() ok");
        s.http.stop();
        return;
    }
    Client c("127.0.0.1", s.port);

    auto r = c.req("GET", "/assurecheck/standards");
    h.check(r.status == 200 && hasField(r.body, "standards"),
            "GET /assurecheck/standards -> 200 with standards");
    if (r.status == 200) {
        try {
            lodestar::Json j = lodestar::Json::parse(r.body);
            h.check(j["standards"].size() == 5, "standards array has 5 entries");
        } catch (...) {
            h.check(false, "standards array parse");
        }
    }

    s.http.stop();
}

// ---------------------------------------------------------------------------
// T2. GET /assurecheck/standards/{code} returns a standard; missing -> 404
// ---------------------------------------------------------------------------
void testStandardGet(Harness& h) {
    h.section("T2. GET /assurecheck/standards/{code} returns a standard; missing -> 404");

    Server s;
    if (!setupServer(s, "lodestar_wp6_ac_stdget.db")) {
        h.check(false, "server start");
        return;
    }
    if (!seedStandards(s.db)) {
        h.check(false, "seedStandards() ok");
        s.http.stop();
        return;
    }
    Client c("127.0.0.1", s.port);

    auto ok = c.req("GET", "/assurecheck/standards/DO-178C");
    h.check(ok.status == 200 && fieldStr(ok.body, "code") == "DO-178C",
            "GET /assurecheck/standards/DO-178C -> 200 code DO-178C");

    auto nf = c.req("GET", "/assurecheck/standards/NOPE");
    h.check(nf.status == 404 && hasField(nf.body, "error.code"),
            "GET /assurecheck/standards/NOPE -> 404 error model");

    s.http.stop();
}

// ---------------------------------------------------------------------------
// T3. POST /assurecheck/checks runs + stores results; missing standard -> 400
// ---------------------------------------------------------------------------
void testChecks(Harness& h) {
    h.section("T3. POST /assurecheck/checks runs + stores results; missing standard -> 400");

    Server s;
    if (!setupServer(s, "lodestar_wp6_ac_checks.db")) {
        h.check(false, "server start");
        return;
    }
    if (!seedStandards(s.db)) {
        h.check(false, "seedStandards() ok");
        s.http.stop();
        return;
    }
    insertT3Data(s.db);
    Client c("127.0.0.1", s.port);

    auto run = c.req("POST", "/assurecheck/checks",
                     "{\"standard\":\"DO-178C\",\"dal\":\"A\"}");
    h.check(run.status == 200 && hasField(run.body, "results"),
            "POST /assurecheck/checks -> 200 with results");
    if (run.status == 200) {
        try {
            lodestar::Json j = lodestar::Json::parse(run.body);
            h.check(j["results"].size() == 82, "results array has 82 entries");
        } catch (...) {
            h.check(false, "results array parse");
        }
    }

    auto bad = c.req("POST", "/assurecheck/checks", "{\"dal\":\"A\"}");
    h.check(bad.status == 400 && hasField(bad.body, "error.code"),
            "POST /assurecheck/checks missing standard -> 400 error model");

    s.http.stop();
}

// ---------------------------------------------------------------------------
// T4. GET /assurecheck/summary?standard= returns summary counts
// ---------------------------------------------------------------------------
void testSummary(Harness& h) {
    h.section("T4. GET /assurecheck/summary?standard= returns summary counts");

    Server s;
    if (!setupServer(s, "lodestar_wp6_ac_summary.db")) {
        h.check(false, "server start");
        return;
    }
    if (!seedStandards(s.db)) {
        h.check(false, "seedStandards() ok");
        s.http.stop();
        return;
    }
    insertT3Data(s.db);
    Client c("127.0.0.1", s.port);

    c.req("POST", "/assurecheck/checks", "{\"standard\":\"DO-178C\",\"dal\":\"A\"}");

    auto sum = c.req("GET", "/assurecheck/summary?standard=DO-178C");
    h.check(sum.status == 200 && hasField(sum.body, "summary"),
            "GET /assurecheck/summary?standard=DO-178C -> 200 with summary");
    if (sum.status == 200) {
        try {
            lodestar::Json j = lodestar::Json::parse(sum.body);
            h.check(j["summary"]["total"].asNumber() == 82, "summary.total == 82");
            h.check(j["summary"]["na"].asNumber() == 0, "summary.na == 0");
            h.check(j["summary"]["pass"].asNumber() == 82, "summary.pass == 82");
        } catch (...) {
            h.check(false, "summary parse");
        }
    }

    auto bad = c.req("GET", "/assurecheck/summary");
    h.check(bad.status == 400 && hasField(bad.body, "error.code"),
            "GET /assurecheck/summary without standard -> 400 error model");

    s.http.stop();
}

// ---------------------------------------------------------------------------
// T5. GET /assurecheck/dashboard returns per-standard coverage
// ---------------------------------------------------------------------------
void testDashboard(Harness& h) {
    h.section("T5. GET /assurecheck/dashboard returns per-standard coverage");

    Server s;
    if (!setupServer(s, "lodestar_wp6_ac_dash.db")) {
        h.check(false, "server start");
        return;
    }
    if (!seedStandards(s.db)) {
        h.check(false, "seedStandards() ok");
        s.http.stop();
        return;
    }
    insertT3Data(s.db);
    Client c("127.0.0.1", s.port);

    c.req("POST", "/assurecheck/checks", "{\"standard\":\"DO-178C\",\"dal\":\"A\"}");

    auto dash = c.req("GET", "/assurecheck/dashboard");
    h.check(dash.status == 200 && hasField(dash.body, "standards"),
            "GET /assurecheck/dashboard -> 200 with standards");
    if (dash.status == 200) {
        try {
            lodestar::Json j = lodestar::Json::parse(dash.body);
            bool found = false;
            for (auto& st : j["standards"].asArray()) {
                if (st["code"].asString() == "DO-178C" &&
                    st["coverage"]["total"].asNumber() == 82) {
                    found = true;
                }
            }
            h.check(found, "dashboard contains DO-178C with coverage.total == 82");
        } catch (...) {
            h.check(false, "dashboard parse");
        }
    }

    s.http.stop();
}

// ---------------------------------------------------------------------------
// T6. DashboardService.dashboard() returns per-standard coverage from stored results
// ---------------------------------------------------------------------------
void testDashboardService(Harness& h) {
    h.section("T6. DashboardService.dashboard() returns per-standard coverage from stored results");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp6_ac_dashsvc.db")) {
        h.check(false, "open fresh db");
        return;
    }
    if (!seedStandards(db)) {
        h.check(false, "seedStandards() ok");
        db.close();
        return;
    }
    insertT3Data(db);

    ac::ComplianceEngine engine(db);
    auto res = engine.runChecks("DO-178C", "A");
    h.check(res.isOk(), "runChecks(\"DO-178C\", \"A\") ok");
    if (!res.isOk()) {
        db.close();
        return;
    }
    auto stored = engine.storeResults(res.value());
    h.check(stored.isOk(), "storeResults ok");
    if (stored.failed()) {
        db.close();
        return;
    }

    ac::DashboardService dash(db);
    auto d = dash.dashboard();
    h.check(d.isOk(), "dashboard() ok");
    if (!d.isOk()) {
        db.close();
        return;
    }
    h.check(d.value().size() == 1, "dashboard returns one entry");
    if (d.value().size() == 1) {
        const auto& st = d.value()[0];
        h.check(st.code == "DO-178C", "entry code is DO-178C");
        h.check(st.coverage.total == 82, "coverage.total == 82");
        h.check(st.coverage.pass == 82, "coverage.pass == 82");
        h.check(st.coverage.percent == 100, "coverage.percent == 100");
    }

    db.close();
    std::remove("lodestar_wp6_ac_dashsvc.db");
    std::remove("lodestar_wp6_ac_dashsvc.db-wal");
    std::remove("lodestar_wp6_ac_dashsvc.db-shm");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("WP-6 AssureCheck REST API + compliance dashboard");
    std::printf("WP-6 ASSURECHECK REST API + DASHBOARD TESTS (migrations: %s)\n",
                g_migrationsDir.c_str());

    testStandards(h);
    testStandardGet(h);
    testChecks(h);
    testSummary(h);
    testDashboard(h);
    testDashboardService(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
