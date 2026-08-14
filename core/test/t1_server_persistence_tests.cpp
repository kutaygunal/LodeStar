// core/test/t1_server_persistence_tests.cpp
// ---------------------------------------------------------------------------
// Gap-Fill TraceLink 3.1: client-server persistence path tests.
//
// Test contract: docs/gap-fill-plan.md (Module 3.1).
//   (A) core/persistence/ServerPersistence.h (+ .cpp): a server-backed
//       persistence adapter that talks to the Lodestar REST API over HTTP
//       (alternative storage backend for the web/multi-user deployment mode),
//       keeping SQLite as the single-user default.
//   (B) The same requirement operations run against both backends
//       (parameterized): create, get by id, list, update, remove.
//
// Deterministic: starts a local HttpServer on an ephemeral port.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

#include "core/api/HttpServer.h"
#include "core/api/TraceLinkApiServer.h"
#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/persistence/ServerPersistence.h"
#include "core/tracelink/BaselineService.h"
#include "core/tracelink/GraphEngine.h"
#include "core/tracelink/IoService.h"
#include "core/tracelink/RulesEngine.h"
#include "core/tracelink/TraceLinkService.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace ap = lodestar::api;
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
    std::remove((std::string(dbfile) + "-wal").c_str());
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
// T1. create + findById + list over the server backend
// ---------------------------------------------------------------------------
void testCreateAndGet(Harness& h) {
    h.section("T1. server-backed create + findById + list");
    Server s;
    if (!setupServer(s, "lodestar_t1srv_crud.db")) {
        h.check(false, "server start");
        return;
    }
    p::ServerRequirementStore store("127.0.0.1", s.port);

    p::ServerRequirement req;
    req.externalId = "REQ-100";
    req.name = "Navigation";
    req.text = "Provide navigation.";
    req.status = "Draft";

    auto created = store.create(req);
    h.check(created.isOk(), "create() ok");
    if (!created.isOk()) { return; }
    h.check(!created.value().id.empty(), "create() returns a server-assigned id");
    h.check(created.value().externalId == "REQ-100", "external id preserved");
    h.check(created.value().name == "Navigation", "name preserved");

    auto found = store.findById(created.value().id);
    h.check(found.isOk() && found.value().has_value(),
            "findById() returns the requirement");
    if (found.isOk() && found.value().has_value()) {
        h.check(found.value()->externalId == "REQ-100",
                "findById external id == REQ-100");
        h.check(found.value()->text == "Provide navigation.",
                "findById text preserved");
    }

    auto list = store.listAll();
    h.check(list.isOk(), "listAll() ok");
    if (list.isOk()) {
        h.check(list.value().size() == 1, "listAll() returns 1 requirement");
        if (list.value().size() == 1) {
            h.check(list.value()[0].externalId == "REQ-100",
                    "listed requirement is REQ-100");
        }
    }

    // findById of unknown -> nullopt.
    auto missing = store.findById("no-such-id");
    h.check(missing.isOk() && !missing.value().has_value(),
            "findById() of unknown id returns none");

    s.http.stop();
    std::remove("lodestar_t1srv_crud.db");
    std::remove("lodestar_t1srv_crud.db-wal");
}

// ---------------------------------------------------------------------------
// T2. update + remove over the server backend
// ---------------------------------------------------------------------------
void testUpdateAndRemove(Harness& h) {
    h.section("T2. server-backed update + remove");
    Server s;
    if (!setupServer(s, "lodestar_t1srv_upd.db")) {
        h.check(false, "server start");
        return;
    }
    p::ServerRequirementStore store("127.0.0.1", s.port);

    p::ServerRequirement req;
    req.externalId = "REQ-200";
    req.name = "Tracking";
    req.text = "Track signals.";
    req.status = "Draft";
    auto created = store.create(req);
    if (!created.isOk()) {
        h.check(false, "create() ok");
        return;
    }

    // Update the name.
    created.value().name = "Advanced Tracking";
    auto updated = store.update(created.value());
    h.check(updated.isOk(), "update() ok");
    if (updated.isOk()) {
        h.check(updated.value().name == "Advanced Tracking",
                "update() changes the name");
    }
    auto reGet = store.findById(created.value().id);
    h.check(reGet.isOk() && reGet.value().has_value() &&
                reGet.value()->name == "Advanced Tracking",
            "updated name persisted on the server");

    // Remove (soft-delete -> the entity becomes Obsolete; it still resolves by
    // id but is no longer listed as an active requirement).
    auto removed = store.remove(created.value().id);
    h.check(removed.isOk(), "remove() ok");
    auto after = store.findById(created.value().id);
    h.check(after.isOk() && after.value().has_value() &&
                after.value()->status == "Obsolete",
            "after remove the entity is marked Obsolete (soft-delete)");

    s.http.stop();
    std::remove("lodestar_t1srv_upd.db");
    std::remove("lodestar_t1srv_upd.db-wal");
}

// ---------------------------------------------------------------------------
// T3. Same operations against the SQLite backend (parameterized parity)
// ---------------------------------------------------------------------------
void testSqliteBackend(Harness& h) {
    h.section("T3. parity: same operations against the SQLite backend");
    p::Database db;
    std::remove("lodestar_t1srv_sqlite.db");
    if (db.open("lodestar_t1srv_sqlite.db").failed()) {
        h.check(false, "open sqlite db");
        return;
    }
    p::MigrationRunner runner(db);
    if (runner.run(g_migrationsDir).failed()) {
        h.check(false, "run migrations");
        return;
    }
    tl::TraceLinkService tls(db);

    // The same requirement lifecycle as T1/T2, against SQLite directly.
    tl::Entity e;
    e.type = tl::EntityType::Requirement;
    e.externalId = "REQ-100";
    e.name = "Navigation";
    e.text = "Provide navigation.";
    e.status = "Draft";
    auto added = tls.addEntity(e);
    h.check(added.isOk(), "sqlite addEntity() ok");
    if (!added.isOk()) { db.close(); return; }
    const std::string id = added.value().id;

    auto got = tls.getEntity(tl::EntityType::Requirement, id);
    h.check(got.isOk() && got.value().has_value() &&
                got.value()->externalId == "REQ-100",
            "sqlite getEntity() returns the requirement");

    auto list = tls.listEntities(tl::EntityType::Requirement, tl::EntityFilter{});
    h.check(list.isOk() && list.value().size() == 1,
            "sqlite listEntities() returns 1 requirement");

    db.close();
    std::remove("lodestar_t1srv_sqlite.db");
    std::remove("lodestar_t1srv_sqlite.db-wal");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) g_migrationsDir = argv[1];

    Harness h("Gap-Fill TraceLink 3.1 client-server persistence path");
    testCreateAndGet(h);
    testUpdateAndRemove(h);
    testSqliteBackend(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
