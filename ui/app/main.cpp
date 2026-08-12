// ui/app/main.cpp
// S1 Phase 1 desktop app entry point.
//
// Opens a real SQLite DB, runs migrations, seeds a small TraceLink graph
// (one requirement + one test case + one `verifies` link), constructs the
// lodestar::ui::MainWindow against that DB, calls refreshAll() and show(),
// and runs the Qt event loop. The window closes cleanly (and the app exits
// 0) either when the user closes it or after a short auto-close timer so a
// headless `--platform offscreen` launch also terminates cleanly.

#include <QApplication>
#include <QTimer>
#include <QWidget>

#include <cstdio>
#include <string>

#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/tracelink/TraceLinkService.h"
#include "core/tracelink/Types.h"
#include "ui/MainWindow.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace tl = lodestar::tracelink;
namespace p  = lodestar::persistence;

namespace {

// Seeds a small TraceLink graph: one requirement, one test case, one verifies
// link. Returns true on success.
bool seedGraph(tl::TraceLinkService& svc) {
    tl::Entity req;
    req.externalId = "REQ-001";
    req.type = tl::EntityType::Requirement;
    req.name = "REQ-001";
    req.text = "The system shall provide GNSS/SBAS test and verification.";
    req.status = "Approved";
    req.owner = "engineer";
    req.verificationMethod = "test";
    req.safetyLevel = "Level A";
    auto r = svc.addEntity(req);
    if (r.failed()) return false;

    tl::Entity tc;
    tc.externalId = "TC-001";
    tc.type = tl::EntityType::TestCase;
    tc.name = "TC-001";
    tc.text = "Verify the GNSS/SBAS test and verification capability.";
    auto t = svc.addEntity(tc);
    if (t.failed()) return false;

    tl::Link link;
    link.sourceType = tl::EntityType::TestCase;
    link.sourceId = t.value().id;
    link.targetType = tl::EntityType::Requirement;
    link.targetId = r.value().id;
    link.relation = "verifies";
    auto l = svc.addLink(link);
    return l.isOk();
}

}  // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    // Open a real DB and run migrations.
    p::Database db;
    const std::string dbPath = "lodestar_app.db";
    std::remove(dbPath.c_str());
    std::remove((dbPath + "-wal").c_str());
    std::remove((dbPath + "-shm").c_str());
    if (db.open(dbPath).failed()) {
        std::fprintf(stderr, "lodestar_app: failed to open database\n");
        return 1;
    }
    p::MigrationRunner runner(db);
    auto mig = runner.run(LODESTAR_MIGRATIONS_DIR);
    if (mig.failed()) {
        std::fprintf(stderr, "lodestar_app: migration failed: %s\n",
                     mig.error().c_str());
        return 1;
    }

    // Seed a small TraceLink graph so the views have data to show.
    tl::TraceLinkService svc(db);
    if (!seedGraph(svc)) {
        std::fprintf(stderr, "lodestar_app: failed to seed TraceLink graph\n");
        return 1;
    }

    // Build the main window, refresh it, and show it.
    lodestar::ui::MainWindow window(db);
    window.refreshAll();
    window.show();

    // NOTE: auto-close timer removed so the window stays open for a real user.
    // A headless (offscreen) launch can still be terminated externally.

    const int rc = app.exec();
    db.close();
    return rc;
}
