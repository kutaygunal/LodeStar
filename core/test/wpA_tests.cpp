// core/test/wpA_tests.cpp
// ---------------------------------------------------------------------------
// WP-A unit/integration tests (test-first).
//
// Written by the scrum-master BEFORE the WP-A engineer implements the feature.
// The engineer must implement the contract documented below so these tests
// compile and pass. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// Covers (PLAN.md, WP-A):
//   A1. FTS5 full-text search (ranked) across entity name + body.
//   B1. Pagination (limit/offset) on list + search endpoints.
//
// Uses the same lightweight self-contained harness as WP-1..WP-8. Each
// DB-dependent test opens its own fresh throwaway DB.
//
// ---------------------------------------------------------------------------
// CONTRACT the WP-A engineer must provide.
// ---------------------------------------------------------------------------
// (A) Migration 010 (core/persistence/migrations/010_*.sql) creates an FTS5
//     virtual table `entity_fts` with columns (type, external_id, name, body)
//     so ranked full-text search works across all entity kinds. The migration
//     is append-only and idempotent (IF NOT EXISTS).
//
// (B) TraceLinkService additions (core/tracelink/TraceLinkService.h):
//
//   // One ranked full-text search hit.
//   struct SearchHit {
//       EntityType type = EntityType::Requirement;
//       std::string id;
//       std::string externalId;
//       std::string name;
//       double rank = 0.0;   // FTS5 bm25 score; LOWER is a better match
//   };
//
//   // Rebuilds the FTS5 index from every entity table. Safe to call any time.
//   common::Result<void> rebuildSearchIndex();
//
//   // Ranked full-text search across name + body for one entity type.
//   // text is a plain term/phrase (the service escapes it for FTS5 MATCH).
//   // limit/offset paginate the ranked result set (0 = no limit).
//   // Results are ordered best-match-first (ascending bm25 rank).
//   // The name column is weighted HIGHER than body, so a name match always
//   // ranks above a body-only match for the same term.
//   common::Result<std::vector<SearchHit>> searchRanked(
//       EntityType type, const std::string& text, int limit = 0, int offset = 0);
//
// (C) listEntities(type, filter) honors filter.limit / filter.offset
//     (already wired through the DAO; the service must pass them through).
// ---------------------------------------------------------------------------

#include <cstdio>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/persistence/Models.h"
#include "core/persistence/daos.h"
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

// ---------------------------------------------------------------------------
// Entity factories.
// ---------------------------------------------------------------------------
tl::Entity makeReq(const std::string& extId, const std::string& name,
                   const std::string& body) {
    tl::Entity e;
    e.externalId = extId;
    e.type = tl::EntityType::Requirement;
    e.name = name;
    e.text = body;
    e.status = "Draft";
    return e;
}

tl::Entity makeDesign(const std::string& extId, const std::string& name,
                      const std::string& body) {
    tl::Entity e;
    e.externalId = extId;
    e.type = tl::EntityType::Design;
    e.name = name;
    e.text = body;
    e.status = "Draft";
    return e;
}

tl::Entity makeTc(const std::string& extId, const std::string& name,
                  const std::string& body) {
    tl::Entity e;
    e.externalId = extId;
    e.type = tl::EntityType::TestCase;
    e.name = name;
    e.text = body;
    e.status = "Draft";
    return e;
}

// ---------------------------------------------------------------------------
// A1. FTS5 ranked full-text search
// ---------------------------------------------------------------------------
void testRankedSearch(Harness& h) {
    h.section("A1. FTS5 ranked full-text search");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wpA_search.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);

    // Seed entities with distinct searchable terms.
    auto r1 = svc.addEntity(makeReq("REQ-1", "Navigation", "The system shall compute GNSS position."));
    auto r2 = svc.addEntity(makeReq("REQ-2", "Altitude", "The system shall compute altitude from GNSS."));
    auto r3 = svc.addEntity(makeReq("REQ-3", "Timing", "The system shall provide precise timing."));
    auto d1 = svc.addEntity(makeDesign("DES-1", "Position Solver", "Solves GNSS position from pseudoranges."));
    auto t1 = svc.addEntity(makeTc("TC-1", "Position Accuracy", "Verify GNSS position accuracy."));
    h.check(r1.isOk() && r2.isOk() && r3.isOk() && d1.isOk() && t1.isOk(),
            "seed entities ok");

    // Rebuild the FTS index.
    auto rebuild = svc.rebuildSearchIndex();
    h.check(rebuild.isOk(), "rebuildSearchIndex ok");

    // Search by a term present in a name.
    auto hits = svc.searchRanked(tl::EntityType::Requirement, "Altitude");
    h.check(hits.isOk(), "searchRanked ok");
    h.check(hits.value().size() == 1, "search by name term returns exactly one hit");
    if (hits.isOk() && !hits.value().empty()) {
        h.check(hits.value().front().externalId == "REQ-2",
                "name-term hit is the correct entity");
    }

    // Search by a term present in the body.
    auto bodyHits = svc.searchRanked(tl::EntityType::Requirement, "precise");
    h.check(bodyHits.isOk() && bodyHits.value().size() == 1,
            "search by body term returns the matching entity");
    if (bodyHits.isOk() && !bodyHits.value().empty()) {
        h.check(bodyHits.value().front().externalId == "REQ-3",
                "body-term hit is the correct entity");
    }

    // Search is case-insensitive.
    auto ci = svc.searchRanked(tl::EntityType::Requirement, "ALTITUDE");
    h.check(ci.isOk() && ci.value().size() == 1,
            "search is case-insensitive");

    // No match -> empty result (not an error).
    auto none = svc.searchRanked(tl::EntityType::Requirement, "zzz_nonexistent_zzz");
    h.check(none.isOk() && none.value().empty(),
            "no-match search returns empty result");

    // Search across a different entity type.
    auto designHits = svc.searchRanked(tl::EntityType::Design, "pseudoranges");
    h.check(designHits.isOk() && designHits.value().size() == 1,
            "search works for design entities");
    if (designHits.isOk() && !designHits.value().empty()) {
        h.check(designHits.value().front().externalId == "DES-1",
                "design hit is the correct entity");
    }

    // Ranked: a term in the name should outrank the same term in the body.
    // REQ-1 has "Navigation" in the name; give another requirement the same
    // word only in its body so the name match ranks higher (lower bm25).
    auto r4 = svc.addEntity(makeReq("REQ-4", "Guidance", "Navigation subsystem guidance logic."));
    h.check(r4.isOk(), "seed ranked-comparison entity ok");
    h.check(svc.rebuildSearchIndex().isOk(), "rebuild after adding entity ok");
    auto ranked = svc.searchRanked(tl::EntityType::Requirement, "Navigation");
    h.check(ranked.isOk() && ranked.value().size() == 2,
            "ranked search returns both matching entities");
    if (ranked.isOk() && ranked.value().size() == 2) {
        h.check(ranked.value().front().externalId == "REQ-1",
                "name match ranks before body-only match");
        h.check(ranked.value().front().rank <= ranked.value().back().rank,
                "rank is ascending (lower = better)");
    }

    db.close();
    std::remove("lodestar_wpA_search.db");
    std::remove("lodestar_wpA_search.db-wal");
    std::remove("lodestar_wpA_search.db-shm");
}

// ---------------------------------------------------------------------------
// B1. Pagination (limit/offset) on list + search
// ---------------------------------------------------------------------------
void testPagination(Harness& h) {
    h.section("B1. Pagination (limit/offset) on list + search");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wpA_page.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);

    // Create 25 requirements.
    for (int i = 0; i < 25; ++i) {
        auto r = svc.addEntity(makeReq("REQ-P" + std::to_string(i),
                                       "Requirement " + std::to_string(i),
                                       "Body text for requirement " + std::to_string(i)));
        if (r.failed()) {
            h.check(false, "seed 25 requirements");
            db.close();
            return;
        }
    }

    // listEntities honors limit/offset.
    tl::EntityFilter f0;
    f0.limit = 10;
    f0.offset = 0;
    auto page1 = svc.listEntities(tl::EntityType::Requirement, f0);
    h.check(page1.isOk() && page1.value().size() == 10,
            "listEntities limit=10 offset=0 -> 10 rows");

    tl::EntityFilter f1;
    f1.limit = 10;
    f1.offset = 10;
    auto page2 = svc.listEntities(tl::EntityType::Requirement, f1);
    h.check(page2.isOk() && page2.value().size() == 10,
            "listEntities limit=10 offset=10 -> 10 rows");

    tl::EntityFilter f2;
    f2.limit = 10;
    f2.offset = 20;
    auto page3 = svc.listEntities(tl::EntityType::Requirement, f2);
    h.check(page3.isOk() && page3.value().size() == 5,
            "listEntities limit=10 offset=20 -> 5 rows (tail)");

    tl::EntityFilter f3;
    f3.limit = 10;
    f3.offset = 25;
    auto page4 = svc.listEntities(tl::EntityType::Requirement, f3);
    h.check(page4.isOk() && page4.value().empty(),
            "listEntities offset past end -> empty");

    // Pages are disjoint (no row appears on two pages).
    bool disjoint = true;
    for (const auto& a : page1.value()) {
        for (const auto& b : page2.value()) {
            if (a.id == b.id) disjoint = false;
        }
    }
    h.check(disjoint, "page 1 and page 2 are disjoint");

    // Rebuild index, then paginate the ranked search.
    h.check(svc.rebuildSearchIndex().isOk(), "rebuildSearchIndex ok");
    auto s0 = svc.searchRanked(tl::EntityType::Requirement, "requirement", 10, 0);
    h.check(s0.isOk() && s0.value().size() == 10,
            "searchRanked limit=10 offset=0 -> 10 hits");
    auto s1 = svc.searchRanked(tl::EntityType::Requirement, "requirement", 10, 10);
    h.check(s1.isOk() && s1.value().size() == 10,
            "searchRanked limit=10 offset=10 -> 10 hits");
    auto s2 = svc.searchRanked(tl::EntityType::Requirement, "requirement", 10, 20);
    h.check(s2.isOk() && s2.value().size() == 5,
            "searchRanked limit=10 offset=20 -> 5 hits (tail)");

    db.close();
    std::remove("lodestar_wpA_page.db");
    std::remove("lodestar_wpA_page.db-wal");
    std::remove("lodestar_wpA_page.db-shm");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("WP-A search + pagination");
    std::printf("WP-A SEARCH + PAGINATION TESTS (migrations: %s)\n",
                g_migrationsDir.c_str());

    testRankedSearch(h);
    testPagination(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
