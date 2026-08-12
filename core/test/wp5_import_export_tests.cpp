// core/test/wp5_import_export_tests.cpp
// ---------------------------------------------------------------------------
// WP-5 unit/integration tests (test-first).
//
// Written by the scrum-master BEFORE the WP-5 engineer implements the feature.
// The engineer must implement the contract documented below so these tests
// compile and pass. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// Covers (docs/tracelink-plan.md, WP-5 / section 4.6 / 3.6 / 8):
//   1. Export: matrixCsv / matrixHtml / entitiesCsv / linksCsv / reqif
//   2. Non-destructive import (importCsv / importReqif) writing
//      import_batches + import_log
//   3. Round-trip stability (export -> import -> export is stable)
//   4. Partial import is logged without corrupting existing data
//   5. WP-5 acceptance: export a 5-requirement graph to CSV and ReqIF, import
//      both back, and the graph matches.
//
// Uses the same lightweight self-contained harness as WP-1..WP-4.
// Each DB-dependent test opens its own fresh throwaway DB.
//
// ---------------------------------------------------------------------------
// CONTRACT the WP-5 engineer must provide (in core/tracelink/IoService.h,
// namespace lodestar::tracelink). Reuses EntityType/Entity/Link/TraceLinkService.
// ---------------------------------------------------------------------------
//
// namespace lodestar::tracelink {
//
// // One line from an import batch log.
// struct ImportLogEntry {
//     int line = 0;           // 1-based source line / record
//     std::string severity;   // "info" | "warning" | "error"
//     std::string message;
// };
//
// // Result of one import call.
// struct ImportReport {
//     std::string batchId;    // UUID of the import_batches row
//     std::string status;     // "ok" | "partial"
//     int imported = 0;       // entities + links successfully imported
//     int errors = 0;         // failed records
//     std::vector<ImportLogEntry> log;
// };
//
// class IoService {
// public:
//     explicit IoService(persistence::Database& db);
//
//     // --- Exports (non-empty content on success) ----------------------------
//     common::Result<std::string> matrixCsv();    // trace matrix CSV
//     common::Result<std::string> matrixHtml();   // auditor-ready HTML report
//     common::Result<std::string> entitiesCsv();  // entity rows (see format)
//     common::Result<std::string> linksCsv();     // link rows (see format)
//     common::Result<std::string> reqif();        // ReqIF XML
//
//     // --- Imports (non-destructive; always write a batch + log) -------------
//     common::Result<ImportReport> importCsv(const std::string& content);
//     common::Result<ImportReport> importReqif(const std::string& content);
// };
// }  // namespace lodestar::tracelink
//
// CSV format (fields are comma-free, no quoting):
//   Entity line : entity,<type>,<external_id>,<name>,<text>,<status>
//   Link line   : link,<src_type>,<src_external_id>,<relation>,<tgt_type>,<tgt_external_id>
//
//   entitiesCsv() emits one entity line per entity (all types).
//   linksCsv()   emits one link line per Active link, keyed by EXTERNAL ids.
//   importCsv()  parses both line kinds. Entities are created by external id;
//   link source/target external ids are resolved to internal ids before adding.
//
// Semantics:
//   - Imports never modify or delete existing data (non-destructive). Records
//     that fail are skipped and logged; the valid ones are still imported.
//   - Each import writes one import_batches row plus import_log rows, and the
//     returned report exposes the batch id + log.
//   - A clean import returns status "ok"; one with any failed record returns
//     "partial" with errors>0 and an "error" log entry.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/tracelink/IoService.h"
#include "core/tracelink/TraceLinkService.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace tl = lodestar::tracelink;
namespace p  = lodestar::persistence;

namespace {

std::string g_migrationsDir = LODESTAR_MIGRATIONS_DIR;

bool openFreshDb(p::Database& db, const char* file) {
    std::remove(file);
    if (db.open(file).failed()) return false;
    p::MigrationRunner runner(db);
    return runner.run(g_migrationsDir).isOk();
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
// Factories (same contract as WP-1).
// ---------------------------------------------------------------------------
tl::Entity makeReq(const std::string& extId, const std::string& status = "Approved") {
    tl::Entity e;
    e.externalId = extId;
    e.type = tl::EntityType::Requirement;
    e.name = extId;
    e.text = "Requirement body for " + extId;
    e.status = status;
    e.owner = "engineer";
    e.verificationMethod = "test";
    e.safetyLevel = "Level A";
    return e;
}

tl::Entity makeTc(const std::string& extId) {
    tl::Entity e;
    e.externalId = extId;
    e.type = tl::EntityType::TestCase;
    e.name = extId;
    e.text = "Test body for " + extId;
    return e;
}

tl::Entity makeDesign(const std::string& extId) {
    tl::Entity e;
    e.externalId = extId;
    e.type = tl::EntityType::Design;
    e.name = extId;
    e.text = "Design body for " + extId;
    return e;
}

tl::Link makeLink(tl::EntityType srcT, const std::string& srcId, tl::EntityType tgtT,
                  const std::string& tgtId, const std::string& rel) {
    tl::Link l;
    l.sourceType = srcT;
    l.sourceId = srcId;
    l.targetType = tgtT;
    l.targetId = tgtId;
    l.relation = rel;
    return l;
}

// ---------------------------------------------------------------------------
// Helpers.
// ---------------------------------------------------------------------------
std::set<std::string> extIdsOfType(tl::TraceLinkService& svc, tl::EntityType type) {
    std::set<std::string> out;
    tl::EntityFilter f;  // no status filter
    auto ents = svc.listEntities(type, f);
    if (ents.isOk()) {
        for (const auto& e : ents.value()) out.insert(e.externalId);
    }
    return out;
}

// Split content into non-empty lines.
std::vector<std::string> linesOf(const std::string& content) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : content) {
        if (c == '\n' || c == '\r') {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

bool sameLineSet(const std::string& a, const std::string& b) {
    auto la = linesOf(a), lb = linesOf(b);
    std::set<std::string> sa(la.begin(), la.end()), sb(lb.begin(), lb.end());
    return sa == sb;
}

// Count links via the DAO (the enriched persistence TraceLinkDao).
int daoLinkCount(p::Database& db) {
    p::TraceLinkDao dao(db);
    auto all = dao.findAll();
    return all.isOk() ? static_cast<int>(all.value().size()) : -1;
}

// Build the WP-5 acceptance graph (5 requirements + design + test + links).
struct Graph {
    std::vector<std::string> reqIds;
    std::string desId;
    std::string tcId;
};

Graph buildGraph(tl::TraceLinkService& svc) {
    Graph g;
    for (int i = 1; i <= 5; ++i) {
        auto r = svc.addEntity(makeReq("REQ-" + std::to_string(i)));
        g.reqIds.push_back(r.value().id);
    }
    auto des = svc.addEntity(makeDesign("DES-1"));
    auto tc = svc.addEntity(makeTc("TC-1"));
    g.desId = des.value().id;
    g.tcId = tc.value().id;

    // REQ-2, REQ-3 derive from REQ-1; DES-1 satisfies REQ-1; TC-1 verifies REQ-1.
    svc.addLink(makeLink(tl::EntityType::Requirement, g.reqIds[1],
                         tl::EntityType::Requirement, g.reqIds[0], "derives"));
    svc.addLink(makeLink(tl::EntityType::Requirement, g.reqIds[2],
                         tl::EntityType::Requirement, g.reqIds[0], "derives"));
    svc.addLink(makeLink(tl::EntityType::Design, g.desId,
                         tl::EntityType::Requirement, g.reqIds[0], "satisfies"));
    svc.addLink(makeLink(tl::EntityType::TestCase, g.tcId,
                         tl::EntityType::Requirement, g.reqIds[0], "verifies"));
    return g;
}

// ---------------------------------------------------------------------------
// 1. Exports produce content
// ---------------------------------------------------------------------------
void testExports(Harness& h) {
    h.section("1. Export methods produce content");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp5_export.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::IoService io(db);
    buildGraph(svc);

    auto csv = io.entitiesCsv();
    h.check(csv.isOk() && !csv.value().empty(), "entitiesCsv non-empty");
    h.check(csv.isOk() && linesOf(csv.value()).size() == 7,
            "entitiesCsv has 5 req + design + test = 7 rows");

    auto links = io.linksCsv();
    h.check(links.isOk() && linesOf(links.value()).size() == 4,
            "linksCsv has 4 link rows");

    auto matrix = io.matrixCsv();
    h.check(matrix.isOk() && !matrix.value().empty(), "matrixCsv non-empty");

    auto html = io.matrixHtml();
    h.check(html.isOk() && html.value().find("<html") != std::string::npos,
            "matrixHtml is an HTML document");

    auto reqif = io.reqif();
    h.check(reqif.isOk() && !reqif.value().empty(), "reqif non-empty");
    h.check(reqif.isOk() && reqif.value().find("REQ-") != std::string::npos,
            "reqif contains requirement external ids");
}

// ---------------------------------------------------------------------------
// 3. Round-trip stability (export -> import -> export stable)
// ---------------------------------------------------------------------------
void testCsvRoundTrip(Harness& h) {
    h.section("3. CSV round-trip is stable");

    p::Database db1;
    if (!openFreshDb(db1, "lodestar_wp5_rt1.db")) {
        h.check(false, "open source db");
        return;
    }
    tl::TraceLinkService s1(db1);
    tl::IoService io1(db1);
    buildGraph(s1);

    auto ent1 = io1.entitiesCsv();
    auto link1 = io1.linksCsv();
    auto combined = ent1.value() + "\n" + link1.value();

    // Import into a fresh DB.
    p::Database db2;
    if (!openFreshDb(db2, "lodestar_wp5_rt2.db")) {
        h.check(false, "open target db");
        return;
    }
    tl::TraceLinkService s2(db2);
    tl::IoService io2(db2);
    auto rep = io2.importCsv(combined);
    h.check(rep.isOk() && rep.value().status == "ok", "importCsv clean import ok");

    h.check(extIdsOfType(s2, tl::EntityType::Requirement).size() == 5,
            "imported DB has 5 requirements");
    h.check(extIdsOfType(s2, tl::EntityType::Requirement) ==
                extIdsOfType(s1, tl::EntityType::Requirement),
            "imported requirement external ids match source");
    h.check(extIdsOfType(s2, tl::EntityType::Design).size() == 1 &&
                extIdsOfType(s2, tl::EntityType::TestCase).size() == 1,
            "imported design + test present");
    h.check(daoLinkCount(db2) == 4, "imported DB has 4 links");

    // Re-export from the imported DB is stable (same line set).
    h.check(sameLineSet(io2.entitiesCsv().value(), ent1.value()),
            "re-exported entitiesCsv stable");
    h.check(sameLineSet(io2.linksCsv().value(), link1.value()),
            "re-exported linksCsv stable");
}

// ---------------------------------------------------------------------------
// 4. Partial import is logged and non-destructive
// ---------------------------------------------------------------------------
void testPartialImport(Harness& h) {
    h.section("4. Partial import logged, non-destructive");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp5_partial.db")) {
        h.check(false, "open fresh db");
        return;
    }
    tl::TraceLinkService svc(db);
    tl::IoService io(db);

    // One valid entity line + one invalid line (bad type).
    const std::string content =
        "entity,requirement,REQ-GOOD,Good,Body,Approved\n"
        "entity,bogus,REQ-BAD,Bad,Body,Draft\n";

    auto rep = io.importCsv(content);
    h.check(rep.isOk(), "importCsv ok");
    h.check(rep.value().status == "partial", "partial import reports 'partial'");
    h.check(rep.value().imported == 1 && rep.value().errors == 1,
            "1 imported, 1 error");
    h.check(!rep.value().batchId.empty(), "partial import still writes a batch id");

    bool hasErrorLog = false;
    for (const auto& e : rep.value().log) {
        if (e.severity == "error") hasErrorLog = true;
    }
    h.check(hasErrorLog, "import_log records the failing line");

    // The valid record was imported; the invalid one was skipped (non-destructive).
    h.check(extIdsOfType(svc, tl::EntityType::Requirement).size() == 1 &&
                extIdsOfType(svc, tl::EntityType::Requirement).count("REQ-GOOD") == 1,
            "valid entity imported, invalid one skipped");
}

// ---------------------------------------------------------------------------
// 7. WP-5 acceptance
// ---------------------------------------------------------------------------
void testAcceptance(Harness& h) {
    h.section("7. WP-5 acceptance: 5-req graph CSV + ReqIF round-trip");

    // Source graph.
    p::Database db1;
    if (!openFreshDb(db1, "lodestar_wp5_acc_src.db")) {
        h.check(false, "open source db");
        return;
    }
    tl::TraceLinkService s1(db1);
    tl::IoService io1(db1);
    Graph g = buildGraph(s1);

    auto csv = io1.entitiesCsv().value() + "\n" + io1.linksCsv().value();
    auto reqif = io1.reqif();
    h.check(reqif.isOk(), "reqif export ok");
    h.check(!csv.empty(), "csv export ok");

    const std::set<std::string> srcReqs = extIdsOfType(s1, tl::EntityType::Requirement);
    const int srcLinks = daoLinkCount(db1);
    h.check(srcReqs.size() == 5 && srcLinks == 4, "source graph is 5 reqs + 4 links");

    // Import CSV back into a fresh DB -> graph matches.
    p::Database dbCsv;
    if (!openFreshDb(dbCsv, "lodestar_wp5_acc_csv.db")) {
        h.check(false, "open csv db");
        return;
    }
    tl::TraceLinkService sc(dbCsv);
    tl::IoService ioCsv(dbCsv);
    auto repCsv = ioCsv.importCsv(csv);
    h.check(repCsv.isOk() && repCsv.value().status == "ok",
            "CSV import clean");
    h.check(extIdsOfType(sc, tl::EntityType::Requirement) == srcReqs,
            "CSV import restores the 5 requirements (ids match)");
    h.check(daoLinkCount(dbCsv) == srcLinks,
            "CSV import restores the 4 links");

    // Import ReqIF back into another fresh DB -> graph matches.
    p::Database dbRif;
    if (!openFreshDb(dbRif, "lodestar_wp5_acc_reqif.db")) {
        h.check(false, "open reqif db");
        return;
    }
    tl::TraceLinkService sr(dbRif);
    tl::IoService ioRif(dbRif);
    auto repRif = ioRif.importReqif(reqif.value());
    h.check(repRif.isOk() && repRif.value().status == "ok",
            "ReqIF import clean");
    h.check(extIdsOfType(sr, tl::EntityType::Requirement) == srcReqs,
            "ReqIF import restores the 5 requirements (ids match)");
    h.check(daoLinkCount(dbRif) == srcLinks,
            "ReqIF import restores the 4 links");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("WP-5 import/export");
    std::printf("WP-5 IMPORT/EXPORT TESTS (migrations: %s)\n", g_migrationsDir.c_str());

    testExports(h);
    testCsvRoundTrip(h);
    testPartialImport(h);
    testAcceptance(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
