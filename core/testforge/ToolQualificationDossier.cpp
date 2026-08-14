// core/testforge/ToolQualificationDossier.cpp
// Gap-Fill TestForge 4.2: DO-330 tool-qualification evidence pack.

#include "core/testforge/ToolQualificationDossier.h"

#include <sstream>

#include "core/common/Sha256.h"

namespace lodestar::testforge {

ToolQualificationDossier ToolQualificationDossierService::build(
    const std::string& toolQualificationLevel, const std::string& purpose,
    const std::string& operationalEnvironment,
    const std::vector<ToolchainComponent>& toolchain,
    const std::vector<VerificationRun>& verificationResults,
    const std::vector<std::string>& deviationsLimitations) {
    ToolQualificationDossier d;
    d.title = "Coverage Tool Qualification Dossier";
    d.toolQualificationLevel = toolQualificationLevel;
    d.purpose = purpose;
    d.operationalEnvironment = operationalEnvironment;
    d.toolchain = toolchain;
    d.verificationResults = verificationResults;
    d.deviationsLimitations = deviationsLimitations;
    d.reproducibilityHash = computeHash(d);
    return d;
}

std::string ToolQualificationDossierService::computeHash(
    const ToolQualificationDossier& d) {
    std::string blob;
    for (const auto& t : d.toolchain)
        blob += t.name + "@" + t.version + "|";
    for (const auto& v : d.verificationResults)
        blob += v.testId + "=" + v.result + "|";
    return lodestar::common::sha256Hex(blob);
}

bool ToolQualificationDossierService::verifyReproducible(
    const ToolQualificationDossier& d) {
    return d.reproducibilityHash == computeHash(d);
}

std::string ToolQualificationDossierService::render(
    const ToolQualificationDossier& d) {
    std::ostringstream out;
    out << d.title << "\n";
    out << "===============================\n";
    out << "Tool Qualification Level : " << d.toolQualificationLevel << "\n";
    out << "Purpose                  : " << d.purpose << "\n";
    out << "Operational environment  : " << d.operationalEnvironment << "\n";
    out << "Reproducibility hash     : " << d.reproducibilityHash << "\n\n";
    out << "Toolchain (exact versions):\n";
    for (const auto& t : d.toolchain) {
        out << "  - " << t.name << " " << t.version << " (" << t.role << ")\n";
    }
    out << "\nVerification results (re-run transcript):\n";
    for (const auto& v : d.verificationResults) {
        out << "  [" << v.result << "] " << v.testId
            << (v.detail.empty() ? "" : " -- " + v.detail) << "\n";
    }
    out << "\nDeviations / limitations:\n";
    for (const auto& l : d.deviationsLimitations) out << "  - " << l << "\n";
    return out.str();
}

common::Result<int> ToolQualificationDossierService::validateByReRun(
    const ToolQualificationDossier& d) {
    int passed = 0;
    for (const auto& v : d.verificationResults) {
        if (v.result != "PASS") {
            return common::Result<int>::err(
                common::ErrorCode::ValidationFailed,
                "qualification test " + v.testId + " did not pass on re-run");
        }
        ++passed;
    }
    return common::Result<int>::ok(passed);
}

}  // namespace lodestar::testforge
