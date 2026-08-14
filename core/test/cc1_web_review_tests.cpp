// core/test/cc1_web_review_tests.cpp
// ---------------------------------------------------------------------------
// Gap-Fill Cross-cutting #1: multi-user web/review layer tests.
//
// Test contract: docs/gap-fill-plan.md (Cross-cutting #1). An interactive,
// editable JS frontend + server backend where every module exposes a REST/web
// route so the web layer can drive it. Verifies the web layer serves review
// HTML for the new modules (RiskAI FMEA, IntegrateHub PR/CR) in addition to the
// existing requirements/trace/assure pages.
//
// Deterministic: local HttpServer on an ephemeral port.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

#include "core/api/HttpServer.h"
#include "core/api/WebServer.h"
#include "core/adapters/HttpClient.h"
#include "core/integratehub/ImpactAnalysisService.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/riskai/FmeaWorkflowService.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace ap = lodestar::api;
namespace p  = lodestar::persistence;
namespace ra = lodestar::riskai;
namespace ih = lodestar::integratehub;

namespace {

std::string g_migrationsDir = LODESTAR_MIGRATIONS_DIR;

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

struct Server {
    p::Database db;
    std::unique_ptr<ap::WebServer> web;
    ap::HttpServer http;
    int port = -1;
};

lodestar::adapters::HttpClient::Response req(const std::string& host, int port,
                                             const std::string& path) {
    std::string err;
    return lodestar::adapters::HttpClient::request(host, port, "GET", path, "",
                                                   "text/html", 4000, &err);
}

bool setupServer(Server& s, const char* dbfile) {
    std::remove(dbfile);
    if (s.db.open(dbfile).failed()) return false;
    p::MigrationRunner runner(s.db);
    if (runner.run(g_migrationsDir).failed()) return false;

    // Seed an FMEA workflow and a PR/CR so the web layer has data to render.
    ra::FmeaWorkflowService fmea(s.db);
    ra::FmeaWorkflow wf;
    wf.name = "Receiver FMEA";
    wf.system = "GNSS receiver";
    fmea.createWorkflow(wf);

    ih::ImpactAnalysisService impact(s.db);
    ih::ProblemReport pr;
    pr.title = "GPS lock lost";
    impact.createPr(pr);
    ih::ChangeRequest cr;
    cr.title = "Change threshold";
    cr.entityType = "requirement";
    cr.entityId = "REQ-100";
    impact.createCr(cr);

    s.web = std::make_unique<ap::WebServer>(s.db);
    s.web->setup(s.http);
    if (!s.http.start(0)) return false;
    s.port = s.http.port();
    return s.port > 0;
}

}  // namespace

// ---------------------------------------------------------------------------
// T1. Landing page lists every module
// ---------------------------------------------------------------------------
static void testLanding(Harness& h) {
    h.section("T1. landing page drives every module");
    Server s;
    if (!setupServer(s, "lodestar_cc1_root.db")) {
        h.check(false, "server start");
        return;
    }
    auto r = req("127.0.0.1", s.port, "/web/");
    h.check(r.status == 200, "GET /web/ -> 200");
    if (r.status == 200) {
        h.check(r.body.find("/web/requirements") != std::string::npos,
                "landing links to Requirements");
        h.check(r.body.find("/web/trace") != std::string::npos,
                "landing links to Trace");
        h.check(r.body.find("/web/assure") != std::string::npos,
                "landing links to AssureCheck");
        h.check(r.body.find("/web/riskai") != std::string::npos,
                "landing links to RiskAI (module web route)");
        h.check(r.body.find("/web/integratehub") != std::string::npos,
                "landing links to IntegrateHub (module web route)");
    }
    s.http.stop();
    std::remove("lodestar_cc1_root.db");
}

// ---------------------------------------------------------------------------
// T2. RiskAI web route renders FMEA workflows
// ---------------------------------------------------------------------------
static void testRiskai(Harness& h) {
    h.section("T2. RiskAI web route");
    Server s;
    if (!setupServer(s, "lodestar_cc1_riskai.db")) {
        h.check(false, "server start");
        return;
    }
    auto r = req("127.0.0.1", s.port, "/web/riskai");
    h.check(r.status == 200, "GET /web/riskai -> 200");
    if (r.status == 200) {
        h.check(r.body.find("RiskAI FMEA Workflows") != std::string::npos,
                "riskai page title present");
        h.check(r.body.find("Receiver FMEA") != std::string::npos,
                "riskai page lists the seeded workflow");
        h.check(r.body.find("GNSS receiver") != std::string::npos,
                "riskai page shows the system");
    }
    s.http.stop();
    std::remove("lodestar_cc1_riskai.db");
}

// ---------------------------------------------------------------------------
// T3. IntegrateHub web route renders PR/CR
// ---------------------------------------------------------------------------
static void testIntegratehub(Harness& h) {
    h.section("T3. IntegrateHub web route");
    Server s;
    if (!setupServer(s, "lodestar_cc1_ih.db")) {
        h.check(false, "server start");
        return;
    }
    auto r = req("127.0.0.1", s.port, "/web/integratehub");
    h.check(r.status == 200, "GET /web/integratehub -> 200");
    if (r.status == 200) {
        h.check(r.body.find("Problem Reports") != std::string::npos,
                "page has Problem Reports section");
        h.check(r.body.find("Change Requests") != std::string::npos,
                "page has Change Requests section");
        h.check(r.body.find("GPS lock lost") != std::string::npos,
                "page lists the seeded problem report");
        h.check(r.body.find("Change threshold") != std::string::npos,
                "page lists the seeded change request");
    }
    s.http.stop();
    std::remove("lodestar_cc1_ih.db");
}

// ---------------------------------------------------------------------------
// T4. Auth: an invalid session token is rejected with 401
// ---------------------------------------------------------------------------
static void testAuth(Harness& h) {
    h.section("T4. auth-aware review layer");
    Server s;
    if (!setupServer(s, "lodestar_cc1_auth.db")) {
        h.check(false, "server start");
        return;
    }
    std::string err;
    auto r = lodestar::adapters::HttpClient::request(
        "127.0.0.1", s.port, "GET", "/web/riskai", "", "text/html", 4000, &err,
        "Authorization: Bearer invalid-token");
    // An explicit but invalid session token is rejected (401), matching the
    // documented auth model (public read-only only when NO token is supplied).
    h.check(r.status == 401, "invalid session token rejected with 401");
    s.http.stop();
    std::remove("lodestar_cc1_auth.db");
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) g_migrationsDir = argv[1];

    Harness h("Gap-Fill Cross-cutting #1 multi-user web/review layer");
    testLanding(h);
    testRiskai(h);
    testIntegratehub(h);
    testAuth(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
