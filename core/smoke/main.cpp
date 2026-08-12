// core/smoke/main.cpp
// Self-verifying smoke path for the Phase 3 vertical slice.
//
// Opens a SQLite database, runs the migration runner to create the schema,
// inserts a requirement + test case + trace link through the TraceGraph, then
// queries the link back and reports success. Exits 0 on success, non-zero on
// failure.

#include <cstdio>
#include <cstdlib>
#include <string>

#include "core/common/Config.h"
#include "core/common/Logger.h"
#include "core/common/Version.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/tracelink/TraceGraph.h"

namespace lodestar::scenario { int runScenarioSmoke(); }
namespace lodestar { int runAdaptersApiSmoke(); }
namespace lodestar::testforge { int runTestForgeSmoke(); }
namespace lodestar::tracelink { int runTraceLinkSmoke(); }

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace {

int fail(const std::string& message) {
    std::fprintf(stderr, "SMOKE FAIL: %s\n", message.c_str());
    return 1;
}

}  // namespace

int main(int argc, char** argv) {
    using namespace lodestar;

    common::Logger::instance().setLevel(common::LogLevel::Info);
    common::Logger::instance().info("Lodestar smoke path starting (common v" +
                                    std::to_string(common::moduleVersion()) + ")");

    // Resolve the migrations directory: argv[1] overrides the compile-time default.
    std::string migrationsDir = LODESTAR_MIGRATIONS_DIR;
    if (argc > 1) {
        migrationsDir = argv[1];
    }

    // Use a throwaway DB in the current working directory.
    const std::string dbPath = "lodestar_smoke.db";
    std::remove(dbPath.c_str());

    persistence::Database db;
    auto open = db.open(dbPath);
    if (open.failed()) {
        return fail(open.error());
    }

    persistence::MigrationRunner runner(db);
    auto migrated = runner.run(migrationsDir);
    if (migrated.failed()) {
        return fail(migrated.error());
    }
    common::Logger::instance().info("Schema migrated to version " +
                                    std::to_string(migrated.value()));

    tracelink::TraceGraph graph(db);

    // Insert a requirement and a test case.
    persistence::Requirement req;
    req.externalId = "REQ-001";
    req.name = "REQ-001";
    req.description = "The system shall provide GNSS position output.";
    auto addReq = graph.addRequirement(req);
    if (addReq.failed()) {
        return fail("addRequirement: " + addReq.error());
    }

    persistence::TestCase tc;
    tc.externalId = "TC-001";
    tc.name = "TC-001";
    tc.description = "Verify GNSS position output accuracy.";
    auto addTc = graph.addTestCase(tc);
    if (addTc.failed()) {
        return fail("addTestCase: " + addTc.error());
    }

    // Link the test case to the requirement (test case verifies requirement).
    persistence::TraceLink link;
    link.sourceType = "test_case";
    link.sourceId = tc.id;
    link.targetType = "requirement";
    link.targetId = req.id;
    link.relation = "verifies";
    auto addLink = graph.addLink(link);
    if (addLink.failed()) {
        return fail("addLink: " + addLink.error());
    }

    // Query the link back from the requirement side.
    auto links = graph.linksTo("requirement", req.id);
    if (links.failed()) {
        return fail("linksTo: " + links.error());
    }
    if (links.value().size() != 1) {
        return fail("expected 1 link to requirement, got " +
                    std::to_string(links.value().size()));
    }
    const persistence::TraceLink& found = links.value().front();
    if (found.sourceId != tc.id || found.relation != "verifies") {
        return fail("queried link does not match inserted link");
    }

    // Confirm the requirement round-trips.
    auto reqs = graph.requirements();
    if (reqs.failed() || reqs.value().size() != 1) {
        return fail("requirements query did not return the inserted requirement");
    }

    common::Logger::instance().info("Trace link verified: " + found.sourceType + " '" +
                                    found.sourceId + "' " + found.relation + " " +
                                    found.targetType + " '" + found.targetId + "'");

    db.close();
    std::remove(dbPath.c_str());
    std::remove((dbPath + "-wal").c_str());
    std::remove((dbPath + "-shm").c_str());

    std::printf("SMOKE OK: schema v%d, trace link insert+query round-trip passed.\n",
                migrated.value());

    // Phase 4: ScenarioForge real GNSS math smoke path.
    int scenarioResult = lodestar::scenario::runScenarioSmoke();
    if (scenarioResult != 0) {
        return fail("ScenarioForge smoke path failed");
    }

    // Phase 5: Adapters + thin C++ REST API smoke path.
    int adaptersResult = lodestar::runAdaptersApiSmoke();
    if (adaptersResult != 0) {
        return fail("Adapters + API smoke path failed");
    }

    // Phase 6: TestForge IT&V plan/run/report/persist smoke path.
    int testforgeResult = lodestar::testforge::runTestForgeSmoke();
    if (testforgeResult != 0) {
        return fail("TestForge smoke path failed");
    }

    // Phase 7 / WP-1: TraceLink rich typed domain model smoke path.
    int tracelinkResult = lodestar::tracelink::runTraceLinkSmoke();
    if (tracelinkResult != 0) {
        return fail("TraceLink WP-1 smoke path failed");
    }

    return 0;
}
