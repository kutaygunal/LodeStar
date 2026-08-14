// core/test/r2_fmea_assess_tests.cpp
// ---------------------------------------------------------------------------
// Gap-Fill RiskAI 1.3: FMEA assessment of existing documents tests.
//
// Test contract: docs/gap-fill-plan.md (Module 1.3).
//   (A) core/riskai/FmeaAssessor.h (+ .cpp) parses imported FMEA content
//       (CSV/text) into structured rows.
//   (B) A checklist-based quality assessor (completeness, clarity, severity
//       justification, detection adequacy, risk-scoring consistency) emits a
//       per-item improvement recommendation list with known-good scores on
//       fixture FMEA inputs.
//
// Deterministic: uses a MockAdapter (no live LLM). The deterministic rule
// engine is the source of truth for these tests.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <string>
#include <vector>

#include "core/adapters/Adapter.h"
#include "core/adapters/MockAdapter.h"
#include "core/riskai/FmeaAssessor.h"

namespace ra = lodestar::riskai;

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

// Find the first finding for a row+dimension, or nullptr.
const ra::FmeaFinding* findFinding(const ra::FmeaAssessmentResult& res,
                                   const std::string& row,
                                   const std::string& dim) {
    for (const auto& f : res.findings) {
        if (f.rowId == row && f.dimension == dim) return &f;
    }
    return nullptr;
}

int countDim(const ra::FmeaAssessmentResult& res, const std::string& dim) {
    int n = 0;
    for (const auto& f : res.findings) if (f.dimension == dim) ++n;
    return n;
}

// ---------------------------------------------------------------------------
// T1. parseImport: CSV/text -> structured rows
// ---------------------------------------------------------------------------
void testParse(Harness& h) {
    h.section("T1. parseImport CSV/text -> structured rows");
    lodestar::adapters::MockAdapter mock;
    lodestar::adapters::AdapterConfig cfg;
    ra::FmeaAssessor ass(mock, cfg);

    const char* text =
        "7|6|6|Actuator|Servo stalls|Control surface frozen|Motor short|fuse|thermal sensor\n"
        "8|4|3|Receiver|Loss of lock|Position error|RF interference||\n";
    std::vector<ra::ImportedFmeaRow> rows;
    h.check(ass.parseImport(text, rows), "parseImport() returns true");
    h.check(rows.size() == 2, "parses 2 rows");
    if (rows.size() == 2) {
        h.check(rows[0].severity == 7, "row1 severity == 7");
        h.check(rows[0].occurrence == 6, "row1 occurrence == 6");
        h.check(rows[0].detectionRating == 6, "row1 detection == 6");
        h.check(rows[0].failureMode == "Servo stalls", "row1 FM == \"Servo stalls\"");
        h.check(rows[0].effect == "Control surface frozen", "row1 FE correct");
        h.check(rows[0].cause == "Motor short", "row1 FC correct");
        h.check(rows[0].preventive == "fuse", "row1 preventive == \"fuse\"");
        h.check(rows[0].detection == "thermal sensor", "row1 detection == \"thermal sensor\"");
        h.check(rows[1].failureMode == "Loss of lock", "row2 FM == \"Loss of lock\"");
    }

    // Header line is skipped.
    const char* hdr =
        "severity|occurrence|detection|function|failureMode|effect|cause\n"
        "7|6|6|Actuator|Servo stalls|Frozen|Short\n";
    std::vector<ra::ImportedFmeaRow> hrows;
    h.check(ass.parseImport(hdr, hrows), "parseImport() with header ok");
    h.check(hrows.size() == 1, "header line skipped (1 data row)");
}

// ---------------------------------------------------------------------------
// T2. Assessor flags completeness + clarity + severity justification
// ---------------------------------------------------------------------------
void testCompletenessAndClarity(Harness& h) {
    h.section("T2. completeness + clarity + severity justification findings");
    lodestar::adapters::MockAdapter mock;
    lodestar::adapters::AdapterConfig cfg;
    ra::FmeaAssessor ass(mock, cfg);

    // Row with FM but missing FE, FC, and a vague effect.
    ra::ImportedFmeaRow r;
    r.id = "R1";
    r.failureMode = "Servo stalls";
    r.effect = "etc";   // vague, not substantial
    r.severity = 8;
    r.occurrence = 4;
    r.detectionRating = 3;
    std::vector<ra::ImportedFmeaRow> rows{r};

    auto res = ra::FmeaAssessor::deterministicAssess(rows);
    h.check(findFinding(res, "R1", "completeness") != nullptr,
            "missing cause flags a completeness finding");
    h.check(findFinding(res, "R1", "clarity") != nullptr,
            "vague wording flags a clarity finding");
    h.check(findFinding(res, "R1", "severity_justification") != nullptr,
            "unsubstantiated severity flags a severity_justification finding");
}

// ---------------------------------------------------------------------------
// T3. Detection adequacy + risk consistency
// ---------------------------------------------------------------------------
void testDetectionAndRisk(Harness& h) {
    h.section("T3. detection adequacy + risk-scoring consistency");
    lodestar::adapters::MockAdapter mock;
    lodestar::adapters::AdapterConfig cfg;
    ra::FmeaAssessor ass(mock, cfg);

    // Poor detection (D=8) with no detection control, plus high RPN but no cause.
    ra::ImportedFmeaRow r;
    r.id = "R2";
    r.failureMode = "Loss of lock";
    r.effect = "Aircraft position uncertainty increases; guidance degraded";
    r.severity = 8;
    r.occurrence = 7;
    r.detectionRating = 8;
    std::vector<ra::ImportedFmeaRow> rows{r};

    auto res = ra::FmeaAssessor::deterministicAssess(rows);
    h.check(findFinding(res, "R2", "detection_adequacy") != nullptr,
            "D>=7 with no controls flags detection_adequacy");
    h.check(findFinding(res, "R2", "risk_consistency") != nullptr,
            "high RPN with no cause flags risk_consistency");
    h.check(findFinding(res, "R2", "completeness") != nullptr,
            "missing cause also flags completeness");
}

// ---------------------------------------------------------------------------
// T4. Clean row produces few/no findings and high score
// ---------------------------------------------------------------------------
void testCleanRow(Harness& h) {
    h.section("T4. clean row => high score, no findings");
    lodestar::adapters::MockAdapter mock;
    lodestar::adapters::AdapterConfig cfg;
    ra::FmeaAssessor ass(mock, cfg);

    ra::ImportedFmeaRow r;
    r.id = "R3";
    r.failureMode = "Servo stalls";
    r.effect = "Control surface freezes; loss of primary flight control";
    r.cause = "Motor winding short circuit in servo coil";
    r.preventive = "Overcurrent protection fuse";
    r.detection = "In-line current sensor trips alarm";
    r.severity = 8;
    r.occurrence = 4;
    r.detectionRating = 3;
    std::vector<ra::ImportedFmeaRow> rows{r};

    auto res = ra::FmeaAssessor::deterministicAssess(rows);
    h.check(res.findings.empty(), "clean row produces no findings");
    h.check(res.score == 100, "clean row scores 100");
    h.check(res.criticalCount == 0 && res.majorCount == 0,
            "no critical/major findings on clean row");
}

// ---------------------------------------------------------------------------
// T5. Scoring aggregation: critical/major/minor counts and score reduction
// ---------------------------------------------------------------------------
void testScoreAggregation(Harness& h) {
    h.section("T5. score aggregation");
    lodestar::adapters::MockAdapter mock;
    lodestar::adapters::AdapterConfig cfg;
    ra::FmeaAssessor ass(mock, cfg);

    // Completely empty row -> many findings.
    ra::ImportedFmeaRow r;
    r.id = "R4";  // FM empty -> critical completeness
    std::vector<ra::ImportedFmeaRow> rows{r};
    auto res = ra::FmeaAssessor::deterministicAssess(rows);
    h.check(res.findings.size() >= 3, "empty row yields multiple findings");
    h.check(res.criticalCount >= 1, "empty FM counts as critical");
    h.check(res.score < 100, "empty row reduces the score below 100");
}

}  // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    Harness h("Gap-Fill RiskAI 1.3 FMEA assessment");

    testParse(h);
    testCompletenessAndClarity(h);
    testDetectionAndRisk(h);
    testCleanRow(h);
    testScoreAggregation(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
