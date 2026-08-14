// core/riskai/FmeaAssessor.h
// Gap-Fill RiskAI 1.3: FMEA assessment of existing documents.
//
// Takes an existing FMEA (imported from CSV/XLSX/PDF text) and runs a
// checklist-based quality assessor against each row: completeness, clarity,
// severity justification, detection adequacy, and risk-scoring consistency.
// Emits a per-item improvement recommendation list.
//
// Uses the local LLM adapter for semantic assessment when available; falls back
// to a deterministic rule engine so the assessor always returns a result and
// stays fully deterministic in CI (no live LLM required).

#pragma once

#include <string>
#include <vector>

#include "core/adapters/Adapter.h"

namespace lodestar::riskai {

// One imported failure row (parsed from an external FMEA document).
struct ImportedFmeaRow {
    std::string id;           // optional row identity from the source
    std::string function;     // function element
    std::string failureMode;  // FM
    std::string effect;       // FE
    std::string cause;        // FC
    std::string preventive;   // existing preventive controls
    std::string detection;    // existing detection controls
    int severity = 0;         // S 1..10 (0 = missing)
    int occurrence = 0;       // O 1..10 (0 = missing)
    int detectionRating = 0;  // D 1..10 (0 = missing)
};

// A checklist-based quality finding on one row.
struct FmeaFinding {
    std::string rowId;
    std::string dimension;   // completeness | clarity | severity_justification |
                             // detection_adequacy | risk_consistency
    std::string severity;    // minor | major | critical
    std::string message;     // human-readable finding
    std::string recommendation;  // improvement recommendation
};

// Aggregated result of assessing one FMEA.
struct FmeaAssessmentResult {
    std::vector<FmeaFinding> findings;
    int criticalCount = 0;
    int majorCount = 0;
    int minorCount = 0;
    // Overall health 0..100 (100 = fully compliant).
    int score = 0;
};

// CSV/XLSX/PDF text import: a lightweight row-oriented text format produced by
// the import path. Each line: severity,occurrence,detection,function|failureMode|
// effect|cause|preventive|detection. The first line may be a header.
class FmeaAssessor {
public:
    explicit FmeaAssessor(adapters::IAdapter& llm,
                          const adapters::AdapterConfig& cfg);

    // Parse a text/CSV import into structured rows. Returns false on unparseable
    // input.
    bool parseImport(const std::string& text,
                     std::vector<ImportedFmeaRow>& out) const;

    // Run the checklist-based quality assessor. On a live-LLM failure the
    // deterministic rule engine runs, so the result is always valid.
    FmeaAssessmentResult assess(const std::vector<ImportedFmeaRow>& rows) const;

    // Pure deterministic quality checks (unit-tested at every boundary).
    static FmeaAssessmentResult deterministicAssess(
        const std::vector<ImportedFmeaRow>& rows);

private:
    adapters::IAdapter& llm_;
    adapters::AdapterConfig cfg_;
};

}  // namespace lodestar::riskai
