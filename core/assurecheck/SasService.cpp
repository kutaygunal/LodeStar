// core/assurecheck/SasService.cpp
// Gap-Fill AssureCheck 2.1: Software Accomplishment Summary (SAS) / PSAC-style
// certification artifact generation.

#include "core/assurecheck/SasService.h"

#include <map>
#include <sstream>

namespace lodestar::assurecheck {

SasService::SasService(persistence::Database& db) : db_(db) {}

common::Result<std::string> SasService::buildPsac(
    const StandardBundle& bundle, const std::string& projectDal,
    const std::string& systemName) {
    if (bundle.standardCode.empty()) {
        return common::Result<std::string>::err(
            common::ErrorCode::InvalidArgument,
            "bundle has no standard code");
    }
    if (projectDal.size() != 1 || projectDal[0] < 'A' || projectDal[0] > 'E') {
        return common::Result<std::string>::err(
            common::ErrorCode::InvalidArgument,
            "project DAL must be a single letter A-E");
    }
    auto flattened = StandardsContentService::flatten(bundle);

    std::ostringstream out;
    out << "PLAN FOR SOFTWARE ASPECTS OF CERTIFICATION (PSAC)\n";
    out << "===================================================\n";
    out << "Standard : " << bundle.standardCode << " - " << bundle.name << "\n";
    out << "System   : " << systemName << "\n";
    out << "Software level (DAL) : " << projectDal << "\n";
    out << "Content bundle       : v" << bundle.bundleVersion
        << " (schema " << bundle.schemaVersion << ")\n\n";
    out << "The following objectives and sub-objectives apply to software "
           "level " << projectDal << ":\n";

    int applicable = 0;
    for (const auto& sub : flattened) {
        if (StandardsContentService::appliesToDal(sub.dal, projectDal)) {
            ++applicable;
            out << "  [" << sub.code << "] " << sub.objective
                << " (DAL " << sub.dal << ") -> evidence: " << sub.evidence
                << "\n";
        }
    }
    out << "\nTotal applicable sub-objectives for DAL " << projectDal
        << ": " << applicable << "\n";
    return common::Result<std::string>::ok(out.str());
}

std::vector<SasObjectiveEvidence> SasService::mapEvidence(
    const StandardBundle& bundle, const std::string& projectDal,
    const std::map<std::string, std::string>& objectiveStatus,
    const std::map<std::string, std::string>& objectiveEvidence) {
    std::vector<SasObjectiveEvidence> out;
    auto flattened = StandardsContentService::flatten(bundle);
    for (const auto& sub : flattened) {
        if (!StandardsContentService::appliesToDal(sub.dal, projectDal)) continue;
        SasObjectiveEvidence row;
        row.subCode = sub.code;
        row.objective = sub.objective;
        row.dal = sub.dal;
        auto st = objectiveStatus.find(sub.code);
        row.status = st == objectiveStatus.end() ? "FAIL" : st->second;
        auto ev = objectiveEvidence.find(sub.code);
        row.evidence = ev == objectiveEvidence.end() ? "" : ev->second;
        out.push_back(std::move(row));
    }
    return out;
}

common::Result<std::string> SasService::buildSas(
    const StandardBundle& bundle, const std::string& projectDal,
    const std::map<std::string, std::string>& objectiveStatus,
    const std::map<std::string, std::string>& objectiveEvidence) {
    if (bundle.standardCode.empty()) {
        return common::Result<std::string>::err(
            common::ErrorCode::InvalidArgument,
            "bundle has no standard code");
    }
    auto rows = mapEvidence(bundle, projectDal, objectiveStatus,
                            objectiveEvidence);

    std::ostringstream out;
    out << "SOFTWARE ACCOMPLISHMENT SUMMARY (SAS)\n";
    out << "=====================================\n";
    out << "Standard : " << bundle.standardCode << " - " << bundle.name << "\n";
    out << "Software level (DAL) : " << projectDal << "\n";
    out << "Content bundle       : v" << bundle.bundleVersion << "\n\n";
    out << "Objectives -> evidence:\n";

    int pass = 0, fail = 0;
    for (const auto& r : rows) {
        out << "  [" << r.subCode << "] " << r.objective << "\n"
            << "      DAL " << r.dal << " | status " << r.status
            << (r.evidence.empty() ? "" : " | evidence: " + r.evidence) << "\n";
        if (r.status == "PASS") ++pass;
        else if (r.status == "FAIL") ++fail;
    }
    out << "\nSummary: " << pass << " PASS, " << fail << " FAIL, "
        << (int)rows.size() << " applicable\n";
    return common::Result<std::string>::ok(out.str());
}

}  // namespace lodestar::assurecheck
