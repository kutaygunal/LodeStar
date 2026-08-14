// core/testforge/ToolQualificationDossier.h
// Gap-Fill TestForge 4.2: DO-330 tool-qualification evidence pack.
//
// Produces a DO-330 tool-qualification dossier for the coverage path:
// purpose, operational environment, verification results, and a
// deviation/limitation log. Documents the exact toolchain versions and the
// verification run used (evidence, not claims), and validates by re-running the
// qualification test set and capturing the transcript.

#pragma once

#include <string>
#include <vector>

#include "core/common/Result.h"

namespace lodestar::testforge {

// One toolchain component in the qualification dossier.
struct ToolchainComponent {
    std::string name;     // e.g. "llvm-cov"
    std::string version;  // exact version string
    std::string role;     // e.g. "branch/decision/MC-DC measurement"
};

// A recorded verification run transcript line.
struct VerificationRun {
    std::string testId;    // e.g. f1_mcdc:testParse
    std::string result;    // PASS | FAIL
    std::string detail;    // transcript of the run
};

// The DO-330 dossier (a documentation + reproducibility artifact).
struct ToolQualificationDossier {
    std::string title;             // e.g. "Coverage Tool Qualification Dossier"
    std::string toolQualificationLevel;  // TQL
    std::string purpose;
    std::string operationalEnvironment;
    std::vector<ToolchainComponent> toolchain;
    std::vector<VerificationRun> verificationResults;
    std::vector<std::string> deviationsLimitations;
    // SHA-256 of the concatenated toolchain versions + verification transcript,
    // so the dossier is reproducible and tamper-evident.
    std::string reproducibilityHash;
};

class ToolQualificationDossierService {
public:
    // Build a dossier from the given components and verification runs.
    // `reproHash` is computed from toolchain versions + run transcripts.
    static ToolQualificationDossier build(
        const std::string& toolQualificationLevel, const std::string& purpose,
        const std::string& operationalEnvironment,
        const std::vector<ToolchainComponent>& toolchain,
        const std::vector<VerificationRun>& verificationResults,
        const std::vector<std::string>& deviationsLimitations);

    // Render the dossier as a deterministic text document (the artifact).
    static std::string render(const ToolQualificationDossier& d);

    // Re-compute the reproducibility hash from toolchain + transcripts; returns
    // true if it matches the dossier's stored hash (evidence, not claims).
    static bool verifyReproducible(const ToolQualificationDossier& d);

    // Re-run the qualification test set against the dossier's transcript: every
    // run must be PASS. Returns ok with the count on success.
    common::Result<int> validateByReRun(const ToolQualificationDossier& d);

private:
    static std::string computeHash(const ToolQualificationDossier& d);
};

}  // namespace lodestar::testforge
