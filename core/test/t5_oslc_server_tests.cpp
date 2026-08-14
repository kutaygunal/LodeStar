// core/test/t5_oslc_server_tests.cpp
// ---------------------------------------------------------------------------
// Gap-Fill TraceLink 3.5: OSLC integration ecosystem (server slice).
//
// Test contract: docs/gap-fill-plan.md (Module 3.5).
//   (A) OslcService already provides provider (export) + consumer (import).
//   (B) This adds the OSLC server slice: service discovery catalog,
//       resource-shape catalog, and query (requirements, optional title filter).
//
// Deterministic.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <string>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/tracelink/OslcService.h"
#include "core/tracelink/TraceLinkService.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace tl = lodestar::tracelink;
namespace p  = lodestar::persistence;

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

bool openFreshDb(p::Database& db, const char* file) {
    std::remove(file);
    std::remove((std::string(file) + "-wal").c_str());
    std::remove((std::string(file) + "-shm").c_str());
    if (db.open(file).failed()) return false;
    p::MigrationRunner runner(db);
    return runner.run(g_migrationsDir).isOk();
}

void closeAndRemove(p::Database& db, const char* file) {
    db.close();
    std::remove(file);
    std::remove((std::string(file) + "-wal").c_str());
    std::remove((std::string(file) + "-shm").c_str());
}

// ---------------------------------------------------------------------------
// T1. Service discovery catalog
// ---------------------------------------------------------------------------
void testServiceCatalog(Harness& h) {
    h.section("T1. OSLC service discovery catalog");
    p::Database db;
    if (!openFreshDb(db, "lodestar_t5_cat.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService tls(db);
    tl::OslcService oslc(tls);

    auto cat = oslc.serviceCatalog("http://host/oslc");
    h.check(cat.isOk(), "serviceCatalog() ok");
    if (cat.isOk()) {
        h.check(cat.value().find("oslc:ServiceProvider") != std::string::npos,
                "catalog has ServiceProvider");
        h.check(cat.value().find("http://host/oslc") != std::string::npos,
                "catalog carries the base URI");
        h.check(cat.value().find("/shape") != std::string::npos,
                "catalog advertises resource shape");
        h.check(cat.value().find("/query") != std::string::npos,
                "catalog advertises query base");
        h.check(cat.value().find("oslc:Service") != std::string::npos,
                "catalog lists a service");
    }

    auto bad = oslc.serviceCatalog("");
    h.check(bad.failed(), "serviceCatalog() with empty base URI fails");

    closeAndRemove(db, "lodestar_t5_cat.db");
}

// ---------------------------------------------------------------------------
// T2. Resource-shape catalog
// ---------------------------------------------------------------------------
void testResourceShape(Harness& h) {
    h.section("T2. OSLC resource-shape catalog");
    p::Database db;
    if (!openFreshDb(db, "lodestar_t5_shape.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService tls(db);
    tl::OslcService oslc(tls);

    auto shape = oslc.resourceShape();
    h.check(shape.isOk(), "resourceShape() ok");
    if (shape.isOk()) {
        h.check(shape.value().find("oslc:ResourceShape") != std::string::npos,
                "shape has ResourceShape");
        h.check(shape.value().find("oslc:Property") != std::string::npos,
                "shape lists properties");
        h.check(shape.value().find("<oslc:name>identifier</oslc:name>") != std::string::npos,
                "shape declares the identifier property");
        h.check(shape.value().find("<oslc:name>title</oslc:name>") != std::string::npos,
                "shape declares the title property");
        h.check(shape.value().find("Exactly-one") != std::string::npos,
                "identifier/title are required (Exactly-one)");
    }

    closeAndRemove(db, "lodestar_t5_shape.db");
}

// ---------------------------------------------------------------------------
// T3. Query: all + title filter
// ---------------------------------------------------------------------------
void testQuery(Harness& h) {
    h.section("T3. OSLC query (all + title filter)");
    p::Database db;
    if (!openFreshDb(db, "lodestar_t5_query.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService tls(db);
    // Seed two requirements.
    tl::Entity r1;
    r1.type = tl::EntityType::Requirement;
    r1.externalId = "REQ-1";
    r1.name = "Acquisition";
    r1.status = "Draft";
    tl::Entity r2;
    r2.type = tl::EntityType::Requirement;
    r2.externalId = "REQ-2";
    r2.name = "Tracking";
    r2.status = "Draft";
    h.check(tls.addEntity(r1).isOk(), "add REQ-1");
    h.check(tls.addEntity(r2).isOk(), "add REQ-2");

    tl::OslcService oslc(tls);

    // Query all -> both members.
    auto all = oslc.queryRequirements("");
    h.check(all.isOk(), "queryRequirements() ok");
    if (all.isOk()) {
        h.check(all.value().find("oslc:QueryResult") != std::string::npos,
                "query returns a QueryResult");
        h.check(all.value().find("rdf:member") != std::string::npos,
                "query has members");
        h.check(all.value().find("REQ-1") != std::string::npos,
                "REQ-1 in results");
        h.check(all.value().find("REQ-2") != std::string::npos,
                "REQ-2 in results");
    }

    // Query with title filter -> only the matching requirement.
    auto filtered = oslc.queryRequirements("track");
    h.check(filtered.isOk(), "queryRequirements(\"track\") ok");
    if (filtered.isOk()) {
        h.check(filtered.value().find("REQ-2") != std::string::npos,
                "filtered query includes REQ-2 (title Tracking)");
        h.check(filtered.value().find("REQ-1") == std::string::npos,
                "filtered query excludes REQ-1 (title Acquisition)");
    }

    closeAndRemove(db, "lodestar_t5_query.db");
}

// ---------------------------------------------------------------------------
// T4. Round-trip: export -> import via OSLC + query sees it
// ---------------------------------------------------------------------------
void testRoundTrip(Harness& h) {
    h.section("T4. provider/consumer round-trip through the server slice");
    p::Database db;
    if (!openFreshDb(db, "lodestar_t5_rt.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService tls(db);
    tl::Entity r;
    r.type = tl::EntityType::Requirement;
    r.externalId = "REQ-100";
    r.name = "Acquire signals";
    r.status = "Draft";
    auto added = tls.addEntity(r);
    h.check(added.isOk(), "add REQ-100");
    const std::string id = added.value().id;

    tl::OslcService oslc(tls);
    auto exported = oslc.exportRequirementAsOslc(id);
    h.check(exported.isOk(), "export as OSLC ok");

    // Consumer re-imports (idempotent update).
    auto imported = oslc.importRequirementFromOslc(exported.value());
    h.check(imported.isOk(), "import from OSLC ok");
    if (imported.isOk()) {
        h.check(imported.value().externalId == "REQ-100",
                "imported requirement keeps external id");
    }

    // Query sees the requirement.
    auto q = oslc.queryRequirements("acquire");
    h.check(q.isOk() && q.value().find("REQ-100") != std::string::npos,
            "query returns the round-tripped requirement");

    closeAndRemove(db, "lodestar_t5_rt.db");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) g_migrationsDir = argv[1];

    Harness h("Gap-Fill TraceLink 3.5 OSLC integration ecosystem");
    testServiceCatalog(h);
    testResourceShape(h);
    testQuery(h);
    testRoundTrip(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
