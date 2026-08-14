// core/test/f2_tool_qual_dossier_tests.cpp
// ---------------------------------------------------------------------------
// Gap-Fill TestForge 4.2: DO-330 tool-qualification evidence pack tests.
//
// Test contract: docs/gap-fill-plan.md (Module 4.2).
//   (A) ToolQualificationDossierService builds a DO-330 dossier (purpose,
//       operational environment, verification results, deviation/limitation
//       log) documenting the exact toolchain versions and the verification run.
//   (B) The dossier is reproducible (hash), renders a complete artifact, and
//       validates by re-running the qualification test set (evidence, not claims).
//
// Deterministic.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <string>
#include <vector>

#include "core/testforge/ToolQualificationDossier.h"

namespace tf = lodestar::testforge;

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
// T1. Build a complete, reproducible dossier
// ---------------------------------------------------------------------------
static void testBuild(Harness& h) {
    h.section("T1. build a complete reproducible dossier");
    std::vector<tf::ToolchainComponent> toolchain;
    toolchain.push_back({"llvm-cov", "19.1.0", "branch/decision/MC-DC measurement"});
    toolchain.push_back({"clang-cl", "19.1.0", "compiler instrumentation"});

    std::vector<tf::VerificationRun> runs;
    runs.push_back({"f1_mcdc:testParse", "PASS", "2 files parsed"});
    runs.push_back({"f1_mcdc:testUnconditional", "PASS", "1 branch counted"});

    auto d = tf::ToolQualificationDossierService::build(
        "TQL-4", "Measure structural coverage for DO-178C verification",
        "Windows 11 x64, clang 19.1.0", toolchain, runs,
        {"MC-DC independent-condition analysis is a manual review step"});

    h.check(d.title == "Coverage Tool Qualification Dossier",
            "dossier has the title");
    h.check(d.toolQualificationLevel == "TQL-4", "TQL recorded");
    h.check(!d.purpose.empty(), "purpose documented");
    h.check(!d.operationalEnvironment.empty(), "operational environment documented");
    h.check(d.toolchain.size() == 2, "toolchain versions documented");
    h.check(d.verificationResults.size() == 2, "verification runs recorded");
    h.check(d.deviationsLimitations.size() == 1, "deviation/limitation log present");
    h.check(!d.reproducibilityHash.empty(), "reproducibility hash computed");

    // Rebuilding the same dossier produces the same hash (reproducible).
    auto d2 = tf::ToolQualificationDossierService::build(
        "TQL-4", "Measure structural coverage for DO-178C verification",
        "Windows 11 x64, clang 19.1.0", toolchain, runs,
        {"MC-DC independent-condition analysis is a manual review step"});
    h.check(d.reproducibilityHash == d2.reproducibilityHash,
            "dossier is reproducible (identical hash)");
    h.check(tf::ToolQualificationDossierService::verifyReproducible(d),
            "verifyReproducible() true");
}

// ---------------------------------------------------------------------------
// T2. Render a complete artifact
// ---------------------------------------------------------------------------
static void testRender(Harness& h) {
    h.section("T2. render the dossier artifact");
    std::vector<tf::ToolchainComponent> toolchain;
    toolchain.push_back({"llvm-cov", "19.1.0", "measurement"});
    std::vector<tf::VerificationRun> runs;
    runs.push_back({"f1_mcdc:testParse", "PASS", ""});

    auto d = tf::ToolQualificationDossierService::build(
        "TQL-4", "Purpose", "Env", toolchain, runs, {"limitation"});
    std::string text = tf::ToolQualificationDossierService::render(d);
    h.check(text.find("Coverage Tool Qualification Dossier") != std::string::npos,
            "rendered artifact has the title");
    h.check(text.find("llvm-cov 19.1.0") != std::string::npos,
            "rendered artifact documents the exact toolchain version");
    h.check(text.find("f1_mcdc:testParse") != std::string::npos,
            "rendered artifact includes the verification run");
    h.check(text.find("limitation") != std::string::npos,
            "rendered artifact includes the limitation log");
    h.check(text.find("Reproducibility hash") != std::string::npos,
            "rendered artifact includes the reproducibility hash");
}

// ---------------------------------------------------------------------------
// T3. Validate by re-running the qualification test set
// ---------------------------------------------------------------------------
static void testValidate(Harness& h) {
    h.section("T3. validate by re-running the qualification set");
    std::vector<tf::ToolchainComponent> toolchain;
    toolchain.push_back({"llvm-cov", "19.1.0", "measurement"});

    std::vector<tf::VerificationRun> allPass;
    allPass.push_back({"f1_mcdc:testParse", "PASS", ""});
    allPass.push_back({"f1_mcdc:testPercentages", "PASS", ""});
    auto good = tf::ToolQualificationDossierService::build(
        "TQL-4", "P", "E", toolchain, allPass, {});
    auto ok = tf::ToolQualificationDossierService().validateByReRun(good);
    h.check(ok.isOk() && ok.value() == 2,
            "re-run validates 2 passing qualification tests");

    // A failing qualification test fails validation (evidence, not claims).
    std::vector<tf::VerificationRun> withFail;
    withFail.push_back({"f1_mcdc:testParse", "PASS", ""});
    withFail.push_back({"f1_mcdc:testUnconditional", "FAIL", "branch miscount"});
    auto bad = tf::ToolQualificationDossierService::build(
        "TQL-4", "P", "E", toolchain, withFail, {});
    auto no = tf::ToolQualificationDossierService().validateByReRun(bad);
    h.check(no.failed(), "re-run fails when a qualification test does not pass");
    h.check(no.errorCode() == lodestar::common::ErrorCode::ValidationFailed,
            "failed re-run reports ValidationFailed");
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    Harness h("Gap-Fill TestForge 4.2 tool-qualification evidence pack");
    testBuild(h);
    testRender(h);
    testValidate(h);
    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
