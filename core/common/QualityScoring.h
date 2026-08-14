// core/common/QualityScoring.h
// Gap-Fill RiskAI 1.7: shared requirement-quality scoring service.
//
// The five-dimension scoring (clarity, testability, atomicity, completeness,
// ambiguity) is shared by BOTH RiskAI and TraceLink. Operates on plain
// requirement text so either module can use it without a dependency cycle, and
// is fully deterministic (no LLM required) so it can be a write hook on save.
//
// Also exposes per-dimension weak flags and deterministic suggested rewording
// for the TraceLink authoring surface.

#pragma once

#include <string>
#include <vector>

namespace lodestar::common {

// Per-dimension + overall quality scores, each in [0,100].
struct QualityScore {
    int clarity = 0;
    int testability = 0;
    int atomicity = 0;
    int completeness = 0;
    int ambiguity = 0;  // higher = less ambiguous
    int overall = 0;
};

// Per-dimension weak flag (dimension name + short reason) and a suggested
// rewording improvement. These are deterministic heuristic flags shown inline
// in the TraceLink authoring surface.
struct QualityFlag {
    std::string dimension;   // clarity | testability | atomicity | completeness | ambiguity
    std::string reason;      // why it is weak
    std::string suggestion;  // concrete improvement
};

struct QualityResult {
    QualityScore score;
    std::vector<QualityFlag> flags;  // only weak dimensions (score < threshold)
};

// Scores a requirement's text on all five dimensions + overall.
QualityScore scoreQuality(const std::string& text);

// Scores and returns per-dimension flags (weak dims only) + suggestions.
QualityResult scoreQualityWithFlags(const std::string& text);

}  // namespace lodestar::common
