// core/riskai/FmeaWorkflowService.h
// Gap-Fill RiskAI 1.1 + 1.2: FMEA workflow engine (AIAG/VDA shape).
//
// Mirrors the AIAG-VDA seven-step workflow as a persistable state machine:
//   Planning -> Structure -> Function -> Failure -> Risk -> Optimization
//              -> Documentation
// Each step is a distinct stage with required fields and validation (stage
// gating): a workflow can only advance when the current stage's mandatory
// fields are present.
//
// RiskAI 1.2: deterministic rating tables (S/O/D 1-10) and the AIAG-VDA Action
// Priority (AP) matrix are data-driven (core/riskai/data/*.csv). RPN = S*O*D
// and AP = High|Medium|Low are computed by pure functions reused by both the
// generation and assessment paths.

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"

namespace lodestar::riskai {

// AIAG-VDA workflow stages (steps 1..7). The enum order IS the allowed
// transition order.
enum class FmeaStage {
    Planning,      // 1. Planning & Preparation
    Structure,     // 2. Structure Analysis
    Function,      // 3. Function Analysis
    Failure,       // 4. Failure Analysis
    Risk,          // 5. Risk Analysis
    Optimization,  // 6. Optimization
    Documentation  // 7. Results & Documentation
};

// The next stage after `s`, or nullopt if s == Documentation (terminal).
std::optional<FmeaStage> nextStage(FmeaStage s);
std::string stageName(FmeaStage s);
std::optional<FmeaStage> stageFromName(const std::string& name);

// One FMEA analysis (workflow instance).
struct FmeaWorkflow {
    std::string id;
    std::string name;
    std::string system;      // focus element
    std::string nextHigher;  // next-higher structure element
    std::string nextLower;   // next-lower structure element
    FmeaStage stage = FmeaStage::Planning;
    std::string createdBy;
    std::string createdAt;
    std::string updatedAt;
};

// A functional element within the FMEA.
struct FmeaFunction {
    std::string id;
    std::string fmeaId;
    std::string text;
    std::string requirement;
    int sortOrder = 0;
};

// One failure-chain row: Effect (FE) -> Failure Mode (FM) -> Cause (FC),
// with S/O/D ratings and the computed Action Priority.
struct FmeaRow {
    std::string id;
    std::string fmeaId;
    std::string functionId;
    std::string effect;
    std::string failureMode;
    std::string cause;
    int severity = 0;     // 1..10 (0 = unset)
    int occurrence = 0;   // 1..10 (0 = unset)
    int detection = 0;    // 1..10 (0 = unset)
    std::string actionPriority;  // High | Medium | Low ('' if not computed)
    int sortOrder = 0;
};

// RPN and Action Priority outcome for one row.
struct RiskScore {
    int rpn = 0;                       // S * O * D
    std::string actionPriority;        // High | Medium | Low
};

class FmeaWorkflowService {
public:
    explicit FmeaWorkflowService(persistence::Database& db);

    // --- Workflow CRUD -----------------------------------------------------
    common::Result<std::string> createWorkflow(const FmeaWorkflow& wf);
    common::Result<std::optional<FmeaWorkflow>> findWorkflow(const std::string& id);
    common::Result<std::vector<FmeaWorkflow>> listWorkflows();
    // Update the analysis fields (name, system, structure elements).
    common::Result<void> updateWorkflow(const FmeaWorkflow& wf);
    common::Result<void> deleteWorkflow(const std::string& id);

    // Stage gating: advance one stage. Fails with ValidationFailed unless the
    // current stage's required fields are present.
    common::Result<FmeaStage> advanceStage(const std::string& id);
    // Set an explicit stage (still enforces required-field gating).
    common::Result<void> setStage(const std::string& id, FmeaStage stage);

    // --- Functions ---------------------------------------------------------
    common::Result<std::string> addFunction(const std::string& fmeaId,
                                            const std::string& text,
                                            const std::string& requirement);
    common::Result<std::vector<FmeaFunction>> functionsFor(const std::string& fmeaId);

    // --- Failure rows ------------------------------------------------------
    common::Result<std::string> addRow(const FmeaRow& row);
    common::Result<std::vector<FmeaRow>> rowsFor(const std::string& fmeaId) const;
    common::Result<void> updateRow(const FmeaRow& row);

    // --- RiskAI 1.2: scoring -------------------------------------------------
    // Pure, deterministic RPN + AP from S/O/D. Reused by generation + assessment.
    static RiskScore computeScore(int severity, int occurrence, int detection);

    // True when the row has all ratings set (S,O,D in [1,10]) and a computed AP.
    static bool rowIsRated(const FmeaRow& row);

private:
    // Returns an error message describing the missing required fields for the
    // given workflow at its current stage, or empty if the stage is complete.
    std::string missingForStage(const FmeaWorkflow& wf) const;

    persistence::Database& db_;
};

}  // namespace lodestar::riskai
