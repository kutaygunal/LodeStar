// core/riskai/RequirementQualityService.h
// S2 Phase 13: AI quality scoring on requirements.
//
// Given a requirement, score it on five quality dimensions — clarity,
// testability, atomicity, completeness and (absence of) ambiguity — each 0-100,
// plus an overall score (0-100). Uses the Phase-2 LLM adapter for semantic
// assessment when available; falls back to a deterministic heuristic when the
// LLM is unavailable (AdapterError, network, timeout, unparseable reply) so
// callers always get a valid score object.

#pragma once

#include <string>

#include "core/adapters/Adapter.h"
#include "core/tracelink/Types.h"

namespace lodestar::riskai {

// Per-dimension + overall quality scores, each in [0,100].
struct QualityScore {
    int clarity = 0;      // how clear / well-worded the requirement is
    int testability = 0;  // how verifiable / measurable it is
    int atomicity = 0;    // single responsibility / single verb
    int completeness = 0; // has subject + verb + object, adequate detail
    int ambiguity = 0;    // higher = less ambiguous (absence of vague wording)
    int overall = 0;      // aggregate 0-100
};

class RequirementQualityService {
public:
    // llm is the Phase-2 LlmAdapter (already connected). cfg carries the model
    // name and any prompt-tuning params.
    explicit RequirementQualityService(adapters::IAdapter& llm,
                                      const adapters::AdapterConfig& cfg);

    // Score a requirement on all quality dimensions. On a live-LLM failure it
    // falls back to the deterministic heuristic so callers always get a valid
    // score object (all dimensions in [0,100]).
    QualityScore scoreRequirement(const tracelink::Entity& req) const;

private:
    // Build the prompt that asks the model for structured quality scores.
    std::string buildPrompt(const tracelink::Entity& req) const;

    // Parse the model reply into a QualityScore. Returns false if unparseable.
    bool parseReply(const std::string& text, QualityScore& out) const;

    // Deterministic heuristic fallback (length, "shall", single-verb, vague
    // wording, measurable units, etc.).
    QualityScore heuristicScore(const tracelink::Entity& req) const;

    adapters::IAdapter& llm_;
    adapters::AdapterConfig cfg_;
};

}  // namespace lodestar::riskai
