// core/test/wp4_assurecheck_tests.cpp
// ---------------------------------------------------------------------------
// Phase 11 WP-4 (AssureCheck) compliance reporting tests (test-first).
//
// Written by the scrum-master BEFORE the WP-4 engineer implements the feature.
// The engineer must implement the contract documented below so these tests
// compile and pass. Do NOT weaken the assertions to make them pass; implement
// the feature to satisfy them.
//
// Covers (docs/wp4-assurecheck-task.md): the ReportService — certification-
// ready reports per standard and per DAL, objective-coverage percentages, and
// HTML/CSV/JSON export.
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G.
// Each DB-dependent test opens its own fresh throwaway DB and runs migrations.
// ---------------------------------------------------------------------------
// CONTRACT the WP-4 engineer must provide.
// ---------------------------------------------------------------------------
// (A) core/assurecheck/ReportService.h (+ .cpp) with the exact API below.
// ---------------------------------------------------------------------------

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/adapters/Json.h"
#include "core/assurecheck/AssureCheckService.h"
#include "core/assurecheck/ComplianceEngine.h"
#include "core/assurecheck/ReportService.h"
#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace ac = lodestar::assurecheck;
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

// Seeds the five standards and returns true on success.
bool seed(p::Database& db, Harness& h) {
    ac::AssureCheckService svc(db);
    auto seed = svc.seedStandards();
    h.check(seed.isOk(), "seedStandards() ok");
    return seed.isOk();
}

// Inserts the T1 project data: one requirements row, one design_items row, one
// test_cases row (result_status='Passed'), and one trace_links row.
void insertT1Data(p::Database& db) {
    db.execute("INSERT INTO requirements (id, name) VALUES ('req1', 'Req 1');");
    db.execute("INSERT INTO design_items (id, name) VALUES ('des1', 'Des 1');");
    db.execute("INSERT INTO test_cases (id, name, result_status) "
               "VALUES ('tc1', 'TC 1', 'Passed');");
    db.execute("INSERT INTO trace_links (id, source_type, source_id, "
               "target_type, target_id, relation) "
               "VALUES ('tl1', 'requirement', 'req1', 'design', 'des1', "
               "'traces_to');");
}

// Returns the row for the given itemCode, or nullptr if absent.
const ac::ReportRow* findRow(const ac::ComplianceReport& report,
                             const std::string& itemCode) {
    for (const auto& r : report.rows) {
        if (r.itemCode == itemCode) return &r;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// T1. buildReport computes coverage (all PASS)
// ---------------------------------------------------------------------------
void testAllPass(Harness& h) {
    h.section("T1. buildReport computes coverage (all PASS)");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp4_ac_t1.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    if (!seed(db, h)) {
        db.close();
        return;
    }
    insertT1Data(db);

    ac::ComplianceEngine engine(db);
    auto res = engine.runChecks("DO-178C", "A");
    h.check(res.isOk(), "runChecks(\"DO-178C\", \"A\") ok");
    if (!res.isOk()) {
        db.close();
        return;
    }

    ac::ReportService svc(db);
    auto rep = svc.buildReport("DO-178C", "A", res.value());
    h.check(rep.isOk(), "buildReport(\"DO-178C\", \"A\", results) ok");
    if (!rep.isOk()) {
        db.close();
        return;
    }
    const auto& cov = rep.value().coverage;
    h.check(cov.total == 82, "coverage.total == 82");
    h.check(cov.na == 0, "coverage.na == 0");
    h.check(cov.pass == 82, "coverage.pass == 82");
    h.check(cov.fail == 0, "coverage.fail == 0");
    h.check(cov.percent == 100, "coverage.percent == 100");

    db.close();
    std::remove("lodestar_wp4_ac_t1.db");
    std::remove("lodestar_wp4_ac_t1.db-wal");
    std::remove("lodestar_wp4_ac_t1.db-shm");
}

// ---------------------------------------------------------------------------
// T2. Coverage with NA (DAL B)
// ---------------------------------------------------------------------------
void testCoverageNa(Harness& h) {
    h.section("T2. Coverage with NA (DAL B)");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp4_ac_t2.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    if (!seed(db, h)) {
        db.close();
        return;
    }
    insertT1Data(db);

    ac::ComplianceEngine engine(db);
    auto res = engine.runChecks("DO-178C", "B");
    h.check(res.isOk(), "runChecks(\"DO-178C\", \"B\") ok");
    if (!res.isOk()) {
        db.close();
        return;
    }

    ac::ReportService svc(db);
    auto rep = svc.buildReport("DO-178C", "B", res.value());
    h.check(rep.isOk(), "buildReport(\"DO-178C\", \"B\", results) ok");
    if (!rep.isOk()) {
        db.close();
        return;
    }
    const auto& cov = rep.value().coverage;
    h.check(cov.total == 82, "coverage.total == 82");
    h.check(cov.na == 1, "coverage.na == 1 (item A6-10)");
    h.check(cov.applicable == 81, "coverage.applicable == 81");
    h.check(cov.pass == 81, "coverage.pass == 81");
    h.check(cov.percent == 100, "coverage.percent == 100");

    db.close();
    std::remove("lodestar_wp4_ac_t2.db");
    std::remove("lodestar_wp4_ac_t2.db-wal");
    std::remove("lodestar_wp4_ac_t2.db-shm");
}

// ---------------------------------------------------------------------------
// T3. Report rows include objective text
// ---------------------------------------------------------------------------
void testObjectiveText(Harness& h) {
    h.section("T3. Report rows include objective text");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp4_ac_t3.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    if (!seed(db, h)) {
        db.close();
        return;
    }
    insertT1Data(db);

    ac::ComplianceEngine engine(db);
    auto res = engine.runChecks("DO-178C", "A");
    h.check(res.isOk(), "runChecks(\"DO-178C\", \"A\") ok");
    if (!res.isOk()) {
        db.close();
        return;
    }

    ac::ReportService svc(db);
    auto rep = svc.buildReport("DO-178C", "A", res.value());
    h.check(rep.isOk(), "buildReport ok");
    if (!rep.isOk()) {
        db.close();
        return;
    }
    const ac::ReportRow* a2_1 = findRow(rep.value(), "A2-1");
    h.check(a2_1 != nullptr, "row A2-1 present");
    if (a2_1 != nullptr) {
        h.check(a2_1->objective == "High-level requirements are developed",
                "A2-1 objective == 'High-level requirements are developed'");
        h.check(a2_1->status == "PASS", "A2-1 status == PASS");
    }

    db.close();
    std::remove("lodestar_wp4_ac_t3.db");
    std::remove("lodestar_wp4_ac_t3.db-wal");
    std::remove("lodestar_wp4_ac_t3.db-shm");
}

// ---------------------------------------------------------------------------
// T4. toHtml produces a valid HTML report
// ---------------------------------------------------------------------------
void testToHtml(Harness& h) {
    h.section("T4. toHtml produces a valid HTML report");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp4_ac_t4.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    if (!seed(db, h)) {
        db.close();
        return;
    }
    insertT1Data(db);

    ac::ComplianceEngine engine(db);
    auto res = engine.runChecks("DO-178C", "A");
    h.check(res.isOk(), "runChecks ok");
    if (!res.isOk()) {
        db.close();
        return;
    }

    ac::ReportService svc(db);
    auto rep = svc.buildReport("DO-178C", "A", res.value());
    h.check(rep.isOk(), "buildReport ok");
    if (!rep.isOk()) {
        db.close();
        return;
    }
    auto html = svc.toHtml(rep.value());
    h.check(html.isOk(), "toHtml ok");
    if (!html.isOk()) {
        db.close();
        return;
    }
    const std::string& out = html.value();
    std::string lower = out;
    for (auto& c : lower) c = static_cast<char>(std::tolower(c));
    h.check(lower.find("<html") != std::string::npos,
            "html contains '<html' (case-insensitive)");
    h.check(out.find("DO-178C") != std::string::npos,
            "html contains 'DO-178C'");
    h.check(out.find("PASS") != std::string::npos, "html contains 'PASS'");
    h.check(out.find("100") != std::string::npos,
            "html contains '100' (coverage percent)");

    db.close();
    std::remove("lodestar_wp4_ac_t4.db");
    std::remove("lodestar_wp4_ac_t4.db-wal");
    std::remove("lodestar_wp4_ac_t4.db-shm");
}

// ---------------------------------------------------------------------------
// T5. toCsv produces CSV with header + rows
// ---------------------------------------------------------------------------
void testToCsv(Harness& h) {
    h.section("T5. toCsv produces CSV with header + rows");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp4_ac_t5.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    if (!seed(db, h)) {
        db.close();
        return;
    }
    insertT1Data(db);

    ac::ComplianceEngine engine(db);
    auto res = engine.runChecks("DO-178C", "A");
    h.check(res.isOk(), "runChecks ok");
    if (!res.isOk()) {
        db.close();
        return;
    }

    ac::ReportService svc(db);
    auto rep = svc.buildReport("DO-178C", "A", res.value());
    h.check(rep.isOk(), "buildReport ok");
    if (!rep.isOk()) {
        db.close();
        return;
    }
    auto csv = svc.toCsv(rep.value());
    h.check(csv.isOk(), "toCsv ok");
    if (!csv.isOk()) {
        db.close();
        return;
    }
    const std::string& out = csv.value();
    const std::string header = "item_code,objective,dal_level,status,evidence";
    h.check(out.compare(0, header.size(), header) == 0,
            "first line is exactly the CSV header");
    h.check(out.find("A2-1") != std::string::npos, "csv contains 'A2-1'");
    h.check(out.find("PASS") != std::string::npos, "csv contains 'PASS'");

    db.close();
    std::remove("lodestar_wp4_ac_t5.db");
    std::remove("lodestar_wp4_ac_t5.db-wal");
    std::remove("lodestar_wp4_ac_t5.db-shm");
}

// ---------------------------------------------------------------------------
// T6. toJson produces structured JSON
// ---------------------------------------------------------------------------
void testToJson(Harness& h) {
    h.section("T6. toJson produces structured JSON");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp4_ac_t6.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    if (!seed(db, h)) {
        db.close();
        return;
    }
    insertT1Data(db);

    ac::ComplianceEngine engine(db);
    auto res = engine.runChecks("DO-178C", "A");
    h.check(res.isOk(), "runChecks ok");
    if (!res.isOk()) {
        db.close();
        return;
    }

    ac::ReportService svc(db);
    auto rep = svc.buildReport("DO-178C", "A", res.value());
    h.check(rep.isOk(), "buildReport ok");
    if (!rep.isOk()) {
        db.close();
        return;
    }
    auto jr = svc.toJson(rep.value());
    h.check(jr.isOk(), "toJson ok");
    if (!jr.isOk()) {
        db.close();
        return;
    }
    const lodestar::Json& json = jr.value();
    h.check(json.at("standard").asString() == "DO-178C",
            "json['standard'] == 'DO-178C'");
    h.check(json.at("dal").asString() == "A", "json['dal'] == 'A'");
    h.check(json.at("coverage").at("total").asNumber() == 82,
            "json['coverage']['total'] == 82");
    h.check(json.at("rows").size() == 82, "json['rows'].size() == 82");

    db.close();
    std::remove("lodestar_wp4_ac_t6.db");
    std::remove("lodestar_wp4_ac_t6.db-wal");
    std::remove("lodestar_wp4_ac_t6.db-shm");
}

// ---------------------------------------------------------------------------
// T7. Report per DAL (DAL B has an NA row)
// ---------------------------------------------------------------------------
void testDalNaRow(Harness& h) {
    h.section("T7. Report per DAL (DAL B has an NA row)");

    p::Database db;
    if (!openFreshDb(db, "lodestar_wp4_ac_t7.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    if (!seed(db, h)) {
        db.close();
        return;
    }
    insertT1Data(db);

    ac::ComplianceEngine engine(db);
    auto res = engine.runChecks("DO-178C", "B");
    h.check(res.isOk(), "runChecks(\"DO-178C\", \"B\") ok");
    if (!res.isOk()) {
        db.close();
        return;
    }

    ac::ReportService svc(db);
    auto rep = svc.buildReport("DO-178C", "B", res.value());
    h.check(rep.isOk(), "buildReport(\"DO-178C\", \"B\", results) ok");
    if (!rep.isOk()) {
        db.close();
        return;
    }
    const ac::ReportRow* a6_10 = findRow(rep.value(), "A6-10");
    h.check(a6_10 != nullptr, "row A6-10 present");
    if (a6_10 != nullptr) {
        h.check(a6_10->status == "NA", "A6-10 status == NA");
    }

    db.close();
    std::remove("lodestar_wp4_ac_t7.db");
    std::remove("lodestar_wp4_ac_t7.db-wal");
    std::remove("lodestar_wp4_ac_t7.db-shm");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("WP-4 AssureCheck compliance reporting");
    std::printf("WP-4 ASSURECHECK COMPLIANCE REPORTING TESTS (migrations: %s)\n",
                g_migrationsDir.c_str());

    testAllPass(h);
    testCoverageNa(h);
    testObjectiveText(h);
    testToHtml(h);
    testToCsv(h);
    testToJson(h);
    testDalNaRow(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
