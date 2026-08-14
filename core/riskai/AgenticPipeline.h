// core/riskai/AgenticPipeline.h
// Gap-Fill RiskAI 1.6: agentic / self-validating pipeline.
//
// Refactors single-pass FMEA generation into five stages, each with a quality
// gate: ANALYZE -> RATE -> VALIDATE -> CORRECT -> FINALIZE.
//   - ANALYZE  : build the failure chain (FE -> FM -> FC) from an input.
//   - RATE     : assign S/O/D and compute RPN + Action Priority.
//   - VALIDATE : check consistency (chain complete, ratings in range, AP
//                consistent with the matrix, row rated).
//   - CORRECT  : on a gate failure, apply a bounded corrective pass.
//   - FINALIZE : persist the corrected, valid rows into a workflow.
// Each correction is logged as an audit event. The pipeline converges to valid
// output on forced-invalid input (bounded, deterministic).

#pragma once

#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/riskai/FmeaWorkflowService.h"

namespace lodestar::riskai {

// Pipeline stage names (public for reporting / audit).
enum class PipelineStage {
    Analyze,
    Rate,
    Validate,
    Correct,
    Finalize
};
std::string pipelineStageName(PipelineStage s);

// One validated failure row produced by the pipeline.
struct ValidatedRow {
    std::string failureMode;
    std::string effect;
    std::string cause;
    int severity = 0;
    int occurrence = 0;
    int detection = 0;
    std::string actionPriority;
    int rpn = 0;
};

// A corrective action applied during CORRECT, logged as an audit event.
struct CorrectionEvent {
    PipelineStage stage;
    std::string message;
};

class AgenticPipeline {
public:
    // Runs the five-stage pipeline, writing validated rows into the workflow
    // `workflowId`. `seedRows` are the ANALYZE-stage output (may be invalid).
    // Returns the final validated rows and the audit of corrections applied.
    common::Result<std::pair<std::vector<ValidatedRow>,
                             std::vector<CorrectionEvent>>>
    run(persistence::Database& db, const std::string& workflowId,
        const std::vector<FmeaRow>& seedRows, int maxCorrections = 3);

    // --- Pure stage functions (unit-tested at each gate) -------------------
    // ANALYZE: from a raw chain input, normalize into a candidate row.
    static FmeaRow analyze(const std::string& failureMode,
                           const std::string& effect,
                           const std::string& cause);

    // RATE: assign S/O/D from an existing (possibly unrated) row.
    static FmeaRow rate(const FmeaRow& in);

    // VALIDATE: returns a list of gate failures (empty = valid).
    static std::vector<std::string> validate(const FmeaRow& in);

    // CORRECT: applies bounded fixes for known gate failures. Returns the
    // corrected row and records the corrections taken.
    static FmeaRow correct(const FmeaRow& in,
                           std::vector<CorrectionEvent>& audit,
                           int& budget);

private:
    // The workflow's required stage gating is re-used to persist only valid
    // rows (a row is valid iff it passes validate()).
    static bool isValid(const FmeaRow& r);
};

}  // namespace lodestar::riskai
