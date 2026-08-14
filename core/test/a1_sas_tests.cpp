// core/test/a1_sas_tests.cpp
// ---------------------------------------------------------------------------
// Gap-Fill AssureCheck 2.1 + 2.2: SAS/PSAC generation + vetted content library.
//
// Test contract: docs/gap-fill-plan.md (Module 2.1, 2.2).
//   (A) StandardsContentService loads + validates a versioned standards
//       content bundle from a data file and maps objectives to DAL A-E.
//   (B) SasService produces PSAC and SAS artifacts with a correct objectives
//       table and evidence links.
//
// Deterministic. Uses the DO-178C bundle in core/assurecheck/data.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include "core/assurecheck/SasService.h"
#include "core/assurecheck/StandardsContentService.h"
#include "core/common/Result.h"
#include "core/persistence/Database.h"

#ifndef LODESTAR_ASSURECHECK_DATA_DIR
#define LODESTAR_ASSURECHECK_DATA_DIR "core/assurecheck/data"
#endif

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

}  // namespace

// ---------------------------------------------------------------------------
// T1. Content library: load + validate the versioned bundle
// ---------------------------------------------------------------------------
static void testContentLoad(Harness& h) {
    h.section("T1. load + validate versioned standards bundle");
    ac::StandardsContentService svc;
    std::string path = std::string(LODESTAR_ASSURECHECK_DATA_DIR) +
                       "/do178c_standards.json";
    auto b = svc.loadBundle(path);
    h.check(b.isOk(), "loadBundle() ok");
    if (!b.isOk()) { std::printf("    err: %s\n", b.error().c_str()); return; }
    h.check(b.value().standardCode == "DO-178C",
            "bundle standard == DO-178C");
    h.check(b.value().bundleVersion == "1.0.0", "bundle version present");
    h.check(b.value().schemaVersion == "1.0.0", "schema version present");
    h.check(!b.value().objectives.empty(), "bundle has objectives");
    auto flat = ac::StandardsContentService::flatten(b.value());
    h.check(flat.size() >= 40, "bundle has >= 40 sub-objectives (flattened)");

    // Validate again on the loaded bundle (idempotent).
    h.check(svc.validateBundle(b.value()).isOk(),
            "validateBundle() ok on loaded bundle");

    // A structurally invalid bundle fails validation.
    ac::StandardBundle bad;
    bad.standardCode = "DO-178C";
    h.check(svc.validateBundle(bad).failed(),
            "validateBundle() rejects an empty bundle");
}

// ---------------------------------------------------------------------------
// T2. DAL A-E applicability mapping (boundaries)
// ---------------------------------------------------------------------------
static void testDalMapping(Harness& h) {
    h.section("T2. DAL A-E applicability mapping");
    h.check(ac::StandardsContentService::appliesToDal("A-D", "A"),
            "A-D applies to A");
    h.check(ac::StandardsContentService::appliesToDal("A-D", "C"),
            "A-D applies to C");
    h.check(ac::StandardsContentService::appliesToDal("A-D", "D"),
            "A-D applies to D");
    h.check(!ac::StandardsContentService::appliesToDal("A-D", "E"),
            "A-D does NOT apply to E");
    h.check(ac::StandardsContentService::appliesToDal("A-C", "C"),
            "A-C applies to C");
    h.check(!ac::StandardsContentService::appliesToDal("A-C", "D"),
            "A-C does NOT apply to D");
    h.check(ac::StandardsContentService::appliesToDal("A", "A"),
            "A applies to A only");
    h.check(!ac::StandardsContentService::appliesToDal("A", "B"),
            "A does NOT apply to B");
    h.check(!ac::StandardsContentService::appliesToDal("A-D", "Z"),
            "invalid project DAL does not apply");
    h.check(!ac::StandardsContentService::appliesToDal("", "A"),
            "empty range applies to nothing");
}

// ---------------------------------------------------------------------------
// T3. PSAC artifact
// ---------------------------------------------------------------------------
static void testPsac(Harness& h) {
    h.section("T3. PSAC artifact");
    ac::StandardsContentService content;
    auto b = content.loadBundle(
        std::string(LODESTAR_ASSURECHECK_DATA_DIR) + "/do178c_standards.json");
    if (!b.isOk()) { h.check(false, "loadBundle() ok"); return; }

    p::Database db;
    db.open("lodestar_a1_dummy.db");
    ac::SasService svc(db);
    auto psac = svc.buildPsac(b.value(), "A", "Avionics Control Unit");
    h.check(psac.isOk(), "buildPsac() ok");
    if (psac.isOk()) {
        h.check(psac.value().find("PLAN FOR SOFTWARE ASPECTS OF CERTIFICATION") !=
                    std::string::npos,
                "PSAC has the title");
        h.check(psac.value().find("DO-178C") != std::string::npos,
                "PSAC names the standard");
        h.check(psac.value().find("Software level (DAL) : A") != std::string::npos,
                "PSAC records DAL A");
        h.check(psac.value().find("[A1-1]") != std::string::npos,
                "PSAC lists sub-objective A1-1");
        // Partitioning (A2-13, DAL A-C) applies to DAL A.
        h.check(psac.value().find("[A2-13]") != std::string::npos,
                "PSAC includes the A-C partitioning objective for DAL A");
    }

    // Invalid DAL -> error.
    auto bad = svc.buildPsac(b.value(), "Z", "System");
    h.check(bad.failed(), "buildPsac() rejects invalid DAL");
}

// ---------------------------------------------------------------------------
// T4. SAS artifact with objectives->evidence mapping
// ---------------------------------------------------------------------------
static void testSas(Harness& h) {
    h.section("T4. SAS artifact + objectives->evidence");
    ac::StandardsContentService content;
    auto b = content.loadBundle(
        std::string(LODESTAR_ASSURECHECK_DATA_DIR) + "/do178c_standards.json");
    if (!b.isOk()) { h.check(false, "loadBundle() ok"); return; }

    std::map<std::string, std::string> status;
    std::map<std::string, std::string> evidence;
    status["A1-1"] = "PASS";
    evidence["A1-1"] = "PSAC approved";
    status["A3-1"] = "FAIL";

    p::Database db;
    db.open("lodestar_a1_dummy.db");
    ac::SasService svc(db);
    auto sas = svc.buildSas(b.value(), "C", status, evidence);
    h.check(sas.isOk(), "buildSas() ok");
    if (sas.isOk()) {
        h.check(sas.value().find("SOFTWARE ACCOMPLISHMENT SUMMARY") !=
                    std::string::npos,
                "SAS has the title");
        h.check(sas.value().find("status PASS") != std::string::npos,
                "SAS shows PASS for A1-1");
        h.check(sas.value().find("PSAC approved") != std::string::npos,
                "SAS shows the A1-1 evidence link");
        h.check(sas.value().find("status FAIL") != std::string::npos,
                "SAS shows FAIL for an unevidenced objective");
        h.check(sas.value().find("Summary: 1 PASS") != std::string::npos,
                "SAS summary counts 1 PASS");
    }

    // mapEvidence: applicable objectives only.
    auto rows = ac::SasService::mapEvidence(b.value(), "C", status, evidence);
    h.check(!rows.empty(), "mapEvidence() returns applicable rows");
    // Partitioning (A2-13, DAL A-C) DOES apply to project DAL C.
    bool hasA113 = false;
    for (const auto& r : rows) if (r.subCode == "A2-13") hasA113 = true;
    h.check(hasA113, "A2-13 (DAL A-C) applies to project DAL C");

    // For project DAL E, the A-C partitioning objective is excluded.
    auto rowsE = ac::SasService::mapEvidence(b.value(), "E", status, evidence);
    bool hasA113E = false;
    for (const auto& r : rowsE) if (r.subCode == "A2-13") hasA113E = true;
    h.check(!hasA113E, "A2-13 (DAL A-C) excluded for project DAL E");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    Harness h("Gap-Fill AssureCheck 2.1/2.2 SAS/PSAC + content library");
    testContentLoad(h);
    testDalMapping(h);
    testPsac(h);
    testSas(h);
    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
