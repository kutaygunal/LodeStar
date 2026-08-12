// core/test/wpD_tests.cpp
// ---------------------------------------------------------------------------
// WP-D unit/integration tests (test-first).
//
// Written by the scrum-master BEFORE the WP-D engineer implements the feature.
// The engineer must implement the contract documented below so these tests
// compile and pass. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// Covers (PLAN.md, WP-D):
//   A5. Coverage by verification method.
//   A6. DO-178C evidence package export (matrix + coverage + validation +
//       audit bundle).
//
// Uses the same lightweight self-contained harness as WP-1..WP-8. Each
// DB-dependent test opens its own fresh throwaway DB.
//
// ---------------------------------------------------------------------------
// CONTRACT the WP-D engineer must provide.
// ---------------------------------------------------------------------------
// (A) GraphEngine addition (core/tracelink/GraphEngine.h):
//
//   // One requirement's coverage row broken down by verification method.
//   struct CoverageByMethodRow {
//       std::string requirementId;
//       std::string requirementExternalId;
//       std::string verificationMethod;  // from the requirement (test/analysis/...)
//       bool verified = false;           // has >=1 Active verifies link
//   };
//
//   // One row per requirement, reporting its verification method and whether
//   // it is verified. Lets an auditor see coverage per method.
//   common::Result<std::vector<CoverageByMethodRow>> coverageByMethod();
//
// (B) IoService addition (core/tracelink/IoService.h):
//
//   // Exports a DO-178C evidence package to `dir` (created if missing):
//   //   matrix.csv, coverage.csv, coverage_by_method.csv, validation.json,
//   //   audit.csv, manifest.json
//   // Each file is non-empty on success. manifest.json lists the files.
//   common::Result<void> exportEvidencePackage(const std::string& dir);
// ---------------------------------------------------------------------------

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/persistence/Models.h"
#include "core/persistence/daos.h"
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

tl::Entity makeReq(const std::string& extId, const std::string& method) {
    tl::Entity e;
    e.externalId = extId;
    e.type = tl::EntityType::Requirement;
    e.name = extId;
    e.text = "Body of " + extId;
    e.status = "Approved";
    e.verificationMethod = method;
    return e;
}

tl::Entity makeTc(const std::string& extId) {
    tl::Entity e;
    e.externalId = extId;
    e.type = tl::EntityType::TestCase;
    e.name = extId;
    e.text = "Verify " + extId;
    e.status = "Draft";
    return e;
}

// Returns true if the file exists and is non-empty.
bool fileNonEmpty(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    in.seekg(0, std::ios::end);
    return in.tellg() > 0;
}

// ---------------------------------------------------------------------------
// A5. Coverage by verification method
// ---------------------------------------------------------------------------
void testCoverageByMethod(Harness& h) {
    h.section("A5. Coverage by verification method");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wpD_method.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::GraphEngine graph(db);

    // Two requirements: one verified by test, one by analysis (unverified).
    auto rTest = svc.addEntity(makeReq("REQ-M1", "test"));
    auto rAnalysis = svc.addEntity(makeReq("REQ-M2", "analysis"));
    auto tc = svc.addEntity(makeTc("TC-M1"));
    h.check(rTest.isOk() && rAnalysis.isOk() && tc.isOk(), "seed entities ok");
    const std::string rTestId = rTest.value().id;
    const std::string rAnalysisId = rAnalysis.value().id;
    const std::string tcId = tc.value().id;

    // Verify REQ-M1 with a test case.
    tl::Link l;
    l.sourceType = tl::EntityType::TestCase;
    l.sourceId = tcId;
    l.targetType = tl::EntityType::Requirement;
    l.targetId = rTestId;
    l.relation = "verifies";
    h.check(svc.addLink(l).isOk(), "add verifies link to REQ-M1 ok");

    auto rows = graph.coverageByMethod();
    h.check(rows.isOk(), "coverageByMethod ok");
    h.check(rows.value().size() == 2, "one row per requirement");

    bool m1Verified = false, m1MethodOk = false;
    bool m2Unverified = false, m2MethodOk = false;
    for (const auto& row : rows.value()) {
        if (row.requirementExternalId == "REQ-M1") {
            m1Verified = row.verified;
            m1MethodOk = row.verificationMethod == "test";
        } else if (row.requirementExternalId == "REQ-M2") {
            m2Unverified = !row.verified;
            m2MethodOk = row.verificationMethod == "analysis";
        }
    }
    h.check(m1Verified, "REQ-M1 (test) is verified");
    h.check(m1MethodOk, "REQ-M1 reports verification method 'test'");
    h.check(m2Unverified, "REQ-M2 (analysis) is not verified");
    h.check(m2MethodOk, "REQ-M2 reports verification method 'analysis'");

    db.close();
    std::remove("lodestar_wpD_method.db");
    std::remove("lodestar_wpD_method.db-wal");
    std::remove("lodestar_wpD_method.db-shm");
}

// ---------------------------------------------------------------------------
// A6. DO-178C evidence package export
// ---------------------------------------------------------------------------
void testEvidencePackage(Harness& h) {
    h.section("A6. DO-178C evidence package export");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wpD_evidence.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::GraphEngine graph(db);
    tl::RulesEngine rules(db);
    tl::BaselineService base(db);
    tl::IoService io(db);

    // Build a small graph: requirement verified by a test case.
    auto r = svc.addEntity(makeReq("REQ-E1", "test"));
    auto tc = svc.addEntity(makeTc("TC-E1"));
    h.check(r.isOk() && tc.isOk(), "seed entities ok");
    const std::string rId = r.value().id;
    const std::string tcId = tc.value().id;
    tl::Link l;
    l.sourceType = tl::EntityType::TestCase;
    l.sourceId = tcId;
    l.targetType = tl::EntityType::Requirement;
    l.targetId = rId;
    l.relation = "verifies";
    h.check(svc.addLink(l).isOk(), "add verifies link ok");

    // Run a validation so validation.json has content.
    tl::Rule rule;
    rule.name = "REQ_MUST_BE_VERIFIED";
    rule.ruleType = "REQ_MUST_BE_VERIFIED";
    rule.severity = tl::Severity::Error;
    rule.enabled = true;
    h.check(rules.defineRule(rule).isOk(), "define rule ok");
    auto run = rules.runValidation();
    h.check(run.isOk(), "run validation ok");

    // Create a baseline so audit/coverage data is present.
    h.check(base.createBaseline("EVIDENCE", "evidence baseline").isOk(),
            "create baseline ok");

    // Export the evidence package.
    const std::string dir = "lodestar_wpD_evidence_pkg";
    auto exp = io.exportEvidencePackage(dir);
    h.check(exp.isOk(), "exportEvidencePackage ok");

    // Verify each expected file exists and is non-empty.
    const char* files[] = {
        "matrix.csv", "coverage.csv", "coverage_by_method.csv",
        "validation.json", "audit.csv", "manifest.json",
    };
    bool allPresent = true;
    for (const char* f : files) {
        if (!fileNonEmpty(dir + "/" + f)) allPresent = false;
    }
    h.check(allPresent, "all evidence package files exist and are non-empty");

    // manifest.json lists the files.
    std::ifstream in(dir + "/manifest.json");
    std::stringstream ss;
    ss << in.rdbuf();
    std::string manifest = ss.str();
    in.close();
    bool listsAll = true;
    for (const char* f : files) {
        if (manifest.find(f) == std::string::npos) listsAll = false;
    }
    h.check(listsAll, "manifest.json lists every evidence file");

    db.close();
    std::remove("lodestar_wpD_evidence.db");
    std::remove("lodestar_wpD_evidence.db-wal");
    std::remove("lodestar_wpD_evidence.db-shm");
    for (const char* f : files) {
        std::remove((dir + "/" + f).c_str());
    }
    std::remove(dir.c_str());
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("WP-D coverage + evidence");
    std::printf("WP-D COVERAGE + EVIDENCE TESTS (migrations: %s)\n",
                g_migrationsDir.c_str());

    testCoverageByMethod(h);
    testEvidencePackage(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
