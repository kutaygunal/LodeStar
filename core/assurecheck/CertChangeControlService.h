// core/assurecheck/CertChangeControlService.h
// Gap-Fill AssureCheck 2.4: certification change/impact control.
//
// Wires IntegrateHub/TraceLink change requests into a DO-178C configuration-
// control view: every PR/CR shows its impacted entities (HLRs, LLRs, tests) and
// its approval state per baseline, and emits the change-impact report as a
// certification artifact (reusing the 2.1 renderer pattern).

#pragma once

#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"

namespace lodestar::assurecheck {

// One change item in the certification configuration-control view.
struct CertChangeRow {
    std::string crId;
    std::string crTitle;
    std::string crStatus;          // Open | InReview | Approved | ...
    std::string prId;              // linked PR (optional)
    std::string baseline;          // target baseline / config control baseline
    std::string impactedType;      // requirement(HLR/LLR) | test_case | design
    std::string impactedId;
    std::string approvalState;     // Approved | Pending | Rejected
    int unverifiedImpact = 0;      // risk flag
};

class CertChangeControlService {
public:
    explicit CertChangeControlService(persistence::Database& db);

    // Builds the DO-178C configuration-control view by joining the IntegrateHub
    // CR/impact tables with the TraceLink link graph. Deterministic.
    common::Result<std::vector<CertChangeRow>> configurationControlView();

    // Emits the change-impact report as a certification artifact (structured
    // text, reused by the SAS/report renderer). Deterministic.
    common::Result<std::string> emitChangeImpactReport();

private:
    persistence::Database& db_;
};

}  // namespace lodestar::assurecheck
