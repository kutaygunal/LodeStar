// core/assurecheck/SasService.h
// Gap-Fill AssureCheck 2.1: Software Accomplishment Summary (SAS) / PSAC-style
// certification artifact generation.
//
// Produces the standard aviation artifacts from live compliance data:
//   - Plan for Software Aspects of Certification (PSAC)
//   - Software Accomplishment Summary (SAS) with objectives->evidence mapping
// Renders as structured text (and the shared templated report renderer where
// used), mapping each vetted standard sub-objective to its current evidence.

#pragma once

#include <map>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/assurecheck/StandardsContentService.h"

namespace lodestar::assurecheck {

// One objectives->evidence mapping row in the SAS.
struct SasObjectiveEvidence {
    std::string subCode;      // e.g. A1-1
    std::string objective;    // objective text
    std::string dal;          // applicable DAL range
    std::string status;       // PASS | FAIL | NA | WARNING
    std::string evidence;     // evidence summary / links
};

class SasService {
public:
    explicit SasService(persistence::Database& db);

    // Build a PSAC artifact (text) describing the plan, the standard, the
    // software level and the objectives to be satisfied, from the vetted
    // standards bundle.
    common::Result<std::string> buildPsac(
        const StandardBundle& bundle, const std::string& projectDal,
        const std::string& systemName);

    // Build a SAS artifact (text) with the objectives->evidence mapping from
    // live compliance results for the standard. statuses are taken from the
    // provided objective status map (subCode -> status).
    common::Result<std::string> buildSas(
        const StandardBundle& bundle, const std::string& projectDal,
        const std::map<std::string, std::string>& objectiveStatus,
        const std::map<std::string, std::string>& objectiveEvidence);

    // Renders the objectives->evidence rows for a SAS (shared with the report
    // renderer). Deterministic.
    static std::vector<SasObjectiveEvidence> mapEvidence(
        const StandardBundle& bundle, const std::string& projectDal,
        const std::map<std::string, std::string>& objectiveStatus,
        const std::map<std::string, std::string>& objectiveEvidence);

private:
    persistence::Database& db_;
};

}  // namespace lodestar::assurecheck
