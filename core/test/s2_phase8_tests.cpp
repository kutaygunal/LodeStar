// core/test/s2_phase8_tests.cpp
// ---------------------------------------------------------------------------
// Sprint 2 Phase 8 (certification-ready reporting + traceability) unit tests.
//
// Written by the scrum-master BEFORE the Phase 8 engineer implements the
// feature. The engineer must implement the contract documented below so these
// tests compile and pass. Do NOT weaken the assertions to make them pass;
// implement the feature to satisfy them.
//
// Covers (docs/s2-phase8-test.md): PDF / Word (docx) / ReQIF export of
// compliance reports and requirements+trace links, and result->requirement
// traceability.
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / S1 phases.
// ---------------------------------------------------------------------------
// CONTRACT the Phase 8 engineer must provide (in core/assurecheck/):
//   class CertReportService {
//     explicit CertReportService(persistence::Database& db);
//     common::Result<std::vector<uint8_t>> exportPdf(const ComplianceReport&);
//     common::Result<std::vector<uint8_t>> exportWord(const ComplianceReport&);
//     common::Result<std::vector<uint8_t>> exportReqif(
//         const std::vector<tracelink::Entity>& requirements,
//         const std::vector<tracelink::Link>& links);
//     common::Result<std::vector<std::string>> traceResultToRequirements(
//         const std::string& resultId);
//   };
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/assurecheck/CertReportService.h"
#include "core/assurecheck/ReportService.h"
#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/tracelink/Types.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace ac = lodestar::assurecheck;
namespace p  = lodestar::persistence;
namespace tl = lodestar::tracelink;

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

// A small in-memory compliance report (no DB needed for export tests).
ac::ComplianceReport makeReport() {
    ac::ComplianceReport rep;
    rep.standardCode = "DO-178C";
    rep.standardName = "Software Considerations in Airborne Systems";
    rep.dalLevel = "A";
    rep.coverage.total = 2;
    rep.coverage.applicable = 2;
    rep.coverage.pass = 2;
    rep.coverage.fail = 0;
    rep.coverage.na = 0;
    rep.coverage.warning = 0;
    rep.coverage.percent = 100;

    ac::ReportRow r1;
    r1.itemCode = "A2-1";
    r1.objective = "High-level requirements are developed";
    r1.dalLevel = "A";
    r1.status = "PASS";
    r1.evidence = "test_case:tc1";
    rep.rows.push_back(r1);

    ac::ReportRow r2;
    r2.itemCode = "A2-2";
    r2.objective = "High-level requirements are verifiable";
    r2.dalLevel = "A";
    r2.status = "PASS";
    r2.evidence = "test_case:tc2";
    rep.rows.push_back(r2);
    return rep;
}

// ---------------------------------------------------------------------------
// T1. PDF export produces non-empty bytes
// ---------------------------------------------------------------------------
void testPdf(Harness& h, ac::CertReportService& svc) {
    h.section("T1. PDF export produces non-empty bytes");

    auto pdf = svc.exportPdf(makeReport());
    h.check(pdf.isOk(), "exportPdf(report) ok");
    if (pdf.isOk()) {
        h.check(!pdf.value().empty(), "exportPdf returns non-empty bytes");
        h.check(pdf.value().size() > 100,
                "exportPdf returns a substantial body (>100 bytes)");
    }
}

// ---------------------------------------------------------------------------
// T2. Word export produces non-empty bytes
// ---------------------------------------------------------------------------
void testWord(Harness& h, ac::CertReportService& svc) {
    h.section("T2. Word export produces non-empty bytes");

    auto doc = svc.exportWord(makeReport());
    h.check(doc.isOk(), "exportWord(report) ok");
    if (doc.isOk()) {
        h.check(!doc.value().empty(), "exportWord returns non-empty bytes");
        h.check(doc.value().size() > 100,
                "exportWord returns a substantial body (>100 bytes)");
    }
}

// ---------------------------------------------------------------------------
// T3. ReQIF export contains requirements + links
// ---------------------------------------------------------------------------
void testReqif(Harness& h, ac::CertReportService& svc) {
    h.section("T3. ReQIF export contains requirements + links");

    tl::Entity req1;
    req1.id = "r1";
    req1.externalId = "REQ-100";
    req1.name = "Position output";
    req1.text = "The system shall provide GNSS position output.";

    tl::Entity req2;
    req2.id = "r2";
    req2.externalId = "REQ-200";
    req2.name = "Altitude accuracy";
    req2.text = "The system shall report altitude within tolerance.";

    tl::Link link;
    link.id = "tl1";
    link.sourceId = "REQ-100";
    link.targetId = "REQ-200";
    link.relation = "verifies";

    auto reqif = svc.exportReqif({req1, req2}, {link});
    h.check(reqif.isOk(), "exportReqif(reqs, links) ok");
    if (reqif.isOk()) {
        const std::vector<std::uint8_t>& bytes = reqif.value();
        h.check(!bytes.empty(), "exportReqif returns non-empty bytes");
        std::string content(bytes.begin(), bytes.end());
        h.check(content.find("REQ-100") != std::string::npos,
                "reqif content references requirement id REQ-100");
        h.check(content.find("REQ-200") != std::string::npos,
                "reqif content references requirement id REQ-200");
        h.check(content.find("tl1") != std::string::npos,
                "reqif content references the trace link id tl1");
    }
}

// ---------------------------------------------------------------------------
// T4. result->requirement traceability
// ---------------------------------------------------------------------------
void testTraceability(Harness& h, ac::CertReportService& svc) {
    h.section("T4. result->requirement traceability");

    // A test case result that verifies two requirements via trace links.
    auto reqs = svc.traceResultToRequirements("tc1");
    h.check(reqs.isOk(), "traceResultToRequirements(\"tc1\") ok");
    if (reqs.isOk()) {
        h.check(reqs.value().size() == 2,
                "result tc1 verifies 2 requirements");
        bool hasReq1 = false;
        bool hasReq2 = false;
        for (const auto& r : reqs.value()) {
            if (r == "req1") hasReq1 = true;
            if (r == "req2") hasReq2 = true;
        }
        h.check(hasReq1, "result tc1 verifies requirement req1");
        h.check(hasReq2, "result tc1 verifies requirement req2");
    }

    // A result with no verifying links returns an empty list (not an error).
    auto none = svc.traceResultToRequirements("tc_missing");
    h.check(none.isOk(), "traceResultToRequirements(\"tc_missing\") ok");
    if (none.isOk()) {
        h.check(none.value().empty(),
                "a result with no verifying links returns an empty list");
    }
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::string migrationsDir = LODESTAR_MIGRATIONS_DIR;
    if (argc > 1) {
        migrationsDir = argv[1];
    }

    const std::string dbPath = "lodestar_s2_phase8_tests.db";
    std::remove(dbPath.c_str());
    std::remove((dbPath + "-wal").c_str());
    std::remove((dbPath + "-shm").c_str());

    p::Database db;
    auto open = db.open(dbPath);
    if (open.failed()) {
        std::fprintf(stderr, "S2 PHASE8 TESTS FAIL: open db: %s\n",
                     open.error().c_str());
        return 1;
    }

    p::MigrationRunner runner(db);
    auto mig = runner.run(migrationsDir);
    if (mig.failed()) {
        std::fprintf(stderr, "S2 PHASE8 TESTS FAIL: migrate: %s\n",
                     mig.error().c_str());
        db.close();
        std::remove(dbPath.c_str());
        return 1;
    }

    // Seed the traceability data for T4: two requirements, one test case
    // result, and two 'verifies' trace links from the test case to them.
    db.execute("INSERT INTO requirements (id, name) VALUES ('req1', 'Req 1');");
    db.execute("INSERT INTO requirements (id, name) VALUES ('req2', 'Req 2');");
    db.execute("INSERT INTO test_cases (id, name, result_status) "
               "VALUES ('tc1', 'TC 1', 'Passed');");
    db.execute("INSERT INTO trace_links (id, source_type, source_id, "
               "target_type, target_id, relation, status) "
               "VALUES ('tl1', 'test_case', 'tc1', 'requirement', 'req1', "
               "'verifies', 'Active');");
    db.execute("INSERT INTO trace_links (id, source_type, source_id, "
               "target_type, target_id, relation, status) "
               "VALUES ('tl2', 'test_case', 'tc1', 'requirement', 'req2', "
               "'verifies', 'Active');");

    ac::CertReportService svc(db);

    Harness h("S2 Phase 8 certification-ready reporting + traceability");
    std::printf("S2 PHASE 8 CERTIFICATION-READY REPORTING + TRACEABILITY TESTS "
                "(schema v%d)\n", mig.value());

    testPdf(h, svc);
    testWord(h, svc);
    testReqif(h, svc);
    testTraceability(h, svc);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());

    db.close();
    std::remove(dbPath.c_str());
    std::remove((dbPath + "-wal").c_str());
    std::remove((dbPath + "-shm").c_str());

    return h.failures() == 0 ? 0 : 1;
}
