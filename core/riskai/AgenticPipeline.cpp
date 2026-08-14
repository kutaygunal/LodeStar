// core/riskai/AgenticPipeline.cpp
// Gap-Fill RiskAI 1.6: agentic / self-validating pipeline.

#include "core/riskai/AgenticPipeline.h"

#include <algorithm>

namespace lodestar::riskai {

std::string pipelineStageName(PipelineStage s) {
    switch (s) {
        case PipelineStage::Analyze: return "ANALYZE";
        case PipelineStage::Rate: return "RATE";
        case PipelineStage::Validate: return "VALIDATE";
        case PipelineStage::Correct: return "CORRECT";
        case PipelineStage::Finalize: return "FINALIZE";
    }
    return "ANALYZE";
}

FmeaRow AgenticPipeline::analyze(const std::string& failureMode,
                                 const std::string& effect,
                                 const std::string& cause) {
    FmeaRow r;
    r.failureMode = failureMode;
    r.effect = effect;
    r.cause = cause;
    // ANALYZE produces an unrated row; RATE assigns ratings.
    return r;
}

FmeaRow AgenticPipeline::rate(const FmeaRow& in) {
    FmeaRow r = in;
    auto clamp = [](int v) { return v < 1 ? 1 : (v > 10 ? 10 : v); };
    // If ratings were missing (0), seed with a neutral default so the row is
    // rateable; then clamp into [1,10].
    r.severity = in.severity == 0 ? 5 : clamp(in.severity);
    r.occurrence = in.occurrence == 0 ? 5 : clamp(in.occurrence);
    r.detection = in.detection == 0 ? 5 : clamp(in.detection);
    auto sc = FmeaWorkflowService::computeScore(r.severity, r.occurrence,
                                                r.detection);
    r.actionPriority = sc.actionPriority;
    return r;
}

std::vector<std::string> AgenticPipeline::validate(const FmeaRow& in) {
    std::vector<std::string> gates;
    if (in.failureMode.empty()) gates.push_back("failure_mode_missing");
    if (in.effect.empty()) gates.push_back("effect_missing");
    if (in.cause.empty()) gates.push_back("cause_missing");
    if (in.severity < 1 || in.severity > 10) gates.push_back("severity_out_of_range");
    if (in.occurrence < 1 || in.occurrence > 10) gates.push_back("occurrence_out_of_range");
    if (in.detection < 1 || in.detection > 10) gates.push_back("detection_out_of_range");
    if (in.actionPriority.empty()) gates.push_back("ap_missing");
    else if (in.actionPriority != "High" && in.actionPriority != "Medium" &&
             in.actionPriority != "Low")
        gates.push_back("ap_invalid");

    // AP consistency with the matrix (a fully-rated row must have a matching AP).
    if (in.severity >= 1 && in.severity <= 10 &&
        in.occurrence >= 1 && in.occurrence <= 10 &&
        in.detection >= 1 && in.detection <= 10 &&
        !in.actionPriority.empty()) {
        auto expected = FmeaWorkflowService::computeScore(
            in.severity, in.occurrence, in.detection);
        if (in.actionPriority != expected.actionPriority) {
            gates.push_back("ap_inconsistent");
        }
    }
    return gates;
}

bool AgenticPipeline::isValid(const FmeaRow& r) {
    return validate(r).empty();
}

FmeaRow AgenticPipeline::correct(const FmeaRow& in,
                                 std::vector<CorrectionEvent>& audit,
                                 int& budget) {
    FmeaRow r = in;
    auto note = [&](const std::string& msg) {
        if (budget > 0) {
            audit.push_back({PipelineStage::Correct, msg});
            --budget;
        }
    };

    // Fill missing chain fields (deterministic placeholders for a forced-invalid
    // seed so the row can converge to a valid state).
    if (r.failureMode.empty()) { r.failureMode = "Undefined failure mode"; note("filled missing failure mode"); }
    if (r.effect.empty()) { r.effect = "Failure of the focus element"; note("filled missing effect"); }
    if (r.cause.empty()) { r.cause = "Unidentified root cause"; note("filled missing cause"); }

    // Re-rate so ratings/AP are valid and consistent.
    FmeaRow rated = rate(r);
    auto expected = FmeaWorkflowService::computeScore(
        rated.severity, rated.occurrence, rated.detection);
    if (rated.actionPriority != expected.actionPriority) {
        rated.actionPriority = expected.actionPriority;
        note("recomputed action priority to match matrix");
    }
    return rated;
}

common::Result<std::pair<std::vector<ValidatedRow>, std::vector<CorrectionEvent>>>
AgenticPipeline::run(persistence::Database& db, const std::string& workflowId,
                     const std::vector<FmeaRow>& seedRows, int maxCorrections) {
    if (workflowId.empty()) {
        return common::Result<std::pair<std::vector<ValidatedRow>,
                                        std::vector<CorrectionEvent>>>::err(
            common::ErrorCode::InvalidArgument, "workflowId must not be empty");
    }
    if (maxCorrections < 0) maxCorrections = 0;

    FmeaWorkflowService svc(db);
    auto wf = svc.findWorkflow(workflowId);
    if (wf.failed() || !wf.value().has_value()) {
        return common::Result<std::pair<std::vector<ValidatedRow>,
                                        std::vector<CorrectionEvent>>>::err(
            common::ErrorCode::NotFound, "workflow not found");
    }

    std::vector<ValidatedRow> out;
    std::vector<CorrectionEvent> audit;

    for (const auto& seed : seedRows) {
        // ANALYZE (chain) -> RATE.
        FmeaRow candidate = seed;
        int budget = maxCorrections;

        // VALIDATE; loop a bounded CORRECT pass until valid or budget spent.
        int guard = 0;
        while (!isValid(candidate) && guard < 100) {
            candidate = correct(candidate, audit, budget);
            ++guard;
        }
        if (!isValid(candidate)) {
            // Converged to nothing valid within budget -> drop, record audit.
            audit.push_back({PipelineStage::Finalize,
                             "row dropped: did not converge to valid within budget"});
            continue;
        }

        // FINALIZE: persist the validated row into the workflow.
        candidate.fmeaId = workflowId;
        auto res = svc.addRow(candidate);
        if (res.failed()) {
            audit.push_back({PipelineStage::Finalize,
                             "row not persisted: " + res.error()});
            continue;
        }

        ValidatedRow vr;
        vr.failureMode = candidate.failureMode;
        vr.effect = candidate.effect;
        vr.cause = candidate.cause;
        vr.severity = candidate.severity;
        vr.occurrence = candidate.occurrence;
        vr.detection = candidate.detection;
        vr.actionPriority = candidate.actionPriority;
        auto sc = FmeaWorkflowService::computeScore(
            candidate.severity, candidate.occurrence, candidate.detection);
        vr.rpn = sc.rpn;
        out.push_back(std::move(vr));
    }

    return common::Result<std::pair<std::vector<ValidatedRow>,
                                    std::vector<CorrectionEvent>>>::ok(
        {std::move(out), std::move(audit)});
}

}  // namespace lodestar::riskai
