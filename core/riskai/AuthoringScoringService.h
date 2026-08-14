// core/riskai/AuthoringScoringService.h
// Gap-Fill RiskAI 1.7: inline requirement-quality scoring in TraceLink.
//
// A TraceLink authoring surface: score a requirement on save, show per-dimension
// flags and suggested rewording. Uses the shared common::QualityScoring service
// (deterministic heuristic, no LLM required) so it can be a write hook on the
// TraceLink save path without data egress.

#pragma once

#include <string>
#include <vector>

#include "core/common/QualityScoring.h"
#include "core/persistence/Database.h"
#include "core/tracelink/Types.h"

namespace lodestar::riskai {

// One scored requirement with the flags/suggestions shown in the authoring UI,
// persisted so the TraceLink write hook can be verified (scoring determinism +
// a real save hook).
class AuthoringScoringService {
public:
    explicit AuthoringScoringService(persistence::Database& db);

    // Score a requirement (deterministic). Returns the per-dimension scores +
    // weak flags + suggestions.
    common::QualityResult score(const tracelink::Entity& req);

    // Authoring surface: score the requirement and persist the per-dimension
    // scores + overall so a save hook leaves a durable, reviewable record.
    // Returns the overall score.
    common::Result<int> scoreOnSave(const tracelink::Entity& req);

    // The most recent persisted score for a requirement, if any.
    common::Result<std::optional<common::QualityScore>> lastScore(
        const std::string& entityId);

private:
    persistence::Database& db_;
};

}  // namespace lodestar::riskai
