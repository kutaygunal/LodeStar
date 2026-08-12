// core/test/s2_phase12_tests.cpp
// ---------------------------------------------------------------------------
// Sprint 2 Phase 12 (OSLC integration) unit tests.
//
// Written by the scrum-master BEFORE the Phase 12 engineer implements the
// feature. The engineer must implement the contract documented below so these
// tests compile and pass. Do NOT weaken the assertions to make them pass;
// implement the feature to satisfy them.
//
// Covers (docs/s2-phase12-test.md): an OSLC provider that exposes requirements
// as standard OSLC RM resources (RDF/XML, dcterms + oslc_rm namespaces) and an
// OSLC consumer that imports an OSLC requirement resource into the local
// TraceLink model.
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / S1 phases.
// ---------------------------------------------------------------------------
// CONTRACT the Phase 12 engineer must provide (in lodestar::tracelink):
//   class OslcService {
//     explicit OslcService(TraceLinkService& svc);
//     common::Result<std::string> exportRequirementAsOslc(const std::string& requirementId);
//     common::Result<Entity>      importRequirementFromOslc(const std::string& oslcResource);
//   };
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

tl::Entity makeReq(const std::string& extId, const std::string& title) {
    tl::Entity e;
    e.externalId = extId;
    e.type = tl::EntityType::Requirement;
    e.name = title;
    e.text = "The system shall provide GNSS position output.";
    e.status = "Draft";
    e.owner = "engineer";
    e.verificationMethod = "test";
    e.safetyLevel = "Level A";
    return e;
}

// ---------------------------------------------------------------------------
// T1. OSLC export contains identifier + title + oslc_rm namespace
// ---------------------------------------------------------------------------
void testExportContainsFields(p::Database& db, Harness& h) {
    h.section("T1. OSLC export contains identifier + title + oslc_rm namespace");

    tl::TraceLinkService svc(db);
    tl::OslcService oslc(svc);

    auto created = svc.addEntity(makeReq("REQ-100", "Position output"));
    h.check(created.isOk(), "add requirement ok");
    const std::string reqId = created.value().id;

    auto exported = oslc.exportRequirementAsOslc(reqId);
    h.check(exported.isOk(), "exportRequirementAsOslc ok");
    const std::string& xml = exported.value();

    h.check(xml.find("REQ-100") != std::string::npos,
            "export contains the requirement identifier");
    h.check(xml.find("Position output") != std::string::npos,
            "export contains the requirement title");
    h.check(xml.find("oslc_rm") != std::string::npos,
            "export references the oslc_rm namespace");
}

// ---------------------------------------------------------------------------
// T2. OSLC export is well-formed (dcterms:identifier + dcterms:title)
// ---------------------------------------------------------------------------
void testExportWellFormed(p::Database& db, Harness& h) {
    h.section("T2. OSLC export is well-formed (dcterms fields)");

    tl::TraceLinkService svc(db);
    tl::OslcService oslc(svc);

    auto created = svc.addEntity(makeReq("REQ-200", "Altitude accuracy"));
    h.check(created.isOk(), "add requirement ok");
    const std::string reqId = created.value().id;

    auto exported = oslc.exportRequirementAsOslc(reqId);
    h.check(exported.isOk(), "exportRequirementAsOslc ok");
    const std::string& xml = exported.value();

    h.check(xml.find("dcterms:identifier") != std::string::npos,
            "export contains a dcterms:identifier field");
    h.check(xml.find("dcterms:title") != std::string::npos,
            "export contains a dcterms:title field");
    h.check(xml.find("oslc_rm:Requirement") != std::string::npos,
            "export types the resource as oslc_rm:Requirement");
}

// ---------------------------------------------------------------------------
// T3. OSLC import creates a local requirement
// ---------------------------------------------------------------------------
void testImportCreatesRequirement(p::Database& db, Harness& h) {
    h.section("T3. OSLC import creates a local requirement");

    tl::TraceLinkService svc(db);
    tl::OslcService oslc(svc);

    const std::string resource =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\"\n"
        "         xmlns:dcterms=\"http://purl.org/dc/terms/\"\n"
        "         xmlns:oslc_rm=\"http://open-services.net/ns/rm#\">\n"
        "  <oslc_rm:Requirement rdf:about=\"REQ-300\">\n"
        "    <dcterms:identifier>REQ-300</dcterms:identifier>\n"
        "    <dcterms:title>Velocity limit</dcterms:title>\n"
        "  </oslc_rm:Requirement>\n"
        "</rdf:RDF>\n";

    auto imported = oslc.importRequirementFromOslc(resource);
    h.check(imported.isOk(), "importRequirementFromOslc ok");
    h.check(imported.value().externalId == "REQ-300",
            "imported requirement external id matches the resource");
    h.check(imported.value().name == "Velocity limit",
            "imported requirement title matches the resource");

    // The requirement is persisted in the model.
    auto found = svc.getEntity(tl::EntityType::Requirement, imported.value().id);
    h.check(found.isOk() && found.value().has_value(),
            "imported requirement is retrievable from the model");
    h.check(found.value()->externalId == "REQ-300",
            "retrieved requirement external id matches");
}

// ---------------------------------------------------------------------------
// T4. round-trip export -> import preserves data
// ---------------------------------------------------------------------------
void testRoundTrip(p::Database& db, Harness& h) {
    h.section("T4. round-trip export -> import preserves data");

    tl::TraceLinkService svc(db);
    tl::OslcService oslc(svc);

    auto created = svc.addEntity(makeReq("REQ-400", "Signal acquisition time"));
    h.check(created.isOk(), "add requirement ok");
    const std::string reqId = created.value().id;

    auto exported = oslc.exportRequirementAsOslc(reqId);
    h.check(exported.isOk(), "exportRequirementAsOslc ok");

    auto imported = oslc.importRequirementFromOslc(exported.value());
    h.check(imported.isOk(), "importRequirementFromOslc ok");
    h.check(imported.value().externalId == "REQ-400",
            "round-trip preserves the identifier");
    h.check(imported.value().name == "Signal acquisition time",
            "round-trip preserves the title");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::string migrationsDir = LODESTAR_MIGRATIONS_DIR;
    if (argc > 1) {
        migrationsDir = argv[1];
    }

    const std::string dbPath = "lodestar_s2_phase12_tests.db";
    std::remove(dbPath.c_str());
    std::remove((dbPath + "-wal").c_str());
    std::remove((dbPath + "-shm").c_str());

    p::Database db;
    auto open = db.open(dbPath);
    if (open.failed()) {
        std::fprintf(stderr, "S2 PHASE12 TESTS FAIL: open db: %s\n",
                     open.error().c_str());
        return 1;
    }

    p::MigrationRunner runner(db);
    auto mig = runner.run(migrationsDir);
    if (mig.failed()) {
        std::fprintf(stderr, "S2 PHASE12 TESTS FAIL: migrate: %s\n",
                     mig.error().c_str());
        db.close();
        std::remove(dbPath.c_str());
        return 1;
    }

    Harness h("S2 Phase 12 OSLC integration");
    std::printf("S2 PHASE 12 OSLC INTEGRATION TESTS (schema v%d)\n", mig.value());

    testExportContainsFields(db, h);
    testExportWellFormed(db, h);
    testImportCreatesRequirement(db, h);
    testRoundTrip(db, h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());

    db.close();
    std::remove(dbPath.c_str());
    std::remove((dbPath + "-wal").c_str());
    std::remove((dbPath + "-shm").c_str());

    return h.failures() == 0 ? 0 : 1;
}
