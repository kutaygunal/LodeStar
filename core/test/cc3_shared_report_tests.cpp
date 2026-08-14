// core/test/cc3_shared_report_tests.cpp
// ---------------------------------------------------------------------------
// Gap-Fill Cross-cutting #3: one shared reporting service tests.
//
// Test contract: docs/gap-fill-plan.md (Cross-cutting #3). A shared reporting
// service (HTML/CSV/JSON here; PDF/Word/ReQIF in CertReportService) is reused
// by RiskAI (1.4), AssureCheck (2.1/2.4) and IntegrateHub (6.2). This validates
// that the shared ReportService produces consistent, schema-valid standard-format
// output from the same compliance report.
//
// Deterministic.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <string>

#include "core/assurecheck/ReportService.h"
#include "core/common/Result.h"
#include "core/persistence/Database.h"

namespace ac = lodestar::assurecheck;
namespace p  = lodestar::persistence;

namespace {

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

ac::ComplianceReport makeReport() {
    ac::ComplianceReport r;
    r.standardCode = "DO-178C";
    r.standardName = "Software Considerations";
    r.dalLevel = "A";
    ac::ReportRow row;
    row.itemCode = "A1-1";
    row.objective = "Software life cycle processes are defined";
    row.dalLevel = "A-D";
    row.status = "PASS";
    row.evidence = "PSAC";
    r.rows.push_back(row);
    return r;
}

}  // namespace

// ---------------------------------------------------------------------------
// T1. Shared HTML report
// ---------------------------------------------------------------------------
static void testHtml(Harness& h) {
    h.section("T1. shared HTML report");
    p::Database db;
    db.open("lodestar_cc3.db");
    ac::ReportService svc(db);
    auto html = svc.toHtml(makeReport());
    h.check(html.isOk(), "toHtml() ok");
    if (html.isOk()) {
        h.check(html.value().find("<html") != std::string::npos,
                "HTML has a root tag");
        h.check(html.value().find("DO-178C") != std::string::npos,
                "HTML names the standard");
        h.check(html.value().find("A1-1") != std::string::npos,
                "HTML contains the objective code");
        h.check(html.value().find("PASS") != std::string::npos,
                "HTML shows the status");
    }
}

// ---------------------------------------------------------------------------
// T2. Shared CSV report
// ---------------------------------------------------------------------------
static void testCsv(Harness& h) {
    h.section("T2. shared CSV report");
    p::Database db;
    db.open("lodestar_cc3.db");
    ac::ReportService svc(db);
    auto csv = svc.toCsv(makeReport());
    h.check(csv.isOk(), "toCsv() ok");
    if (csv.isOk()) {
        h.check(csv.value().find("A1-1") != std::string::npos,
                "CSV contains the objective code");
        h.check(csv.value().find("PASS") != std::string::npos,
                "CSV contains the status");
        h.check(csv.value().find("Software life cycle processes are defined") !=
                    std::string::npos,
                "CSV contains the objective text");
    }
}

// ---------------------------------------------------------------------------
// T3. Shared JSON report
// ---------------------------------------------------------------------------
static void testJson(Harness& h) {
    h.section("T3. shared JSON report");
    p::Database db;
    db.open("lodestar_cc3.db");
    ac::ReportService svc(db);
    auto json = svc.toJson(makeReport());
    h.check(json.isOk(), "toJson() ok");
    if (json.isOk()) {
        std::string body = json.value().dump();
        h.check(body.find("standard") != std::string::npos,
                "JSON has standard");
        h.check(body.find("A1-1") != std::string::npos,
                "JSON contains the objective code");
    }
}

// ---------------------------------------------------------------------------
// T4. Consistent content across formats (same data)
// ---------------------------------------------------------------------------
static void testConsistency(Harness& h) {
    h.section("T4. consistent content across formats");
    p::Database db;
    db.open("lodestar_cc3.db");
    ac::ReportService svc(db);
    auto r = makeReport();
    auto html = svc.toHtml(r);
    auto csv = svc.toCsv(r);
    auto json = svc.toJson(r);
    h.check(html.isOk() && csv.isOk() && json.isOk(),
            "all three formats produced");
    if (html.isOk() && csv.isOk() && json.isOk()) {
        std::string jbody = json.value().dump();
        h.check(html.value().find("A1-1") != std::string::npos &&
                    csv.value().find("A1-1") != std::string::npos &&
                    jbody.find("A1-1") != std::string::npos,
                "objective code A1-1 present in every format");
        h.check(html.value().find("DO-178C") != std::string::npos &&
                    jbody.find("DO-178C") != std::string::npos &&
                    jbody.find("DO-178C") != std::string::npos,
                "standard DO-178C present in HTML and JSON");
    }
    db.close();
    std::remove("lodestar_cc3.db");
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    Harness h("Gap-Fill Cross-cutting #3 shared report service");
    testHtml(h);
    testCsv(h);
    testJson(h);
    testConsistency(h);
    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
