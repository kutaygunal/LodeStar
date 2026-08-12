// core/riskai/RiskAiService.h
// RiskAI first slice (S1 Phase 3): LLM-assisted FMEA.
//
// Takes a hazard description, calls the Phase-2 LLM adapter, and returns a
// structured FMEA table. On a live-LLM failure (AdapterError, network, timeout)
// or an unparseable reply it falls back to a deterministic canned table so
// callers always get rows.

#pragma once

#include <string>
#include <vector>

#include "core/adapters/Adapter.h"
#include "core/common/Result.h"

namespace lodestar::riskai {

// One FMEA row.
struct FmeaRow {
    std::string failureMode;  // how the hazard manifests
    std::string effect;       // consequence on the system
    int severity = 0;         // 1..10
    int likelihood = 0;       // 1..10
    int risk = 0;             // severity * likelihood
};

class RiskAiService {
public:
    // llm is the Phase-2 LlmAdapter (already connected). cfg carries the model
    // name and any prompt-tuning params.
    explicit RiskAiService(adapters::IAdapter& llm,
                           const adapters::AdapterConfig& cfg);

    // Run FMEA on a hazard. Returns the FMEA table. On a live-LLM failure it
    // falls back to a deterministic canned table so callers always get rows.
    common::Result<std::vector<FmeaRow>> analyze(const std::string& hazard);

private:
    // Build the prompt that asks the model for a structured FMEA table.
    std::string buildPrompt(const std::string& hazard) const;

    // Parse the model reply text into FMEA rows. Returns false if unparseable.
    bool parseReply(const std::string& text, std::vector<FmeaRow>& out) const;

    // Deterministic canned fallback table (>= 2 valid rows).
    std::vector<FmeaRow> cannedTable() const;

    adapters::IAdapter& llm_;
    adapters::AdapterConfig cfg_;
};

}  // namespace lodestar::riskai
