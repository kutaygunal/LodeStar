// core/test/s3_phase3_tests.cpp
// ---------------------------------------------------------------------------
// Sprint 3 Phase 3 (real structural coverage - Cobertura import) tests.
//
// Covers PLAN.md S3.3: replacing the caller-supplied coverage-percentage
// model (S2 Phase 7) with data imported from a real Cobertura XML report, as
// produced by OpenCppCoverage against a Debug build of the test suite (see
// ci/run_coverage.ps1). These tests exercise the parser/importer against a
// fixture XML shaped exactly like OpenCppCoverage 0.9.9's real output
// (verified by hand against a live `OpenCppCoverage --export_type cobertura`
// run over lodestar_smoke.exe during development of this phase) - they do
// not require OpenCppCoverage to be installed to run.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/testforge/CoberturaImport.h"
#include "core/testforge/Coverage.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace p = lodestar::persistence;
namespace tf = lodestar::testforge;

namespace {

std::string g_migrationsDir = LODESTAR_MIGRATIONS_DIR;

// ---------------------------------------------------------------------------
// Lightweight test harness (same shape as every other phase's).
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

bool openFreshDb(p::Database& db, const char* file) {
    std::remove(file);
    if (db.open(file).failed()) return false;
    p::MigrationRunner runner(db);
    return runner.run(g_migrationsDir).isOk();
}

void cleanupDb(const char* file) {
    std::remove(file);
    std::remove((std::string(file) + "-wal").c_str());
    std::remove((std::string(file) + "-shm").c_str());
}

// A fixture shaped like a real OpenCppCoverage cobertura export: two files
// under core/ (one with partial coverage, one fully covered), one file under
// core/test/ (must be excluded - it's test code, not product code), and one
// file outside core/ entirely (a vcpkg/system header OpenCppCoverage's loose
// --sources match can still pick up; must also be excluded).
const char* kFixtureXml = R"XML(<?xml version="1.0" encoding="utf-8"?>
<coverage line-rate="0.75" branch-rate="0" complexity="0" branches-covered="0"
          branches-valid="0" timestamp="1786571552" lines-covered="7"
          lines-valid="10" version="0">
  <sources><source>C:</source></sources>
  <packages>
    <package name="lodestar_smoke.exe" line-rate="0.75" branch-rate="0" complexity="0">
      <classes>
        <class name="AdapterRegistry.cpp"
               filename="Users\dev\Lodestar\core\adapters\AdapterRegistry.cpp"
               line-rate="0.6" branch-rate="0" complexity="0">
          <methods/>
          <lines>
            <line number="10" hits="1"/>
            <line number="11" hits="1"/>
            <line number="12" hits="1"/>
            <line number="13" hits="0"/>
            <line number="14" hits="0"/>
          </lines>
        </class>
        <class name="MockAdapter.cpp"
               filename="Users\dev\Lodestar\core\adapters\MockAdapter.cpp"
               line-rate="1" branch-rate="0" complexity="0">
          <methods/>
          <lines>
            <line number="20" hits="1"/>
            <line number="21" hits="1"/>
            <line number="22" hits="1"/>
            <line number="23" hits="1"/>
            <line number="24" hits="1"/>
          </lines>
        </class>
        <class name="s3_phase3_tests.cpp"
               filename="Users\dev\Lodestar\core\test\s3_phase3_tests.cpp"
               line-rate="1" branch-rate="0" complexity="0">
          <methods/>
          <lines>
            <line number="1" hits="1"/>
          </lines>
        </class>
        <class name="vector"
               filename="Users\dev\vcpkg\installed\x64-windows\include\vector"
               line-rate="1" branch-rate="0" complexity="0">
          <methods/>
          <lines>
            <line number="500" hits="1"/>
          </lines>
        </class>
      </classes>
    </package>
    <!-- A second package, as OpenCppCoverage --cover_children produces one
         per child process (one per test binary in ci/run_coverage.ps1).
         Touches the SAME file (AdapterRegistry.cpp is a shared module every
         test binary links) with a DIFFERENT hit pattern: line 13, which the
         first package never executed, is executed here. The importer must
         union these by line number, not just take the last package seen -
         that was a real bug found and fixed while building this phase. -->
    <package name="lodestar_s1_phase1_tests.exe" line-rate="0.8" branch-rate="0" complexity="0">
      <classes>
        <class name="AdapterRegistry.cpp"
               filename="Users\dev\Lodestar\core\adapters\AdapterRegistry.cpp"
               line-rate="0.8" branch-rate="0" complexity="0">
          <methods/>
          <lines>
            <line number="10" hits="1"/>
            <line number="11" hits="0"/>
            <line number="12" hits="1"/>
            <line number="13" hits="1"/>
            <line number="14" hits="0"/>
          </lines>
        </class>
      </classes>
    </package>
  </packages>
</coverage>
)XML";

void writeFixture(const char* path) {
    std::ofstream out(path, std::ios::binary);
    out << kFixtureXml;
}

// ---------------------------------------------------------------------------
// T1. parseCoberturaReport
// ---------------------------------------------------------------------------
void testParse(Harness& h) {
    h.section("T1. parseCoberturaReport parses real-shaped Cobertura XML");

    const char* xmlPath = "lodestar_s3p3_fixture.xml";
    writeFixture(xmlPath);

    auto parsed = tf::parseCoberturaReport(xmlPath);
    h.check(parsed.isOk(), "parse succeeds");
    if (parsed.isOk()) {
        const auto& files = parsed.value();
        h.check(files.size() == 2,
                "exactly 2 files parsed (core/test/ and non-core/ excluded)");

        bool foundRegistry = false, foundMock = false, foundTest = false,
             foundVector = false;
        for (const auto& f : files) {
            if (f.filename == "core/adapters/AdapterRegistry.cpp") {
                foundRegistry = true;
                // Two packages touch this file (see fixture comment): union
                // of hits by line number, not "last package wins". Line 13
                // is only hit in the second package and must still count.
                h.check(f.statementsExecuted == 4,
                        "AdapterRegistry: 4 executed (union across 2 packages, "
                        "not last-wins)");
                h.check(f.statementsTotal == 5,
                        "AdapterRegistry: 5 total (deduped by line number, "
                        "not summed across packages)");
            }
            if (f.filename == "core/adapters/MockAdapter.cpp") {
                foundMock = true;
                h.check(f.statementsExecuted == 5, "MockAdapter: 5 executed");
                h.check(f.statementsTotal == 5, "MockAdapter: 5 total");
            }
            if (f.filename.find("core/test/") != std::string::npos) foundTest = true;
            if (f.filename.find("vcpkg") != std::string::npos) foundVector = true;
        }
        h.check(foundRegistry, "AdapterRegistry.cpp present");
        h.check(foundMock, "MockAdapter.cpp present");
        h.check(!foundTest, "core/test/ file excluded (not product code)");
        h.check(!foundVector, "non-core/ file excluded (not this project's source)");

        // Paths normalized to forward slashes with no leading drive/absolute
        // prefix - matches the "module:core/..." scope convention.
        h.check(files[0].filename.find('\\') == std::string::npos,
                "normalized filename has no backslashes");
    }

    std::remove(xmlPath);
}

void testParseMissingFile(Harness& h) {
    h.section("T2. parseCoberturaReport on a missing file fails cleanly");
    auto parsed = tf::parseCoberturaReport("lodestar_s3p3_does_not_exist.xml");
    h.check(parsed.failed(), "missing report file -> error, not a crash");
}

// ---------------------------------------------------------------------------
// T3. importCoberturaReport persists real coverage + is idempotent
// ---------------------------------------------------------------------------
void testImport(Harness& h) {
    h.section("T3. importCoberturaReport persists + is idempotent on re-import");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s3p3_cov.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    tf::CoverageDao dao(db);

    const char* xmlPath = "lodestar_s3p3_fixture2.xml";
    writeFixture(xmlPath);

    auto n1 = tf::importCoberturaReport(xmlPath, "run-s3p3-A", dao);
    h.check(n1.isOk() && n1.value() == 2, "first import: 2 files imported");

    auto all1 = dao.list();
    h.check(all1.isOk(), "list ok after first import");
    size_t countAfterFirst = all1.isOk() ? all1.value().size() : 0;

    // Re-importing the SAME report under the SAME run id must update the
    // existing rows in place (deterministic id from runId+scope), not create
    // duplicates - otherwise every re-run of ci/run_coverage.ps1 would grow
    // the table forever.
    auto n2 = tf::importCoberturaReport(xmlPath, "run-s3p3-A", dao);
    h.check(n2.isOk() && n2.value() == 2, "re-import: 2 files imported again");

    auto all2 = dao.list();
    h.check(all2.isOk() && all2.value().size() == countAfterFirst,
            "re-import under the same run id does not create duplicate rows");

    // A different run id, though, is a distinct set of rows.
    auto n3 = tf::importCoberturaReport(xmlPath, "run-s3p3-B", dao);
    h.check(n3.isOk() && n3.value() == 2, "import under a different run id: 2 files");

    auto all3 = dao.list();
    h.check(all3.isOk() && all3.value().size() == countAfterFirst + 2,
            "a different run id adds new rows rather than colliding");

    // The persisted counts recompute to the right real percentages, and
    // decision/condition fields are honestly zero (not measured, not
    // fabricated) rather than silently defaulted to some other value.
    bool checkedOne = false;
    if (all1.isOk()) {
        for (const auto& r : all1.value()) {
            if (r.scope == "module:core/adapters/AdapterRegistry.cpp") {
                checkedOne = true;
                h.check(r.statementsExecuted == 4 && r.statementsTotal == 5,
                        "persisted AdapterRegistry counts are the real "
                        "union-across-packages 4/5");
                h.check(r.decisionsTotal == 0 && r.conditionsTotal == 0,
                        "decision/MC-DC fields left at 0 (not measured, not faked)");
                double pct = tf::computeStatementCoverage(r.statementsExecuted,
                                                          r.statementsTotal);
                h.check(pct > 79.9 && pct < 80.1,
                        "recomputed statement coverage is 80%");
            }
        }
    }
    h.check(checkedOne, "found the AdapterRegistry row to check");

    std::remove(xmlPath);
    db.close();
    cleanupDb("lodestar_s3p3_cov.db");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) g_migrationsDir = argv[1];

    Harness h("S3 Phase 3 Real structural coverage (Cobertura import)");
    std::printf("S3 PHASE 3 REAL STRUCTURAL COVERAGE TESTS (migrations: %s)\n",
               g_migrationsDir.c_str());

    testParse(h);
    testParseMissingFile(h);
    testImport(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
